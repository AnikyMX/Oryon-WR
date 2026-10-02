// Oryon -- render-thread scheduling on big.LITTLE devices.
// Device data (Mali-G52 / Helio G80, MC 1.12.2, ORYON_STATS): when the render thread ran on the little cores its GL
// driver time per draw doubled (22-26 us vs 12 us) and the frame rate fell from ~64 to ~42 fps; while a world loads,
// other threads kept it runnable-but-waiting for up to 69 ms per frame. At the first frame (colour clear of framebuffer
// 0, always the render thread) Oryon therefore
//  * pins the render thread to the fastest cores: EAS cpu_capacity >= 60% of the maximum, else cpuinfo_max_freq
//    >= 95% of the maximum (ORYON_BIG_CORES=<hex mask> overrides, ORYON_NO_AFFINITY=1 disables), and
//  * raises its priority to nice -10, the level Android gives a top app's UI thread and RenderThread
//    (ORYON_RENDER_NICE=<n>, 0 = unchanged), with SCHED_RESET_ON_FORK so threads it creates start at nice 0.
// Masks come from /proc/<tid>/status Cpus_allowed (sched_getaffinity hides cores that are paused or hot-unplugged at
// that moment, which would freeze a partial pin). Threads the render thread creates later (integrated server, chunk
// builders) inherit its CPU mask: when the thread count changes (and every 8 checks), threads whose mask equals the
// render thread's current pinned mask get the original mask back. Pin and priority are re-applied when the system
// resets them (cpuset moves on foreground/background changes); a pin that failed because all fast cores were offline
// is retried; a new render thread (context moved) is tuned and the old one restored. The check runs every 16 frames and
// costs a few system calls; nothing here touches the GL hot paths.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include "oryon.hpp"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifndef SCHED_RESET_ON_FORK
#define SCHED_RESET_ON_FORK 0x40000000
#endif

