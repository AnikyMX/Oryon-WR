// Oryon -- fixed-function state: enables, lighting, material, fog, texenv, texgen, alpha test, clip planes.
// Pure CPU state; consumed by the program generator (ffp_prog.cpp) through key/uniform versions.
#include "oryon.hpp"
#include "mathx.hpp"
#include "imm.hpp"

namespace ory {

ORY_INLINE void key_dirty() { g.f.key_dirty = true; }
static inline bool nonzero3(const Vec4 &v) { return v.x != 0.0f || v.y != 0.0f || v.z != 0.0f; }
static inline Vec4 v4(const GLfloat *p) { return Vec4{p[0], p[1], p[2], p[3]}; }

void ffp_defaults() {
    Ffp &f = g.f;
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        Light &l = f.l[i];
        l.amb = {0, 0, 0, 1};
        l.dif = i == 0 ? Vec4{1, 1, 1, 1} : Vec4{0, 0, 0, 1};
        l.spe = i == 0 ? Vec4{1, 1, 1, 1} : Vec4{0, 0, 0, 1};
        l.pos = {0, 0, 1, 0}; l.dir = {0, 0, -1, 0};
        l.spot_exp = 0; l.spot_cut = 180; l.k0 = 1; l.k1 = 0; l.k2 = 0;
    }
    for (int s = 0; s < 2; ++s) f.mat[s] = Material{{0.2f, 0.2f, 0.2f, 1}, {0.8f, 0.8f, 0.8f, 1}, {0, 0, 0, 1}, {0, 0, 0, 1}, 0};
    for (int u = 0; u < MAX_TEX_UNITS; ++u) {
        TexUnit &t = f.tu[u];
        memset(&t, 0, sizeof t);
        TexEnv &e = t.env;
        e.mode = GL_MODULATE; e.comb_rgb = e.comb_a = GL_MODULATE;
        e.src_rgb[0] = e.src_a[0] = GL_TEXTURE; e.src_rgb[1] = e.src_a[1] = GL_PREVIOUS; e.src_rgb[2] = e.src_a[2] = GL_CONSTANT;
        e.op_rgb[0] = e.op_rgb[1] = GL_SRC_COLOR; e.op_rgb[2] = GL_SRC_ALPHA;
        e.op_a[0] = e.op_a[1] = e.op_a[2] = GL_SRC_ALPHA;
        e.scale_rgb = e.scale_a = 1;
        for (int c = 0; c < 4; ++c) t.gen.mode[c] = GL_EYE_LINEAR;
        t.gen.obj[0] = t.gen.eye[0] = {1, 0, 0, 0};
        t.gen.obj[1] = t.gen.eye[1] = {0, 1, 0, 0};
    }
}

