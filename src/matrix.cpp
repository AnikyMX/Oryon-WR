// Oryon -- matrix stacks (MODELVIEW / PROJECTION / TEXTURE per unit), fully CPU-side.
#include "oryon.hpp"
#include "mathx.hpp"

namespace ory {

void mat_rotate(GLfloat *m, GLfloat angle_deg, GLfloat x, GLfloat y, GLfloat z) {
    double len = sqrt((double)x * x + (double)y * y + (double)z * z);
    if (len == 0.0) return;
    double a = angle_deg * (3.14159265358979323846 / 180.0);
    double c = cos(a), s = sin(a), ic = 1.0 - c;
    double nx = x / len, ny = y / len, nz = z / len;
    GLfloat r0 = (GLfloat)(nx * nx * ic + c), r1 = (GLfloat)(ny * nx * ic + nz * s), r2 = (GLfloat)(nx * nz * ic - ny * s);
    GLfloat r4 = (GLfloat)(nx * ny * ic - nz * s), r5 = (GLfloat)(ny * ny * ic + c), r6 = (GLfloat)(ny * nz * ic + nx * s);
    GLfloat r8 = (GLfloat)(nx * nz * ic + ny * s), r9 = (GLfloat)(ny * nz * ic - nx * s), r10 = (GLfloat)(nz * nz * ic + c);
    for (int i = 0; i < 4; ++i) {
        GLfloat c0 = m[i], c1 = m[4 + i], c2 = m[8 + i];
        m[i] = c0 * r0 + c1 * r1 + c2 * r2;
        m[4 + i] = c0 * r4 + c1 * r5 + c2 * r6;
        m[8 + i] = c0 * r8 + c1 * r9 + c2 * r10;
    }
}

bool mat_invert(GLfloat *r, const GLfloat *m) {
    double inv[16];
    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
    double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0.0) return false;
    det = 1.0 / det;
    for (int i = 0; i < 16; ++i) r[i] = (GLfloat)(inv[i] * det);
    return true;
}

void mat_normal(GLfloat *nm, const GLfloat *m, bool rescale) {
    // inverse of upper-left 3x3 (column-major a[c*4+r]) -> transpose -> normal matrix
    double a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], gg = m[2], h = m[6], i = m[10];
    double A = e * i - f * h, B = -(d * i - f * gg), C = d * h - e * gg;
    double det = a * A + b * B + c * C;
    if (det == 0.0) { mat_identity(nm); nm[0] = nm[4] = nm[8] = 1; return; }
    double id = 1.0 / det;
    // inverse (row-major inv[r][c])
    double inv[3][3] = {{A * id, -(b * i - c * h) * id, (b * f - c * e) * id},
                        {B * id, (a * i - c * gg) * id, -(a * f - c * d) * id},
                        {C * id, -(a * h - b * gg) * id, (a * e - b * d) * id}};
    double s = 1.0;
    if (rescale) {
        double l = sqrt(inv[2][0] * inv[2][0] + inv[2][1] * inv[2][1] + inv[2][2] * inv[2][2]);
        if (l > 0.0) s = 1.0 / l;
    }
    // normal matrix N = transpose(inv): column-major N[c*3+r] = N[r][c] = inv[c][r]
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) nm[col * 3 + row] = (GLfloat)(inv[col][row] * s);
}

void plane_to_eye(GLfloat *out, const GLfloat *p, const GLfloat *mv) {
    GLfloat inv[16];
    if (!mat_invert(inv, mv)) { memcpy(out, p, 16); return; }
    // row vector p * inv
    for (int c = 0; c < 4; ++c) out[c] = p[0] * inv[c * 4] + p[1] * inv[c * 4 + 1] + p[2] * inv[c * 4 + 2] + p[3] * inv[c * 4 + 3];
}

static void mat_select() {
    Matrices &M = g.m;
    switch (M.mode) {
    case GL_PROJECTION: M.cur = M.p[M.p_top].m; M.cur_ver = &M.p_ver; break;
    case GL_TEXTURE: { GLuint u = g.f.active; M.cur = M.t[u][M.t_top[u]].m; M.cur_ver = &M.t_ver[u]; break; }
    default: M.cur = M.mv[M.mv_top].m; M.cur_ver = &M.mv_ver; break;
    }
}
void mat_reselect() { mat_select(); }

void mat_defaults() {
    mat_identity(g.m.mv[0].m); mat_identity(g.m.p[0].m);
    for (int u = 0; u < MAX_TEX_UNITS; ++u) mat_identity(g.m.t[u][0].m);
    g.m.mode = GL_MODELVIEW; mat_select();
}
ORY_INLINE void touched() { *g.m.cur_ver = next_ver(); }

} // namespace ory

