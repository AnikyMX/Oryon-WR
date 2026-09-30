// Oryon -- core queries: error model, strings, versions, typed glGet* virtualisation.
#include "oryon.hpp"

namespace ory {
namespace {
// Virtual state query. Returns component count (0 = not virtual -> forward to ES).
// Values are produced as doubles and converted per API flavour by the caller.
int vquery(GLenum pname, double *v) {
    const Matrices &M = g.m; const Ffp &f = g.f;
    auto mat = [&](const GLfloat *m, bool tr) { for (int i = 0; i < 16; ++i) v[i] = tr ? m[(i % 4) * 4 + i / 4] : m[i]; return 16; };
    auto vec = [&](const Vec4 &a, int n) { v[0] = a.x; v[1] = a.y; v[2] = a.z; v[3] = a.w; return n; };
    GLuint au = f.active < (GLuint)MAX_TEX_UNITS ? f.active : 0;
    switch (pname) {
    case GL_MAJOR_VERSION: v[0] = REPORT_MAJOR; return 1;
    case GL_MINOR_VERSION: v[0] = REPORT_MINOR; return 1;
    case GL_NUM_EXTENSIONS: v[0] = g.ext_count; return 1;
    case GL_CONTEXT_FLAGS: v[0] = 0; return 1;
    case GL_CONTEXT_PROFILE_MASK: v[0] = GL_CONTEXT_COMPATIBILITY_PROFILE_BIT; return 1;
    // matrices
    case GL_MODELVIEW_MATRIX: return mat(M.mv[M.mv_top].m, false);
    case GL_PROJECTION_MATRIX: return mat(M.p[M.p_top].m, false);
    case GL_TEXTURE_MATRIX: return mat(M.t[au][M.t_top[au]].m, false);
    case GL_TRANSPOSE_MODELVIEW_MATRIX: return mat(M.mv[M.mv_top].m, true);
    case GL_TRANSPOSE_PROJECTION_MATRIX: return mat(M.p[M.p_top].m, true);
    case GL_TRANSPOSE_TEXTURE_MATRIX: return mat(M.t[au][M.t_top[au]].m, true);
    case GL_MODELVIEW_STACK_DEPTH: v[0] = M.mv_top + 1; return 1;
    case GL_PROJECTION_STACK_DEPTH: v[0] = M.p_top + 1; return 1;
    case GL_TEXTURE_STACK_DEPTH: v[0] = M.t_top[au] + 1; return 1;
    case GL_MAX_MODELVIEW_STACK_DEPTH: v[0] = MV_STACK; return 1;
    case GL_MAX_PROJECTION_STACK_DEPTH: v[0] = P_STACK; return 1;
    case GL_MAX_TEXTURE_STACK_DEPTH: v[0] = T_STACK; return 1;
    case GL_MATRIX_MODE: v[0] = M.mode; return 1;
    // limits
    case GL_MAX_TEXTURE_UNITS: case GL_MAX_TEXTURE_COORDS: v[0] = MAX_TEX_UNITS; return 1;
    case GL_MAX_LIGHTS: v[0] = MAX_LIGHTS; return 1;
    case GL_MAX_CLIP_PLANES: v[0] = MAX_CLIP_PLANES; return 1;
    case GL_MAX_ATTRIB_STACK_DEPTH: case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH: v[0] = ATTRIB_STACK; return 1;
    case GL_ATTRIB_STACK_DEPTH: v[0] = attrib_depth(); return 1;
    case GL_CLIENT_ATTRIB_STACK_DEPTH: v[0] = 0; return 1;
    case GL_MAX_LIST_NESTING: v[0] = 64; return 1;
    case GL_MAX_EVAL_ORDER: v[0] = 8; return 1;
    case GL_MAX_PIXEL_MAP_TABLE: v[0] = 256; return 1;
    case GL_MAX_NAME_STACK_DEPTH: v[0] = 64; return 1;
    // current values
    case GL_CURRENT_COLOR: return vec(g.cur.color, 4);
    case GL_CURRENT_SECONDARY_COLOR: return vec(g.cur.color2, 4);
    case GL_CURRENT_NORMAL: return vec(g.cur.normal, 3);
    case GL_CURRENT_TEXTURE_COORDS: return vec(g.cur.tex[f.client_active], 4);
    case GL_CURRENT_FOG_COORD: v[0] = g.cur.fog; return 1;
    case GL_CURRENT_RASTER_POSITION: return vec(g.cur.raster, 4);
    case GL_CURRENT_RASTER_POSITION_VALID: v[0] = g.cur.raster_valid; return 1;
    case GL_CURRENT_RASTER_COLOR: return vec(g.cur.color, 4);
    case GL_CURRENT_RASTER_TEXTURE_COORDS: return vec(g.cur.tex[0], 4);
    case GL_CURRENT_RASTER_DISTANCE: v[0] = 0; return 1;
    // fixed-function state
    case GL_ALPHA_TEST_FUNC: v[0] = f.alpha_func; return 1;
    case GL_ALPHA_TEST_REF: v[0] = f.alpha_ref; return 1;
    case GL_SHADE_MODEL: v[0] = f.shade; return 1;
    case GL_FOG_MODE: v[0] = f.fog_mode; return 1;
    case GL_FOG_DENSITY: v[0] = f.fog_density; return 1;
    case GL_FOG_START: v[0] = f.fog_start; return 1;
    case GL_FOG_END: v[0] = f.fog_end; return 1;
    case GL_FOG_INDEX: v[0] = f.fog_index; return 1;
    case GL_FOG_COLOR: return vec(f.fog_color, 4);
    case GL_FOG_COORD_SRC: v[0] = f.fog_src; return 1;
    case GL_FOG_DISTANCE_MODE_NV: v[0] = f.fog_dist; return 1;
    case GL_LIGHT_MODEL_AMBIENT: return vec(f.model_amb, 4);
    case GL_LIGHT_MODEL_LOCAL_VIEWER: v[0] = f.local_viewer; return 1;
    case GL_LIGHT_MODEL_TWO_SIDE: v[0] = f.two_side; return 1;
    case GL_LIGHT_MODEL_COLOR_CONTROL: v[0] = f.sep_spec ? GL_SEPARATE_SPECULAR_COLOR : GL_SINGLE_COLOR; return 1;
    case GL_COLOR_MATERIAL_FACE: v[0] = f.cm_face; return 1;
    case GL_COLOR_MATERIAL_PARAMETER: v[0] = f.cm_mode; return 1;
    case GL_LOGIC_OP_MODE: v[0] = f.logic_mode; return 1;
    case GL_POINT_SIZE: v[0] = f.point_size; return 1;
    case GL_POLYGON_MODE: v[0] = f.poly_mode[0]; v[1] = f.poly_mode[1]; return 2;
    case GL_CLIENT_ACTIVE_TEXTURE: v[0] = GL_TEXTURE0 + f.client_active; return 1;
    // bindings (application view)
    case GL_ARRAY_BUFFER_BINDING: v[0] = g.b.app_array; return 1;
    case GL_COPY_WRITE_BUFFER_BINDING: v[0] = g.b.app_copy_write; return 1;
    case GL_VERTEX_ARRAY_BINDING: v[0] = g.b.app_vao; return 1;
    case GL_CURRENT_PROGRAM: v[0] = g.b.app_program; return 1;
    case GL_TEXTURE_BINDING_1D: v[0] = f.active < 32 ? g.tex_bind[f.active][0] : 0; return 1;
    // legacy arrays
    case GL_VERTEX_ARRAY_SIZE: v[0] = g.ca[CA_VERTEX].size; return 1;
    case GL_VERTEX_ARRAY_TYPE: v[0] = g.ca[CA_VERTEX].type; return 1;
    case GL_VERTEX_ARRAY_STRIDE: v[0] = g.ca[CA_VERTEX].stride; return 1;
    case GL_VERTEX_ARRAY_BUFFER_BINDING: v[0] = g.ca[CA_VERTEX].buf; return 1;
    case GL_NORMAL_ARRAY_TYPE: v[0] = g.ca[CA_NORMAL].type; return 1;
    case GL_NORMAL_ARRAY_STRIDE: v[0] = g.ca[CA_NORMAL].stride; return 1;
    case GL_NORMAL_ARRAY_BUFFER_BINDING: v[0] = g.ca[CA_NORMAL].buf; return 1;
    case GL_COLOR_ARRAY_SIZE: v[0] = g.ca[CA_COLOR].size; return 1;
    case GL_COLOR_ARRAY_TYPE: v[0] = g.ca[CA_COLOR].type; return 1;
    case GL_COLOR_ARRAY_STRIDE: v[0] = g.ca[CA_COLOR].stride; return 1;
    case GL_COLOR_ARRAY_BUFFER_BINDING: v[0] = g.ca[CA_COLOR].buf; return 1;
    case GL_TEXTURE_COORD_ARRAY_SIZE: v[0] = g.ca[CA_TEX0 + f.client_active].size; return 1;
    case GL_TEXTURE_COORD_ARRAY_TYPE: v[0] = g.ca[CA_TEX0 + f.client_active].type; return 1;
    case GL_TEXTURE_COORD_ARRAY_STRIDE: v[0] = g.ca[CA_TEX0 + f.client_active].stride; return 1;
    case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING: v[0] = g.ca[CA_TEX0 + f.client_active].buf; return 1;
    // framebuffer / misc desktop-only
    case GL_DOUBLEBUFFER: case GL_RGBA_MODE: v[0] = 1; return 1;
    case GL_STEREO: case GL_INDEX_MODE: case GL_AUX_BUFFERS: v[0] = 0; return 1;
    case GL_UNPACK_SWAP_BYTES: case GL_UNPACK_LSB_FIRST: case GL_PACK_SWAP_BYTES: case GL_PACK_LSB_FIRST:
    case GL_PACK_IMAGE_HEIGHT: case GL_PACK_SKIP_IMAGES: v[0] = 0; return 1;
    case GL_LIST_BASE: v[0] = g.dl.base; return 1;
    case GL_LIST_INDEX: v[0] = g.dl.index; return 1;
    case GL_LIST_MODE: v[0] = g.dl.mode; return 1;
    case GL_POINT_SIZE_RANGE: case GL_LINE_WIDTH_RANGE: {
        GLfloat r[2] = {1, 1};
        es.glGetFloatv(pname == GL_POINT_SIZE_RANGE ? GL_ALIASED_POINT_SIZE_RANGE : GL_ALIASED_LINE_WIDTH_RANGE, r);
        v[0] = r[0]; v[1] = r[1]; return 2;
    }
    case GL_POINT_SIZE_GRANULARITY: case GL_LINE_WIDTH_GRANULARITY: v[0] = 0.125; return 1;
    case GL_DRAW_BUFFER: { GLint b = GL_BACK; es.glGetIntegerv(GL_DRAW_BUFFER0, &b); v[0] = b; return 1; }
    default: {
        int e = ffp_is_enabled(pname);
        if (e >= 0) { v[0] = e; return 1; }
        return 0;
    }
    }
}
} // namespace
} // namespace ory

