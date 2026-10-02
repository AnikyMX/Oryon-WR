/* tools/bench/perfprobe.c -- ORYON_STATS CPU-sampler check: runs three phases on one render thread, each dominated by a
 * known code owner, and frames them with colour clears of framebuffer 0 so Oryon prints its per-second "cpu" line:
 *   app   : busy loop inside this executable            -> "other"
 *   oryon : immediate-mode vertices, draws dropped by the drawshim (SHIM_NODRAW=1) -> "oryon"
 *   gl    : large glTexSubImage2D uploads (driver texture store)                 -> "gl"
 * Loads liboryon.so like the launcher does (dlopen + dlsym). Usage: perfprobe <liboryon.so> [seconds-per-phase] */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef unsigned int E; typedef int I; typedef float F;
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_TRIANGLES 0x0004
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700

static void *L;
#define S(ret, name, args) static ret (*name) args;
#define LOAD(name) name = (__typeof__(name))dlsym(L, #name); if (!name) { fprintf(stderr, "missing %s\n", #name); exit(1); }
S(void, glClear, (E)) S(void, glBegin, (E)) S(void, glEnd, (void)) S(void, glVertex3f, (F, F, F)) S(void, glColor4f, (F, F, F, F))
S(void, glGenTextures, (I, unsigned *)) S(void, glBindTexture, (E, unsigned)) S(void, glTexImage2D, (E, I, I, I, I, I, E, E, const void *))
S(void, glTexSubImage2D, (E, I, I, I, I, I, E, E, const void *)) S(void, glFinish, (void)) S(unsigned, glGetError, (void))
S(void, glViewport, (I, I, I, I)) S(void, glMatrixMode, (E)) S(void, glLoadIdentity, (void))

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static volatile unsigned long sink;
static void __attribute__((noinline)) spin_app(double sec) {
    double t0 = now(); unsigned long x = 1;
    while (now() - t0 < sec) for (int i = 0; i < 20000; ++i) x = x * 6364136223846793005ul + 1442695040888963407ul;
    sink = x;
}

int main(int argc, char **argv) {
    const char *so = argc > 1 ? argv[1] : "build-host/liboryon.so";
    const double per = argc > 2 ? atof(argv[2]) : 3.2;
    EGLDisplay dpy = ((PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT"))(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    eglInitialize(dpy, NULL, NULL); eglBindAPI(EGL_OPENGL_ES_API);
    EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig cfg; EGLint nc; eglChooseConfig(dpy, ca, &cfg, 1, &nc);
    EGLint pa[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE}; EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pa);
    EGLint xa[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, xa); eglMakeCurrent(dpy, surf, surf, ctx);
    L = dlopen(so, RTLD_NOW | RTLD_LOCAL);
    if (!L) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    LOAD(glClear) LOAD(glBegin) LOAD(glEnd) LOAD(glVertex3f) LOAD(glColor4f) LOAD(glGenTextures) LOAD(glBindTexture) LOAD(glTexImage2D)
    LOAD(glTexSubImage2D) LOAD(glFinish) LOAD(glGetError) LOAD(glViewport) LOAD(glMatrixMode) LOAD(glLoadIdentity)
    glViewport(0, 0, 64, 64);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    enum { TS = 1024 };
    unsigned tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TS, TS, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    unsigned char *pix = malloc((size_t)TS * TS * 4); memset(pix, 0x5a, (size_t)TS * TS * 4);
    const char *phase[] = {"app", "oryon", "gl"};
    for (int p = 0; p < 3; ++p) {
        fprintf(stderr, "PHASE %s\n", phase[p]);
        double t_end = now() + per;
        while (now() < t_end) {
            glClear(GL_COLOR_BUFFER_BIT);                                 /* frame boundary */
            if (p == 0) spin_app(0.004);
            else if (p == 1) {
                for (int b = 0; b < 4; ++b) {
                    glBegin(GL_TRIANGLES);
                    for (int i = 0; i < 3000; ++i) { glColor4f(1, 0.5f, 0.25f, 1); glVertex3f(i * 1e-4f, b * 0.1f, 0); }
                    glEnd();
                }
            } else {
                for (int k = 0; k < 2; ++k) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TS, TS, GL_RGBA, GL_UNSIGNED_BYTE, pix);
            }
        }
    }
    glFinish();
    fprintf(stderr, "PHASE end err=0x%x\n", glGetError());
    free(pix);
    return 0;
}
