// Oryon -- GLES 3.2 driver loader (dlopen/dlsym) + lazy per-context init.
#include "oryon.hpp"
#include "imm.hpp"
#include "ffp_prog.hpp"
#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace ory {
EsFuncs es;
Ctx g;
void mat_defaults();

void ctx_defaults() { mat_defaults(); ffp_defaults(); }

void log(const char *fmt, ...) {
    char buf[768];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
#ifdef __ANDROID__
    __android_log_write(ANDROID_LOG_INFO, "Oryon", buf);
#endif
    // Launchers (Pojav/Zalith family) copy the game's stdio into latestlog.txt but filter logcat tags,
    // so the same line also goes to stderr (unbuffered). Only init/error paths call this.
    fprintf(stderr, "[Oryon] %s\n", buf);
}

namespace {
struct EsEntry { const char *name; void **slot; int ext; };
const EsEntry k_es[] = {
#define ES_FN(r, n, p, x) {#n, (void **)&es.n, x},
#include "gen/es_funcs.inc"
#undef ES_FN
};

void *open_first(const char *env, const char *const *names) {
    const char *e = getenv(env);
    if (e && *e) { if (void *h = dlopen(e, RTLD_NOW | RTLD_LOCAL)) return h; log("dlopen(%s) failed: %s", e, dlerror()); }
    for (; *names; ++names) if (void *h = dlopen(*names, RTLD_NOW | RTLD_LOCAL)) return h;
    return nullptr;
}

// glDrawRangeElementsBaseVertex (ES 3.2) without driver support: the range is only a hint. Calls the driver entry
// directly, so the ORYON_STATS draw counters see one draw.
decltype(EsFuncs::glDrawElementsBaseVertex) s_dbv_driver = nullptr;
void GL_APIENTRY draw_range_bv_fallback(GLenum mode, GLuint, GLuint, GLsizei count, GLenum type, const void *indices, GLint bv) {
    s_dbv_driver(mode, count, type, indices, bv);
}
__attribute__((constructor)) void load_es() {
    static const char *const kGles[] = {"libGLESv3.so", "libGLESv2.so.2", "libGLESv2.so", nullptr};
    static const char *const kEgl[] = {"libEGL.so", "libEGL.so.1", nullptr};
    void *hg = open_first("ORYON_GLES_LIB", kGles);
    void *he = open_first("ORYON_EGL_LIB", kEgl);
    g_egl_lib = he;
    typedef void *(*Gpa)(const char *);
    Gpa gpa = he ? (Gpa)dlsym(he, "eglGetProcAddress") : nullptr;
    int miss = 0;
    for (const EsEntry &e : k_es) {
        void *p = (!e.ext && hg) ? dlsym(hg, e.name) : nullptr;
        if (!p && gpa) p = gpa(e.name);
        *e.slot = p;
        if (!p && !e.ext) ++miss;
    }
    if (!es.glDrawRangeElementsBaseVertex && es.glDrawElementsBaseVertex) {
        s_dbv_driver = es.glDrawElementsBaseVertex; es.glDrawRangeElementsBaseVertex = draw_range_bv_fallback;
    }
    ctx_defaults();
    if (!hg) log("no GLES library found (set ORYON_GLES_LIB)");
    if (miss) log("%d core GLES 3.2 entry points unresolved", miss);
}

struct EsExtMap { const char *name; uint32_t bit; };
const EsExtMap k_esext[] = {
    {"GL_EXT_texture_filter_anisotropic", ORY_ESCAP_ANISO},
    {"GL_EXT_texture_border_clamp", ORY_ESCAP_BORDER_CLAMP}, {"GL_OES_texture_border_clamp", ORY_ESCAP_BORDER_CLAMP},
    {"GL_EXT_buffer_storage", ORY_ESCAP_BUFFER_STORAGE}, {"GL_EXT_texture_format_BGRA8888", ORY_ESCAP_BGRA},
    {"GL_EXT_clip_cull_distance", ORY_ESCAP_CLIP_DISTANCE}, {"GL_NV_polygon_mode", ORY_ESCAP_POLYGON_MODE},
    {"GL_EXT_multi_draw_arrays", ORY_ESCAP_MULTI_DRAW}, {"GL_EXT_depth_clamp", ORY_ESCAP_DEPTH_CLAMP},
    {"GL_EXT_texture_norm16", ORY_ESCAP_NORM16}, {"GL_EXT_color_buffer_float", ORY_ESCAP_COLOR_BUFFER_FLOAT},
    {"GL_EXT_blend_func_extended", ORY_ESCAP_BLEND_FUNC_EXTENDED},
    {"GL_EXT_polygon_offset_clamp", ORY_ESCAP_POLYGON_OFFSET_CLAMP},
    {"GL_EXT_disjoint_timer_query", ORY_ESCAP_TIMER_QUERY},
};
struct DeskExt { const char *name; uint32_t need; };
const DeskExt k_desk[] = {
#define ORY_EXT(n, need) {n, (uint32_t)(need)},
#include "gen/ext_list.inc"
#undef ORY_EXT
};

char g_strbuf[4096];
char *g_strp = g_strbuf;
const GLubyte *keep(const char *a, const char *b = "", const char *c = "") {
    size_t la = strlen(a), lb = strlen(b), lc = strlen(c);
    if ((size_t)(g_strbuf + sizeof g_strbuf - g_strp) < la + lb + lc + 1) return (const GLubyte *)"";
    char *r = g_strp;
    memcpy(g_strp, a, la); memcpy(g_strp + la, b, lb); memcpy(g_strp + la + lb, c, lc);
    g_strp += la + lb + lc; *g_strp++ = 0;
    return (const GLubyte *)r;
}
} // namespace

