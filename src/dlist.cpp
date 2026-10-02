// Oryon -- display lists. Recording: op stream (thunk calls with copied args, current-attribute ops, nested calls)
// + captured geometry (immediate blocks and client/VBO arrays, dereferenced at compile time). Consecutive
// compatible geometry is merged at compile time and uploaded once to a per-list VBO/IBO/VAO at glEndList, so
// glCallList costs one VAO bind + one draw per geometry run; non-captured attributes come from current state.
#include "oryon.hpp"
#include "imm.hpp"
#include "bind.hpp"
#include "vertex_int.hpp"

// entry points invoked by replay thunks (defined in other translation units)
/* jar: GL11.nglLightfv(IIJ)V */
OGL_EXPORT void glLightfv(GLenum light, GLenum pname, const GLfloat *params);
/* jar: GL11.nglLightiv(IIJ)V */
OGL_EXPORT void glLightiv(GLenum light, GLenum pname, const GLint *params);
/* jar: GL11.nglLightModelfv(IJ)V */
OGL_EXPORT void glLightModelfv(GLenum pname, const GLfloat *params);
/* jar: GL11.nglLightModeliv(IJ)V */
OGL_EXPORT void glLightModeliv(GLenum pname, const GLint *params);
/* jar: GL11.nglMaterialfv(IIJ)V */
OGL_EXPORT void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
/* jar: GL11.nglMaterialiv(IIJ)V */
OGL_EXPORT void glMaterialiv(GLenum face, GLenum pname, const GLint *params);
/* jar: GL11.nglFogfv(IJ)V */
OGL_EXPORT void glFogfv(GLenum pname, const GLfloat *params);
/* jar: GL11.nglFogiv(IJ)V */
OGL_EXPORT void glFogiv(GLenum pname, const GLint *params);
/* jar: GL11.nglTexEnvfv(IIJ)V */
OGL_EXPORT void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params);
/* jar: GL11.nglTexEnviv(IIJ)V */
OGL_EXPORT void glTexEnviv(GLenum target, GLenum pname, const GLint *params);
/* jar: GL11.nglTexGenfv(IIJ)V */
OGL_EXPORT void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params);
/* jar: GL11.nglTexGeniv(IIJ)V */
OGL_EXPORT void glTexGeniv(GLenum coord, GLenum pname, const GLint *params);
/* jar: GL11.nglTexGendv(IIJ)V */
OGL_EXPORT void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params);
/* jar: GL11C.nglTexParameterfv(IIJ)V */
OGL_EXPORT void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);
/* jar: GL11C.nglTexParameteriv(IIJ)V */
OGL_EXPORT void glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
/* jar: GL11.nglLoadMatrixf(J)V */
OGL_EXPORT void glLoadMatrixf(const GLfloat *m);
/* jar: GL11.nglLoadMatrixd(J)V */
OGL_EXPORT void glLoadMatrixd(const GLdouble *m);
/* jar: GL11.nglMultMatrixf(J)V */
OGL_EXPORT void glMultMatrixf(const GLfloat *m);
/* jar: GL11.nglMultMatrixd(J)V */
OGL_EXPORT void glMultMatrixd(const GLdouble *m);
/* jar: GL13.nglLoadTransposeMatrixf(J)V */
OGL_EXPORT void glLoadTransposeMatrixf(const GLfloat *m);
/* jar: GL13.nglLoadTransposeMatrixd(J)V */
OGL_EXPORT void glLoadTransposeMatrixd(const GLdouble *m);
/* jar: GL13.nglMultTransposeMatrixf(J)V */
OGL_EXPORT void glMultTransposeMatrixf(const GLfloat *m);
/* jar: GL13.nglMultTransposeMatrixd(J)V */
OGL_EXPORT void glMultTransposeMatrixd(const GLdouble *m);
/* jar: GL11.nglClipPlane(IJ)V */
OGL_EXPORT void glClipPlane(GLenum plane, const GLdouble *equation);

