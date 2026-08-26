#include "ff_state.h"
#include "../util/mat4.h"

#include <string.h>

namespace oryon {

FFState g_ff;

void ff_init() {
    memset(&g_ff, 0, sizeof g_ff);

    g_ff.mode = GL_MODELVIEW;
    mat4_identity(g_ff.mv[0]);
    mat4_identity(g_ff.pr[0]);
    for (int u = 0; u < FF_MAX_TEX; ++u) mat4_identity(g_ff.tx[u][0]);
    g_ff.serial = 1;
    g_ff.uni_serial = 1;

    FFAttribState &a = g_ff.a;
    a.fog_mode    = FOG_EXP;
    a.fog_density = 1.0f;
    a.fog_start   = 0.0f;
    a.fog_end     = 1.0f;

    a.alpha_func = GL_ALWAYS;
    a.alpha_ref  = 0.0f;

    /* Bawaan GL: arah cahaya (0,0,1,0); LIGHT0 difus putih, sisanya hitam. */
    for (int i = 0; i < FF_MAX_LIGHTS; ++i) a.light[i].position[2] = 1.0f;
    for (int c = 0; c < 4; ++c) a.light[0].diffuse[c] = 1.0f;

    a.lm_ambient[0] = a.lm_ambient[1] = a.lm_ambient[2] = 0.2f;
    a.lm_ambient[3] = 1.0f;
    a.color_material_mode = GL_AMBIENT_AND_DIFFUSE;

    for (int u = 0; u < FF_MAX_TEX; ++u) {
        a.tex[u].env_mode    = ENV_MODULATE;
        /* Nilai bawaan GL untuk combiner. Kombinasi ini setara persis dengan
           GL_MODULATE, dan itulah satu-satunya konfigurasi yang benar-benar
           disetel Minecraft 1.12.2. */
        a.tex[u].raw_mode    = GL_MODULATE;
        a.tex[u].combine_rgb = GL_MODULATE;
        a.tex[u].src0_rgb    = GL_TEXTURE;
        a.tex[u].src1_rgb    = GL_PREVIOUS;
        a.tex[u].op0_rgb     = GL_SRC_COLOR;
        a.tex[u].op1_rgb     = GL_SRC_COLOR;
        a.tex[u].gen_plane[0][0] = 1.0f;   /* S = (1,0,0,0) */
        a.tex[u].gen_plane[1][1] = 1.0f;   /* T = (0,1,0,0) */
    }

    for (int c = 0; c < 4; ++c) a.cur_color[c] = 1.0f;
    a.cur_normal[2] = 1.0f;
    /* cur_tex sudah nol dari memset. */
}

float *ff_tex_matrix(int unit) { return g_ff.tx[unit][g_ff.tx_top[unit]]; }

float *ff_current_matrix() {
    switch (g_ff.mode) {
    case GL_PROJECTION: return g_ff.pr[g_ff.pr_top];
    case GL_TEXTURE:    return ff_tex_matrix(g_ff.a.active_tex);
    default:            return g_ff.mv[g_ff.mv_top];
    }
}

void ff_key(FFKey *k, uint16_t attr_tex, bool attr_color, bool attr_normal) {
    memset(k, 0, sizeof *k);
    const FFAttribState &a = g_ff.a;

    for (int u = 0; u < FF_MAX_TEX; ++u) {
        const TexEnv &t = a.tex[u];
        if (!t.enabled) continue;
        const uint16_t bitu = (uint16_t) (1u << u);
        k->tex_enable |= bitu;
        if (t.env_mode == ENV_REPLACE) k->tex_replace |= bitu;
        if (t.gen_mask) {
            k->tex_gen |= bitu;
            /* Minecraft memakai mode sama untuk semua koordinat aktif;
               koordinat S yang menentukan. */
            if (t.gen_mode[0] == TG_EYE) k->tex_gen_eye |= bitu;
        }
        if (!mat4_is_identity(g_ff.tx[u][g_ff.tx_top[u]])) k->tex_matrix |= bitu;
    }
    k->attr_tex = (uint16_t) (attr_tex & k->tex_enable);

    uint8_t f = 0;
    if (a.lighting)       f |= FF_LIGHTING;
    if (a.color_material) f |= FF_COLOR_MATERIAL;
    if (a.normalize || a.rescale_normal) f |= FF_NORMALIZE;
    if (a.fog)            f |= FF_FOG;
    if (a.alpha_test && a.alpha_func != GL_ALWAYS) f |= FF_ALPHA_TEST;
    if (a.flat_shade)     f |= FF_FLAT;
    if (attr_color)       f |= FF_ATTR_COLOR;
    if (attr_normal)      f |= FF_ATTR_NORMAL;
    k->flags = f;

    k->light_mask = a.lighting ? a.light_mask : 0;
    k->fog_mode   = a.fog ? a.fog_mode : 0;
    k->alpha_func = (uint8_t) ((f & FF_ALPHA_TEST)
                               ? (uint8_t) ((a.alpha_func - GL_NEVER) & 7u) : 0u);
}

}  /* namespace oryon */
