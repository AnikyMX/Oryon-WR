/* Tes kontrak bootstrap LWJGL.

   Meniru persis apa yang dilakukan org.lwjgl.opengl.GL saat Minecraft mulai:
   membuat context lewat EGL (peran libpojavexec.so), dlopen liboryon.so,
   mencari hub getProcAddress, lalu menjalankan urutan yang sama dengan
   GL.createCapabilities() dan GLCapabilities.check_*.

   Lulus di sini = flag kapabilitas yang dijanjikan memang akan menyala. */

#define _GNU_SOURCE
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "checks.inc"

typedef unsigned int   GLenum;
typedef unsigned char  GLubyte;
typedef int            GLint;
typedef unsigned int   GLbitfield;
typedef float          GLclampf;
typedef int            GLsizei;

#define GL_NO_ERROR                     0
#define GL_VERSION                      0x1F02
#define GL_EXTENSIONS                   0x1F03
#define GL_RENDERER                     0x1F01
#define GL_VENDOR                       0x1F00
#define GL_SHADING_LANGUAGE_VERSION     0x8B8C
#define GL_MAX_TEXTURE_UNITS            0x84E2
#define GL_MAX_TEXTURE_SIZE             0x0D33
#define GL_COLOR_BUFFER_BIT             0x00004000
#define GL_FRAMEBUFFER                  0x8D40
#define GL_RENDERBUFFER                 0x8D41
#define GL_COLOR_ATTACHMENT0            0x8CE0
#define GL_RGBA8                        0x8058
#define GL_RGBA                         0x1908
#define GL_UNSIGNED_BYTE                0x1401
#define GL_FRAMEBUFFER_COMPLETE         0x8CD5

static int g_pass, g_fail;

static void ok(int cond, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (cond) { g_pass++; fputs("  ok   ", stdout); }
    else      { g_fail++; fputs("  GAGAL ", stdout); }
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
}

/* ----------------------------------------------------------------- EGL --- */

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLContext g_ctx = EGL_NO_CONTEXT;
static EGLSurface g_surf = EGL_NO_SURFACE;

static int egl_up(void) {
    PFNEGLGETPLATFORMDISPLAYEXTPROC getdpy =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (getdpy)
        g_dpy = getdpy(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    if (g_dpy == EGL_NO_DISPLAY)
        g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_dpy == EGL_NO_DISPLAY) { puts("  (tidak ada EGLDisplay)"); return 0; }

    EGLint maj = 0, min = 0;
    if (!eglInitialize(g_dpy, &maj, &min)) { puts("  (eglInitialize gagal)"); return 0; }
    printf("  EGL %d.%d  vendor=%s\n", maj, min, eglQueryString(g_dpy, EGL_VENDOR));

    if (!eglBindAPI(EGL_OPENGL_ES_API)) { puts("  (eglBindAPI gagal)"); return 0; }

    static const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    EGLConfig cfg; EGLint n = 0;
    if (!eglChooseConfig(g_dpy, cfg_attr, &cfg, 1, &n) || n < 1) {
        puts("  (tidak ada EGLConfig GLES3)"); return 0;
    }

    static const EGLint want[][5] = {
        { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE },
        { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE },
        { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE },
    };
    for (unsigned i = 0; i < 3 && g_ctx == EGL_NO_CONTEXT; ++i) {
        g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, want[i]);
        if (g_ctx != EGL_NO_CONTEXT)
            printf("  context GLES 3.%d dibuat\n", (int) want[i][3]);
    }
    if (g_ctx == EGL_NO_CONTEXT) { puts("  (eglCreateContext gagal)"); return 0; }

    if (!eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g_ctx)) {
        static const EGLint pb[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
        g_surf = eglCreatePbufferSurface(g_dpy, cfg, pb);
        if (g_surf == EGL_NO_SURFACE ||
            !eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
            puts("  (eglMakeCurrent gagal)"); return 0;
        }
        puts("  memakai pbuffer 64x64");
    } else {
        puts("  memakai context surfaceless");
    }
    return 1;
}

/* ----------------------------------------------------------------- hub --- */

typedef void *(*HubFn)(const void *);
static HubFn g_hub;
static const char *g_hub_name;
static void *g_lib;

