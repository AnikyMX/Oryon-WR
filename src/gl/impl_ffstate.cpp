/* Kapabilitas dan parameter fixed-function.

   Semua di berkas ini hanya menyentuh state Oryon; tidak satu pun diteruskan
   apa adanya ke GLES, karena GLES tidak mengenal GL_ALPHA_TEST, GL_LIGHTING,
   GL_FOG, maupun GL_TEXTURE_2D sebagai kapabilitas. Efeknya muncul nanti
   sebagai cabang di shader yang dibangkitkan.

   ORYON_IMPL(glEnable)
   ORYON_IMPL(glDisable)
   ORYON_IMPL(glAlphaFunc)
   ORYON_IMPL(glFogf)
   ORYON_IMPL(glFogfv)
   ORYON_IMPL(glFogi)
   ORYON_IMPL(glColorMaterial)
   ORYON_IMPL(glLightfv)
   ORYON_IMPL(glLightModelfv)
   ORYON_IMPL(glShadeModel)
   ORYON_IMPL(glTexEnvf)
   ORYON_IMPL(glTexEnvfv)
   ORYON_IMPL(glTexEnvi)
   ORYON_IMPL(glTexGenfv)
   ORYON_IMPL(glTexGeni)
   ORYON_IMPL(glPushAttrib)
   ORYON_IMPL(glPopAttrib)
   ORYON_IMPL(glActiveTexture)
   ORYON_IMPL(glActiveTextureARB)
   ORYON_IMPL(glClientActiveTexture)
   ORYON_IMPL(glClientActiveTextureARB)
   ORYON_IMPL(glColor4f)
   ORYON_IMPL(glNormal3f)
*/

#include "oryon/oryon.h"
#include "../driver/driver.h"
#include "../state/ff_state.h"
#include "../state/state.h"
#include "../util/mat4.h"

#include <string.h>

using namespace oryon;

namespace {

ORYON_INLINE FFAttribState &A() { return g_ff.a; }

/* Kapabilitas yang memang dikenal GLES 3.2. Selain daftar ini dan daftar
   fixed-function di bawah, glEnable diabaikan diam-diam - meneruskannya ke
   driver hanya akan memasang GL_INVALID_ENUM yang lalu membingungkan LWJGL. */
bool gles_cap(GLenum c) {
    switch (c) {
    case GL_BLEND: case GL_CULL_FACE: case GL_DEPTH_TEST: case GL_DITHER:
    case GL_POLYGON_OFFSET_FILL: case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE: case GL_SCISSOR_TEST: case GL_STENCIL_TEST:
    case GL_PRIMITIVE_RESTART_FIXED_INDEX: case GL_RASTERIZER_DISCARD:
    case GL_SAMPLE_MASK: case GL_DEBUG_OUTPUT: case GL_DEBUG_OUTPUT_SYNCHRONOUS:
    case GL_TEXTURE_CUBE_MAP_SEAMLESS:
        return true;
    default:
        return false;
    }
}

void set_cap(GLenum cap, bool on) {
    FFAttribState &a = A();
    if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + FF_MAX_LIGHTS) {
        const uint8_t b = (uint8_t) (1u << (cap - GL_LIGHT0));
        if (on) a.light_mask |= b; else a.light_mask = (uint8_t) (a.light_mask & ~b);
        return;
    }
    if (cap >= GL_TEXTURE_GEN_S && cap <= GL_TEXTURE_GEN_Q) {
        const uint8_t b = (uint8_t) (1u << (cap - GL_TEXTURE_GEN_S));
        TexEnv &t = a.tex[a.active_tex];
        if (on) t.gen_mask |= b; else t.gen_mask = (uint8_t) (t.gen_mask & ~b);
        return;
    }
    switch (cap) {
    case GL_ALPHA_TEST:      a.alpha_test = on; return;
    case GL_LIGHTING:        a.lighting = on; return;
    case GL_FOG:             a.fog = on; return;
    case GL_COLOR_MATERIAL:  a.color_material = on; return;
    case GL_NORMALIZE:       a.normalize = on; return;
    case GL_RESCALE_NORMAL:  a.rescale_normal = on; return;
    case GL_TEXTURE_2D:      a.tex[a.active_tex].enabled = on; return;

    /* Tidak ada di GLES dan tidak dipakai 1.12.2 untuk hal yang terlihat. */
    case GL_COLOR_LOGIC_OP:
    case GL_LINE_SMOOTH:
    case GL_POINT_SMOOTH:
    case GL_POLYGON_SMOOTH:
    case GL_TEXTURE_1D:
        return;

    default:
        if (gles_cap(cap)) {
            if (on) gles.glEnable(cap); else gles.glDisable(cap);
        } else {
            ORYON_LOG("gl%s(0x%X) diabaikan: bukan kapabilitas GLES",
                      on ? "Enable" : "Disable", cap);
        }
        return;
    }
}

