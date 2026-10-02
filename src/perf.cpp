// Oryon -- ORYON_STATS performance probes. Installed only when stats are enabled, so normal runs execute none of this.
//  * GPU pass timeline (EGL_KHR_fence_sync): at every draw-framebuffer switch and at frame start the outgoing render pass
//    gets an EGL fence + flush; a helper thread records when each fence signals. The GPU interval a pass occupied in the
//    in-order queue is busy_i = done_i - max(submit_i, done_{i-1}) (render-pass granularity, which is what tilers allow).
//    A pass submitted to an idle GPU also carries the fence round trip (flush, signal, wake-up). That floor is the
//    25th percentile of recent idle-submitted passes that contain no draw call (pure round trips) and is subtracted
//    from idle-submitted passes (and printed); without such samples it stays 0.
//  * GPU timer (GL_EXT_disjoint_timer_query, opt-in ORYON_STATS_TIMER=1): the same passes bracketed by GL_TIME_ELAPSED_EXT
//    queries. On Mali the result spans the CPU recording of the pass as well, so it is off by default. The last pass of
//    a frame ends at the app's eglSwapBuffers, which Oryon cannot see, so it is reported apart as "tail" (an upper
//    bound: it also contains whatever idle time follows the swap).
//  * CPU: render-thread CPU time per frame, process CPU time, and a SIGPROF sampler on the render thread's CPU clock
//    (period 1 ms, delivered at kernel-tick granularity) that attributes each sample to Oryon / GL driver / JVM /
//    Java (JIT code) / libc / other by program counter, plus the share taken on the fastest cores (src/sched.cpp) and the
//    render-thread scheduling state (nice, CPU mask, re-applications); the render thread's
//    run-queue wait (runnable but not running) from /proc schedstat; and the present segment (last switch to
//    framebuffer 0 until the next frame start: blit, the app's eglSwapBuffers, frame-start work).
//  * ES calls per frame (draw, program, uniform, texture, VAO, buffer bind, attribute pointer, vertex buffer binding,
//    attribute format/binding) by interposing the ES table.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include "oryon.hpp"
#define EGL_NO_PLATFORM_SPECIFIC_TYPES 1
#define EGL_EGLEXT_PROTOTYPES 1          // declarations only: used for decltype, never called directly (no link dependency)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