// ------------------------------------------------------------------ enables
// returns true when handled by the emulation (not forwarded to ES)
bool ffp_enable(GLenum cap, bool on) {
    Ffp &f = g.f;
    if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + MAX_LIGHTS) {
        uint8_t b = (uint8_t)(1u << (cap - GL_LIGHT0)), m = on ? (f.light_mask | b) : (f.light_mask & ~b);
        if (m != f.light_mask) { f.light_mask = m; key_dirty(); f.v_light = next_ver(); }
        return true;
    }
    if (cap >= GL_CLIP_PLANE0 && cap < GL_CLIP_PLANE0 + MAX_CLIP_PLANES) {
        uint8_t b = (uint8_t)(1u << (cap - GL_CLIP_PLANE0)), m = on ? (f.clip_mask | b) : (f.clip_mask & ~b);
        if (m != f.clip_mask) { f.clip_mask = m; key_dirty(); f.v_clip = next_ver(); }
        return true;
    }
    TexUnit *tu = f.active < (GLuint)MAX_TEX_UNITS ? &f.tu[f.active] : nullptr;
    switch (cap) {
#define ORY_FLAG(C, F) case C: if (f.F != on) { f.F = on; key_dirty(); } return true;
    ORY_FLAG(GL_ALPHA_TEST, alpha_test)
    ORY_FLAG(GL_LIGHTING, lighting)
    ORY_FLAG(GL_NORMALIZE, normalize)
    ORY_FLAG(GL_RESCALE_NORMAL, rescale)
    ORY_FLAG(GL_FOG, fog)
    ORY_FLAG(GL_COLOR_SUM, color_sum)
    ORY_FLAG(GL_POINT_SPRITE, point_sprite)
#undef ORY_FLAG
    case GL_COLOR_MATERIAL:
        if (f.color_material != on) {
            if (!on) {        // tracked material parameters keep the last current color (GL semantics)
                for (int s = 0; s < 2; ++s) {
                    if (!(f.cm_face == GL_FRONT_AND_BACK || (s == 0 && f.cm_face == GL_FRONT) || (s == 1 && f.cm_face == GL_BACK))) continue;
                    Material &m = f.mat[s];
                    switch (f.cm_mode) {
                    case GL_AMBIENT: m.amb = g.cur.color; break;
                    case GL_DIFFUSE: m.dif = g.cur.color; break;
                    case GL_SPECULAR: m.spe = g.cur.color; break;
                    case GL_EMISSION: m.emi = g.cur.color; break;
                    default: m.amb = m.dif = g.cur.color; break;
                    }
                }
                f.v_mat = next_ver();
            }
            f.color_material = on; key_dirty();
        }
        return true;
    case GL_TEXTURE_1D: case GL_TEXTURE_2D: case GL_TEXTURE_3D: case GL_TEXTURE_CUBE_MAP: {
        if (!tu) return true;
        uint8_t b = cap == GL_TEXTURE_1D ? TGT_1D : cap == GL_TEXTURE_2D ? TGT_2D : cap == GL_TEXTURE_3D ? TGT_3D : TGT_CUBE;
        uint8_t m = on ? (tu->enabled | b) : (tu->enabled & ~b);
        if (m != tu->enabled) { tu->enabled = m; key_dirty(); }
        return true;
    }
    case GL_TEXTURE_GEN_S: case GL_TEXTURE_GEN_T: case GL_TEXTURE_GEN_R: case GL_TEXTURE_GEN_Q: {
        if (!tu) return true;
        uint8_t b = (uint8_t)(1u << (cap - GL_TEXTURE_GEN_S)), m = on ? (tu->gen.on | b) : (tu->gen.on & ~b);
        if (m != tu->gen.on) { tu->gen.on = m; key_dirty(); f.v_gen = next_ver(); }
        return true;
    }
    case GL_COLOR_LOGIC_OP: f.logic_op = on; return true;
    case GL_INDEX_LOGIC_OP: f.index_logic_op = on; return true;
    case GL_POLYGON_OFFSET_LINE: f.poly_offset_line = on; return true;
    case GL_POLYGON_OFFSET_POINT: f.poly_offset_point = on; return true;
    case GL_LINE_STIPPLE: f.line_stipple = on; return true;
    case GL_POLYGON_STIPPLE: f.poly_stipple = on; return true;
    case GL_POINT_SMOOTH: f.point_smooth = on; return true;
    case GL_LINE_SMOOTH: f.line_smooth = on; return true;
    case GL_POLYGON_SMOOTH: f.poly_smooth = on; return true;
    case GL_AUTO_NORMAL: case GL_MAP1_VERTEX_3: case GL_MAP1_VERTEX_4: case GL_MAP2_VERTEX_3: case GL_MAP2_VERTEX_4:
    case GL_MAP1_COLOR_4: case GL_MAP2_COLOR_4: case GL_MAP1_NORMAL: case GL_MAP2_NORMAL:
    case GL_MAP1_TEXTURE_COORD_1: case GL_MAP1_TEXTURE_COORD_2: case GL_MAP1_TEXTURE_COORD_3: case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP2_TEXTURE_COORD_1: case GL_MAP2_TEXTURE_COORD_2: case GL_MAP2_TEXTURE_COORD_3: case GL_MAP2_TEXTURE_COORD_4:
    case GL_MAP1_INDEX: case GL_MAP2_INDEX:
    case GL_MULTISAMPLE: case GL_SAMPLE_ALPHA_TO_ONE: case GL_VERTEX_PROGRAM_POINT_SIZE: case GL_VERTEX_PROGRAM_TWO_SIDE:
    case GL_TEXTURE_CUBE_MAP_SEAMLESS: case GL_FRAMEBUFFER_SRGB:
        return true;                                   // no ES equivalent, no visible effect needed
    case GL_DEPTH_CLAMP:
        if (g.escaps & ORY_ESCAP_DEPTH_CLAMP) return false;
        return true;
    default: return false;
    }
}

int ffp_is_enabled(GLenum cap) {          // -1: not emulated
    const Ffp &f = g.f;
    if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + MAX_LIGHTS) return (f.light_mask >> (cap - GL_LIGHT0)) & 1;
    if (cap >= GL_CLIP_PLANE0 && cap < GL_CLIP_PLANE0 + MAX_CLIP_PLANES) return (f.clip_mask >> (cap - GL_CLIP_PLANE0)) & 1;
    const TexUnit *tu = f.active < (GLuint)MAX_TEX_UNITS ? &f.tu[f.active] : nullptr;
    switch (cap) {
    case GL_ALPHA_TEST: return f.alpha_test;
    case GL_LIGHTING: return f.lighting;
    case GL_NORMALIZE: return f.normalize;
    case GL_RESCALE_NORMAL: return f.rescale;
    case GL_FOG: return f.fog;
    case GL_COLOR_SUM: return f.color_sum;
    case GL_POINT_SPRITE: return f.point_sprite;
    case GL_COLOR_MATERIAL: return f.color_material;
    case GL_TEXTURE_1D: return tu && (tu->enabled & TGT_1D);
    case GL_TEXTURE_2D: return tu && (tu->enabled & TGT_2D);
    case GL_TEXTURE_3D: return tu && (tu->enabled & TGT_3D);
    case GL_TEXTURE_CUBE_MAP: return tu && (tu->enabled & TGT_CUBE);
    case GL_TEXTURE_GEN_S: case GL_TEXTURE_GEN_T: case GL_TEXTURE_GEN_R: case GL_TEXTURE_GEN_Q:
        return tu && ((tu->gen.on >> (cap - GL_TEXTURE_GEN_S)) & 1);
    case GL_COLOR_LOGIC_OP: return f.logic_op;
    case GL_INDEX_LOGIC_OP: return f.index_logic_op;
    case GL_POLYGON_OFFSET_LINE: return f.poly_offset_line;
    case GL_POLYGON_OFFSET_POINT: return f.poly_offset_point;
    case GL_LINE_STIPPLE: return f.line_stipple;
    case GL_POLYGON_STIPPLE: return f.poly_stipple;
    case GL_POINT_SMOOTH: return f.point_smooth;
    case GL_LINE_SMOOTH: return f.line_smooth;
    case GL_POLYGON_SMOOTH: return f.poly_smooth;
    case GL_VERTEX_ARRAY: return (g.ca_on >> CA_VERTEX) & 1;
    case GL_NORMAL_ARRAY: return (g.ca_on >> CA_NORMAL) & 1;
    case GL_COLOR_ARRAY: return (g.ca_on >> CA_COLOR) & 1;
    case GL_SECONDARY_COLOR_ARRAY: return (g.ca_on >> CA_COLOR2) & 1;
    case GL_FOG_COORD_ARRAY: return (g.ca_on >> CA_FOG) & 1;
    case GL_TEXTURE_COORD_ARRAY: return (g.ca_on >> (CA_TEX0 + (int)f.client_active)) & 1;
    case GL_INDEX_ARRAY: case GL_EDGE_FLAG_ARRAY: return 0;
    case GL_DEPTH_CLAMP: return (g.escaps & ORY_ESCAP_DEPTH_CLAMP) ? -1 : 0;
    default: return -1;
    }
}

} // namespace ory