int coord_index(GLenum c) {
    switch (c) {
    case GL_S: return 0;
    case GL_T: return 1;
    case GL_R: return 2;
    case GL_Q: return 3;
    default:   return -1;
    }
}

}  /* namespace */

ORYON_API void glEnable(GLenum cap)  { if (ensure_init()) set_cap(cap, true); }
ORYON_API void glDisable(GLenum cap) { if (ensure_init()) set_cap(cap, false); }

/* ------------------------------------------------------------ uji alpha --- */

ORYON_API void glAlphaFunc(GLenum func, GLclampf ref) {
    if (func < GL_NEVER || func > GL_ALWAYS) { set_error(GL_INVALID_ENUM); return; }
    A().alpha_func = func;
    A().alpha_ref  = ref < 0.0f ? 0.0f : (ref > 1.0f ? 1.0f : ref);
}

/* ----------------------------------------------------------------- kabut -- */

namespace {
void fog_scalar(GLenum pname, float v) {
    FFAttribState &a = A();
    switch (pname) {
    case GL_FOG_MODE:
        a.fog_mode = (uint8_t) (v == (float) GL_LINEAR ? FOG_LINEAR
                              : v == (float) GL_EXP2   ? FOG_EXP2 : FOG_EXP);
        return;
    case GL_FOG_DENSITY: a.fog_density = v; return;
    case GL_FOG_START:   a.fog_start = v; return;
    case GL_FOG_END:     a.fog_end = v; return;
    case GL_FOG_INDEX:   return;                 /* mode indeks warna: tidak ada */
    default: set_error(GL_INVALID_ENUM); return;
    }
}
}  /* namespace */

ORYON_API void glFogf(GLenum pname, GLfloat param) { fog_scalar(pname, param); }
ORYON_API void glFogi(GLenum pname, GLint param)   { fog_scalar(pname, (float) param); }

ORYON_API void glFogfv(GLenum pname, const GLfloat *params) {
    if (!params) return;
    if (pname == GL_FOG_COLOR) { memcpy(A().fog_color, params, 4 * sizeof(float)); return; }
    fog_scalar(pname, params[0]);
}

/* ------------------------------------------------------------- cahaya ----- */

ORYON_API void glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    if (!params) return;
    const int i = (int) (light - GL_LIGHT0);
    if (i < 0 || i >= FF_MAX_LIGHTS) { set_error(GL_INVALID_ENUM); return; }
    Light &L = A().light[i];
    switch (pname) {
    case GL_AMBIENT:  memcpy(L.ambient, params, 4 * sizeof(float)); return;
    case GL_DIFFUSE:  memcpy(L.diffuse, params, 4 * sizeof(float)); return;
    case GL_SPECULAR: return;   /* Minecraft menyetelnya hitam; shader mengabaikannya */
    case GL_POSITION:
        /* Aturan GL: posisi ditransformasi oleh modelview SAAT ditetapkan. */
        mat4_xform(L.position, g_ff.mv[g_ff.mv_top], params);
        return;
    default:
        ORYON_LOG("glLightfv pname 0x%X diabaikan", pname);
        return;
    }
}

ORYON_API void glLightModelfv(GLenum pname, const GLfloat *params) {
    if (!params) return;
    if (pname == GL_LIGHT_MODEL_AMBIENT) {
        memcpy(A().lm_ambient, params, 4 * sizeof(float));
        return;
    }
    ORYON_LOG("glLightModelfv pname 0x%X diabaikan", pname);
}

ORYON_API void glColorMaterial(GLenum face, GLenum mode) {
    A().color_material_mode = mode;
}