void ctx_init() {
    const char *ver = es.glGetString ? (const char *)es.glGetString(GL_VERSION) : nullptr;
    if (!ver) return;                                   // no current context yet: retry on next call
    GLint n = 0;
    es.glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; ++i) {
        const char *e = (const char *)es.glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (!e) continue;
        for (const EsExtMap &m : k_esext) if (!strcmp(e, m.name)) g.escaps |= m.bit;
    }
    es.glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &g.es_max_tex_units);
    es.glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &g.es_max_vertex_attribs);
    es.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &g.es_max_tex_size);
    // Desktop view reported to LWJGL / Minecraft.
    const char *vend = (const char *)es.glGetString(GL_VENDOR);
    const char *rend = (const char *)es.glGetString(GL_RENDERER);
    g.str_vendor = keep(vend ? vend : "Oryon");
    g.str_renderer = keep(rend ? rend : "GLES", " (Oryon)");
    g.str_version = keep("3.0 Oryon " ORYON_VERSION " (", ver, ")");
    g.str_glsl = keep("1.30 Oryon");
    char *start = g_strp;
    g.ext_count = 0;
    for (const DeskExt &d : k_desk) {
        if (d.need && !(g.escaps & d.need)) continue;
        if (g.ext_count < (int)(sizeof g.ext / sizeof g.ext[0])) g.ext[g.ext_count++] = d.name;
    }
    for (int i = 0; i < g.ext_count; ++i) {
        size_t l = strlen(g.ext[i]);
        if ((size_t)(g_strbuf + sizeof g_strbuf - g_strp) < l + 2) break;
        memcpy(g_strp, g.ext[i], l); g_strp += l; *g_strp++ = ' ';
    }
    if (g_strp > start) g_strp[-1] = 0; else *g_strp++ = 0;
    g.str_ext = (const GLubyte *)start;
    g.inited = true;
    g.hooks &= ~HOOK_INIT;
    g.st.on = env_on("ORYON_STATS");
    if (g.st.on) {
        stats_install();
        if (const char *ms = getenv("ORYON_STATS_SLOW_MS")) g.stats_slow_us = (uint32_t)(strtod(ms, nullptr) * 1000.0);
    }
    vertex_init();
    fb_init();
    while (es.glGetError() != GL_NO_ERROR) {}           // never leak init-time state into the app's error flag
    log("init " ORYON_VERSION ": %s | %s | ES ext mask 0x%x | desktop ext %d | stream %s | vertex %s | fb0 clear %s | max tex %d",
        ver, rend ? rend : "?", g.escaps, g.ext_count, g.rv.persistent ? "persistent" : "map-unsync",
        g.vbind ? "attrib-binding" : "attrib-pointer", g.fbc.off ? "immediate" : "deferred", g.es_max_tex_size);
    if (g.st.on) {
        log("stats enabled: one summary line per second (frame = colour clear of framebuffer 0)");
        perf_install();
    }
    ffp_cache_init();
}

void run_hooks() {
    if (g.hooks & HOOK_INIT) ctx_init();
    if (g.hooks & HOOK_FB0_CLEAR) fb0_clear_exec();     // recorded before any pending immediate-mode batch
    if (g.hooks & HOOK_FLUSH) imm_flush();
}
} // namespace ory