using namespace ory;

static TexUnit *active_tu() { return g.f.active < (GLuint)MAX_TEX_UNITS ? &g.f.tu[g.f.active] : nullptr; }

/* jar: GL11C.glEnable(I)V */
OGL_EXPORT void glEnable(GLenum cap) { ORY_DL(glEnable, cap); ORY_PROLOGUE(); if (!ffp_enable(cap, true)) es.glEnable(cap); }
/* jar: GL11C.glDisable(I)V */
OGL_EXPORT void glDisable(GLenum cap) { ORY_DL(glDisable, cap); ORY_PROLOGUE(); if (!ffp_enable(cap, false)) es.glDisable(cap); }
/* jar: GL11C.glIsEnabled(I)Z */
OGL_EXPORT GLboolean glIsEnabled(GLenum cap) {
    ORY_PROLOGUE();
    int r = ffp_is_enabled(cap);
    return r >= 0 ? (GLboolean)(r ? GL_TRUE : GL_FALSE) : es.glIsEnabled(cap);
}

// ------------------------------------------------------------------ alpha / shade / logic / polygon / point / line
/* jar: GL11.glAlphaFunc(IF)V */
OGL_EXPORT void glAlphaFunc(GLenum func, GLfloat ref) { ORY_DL(glAlphaFunc, func, ref);
    ORY_PROLOGUE();
    if (func < GL_NEVER || func > GL_ALWAYS) { set_error(GL_INVALID_ENUM); return; }
    ref = ref < 0 ? 0 : ref > 1 ? 1 : ref;
    if (func != g.f.alpha_func) { g.f.alpha_func = func; key_dirty(); }
    if (ref != g.f.alpha_ref) { g.f.alpha_ref = ref; g.f.v_alpha = next_ver(); }
}
/* jar: GL11.glShadeModel(I)V */
OGL_EXPORT void glShadeModel(GLenum mode) { ORY_DL(glShadeModel, mode);
    ORY_PROLOGUE();
    if (mode != GL_FLAT && mode != GL_SMOOTH) { set_error(GL_INVALID_ENUM); return; }
    if (mode != g.f.shade) { g.f.shade = mode; key_dirty(); }
}
/* jar: GL11C.glLogicOp(I)V */
OGL_EXPORT void glLogicOp(GLenum opcode) { ORY_DL(glLogicOp, opcode); ORY_PROLOGUE(); g.f.logic_mode = opcode; }
/* jar: GL11C.glPolygonMode(II)V */
OGL_EXPORT void glPolygonMode(GLenum face, GLenum mode) { ORY_DL(glPolygonMode, face, mode);
    ORY_PROLOGUE();
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) g.f.poly_mode[0] = mode;
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) g.f.poly_mode[1] = mode;
    if ((g.escaps & ORY_ESCAP_POLYGON_MODE) && es.glPolygonModeNV) es.glPolygonModeNV(face, mode);
}
/* jar: GL11C.glPointSize(F)V */
OGL_EXPORT void glPointSize(GLfloat size) { ORY_DL(glPointSize, size);
    ORY_PROLOGUE();
    if (size <= 0) { set_error(GL_INVALID_VALUE); return; }
    if (size != g.f.point_size) { g.f.point_size = size; g.f.v_point = next_ver(); }
}
/* jar: GL11C.glLineWidth(F)V */
OGL_EXPORT void glLineWidth(GLfloat width) { ORY_DL(glLineWidth, width); ORY_PROLOGUE(); g.f.line_width = width; es.glLineWidth(width); }

