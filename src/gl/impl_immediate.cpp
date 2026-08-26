/* Mode langsung. Minecraft memakainya untuk GUI, teks, dan debug - bukan untuk
   terrain - jadi yang dikejar di sini kebenaran, bukan bandwidth.

   Hanya varian yang benar-benar dipanggil 1.12.2 yang diimplementasikan;
   itu terbaca dari call site di 1.12.2.jar, bukan dari tebakan.

   ORYON_IMPL(glBegin)
   ORYON_IMPL(glEnd)
   ORYON_IMPL(glVertex3f)
   ORYON_IMPL(glTexCoord2f)
   ORYON_IMPL(glMultiTexCoord2f)
   ORYON_IMPL(glMultiTexCoord2fARB)
*/

#include "oryon/oryon.h"
#include "../draw/vertexpipe.h"
#include "../state/ff_state.h"
#include "../state/state.h"

using namespace oryon;

ORYON_API void glBegin(GLenum mode) {
    if (!ensure_init()) return;
    if (imm_active()) { set_error(GL_INVALID_OPERATION); return; }
    imm_begin(mode);
}

ORYON_API void glEnd(void) {
    if (!imm_active()) { set_error(GL_INVALID_OPERATION); return; }
    imm_end();
}

ORYON_API void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    imm_vertex(x, y, z);
}

ORYON_API void glTexCoord2f(GLfloat s, GLfloat t) {
    g_ff.a.cur_tex[0][0] = s;
    g_ff.a.cur_tex[0][1] = t;
    ff_touch_uniform();
}

namespace {
void multi_tex(GLenum target, GLfloat s, GLfloat t) {
    const int u = (int) (target - GL_TEXTURE0);
    if (u < 0 || u >= FF_MAX_TEX) { set_error(GL_INVALID_ENUM); return; }
    g_ff.a.cur_tex[u][0] = s;
    g_ff.a.cur_tex[u][1] = t;
    ff_touch_uniform();
}
}  /* namespace */

ORYON_API void glMultiTexCoord2f(GLenum t, GLfloat s, GLfloat v)    { multi_tex(t, s, v); }
ORYON_API void glMultiTexCoord2fARB(GLenum t, GLfloat s, GLfloat v) { multi_tex(t, s, v); }
