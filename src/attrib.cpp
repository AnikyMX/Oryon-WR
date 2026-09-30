// Oryon -- glPushAttrib / glPopAttrib (server attribute stack). Emulated state is copied directly;
// ES-side state is queried only at push time (no shadowing cost on the hot path).
#include "oryon.hpp"

using namespace ory;

namespace {
struct Frame {
    GLbitfield mask;
    // enable / emulated
    bool alpha_test, lighting, color_material, normalize, rescale, fog, color_sum, logic_op, point_sprite;
    uint8_t light_mask, clip_mask, tex_en[MAX_TEX_UNITS], gen_on[MAX_TEX_UNITS];
    GLboolean e_blend, e_cull, e_depth, e_stencil, e_scissor, e_dither, e_pofs, e_a2c, e_cov;
    // lighting
    Light l[MAX_LIGHTS]; Material mat[2]; Vec4 model_amb; bool local_viewer, two_side, sep_spec;
    GLenum cm_face, cm_mode, shade;
    // current
    CurAttr cur;
    // color buffer
    GLenum alpha_func; GLfloat alpha_ref; GLint bsrc_rgb, bdst_rgb, bsrc_a, bdst_a, beq_rgb, beq_a;
    GLfloat bcolor[4]; GLboolean cmask[4]; GLfloat clear_color[4]; GLenum logic_mode;
    // depth
    GLint depth_func; GLboolean depth_mask; GLfloat clear_depth;
    // fog
    GLenum fog_mode, fog_src, fog_dist; GLfloat fog_density, fog_start, fog_end, fog_index; Vec4 fog_color;
    // transform
    GLenum matrix_mode; Vec4 clip[MAX_CLIP_PLANES];
    // viewport
    GLint viewport[4]; GLfloat depth_range[2];
    // polygon
    GLint cull_mode, front_face; GLfloat pofs_factor, pofs_units; GLenum poly_mode[2];
    // scissor / line / point
    GLint scissor[4]; GLfloat line_width, point_size;
    // texture
    GLuint active; TexEnv env[MAX_TEX_UNITS]; TexGen gen[MAX_TEX_UNITS]; GLuint bind2d[MAX_TEX_UNITS];
    // stencil
    GLint st_func, st_ref, st_vmask, st_fail, st_zfail, st_zpass, st_wmask, st_clear;
};
Frame g_stack[ATTRIB_STACK];
int g_top = 0;
}
int ory::attrib_depth() { return g_top; }