namespace ory {
void *g_egl_lib = nullptr;               // set by load_es (gles.cpp)

namespace {
// ------------------------------------------------------------------ EGL entry points (types taken from the headers)
using PfnGetProcAddress = decltype(&eglGetProcAddress);
using PfnGetCurrentDisplay = decltype(&eglGetCurrentDisplay);
using PfnQueryString = decltype(&eglQueryString);
using PfnCreateSync = decltype(&eglCreateSyncKHR);
using PfnDestroySync = decltype(&eglDestroySyncKHR);
using PfnClientWaitSync = decltype(&eglClientWaitSyncKHR);
EGLDisplay e_dpy = EGL_NO_DISPLAY;
PfnCreateSync e_create = nullptr;
PfnDestroySync e_destroy = nullptr;
PfnClientWaitSync e_wait = nullptr;

bool env_off(const char *n) { const char *v = getenv(n); return v && *v && !env_on(n); }
bool has_token(const char *list, const char *tok) {
    const size_t l = strlen(tok);
    for (const char *p = list; p && (p = strstr(p, tok)) != nullptr; p += l)
        if ((p == list || p[-1] == ' ') && (p[l] == ' ' || p[l] == 0)) return true;
    return false;
}
uint64_t clock_us(clockid_t c) { timespec t; clock_gettime(c, &t); return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u; }

bool s_timer = false, s_fence = false;   // GPU probes available
const char *s_gpu_why = "";              // reason when unavailable
GLint s_timer_bits = 0;

// ------------------------------------------------------------------ passes (render-thread producer, helper consumer)
const uint32_t kRing = 64, kMask = kRing - 1, kMaxPasses = 24, kFrames = 32;
struct Pass {
    uint32_t frame; GLuint fb;
    uint64_t submit, done;               // CPU microseconds (CLOCK_MONOTONIC)
    EGLSyncKHR sync;
    GLuint q;                            // timer query object owned by this slot
    uint64_t gpu_ns;
    uint8_t fence;                       // 0 none, 1 pending, 2 signalled, 3 failed
    uint8_t timer;                       // 0 none, 1 issued, 2 result read
    bool tail, qbad;
    uint32_t ndraw;                      // draw calls recorded in the pass
    uint64_t draws0;
    uint8_t age;                         // boundaries waited for an unavailable timer result
};
Pass p_ring[kRing];
uint32_t p_head = 0, p_wait = 0, p_tail = 0;   // [p_tail,p_wait) fence settled, [p_wait,p_head) pending, p_head = open slot
bool p_open = false;
pthread_mutex_t p_mu = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t p_cv = PTHREAD_COND_INITIALIZER;
bool p_stop = false, p_alive = false;

struct Frame {
    uint32_t id, period_us, cpu_us, busy_us, timer_us, tail_us, lag_us;
    uint16_t passes, settled;
    bool closed, done, fence_bad, timer_bad, has_tail, has_lag;
};
Frame f_ring[kFrames];
uint32_t fl_ring[64], fl_n = 0, fl_i = 0, s_floor = 0;   // idle-submitted empty-pass fence latencies, 25th percentile
uint32_t f_cur = 0;                      // open frame id (0 = none yet)
uint64_t f_t0 = 0, f_cpu0 = 0, last_done = 0;
GLuint draw_fb = 0;

struct Win {
    uint32_t frames, cpu_bound; uint64_t period_us, cpu_us;
    uint32_t gframes, busy_max, gpu_bound; uint64_t busy_us;
    uint32_t tframes, tails; uint64_t timer_us, tail_us;
    uint32_t lags, lag_max; uint64_t lag_us;
    GLuint fb_id[3]; uint64_t fb_us[3], fb_other;
    uint32_t disjoint, overflow, fence_fail;
    uint32_t pres_n; uint64_t pres_wall, pres_cpu;
};
Win w;
uint64_t t_pres = 0, cpu_pres = 0;     // last switch to framebuffer 0 in the open frame
uint64_t w_draws0 = 0;

Frame &frame_rec(uint32_t id) { return f_ring[id % kFrames]; }

void frame_done(Frame &F) {
    if (F.done) return;
    F.done = true;
    if (!F.passes) return;
    if (s_fence && !F.fence_bad) {
        ++w.gframes; w.busy_us += F.busy_us;
        if (F.busy_us > w.busy_max) w.busy_max = F.busy_us;
        if ((uint64_t)F.busy_us * 100 >= (uint64_t)F.period_us * 85) ++w.gpu_bound;
        if (F.has_lag) { ++w.lags; w.lag_us += F.lag_us; if (F.lag_us > w.lag_max) w.lag_max = F.lag_us; }
    }
    if (s_timer && !F.timer_bad) {
        ++w.tframes; w.timer_us += F.timer_us;
        if (F.has_tail) { ++w.tails; w.tail_us += F.tail_us; }
    }
}

void fb_account(GLuint fb, uint32_t us) {
    for (int i = 0; i < 3; ++i) if (w.fb_us[i] && w.fb_id[i] == fb) { w.fb_us[i] += us; return; }
    for (int i = 0; i < 3; ++i) if (!w.fb_us[i]) { w.fb_id[i] = fb; w.fb_us[i] = us ? us : 1; return; }
    w.fb_other += us;
}

void floor_add(uint32_t us) {
    fl_ring[fl_i++ & 63u] = us;
    if (fl_n < 64) ++fl_n;
    uint32_t v[64];
    for (uint32_t i = 0; i < fl_n; ++i) {                // insertion sort of <= 64 values
        uint32_t x = fl_ring[i], j = i;
        while (j && v[j - 1] > x) { v[j] = v[j - 1]; --j; }
        v[j] = x;
    }
    s_floor = v[fl_n / 4];
}

void settle(Pass &P) {
    Frame &F = frame_rec(P.frame);
    if (F.id != P.frame) return;                         // frame record already recycled
    if (P.fence == 2) {
        const bool idle = P.submit >= last_done;         // nothing queued on the GPU when this pass was submitted
        const uint32_t raw = P.done > (idle ? P.submit : last_done) ? (uint32_t)(P.done - (idle ? P.submit : last_done)) : 0;
        if (idle && P.ndraw == 0) floor_add(raw);       // pure round trip: nothing to execute
        const uint32_t busy = idle ? (raw > s_floor ? raw - s_floor : 0) : raw;
        if (P.done > last_done) last_done = P.done;
        F.busy_us += busy;
        fb_account(P.fb, busy);
        if (P.tail) {                                    // GPU work still pending when the next frame started
            const uint32_t l = P.done > P.submit ? (uint32_t)(P.done - P.submit) : 0;
            F.lag_us = l > s_floor ? l - s_floor : 0; F.has_lag = true;
        }
    } else if (s_fence) {
        F.fence_bad = true;
    }
    if (P.timer == 2 && P.gpu_ns > 10000000000ull) P.qbad = true;    // > 10 s: not a pass duration
    if (P.timer == 2 && !P.qbad) {
        const uint32_t us = (uint32_t)(P.gpu_ns / 1000u);
        if (P.tail) { F.tail_us = us; F.has_tail = true; } else F.timer_us += us;
    } else if (P.timer) {
        F.timer_bad = true;
    }
    if (++F.settled == F.passes && F.closed) frame_done(F);
}

void pass_open(GLuint fb) {
    if (!(s_timer || s_fence) || !f_cur || p_open) return;
    Frame &F = frame_rec(f_cur);
    if (p_head - p_tail >= kRing) { ++w.overflow; F.fence_bad = F.timer_bad = true; return; }
    Pass &P = p_ring[p_head & kMask];
    P.frame = f_cur; P.fb = fb; P.submit = P.done = 0; P.sync = EGL_NO_SYNC_KHR; P.gpu_ns = 0;
    P.fence = 0; P.timer = 0; P.tail = P.qbad = false; P.age = 0; P.draws0 = g_draws_total; P.ndraw = 0;
    if (s_timer) { es.glBeginQuery(GL_TIME_ELAPSED_EXT, P.q); P.timer = 1; }
    ++F.passes; p_open = true;
}

void pass_close(bool tail) {
    if (!p_open) return;
    Pass &P = p_ring[p_head & kMask];
    if (P.timer == 1) es.glEndQuery(GL_TIME_ELAPSED_EXT);
    if (s_fence) {
        P.sync = e_create(e_dpy, EGL_SYNC_FENCE_KHR, nullptr);
        P.fence = P.sync != EGL_NO_SYNC_KHR ? 1 : 3;
        if (P.fence == 3) ++w.fence_fail;
        es.glFlush();                                    // submit the pass now: its start is not delayed by batching
    }
    P.submit = now_us(); P.tail = tail; P.ndraw = (uint32_t)(g_draws_total - P.draws0);
    pthread_mutex_lock(&p_mu);
    ++p_head;
    if (!s_fence) p_wait = p_head;
    pthread_cond_signal(&p_cv);
    pthread_mutex_unlock(&p_mu);
    p_open = false;
}

void *helper_main(void *) {
    pthread_mutex_lock(&p_mu);
    for (;;) {
        while (!p_stop && p_wait == p_head) pthread_cond_wait(&p_cv, &p_mu);
        if (p_stop) break;
        Pass &P = p_ring[p_wait & kMask];
        pthread_mutex_unlock(&p_mu);
        uint8_t st = P.fence; uint64_t done = 0;
        if (st == 1) {
            EGLint r;
            do r = e_wait(e_dpy, P.sync, 0, (EGLTimeKHR)20000000u);     // 20 ms slices: exit stays prompt
            while (r == EGL_TIMEOUT_EXPIRED_KHR && !__atomic_load_n(&p_stop, __ATOMIC_RELAXED));
            done = now_us();
            e_destroy(e_dpy, P.sync);
            st = r == EGL_CONDITION_SATISFIED_KHR ? 2 : 3;
        }
        pthread_mutex_lock(&p_mu);
        P.done = done; P.fence = st; ++p_wait;
    }
    p_alive = false;
    pthread_cond_broadcast(&p_cv);
    pthread_mutex_unlock(&p_mu);
    return nullptr;
}

// atexit: stop the helper before library destructors run, so no thread is inside EGL during teardown.
void helper_stop() {
    pthread_mutex_lock(&p_mu);
    p_stop = true;
    pthread_cond_broadcast(&p_cv);
    timespec dl; clock_gettime(CLOCK_REALTIME, &dl);
    dl.tv_nsec += 200000000; if (dl.tv_nsec >= 1000000000) { dl.tv_nsec -= 1000000000; ++dl.tv_sec; }
    while (p_alive) if (pthread_cond_timedwait(&p_cv, &p_mu, &dl)) break;
    pthread_mutex_unlock(&p_mu);
}

void harvest() {
    if (s_timer) {
        GLint dj = 0;
        es.glGetIntegerv(GL_GPU_DISJOINT_EXT, &dj);
        if (dj) {                                        // results overlapping a disjoint event are undefined
            ++w.disjoint;
            for (uint32_t i = p_tail; i != p_head + (p_open ? 1u : 0u); ++i) p_ring[i & kMask].qbad = true;
        }
    }
    pthread_mutex_lock(&p_mu);
    const uint32_t settled = p_wait;
    pthread_mutex_unlock(&p_mu);
    while (p_tail != settled) {
        Pass &P = p_ring[p_tail & kMask];
        if (P.timer == 1) {
            GLuint avail = 0;
            es.glGetQueryObjectuiv(P.q, GL_QUERY_RESULT_AVAILABLE, &avail);
            if (!avail) { if (++P.age < 8) break; P.qbad = true; P.timer = 2; }
            else if (es.glGetQueryObjectui64vEXT) { GLuint64 v = 0; es.glGetQueryObjectui64vEXT(P.q, GL_QUERY_RESULT, &v); P.gpu_ns = v; P.timer = 2; }
            else { GLuint v = 0; es.glGetQueryObjectuiv(P.q, GL_QUERY_RESULT, &v); P.gpu_ns = v; P.timer = 2; }
        }
        settle(P);
        ++p_tail;
    }
}

// ------------------------------------------------------------------ framebuffer tracking (pass boundaries)
decltype(EsFuncs::glBindFramebuffer) o_bind_fb = nullptr;
void GL_APIENTRY w_bind_fb(GLenum target, GLuint fb) {
    if ((target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) && fb != draw_fb) {
        const bool cut = p_open && frame_rec(f_cur).passes < kMaxPasses;
        if (cut) pass_close(false);
        o_bind_fb(target, fb);
        draw_fb = fb;
        if (cut) pass_open(fb);
        if (fb == 0 && f_cur) { t_pres = now_us(); cpu_pres = clock_us(CLOCK_THREAD_CPUTIME_ID); }
        return;
    }
    o_bind_fb(target, fb);
}

// ------------------------------------------------------------------ state call counters (ES table interposition)
uint32_t c_prog = 0, c_unif = 0, c_tex = 0, c_vao = 0, c_buf = 0, c_attr = 0, c_vbuf = 0, c_fmt = 0;
template <auto *Orig, uint32_t *Ctr, typename R, typename... A>
R GL_APIENTRY counted(A... a) { ++*Ctr; return (*Orig)(a...); }
template <auto *Orig, uint32_t *Ctr, typename R, typename... A>
void hook(R (GL_APIENTRY *&slot)(A...)) { if (slot) { *Orig = slot; slot = &counted<Orig, Ctr, R, A...>; } }
#define ORY_COUNTED(X) \
    X(glUseProgram, c_prog) X(glBindTexture, c_tex) X(glBindVertexArray, c_vao) X(glBindBuffer, c_buf) \
    X(glVertexAttribPointer, c_attr) X(glVertexAttribIPointer, c_attr) X(glBindVertexBuffer, c_vbuf) \
    X(glVertexAttribFormat, c_fmt) X(glVertexAttribBinding, c_fmt) \
    X(glUniform1f, c_unif) X(glUniform1fv, c_unif) X(glUniform1i, c_unif) X(glUniform1iv, c_unif) \
    X(glUniform1ui, c_unif) X(glUniform1uiv, c_unif) X(glUniform2f, c_unif) X(glUniform2fv, c_unif) \
    X(glUniform2i, c_unif) X(glUniform2iv, c_unif) X(glUniform2ui, c_unif) X(glUniform2uiv, c_unif) \
    X(glUniform3f, c_unif) X(glUniform3fv, c_unif) X(glUniform3i, c_unif) X(glUniform3iv, c_unif) \
    X(glUniform3ui, c_unif) X(glUniform3uiv, c_unif) X(glUniform4f, c_unif) X(glUniform4fv, c_unif) \
    X(glUniform4i, c_unif) X(glUniform4iv, c_unif) X(glUniform4ui, c_unif) X(glUniform4uiv, c_unif) \
    X(glUniformMatrix2fv, c_unif) X(glUniformMatrix2x3fv, c_unif) X(glUniformMatrix2x4fv, c_unif) X(glUniformMatrix3fv, c_unif) \
    X(glUniformMatrix3x2fv, c_unif) X(glUniformMatrix3x4fv, c_unif) X(glUniformMatrix4fv, c_unif) X(glUniformMatrix4x2fv, c_unif) \
    X(glUniformMatrix4x3fv, c_unif) \
    /* end */
#define X(fn, ctr) decltype(EsFuncs::fn) o_##fn = nullptr;
ORY_COUNTED(X)
#undef X

// ------------------------------------------------------------------ CPU sampler (SIGPROF on the render thread's CPU clock)
enum : uint8_t { C_ORYON, C_GL, C_JVM, C_JAVA, C_LIBC, C_OTHER, C_N };
struct Range { uintptr_t lo, hi; uint8_t cat; };
const int kRanges = 2048;                // app processes on Android map several hundred libraries
Range r_tab[2][kRanges];
int r_n[2] = {0, 0};
int r_pub = -1;                          // published table, read by the handler (same thread as the builder)
uint32_t r_smp[C_N];
uint32_t r_cpu[16];                      // samples per CPU (sched_getcpu)
uint32_t s_big_mask = 0;                 // fastest CPUs (src/sched.cpp; 0: uniform or unknown)
int s_sched_fd = -1;                     // /proc/self/task/<render tid>/schedstat
uint64_t s_sched_run = 0, s_sched_wait = 0;
bool s_sampler = false, s_sampler_tried = false;
const char *s_sampler_why = "";

uint8_t cat_of_name(const char *path) {
    const char *b = strrchr(path, '/'); b = b ? b + 1 : path;
    if (!*b) return C_OTHER;                             // the main executable
    char l[128]; size_t n = 0;
    for (; b[n] && n + 1 < sizeof l; ++n) l[n] = (char)tolower((unsigned char)b[n]);
    l[n] = 0;
    static const char *const kGl[] = {"gles", "libegl", "mali", "gallium", "_dri", "glapi", "libgsl", "adreno", "libllvm",
                                      "glnext", "powervr", "libimg", "libsrv_um", "libglx", "libgl."};
    for (const char *k : kGl) if (strstr(l, k)) return C_GL;
    if (!strncmp(l, "libjvm", 6)) return C_JVM;
    static const char *const kLibc[] = {"libc.", "libm.", "libdl.", "ld-linux", "linker", "vdso", "libpthread", "libc++", "libstdc++", "libgcc"};
    for (const char *k : kLibc) if (strstr(l, k)) return C_LIBC;
    return C_OTHER;
}
struct Build { Range *t; int n; uintptr_t self; };
int phdr_cb(dl_phdr_info *info, size_t, void *data) {
    Build &b = *(Build *)data;
    bool self = false;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) &ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_LOAD) continue;
        const uintptr_t lo = info->dlpi_addr + ph.p_vaddr;
        if (b.self >= lo && b.self < lo + ph.p_memsz) self = true;
    }
    const uint8_t c = self ? C_ORYON : cat_of_name(info->dlpi_name ? info->dlpi_name : "");
    for (int i = 0; i < info->dlpi_phnum && b.n < kRanges; ++i) {
        const ElfW(Phdr) &ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_LOAD || !(ph.p_flags & PF_X)) continue;
        const uintptr_t lo = info->dlpi_addr + ph.p_vaddr;
        b.t[b.n++] = Range{lo, lo + ph.p_memsz, c};
    }
    return 0;
}
void ranges_build() {
    const int slot = r_pub == 0 ? 1 : 0;
    Build b{r_tab[slot], 0, (uintptr_t)&ranges_build};
    dl_iterate_phdr(phdr_cb, &b);
    for (int i = 1; i < b.n; ++i) {                      // insertion sort by start address
        Range x = b.t[i]; int j = i - 1;
        while (j >= 0 && b.t[j].lo > x.lo) { b.t[j + 1] = b.t[j]; --j; }
        b.t[j + 1] = x;
    }
    r_n[slot] = b.n;
    __atomic_store_n(&r_pub, slot, __ATOMIC_RELEASE);
}
inline uint8_t classify(const Range *t, int n, uintptr_t pc) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        const int m = (lo + hi) >> 1;
        if (pc < t[m].lo) hi = m - 1; else if (pc >= t[m].hi) lo = m + 1; else return t[m].cat;
    }
    return C_JAVA;                                       // not in any loaded object: JIT / interpreter code
}
#if defined(__aarch64__) || defined(__x86_64__)
void on_prof(int, siginfo_t *si, void *ctx) {
    const int t = __atomic_load_n(&r_pub, __ATOMIC_ACQUIRE);
    if (t < 0) return;
    const int saved_errno = errno;
    const ucontext_t *uc = (const ucontext_t *)ctx;
#if defined(__aarch64__)
    const uintptr_t pc = (uintptr_t)uc->uc_mcontext.pc, ra = (uintptr_t)uc->uc_mcontext.regs[30];
#else
    const uintptr_t pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
    const uintptr_t ra = *(const uintptr_t *)(uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
#endif
    uint8_t c = classify(r_tab[t], r_n[t], pc);
    if (c == C_LIBC) {                                   // leaf libc routine (memcpy...): charge the caller
        const uint8_t r = classify(r_tab[t], r_n[t], ra);
        if (r == C_ORYON || r == C_GL || r == C_JVM) c = r;
    }
    const int ov = si ? si->si_overrun : 0;          // expirations merged into this signal (tick granularity)
    const uint32_t wgt = 1u + (uint32_t)(ov > 0 && ov < 1000 ? ov : 0);
    __atomic_fetch_add(&r_smp[c], wgt, __ATOMIC_RELAXED);
    const int cpu = sched_getcpu();
    if ((unsigned)cpu < 16u) __atomic_fetch_add(&r_cpu[cpu], wgt, __ATOMIC_RELAXED);
    errno = saved_errno;
}
struct KSigevent {                       // kernel ABI struct sigevent (64 bytes)
    union { int i; void *p; } value;
    int signo, notify;
    union { int pad[(64 - 2 * sizeof(int) - sizeof(void *)) / sizeof(int)]; int tid; } un;
};
static_assert(sizeof(KSigevent) == 64, "kernel sigevent layout");
#ifdef SIGEV_THREAD_ID
const int kSigevThreadId = SIGEV_THREAD_ID;
#else
const int kSigevThreadId = 4;
#endif
#endif

bool sched_read(uint64_t &run, uint64_t &wait) {
    char b[96];
    if (s_sched_fd < 0) return false;
    const ssize_t n = pread(s_sched_fd, b, sizeof b - 1, 0);
    if (n <= 0) return false;
    b[n] = 0;
    char *e = nullptr;
    run = strtoull(b, &e, 10); wait = strtoull(e, nullptr, 10);
    return true;
}
void sampler_start() {
    s_sampler_tried = true;
    {                                     // render-thread scheduler accounting (independent of the sampler)
        char p[64]; snprintf(p, sizeof p, "/proc/self/task/%d/schedstat", (int)syscall(__NR_gettid));
        s_sched_fd = open(p, O_RDONLY | O_CLOEXEC);
        if (!sched_read(s_sched_run, s_sched_wait)) { if (s_sched_fd >= 0) close(s_sched_fd); s_sched_fd = -1; }
    }
    s_big_mask = big_core_mask();
#if defined(__aarch64__) || defined(__x86_64__)
    if (env_off("ORYON_STATS_PROFILE")) { s_sampler_why = "off (ORYON_STATS_PROFILE=0)"; return; }
    struct sigaction old;
    if (sigaction(SIGPROF, nullptr, &old)) { s_sampler_why = "off (sigaction)"; return; }
    if (old.sa_handler != SIG_DFL && old.sa_handler != SIG_IGN) { s_sampler_why = "off (SIGPROF in use)"; return; }
    ranges_build();
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_prof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPROF, &sa, nullptr)) { s_sampler_why = "off (sigaction)"; return; }
    KSigevent ev;
    memset(&ev, 0, sizeof ev);
    ev.signo = SIGPROF; ev.notify = kSigevThreadId; ev.un.tid = (int)syscall(__NR_gettid);
    int timer = -1;
    if (syscall(__NR_timer_create, (long)CLOCK_THREAD_CPUTIME_ID, &ev, &timer) != 0) {
        sigaction(SIGPROF, &old, nullptr); s_sampler_why = "off (timer_create)"; return;
    }
    itimerspec its;
    its.it_interval.tv_sec = 0; its.it_interval.tv_nsec = 1000000;       // 1 ms of render-thread CPU time
    its.it_value = its.it_interval;
    if (syscall(__NR_timer_settime, timer, 0, &its, nullptr) != 0) {
        syscall(__NR_timer_delete, timer); sigaction(SIGPROF, &old, nullptr); s_sampler_why = "off (timer_settime)"; return;
    }
    s_sampler = true;