ORYON_API void glShadeModel(GLenum mode) {
    if (mode != GL_FLAT && mode != GL_SMOOTH) { set_error(GL_INVALID_ENUM); return; }
    A().flat_shade = (mode == GL_FLAT);
}

/* ------------------------------------------------------- lingkungan tekstur */

namespace {

/* Meringkas state combiner menjadi satu mode efektif.

   Minecraft 1.12.2 memang menyentuh GL_COMBINE - tapi hanya untuk menuliskan
   NILAI BAWAAN GL (COMBINE_RGB=MODULATE, SRC0=TEXTURE, SRC1=PREVIOUS,
   OPERAND0/1=SRC_COLOR). Konfigurasi itu setara persis dengan GL_MODULATE,
   jadi diringkas ke sana alih-alih membangkitkan cabang shader tersendiri.
   Konfigurasi lain dicatat di build debug, bukan ditebak diam-diam. */
uint8_t effective_env(const TexEnv &t) {
    if (t.raw_mode == GL_REPLACE) return ENV_REPLACE;
    if (t.raw_mode != GL_COMBINE) return ENV_MODULATE;

    const bool src_tex = (t.src0_rgb == GL_TEXTURE && t.op0_rgb == GL_SRC_COLOR);
    if (t.combine_rgb == GL_REPLACE && src_tex) return ENV_REPLACE;
    if (t.combine_rgb == GL_MODULATE && src_tex &&
        t.src1_rgb == GL_PREVIOUS && t.op1_rgb == GL_SRC_COLOR)
        return ENV_MODULATE;

    /* Konfigurasi di luar dua pola di atas - misalnya GL_INTERPOLATE dengan
       GL_PRIMARY_COLOR dan GL_CONSTANT - tidak dibangkitkan sebagai cabang
       shader tersendiri. 1.12.2 hanya memakainya untuk menuliskan nilai bawaan,
       jadi GL_MODULATE adalah hasil yang sama; kalau ternyata bukan, baris log
       inilah yang memberitahu. */
    ORYON_LOG("combiner texenv tidak dikenal (rgb=0x%X src0=0x%X src1=0x%X), "
              "memakai GL_MODULATE", t.combine_rgb, t.src0_rgb, t.src1_rgb);
    return ENV_MODULATE;
}

void texenv_scalar(GLenum target, GLenum pname, float v) {
    FFAttribState &a = A();
    if (target == GL_TEXTURE_FILTER_CONTROL) {
        if (pname == GL_TEXTURE_LOD_BIAS) a.tex[a.active_tex].lod_bias = v;
        return;
    }
    if (target != GL_TEXTURE_ENV) { set_error(GL_INVALID_ENUM); return; }

    TexEnv &t = a.tex[a.active_tex];
    const GLenum m = (GLenum) v;
    switch (pname) {
    case GL_TEXTURE_ENV_MODE: t.raw_mode    = m; break;
    case GL_COMBINE_RGB:      t.combine_rgb = m; break;
    case GL_SOURCE0_RGB:      t.src0_rgb    = m; break;
    case GL_SOURCE1_RGB:      t.src1_rgb    = m; break;
    case GL_OPERAND0_RGB:     t.op0_rgb     = m; break;
    case GL_OPERAND1_RGB:     t.op1_rgb     = m; break;
    /* Jalur alpha, skala, dan sumber ke-3 hanya pernah disetel ke nilai bawaan
       oleh 1.12.2; diterima lalu diabaikan. */
    case GL_COMBINE_ALPHA: case GL_SOURCE0_ALPHA: case GL_SOURCE1_ALPHA:
    case GL_SOURCE2_ALPHA: case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA:
    case GL_OPERAND2_ALPHA: case GL_SOURCE2_RGB: case GL_OPERAND2_RGB:
    case GL_RGB_SCALE: case GL_ALPHA_SCALE: case GL_TEXTURE_ENV_COLOR:
        return;
    default:
        return;
    }
    t.env_mode = effective_env(t);
}

}  /* namespace */

ORYON_API void glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    texenv_scalar(target, pname, param);
}

ORYON_API void glTexEnvi(GLenum target, GLenum pname, GLint param) {
    texenv_scalar(target, pname, (float) param);
}

ORYON_API void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) {
    if (params) texenv_scalar(target, pname, params[0]);
}

