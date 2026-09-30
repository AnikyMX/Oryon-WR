// Oryon -- immediate-mode / current-attribute fast paths (inlined into generated glVertex*/glColor*/... exports).
#pragma once
namespace ory {
ORY_INLINE GLfloat unorm_ub(GLubyte x) { return (GLfloat)x * (1.0f / 255.0f); }
ORY_INLINE GLfloat unorm_us(GLushort x) { return (GLfloat)x * (1.0f / 65535.0f); }
ORY_INLINE GLfloat unorm_ui(GLuint x) { return (GLfloat)((double)x * (1.0 / 4294967295.0)); }
ORY_INLINE GLfloat snorm_b(GLbyte x) { GLfloat f = (GLfloat)x * (1.0f / 127.0f); return f < -1.0f ? -1.0f : f; }
ORY_INLINE GLfloat snorm_s(GLshort x) { GLfloat f = (GLfloat)x * (1.0f / 32767.0f); return f < -1.0f ? -1.0f : f; }
ORY_INLINE GLfloat snorm_i(GLint x) { GLfloat f = (GLfloat)((double)x * (1.0 / 2147483647.0)); return f < -1.0f ? -1.0f : f; }

bool dl_cur(uint32_t which, GLfloat x, GLfloat y, GLfloat z, GLfloat w);   // true: GL_COMPILE (do not execute)
bool dl_vattrib(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
bool imm_grow();
void imm_vary(uint32_t bit);
void imm_flush();

ORY_INLINE void imm_fill(ImmRec &r, uint32_t v) {
    if (v & AB_COLOR) memcpy(r.col, &g.cur.color, 16);
    if (v & AB_NORMAL) memcpy(r.nrm, &g.cur.normal, 16);
    if (UNLIKELY(v & (AB_COLOR2 | AB_FOG))) {
        if (v & AB_COLOR2) memcpy(r.col2, &g.cur.color2, 16);
        if (v & AB_FOG) r.fog = g.cur.fog;
    }
    uint32_t t = v >> 4;
    if (t & 1u) memcpy(r.tex[0], &g.cur.tex[0], 16);
    if (UNLIKELY(t & ~1u))
        for (int u = 1; u < MAX_TEX_UNITS; ++u) if (t & (1u << u)) memcpy(r.tex[u], &g.cur.tex[u], 16);
}
ORY_INLINE void imm_vertex(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    Imm &I = g.imm;
    if (UNLIKELY(!I.inside)) return;
    if (UNLIKELY(I.n == I.cap) && !imm_grow()) return;
    ImmRec &r = I.rec[I.n++];
    r.pos[0] = x; r.pos[1] = y; r.pos[2] = z; r.pos[3] = w;
    if (I.vary) imm_fill(r, I.vary);
}
#define ORY_DL_CUR(bit, x, y, z, w) if (UNLIKELY(g.dl.mode) && !g.imm.inside && dl_cur(bit, x, y, z, w)) return
ORY_INLINE void cur_color(GLfloat r, GLfloat gg, GLfloat b, GLfloat a) {
    ORY_DL_CUR(AB_COLOR, r, gg, b, a);
    if (UNLIKELY(g.imm.inside) && !(g.imm.vary & AB_COLOR)) imm_vary(AB_COLOR);
    g.cur.color = Vec4{r, gg, b, a};
}
ORY_INLINE void cur_color2(GLfloat r, GLfloat gg, GLfloat b) {
    ORY_DL_CUR(AB_COLOR2, r, gg, b, 1.0f);
    if (UNLIKELY(g.imm.inside) && !(g.imm.vary & AB_COLOR2)) imm_vary(AB_COLOR2);
    g.cur.color2 = Vec4{r, gg, b, 1.0f};
}
ORY_INLINE void cur_normal(GLfloat x, GLfloat y, GLfloat z) {
    ORY_DL_CUR(AB_NORMAL, x, y, z, 0.0f);
    if (UNLIKELY(g.imm.inside) && !(g.imm.vary & AB_NORMAL)) imm_vary(AB_NORMAL);
    g.cur.normal = Vec4{x, y, z, 0.0f};
}
ORY_INLINE void cur_texcoord(GLuint unit, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    if (UNLIKELY(unit >= (GLuint)MAX_TEX_UNITS)) { set_error(GL_INVALID_ENUM); return; }
    uint32_t bit = AB_TEX0 << unit;
    ORY_DL_CUR(bit, s, t, r, q);
    if (UNLIKELY(g.imm.inside) && !(g.imm.vary & bit)) imm_vary(bit);
    g.cur.tex[unit] = Vec4{s, t, r, q};
}
ORY_INLINE void cur_fogcoord(GLfloat f) {
    ORY_DL_CUR(AB_FOG, f, 0.0f, 0.0f, 1.0f);
    if (UNLIKELY(g.imm.inside) && !(g.imm.vary & AB_FOG)) imm_vary(AB_FOG);
    g.cur.fog = f;
}
void raster_pos(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void window_pos(GLfloat x, GLfloat y, GLfloat z);
void rectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void vattrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void vattrib4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
void vattrib4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
} // namespace ory
