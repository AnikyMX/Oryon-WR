#include "driver.h"

#include <dlfcn.h>
#include <stdlib.h>

namespace oryon {

GlesProcs gles;

static void *s_gles_lib;
static void *s_egl_lib;
static void *(*s_egl_gpa)(const char *);

/* Android menaruh GLES 3.x di libGLESv3.so; loader Mesa di sandbox memakai
   nama ber-soname. ORYON_GLES_LIB menimpa keduanya untuk keperluan tes. */
static const char *const kCandidates[] = {
    "libGLESv3.so", "libGLESv2.so",
    "libGLESv2.so.2", "libGLESv3.so.2",
    0
};

static const char *const kEglCandidates[] = {
    "libEGL.so", "libEGL.so.1", 0
};

static void *sym(const char *name) {
    void *p = s_gles_lib ? dlsym(s_gles_lib, name) : 0;
    if (!p && s_egl_gpa) p = s_egl_gpa(name);   /* ekstensi lewat EGL */
    return p;
}

bool driver_load() {
    if (s_gles_lib) return true;

    if (const char *env = getenv("ORYON_GLES_LIB"))
        s_gles_lib = dlopen(env, RTLD_NOW | RTLD_LOCAL);
    for (int i = 0; !s_gles_lib && kCandidates[i]; ++i)
        s_gles_lib = dlopen(kCandidates[i], RTLD_NOW | RTLD_LOCAL);
    if (!s_gles_lib) return false;

    for (int i = 0; !s_egl_lib && kEglCandidates[i]; ++i)
        s_egl_lib = dlopen(kEglCandidates[i], RTLD_NOW | RTLD_LOCAL);
    if (s_egl_lib)
        s_egl_gpa = (void *(*)(const char *)) dlsym(s_egl_lib, "eglGetProcAddress");

#define ORYON_GLES(ret, name, params, args) \
    gles.name = (ret (*) params) sym(#name);
#include "gles_procs.inc"

    /* Tiga ini adalah syarat hidup: LWJGL memanggilnya sebelum apa pun. */
    return gles.glGetError && gles.glGetString && gles.glGetIntegerv;
}

void *egl_get_proc(const char *name) {
    return s_egl_gpa ? s_egl_gpa(name) : 0;
}

}  /* namespace oryon */