#else
    s_sampler_why = "off (unsupported architecture)";
#endif
}

uint64_t p_proc_prev = 0, p_wall_prev = 0;
uint32_t p_reports = 0;

int fmt_ms(char *o, size_t n, const char *label, uint64_t sum_us, uint32_t cnt) {
    return snprintf(o, n, "%s %.1f", label, cnt ? sum_us / 1000.0 / cnt : 0.0);
}
} // namespace

// ------------------------------------------------------------------ public entry points (render thread, stats mode)
void perf_install() {
#define X(fn, ctr) hook<&o_##fn, &ctr>(es.fn);
    ORY_COUNTED(X)
#undef X
    o_bind_fb = es.glBindFramebuffer;
    es.glBindFramebuffer = w_bind_fb;
    p_proc_prev = clock_us(CLOCK_PROCESS_CPUTIME_ID); p_wall_prev = now_us();
    if (env_off("ORYON_STATS_GPU")) { s_gpu_why = "off (ORYON_STATS_GPU=0)"; log("perf: gpu probes %s", s_gpu_why); return; }
    // GPU timer queries (opt-in: on Mali they also span the CPU recording of a pass)
    const char *tq = env_on("ORYON_STATS_TIMER") ? "no GL_EXT_disjoint_timer_query" : "off (ORYON_STATS_TIMER=1 enables)";
    if ((g.escaps & ORY_ESCAP_TIMER_QUERY) && env_on("ORYON_STATS_TIMER")) {
        es.glGetQueryiv(GL_TIME_ELAPSED_EXT, GL_QUERY_COUNTER_BITS_EXT, &s_timer_bits);
        if (s_timer_bits > 0) {
            GLuint q[kRing + 1] = {};
            es.glGenQueries((GLsizei)(kRing + 1), q);
            es.glBeginQuery(GL_TIME_ELAPSED_EXT, q[kRing]);  // probe on its own object, never reused
            es.glEndQuery(GL_TIME_ELAPSED_EXT);
            const bool ok = es.glGetError() == GL_NO_ERROR && q[0];
            es.glDeleteQueries(1, &q[kRing]);
            if (ok) { for (uint32_t i = 0; i < kRing; ++i) p_ring[i].q = q[i]; s_timer = true; }
            else { es.glDeleteQueries((GLsizei)kRing, q); tq = "TIME_ELAPSED rejected"; }
        } else tq = "TIME_ELAPSED has 0 counter bits";
    }
    // EGL fence timeline
    const char *fq = "no EGL library";
    if (void *lib = g_egl_lib) {
        auto gpa = (PfnGetProcAddress)dlsym(lib, "eglGetProcAddress");
        auto cur = (PfnGetCurrentDisplay)dlsym(lib, "eglGetCurrentDisplay");
        auto qs = (PfnQueryString)dlsym(lib, "eglQueryString");
        e_dpy = cur ? cur() : EGL_NO_DISPLAY;
        const char *ex = (e_dpy != EGL_NO_DISPLAY && qs) ? qs(e_dpy, EGL_EXTENSIONS) : nullptr;
        fq = "no EGL_KHR_fence_sync";
        if (has_token(ex, "EGL_KHR_fence_sync")) {
            auto sym = [&](const char *n) -> void * { void *p = gpa ? (void *)gpa(n) : nullptr; return p ? p : dlsym(lib, n); };
            e_create = (PfnCreateSync)sym("eglCreateSyncKHR");
            e_destroy = (PfnDestroySync)sym("eglDestroySyncKHR");
            e_wait = (PfnClientWaitSync)sym("eglClientWaitSyncKHR");
            fq = "EGL fence entry points missing";
            if (e_create && e_destroy && e_wait) {
                pthread_attr_t at; pthread_attr_init(&at);
                pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
                pthread_t th;
                p_alive = true;
                if (pthread_create(&th, &at, helper_main, nullptr) == 0) { s_fence = true; atexit(helper_stop); }
                else { p_alive = false; fq = "helper thread failed"; }
                pthread_attr_destroy(&at);
            }
        }
    }
    while (es.glGetError() != GL_NO_ERROR) {}
    char tb[64]; snprintf(tb, sizeof tb, "TIME_ELAPSED (%d bits)", (int)s_timer_bits);
    log("perf: gpu fence timeline %s | gpu timer %s | state counters on | cpu sampler starts at first frame",
        s_fence ? "EGL_KHR_fence_sync" : fq, s_timer ? tb : tq);
}