// ------------------------------------------------------------------ clip planes
/* jar: GL11.nglClipPlane(IJ)V */
OGL_EXPORT void glClipPlane(GLenum plane, const GLdouble *eq) { ORY_DL(glClipPlane, plane, eq);
    ORY_PROLOGUE();
    if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + MAX_CLIP_PLANES) { set_error(GL_INVALID_ENUM); return; }
    GLfloat p[4] = {(GLfloat)eq[0], (GLfloat)eq[1], (GLfloat)eq[2], (GLfloat)eq[3]}, o[4];
    plane_to_eye(o, p, g.m.mv[g.m.mv_top].m);
    g.f.clip[plane - GL_CLIP_PLANE0] = v4(o); g.f.v_clip = next_ver();
}
/* jar: GL11.nglGetClipPlane(IJ)V */
OGL_EXPORT void glGetClipPlane(GLenum plane, GLdouble *eq) {
    ORY_PROLOGUE();
    if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + MAX_CLIP_PLANES) { set_error(GL_INVALID_ENUM); return; }
    const Vec4 &c = g.f.clip[plane - GL_CLIP_PLANE0];
    eq[0] = c.x; eq[1] = c.y; eq[2] = c.z; eq[3] = c.w;
}

// ------------------------------------------------------------------ lighting
static void light_set(GLenum light, GLenum pname, const GLfloat *p) {
    if (light < GL_LIGHT0 || light >= GL_LIGHT0 + MAX_LIGHTS) { set_error(GL_INVALID_ENUM); return; }
    Ffp &f = g.f; Light &l = f.l[light - GL_LIGHT0];
    const GLfloat *mv = g.m.mv[g.m.mv_top].m;
    switch (pname) {
    case GL_AMBIENT: l.amb = v4(p); break;
    case GL_DIFFUSE: l.dif = v4(p); break;
    case GL_SPECULAR: { bool was = nonzero3(l.spe); l.spe = v4(p); if (was != nonzero3(l.spe)) key_dirty(); break; }
    case GL_POSITION: {
        GLfloat o[4]; mat_xform4(o, mv, p);
        if ((o[3] == 0.0f) != (l.pos.w == 0.0f)) key_dirty();
        l.pos = v4(o); break;
    }
    case GL_SPOT_DIRECTION: {
        GLfloat x = p[0], y = p[1], z = p[2];
        l.dir = {mv[0] * x + mv[4] * y + mv[8] * z, mv[1] * x + mv[5] * y + mv[9] * z, mv[2] * x + mv[6] * y + mv[10] * z, 0};
        break;
    }
    case GL_SPOT_EXPONENT: l.spot_exp = p[0]; break;
    case GL_SPOT_CUTOFF: if ((p[0] == 180.0f) != (l.spot_cut == 180.0f)) key_dirty(); l.spot_cut = p[0]; break;
    case GL_CONSTANT_ATTENUATION: l.k0 = p[0]; break;
    case GL_LINEAR_ATTENUATION: l.k1 = p[0]; break;
    case GL_QUADRATIC_ATTENUATION: l.k2 = p[0]; break;
    default: set_error(GL_INVALID_ENUM); return;
    }
    f.v_light = next_ver();
}
static int light_get(GLenum light, GLenum pname, GLfloat *p) {
    if (light < GL_LIGHT0 || light >= GL_LIGHT0 + MAX_LIGHTS) { set_error(GL_INVALID_ENUM); return 0; }
    const Light &l = g.f.l[light - GL_LIGHT0];
    const Vec4 *v = nullptr;
    switch (pname) {
    case GL_AMBIENT: v = &l.amb; break;
    case GL_DIFFUSE: v = &l.dif; break;
    case GL_SPECULAR: v = &l.spe; break;
    case GL_POSITION: v = &l.pos; break;
    case GL_SPOT_DIRECTION: p[0] = l.dir.x; p[1] = l.dir.y; p[2] = l.dir.z; return 3;
    case GL_SPOT_EXPONENT: p[0] = l.spot_exp; return 1;
    case GL_SPOT_CUTOFF: p[0] = l.spot_cut; return 1;
    case GL_CONSTANT_ATTENUATION: p[0] = l.k0; return 1;
    case GL_LINEAR_ATTENUATION: p[0] = l.k1; return 1;
    case GL_QUADRATIC_ATTENUATION: p[0] = l.k2; return 1;
    default: set_error(GL_INVALID_ENUM); return 0;
    }
    p[0] = v->x; p[1] = v->y; p[2] = v->z; p[3] = v->w; return 4;
}
/* jar: GL11.glLightf(IIF)V */
OGL_EXPORT void glLightf(GLenum light, GLenum pname, GLfloat param) { ORY_DL(glLightf, light, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {param, 0, 0, 0}; light_set(light, pname, p); }
/* jar: GL11.glLighti(III)V */
OGL_EXPORT void glLighti(GLenum light, GLenum pname, GLint param) { ORY_DL(glLighti, light, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)param, 0, 0, 0}; light_set(light, pname, p); }
/* jar: GL11.nglLightfv(IIJ)V */
OGL_EXPORT void glLightfv(GLenum light, GLenum pname, const GLfloat *params) { ORY_DL(glLightfv, light, pname, params);
    ORY_PROLOGUE();
    GLfloat p[4] = {0, 0, 0, 0};
    int n = (pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR || pname == GL_POSITION) ? 4 : pname == GL_SPOT_DIRECTION ? 3 : 1;
    for (int i = 0; i < n; ++i) p[i] = params[i];
    light_set(light, pname, p);
}
/* jar: GL11.nglLightiv(IIJ)V */
OGL_EXPORT void glLightiv(GLenum light, GLenum pname, const GLint *params) { ORY_DL(glLightiv, light, pname, params);
    ORY_PROLOGUE();
    GLfloat p[4] = {0, 0, 0, 0};
    bool color = pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR;
    int n = (color || pname == GL_POSITION) ? 4 : pname == GL_SPOT_DIRECTION ? 3 : 1;
    for (int i = 0; i < n; ++i) p[i] = color ? snorm_i(params[i]) : (GLfloat)params[i];
    light_set(light, pname, p);
}
/* jar: GL11.nglGetLightfv(IIJ)V */
OGL_EXPORT void glGetLightfv(GLenum light, GLenum pname, GLfloat *params) { ORY_PROLOGUE(); light_get(light, pname, params); }
/* jar: GL11.nglGetLightiv(IIJ)V */
OGL_EXPORT void glGetLightiv(GLenum light, GLenum pname, GLint *params) {
    ORY_PROLOGUE(); GLfloat p[4]; int n = light_get(light, pname, p);
    for (int i = 0; i < n; ++i) params[i] = (GLint)p[i];
}
static void lightmodel_set(GLenum pname, const GLfloat *p) {
    Ffp &f = g.f;
    switch (pname) {
    case GL_LIGHT_MODEL_AMBIENT: f.model_amb = v4(p); f.v_mat = next_ver(); break;
    case GL_LIGHT_MODEL_LOCAL_VIEWER: { bool b = p[0] != 0; if (b != f.local_viewer) { f.local_viewer = b; key_dirty(); } break; }
    case GL_LIGHT_MODEL_TWO_SIDE: { bool b = p[0] != 0; if (b != f.two_side) { f.two_side = b; key_dirty(); } break; }
    case GL_LIGHT_MODEL_COLOR_CONTROL: { bool b = (GLenum)p[0] == GL_SEPARATE_SPECULAR_COLOR; if (b != f.sep_spec) { f.sep_spec = b; key_dirty(); } break; }
    default: set_error(GL_INVALID_ENUM);
    }
}
/* jar: GL11.glLightModelf(IF)V */
OGL_EXPORT void glLightModelf(GLenum pname, GLfloat param) { ORY_DL(glLightModelf, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {param, 0, 0, 0}; lightmodel_set(pname, p); }
/* jar: GL11.glLightModeli(II)V */
OGL_EXPORT void glLightModeli(GLenum pname, GLint param) { ORY_DL(glLightModeli, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)param, 0, 0, 0}; lightmodel_set(pname, p); }
/* jar: GL11.nglLightModelfv(IJ)V */
OGL_EXPORT void glLightModelfv(GLenum pname, const GLfloat *params) { ORY_DL(glLightModelfv, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {params[0], 0, 0, 0};
    if (pname == GL_LIGHT_MODEL_AMBIENT) { p[1] = params[1]; p[2] = params[2]; p[3] = params[3]; }
    lightmodel_set(pname, p);
}
/* jar: GL11.nglLightModeliv(IJ)V */
OGL_EXPORT void glLightModeliv(GLenum pname, const GLint *params) { ORY_DL(glLightModeliv, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)params[0], 0, 0, 0};
    if (pname == GL_LIGHT_MODEL_AMBIENT) for (int i = 0; i < 4; ++i) p[i] = snorm_i(params[i]);
    lightmodel_set(pname, p);
}