namespace ory {
enum : uint8_t { OP_CALL = 1, OP_GEOM, OP_CUR, OP_VATTR, OP_CALLLIST, OP_CALLLISTS, OP_LISTBASE };
struct OpHdr { uint8_t op, pad0, pad1, pad2; uint32_t size; };

struct DLGeom {
    GLenum mode; ImmLayout lay;
    uint8_t *v; uint32_t vsize, vcap, nverts;
    uint32_t *idx; uint32_t ni, icap;
    GLuint vbo, ibo, vao; GLenum itype; GLsizei nidx; uint32_t amask;
    uint32_t fmask; ImmRec fin;
};
struct DList {
    uint8_t *ops; uint32_t size, cap;
    DLGeom **geoms; uint32_t ngeom, gcap;
    int32_t last_geom;                        // op offset when the last op is OP_GEOM, else -1
};

static DList **g_lists = nullptr;
static GLuint g_cap = 0, g_next = 1;

static DList *list_new() { DList *d = (DList *)calloc(1, sizeof(DList)); if (d) d->last_geom = -1; return d; }
static void list_free(DList *d) {
    if (!d) return;
    for (uint32_t i = 0; i < d->ngeom; ++i) {
        DLGeom *G = d->geoms[i];
        if (G->vbo) es.glDeleteBuffers(1, &G->vbo);
        if (G->ibo) es.glDeleteBuffers(1, &G->ibo);
        if (G->vao) { es.glDeleteVertexArrays(1, &G->vao); if (g.b.es_vao == G->vao) g.b.es_vao = 0; }
        if (g.b.es_copy_write == G->vbo || g.b.es_copy_write == G->ibo) g.b.es_copy_write = 0;
        if (g.b.es_array == G->vbo) g.b.es_array = 0;
        free(G->v); free(G->idx); free(G);
    }
    free(d->geoms); free(d->ops); free(d);
}
static bool ensure_cap(GLuint id) {
    if (id < g_cap) return true;
    GLuint nc = g_cap ? g_cap : 1024;
    while (nc <= id) nc *= 2;
    DList **n = (DList **)realloc(g_lists, (size_t)nc * sizeof(DList *));
    if (!n) return false;
    memset(n + g_cap, 0, (size_t)(nc - g_cap) * sizeof(DList *));
    g_lists = n; g_cap = nc;
    return true;
}
static inline DList *get_list(GLuint id) { return id && id < g_cap ? g_lists[id] : nullptr; }

static uint8_t *push_op(uint8_t op, uint32_t payload) {
    DList *d = g.dl.cur;
    if (!d) return nullptr;
    payload = (payload + 7u) & ~7u;
    uint32_t need = d->size + (uint32_t)sizeof(OpHdr) + payload;
    if (need > d->cap) {
        uint32_t nc = d->cap ? d->cap : 256;
        while (nc < need) nc *= 2;
        uint8_t *n = (uint8_t *)realloc(d->ops, nc);
        if (!n) { set_error(GL_OUT_OF_MEMORY); return nullptr; }
        d->ops = n; d->cap = nc;
    }
    OpHdr h = {op, 0, 0, 0, payload};
    uint32_t at = d->size;
    memcpy(d->ops + at, &h, sizeof h);
    memset(d->ops + at + sizeof h, 0, payload);
    d->size = need;
    d->last_geom = op == OP_GEOM ? (int32_t)at : -1;
    return d->ops + at + sizeof h;
}

void dl_push_call(DlThunk fn, const void *args, size_t size) {
    uint8_t *p = push_op(OP_CALL, (uint32_t)(8 + size));
    if (!p) return;
    memcpy(p, &fn, sizeof fn);
    if (size) memcpy(p + 8, args, size);
}
bool dl_cur(uint32_t which, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if (uint8_t *p = push_op(OP_CUR, 24)) { GLfloat v[4] = {x, y, z, w}; memcpy(p, &which, 4); memcpy(p + 8, v, 16); }
    return g.dl.mode == GL_COMPILE;
}
bool dl_vattrib(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if (uint8_t *p = push_op(OP_VATTR, 24)) { GLfloat v[4] = {x, y, z, w}; memcpy(p, &index, 4); memcpy(p + 8, v, 16); }
    return g.dl.mode == GL_COMPILE;
}

// ------------------------------------------------------------------ geometry capture (compile time)
static bool grow(void **p, uint32_t *cap, size_t need, size_t elem) {
    if (need <= *cap) return true;
    size_t nc = *cap ? *cap : 1024;
    while (nc < need) nc *= 2;
    void *q = realloc(*p, nc * elem);
    if (!q) { set_error(GL_OUT_OF_MEMORY); return false; }
    *p = q; *cap = (uint32_t)nc;
    return true;
}
void dl_capture_imm(GLenum mode, const ImmRec *rec, uint32_t n, uint32_t vary) {
    DList *D = g.dl.cur;
    if (!D) return;
    uint32_t nn = n, nidx; GLenum cls;
    if (!prim_class(mode, nn, cls, nidx)) return;
    ImmLayout L; imm_layout(L, vary, rec, nn);
    DLGeom *G = nullptr;
    if (D->last_geom >= 0) {
        uint32_t gi; memcpy(&gi, D->ops + D->last_geom + sizeof(OpHdr), 4);
        DLGeom *P = D->geoms[gi];
        if (P->mode == cls && !memcmp(&P->lay, &L, sizeof L)) G = P;     // merge with previous run
    }
    if (!G) {
        if (!grow((void **)&D->geoms, &D->gcap, (size_t)D->ngeom + 1, sizeof(DLGeom *))) return;
        G = (DLGeom *)calloc(1, sizeof(DLGeom));
        if (!G) { set_error(GL_OUT_OF_MEMORY); return; }
        G->mode = cls; G->lay = L;
        uint32_t gi = D->ngeom;
        D->geoms[D->ngeom++] = G;
        uint8_t *p = push_op(OP_GEOM, 8);
        if (p) memcpy(p, &gi, 4);
    }
    if (!grow((void **)&G->v, &G->vcap, (size_t)G->vsize + (size_t)nn * L.stride, 1) ||
        !grow((void **)&G->idx, &G->icap, (size_t)G->ni + nidx, 4)) return;
    imm_pack(G->v + G->vsize, L, rec, nn);
    G->ni += imm_indices32(G->idx + G->ni, mode, nn, G->nverts);
    G->vsize += nn * L.stride; G->nverts += nn;
    G->fmask = vary; G->fin = rec[nn - 1];
}

void dl_capture_arrays(GLenum mode, GLint first, GLsizei count, GLenum itype, const void *indices) {
    if (!g.dl.cur || count <= 0 || !(g.ca_on & (1u << CA_VERTEX))) return;
    struct Map { GLuint buf; const uint8_t *p; } maps[CA_COUNT + 1]; int nm = 0;
    GLint prev = 0; es.glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &prev);
    auto mapbuf = [&](GLuint b) -> const uint8_t * {
        for (int i = 0; i < nm; ++i) if (maps[i].buf == b) return maps[i].p;
        GLint64 sz = 0;
        es.glBindBuffer(GL_COPY_READ_BUFFER, b);
        es.glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &sz);
        const uint8_t *p = sz > 0 ? (const uint8_t *)es.glMapBufferRange(GL_COPY_READ_BUFFER, 0, (GLsizeiptr)sz, GL_MAP_READ_BIT) : nullptr;
        maps[nm++] = Map{b, p};
        return p;
    };
    const uint8_t *base[CA_COUNT] = {};
    uint32_t vary = 0;
    for (int c = 0; c < CA_COUNT; ++c) {
        if (!(g.ca_on & (1u << c))) continue;
        if (g.ca[c].buf) base[c] = mapbuf(g.ca[c].buf);
        if (c == CA_COLOR) vary |= AB_COLOR; else if (c == CA_NORMAL) vary |= AB_NORMAL;
        else if (c == CA_COLOR2) vary |= AB_COLOR2; else if (c == CA_FOG) vary |= AB_FOG;
        else if (c >= CA_TEX0) vary |= AB_TEX0 << (c - CA_TEX0);
    }
    const void *isrc = indices;
    if (itype) {
        GLuint ebo = app_element_buffer();
        if (ebo) { const uint8_t *ep = mapbuf(ebo); isrc = ep ? ep + (uintptr_t)indices : nullptr; }
    }
    ImmRec *recs = (ImmRec *)calloc((size_t)count, sizeof(ImmRec));
    if (recs && (!itype || isrc)) {
        for (GLsizei i = 0; i < count; ++i) {
            GLint idx = itype ? (GLint)idx_at(isrc, itype, (size_t)i) : first + i;
            ImmRec &r = recs[i];
            GLfloat v[4];
            if (ca_fetch(CA_VERTEX, idx, v, base[CA_VERTEX])) { r.pos[0] = v[0]; r.pos[1] = v[1]; r.pos[2] = v[2]; r.pos[3] = v[3]; }
            if ((vary & AB_COLOR) && ca_fetch(CA_COLOR, idx, v, base[CA_COLOR])) { memcpy(r.col, v, 16); if (g.ca[CA_COLOR].size == 3) r.col[3] = 1; }
            if ((vary & AB_NORMAL) && ca_fetch(CA_NORMAL, idx, v, base[CA_NORMAL])) memcpy(r.nrm, v, 16);
            if ((vary & AB_COLOR2) && ca_fetch(CA_COLOR2, idx, v, base[CA_COLOR2])) memcpy(r.col2, v, 16);
            if ((vary & AB_FOG) && ca_fetch(CA_FOG, idx, v, base[CA_FOG])) r.fog = v[0];
            for (int u = 0; u < MAX_TEX_UNITS; ++u)
                if ((vary & (AB_TEX0 << u)) && ca_fetch(CA_TEX0 + u, idx, v, base[CA_TEX0 + u])) memcpy(r.tex[u], v, 16);
        }
    }
    for (int i = 0; i < nm; ++i) if (maps[i].p) { es.glBindBuffer(GL_COPY_READ_BUFFER, maps[i].buf); es.glUnmapBuffer(GL_COPY_READ_BUFFER); }
    es.glBindBuffer(GL_COPY_READ_BUFFER, (GLuint)prev);
    if (recs && (!itype || isrc)) dl_capture_imm(mode, recs, (uint32_t)count, vary);
    free(recs);
}