/* jar: GL11.glPushAttrib(I)V */
OGL_EXPORT void glPushAttrib(GLbitfield mask) { ORY_DL(glPushAttrib, mask);
    ORY_PROLOGUE();
    if (g_top >= ATTRIB_STACK) { set_error(GL_STACK_OVERFLOW); return; }
    Frame &F = g_stack[g_top++];
    Ffp &f = g.f;
    F.mask = mask;
    if (mask & GL_ENABLE_BIT) {
        F.alpha_test = f.alpha_test; F.lighting = f.lighting; F.color_material = f.color_material; F.normalize = f.normalize;
        F.rescale = f.rescale; F.fog = f.fog; F.color_sum = f.color_sum; F.logic_op = f.logic_op; F.point_sprite = f.point_sprite;
        F.light_mask = f.light_mask; F.clip_mask = f.clip_mask;
        for (int u = 0; u < MAX_TEX_UNITS; ++u) { F.tex_en[u] = f.tu[u].enabled; F.gen_on[u] = f.tu[u].gen.on; }
        F.e_blend = es.glIsEnabled(GL_BLEND); F.e_cull = es.glIsEnabled(GL_CULL_FACE); F.e_depth = es.glIsEnabled(GL_DEPTH_TEST);
        F.e_stencil = es.glIsEnabled(GL_STENCIL_TEST); F.e_scissor = es.glIsEnabled(GL_SCISSOR_TEST); F.e_dither = es.glIsEnabled(GL_DITHER);
        F.e_pofs = es.glIsEnabled(GL_POLYGON_OFFSET_FILL); F.e_a2c = es.glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE); F.e_cov = es.glIsEnabled(GL_SAMPLE_COVERAGE);
    }
    if (mask & GL_LIGHTING_BIT) {
        memcpy(F.l, f.l, sizeof F.l); memcpy(F.mat, f.mat, sizeof F.mat); F.model_amb = f.model_amb;
        F.local_viewer = f.local_viewer; F.two_side = f.two_side; F.sep_spec = f.sep_spec;
        F.cm_face = f.cm_face; F.cm_mode = f.cm_mode; F.shade = f.shade;
        if (!(mask & GL_ENABLE_BIT)) { F.lighting = f.lighting; F.color_material = f.color_material; F.light_mask = f.light_mask; }
    }
    if (mask & GL_CURRENT_BIT) F.cur = g.cur;
    if (mask & GL_COLOR_BUFFER_BIT) {
        F.alpha_func = f.alpha_func; F.alpha_ref = f.alpha_ref; F.logic_mode = f.logic_mode;
        es.glGetIntegerv(GL_BLEND_SRC_RGB, &F.bsrc_rgb); es.glGetIntegerv(GL_BLEND_DST_RGB, &F.bdst_rgb);
        es.glGetIntegerv(GL_BLEND_SRC_ALPHA, &F.bsrc_a); es.glGetIntegerv(GL_BLEND_DST_ALPHA, &F.bdst_a);
        es.glGetIntegerv(GL_BLEND_EQUATION_RGB, &F.beq_rgb); es.glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &F.beq_a);
        es.glGetFloatv(GL_BLEND_COLOR, F.bcolor); es.glGetBooleanv(GL_COLOR_WRITEMASK, F.cmask);
        es.glGetFloatv(GL_COLOR_CLEAR_VALUE, F.clear_color);
        if (!(mask & GL_ENABLE_BIT)) { F.alpha_test = f.alpha_test; F.e_blend = es.glIsEnabled(GL_BLEND); F.e_dither = es.glIsEnabled(GL_DITHER); F.logic_op = f.logic_op; }
    }
    if (mask & GL_DEPTH_BUFFER_BIT) {
        es.glGetIntegerv(GL_DEPTH_FUNC, &F.depth_func); es.glGetBooleanv(GL_DEPTH_WRITEMASK, &F.depth_mask);
        es.glGetFloatv(GL_DEPTH_CLEAR_VALUE, &F.clear_depth);
        if (!(mask & GL_ENABLE_BIT)) F.e_depth = es.glIsEnabled(GL_DEPTH_TEST);
    }
    if (mask & GL_FOG_BIT) {
        F.fog_mode = f.fog_mode; F.fog_src = f.fog_src; F.fog_dist = f.fog_dist; F.fog_density = f.fog_density;
        F.fog_start = f.fog_start; F.fog_end = f.fog_end; F.fog_index = f.fog_index; F.fog_color = f.fog_color;
        if (!(mask & GL_ENABLE_BIT)) F.fog = f.fog;
    }
    if (mask & GL_TRANSFORM_BIT) {
        F.matrix_mode = g.m.mode; memcpy(F.clip, f.clip, sizeof F.clip);
        if (!(mask & GL_ENABLE_BIT)) { F.normalize = f.normalize; F.rescale = f.rescale; F.clip_mask = f.clip_mask; }
    }
    if (mask & GL_VIEWPORT_BIT) { es.glGetIntegerv(GL_VIEWPORT, F.viewport); es.glGetFloatv(GL_DEPTH_RANGE, F.depth_range); }
    if (mask & GL_POLYGON_BIT) {
        es.glGetIntegerv(GL_CULL_FACE_MODE, &F.cull_mode); es.glGetIntegerv(GL_FRONT_FACE, &F.front_face);
        es.glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &F.pofs_factor); es.glGetFloatv(GL_POLYGON_OFFSET_UNITS, &F.pofs_units);
        F.poly_mode[0] = f.poly_mode[0]; F.poly_mode[1] = f.poly_mode[1];
        if (!(mask & GL_ENABLE_BIT)) { F.e_cull = es.glIsEnabled(GL_CULL_FACE); F.e_pofs = es.glIsEnabled(GL_POLYGON_OFFSET_FILL); }
    }
    if (mask & GL_SCISSOR_BIT) { es.glGetIntegerv(GL_SCISSOR_BOX, F.scissor); if (!(mask & GL_ENABLE_BIT)) F.e_scissor = es.glIsEnabled(GL_SCISSOR_TEST); }
    if (mask & GL_LINE_BIT) F.line_width = f.line_width;
    if (mask & GL_POINT_BIT) F.point_size = f.point_size;
    if (mask & GL_TEXTURE_BIT) {
        F.active = f.active;
        for (int u = 0; u < MAX_TEX_UNITS; ++u) { F.env[u] = f.tu[u].env; F.gen[u] = f.tu[u].gen; F.bind2d[u] = g.tex_bind[u][0]; }
        if (!(mask & GL_ENABLE_BIT)) for (int u = 0; u < MAX_TEX_UNITS; ++u) { F.tex_en[u] = f.tu[u].enabled; F.gen_on[u] = f.tu[u].gen.on; }
    }
    if (mask & GL_STENCIL_BUFFER_BIT) {
        es.glGetIntegerv(GL_STENCIL_FUNC, &F.st_func); es.glGetIntegerv(GL_STENCIL_REF, &F.st_ref);
        es.glGetIntegerv(GL_STENCIL_VALUE_MASK, &F.st_vmask); es.glGetIntegerv(GL_STENCIL_FAIL, &F.st_fail);
        es.glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &F.st_zfail); es.glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &F.st_zpass);
        es.glGetIntegerv(GL_STENCIL_WRITEMASK, &F.st_wmask); es.glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &F.st_clear);
        if (!(mask & GL_ENABLE_BIT)) F.e_stencil = es.glIsEnabled(GL_STENCIL_TEST);
    }
}

