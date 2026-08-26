/* Kueri state. Oryon yang menjawab, bukan driver: string versi menentukan
   jalur render yang dipilih Minecraft, dan sebagian besar pname fixed-function
   tidak dikenal GLES sama sekali.

   ORYON_IMPL(glGetString)
   ORYON_IMPL(glGetError)
   ORYON_IMPL(glGetIntegerv)
   ORYON_IMPL(glGetFloatv)
*/

#include "oryon/oryon.h"
#include "../driver/driver.h"
#include "../state/state.h"
#include "../state/ff_state.h"

#include <string.h>

using namespace oryon;

ORYON_API const GLubyte *glGetString(GLenum name) {
    if (ORYON_UNLIKELY(!ensure_init())) return 0;
    switch (name) {
    case GL_VERSION:                  return (const GLubyte *) g_state.version;
    case GL_SHADING_LANGUAGE_VERSION: return (const GLubyte *) g_state.glsl;
    case GL_VENDOR:                   return (const GLubyte *) g_state.vendor;
    case GL_RENDERER:                 return (const GLubyte *) g_state.renderer;
    case GL_EXTENSIONS:               return (const GLubyte *) g_state.extensions;
    default:
        set_error(GL_INVALID_ENUM);
        return 0;
    }
}

/* Semantik GL: error Oryon lebih dulu, lalu error driver. Satu per panggilan. */
ORYON_API GLenum glGetError(void) {
    if (ORYON_UNLIKELY(!ensure_init())) return GL_NO_ERROR;
    GLenum e = g_state.error;
    if (e != GL_NO_ERROR) {
        g_state.error = GL_NO_ERROR;
        return e;
    }
    return gles.glGetError();
}

/* Batas fixed-function yang tidak ada di GLES. Angka-angka ini adalah janji
   yang harus ditepati generator shader; lihat FF_MAX_* di ff_state.h. */
ORYON_API void glGetIntegerv(GLenum pname, GLint *params) {
    if (ORYON_UNLIKELY(!ensure_init() || !params)) return;
    const FFAttribState &a = g_ff.a;
    switch (pname) {
    case GL_MAX_TEXTURE_UNITS:           *params = FF_MAX_TEX; return;
    case GL_MAX_LIGHTS:                  *params = FF_MAX_LIGHTS; return;
    case GL_MAX_MODELVIEW_STACK_DEPTH:   *params = FF_MV_DEPTH; return;
    case GL_MAX_PROJECTION_STACK_DEPTH:  *params = FF_PR_DEPTH; return;
    case GL_MAX_TEXTURE_STACK_DEPTH:     *params = FF_TX_DEPTH; return;
    case GL_MAX_CLIP_PLANES:             *params = 6; return;
    case GL_MAJOR_VERSION:               *params = 2; return;
    case GL_MINOR_VERSION:               *params = 1; return;

    case GL_MATRIX_MODE:                 *params = (GLint) g_ff.mode; return;
    case GL_MODELVIEW_STACK_DEPTH:       *params = g_ff.mv_top + 1; return;
    case GL_PROJECTION_STACK_DEPTH:      *params = g_ff.pr_top + 1; return;
    case GL_TEXTURE_STACK_DEPTH:         *params = g_ff.tx_top[a.active_tex] + 1; return;
    case GL_ATTRIB_STACK_DEPTH:          *params = g_ff.saved_top; return;
    case GL_ACTIVE_TEXTURE:              *params = GL_TEXTURE0 + a.active_tex; return;
    case GL_CLIENT_ACTIVE_TEXTURE:       *params = GL_TEXTURE0 + a.client_tex; return;
    case GL_ALPHA_TEST_FUNC:             *params = (GLint) a.alpha_func; return;
    case GL_SHADE_MODEL:                 *params = a.flat_shade ? GL_FLAT : GL_SMOOTH; return;
    case GL_FOG_MODE:
        *params = a.fog_mode == FOG_LINEAR ? GL_LINEAR
                : a.fog_mode == FOG_EXP2   ? GL_EXP2 : GL_EXP;
        return;
    case GL_TEXTURE_ENV_MODE:
        *params = a.tex[a.active_tex].env_mode == ENV_REPLACE ? GL_REPLACE : GL_MODULATE;
        return;
    default:
        gles.glGetIntegerv(pname, params);
        return;
    }
}

/* Minecraft membaca GL_MODELVIEW_MATRIX dan GL_PROJECTION_MATRIX tiap frame di
   ActiveRenderInfo untuk menempatkan partikel dan memilih entitas. Jawaban di
   sini harus persis sama dengan yang dipakai shader, kalau tidak partikel akan
   melayang lepas dari dunianya. */
ORYON_API void glGetFloatv(GLenum pname, GLfloat *params) {
    if (ORYON_UNLIKELY(!ensure_init() || !params)) return;
    const FFAttribState &a = g_ff.a;
    switch (pname) {
    case GL_MODELVIEW_MATRIX:
        memcpy(params, g_ff.mv[g_ff.mv_top], 16 * sizeof(float)); return;
    case GL_PROJECTION_MATRIX:
        memcpy(params, g_ff.pr[g_ff.pr_top], 16 * sizeof(float)); return;
    case GL_TEXTURE_MATRIX:
        memcpy(params, ff_tex_matrix(a.active_tex), 16 * sizeof(float)); return;
    case GL_CURRENT_COLOR:
        memcpy(params, a.cur_color, 4 * sizeof(float)); return;
    case GL_CURRENT_NORMAL:
        memcpy(params, a.cur_normal, 3 * sizeof(float)); return;
    case GL_FOG_COLOR:
        memcpy(params, a.fog_color, 4 * sizeof(float)); return;
    case GL_LIGHT_MODEL_AMBIENT:
        memcpy(params, a.lm_ambient, 4 * sizeof(float)); return;
    case GL_ALPHA_TEST_REF:  *params = a.alpha_ref; return;
    case GL_FOG_START:       *params = a.fog_start; return;
    case GL_FOG_END:         *params = a.fog_end; return;
    case GL_FOG_DENSITY:     *params = a.fog_density; return;
    default:
        gles.glGetFloatv(pname, params);
        return;
    }
}