// ------------------------------------------------------------------ material
static void material_set(GLenum face, GLenum pname, const GLfloat *p) {
    Ffp &f = g.f;
    int s0 = face == GL_BACK ? 1 : 0, s1 = face == GL_FRONT ? 0 : 1;
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) { set_error(GL_INVALID_ENUM); return; }
    for (int s = s0; s <= s1; ++s) {
        Material &m = f.mat[s];
        switch (pname) {
        case GL_AMBIENT: m.amb = v4(p); break;
        case GL_DIFFUSE: m.dif = v4(p); break;
        case GL_SPECULAR: { bool was = nonzero3(m.spe); m.spe = v4(p); if (was != nonzero3(m.spe)) key_dirty(); break; }
        case GL_EMISSION: m.emi = v4(p); break;
        case GL_SHININESS: m.shin = p[0]; break;
        case GL_AMBIENT_AND_DIFFUSE: m.amb = m.dif = v4(p); break;
        case GL_COLOR_INDEXES: break;
        default: set_error(GL_INVALID_ENUM); return;
        }
    }
    f.v_mat = next_ver();
}
static int material_get(GLenum face, GLenum pname, GLfloat *p) {
    const Material &m = g.f.mat[face == GL_BACK ? 1 : 0];
    const Vec4 *v = nullptr;
    switch (pname) {
    case GL_AMBIENT: v = &m.amb; break;
    case GL_DIFFUSE: v = &m.dif; break;
    case GL_SPECULAR: v = &m.spe; break;
    case GL_EMISSION: v = &m.emi; break;
    case GL_SHININESS: p[0] = m.shin; return 1;
    case GL_COLOR_INDEXES: p[0] = 0; p[1] = 1; p[2] = 1; return 3;
    default: set_error(GL_INVALID_ENUM); return 0;
    }
    p[0] = v->x; p[1] = v->y; p[2] = v->z; p[3] = v->w; return 4;
}
/* jar: GL11.glMaterialf(IIF)V */
OGL_EXPORT void glMaterialf(GLenum face, GLenum pname, GLfloat param) { ORY_DL(glMaterialf, face, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {param, 0, 0, 0}; material_set(face, pname, p); }
/* jar: GL11.glMateriali(III)V */
OGL_EXPORT void glMateriali(GLenum face, GLenum pname, GLint param) { ORY_DL(glMateriali, face, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)param, 0, 0, 0}; material_set(face, pname, p); }
/* jar: GL11.nglMaterialfv(IIJ)V */
OGL_EXPORT void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) { ORY_DL(glMaterialfv, face, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {params[0], 0, 0, 0};
    if (pname != GL_SHININESS) { p[1] = params[1]; p[2] = params[2]; if (pname != GL_COLOR_INDEXES) p[3] = params[3]; }
    material_set(face, pname, p);
}
/* jar: GL11.nglMaterialiv(IIJ)V */
OGL_EXPORT void glMaterialiv(GLenum face, GLenum pname, const GLint *params) { ORY_DL(glMaterialiv, face, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)params[0], 0, 0, 0};
    if (pname != GL_SHININESS && pname != GL_COLOR_INDEXES) for (int i = 0; i < 4; ++i) p[i] = snorm_i(params[i]);
    material_set(face, pname, p);
}
/* jar: GL11.nglGetMaterialfv(IIJ)V */
OGL_EXPORT void glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params) { ORY_PROLOGUE(); material_get(face, pname, params); }
/* jar: GL11.nglGetMaterialiv(IIJ)V */
OGL_EXPORT void glGetMaterialiv(GLenum face, GLenum pname, GLint *params) {
    ORY_PROLOGUE(); GLfloat p[4]; int n = material_get(face, pname, p);
    for (int i = 0; i < n; ++i) params[i] = (GLint)p[i];
}
/* jar: GL11.glColorMaterial(II)V */
OGL_EXPORT void glColorMaterial(GLenum face, GLenum mode) { ORY_DL(glColorMaterial, face, mode);
    ORY_PROLOGUE();
    if (face != g.f.cm_face || mode != g.f.cm_mode) { g.f.cm_face = face; g.f.cm_mode = mode; key_dirty(); }
}