// Frame start (colour clear of framebuffer 0), called before the clear executes.
void perf_boundary(uint64_t t) {
    const uint64_t cpu = clock_us(CLOCK_THREAD_CPUTIME_ID);
    if (!s_sampler_tried) {                             // first frame: measurement windows start here
        sampler_start();
        char cores[48];
        if (s_big_mask) snprintf(cores, sizeof cores, "big cores mask 0x%x", s_big_mask);
        else snprintf(cores, sizeof cores, "cores uniform or unknown");
        log("perf: cpu sampler %s | schedstat %s | %s", s_sampler ? "SIGPROF every 1 ms of render-thread CPU time (kernel-tick granularity)" : s_sampler_why,
            s_sched_fd >= 0 ? "on" : "unavailable", cores);
        c_prog = c_unif = c_tex = c_vao = c_buf = c_attr = c_vbuf = c_fmt = 0;
        w_draws0 = g_draws_total;
    }
    if (t_pres) { ++w.pres_n; w.pres_wall += t - t_pres; w.pres_cpu += cpu - cpu_pres; t_pres = 0; }
    if (f_cur) {
        pass_close(true);                                // tail: the app's swap already submitted it
        Frame &F = frame_rec(f_cur);
        F.period_us = (uint32_t)(t - f_t0); F.cpu_us = (uint32_t)(cpu - f_cpu0); F.closed = true;
        ++w.frames; w.period_us += F.period_us; w.cpu_us += F.cpu_us;
        if ((uint64_t)F.cpu_us * 100 >= (uint64_t)F.period_us * 85) ++w.cpu_bound;
        if (F.settled == F.passes) frame_done(F);
    }
    harvest();
    if (++f_cur == 0) f_cur = 1;
    Frame &N = frame_rec(f_cur);
    N = Frame{}; N.id = f_cur;
    f_t0 = t; f_cpu0 = cpu;
    draw_fb = 0;
    pass_open(0);
}

