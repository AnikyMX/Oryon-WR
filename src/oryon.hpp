// Oryon -- desktop OpenGL (LWJGL 3.3.6 / Minecraft 1.12.2..1.16.5) -> OpenGL ES 3.2 translation layer.
// Loaded by the launcher with dlopen() and resolved by LWJGL with dlsym(). No JNI.
#pragma once
#define GL_GLES_PROTOTYPES 0
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// desktop-only scalar types (ABI identical to LWJGL NativeType)
typedef double GLdouble;
typedef double GLclampd;
typedef unsigned int GLhandleARB;
typedef char GLcharARB;
typedef khronos_ssize_t GLsizeiptrARB;
typedef khronos_intptr_t GLintptrARB;
typedef unsigned short GLhalfARB;

#include "gen/gl_desktop.hpp"

#define OGL_EXPORT extern "C" __attribute__((visibility("default")))
#define ORY_INLINE inline __attribute__((always_inline))
#define ORY_NOINLINE __attribute__((noinline))
#define ORY_COLD __attribute__((cold, noinline))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#ifndef ORYON_VERSION
#define ORYON_VERSION "0.1.0"
#endif

namespace ory {
// Real GLES 3.2 driver entry points (dlopen/dlsym at load, eglGetProcAddress for extensions).
struct EsFuncs {
#define ES_FN(r, n, p, x) r (GL_APIENTRY *n) p;
#include "gen/es_funcs.inc"
#undef ES_FN
};
extern EsFuncs es;
void log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
} // namespace ory

#include "state.hpp"
#include "dlist.hpp"

// Every state-touching entry point starts with this: one load + one predicted branch.
#define ORY_PROLOGUE() do { if (UNLIKELY(::ory::g.hooks)) ::ory::run_hooks(); } while (0)
