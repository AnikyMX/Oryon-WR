/* Penerjemah GLSL 1.20 -> GLSL ES 3.20 untuk shader bawaan Minecraft. */
#ifndef ORYON_TRANSLATE_H
#define ORYON_TRANSLATE_H

#include "oryon/oryon.h"

namespace oryon {

/* GLSL 1.20 membolehkan `uniform float X = 1.5;`. GLSL ES 3.20 tidak.
   Nilainya tidak boleh hilang begitu saja - 20 uniform di shader Minecraft
   mengandalkannya - jadi diangkat ke sini lalu dipasang kembali setelah
   program tertaut. */
struct GlslDefault {
    char     name[48];
    unsigned comps;      /* 1..4 */
    float    v[4];
};

/* Menerjemahkan satu sumber shader. `attrib_out` diisi nama atribut pertama
   yang dideklarasikan (dipakai untuk mengikatnya ke lokasi 0), atau string
   kosong bila tidak ada. Mengembalikan panjang keluaran, 0 bila gagal. */
ORYON_LOCAL unsigned glsl_translate(const char *src, unsigned len, bool fragment,
                                    char *out, unsigned cap,
                                    char *attrib_out, unsigned attrib_cap,
                                    GlslDefault *defs, unsigned def_cap,
                                    unsigned *def_count);

}  /* namespace oryon */

#endif /* ORYON_TRANSLATE_H */