void perf_report(uint64_t t0, uint64_t t1) {
    const double secs = (double)(t1 - t0) / 1e6;
    const uint64_t proc = clock_us(CLOCK_PROCESS_CPUTIME_ID), wall = now_us();
    const double cores = wall > p_wall_prev ? (double)(proc - p_proc_prev) / (double)(wall - p_wall_prev) : 0.0;
    p_proc_prev = proc; p_wall_prev = wall;
    const double fr = w.frames ? (double)w.frames : 1.0;
    char smp[160];
    if (s_sampler) {
        uint32_t c[C_N], tot = 0;
        for (int i = 0; i < C_N; ++i) { c[i] = __atomic_exchange_n(&r_smp[i], 0u, __ATOMIC_RELAXED); tot += c[i]; }
        const double k = tot ? 100.0 / tot : 0.0;
        snprintf(smp, sizeof smp, "oryon %.0f gl %.0f jvm %.0f java %.0f libc %.0f other %.0f %%, %u ms sampled",
                 c[C_ORYON] * k, c[C_GL] * k, c[C_JVM] * k, c[C_JAVA] * k, c[C_LIBC] * k, c[C_OTHER] * k, tot);
    } else {
        snprintf(smp, sizeof smp, "sampler %s", s_sampler_tried ? s_sampler_why : "pending");
    }
    char sch[96] = "", big[48] = "", pres[64] = "";
    uint64_t run = 0, wait = 0;
    if (sched_read(run, wait)) {
        snprintf(sch, sizeof sch, " | runnable-waiting %.1f ms/f", (double)(wait - s_sched_wait) / 1e6 / fr);
        s_sched_run = run; s_sched_wait = wait;
    }
    if (s_sampler && s_big_mask) {
        uint64_t tb = 0, ta = 0;
        for (int i = 0; i < 16; ++i) { const uint32_t v = __atomic_exchange_n(&r_cpu[i], 0u, __ATOMIC_RELAXED); ta += v; if (s_big_mask & (1u << i)) tb += v; }
        if (ta) snprintf(big, sizeof big, " | big cores %.0f%%", 100.0 * tb / ta);
    } else {
        for (int i = 0; i < 16; ++i) __atomic_store_n(&r_cpu[i], 0u, __ATOMIC_RELAXED);
    }
    if (w.pres_n) snprintf(pres, sizeof pres, " | present %.1f ms/f (cpu %.1f)", w.pres_wall / 1000.0 / w.pres_n, w.pres_cpu / 1000.0 / w.pres_n);
    char rts[112]; rt_describe(rts, sizeof rts);
    if (w.frames)
        log("cpu %.2fs: frame %.1f ms | render thread %.1f ms/f (%.0f%%, cpu-bound %.0f%%)%s%s%s | %s | [%s] | process %.2f cores",
            secs, w.period_us / 1000.0 / fr, w.cpu_us / 1000.0 / fr, w.period_us ? 100.0 * w.cpu_us / w.period_us : 0.0,
            100.0 * w.cpu_bound / fr, sch, big, pres, rts, smp, cores);
    else
        log("cpu %.2fs: no frame boundary | [%s] | process %.2f cores", secs, smp, cores);
    if (w.frames)
        log("calls %.2fs: per frame draw %.0f prog %.0f unif %.0f tex %.0f vao %.0f bind %.0f attr %.0f vbuf %.0f fmt %.0f | fb0 clears %u (deferred %u)",
            secs, (double)(g_draws_total - w_draws0) / fr, c_prog / fr, c_unif / fr, c_tex / fr, c_vao / fr, c_buf / fr, c_attr / fr,
            c_vbuf / fr, c_fmt / fr, g.fbc.n_clear, g.fbc.n_defer);
    g.fbc.n_clear = g.fbc.n_defer = 0;
    w_draws0 = g_draws_total;
    char busy[160], tim[96], lag[96];
    if (!s_fence) snprintf(busy, sizeof busy, "busy n/a (%s)", *s_gpu_why ? s_gpu_why : "no fence");
    else if (!w.gframes) snprintf(busy, sizeof busy, "busy n/a (no settled frame)");
    else {
        int n = snprintf(busy, sizeof busy, "busy %.1f ms/f (max %.1f, gpu-bound %.0f%%) [", w.busy_us / 1000.0 / w.gframes,
                         w.busy_max / 1000.0, 100.0 * w.gpu_bound / w.gframes);
        for (int i = 0; i < 3 && n > 0 && n < (int)sizeof busy; ++i)
            if (w.fb_us[i]) n += snprintf(busy + n, sizeof busy - n, "%sfb%u %.1f", i ? " " : "", w.fb_id[i], w.fb_us[i] / 1000.0 / w.gframes);
        if (w.fb_other && n > 0 && n < (int)sizeof busy) n += snprintf(busy + n, sizeof busy - n, " other %.1f", w.fb_other / 1000.0 / w.gframes);
        if (n > 0 && n < (int)sizeof busy) snprintf(busy + n, sizeof busy - n, "]");
    }
    if (!s_timer) snprintf(tim, sizeof tim, "timer off");
    else if (!w.tframes) snprintf(tim, sizeof tim, "timer n/a (no settled frame%s)", w.disjoint ? ", disjoint" : "");
    else {
        int n = fmt_ms(tim, sizeof tim, "timer", w.timer_us, w.tframes);
        if (n > 0 && n < (int)sizeof tim) snprintf(tim + n, sizeof tim - n, " ms/f + tail <=%.1f%s", w.tails ? w.tail_us / 1000.0 / w.tails : 0.0,
                                                    w.disjoint ? " (disjoint)" : "");
    }
    if (w.lags) snprintf(lag, sizeof lag, "lag %.1f ms (max %.1f) | fence floor %.1f ms", w.lag_us / 1000.0 / w.lags, w.lag_max / 1000.0,
                         s_floor / 1000.0);
    else snprintf(lag, sizeof lag, "lag n/a");
    log("gpu %.2fs: %s | %s | %s | %u frames%s", secs, busy, tim, lag, w.gframes > w.tframes ? w.gframes : w.tframes,
        w.overflow ? " (ring overflow)" : "");
    w = Win{};
    c_prog = c_unif = c_tex = c_vao = c_buf = c_attr = c_vbuf = c_fmt = 0;
    if (s_sampler && ++p_reports % 5 == 0) ranges_build();   // pick up libraries loaded later
}

} // namespace ory
