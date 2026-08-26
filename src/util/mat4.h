/* Matriks 4x4 kolom-major, tata letak persis seperti OpenGL.
   Semua operasi bekerja di tempat pada float[16]; tanpa alokasi, tanpa STL. */
#ifndef ORYON_MAT4_H
#define ORYON_MAT4_H

#include "oryon/oryon.h"
#include <string.h>

namespace oryon {

ORYON_INLINE void mat4_copy(float *d, const float *s) { memcpy(d, s, 16 * sizeof(float)); }

ORYON_INLINE void mat4_identity(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* out = a * b, konvensi GL (a diterapkan setelah b pada vektor kolom). */
ORYON_INLINE void mat4_mul(float *out, const float *a, const float *b) {
    float t[16];
    for (int c = 0; c < 4; ++c) {
        const float b0 = b[c * 4 + 0], b1 = b[c * 4 + 1],
                    b2 = b[c * 4 + 2], b3 = b[c * 4 + 3];
        t[c * 4 + 0] = a[0] * b0 + a[4] * b1 + a[8]  * b2 + a[12] * b3;
        t[c * 4 + 1] = a[1] * b0 + a[5] * b1 + a[9]  * b2 + a[13] * b3;
        t[c * 4 + 2] = a[2] * b0 + a[6] * b1 + a[10] * b2 + a[14] * b3;
        t[c * 4 + 3] = a[3] * b0 + a[7] * b1 + a[11] * b2 + a[15] * b3;
    }
    memcpy(out, t, sizeof t);
}

ORYON_INLINE bool mat4_is_identity(const float *m) {
    static const float I[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    return memcmp(m, I, sizeof I) == 0;
}

/* v' = m * v */
ORYON_INLINE void mat4_xform(float *out, const float *m, const float *v) {
    const float x = v[0], y = v[1], z = v[2], w = v[3];
    out[0] = m[0] * x + m[4] * y + m[8]  * z + m[12] * w;
    out[1] = m[1] * x + m[5] * y + m[9]  * z + m[13] * w;
    out[2] = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
    out[3] = m[3] * x + m[7] * y + m[11] * z + m[15] * w;
}

ORYON_LOCAL void mat4_translate(float *m, float x, float y, float z);
ORYON_LOCAL void mat4_rotate(float *m, float deg, float x, float y, float z);
ORYON_LOCAL void mat4_scale(float *m, float x, float y, float z);
ORYON_LOCAL void mat4_ortho(float *m, double l, double r, double b, double t,
                            double n, double f);
ORYON_LOCAL void mat4_transpose(float *out, const float *in);
ORYON_LOCAL bool mat4_inverse(float *out, const float *in);
/* Matriks normal: transpos-invers bagian 3x3, ditulis sebagai mat3 kolom-major. */
ORYON_LOCAL void mat4_normal_matrix(float *out9, const float *mv);

}  /* namespace oryon */

#endif /* ORYON_MAT4_H */
