/* Generator shader fixed-function.

   Satu FFKey -> satu program GLSL ES 320. Program di-cache dengan alamat
   terbuka; sekali sebuah kombinasi state dipakai, biaya generasinya nol
   selamanya. Tidak ada nilai numerik yang ditanam ke dalam sumber shader -
   semuanya uniform, supaya kunci tetap sedikit dan cache tetap kecil. */
#ifndef ORYON_SHADERGEN_H
#define ORYON_SHADERGEN_H

#include "oryon/oryon.h"
#include "../state/ff_state.h"

namespace oryon {

/* Lokasi atribut dipatok, jadi satu tata letak VAO berlaku untuk semua
   program - mengganti shader tidak pernah memaksa menata ulang atribut. */
enum {
    ATTR_POS    = 0,
    ATTR_COLOR  = 1,
    ATTR_NORMAL = 2,
    ATTR_TEX0   = 3        /* .. ATTR_TEX0 + 7 */
};

struct FFProgram {
    FFKey  key;
    GLuint prog;
    GLint  u_mvp, u_mv, u_nrm, u_color, u_alpha_ref;
    GLint  u_fog_color, u_fog_start, u_fog_end, u_fog_density;
    GLint  u_lm_ambient;
    GLint  u_light_pos[FF_MAX_LIGHTS];
    GLint  u_light_diff[FF_MAX_LIGHTS];
    GLint  u_light_amb[FF_MAX_LIGHTS];
    GLint  u_tex_mat[FF_MAX_TEX];
    GLint  u_tg_s[FF_MAX_TEX];
    GLint  u_tg_t[FF_MAX_TEX];
    bool   used;
};

/* Membangun sumber GLSL. Dipakai langsung oleh tes; di jalur normal dipanggil
   dari ffp_program(). Mengembalikan panjang yang ditulis, 0 bila kehabisan
   ruang. */
ORYON_LOCAL unsigned ffp_gen_vertex(const FFKey &k, char *out, unsigned cap);
ORYON_LOCAL unsigned ffp_gen_fragment(const FFKey &k, char *out, unsigned cap);

/* Ambil dari cache, kompilasi bila belum ada. NULL bila kompilasi gagal. */
ORYON_LOCAL FFProgram *ffp_program(const FFKey &k);

/* Pasang program untuk state sekarang dan unggah seluruh uniform-nya. */
ORYON_LOCAL bool ffp_bind(const FFKey &k);

ORYON_LOCAL void ffp_reset();

/* Program yang sedang dipasang Minecraft sendiri lewat glUseProgram.
   Selama bukan nol, jalur gambar tidak boleh memasang program fixed-function -
   persis seperti GL sungguhan, di mana program pengguna menggantikan pipeline
   tetap sampai glUseProgram(0). */
ORYON_LOCAL extern GLuint g_user_program;

/* Membatalkan cache pengikatan setelah program diganti dari luar. */
ORYON_LOCAL void ffp_invalidate();

}  /* namespace oryon */

#endif /* ORYON_SHADERGEN_H */