// ------------------------------------------------------------------ fog
static void fog_set(GLenum pname, const GLfloat *p) {
    Ffp &f = g.f;
    switch (pname) {
    case GL_FOG_MODE: { GLenum m = (GLenum)p[0]; if (m != f.fog_mode) { f.fog_mode = m; key_dirty(); } return; }
    case GL_FOG_COORD_SRC: { GLenum m = (GLenum)p[0]; if (m != f.fog_src) { f.fog_src = m; key_dirty(); } return; }
    case GL_FOG_DISTANCE_MODE_NV: { GLenum m = (GLenum)p[0]; if (m != f.fog_dist) { f.fog_dist = m; key_dirty(); } return; }
    case GL_FOG_DENSITY: f.fog_density = p[0]; break;
    case GL_FOG_START: f.fog_start = p[0]; break;
    case GL_FOG_END: f.fog_end = p[0]; break;
    case GL_FOG_INDEX: f.fog_index = p[0]; return;
    case GL_FOG_COLOR: f.fog_color = v4(p); break;
    default: set_error(GL_INVALID_ENUM); return;
    }
    f.v_fog = next_ver();
}
/* jar: GL11.glFogf(IF)V */
OGL_EXPORT void glFogf(GLenum pname, GLfloat param) { ORY_DL(glFogf, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {param, 0, 0, 0}; fog_set(pname, p); }
/* jar: GL11.glFogi(II)V */
OGL_EXPORT void glFogi(GLenum pname, GLint param) { ORY_DL(glFogi, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)param, 0, 0, 0}; fog_set(pname, p); }
/* jar: GL11.nglFogfv(IJ)V */
OGL_EXPORT void glFogfv(GLenum pname, const GLfloat *params) { ORY_DL(glFogfv, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {params[0], 0, 0, 0};
    if (pname == GL_FOG_COLOR) { p[1] = params[1]; p[2] = params[2]; p[3] = params[3]; }
    fog_set(pname, p);
}
/* jar: GL11.nglFogiv(IJ)V */
OGL_EXPORT void glFogiv(GLenum pname, const GLint *params) { ORY_DL(glFogiv, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)params[0], 0, 0, 0};
    if (pname == GL_FOG_COLOR) for (int i = 0; i < 4; ++i) p[i] = snorm_i(params[i]);
    fog_set(pname, p);
}