ORYON_API void glTexGeni(GLenum coord, GLenum pname, GLint param) {
    const int c = coord_index(coord);
    if (c < 0) { set_error(GL_INVALID_ENUM); return; }
    if (pname != GL_TEXTURE_GEN_MODE) return;
    /* Hanya GL_EYE_LINEAR dan GL_OBJECT_LINEAR yang dipakai 1.12.2;
       GL_SPHERE_MAP dan GL_REFLECTION_MAP tidak pernah muncul. */
    A().tex[A().active_tex].gen_mode[c] =
        (uint8_t) ((GLenum) param == GL_EYE_LINEAR ? TG_EYE
                 : (GLenum) param == GL_OBJECT_LINEAR ? TG_OBJECT : TG_OBJECT);
}

ORYON_API void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params) {
    if (!params) return;
    const int c = coord_index(coord);
    if (c < 0) { set_error(GL_INVALID_ENUM); return; }
    TexEnv &t = A().tex[A().active_tex];

    if (pname == GL_OBJECT_PLANE) {
        memcpy(t.gen_plane[c], params, 4 * sizeof(float));
        return;
    }
    if (pname == GL_EYE_PLANE) {
        /* Aturan GL: koefisien bidang dikalikan invers modelview SAAT
           ditetapkan, sehingga koordinat nanti dihitung di ruang mata. */
        float inv[16], invT[16];
        if (mat4_inverse(inv, g_ff.mv[g_ff.mv_top])) {
            mat4_transpose(invT, inv);
            mat4_xform(t.gen_plane[c], invT, params);
        } else {
            memcpy(t.gen_plane[c], params, 4 * sizeof(float));
        }
        return;
    }
    if (pname == GL_TEXTURE_GEN_MODE) {
        t.gen_mode[c] = (uint8_t) ((GLenum) params[0] == GL_EYE_LINEAR ? TG_EYE : TG_OBJECT);
        return;
    }
    set_error(GL_INVALID_ENUM);
}

/* ------------------------------------------------------------ unit tekstur */

namespace {
void set_active(GLenum unit) {
    const int u = (int) (unit - GL_TEXTURE0);
    if (u < 0 || u >= FF_MAX_TEX) { set_error(GL_INVALID_ENUM); return; }
    A().active_tex = u;
    gles.glActiveTexture(unit);
}
void set_client_active(GLenum unit) {
    const int u = (int) (unit - GL_TEXTURE0);
    if (u < 0 || u >= FF_MAX_TEX) { set_error(GL_INVALID_ENUM); return; }
    A().client_tex = u;   /* murni sisi klien: GLES tidak punya padanannya */
}
}  /* namespace */

ORYON_API void glActiveTexture(GLenum t)          { if (ensure_init()) set_active(t); }
ORYON_API void glActiveTextureARB(GLenum t)       { if (ensure_init()) set_active(t); }
ORYON_API void glClientActiveTexture(GLenum t)    { set_client_active(t); }
ORYON_API void glClientActiveTextureARB(GLenum t) { set_client_active(t); }

/* --------------------------------------------------- nilai berjalan ------- */

ORYON_API void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    float *c = A().cur_color;
    c[0] = r; c[1] = g; c[2] = b; c[3] = a;
    /* Tahap 5 menambahkan penerbitan per-vertex saat di antara glBegin/glEnd. */
}

ORYON_API void glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
    float *n = A().cur_normal;
    n[0] = x; n[1] = y; n[2] = z;
}

/* ------------------------------------------------------- tumpukan atribut - */

/* Granularitas mask sengaja diabaikan: satu-satunya pemakaian di 1.12.2 adalah
   GL_ENABLE_BIT | GL_LIGHTING_BIT, dan itu persis himpunan yang kita modelkan. */
ORYON_API void glPushAttrib(GLbitfield mask) {
    if (g_ff.saved_top >= FF_ATTRIB_DEPTH) { set_error(GL_STACK_OVERFLOW); return; }
    g_ff.saved[g_ff.saved_top++] = g_ff.a;
}

ORYON_API void glPopAttrib(void) {
    if (g_ff.saved_top == 0) { set_error(GL_STACK_UNDERFLOW); return; }
    g_ff.a = g_ff.saved[--g_ff.saved_top];
    gles.glActiveTexture((GLenum) (GL_TEXTURE0 + g_ff.a.active_tex));
}