namespace ory {
namespace {
const int kMaxCpu = 64;
typedef uint64_t Mask;
bool s_started = false, s_big_done = false, s_pin_want = false, s_pin = false, s_nice_on = false;
int s_tid = 0, s_nice = -10, s_nice0 = 0, s_threads = -1, s_stat_fd = -1, s_scan_tick = 0;
Mask s_big = 0, s_mask0 = 0, s_mask = 0;
uint32_t s_hi = 0, s_lo = 0, s_repin = 0, s_renice = 0, s_unpinned = 0;
const char *s_how = "", *s_pin_why = "", *s_nice_why = "";

uint32_t read_u32(const char *path) {
    unsigned v = 0;
    if (FILE *f = fopen(path, "r")) { if (fscanf(f, "%u", &v) != 1) v = 0; fclose(f); }
    return v;
}
void probe_big() {
    s_big_done = true;
    if (const char *e = getenv("ORYON_BIG_CORES")) { s_big = (Mask)strtoull(e, nullptr, 16); s_how = "ORYON_BIG_CORES"; return; }
    uint32_t cap[kMaxCpu], frq[kMaxCpu], cmax = 0, cmin = UINT32_MAX, fmax = 0, fmin = UINT32_MAX;
    Mask present = 0;
    for (int i = 0; i < kMaxCpu; ++i) {
        char p[96];
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/cpu_capacity", i); cap[i] = read_u32(p);
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i); frq[i] = read_u32(p);
        if (cap[i]) { if (cap[i] > cmax) cmax = cap[i]; if (cap[i] < cmin) cmin = cap[i]; }
        if (frq[i]) { if (frq[i] > fmax) fmax = frq[i]; if (frq[i] < fmin) fmin = frq[i]; }
        if (cap[i] || frq[i]) present |= (Mask)1 << i;
    }
    if (cmax && cmin < cmax) {                   // EAS capacities: big = within 60% of the fastest core
        for (int i = 0; i < kMaxCpu; ++i) if (cap[i] && (uint64_t)cap[i] * 10 >= (uint64_t)cmax * 6) s_big |= (Mask)1 << i;
        s_hi = cmax; s_lo = cmin; s_how = "cpu_capacity";
    } else if (fmax && fmin < fmax) {            // no capacities: cores within 5% of the highest maximum frequency
        for (int i = 0; i < kMaxCpu; ++i) if (frq[i] && (uint64_t)frq[i] * 100 >= (uint64_t)fmax * 95) s_big |= (Mask)1 << i;
        s_hi = fmax / 1000; s_lo = fmin / 1000; s_how = "max MHz";
    }
    if (s_big == present) s_big = 0;             // every core qualifies: nothing to prefer
}
void to_set(cpu_set_t &s, Mask m) { CPU_ZERO(&s); for (int i = 0; i < kMaxCpu && i < CPU_SETSIZE; ++i) if (m >> i & 1) CPU_SET(i, &s); }
Mask of_set(const cpu_set_t &s) { Mask m = 0; for (int i = 0; i < kMaxCpu && i < CPU_SETSIZE; ++i) if (CPU_ISSET(i, &s)) m |= (Mask)1 << i; return m; }
Mask affinity(int tid) { cpu_set_t s; return sched_getaffinity(tid, sizeof s, &s) == 0 ? of_set(s) : 0; }   // allowed AND active
bool set_affinity(int tid, Mask m) { cpu_set_t s; to_set(s, m); return sched_setaffinity(tid, sizeof s, &s) == 0; }
Mask allowed(int tid) {                          // Cpus_allowed: not reduced by paused / offline cores
    char p[64], b[2048];
    snprintf(p, sizeof p, "/proc/self/task/%d/status", tid);
    const int fd = open(p, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return affinity(tid);
    const ssize_t n = read(fd, b, sizeof b - 1);
    close(fd);
    if (n <= 0) return affinity(tid);
    b[n] = 0;
    const char *l = strstr(b, "\nCpus_allowed:");
    if (!l) return affinity(tid);
    l += 14;
    const char *e = strchr(l, '\n');
    const size_t len = e ? (size_t)(e - l) : strlen(l);
    Mask m = 0; int shift = 0;                   // comma-separated 32-bit hex words, most significant first
    for (size_t i = len; i > 0 && shift < 64;) {
        size_t j = i;
        while (j > 0 && l[j - 1] != ',') --j;
        char w[16] = {};
        size_t k = 0;
        for (size_t q = j; q < i && k < sizeof w - 1; ++q) if (l[q] != ' ' && l[q] != '\t') w[k++] = l[q];
        m |= (Mask)strtoull(w, nullptr, 16) << shift;
        shift += 32;
        i = j ? j - 1 : 0;
    }
    return m ? m : affinity(tid);
}
int thread_count() {                             // /proc/self/stat field 20
    char b[1024];
    if (s_stat_fd < 0) return -1;
    const ssize_t n = pread(s_stat_fd, b, sizeof b - 1, 0);
    if (n <= 0) return -1;
    b[n] = 0;
    const char *p = strrchr(b, ')');
    if (!p) return -1;
    for (int field = 2; *p; ++p) if (*p == ' ' && ++field == 20) return atoi(p + 1);
    return -1;
}
void unpin_children() {                          // threads created after pinning inherited the pin
    const Mask pin_now = affinity(s_tid);        // the render thread's pin as the kernel applies it now
    if (!pin_now || (pin_now & ~s_mask)) return; // not pinned at the moment (reset or failed): nothing inherited
    DIR *d = opendir("/proc/self/task");
    if (!d) return;
    while (dirent *e = readdir(d)) {
        const int tid = atoi(e->d_name);
        if (tid <= 0 || tid == s_tid) continue;
        if (affinity(tid) == pin_now && set_affinity(tid, s_mask0)) ++s_unpinned;
    }
    closedir(d);
}
bool try_pin() {
    if (!set_affinity(s_tid, s_mask)) return false;
    s_pin = true;
    if (s_stat_fd < 0) s_stat_fd = open("/proc/self/stat", O_RDONLY | O_CLOEXEC);
    s_threads = thread_count();
    return true;
}
void rt_start(int tid) {
    s_started = true;
    s_tid = tid; s_pin_want = s_pin = s_nice_on = false; s_pin_why = s_nice_why = "";
    // priority
    const char *ne = getenv("ORYON_RENDER_NICE");
    s_nice = ne ? atoi(ne) : -10;
    if (s_nice < -20) s_nice = -20;
    if (s_nice > 19) s_nice = 19;
    errno = 0;
    s_nice0 = getpriority(PRIO_PROCESS, (id_t)s_tid);
    if (errno) s_nice0 = 0;
    if (!s_nice) s_nice_why = " (ORYON_RENDER_NICE=0)";
    else if (s_nice >= s_nice0) s_nice_why = " (already at or above)";
    else {
        sched_param sp; memset(&sp, 0, sizeof sp);
        if (sched_getscheduler(s_tid) == SCHED_OTHER) sched_setscheduler(s_tid, SCHED_OTHER | SCHED_RESET_ON_FORK, &sp);
        if (setpriority(PRIO_PROCESS, (id_t)s_tid, s_nice) == 0) s_nice_on = true;
        else s_nice_why = errno == EACCES || errno == EPERM ? " (not permitted)" : " (setpriority failed)";
    }
    // affinity
    s_mask0 = allowed(s_tid);
    if (env_on("ORYON_NO_AFFINITY")) s_pin_why = " (ORYON_NO_AFFINITY)";
    else {
        if (!s_big_done) probe_big();
        s_mask = s_big & s_mask0;
        if (!s_mask || s_mask == s_mask0) s_pin_why = s_big ? " (no faster core allowed)" : " (uniform cores)";
        else { s_pin_want = true; if (!try_pin()) s_pin_why = " (fast cores offline, retrying)"; }
    }
    char cores[64] = "";
    if (s_big) snprintf(cores, sizeof cores, ", fastest by %s %u vs %u", s_how, s_hi, s_lo);
    log("render thread %d: nice %d -> %d%s | cpus 0x%llx -> 0x%llx%s%s", s_tid, s_nice0, s_nice_on ? s_nice : s_nice0, s_nice_why,
        (unsigned long long)s_mask0, (unsigned long long)(s_pin ? s_mask : s_mask0), s_pin_why, cores);
}
void rt_restore() {                              // previous render thread (context moved to another thread)
    if (s_pin) set_affinity(s_tid, s_mask0);
    if (s_nice_on) setpriority(PRIO_PROCESS, (id_t)s_tid, s_nice0);
}
void rt_check() {
    if (s_pin_want) {
        if (!s_pin) { if (try_pin()) log("render thread %d: pinned to 0x%llx", s_tid, (unsigned long long)s_mask); }
        else {
            if (affinity(s_tid) & ~s_mask) { if (set_affinity(s_tid, s_mask)) ++s_repin; }   // reset by a cpuset move
            const int n = thread_count();        // new threads, plus a full scan every 8 checks (exit + create)
            if (n != s_threads || ++s_scan_tick >= 8) { s_threads = n; s_scan_tick = 0; unpin_children(); }
        }
    }
    if (s_nice_on) {
        errno = 0;
        const int p = getpriority(PRIO_PROCESS, (id_t)s_tid);
        if (!errno && p != s_nice && setpriority(PRIO_PROCESS, (id_t)s_tid, s_nice) == 0) ++s_renice;
    }
}
} // namespace

void rt_frame_tick() {
    const int tid = (int)syscall(__NR_gettid);
    if (!s_started) rt_start(tid);
    else if (tid != s_tid) { rt_restore(); rt_start(tid); }
    else rt_check();
    g.rt_countdown = (s_pin_want || s_nice_on) ? 16 : 0x7fffffff;
}
uint32_t big_core_mask() { if (!s_big_done) probe_big(); return (uint32_t)s_big; }
int rt_describe(char *out, size_t n) {
    if (!s_started) return snprintf(out, n, "rt sched pending");
    return snprintf(out, n, "rt nice %d cpus 0x%llx (re-pinned %u, re-niced %u, children unpinned %u)", s_nice_on ? s_nice : s_nice0,
                    (unsigned long long)(s_pin ? s_mask : s_mask0), s_repin, s_renice, s_unpinned);
}

} // namespace ory