static inline void es_cap(GLenum cap, GLboolean on) { if (on) es.glEnable(cap); else es.glDisable(cap); }

/* jar: GL11.glPopAttrib()V */
OGL_EXPORT void glPopAttrib(void) { ORY_DL(glPopAttrib);
    ORY_PROLOGUE();
    if (g_top <= 0) { set_error(GL_STACK_UNDERFLOW); return; }
    Frame &F = g_stack[--g_top];
    Ffp &f = g.f;
    GLbitfield mask = F.mask;
    bool en = mask & GL_ENABLE_BIT;
    if (en || (mask & GL_LIGHTING_BIT)) { f.lighting = F.lighting; f.color_material = F.color_material; f.light_mask = F.light_mask; }
    if (en || (mask & GL_COLOR_BUFFER_BIT)) { f.alpha_test = F.alpha_test; f.logic_op = F.logic_op; es_cap(GL_BLEND, F.e_blend); es_cap(GL_DITHER, F.e_dither); }
    if (en || (mask & GL_DEPTH_BUFFER_BIT)) es_cap(GL_DEPTH_TEST, F.e_depth);
    if (en || (mask & GL_FOG_BIT)) f.fog = F.fog;
    if (en || (mask & GL_TRANSFORM_BIT)) { f.normalize = F.normalize; f.rescale = F.rescale; f.clip_mask = F.clip_mask; }
    if (en || (mask & GL_POLYGON_BIT)) { es_cap(GL_CULL_FACE, F.e_cull); es_cap(GL_POLYGON_OFFSET_FILL, F.e_pofs); }
    if (en || (mask & GL_SCISSOR_BIT)) es_cap(GL_SCISSOR_TEST, F.e_scissor);
    if (en || (mask & GL_STENCIL_BUFFER_BIT)) es_cap(GL_STENCIL_TEST, F.e_stencil);
    if (en || (mask & GL_TEXTURE_BIT)) for (int u = 0; u < MAX_TEX_UNITS; ++u) { f.tu[u].enabled = F.tex_en[u]; f.tu[u].gen.on = F.gen_on[u]; }
    if (en) {
        f.color_sum = F.color_sum; f.point_sprite = F.point_sprite;
        es_cap(GL_SAMPLE_ALPHA_TO_COVERAGE, F.e_a2c); es_cap(GL_SAMPLE_COVERAGE, F.e_cov);
    }
    if (mask & GL_LIGHTING_BIT) {
        memcpy(f.l, F.l, sizeof F.l); memcpy(f.mat, F.mat, sizeof F.mat); f.model_amb = F.model_amb;
        f.local_viewer = F.local_viewer; f.two_side = F.two_side; f.sep_spec = F.sep_spec;
        f.cm_face = F.cm_face; f.cm_mode = F.cm_mode; f.shade = F.shade;
        f.v_light = next_ver(); f.v_mat = next_ver();
    }
    if (mask & GL_CURRENT_BIT) g.cur = F.cur;
    if (mask & GL_COLOR_BUFFER_BIT) {
        f.alpha_func = F.alpha_func; f.alpha_ref = F.alpha_ref; f.logic_mode = F.logic_mode; f.v_alpha = next_ver();
        es.glBlendFuncSeparate((GLenum)F.bsrc_rgb, (GLenum)F.bdst_rgb, (GLenum)F.bsrc_a, (GLenum)F.bdst_a);
        es.glBlendEquationSeparate((GLenum)F.beq_rgb, (GLenum)F.beq_a);
        es.glBlendColor(F.bcolor[0], F.bcolor[1], F.bcolor[2], F.bcolor[3]);
        es.glColorMask(F.cmask[0], F.cmask[1], F.cmask[2], F.cmask[3]);
        es.glClearColor(F.clear_color[0], F.clear_color[1], F.clear_color[2], F.clear_color[3]);
    }
    if (mask & GL_DEPTH_BUFFER_BIT) { es.glDepthFunc((GLenum)F.depth_func); es.glDepthMask(F.depth_mask); es.glClearDepthf(F.clear_depth); }
    if (mask & GL_FOG_BIT) {
        f.fog_mode = F.fog_mode; f.fog_src = F.fog_src; f.fog_dist = F.fog_dist; f.fog_density = F.fog_density;
        f.fog_start = F.fog_start; f.fog_end = F.fog_end; f.fog_index = F.fog_index; f.fog_color = F.fog_color; f.v_fog = next_ver();
    }
    if (mask & GL_TRANSFORM_BIT) { g.m.mode = F.matrix_mode; memcpy(f.clip, F.clip, sizeof F.clip); f.v_clip = next_ver(); mat_reselect(); }
    if (mask & GL_VIEWPORT_BIT) { es.glViewport(F.viewport[0], F.viewport[1], F.viewport[2], F.viewport[3]); es.glDepthRangef(F.depth_range[0], F.depth_range[1]); }
    if (mask & GL_POLYGON_BIT) {
        es.glCullFace((GLenum)F.cull_mode); es.glFrontFace((GLenum)F.front_face); es.glPolygonOffset(F.pofs_factor, F.pofs_units);
        f.poly_mode[0] = F.poly_mode[0]; f.poly_mode[1] = F.poly_mode[1];
    }
    if (mask & GL_SCISSOR_BIT) es.glScissor(F.scissor[0], F.scissor[1], F.scissor[2], F.scissor[3]);
    if (mask & GL_LINE_BIT) { f.line_width = F.line_width; es.glLineWidth(F.line_width); }
    if (mask & GL_POINT_BIT) { f.point_size = F.point_size; f.v_point = next_ver(); }
    if (mask & GL_TEXTURE_BIT) {
        for (int u = 0; u < MAX_TEX_UNITS; ++u) {
            f.tu[u].env = F.env[u]; f.tu[u].gen = F.gen[u];
            if (g.tex_bind[u][0] != F.bind2d[u]) {
                es.glActiveTexture(GL_TEXTURE0 + u); es.glBindTexture(GL_TEXTURE_2D, F.bind2d[u]); g.tex_bind[u][0] = F.bind2d[u];
            }
        }
        f.active = F.active; es.glActiveTexture(GL_TEXTURE0 + F.active);
        f.v_env = next_ver(); f.v_gen = next_ver();
        if (g.m.mode == GL_TEXTURE) mat_reselect();
    }
    if (mask & GL_STENCIL_BUFFER_BIT) {
        es.glStencilFunc((GLenum)F.st_func, F.st_ref, (GLuint)F.st_vmask);
        es.glStencilOp((GLenum)F.st_fail, (GLenum)F.st_zfail, (GLenum)F.st_zpass);
        es.glStencilMask((GLuint)F.st_wmask); es.glClearStencil(F.st_clear);
    }
    f.key_dirty = true;
    f.v_light = next_ver();
}
/* jar: GL11.glPushClientAttrib(I)V */
OGL_EXPORT void glPushClientAttrib(GLbitfield mask) { ORY_PROLOGUE(); (void)mask; }
/* jar: GL11.glPopClientAttrib()V */
OGL_EXPORT void glPopClientAttrib(void) { ORY_PROLOGUE(); }
