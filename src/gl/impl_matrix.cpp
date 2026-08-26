/* Tumpukan matriks fixed-function. Dihitung di CPU, dikirim sebagai uniform
   saat menggambar - bukan tiap panggilan, karena Minecraft merangkai puluhan
   glTranslate/glRotate untuk satu draw call.

   ORYON_IMPL(glMatrixMode)
   ORYON_IMPL(glLoadIdentity)
   ORYON_IMPL(glPushMatrix)
   ORYON_IMPL(glPopMatrix)
   ORYON_IMPL(glTranslatef)
   ORYON_IMPL(glTranslated)
   ORYON_IMPL(glRotatef)
   ORYON_IMPL(glScalef)
   ORYON_IMPL(glScaled)
   ORYON_IMPL(glMultMatrixf)
   ORYON_IMPL(glOrtho)
   ORYON_IMPL(glLoadTransposeMatrixf)
   ORYON_IMPL(glLoadTransposeMatrixd)
   ORYON_IMPL(glMultTransposeMatrixf)
   ORYON_IMPL(glMultTransposeMatrixd)
*/

#include "oryon/oryon.h"
#include "../state/ff_state.h"
#include "../state/state.h"
#include "../util/mat4.h"

using namespace oryon;

namespace {

ORYON_INLINE void touch() { ++g_ff.serial; }

/* Tumpukan mana yang aktif, berikut batas kedalamannya. */
struct Stack { float (*base)[16]; int *top; int depth; };

Stack current_stack() {
    switch (g_ff.mode) {
    case GL_PROJECTION: { Stack s = { g_ff.pr, &g_ff.pr_top, FF_PR_DEPTH }; return s; }
    case GL_TEXTURE: {
        const int u = g_ff.a.active_tex;
        Stack s = { g_ff.tx[u], &g_ff.tx_top[u], FF_TX_DEPTH };
        return s;
    }
    default: { Stack s = { g_ff.mv, &g_ff.mv_top, FF_MV_DEPTH }; return s; }
    }
}

void d2f(float *dst, const GLdouble *src, int n) {
    for (int i = 0; i < n; ++i) dst[i] = (float) src[i];
}

}  /* namespace */

ORYON_API void glMatrixMode(GLenum mode) {
    if (mode != GL_MODELVIEW && mode != GL_PROJECTION && mode != GL_TEXTURE) {
        set_error(GL_INVALID_ENUM);
        return;
    }
    g_ff.mode = mode;
}

ORYON_API void glLoadIdentity(void) {
    mat4_identity(ff_current_matrix());
    touch();
}

ORYON_API void glPushMatrix(void) {
    Stack s = current_stack();
    if (*s.top + 1 >= s.depth) { set_error(GL_STACK_OVERFLOW); return; }
    mat4_copy(s.base[*s.top + 1], s.base[*s.top]);
    ++*s.top;
}

ORYON_API void glPopMatrix(void) {
    Stack s = current_stack();
    if (*s.top == 0) { set_error(GL_STACK_UNDERFLOW); return; }
    --*s.top;
    touch();
}

ORYON_API void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    mat4_translate(ff_current_matrix(), x, y, z);
    touch();
}

ORYON_API void glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    mat4_translate(ff_current_matrix(), (float) x, (float) y, (float) z);
    touch();
}

ORYON_API void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z) {
    mat4_rotate(ff_current_matrix(), a, x, y, z);
    touch();
}

ORYON_API void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    mat4_scale(ff_current_matrix(), x, y, z);
    touch();
}

ORYON_API void glScaled(GLdouble x, GLdouble y, GLdouble z) {
    mat4_scale(ff_current_matrix(), (float) x, (float) y, (float) z);
    touch();
}

ORYON_API void glMultMatrixf(const GLfloat *m) {
    if (!m) return;
    float *c = ff_current_matrix();
    mat4_mul(c, c, m);
    touch();
}

ORYON_API void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
                       GLdouble n, GLdouble f) {
    mat4_ortho(ff_current_matrix(), l, r, b, t, n, f);
    touch();
}

ORYON_API void glLoadTransposeMatrixf(const GLfloat *m) {
    if (!m) return;
    mat4_transpose(ff_current_matrix(), m);
    touch();
}

ORYON_API void glLoadTransposeMatrixd(const GLdouble *m) {
    if (!m) return;
    float f[16];
    d2f(f, m, 16);
    mat4_transpose(ff_current_matrix(), f);
    touch();
}

ORYON_API void glMultTransposeMatrixf(const GLfloat *m) {
    if (!m) return;
    float t[16], *c = ff_current_matrix();
    mat4_transpose(t, m);
    mat4_mul(c, c, t);
    touch();
}

ORYON_API void glMultTransposeMatrixd(const GLdouble *m) {
    if (!m) return;
    float f[16], t[16], *c = ff_current_matrix();
    d2f(f, m, 16);
    mat4_transpose(t, f);
    mat4_mul(c, c, t);
    touch();
}