// ------------------------------------------------------------------ texture environment
static void texenv_set(GLenum target, GLenum pname, const GLfloat *p, bool vec) {
    TexUnit *tu = active_tu();
    if (!tu) return;
    TexEnv &e = tu->env;
    if (target == GL_TEXTURE_FILTER_CONTROL) {
        if (pname == GL_TEXTURE_LOD_BIAS) e.lod_bias = p[0]; else set_error(GL_INVALID_ENUM);
        return;
    }
    if (target == GL_POINT_SPRITE) return;             // GL_COORD_REPLACE: point sprites use gl_PointCoord
    if (target != GL_TEXTURE_ENV) { set_error(GL_INVALID_ENUM); return; }
    GLenum v = (GLenum)p[0];
    switch (pname) {
    case GL_TEXTURE_ENV_MODE: if (v != e.mode) { e.mode = v; key_dirty(); } return;
    case GL_TEXTURE_ENV_COLOR: if (!vec) { set_error(GL_INVALID_ENUM); return; } e.color = v4(p); g.f.v_env = next_ver(); return;
    case GL_COMBINE_RGB: e.comb_rgb = v; break;
    case GL_COMBINE_ALPHA: e.comb_a = v; break;
    case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB: e.src_rgb[pname - GL_SRC0_RGB] = v; break;
    case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA: e.src_a[pname - GL_SRC0_ALPHA] = v; break;
    case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB: e.op_rgb[pname - GL_OPERAND0_RGB] = v; break;
    case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA: e.op_a[pname - GL_OPERAND0_ALPHA] = v; break;
    case GL_RGB_SCALE: e.scale_rgb = p[0]; break;
    case GL_ALPHA_SCALE: e.scale_a = p[0]; break;
    default: set_error(GL_INVALID_ENUM); return;
    }
    key_dirty();
}
static int texenv_get(GLenum target, GLenum pname, GLfloat *p) {
    TexUnit *tu = active_tu();
    if (!tu) return 0;
    const TexEnv &e = tu->env;
    if (target == GL_TEXTURE_FILTER_CONTROL && pname == GL_TEXTURE_LOD_BIAS) { p[0] = e.lod_bias; return 1; }
    switch (pname) {
    case GL_TEXTURE_ENV_MODE: p[0] = (GLfloat)e.mode; return 1;
    case GL_TEXTURE_ENV_COLOR: p[0] = e.color.x; p[1] = e.color.y; p[2] = e.color.z; p[3] = e.color.w; return 4;
    case GL_COMBINE_RGB: p[0] = (GLfloat)e.comb_rgb; return 1;
    case GL_COMBINE_ALPHA: p[0] = (GLfloat)e.comb_a; return 1;
    case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB: p[0] = (GLfloat)e.src_rgb[pname - GL_SRC0_RGB]; return 1;
    case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA: p[0] = (GLfloat)e.src_a[pname - GL_SRC0_ALPHA]; return 1;
    case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB: p[0] = (GLfloat)e.op_rgb[pname - GL_OPERAND0_RGB]; return 1;
    case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA: p[0] = (GLfloat)e.op_a[pname - GL_OPERAND0_ALPHA]; return 1;
    case GL_RGB_SCALE: p[0] = e.scale_rgb; return 1;
    case GL_ALPHA_SCALE: p[0] = e.scale_a; return 1;
    default: set_error(GL_INVALID_ENUM); return 0;
    }
}
/* jar: GL11.glTexEnvf(IIF)V */
OGL_EXPORT void glTexEnvf(GLenum target, GLenum pname, GLfloat param) { ORY_DL(glTexEnvf, target, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {param, 0, 0, 0}; texenv_set(target, pname, p, false); }
/* jar: GL11.glTexEnvi(III)V */
OGL_EXPORT void glTexEnvi(GLenum target, GLenum pname, GLint param) { ORY_DL(glTexEnvi, target, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)param, 0, 0, 0}; texenv_set(target, pname, p, false); }
/* jar: GL11.nglTexEnvfv(IIJ)V */
OGL_EXPORT void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) { ORY_DL(glTexEnvfv, target, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {params[0], 0, 0, 0};
    if (pname == GL_TEXTURE_ENV_COLOR) { p[1] = params[1]; p[2] = params[2]; p[3] = params[3]; }
    texenv_set(target, pname, p, true);
}
/* jar: GL11.nglTexEnviv(IIJ)V */
OGL_EXPORT void glTexEnviv(GLenum target, GLenum pname, const GLint *params) { ORY_DL(glTexEnviv, target, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)params[0], 0, 0, 0};
    if (pname == GL_TEXTURE_ENV_COLOR) for (int i = 0; i < 4; ++i) p[i] = snorm_i(params[i]);
    texenv_set(target, pname, p, true);
}
/* jar: GL11.nglGetTexEnvfv(IIJ)V */
OGL_EXPORT void glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params) { ORY_PROLOGUE(); texenv_get(target, pname, params); }
/* jar: GL11.nglGetTexEnviv(IIJ)V */
OGL_EXPORT void glGetTexEnviv(GLenum target, GLenum pname, GLint *params) {
    ORY_PROLOGUE(); GLfloat p[4]; int n = texenv_get(target, pname, p);
    for (int i = 0; i < n; ++i) params[i] = (GLint)p[i];
}

