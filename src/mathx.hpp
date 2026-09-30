// Oryon -- column-major 4x4 math (matches GL memory layout).
#pragma once
namespace ory {
ORY_INLINE void mat_identity(GLfloat *m) {
    memset(m, 0, 16 * sizeof(GLfloat)); m[0] = m[5] = m[10] = m[15] = 1.0f;
}
ORY_INLINE void mat_mul(GLfloat *r, const GLfloat *a, const GLfloat *b) {   // r = a * b (r may alias a or b)
    GLfloat t[16];
    for (int c = 0; c < 4; ++c) {
        const GLfloat b0 = b[c * 4], b1 = b[c * 4 + 1], b2 = b[c * 4 + 2], b3 = b[c * 4 + 3];
        for (int i = 0; i < 4; ++i) t[c * 4 + i] = a[i] * b0 + a[4 + i] * b1 + a[8 + i] * b2 + a[12 + i] * b3;
    }
    memcpy(r, t, sizeof t);
}
ORY_INLINE void mat_translate(GLfloat *m, GLfloat x, GLfloat y, GLfloat z) {
    for (int i = 0; i < 4; ++i) m[12 + i] += m[i] * x + m[4 + i] * y + m[8 + i] * z;
}
ORY_INLINE void mat_scale(GLfloat *m, GLfloat x, GLfloat y, GLfloat z) {
    for (int i = 0; i < 4; ++i) { m[i] *= x; m[4 + i] *= y; m[8 + i] *= z; }
}
ORY_INLINE void mat_xform4(GLfloat *o, const GLfloat *m, const GLfloat *v) {
    GLfloat x = v[0], y = v[1], z = v[2], w = v[3];
    for (int i = 0; i < 4; ++i) o[i] = m[i] * x + m[4 + i] * y + m[8 + i] * z + m[12 + i] * w;
}
void mat_rotate(GLfloat *m, GLfloat angle_deg, GLfloat x, GLfloat y, GLfloat z);
bool mat_invert(GLfloat *r, const GLfloat *m);
void mat_normal(GLfloat *nm3, const GLfloat *mv, bool rescale);   // inverse-transpose of upper 3x3
void plane_to_eye(GLfloat *out, const GLfloat *plane, const GLfloat *mv);   // p * inverse(MV)
} // namespace ory