using namespace ory;

/* jar: GL11C.glGetError()I */
OGL_EXPORT GLenum glGetError(void) {
    ORY_PROLOGUE();
    GLenum e = g.error;
    if (e) { g.error = 0; return e; }
    return es.glGetError();
}

/* jar: GL11C.nglGetString(I)J */
OGL_EXPORT const GLubyte *glGetString(GLenum name) {
    ORY_PROLOGUE();
    switch (name) {
    case GL_VENDOR: if (g.str_vendor) return g.str_vendor; break;
    case GL_RENDERER: if (g.str_renderer) return g.str_renderer; break;
    case GL_VERSION: if (g.str_version) return g.str_version; break;
    case GL_SHADING_LANGUAGE_VERSION: if (g.str_glsl) return g.str_glsl; break;
    case GL_EXTENSIONS: if (g.str_ext) return g.str_ext; break;
    default: break;
    }
    return es.glGetString(name);
}

/* jar: GL30C.nglGetStringi(II)J */
OGL_EXPORT const GLubyte *glGetStringi(GLenum name, GLuint index) {
    ORY_PROLOGUE();
    if (name == GL_EXTENSIONS) {
        if (index < (GLuint)g.ext_count) return (const GLubyte *)g.ext[index];
        set_error(GL_INVALID_VALUE);
        return nullptr;
    }
    return es.glGetStringi(name, index);
}

