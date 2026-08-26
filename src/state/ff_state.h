/* State fixed-function: semua yang tidak punya padanan di GLES 3.2.

   Dipecah dua dengan sengaja:
     FFAttribState  bagian yang disimpan glPushAttrib - satu memcpy
     FFState        pembungkusnya, plus tumpukan matriks yang jauh lebih besar
                    dan memang tidak pernah ikut tersimpan glPushAttrib

   Ringkasannya adalah FFKey: 16 byte yang menentukan satu program GLSL. */
#ifndef ORYON_FF_STATE_H
#define ORYON_FF_STATE_H

#include "oryon/oryon.h"
#include <stdint.h>

namespace oryon {

enum {
    FF_MAX_TEX      = 8,
    FF_MAX_LIGHTS   = 8,
    FF_MV_DEPTH     = 32,
    FF_PR_DEPTH     = 8,
    FF_TX_DEPTH     = 8,
    FF_ATTRIB_DEPTH = 16
};

enum { ENV_MODULATE = 0, ENV_REPLACE = 1 };
enum { TG_OBJECT = 0, TG_EYE = 1 };
enum { FOG_LINEAR = 0, FOG_EXP = 1, FOG_EXP2 = 2 };

/* Bit di FFKey::flags */
enum {
    FF_LIGHTING       = 1u << 0,
    FF_COLOR_MATERIAL = 1u << 1,
    FF_NORMALIZE      = 1u << 2,
    FF_FOG            = 1u << 3,
    FF_ALPHA_TEST     = 1u << 4,
    FF_FLAT           = 1u << 5,
    FF_ATTR_COLOR     = 1u << 6,
    FF_ATTR_NORMAL    = 1u << 7
};

/* Persis 16 byte: dibandingkan dan di-hash sebagai blok bita. */
struct FFKey {
    uint16_t tex_enable;
    uint16_t tex_replace;
    uint16_t tex_gen;
    uint16_t tex_gen_eye;
    uint16_t tex_matrix;
    uint16_t attr_tex;
    uint8_t  light_mask;
    uint8_t  flags;
    uint8_t  fog_mode;
    uint8_t  alpha_func;      /* 0..7, GL_NEVER..GL_ALWAYS */
};

struct TexEnv {
    bool     enabled;
    uint8_t  env_mode;        /* mode efektif setelah combiner diringkas */
    GLenum   raw_mode;        /* GL_MODULATE / GL_REPLACE / GL_COMBINE apa adanya */
    GLenum   combine_rgb, src0_rgb, src1_rgb, op0_rgb, op1_rgb;
    uint8_t  gen_mask;        /* bit 0..3 = S,T,R,Q */
    uint8_t  gen_mode[4];
    float    gen_plane[4][4]; /* EYE_LINEAR sudah ditransformasi saat ditetapkan */
    float    lod_bias;
};

struct Light {
    float ambient[4];
    float diffuse[4];
    float position[4];        /* ruang mata, sesuai aturan GL */
};

/* Bagian yang ikut glPushAttrib. */
struct FFAttribState {
    bool     lighting, color_material, normalize, rescale_normal;
    bool     fog, alpha_test, flat_shade;
    uint8_t  light_mask;

    uint8_t  fog_mode;
    float    fog_color[4], fog_start, fog_end, fog_density;

    GLenum   alpha_func;
    float    alpha_ref;

    Light    light[FF_MAX_LIGHTS];
    float    lm_ambient[4];
    GLenum   color_material_mode;

    TexEnv   tex[FF_MAX_TEX];
    int      active_tex, client_tex;

    float    cur_color[4];
    float    cur_normal[3];
    float    cur_tex[FF_MAX_TEX][2];
};

struct FFState {
    GLenum   mode;
    float    mv[FF_MV_DEPTH][16];              int mv_top;
    float    pr[FF_PR_DEPTH][16];              int pr_top;
    float    tx[FF_MAX_TEX][FF_TX_DEPTH][16];  int tx_top[FF_MAX_TEX];
    uint32_t serial;      /* naik tiap matriks berubah */
    uint32_t uni_serial;  /* naik tiap uniform non-matriks berubah */

    FFAttribState a;

    FFAttribState saved[FF_ATTRIB_DEPTH];
    int           saved_top;
};

ORYON_LOCAL extern FFState g_ff;

ORYON_LOCAL void ff_init();
ORYON_LOCAL float *ff_current_matrix();
ORYON_LOCAL float *ff_tex_matrix(int unit);
ORYON_LOCAL void ff_key(FFKey *out, uint16_t attr_tex, bool attr_color, bool attr_normal);

/* Dipanggil tiap kali warna, uji alpha, kabut, cahaya, atau bidang texgen
   berubah. Tanpa penanda ini, ffp_bind mengunggah ulang setiap uniform pada
   SETIAP draw call - dan Minecraft mengeluarkan ribuan draw call per frame. */
ORYON_INLINE void ff_touch_uniform() { ++g_ff.uni_serial; }

}  /* namespace oryon */

#endif /* ORYON_FF_STATE_H */