static void *hub(const char *name) { return g_hub((const void *) name); }

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "./liboryon.so";

    puts("== 1. Context EGL (peran libpojavexec.so) ==");
    if (!egl_up()) { puts("\nLINGKUNGAN TIDAK MENDUKUNG - dilewati"); return 77; }

    puts("\n== 2. dlopen liboryon.so persis seperti LWJGL ==");
    g_lib = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
    ok(g_lib != NULL, "dlopen(%s, RTLD_LAZY|RTLD_GLOBAL)%s%s", path,
       g_lib ? "" : " -> ", g_lib ? "" : dlerror());
    if (!g_lib) return 1;

    /* Urutan pencarian hub milik org/lwjgl/opengl/GL$1. */
    static const char *const kHubNames[] = {
        "glXGetProcAddress", "glXGetProcAddressARB", "wglGetProcAddress",
        "eglGetProcAddress", "OSMesaGetProcAddress", NULL
    };
    for (int i = 0; kHubNames[i] && !g_hub; ++i) {
        void *p = dlsym(g_lib, kHubNames[i]);
        if (p) { g_hub = (HubFn) p; g_hub_name = kHubNames[i]; }
    }
    ok(g_hub != NULL, "hub ditemukan: %s", g_hub_name ? g_hub_name : "(tidak ada)");
    if (!g_hub) return 1;

    puts("\n== 3. Kontrak GL.createCapabilities ==");
    void *pGetError    = hub("glGetError");
    void *pGetString   = hub("glGetString");
    void *pGetIntegerv = hub("glGetIntegerv");
    ok(pGetError && pGetString && pGetIntegerv,
       "glGetError / glGetString / glGetIntegerv semuanya resolve");
    if (!(pGetError && pGetString && pGetIntegerv)) return 1;

    GLenum (*glGetError)(void) = (GLenum (*)(void)) pGetError;
    const GLubyte *(*glGetString)(GLenum) = (const GLubyte *(*)(GLenum)) pGetString;
    void (*glGetIntegerv)(GLenum, GLint *) = (void (*)(GLenum, GLint *)) pGetIntegerv;

    GLenum e = glGetError();
    ok(e == GL_NO_ERROR, "glGetError() == 0 (dapat 0x%X)", e);

    const GLubyte *ver = glGetString(GL_VERSION);
    ok(ver != NULL, "glGetString(GL_VERSION) bukan NULL");
    if (!ver) return 1;
    int maj = 0, min = 0;
    sscanf((const char *) ver, "%d.%d", &maj, &min);
    ok(maj > 1 || (maj == 1 && min >= 1),
       "versi >= 1.1  ->  \"%s\"", (const char *) ver);

    printf("       GL_VENDOR   = %s\n", (const char *) glGetString(GL_VENDOR));
    printf("       GL_RENDERER = %s\n", (const char *) glGetString(GL_RENDERER));
    printf("       GLSL        = %s\n",
           (const char *) glGetString(GL_SHADING_LANGUAGE_VERSION));

    const GLubyte *ext = glGetString(GL_EXTENSIONS);
    ok(ext != NULL, "glGetString(GL_EXTENSIONS) bukan NULL");
    int next = 0;
    for (const char *p = (const char *) ext; p && *p; ) {
        while (*p == ' ') ++p;
        if (*p) { ++next; while (*p && *p != ' ') ++p; }
    }
    printf("       %d ekstensi diiklankan\n", next);

    GLint units = -1;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
    ok(units == 8, "glGetIntegerv(GL_MAX_TEXTURE_UNITS) == 8 (dapat %d)", units);
    GLint tsz = -1;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &tsz);
    ok(tsz > 0, "glGetIntegerv(GL_MAX_TEXTURE_SIZE) diteruskan ke driver -> %d", tsz);

    puts("\n== 4. Setiap ekspor harus resolve lewat hub ==");
    int missing = 0;
    const char *first_missing = NULL;
    for (int i = 0; kExports[i]; ++i)
        if (!hub(kExports[i])) { if (!missing) first_missing = kExports[i]; ++missing; }
    ok(missing == 0, "352 ekspor resolve (hilang: %d%s%s)", missing,
       first_missing ? ", mis. " : "", first_missing ? first_missing : "");

    puts("\n== 5. Nama di luar lingkup HARUS NULL ==");
    int leaked = 0;
    const char *first_leak = NULL;
    for (int i = 0; kMustBeNull[i]; ++i)
        if (hub(kMustBeNull[i])) { if (!leaked) first_leak = kMustBeNull[i]; ++leaked; }
    ok(leaked == 0, "40 simbol GL di luar lingkup -> NULL (bocor: %d%s%s)", leaked,
       first_leak ? ", mis. " : "", first_leak ? first_leak : "");
    ok(hub("glTidakAdaSamaSekali") == NULL, "nama karangan -> NULL");
    ok(dlsym(g_lib, "glAccum") == NULL, "fallback dlsym juga NULL untuk di luar lingkup");

    puts("\n== 6. Flag kapabilitas yang dijanjikan ==");
    for (int f = 0; kFlags[f].name; ++f) {
        int n = 0, bad = 0;
        const char *sample = NULL;
        for (int i = 0; kFlags[f].syms[i]; ++i) {
            ++n;
            if (!hub(kFlags[f].syms[i])) { if (!bad) sample = kFlags[f].syms[i]; ++bad; }
        }
        ok(bad == 0, "%-24s %3d simbol lengkap%s%s", kFlags[f].name, n,
           bad ? "  <- hilang mis. " : "", bad ? sample : "");
    }

    puts("\n== 7. Jalur render sungguhan lewat wrapper ==");
    /* Context surfaceless tidak punya framebuffer bawaan, jadi kita bangun FBO
       persis lewat jalur ARB_framebuffer_object yang dipilih Minecraft. */
    void (*glGenRenderbuffers)(GLsizei, unsigned *) =
        (void (*)(GLsizei, unsigned *)) hub("glGenRenderbuffers");
    void (*glBindRenderbuffer)(GLenum, unsigned) =
        (void (*)(GLenum, unsigned)) hub("glBindRenderbuffer");
    void (*glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei) =
        (void (*)(GLenum, GLenum, GLsizei, GLsizei)) hub("glRenderbufferStorage");
    void (*glGenFramebuffers)(GLsizei, unsigned *) =
        (void (*)(GLsizei, unsigned *)) hub("glGenFramebuffers");
    void (*glBindFramebuffer)(GLenum, unsigned) =
        (void (*)(GLenum, unsigned)) hub("glBindFramebuffer");
    void (*glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, unsigned) =
        (void (*)(GLenum, GLenum, GLenum, unsigned)) hub("glFramebufferRenderbuffer");
    GLenum (*glCheckFramebufferStatus)(GLenum) =
        (GLenum (*)(GLenum)) hub("glCheckFramebufferStatus");
    void (*glViewport)(GLint, GLint, GLsizei, GLsizei) =
        (void (*)(GLint, GLint, GLsizei, GLsizei)) hub("glViewport");
    void (*glClearColor)(GLclampf, GLclampf, GLclampf, GLclampf) =
        (void (*)(GLclampf, GLclampf, GLclampf, GLclampf)) hub("glClearColor");
    void (*glClear)(GLbitfield) = (void (*)(GLbitfield)) hub("glClear");

    ok(glGenRenderbuffers && glBindRenderbuffer && glRenderbufferStorage &&
       glGenFramebuffers && glBindFramebuffer && glFramebufferRenderbuffer &&
       glCheckFramebufferStatus && glViewport && glClearColor && glClear,
       "seluruh jalur FBO + clear resolve lewat hub");

    unsigned rb = 0, fb = 0;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 64, 64);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ok(st == GL_FRAMEBUFFER_COMPLETE, "FBO lengkap (0x%X), rb=%u fb=%u", st, rb, fb);

    glViewport(0, 0, 64, 64);
    glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    e = glGetError();
    ok(e == GL_NO_ERROR, "tanpa error GL setelah clear (0x%X)", e);

    /* Dibaca balik lewat driver Mesa langsung: bukti perintah Oryon benar-benar
       sampai ke GPU, bukan sekadar tidak melempar error. */
    void *mesa = dlopen("libGLESv2.so.2", RTLD_LAZY);
    if (!mesa) mesa = dlopen("libGLESv2.so", RTLD_LAZY);
    void (*rp)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *) =
        mesa ? (void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))
               dlsym(mesa, "glReadPixels") : 0;
    if (rp) {
        unsigned char px[4] = { 0, 0, 0, 0 };
        rp(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        ok(px[0] == 51 && px[1] == 102 && px[2] == 153 && px[3] == 255,
           "piksel hasil clear = (%u,%u,%u,%u), harusnya (51,102,153,255)",
           px[0], px[1], px[2], px[3]);
    } else {
        puts("       (glReadPixels driver tidak ada, verifikasi piksel dilewati)");
    }

    printf("\n===== %d lulus, %d gagal =====\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
