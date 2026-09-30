/* tools/bench/drawshim.c -- test-only GLES shim: counts real ES draw calls, forwards to Mesa libGLESv2.
 * Used via ORYON_GLES_LIB; functions it does not export resolve through eglGetProcAddress in liboryon's loader. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
typedef unsigned int GLenum; typedef int GLint; typedef int GLsizei; typedef unsigned int GLuint;
static void *real;
static void *R(const char *n) { if (!real) real = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_LOCAL); return dlsym(real, n); }
static unsigned long draws;
static int nodraw = -1;
#define FWD (nodraw < 0 ? (nodraw = getenv("SHIM_NODRAW") != 0) : nodraw) == 0
unsigned long shim_draw_count(void) { return draws; }
void glDrawArrays(GLenum m, GLint f, GLsizei c) { static void (*p)(GLenum, GLint, GLsizei); if (!p) p = R("glDrawArrays"); ++draws; if (FWD) p(m, f, c); }
void glDrawElements(GLenum m, GLsizei c, GLenum t, const void *i) { static void (*p)(GLenum, GLsizei, GLenum, const void *); if (!p) p = R("glDrawElements"); ++draws; if (FWD) p(m, c, t, i); }
void glDrawElementsBaseVertex(GLenum m, GLsizei c, GLenum t, const void *i, GLint b) { static void (*p)(GLenum, GLsizei, GLenum, const void *, GLint); if (!p) p = R("glDrawElementsBaseVertex"); ++draws; if (FWD) p(m, c, t, i, b); }
void glDrawRangeElements(GLenum m, GLuint s, GLuint e, GLsizei c, GLenum t, const void *i) { static void (*p)(GLenum, GLuint, GLuint, GLsizei, GLenum, const void *); if (!p) p = R("glDrawRangeElements"); ++draws; if (FWD) p(m, s, e, c, t, i); }