// ------------------------------------------------------------------ finalize (glEndList) and draw
static void geom_finalize(DLGeom &G) {
    if (!G.nverts || !G.ni) return;
    GLenum it = G.nverts <= 65536u ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    uint16_t *i16 = nullptr;
    const void *ib = G.idx;
    if (it == GL_UNSIGNED_SHORT) {
        i16 = (uint16_t *)malloc((size_t)G.ni * 2);
        if (!i16) return;
        for (uint32_t i = 0; i < G.ni; ++i) i16[i] = (uint16_t)G.idx[i];
        ib = i16;
    }
    es.glGenBuffers(1, &G.vbo); es.glGenBuffers(1, &G.ibo);
    es_bind_copy_write(G.vbo); es.glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)G.vsize, G.v, GL_STATIC_DRAW);
    es_bind_copy_write(G.ibo); es.glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)((size_t)G.ni * (it == GL_UNSIGNED_SHORT ? 2 : 4)), ib, GL_STATIC_DRAW);
    es.glGenVertexArrays(1, &G.vao);
    es_bind_vao(G.vao);
    es_bind_array(G.vbo);
    const ImmLayout &L = G.lay;
    uint32_t am = 1u << LOC_POS;
    es.glVertexAttribPointer(LOC_POS, L.pos_n, GL_FLOAT, GL_FALSE, L.stride, (const void *)0);
    if (L.vary & AB_COLOR) { es.glVertexAttribPointer(LOC_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, L.stride, (const void *)(uintptr_t)L.off_col); am |= 1u << LOC_COLOR; }
    if (L.vary & AB_NORMAL) { es.glVertexAttribPointer(LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, L.stride, (const void *)(uintptr_t)L.off_nrm); am |= 1u << LOC_NORMAL; }
    if (L.vary & AB_COLOR2) { es.glVertexAttribPointer(LOC_COLOR2, 4, GL_UNSIGNED_BYTE, GL_TRUE, L.stride, (const void *)(uintptr_t)L.off_col2); am |= 1u << LOC_COLOR2; }
    if (L.vary & AB_FOG) { es.glVertexAttribPointer(LOC_FOG, 1, GL_FLOAT, GL_FALSE, L.stride, (const void *)(uintptr_t)L.off_fog); am |= 1u << LOC_FOG; }
    for (int u = 0; u < MAX_TEX_UNITS; ++u)
        if (L.vary & (AB_TEX0 << u)) {
            es.glVertexAttribPointer((GLuint)(LOC_TEX0 + u), L.tex_n[u], GL_FLOAT, GL_FALSE, L.stride, (const void *)(uintptr_t)L.off_tex[u]);
            am |= 1u << (LOC_TEX0 + u);
        }
    for (uint32_t m = am; m; m &= m - 1) es.glEnableVertexAttribArray((GLuint)__builtin_ctz(m));
    es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, G.ibo);
    G.itype = it; G.nidx = (GLsizei)G.ni; G.amask = am;
    free(G.v); free(G.idx); free(i16);
    G.v = nullptr; G.idx = nullptr; G.vcap = G.icap = 0;
}
static void geom_draw(const DLGeom &G) {
    if (!G.vao) return;
    if (UNLIKELY(g.hooks & HOOK_FB0_CLEAR)) fb0_clear_exec();     // a replayed glClear precedes this geometry
    uint32_t reads = program_prepare(G.mode == GL_POINTS);
    if (!reads) return;
    es_bind_vao(G.vao);
    set_consts(reads & ~G.amask, g.cur.color, g.cur.normal, g.cur.color2, g.cur.fog, g.cur.tex);
    es.glDrawRangeElements(G.mode, 0, G.nverts - 1, G.nidx, G.itype, nullptr);   // indices address the list's vertices
    const ImmRec &r = G.fin; uint32_t v = G.fmask;                  // current := last captured vertex
    if (v & AB_COLOR) g.cur.color = Vec4{r.col[0], r.col[1], r.col[2], r.col[3]};
    if (v & AB_NORMAL) g.cur.normal = Vec4{r.nrm[0], r.nrm[1], r.nrm[2], 0};
    if (v & AB_COLOR2) g.cur.color2 = Vec4{r.col2[0], r.col2[1], r.col2[2], 1};
    if (v & AB_FOG) g.cur.fog = r.fog;
    for (int u = 0; u < MAX_TEX_UNITS; ++u) if (v & (AB_TEX0 << u)) g.cur.tex[u] = Vec4{r.tex[u][0], r.tex[u][1], r.tex[u][2], r.tex[u][3]};
}
static void apply_cur(uint32_t which, const GLfloat *v) {
    if (which == AB_COLOR) g.cur.color = Vec4{v[0], v[1], v[2], v[3]};
    else if (which == AB_NORMAL) g.cur.normal = Vec4{v[0], v[1], v[2], 0};
    else if (which == AB_COLOR2) g.cur.color2 = Vec4{v[0], v[1], v[2], 1};
    else if (which == AB_FOG) g.cur.fog = v[0];
    else for (int u = 0; u < MAX_TEX_UNITS; ++u) if (which == (AB_TEX0 << u)) g.cur.tex[u] = Vec4{v[0], v[1], v[2], v[3]};
}
static void exec_list(GLuint id, int depth);
static void exec_ops(const DList *D, int depth) {
    const uint8_t *p = D->ops, *end = D->ops + D->size;
    while (p < end) {
        OpHdr h; memcpy(&h, p, sizeof h);
        const uint8_t *pl = p + sizeof h;
        switch (h.op) {
        case OP_CALL: { DlThunk fn; memcpy(&fn, pl, sizeof fn); fn(pl + 8); break; }
        case OP_GEOM: { uint32_t gi; memcpy(&gi, pl, 4); geom_draw(*D->geoms[gi]); break; }
        case OP_CUR: { uint32_t w; GLfloat v[4]; memcpy(&w, pl, 4); memcpy(v, pl + 8, 16); apply_cur(w, v); break; }
        case OP_VATTR: { GLuint i; GLfloat v[4]; memcpy(&i, pl, 4); memcpy(v, pl + 8, 16); vattrib4f(i, v[0], v[1], v[2], v[3]); break; }
        case OP_CALLLIST: { GLuint id; memcpy(&id, pl, 4); exec_list(id, depth + 1); break; }
        case OP_CALLLISTS: {
            uint32_t n; memcpy(&n, pl, 4);
            for (uint32_t i = 0; i < n; ++i) { uint32_t o; memcpy(&o, pl + 8 + 4 * i, 4); exec_list(g.dl.base + o, depth + 1); }
            break;
        }
        case OP_LISTBASE: { GLuint b; memcpy(&b, pl, 4); g.dl.base = b; break; }
        default: return;
        }
        p = pl + h.size;
    }
}
static void exec_list(GLuint id, int depth) {
    if (depth > 64) return;
    const DList *D = get_list(id);
    if (D && D->size) exec_ops(D, depth);
}
static void run_list(GLuint id) {
    GLenum m = g.dl.mode; DList *c = g.dl.cur;
    g.dl.mode = 0; g.dl.cur = nullptr;              // commands replayed from a list must not re-record
    exec_list(id, 0);
    g.dl.mode = m; g.dl.cur = c;
}
static uint32_t list_offset(GLenum type, const void *lists, GLsizei i) {
    const uint8_t *b = (const uint8_t *)lists;
    switch (type) {
    case GL_BYTE: return (uint32_t)(int32_t)((const GLbyte *)lists)[i];
    case GL_UNSIGNED_BYTE: return b[i];
    case GL_SHORT: return (uint32_t)(int32_t)((const GLshort *)lists)[i];
    case GL_UNSIGNED_SHORT: return ((const GLushort *)lists)[i];
    case GL_INT: return (uint32_t)((const GLint *)lists)[i];
    case GL_UNSIGNED_INT: return ((const GLuint *)lists)[i];
    case GL_FLOAT: return (uint32_t)((const GLfloat *)lists)[i];
    case GL_2_BYTES: return (uint32_t)b[2 * i] * 256u + b[2 * i + 1];
    case GL_3_BYTES: return ((uint32_t)b[3 * i] << 16) | ((uint32_t)b[3 * i + 1] << 8) | b[3 * i + 2];
    case GL_4_BYTES: return ((uint32_t)b[4 * i] << 24) | ((uint32_t)b[4 * i + 1] << 16) | ((uint32_t)b[4 * i + 2] << 8) | b[4 * i + 3];
    default: return 0;
    }
}