// ------------------------------------------------------------------ texture coordinate generation
static void texgen_set(GLenum coord, GLenum pname, const GLfloat *p) {
    TexUnit *tu = active_tu();
    if (!tu) return;
    if (coord < GL_S || coord > GL_Q) { set_error(GL_INVALID_ENUM); return; }
    int c = coord - GL_S;
    switch (pname) {
    case GL_TEXTURE_GEN_MODE: { GLenum m = (GLenum)p[0]; if (m != tu->gen.mode[c]) { tu->gen.mode[c] = m; key_dirty(); } return; }
    case GL_OBJECT_PLANE: tu->gen.obj[c] = v4(p); break;
    case GL_EYE_PLANE: { GLfloat o[4]; plane_to_eye(o, p, g.m.mv[g.m.mv_top].m); tu->gen.eye[c] = v4(o); break; }
    default: set_error(GL_INVALID_ENUM); return;
    }
    g.f.v_gen = next_ver();
}
static int texgen_get(GLenum coord, GLenum pname, GLfloat *p) {
    TexUnit *tu = active_tu();
    if (!tu || coord < GL_S || coord > GL_Q) { set_error(GL_INVALID_ENUM); return 0; }
    int c = coord - GL_S; const Vec4 *v;
    switch (pname) {
    case GL_TEXTURE_GEN_MODE: p[0] = (GLfloat)tu->gen.mode[c]; return 1;
    case GL_OBJECT_PLANE: v = &tu->gen.obj[c]; break;
    case GL_EYE_PLANE: v = &tu->gen.eye[c]; break;
    default: set_error(GL_INVALID_ENUM); return 0;
    }
    p[0] = v->x; p[1] = v->y; p[2] = v->z; p[3] = v->w; return 4;
}
/* jar: GL11.glTexGeni(III)V */
OGL_EXPORT void glTexGeni(GLenum coord, GLenum pname, GLint param) { ORY_DL(glTexGeni, coord, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)param, 0, 0, 0}; texgen_set(coord, pname, p); }
/* jar: GL11.glTexGenf(IIF)V */
OGL_EXPORT void glTexGenf(GLenum coord, GLenum pname, GLfloat param) { ORY_DL(glTexGenf, coord, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {param, 0, 0, 0}; texgen_set(coord, pname, p); }
/* jar: GL11.glTexGend(IID)V */
OGL_EXPORT void glTexGend(GLenum coord, GLenum pname, GLdouble param) { ORY_DL(glTexGend, coord, pname, param); ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)param, 0, 0, 0}; texgen_set(coord, pname, p); }
/* jar: GL11.nglTexGenfv(IIJ)V */
OGL_EXPORT void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params) { ORY_DL(glTexGenfv, coord, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {params[0], 0, 0, 0};
    if (pname != GL_TEXTURE_GEN_MODE) { p[1] = params[1]; p[2] = params[2]; p[3] = params[3]; }
    texgen_set(coord, pname, p);
}
/* jar: GL11.nglTexGeniv(IIJ)V */
OGL_EXPORT void glTexGeniv(GLenum coord, GLenum pname, const GLint *params) { ORY_DL(glTexGeniv, coord, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)params[0], 0, 0, 0};
    if (pname != GL_TEXTURE_GEN_MODE) for (int i = 1; i < 4; ++i) p[i] = (GLfloat)params[i];
    texgen_set(coord, pname, p);
}
/* jar: GL11.nglTexGendv(IIJ)V */
OGL_EXPORT void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params) { ORY_DL(glTexGendv, coord, pname, params);
    ORY_PROLOGUE(); GLfloat p[4] = {(GLfloat)params[0], 0, 0, 0};
    if (pname != GL_TEXTURE_GEN_MODE) for (int i = 1; i < 4; ++i) p[i] = (GLfloat)params[i];
    texgen_set(coord, pname, p);
}
/* jar: GL11.nglGetTexGenfv(IIJ)V */
OGL_EXPORT void glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params) { ORY_PROLOGUE(); texgen_get(coord, pname, params); }
/* jar: GL11.nglGetTexGeniv(IIJ)V */
OGL_EXPORT void glGetTexGeniv(GLenum coord, GLenum pname, GLint *params) {
    ORY_PROLOGUE(); GLfloat p[4]; int n = texgen_get(coord, pname, p); for (int i = 0; i < n; ++i) params[i] = (GLint)p[i];
}
/* jar: GL11.nglGetTexGendv(IIJ)V */
OGL_EXPORT void glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params) {
    ORY_PROLOGUE(); GLfloat p[4]; int n = texgen_get(coord, pname, p); for (int i = 0; i < n; ++i) params[i] = p[i];
}

// ------------------------------------------------------------------ texture units
/* jar: GL13C.glActiveTexture(I)V */
OGL_EXPORT void glActiveTexture(GLenum texture) { ORY_DL(glActiveTexture, texture);
    ORY_PROLOGUE();
    GLuint u = texture - GL_TEXTURE0;
    g.f.active = u;
    es.glActiveTexture(texture);
    if (g.m.mode == GL_TEXTURE) mat_reselect();
}
/* jar: GL13.glClientActiveTexture(I)V */
OGL_EXPORT void glClientActiveTexture(GLenum texture) {
    GLuint u = texture - GL_TEXTURE0;
    if (u >= (GLuint)MAX_TEX_UNITS) { set_error(GL_INVALID_ENUM); return; }
    g.f.client_active = u;
}