using namespace ory;

/* jar: GL11.glMatrixMode(I)V */
OGL_EXPORT void glMatrixMode(GLenum mode) { ORY_DL(glMatrixMode, mode);
    ORY_PROLOGUE();
    if (mode != GL_MODELVIEW && mode != GL_PROJECTION && mode != GL_TEXTURE) { set_error(GL_INVALID_ENUM); return; }
    g.m.mode = mode; mat_select();
}
/* jar: GL11.glLoadIdentity()V */
OGL_EXPORT void glLoadIdentity(void) { ORY_DL(glLoadIdentity); ORY_PROLOGUE(); mat_identity(g.m.cur); touched(); }
/* jar: GL11.nglLoadMatrixf(J)V */
OGL_EXPORT void glLoadMatrixf(const GLfloat *m) { ORY_DL(glLoadMatrixf, m); ORY_PROLOGUE(); memcpy(g.m.cur, m, 64); touched(); }
/* jar: GL11.nglLoadMatrixd(J)V */
OGL_EXPORT void glLoadMatrixd(const GLdouble *m) { ORY_DL(glLoadMatrixd, m); ORY_PROLOGUE(); for (int i = 0; i < 16; ++i) g.m.cur[i] = (GLfloat)m[i]; touched(); }
/* jar: GL11.nglMultMatrixf(J)V */
OGL_EXPORT void glMultMatrixf(const GLfloat *m) { ORY_DL(glMultMatrixf, m); ORY_PROLOGUE(); mat_mul(g.m.cur, g.m.cur, m); touched(); }
/* jar: GL11.nglMultMatrixd(J)V */
OGL_EXPORT void glMultMatrixd(const GLdouble *m) { ORY_DL(glMultMatrixd, m);
    ORY_PROLOGUE(); GLfloat f[16]; for (int i = 0; i < 16; ++i) f[i] = (GLfloat)m[i];
    mat_mul(g.m.cur, g.m.cur, f); touched();
}
/* jar: GL13.nglLoadTransposeMatrixf(J)V */
OGL_EXPORT void glLoadTransposeMatrixf(const GLfloat *m) { ORY_DL(glLoadTransposeMatrixf, m);
    ORY_PROLOGUE(); for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) g.m.cur[c * 4 + r] = m[r * 4 + c]; touched();
}
/* jar: GL13.nglLoadTransposeMatrixd(J)V */
OGL_EXPORT void glLoadTransposeMatrixd(const GLdouble *m) { ORY_DL(glLoadTransposeMatrixd, m);
    ORY_PROLOGUE(); for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) g.m.cur[c * 4 + r] = (GLfloat)m[r * 4 + c]; touched();
}
/* jar: GL13.nglMultTransposeMatrixf(J)V */
OGL_EXPORT void glMultTransposeMatrixf(const GLfloat *m) { ORY_DL(glMultTransposeMatrixf, m);
    ORY_PROLOGUE(); GLfloat f[16]; for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) f[c * 4 + r] = m[r * 4 + c];
    mat_mul(g.m.cur, g.m.cur, f); touched();
}
/* jar: GL13.nglMultTransposeMatrixd(J)V */
OGL_EXPORT void glMultTransposeMatrixd(const GLdouble *m) { ORY_DL(glMultTransposeMatrixd, m);
    ORY_PROLOGUE(); GLfloat f[16]; for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) f[c * 4 + r] = (GLfloat)m[r * 4 + c];
    mat_mul(g.m.cur, g.m.cur, f); touched();
}
/* jar: GL11.glTranslatef(FFF)V */
OGL_EXPORT void glTranslatef(GLfloat x, GLfloat y, GLfloat z) { ORY_DL(glTranslatef, x, y, z); ORY_PROLOGUE(); mat_translate(g.m.cur, x, y, z); touched(); }
/* jar: GL11.glTranslated(DDD)V */
OGL_EXPORT void glTranslated(GLdouble x, GLdouble y, GLdouble z) { ORY_DL(glTranslated, x, y, z); ORY_PROLOGUE(); mat_translate(g.m.cur, (GLfloat)x, (GLfloat)y, (GLfloat)z); touched(); }
/* jar: GL11.glScalef(FFF)V */
OGL_EXPORT void glScalef(GLfloat x, GLfloat y, GLfloat z) { ORY_DL(glScalef, x, y, z); ORY_PROLOGUE(); mat_scale(g.m.cur, x, y, z); touched(); }
/* jar: GL11.glScaled(DDD)V */
OGL_EXPORT void glScaled(GLdouble x, GLdouble y, GLdouble z) { ORY_DL(glScaled, x, y, z); ORY_PROLOGUE(); mat_scale(g.m.cur, (GLfloat)x, (GLfloat)y, (GLfloat)z); touched(); }
/* jar: GL11.glRotatef(FFFF)V */
OGL_EXPORT void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z) { ORY_DL(glRotatef, a, x, y, z); ORY_PROLOGUE(); mat_rotate(g.m.cur, a, x, y, z); touched(); }
/* jar: GL11.glRotated(DDDD)V */
OGL_EXPORT void glRotated(GLdouble a, GLdouble x, GLdouble y, GLdouble z) { ORY_DL(glRotated, a, x, y, z); ORY_PROLOGUE(); mat_rotate(g.m.cur, (GLfloat)a, (GLfloat)x, (GLfloat)y, (GLfloat)z); touched(); }
/* jar: GL11.glOrtho(DDDDDD)V */
OGL_EXPORT void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { ORY_DL(glOrtho, l, r, b, t, n, f);
    ORY_PROLOGUE();
    if (l == r || b == t || n == f) { set_error(GL_INVALID_VALUE); return; }
    GLfloat o[16] = {0};
    o[0] = (GLfloat)(2.0 / (r - l)); o[5] = (GLfloat)(2.0 / (t - b)); o[10] = (GLfloat)(-2.0 / (f - n));
    o[12] = (GLfloat)(-(r + l) / (r - l)); o[13] = (GLfloat)(-(t + b) / (t - b)); o[14] = (GLfloat)(-(f + n) / (f - n)); o[15] = 1.0f;
    mat_mul(g.m.cur, g.m.cur, o); touched();
}
/* jar: GL11.glFrustum(DDDDDD)V */
OGL_EXPORT void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { ORY_DL(glFrustum, l, r, b, t, n, f);
    ORY_PROLOGUE();
    if (n <= 0 || f <= 0 || l == r || b == t || n == f) { set_error(GL_INVALID_VALUE); return; }
    GLfloat o[16] = {0};
    o[0] = (GLfloat)(2.0 * n / (r - l)); o[5] = (GLfloat)(2.0 * n / (t - b));
    o[8] = (GLfloat)((r + l) / (r - l)); o[9] = (GLfloat)((t + b) / (t - b)); o[10] = (GLfloat)(-(f + n) / (f - n)); o[11] = -1.0f;
    o[14] = (GLfloat)(-2.0 * f * n / (f - n));
    mat_mul(g.m.cur, g.m.cur, o); touched();
}
/* jar: GL11.glPushMatrix()V */
OGL_EXPORT void glPushMatrix(void) { ORY_DL(glPushMatrix);
    ORY_PROLOGUE();
    Matrices &M = g.m;
    switch (M.mode) {
    case GL_PROJECTION:
        if (M.p_top + 1 >= P_STACK) { set_error(GL_STACK_OVERFLOW); return; }
        M.p[M.p_top + 1] = M.p[M.p_top]; ++M.p_top; break;
    case GL_TEXTURE: {
        GLuint u = g.f.active;
        if (M.t_top[u] + 1 >= T_STACK) { set_error(GL_STACK_OVERFLOW); return; }
        M.t[u][M.t_top[u] + 1] = M.t[u][M.t_top[u]]; ++M.t_top[u]; break;
    }
    default:
        if (M.mv_top + 1 >= MV_STACK) { set_error(GL_STACK_OVERFLOW); return; }
        M.mv[M.mv_top + 1] = M.mv[M.mv_top]; ++M.mv_top; break;
    }
    mat_select();            // same values: no version bump
}
/* jar: GL11.glPopMatrix()V */
OGL_EXPORT void glPopMatrix(void) { ORY_DL(glPopMatrix);
    ORY_PROLOGUE();
    Matrices &M = g.m;
    switch (M.mode) {
    case GL_PROJECTION: if (M.p_top == 0) { set_error(GL_STACK_UNDERFLOW); return; } --M.p_top; break;
    case GL_TEXTURE: { GLuint u = g.f.active; if (M.t_top[u] == 0) { set_error(GL_STACK_UNDERFLOW); return; } --M.t_top[u]; break; }
    default: if (M.mv_top == 0) { set_error(GL_STACK_UNDERFLOW); return; } --M.mv_top; break;
    }
    mat_select(); touched();
}
