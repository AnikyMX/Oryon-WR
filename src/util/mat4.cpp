#include "mat4.h"
#include <math.h>

namespace oryon {

void mat4_translate(float *m, float x, float y, float z) {
    m[12] += m[0] * x + m[4] * y + m[8]  * z;
    m[13] += m[1] * x + m[5] * y + m[9]  * z;
    m[14] += m[2] * x + m[6] * y + m[10] * z;
    m[15] += m[3] * x + m[7] * y + m[11] * z;
}

void mat4_scale(float *m, float x, float y, float z) {
    for (int i = 0; i < 4; ++i) {
        m[i]     *= x;
        m[4 + i] *= y;
        m[8 + i] *= z;
    }
}

void mat4_rotate(float *m, float deg, float x, float y, float z) {
    const float len = sqrtf(x * x + y * y + z * z);
    if (len == 0.0f) return;
    x /= len; y /= len; z /= len;

    const float a = deg * 0.01745329252f;      /* derajat -> radian */
    const float s = sinf(a), c = cosf(a), t = 1.0f - c;

    float r[16];
    r[0]  = t * x * x + c;      r[1]  = t * x * y + s * z;  r[2]  = t * x * z - s * y;  r[3]  = 0.0f;
    r[4]  = t * x * y - s * z;  r[5]  = t * y * y + c;      r[6]  = t * y * z + s * x;  r[7]  = 0.0f;
    r[8]  = t * x * z + s * y;  r[9]  = t * y * z - s * x;  r[10] = t * z * z + c;      r[11] = 0.0f;
    r[12] = 0.0f;               r[13] = 0.0f;               r[14] = 0.0f;               r[15] = 1.0f;
    mat4_mul(m, m, r);
}

void mat4_ortho(float *m, double l, double r, double b, double t, double n, double f) {
    float o[16];
    memset(o, 0, sizeof o);
    o[0]  = (float) (2.0 / (r - l));
    o[5]  = (float) (2.0 / (t - b));
    o[10] = (float) (-2.0 / (f - n));
    o[12] = (float) (-(r + l) / (r - l));
    o[13] = (float) (-(t + b) / (t - b));
    o[14] = (float) (-(f + n) / (f - n));
    o[15] = 1.0f;
    mat4_mul(m, m, o);
}

void mat4_transpose(float *out, const float *in) {
    float t[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            t[c * 4 + r] = in[r * 4 + c];
    memcpy(out, t, sizeof t);
}

/* Invers umum lewat kofaktor. Hanya dipanggil di jalur dingin
   (glTexGen bidang mata, kueri matriks), jadi kejelasan menang atas kecepatan. */
bool mat4_inverse(float *out, const float *m) {
    float inv[16];
    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
             + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
             - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
             + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
             - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
             - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
             + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
             - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
             + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15]
             + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15]
             - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15]
             + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14]
             - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
    inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11]
             - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11]
             + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11]
             - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10]
             + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

    float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (det == 0.0f) return false;
    det = 1.0f / det;
    for (int i = 0; i < 16; ++i) out[i] = inv[i] * det;
    return true;
}

void mat4_normal_matrix(float *out9, const float *mv) {
    float inv[16];
    if (!mat4_inverse(inv, mv)) {
        out9[0] = out9[4] = out9[8] = 1.0f;
        out9[1] = out9[2] = out9[3] = out9[5] = out9[6] = out9[7] = 0.0f;
        return;
    }
    /* transpos bagian 3x3 dari invers */
    out9[0] = inv[0];  out9[1] = inv[4];  out9[2] = inv[8];
    out9[3] = inv[1];  out9[4] = inv[5];  out9[5] = inv[9];
    out9[6] = inv[2];  out9[7] = inv[6];  out9[8] = inv[10];
}

}  /* namespace oryon */