// ------------------------------------------------------------------ recorders for pointer-argument commands
namespace {
int n_light(GLenum p) { return (p == GL_AMBIENT || p == GL_DIFFUSE || p == GL_SPECULAR || p == GL_POSITION) ? 4 : p == GL_SPOT_DIRECTION ? 3 : 1; }
int n_lmodel(GLenum p) { return p == GL_LIGHT_MODEL_AMBIENT ? 4 : 1; }
int n_mat(GLenum p) { return p == GL_SHININESS ? 1 : p == GL_COLOR_INDEXES ? 3 : 4; }
int n_fog(GLenum p) { return p == GL_FOG_COLOR ? 4 : 1; }
int n_env(GLenum p) { return p == GL_TEXTURE_ENV_COLOR ? 4 : 1; }
int n_gen(GLenum p) { return p == GL_TEXTURE_GEN_MODE ? 1 : 4; }
int n_tp(GLenum p) { return (p == GL_TEXTURE_BORDER_COLOR || p == GL_TEXTURE_SWIZZLE_RGBA) ? 4 : 1; }
template <typename T> struct A2 { GLenum a, b; T v[4]; };
template <typename T> struct A1 { GLenum a; T v[4]; };
template <typename T> struct AM { T m[16]; };
} // namespace

#define DL_REC2(NAME, T, CNT) \
    static void dlx_##NAME(const void *p) { const A2<T> *a = (const A2<T> *)p; ::NAME(a->a, a->b, a->v); } \
    bool dlr_##NAME(GLenum a, GLenum b, const T *v) { \
        A2<T> s; memset(&s, 0, sizeof s); s.a = a; s.b = b; memcpy(s.v, v, sizeof(T) * (size_t)CNT(b)); \
        dl_push_call(dlx_##NAME, &s, sizeof s); return g.dl.mode == GL_COMPILE; }
#define DL_REC1(NAME, T, CNT) \
    static void dlx_##NAME(const void *p) { const A1<T> *a = (const A1<T> *)p; ::NAME(a->a, a->v); } \
    bool dlr_##NAME(GLenum a, const T *v) { \
        A1<T> s; memset(&s, 0, sizeof s); s.a = a; memcpy(s.v, v, sizeof(T) * (size_t)CNT(a)); \
        dl_push_call(dlx_##NAME, &s, sizeof s); return g.dl.mode == GL_COMPILE; }
#define DL_RECM(NAME, T) \
    static void dlx_##NAME(const void *p) { const AM<T> *a = (const AM<T> *)p; ::NAME(a->m); } \
    bool dlr_##NAME(const T *m) { AM<T> s; memcpy(s.m, m, sizeof s.m); dl_push_call(dlx_##NAME, &s, sizeof s); return g.dl.mode == GL_COMPILE; }

DL_REC2(glLightfv, GLfloat, n_light)
DL_REC2(glLightiv, GLint, n_light)
DL_REC1(glLightModelfv, GLfloat, n_lmodel)
DL_REC1(glLightModeliv, GLint, n_lmodel)
DL_REC2(glMaterialfv, GLfloat, n_mat)
DL_REC2(glMaterialiv, GLint, n_mat)
DL_REC1(glFogfv, GLfloat, n_fog)
DL_REC1(glFogiv, GLint, n_fog)
DL_REC2(glTexEnvfv, GLfloat, n_env)
DL_REC2(glTexEnviv, GLint, n_env)
DL_REC2(glTexGenfv, GLfloat, n_gen)
DL_REC2(glTexGeniv, GLint, n_gen)
DL_REC2(glTexGendv, GLdouble, n_gen)
DL_REC2(glTexParameterfv, GLfloat, n_tp)
DL_REC2(glTexParameteriv, GLint, n_tp)
DL_RECM(glLoadMatrixf, GLfloat)
DL_RECM(glLoadMatrixd, GLdouble)
DL_RECM(glMultMatrixf, GLfloat)
DL_RECM(glMultMatrixd, GLdouble)
DL_RECM(glLoadTransposeMatrixf, GLfloat)
DL_RECM(glLoadTransposeMatrixd, GLdouble)
DL_RECM(glMultTransposeMatrixf, GLfloat)
DL_RECM(glMultTransposeMatrixd, GLdouble)
static void dlx_glClipPlane(const void *p) { const A1<GLdouble> *a = (const A1<GLdouble> *)p; ::glClipPlane(a->a, a->v); }
bool dlr_glClipPlane(GLenum plane, const GLdouble *eq) {
    A1<GLdouble> s; s.a = plane; memcpy(s.v, eq, sizeof s.v);
    dl_push_call(dlx_glClipPlane, &s, sizeof s); return g.dl.mode == GL_COMPILE;
}
} // namespace ory

using namespace ory;

/* jar: GL11.glGenLists(I)I */
OGL_EXPORT GLuint glGenLists(GLsizei range) {
    ORY_PROLOGUE();
    if (range < 0) { set_error(GL_INVALID_VALUE); return 0; }
    if (range == 0) return 0;
    GLuint first = g_next;
    for (;;) {                                     // first fit from the hint
        if (!ensure_cap(first + (GLuint)range)) { set_error(GL_OUT_OF_MEMORY); return 0; }
        GLsizei k = 0;
        while (k < range && !g_lists[first + (GLuint)k]) ++k;
        if (k == range) break;
        first += (GLuint)k + 1;
    }
    for (GLsizei i = 0; i < range; ++i) g_lists[first + (GLuint)i] = list_new();
    g_next = first + (GLuint)range;
    return first;
}
/* jar: GL11.glDeleteLists(II)V */
OGL_EXPORT void glDeleteLists(GLuint list, GLsizei range) {
    ORY_PROLOGUE();
    if (range < 0) { set_error(GL_INVALID_VALUE); return; }
    for (GLsizei i = 0; i < range; ++i) {
        GLuint id = list + (GLuint)i;
        if (id && id < g_cap && g_lists[id]) { list_free(g_lists[id]); g_lists[id] = nullptr; if (id < g_next) g_next = id; }
    }
}
/* jar: GL11.glIsList(I)Z */
OGL_EXPORT GLboolean glIsList(GLuint list) { ORY_PROLOGUE(); return get_list(list) ? GL_TRUE : GL_FALSE; }
/* jar: GL11.glNewList(II)V */
OGL_EXPORT void glNewList(GLuint list, GLenum mode) {
    ORY_PROLOGUE();
    if (!list) { set_error(GL_INVALID_VALUE); return; }
    if (mode != GL_COMPILE && mode != GL_COMPILE_AND_EXECUTE) { set_error(GL_INVALID_ENUM); return; }
    if (g.dl.mode || g.imm.inside) { set_error(GL_INVALID_OPERATION); return; }
    g.dl.cur = list_new();
    if (!g.dl.cur) { set_error(GL_OUT_OF_MEMORY); return; }
    g.dl.index = list; g.dl.mode = mode;
}
/* jar: GL11.glEndList()V */
OGL_EXPORT void glEndList(void) {
    ORY_PROLOGUE();
    if (!g.dl.mode) { set_error(GL_INVALID_OPERATION); return; }
    StatTimer st_(g.st.dl_n, g.st.dl_us);
    DList *D = g.dl.cur;
    for (uint32_t i = 0; i < D->ngeom; ++i) geom_finalize(*D->geoms[i]);
    GLuint id = g.dl.index;
    g.dl.mode = 0; g.dl.cur = nullptr; g.dl.index = 0;
    if (!ensure_cap(id)) { list_free(D); set_error(GL_OUT_OF_MEMORY); return; }
    list_free(g_lists[id]);
    g_lists[id] = D;
}
/* jar: GL11.glCallList(I)V */
OGL_EXPORT void glCallList(GLuint list) {
    ORY_PROLOGUE();
    if (g.dl.mode) {
        if (uint8_t *p = push_op(OP_CALLLIST, 8)) memcpy(p, &list, 4);
        if (g.dl.mode == GL_COMPILE) return;
    }
    run_list(list);
}
/* jar: GL11.nglCallLists(IIJ)V */
OGL_EXPORT void glCallLists(GLsizei n, GLenum type, const void *lists) {
    ORY_PROLOGUE();
    if (n < 0) { set_error(GL_INVALID_VALUE); return; }
    if (g.dl.mode) {
        if (uint8_t *p = push_op(OP_CALLLISTS, (uint32_t)(8 + 4 * (size_t)n))) {
            uint32_t nn = (uint32_t)n; memcpy(p, &nn, 4);
            for (GLsizei i = 0; i < n; ++i) { uint32_t o = list_offset(type, lists, i); memcpy(p + 8 + 4 * i, &o, 4); }
        }
        if (g.dl.mode == GL_COMPILE) return;
    }
    GLenum m = g.dl.mode; DList *c = g.dl.cur;
    g.dl.mode = 0; g.dl.cur = nullptr;
    for (GLsizei i = 0; i < n; ++i) exec_list(g.dl.base + list_offset(type, lists, i), 0);
    g.dl.mode = m; g.dl.cur = c;
}
/* jar: GL11.glListBase(I)V */
OGL_EXPORT void glListBase(GLuint base) {
    ORY_PROLOGUE();
    if (g.dl.mode) {
        if (uint8_t *p = push_op(OP_LISTBASE, 8)) memcpy(p, &base, 4);
        if (g.dl.mode == GL_COMPILE) return;
    }
    g.dl.base = base;
}