/* jar: GL11C.nglGetIntegerv(IJ)V */
OGL_EXPORT void glGetIntegerv(GLenum pname, GLint *data) {
    ORY_PROLOGUE();
    double v[16];
    int n = vquery(pname, v);
    if (!n) { es.glGetIntegerv(pname, data); return; }
    for (int i = 0; i < n; ++i) data[i] = (GLint)v[i];
}

/* jar: GL11C.nglGetFloatv(IJ)V */
OGL_EXPORT void glGetFloatv(GLenum pname, GLfloat *data) {
    ORY_PROLOGUE();
    double v[16];
    int n = vquery(pname, v);
    if (!n) { es.glGetFloatv(pname, data); return; }
    for (int i = 0; i < n; ++i) data[i] = (GLfloat)v[i];
}

/* jar: GL11C.nglGetBooleanv(IJ)V */
OGL_EXPORT void glGetBooleanv(GLenum pname, GLboolean *data) {
    ORY_PROLOGUE();
    double v[16];
    int n = vquery(pname, v);
    if (!n) { es.glGetBooleanv(pname, data); return; }
    for (int i = 0; i < n; ++i) data[i] = v[i] != 0.0 ? GL_TRUE : GL_FALSE;
}

/* jar: GL11C.nglGetDoublev(IJ)V */
OGL_EXPORT void glGetDoublev(GLenum pname, GLdouble *data) {
    ORY_PROLOGUE();
    double v[16];
    int n = vquery(pname, v);
    if (n) { for (int i = 0; i < n; ++i) data[i] = v[i]; return; }
    GLfloat f[16];
    es.glGetFloatv(pname, f);          // ES has no double queries; <=16 components (matrices)
    for (int i = 0; i < 16; ++i) data[i] = f[i];
}

/* jar: GL11C.glClearDepth(D)V */
OGL_EXPORT void glClearDepth(GLdouble depth) { ORY_DL(glClearDepth, depth); ORY_PROLOGUE(); es.glClearDepthf((GLfloat)depth); }

/* jar: GL11C.glDepthRange(DD)V */
OGL_EXPORT void glDepthRange(GLdouble zNear, GLdouble zFar) { ORY_DL(glDepthRange, zNear, zFar); ORY_PROLOGUE(); es.glDepthRangef((GLfloat)zNear, (GLfloat)zFar); }
