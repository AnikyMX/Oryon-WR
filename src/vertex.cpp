// Oryon -- vertex pipeline: streaming rings, quad IBO, VAO/pointer caches, immediate mode (+batching),
// legacy client arrays, draw calls, buffer/VAO binding shadow.
#include "oryon.hpp"
#include "imm.hpp"
#include "mathx.hpp"
#include "bind.hpp"
#include "ffp_prog.hpp"
#include "vertex_int.hpp"

namespace ory {
uint32_t app_program_prepare();          // shader.cpp

static inline GLsizei type_size(GLenum t) {
    switch (t) {
    case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
    case GL_SHORT: case GL_UNSIGNED_SHORT: case GL_HALF_FLOAT: return 2;
    case GL_DOUBLE: return 8;
    default: return 4;
    }
}
static inline GLuint ca_loc(int c) {
    static const GLuint k[CA_TEX0] = {LOC_POS, LOC_NORMAL, LOC_COLOR, LOC_COLOR2, LOC_FOG};
    return c < CA_TEX0 ? k[c] : (GLuint)(LOC_TEX0 + (c - CA_TEX0));
}
static inline bool is_float_type(GLenum t) { return t == GL_FLOAT || t == GL_DOUBLE || t == GL_HALF_FLOAT || t == GL_FIXED; }

// ------------------------------------------------------------------ streaming rings
static void ring_create(Ring &r, size_t size) {
    memset(&r, 0, sizeof r);
    es.glGenBuffers(1, &r.buf);
    es_bind_copy_write(r.buf);
    r.size = size; r.seg = size / 4;
    if ((g.escaps & ORY_ESCAP_BUFFER_STORAGE) && es.glBufferStorageEXT && !env_on("ORYON_NO_BUFFER_STORAGE")) {
        const GLbitfield fl = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT;
        es.glBufferStorageEXT(GL_COPY_WRITE_BUFFER, (GLsizeiptr)size, nullptr, fl);
        r.map = (uint8_t *)es.glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)size, fl);
        r.persistent = r.map != nullptr;
        if (!r.persistent) {                     // immutable storage without mapping: start over
            es.glDeleteBuffers(1, &r.buf); g.b.es_copy_write = 0;
            es.glGenBuffers(1, &r.buf); es_bind_copy_write(r.buf);
        }
    }
    if (!r.persistent) es.glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)size, nullptr, GL_STREAM_DRAW);
}
static __attribute__((noinline, cold)) void seg_wait(Ring &r, int s) {
    if (GLsync f = r.fence[s]) {
        const uint64_t t0 = now_us();
        if (es.glClientWaitSync(f, GL_SYNC_FLUSH_COMMANDS_BIT, (GLuint64)2000000000ull) != GL_ALREADY_SIGNALED) {
            ++g.st.wait_n; g.st.wait_us += now_us() - t0;    // the CPU actually waited for the GPU
        }
        es.glDeleteSync(f); r.fence[s] = nullptr;
    }
}
static void seg_fence(Ring &r, int s) {
    if (r.fence[s]) es.glDeleteSync(r.fence[s]);
    r.fence[s] = es.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}
// Reserve 'bytes' (<= one segment). Returns a CPU pointer, or nullptr for oversize requests.
static uint8_t *ring_alloc(Ring &r, size_t bytes, size_t align, size_t *off) {
    if (!bytes || bytes > r.seg) return nullptr;
    size_t o = (r.head + align - 1) / align * align;
    bool wrapped = false;
    if (o + bytes > r.size) { o = 0; wrapped = true; ++r.wraps; }
    if (r.persistent) {
        int s0 = (int)(o / r.seg), s1 = (int)((o + bytes - 1) / r.seg);
        if (r.open_valid) {
            if (wrapped) {
                for (int s = r.open_first; s <= r.open_last; ++s) seg_fence(r, s);
                r.open_valid = false;
            } else if (s0 > r.open_first) {
                int last = s0 - 1 < r.open_last ? s0 - 1 : r.open_last;
                for (int s = r.open_first; s <= last; ++s) seg_fence(r, s);
                if (s0 > r.open_last) r.open_valid = false; else r.open_first = s0;
            }
        }
        for (int s = s0; s <= s1; ++s)
            if (!(r.open_valid && s >= r.open_first && s <= r.open_last)) seg_wait(r, s);
        if (!r.open_valid) { r.open_first = s0; r.open_last = s1; r.open_valid = true; }
        else if (s1 > r.open_last) r.open_last = s1;
        r.head = o + bytes; *off = o;
        return r.map + o;
    }
    es_bind_copy_write(r.buf);
    if (wrapped) es.glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)r.size, nullptr, GL_STREAM_DRAW);
    void *p = es.glMapBufferRange(GL_COPY_WRITE_BUFFER, (GLintptr)o, (GLsizeiptr)bytes,
                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if (!p) return nullptr;
    r.head = o + bytes; *off = o;
    return (uint8_t *)p;
}
static inline void ring_commit(Ring &r) {
    if (!r.persistent) { es_bind_copy_write(r.buf); es.glUnmapBuffer(GL_COPY_WRITE_BUFFER); }
}
static __attribute__((noinline, cold)) void ring_upload_big(Ring &r, const void *src, size_t bytes, GLuint *buf, size_t *off) {
    if (!r.big) es.glGenBuffers(1, &r.big);
    es_bind_copy_write(r.big);
    es.glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)bytes, src, GL_STREAM_DRAW);
    *buf = r.big; *off = 0; g.st.stream += bytes;
}
static void ring_upload(Ring &r, const void *src, size_t bytes, size_t align, GLuint *buf, size_t *off) {
    if (uint8_t *p = ring_alloc(r, bytes, align, off)) {
        memcpy(p, src, bytes); ring_commit(r); *buf = r.buf; return;
    }
    ring_upload_big(r, src, bytes, buf, off);
}

// ------------------------------------------------------------------ static quad index buffer (0,1,2, 0,2,3)
static bool quad_ibo_ensure(GLsizei verts) {
    if (g.quad_ibo && g.quad_ibo_verts >= verts) return true;
    GLsizei nv = g.quad_ibo_verts ? g.quad_ibo_verts : 16384;
    while (nv < verts) nv *= 2;
    GLenum type = nv <= 65536 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    size_t nq = (size_t)nv / 4, isz = type == GL_UNSIGNED_SHORT ? 2 : 4;
    void *d = malloc(nq * 6 * isz);
    if (!d) return false;
    for (size_t q = 0; q < nq; ++q) {
        uint32_t b = (uint32_t)(q * 4), v[6] = {b, b + 1, b + 2, b, b + 2, b + 3};
        for (int i = 0; i < 6; ++i) {
            if (type == GL_UNSIGNED_SHORT) ((uint16_t *)d)[q * 6 + i] = (uint16_t)v[i];
            else ((uint32_t *)d)[q * 6 + i] = v[i];
        }
    }
    if (!g.quad_ibo) es.glGenBuffers(1, &g.quad_ibo);
    es_bind_copy_write(g.quad_ibo);
    es.glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(nq * 6 * isz), d, GL_STATIC_DRAW);
    free(d);
    g.quad_ibo_verts = nv; g.quad_ibo_type = type;
    return true;
}

// ------------------------------------------------------------------ VAO / attribute caches
static void vao_create(Vao &v) {
    memset(&v, 0, sizeof v);
    es.glGenVertexArrays(1, &v.id);
    for (GLuint i = 0; i < LOC_COUNT; ++i) v.a[i].binding = i;   // ES default: attribute i reads binding i
}
static inline void set_attr(Vao &v, GLuint loc, GLuint buf, GLint size, GLenum type, GLboolean norm, GLsizei stride, uintptr_t off) {
    AttrCache &a = v.a[loc];
    if (a.buf != buf || a.size != size || a.type != type || a.norm != norm || a.stride != stride || a.off != off) {
        es_bind_array(buf);
        es.glVertexAttribPointer(loc, size, type, norm, stride, (const void *)off);
        a.buf = buf; a.size = size; a.type = type; a.norm = norm; a.stride = stride; a.off = off;
    }
}
// ES 3.1 vertex attribute binding: the format of each location (size/type/normalized/relative offset) and its binding
// slot are cached apart from the buffer bound to the slot. Attributes that share a buffer and stride share one slot, so
// switching between VBOs with the same layout (Minecraft chunk sections) costs one glBindVertexBuffer instead of a
// glBindBuffer plus one glVertexAttribPointer per attribute. Without ES 3.1 the pointer path above is used.
struct AttrSrc { GLuint loc; GLint size; GLenum type; GLboolean norm; uintptr_t off; };
static inline void set_fmt(Vao &v, GLuint loc, GLint size, GLenum type, GLboolean norm, GLuint rel, GLuint bi) {
    AttrCache &a = v.a[loc];
    if (a.size != size || a.type != type || a.norm != norm || a.off != rel) {
        es.glVertexAttribFormat(loc, size, type, norm, rel);
        a.size = size; a.type = type; a.norm = norm; a.off = rel;
    }
    if (a.binding != bi) { es.glVertexAttribBinding(loc, bi); a.binding = bi; }
}
static inline void set_vbuf(Vao &v, GLuint bi, GLuint buf, uintptr_t off, GLsizei stride) {
    VBind &b = v.b[bi];
    if (b.buf != buf || b.off != off || b.stride != stride) {
        es.glBindVertexBuffer(bi, buf, (GLintptr)off, stride);
        b.buf = buf; b.off = off; b.stride = stride;
    }
}
// Points n attributes at 'buf' with a common stride (0 = each attribute tightly packed on its own).
static void set_attrs(Vao &V, GLuint buf, GLsizei stride, const AttrSrc *a, int n) {
    if (!g.vbind) {
        for (int i = 0; i < n; ++i) set_attr(V, a[i].loc, buf, a[i].size, a[i].type, a[i].norm, stride, a[i].off);
        return;
    }
    if (stride > 0 && n > 0) {                   // one slot for the group: binding index = lowest location
        uintptr_t base = a[0].off, top = a[0].off; GLuint bi = a[0].loc;
        for (int i = 1; i < n; ++i) {
            if (a[i].off < base) base = a[i].off;
            if (a[i].off > top) top = a[i].off;
            if (a[i].loc < bi) bi = a[i].loc;
        }
        if (top - base <= (uintptr_t)g.max_reloff) {
            for (int i = 0; i < n; ++i) set_fmt(V, a[i].loc, a[i].size, a[i].type, a[i].norm, (GLuint)(a[i].off - base), bi);
            set_vbuf(V, bi, buf, base, stride);
            return;
        }
    }
    for (int i = 0; i < n; ++i) {                // own slot per attribute (binding index = location)
        const GLsizei st = stride ? stride : a[i].size * type_size(a[i].type);
        set_fmt(V, a[i].loc, a[i].size, a[i].type, a[i].norm, 0, a[i].loc);
        set_vbuf(V, a[i].loc, buf, a[i].off, st);
    }
}
static inline void set_enabled(Vao &v, uint32_t mask) {
    uint32_t d = v.enabled ^ mask;
    while (d) {
        int i = __builtin_ctz(d); d &= d - 1;
        if (mask & (1u << i)) es.glEnableVertexAttribArray((GLuint)i); else es.glDisableVertexAttribArray((GLuint)i);
    }
    v.enabled = mask;
}
static inline void set_const(GLuint loc, const Vec4 &v) {
    if (!(g.es_const_valid & (1u << loc)) || memcmp(&g.es_const[loc], &v, sizeof v)) {
        es.glVertexAttrib4f(loc, v.x, v.y, v.z, v.w);
        g.es_const[loc] = v; g.es_const_valid |= 1u << loc;
    }
}
void set_consts(uint32_t need, const Vec4 &col, const Vec4 &nrm, const Vec4 &col2, GLfloat fog, const Vec4 *tex) {
    if (need & (1u << LOC_COLOR)) set_const(LOC_COLOR, col);
    if (need & (1u << LOC_NORMAL)) set_const(LOC_NORMAL, nrm);
    if (need & (1u << LOC_COLOR2)) set_const(LOC_COLOR2, col2);
    if (need & (1u << LOC_FOG)) set_const(LOC_FOG, Vec4{fog, 0, 0, 1});
    uint32_t t = (need >> LOC_TEX0) & 0xFFu;
    for (int u = 0; t; ++u, t >>= 1) if (t & 1u) set_const((GLuint)(LOC_TEX0 + u), tex[u]);
}
static void invalidate_buffer_refs(GLuint id) {
    Vao *vs[2] = {&g.vao_client, &g.vao_imm};
    for (Vao *v : vs) {
        for (AttrCache &a : v->a) if (a.buf == id) a.size = 0;
        for (VBind &b : v->b) if (b.buf == id) b.stride = -1;   // a new buffer may reuse the name: rebind
    }
}

void vertex_init() {
    GLint maj = 0, mnr = 0;
    es.glGetIntegerv(GL_MAJOR_VERSION, &maj); es.glGetIntegerv(GL_MINOR_VERSION, &mnr);
    g.vbind = (maj > 3 || (maj == 3 && mnr >= 1)) && es.glBindVertexBuffer && es.glVertexAttribFormat && es.glVertexAttribBinding &&
              !env_on("ORYON_NO_VERTEX_BINDING");
    if (g.vbind) { GLint r = 0; es.glGetIntegerv(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET, &r); if (r > 0) g.max_reloff = r; }
    ring_create(g.rv, (size_t)8 << 20);
    ring_create(g.ri, (size_t)2 << 20);
    vao_create(g.vao_client);
    vao_create(g.vao_imm);
    quad_ibo_ensure(16384);
}

uint32_t program_prepare(bool points) {
    if (g.b.app_program) return app_program_prepare();
    Program *P = ffp_prepare(points);
    return P ? P->attr_mask : 0;
}

// ------------------------------------------------------------------ immediate mode
bool imm_grow() {
    Imm &I = g.imm;
    uint32_t nc = I.cap ? I.cap * 2 : 1024;
    ImmRec *r = (ImmRec *)realloc(I.rec, (size_t)nc * sizeof(ImmRec));
    if (!r) { set_error(GL_OUT_OF_MEMORY); return false; }
    I.rec = r; I.cap = nc;
    return true;
}
void imm_vary(uint32_t bit) {                   // attribute becomes per-vertex: backfill earlier vertices
    Imm &I = g.imm;
    for (uint32_t i = 0; i < I.n; ++i) imm_fill(I.rec[i], bit);
    I.vary |= bit;
}
void imm_layout(ImmLayout &L, uint32_t vary, const ImmRec *rec, uint32_t n) {
    memset(&L, 0, sizeof L);
    L.vary = vary;
    bool w4 = false;
    for (uint32_t i = 0; i < n && !w4; ++i) w4 = rec[i].pos[3] != 1.0f;
    L.pos_n = w4 ? 4 : 3;
    uint16_t off = (uint16_t)(L.pos_n * 4);
    if (vary & AB_COLOR) { L.off_col = off; off += 4; }
    if (vary & AB_NORMAL) { L.off_nrm = off; off += 12; }
    if (vary & AB_COLOR2) { L.off_col2 = off; off += 4; }
    if (vary & AB_FOG) { L.off_fog = off; off += 4; }
    for (int u = 0; u < MAX_TEX_UNITS; ++u) {
        if (!(vary & (AB_TEX0 << u))) continue;
        bool t4 = false;
        for (uint32_t i = 0; i < n && !t4; ++i) t4 = rec[i].tex[u][2] != 0.0f || rec[i].tex[u][3] != 1.0f;
        L.tex_n[u] = t4 ? 4 : 2; L.off_tex[u] = off; off = (uint16_t)(off + L.tex_n[u] * 4);
    }
    L.stride = off;
}
static inline uint8_t to_u8(GLfloat f) { f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f; return (uint8_t)(f * 255.0f + 0.5f); }
void imm_pack(uint8_t *dst, const ImmLayout &L, const ImmRec *rec, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i, dst += L.stride) {
        const ImmRec &r = rec[i];
        memcpy(dst, r.pos, (size_t)L.pos_n * 4);
        if (L.vary & AB_COLOR) { uint8_t *c = dst + L.off_col; c[0] = to_u8(r.col[0]); c[1] = to_u8(r.col[1]); c[2] = to_u8(r.col[2]); c[3] = to_u8(r.col[3]); }
        if (L.vary & AB_NORMAL) memcpy(dst + L.off_nrm, r.nrm, 12);
        if (L.vary & AB_COLOR2) { uint8_t *c = dst + L.off_col2; c[0] = to_u8(r.col2[0]); c[1] = to_u8(r.col2[1]); c[2] = to_u8(r.col2[2]); c[3] = 255; }
        if (L.vary & AB_FOG) memcpy(dst + L.off_fog, &r.fog, 4);
        uint32_t t = L.vary >> 4;
        for (int u = 0; t; ++u, t >>= 1) if (t & 1u) memcpy(dst + L.off_tex[u], r.tex[u], (size_t)L.tex_n[u] * 4);
    }
}
template <typename T>
static uint32_t imm_indices(T *d, GLenum mode, uint32_t n, uint32_t b) {
    uint32_t k = 0;
    switch (mode) {
    case GL_POINTS: case GL_LINES: case GL_TRIANGLES:
        for (uint32_t i = 0; i < n; ++i) d[k++] = (T)(b + i);
        break;
    case GL_LINE_STRIP: case GL_LINE_LOOP:
        for (uint32_t i = 0; i + 1 < n; ++i) { d[k++] = (T)(b + i); d[k++] = (T)(b + i + 1); }
        if (mode == GL_LINE_LOOP) { d[k++] = (T)(b + n - 1); d[k++] = (T)b; }
        break;
    case GL_TRIANGLE_STRIP:
        for (uint32_t i = 0; i + 2 < n; ++i) {
            if (i & 1u) { d[k++] = (T)(b + i + 1); d[k++] = (T)(b + i); } else { d[k++] = (T)(b + i); d[k++] = (T)(b + i + 1); }
            d[k++] = (T)(b + i + 2);
        }
        break;
    case GL_TRIANGLE_FAN: case GL_POLYGON:
        for (uint32_t i = 1; i + 1 < n; ++i) { d[k++] = (T)b; d[k++] = (T)(b + i); d[k++] = (T)(b + i + 1); }
        break;
    case GL_QUADS:
        for (uint32_t q = 0; q + 3 < n; q += 4) {
            d[k++] = (T)(b + q); d[k++] = (T)(b + q + 1); d[k++] = (T)(b + q + 2);
            d[k++] = (T)(b + q); d[k++] = (T)(b + q + 2); d[k++] = (T)(b + q + 3);
        }
        break;
    case GL_QUAD_STRIP:
        for (uint32_t i = 0; i + 3 < n; i += 2) {
            d[k++] = (T)(b + i); d[k++] = (T)(b + i + 1); d[k++] = (T)(b + i + 3);
            d[k++] = (T)(b + i); d[k++] = (T)(b + i + 3); d[k++] = (T)(b + i + 2);
        }
        break;
    }
    return k;
}
uint32_t imm_indices32(uint32_t *d, GLenum mode, uint32_t n, uint32_t base) { return imm_indices<uint32_t>(d, mode, n, base); }

bool prim_class(GLenum mode, uint32_t &n, GLenum &cls, uint32_t &nidx) {
    switch (mode) {
    case GL_POINTS: cls = GL_POINTS; nidx = n; break;
    case GL_LINES: n &= ~1u; cls = GL_LINES; nidx = n; break;
    case GL_LINE_STRIP: if (n < 2) return false; cls = GL_LINES; nidx = (n - 1) * 2; break;
    case GL_LINE_LOOP: if (n < 2) return false; cls = GL_LINES; nidx = n * 2; break;
    case GL_TRIANGLES: n -= n % 3; cls = GL_TRIANGLES; nidx = n; break;
    case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN: case GL_POLYGON:
        if (n < 3) return false; cls = GL_TRIANGLES; nidx = (n - 2) * 3; break;
    case GL_QUADS: n &= ~3u; cls = GL_TRIANGLES; nidx = n / 4 * 6; break;
    case GL_QUAD_STRIP: n &= ~1u; if (n < 4) return false; cls = GL_TRIANGLES; nidx = (n / 2 - 1) * 6; break;
    default: return false;
    }
    return n && nidx;
}
static void imm_draw(const ImmLayout &L, const ImmConst &C, GLenum mode, const void *vd, size_t vbytes,
                     const void *id, size_t ibytes, GLenum itype, GLsizei nidx) {
    uint32_t reads = program_prepare(mode == GL_POINTS);
    if (!reads) return;
    GLuint vb, ib; size_t voff, ioff;
    size_t align = (L.stride % 4) ? (size_t)L.stride * 4 : L.stride;
    ring_upload(g.rv, vd, vbytes, align, &vb, &voff);
    ring_upload(g.ri, id, ibytes, 4, &ib, &ioff);
    Vao &V = g.vao_imm;
    es_bind_vao(V.id);
    if (V.element != ib) { es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib); V.element = ib; }
    AttrSrc as[LOC_COUNT]; int na = 0;
    as[na++] = AttrSrc{LOC_POS, L.pos_n, GL_FLOAT, GL_FALSE, 0};
    if (L.vary & AB_COLOR) as[na++] = AttrSrc{LOC_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, L.off_col};
    if (L.vary & AB_NORMAL) as[na++] = AttrSrc{LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, L.off_nrm};
    if (L.vary & AB_COLOR2) as[na++] = AttrSrc{LOC_COLOR2, 4, GL_UNSIGNED_BYTE, GL_TRUE, L.off_col2};
    if (L.vary & AB_FOG) as[na++] = AttrSrc{LOC_FOG, 1, GL_FLOAT, GL_FALSE, L.off_fog};
    for (int u = 0; u < MAX_TEX_UNITS; ++u)
        if (L.vary & (AB_TEX0 << u)) as[na++] = AttrSrc{(GLuint)(LOC_TEX0 + u), L.tex_n[u], GL_FLOAT, GL_FALSE, L.off_tex[u]};
    set_attrs(V, vb, L.stride, as, na);
    uint32_t am = 0;
    for (int i = 0; i < na; ++i) am |= 1u << as[i].loc;
    set_enabled(V, am);
    set_consts(reads & ~am, C.col, C.nrm, C.col2, C.fog, C.tex);
    es.glDrawElementsBaseVertex(mode, nidx, itype, (const void *)ioff, (GLint)(voff / L.stride));
}
void imm_flush() {
    Imm &I = g.imm;
    g.hooks &= ~HOOK_FLUSH;
    if (!I.pending) return;
    I.pending = false;
    imm_draw(I.blay, I.bconst, I.bmode, I.bv, I.bv_size, I.bi, (size_t)I.bi_n * 2, GL_UNSIGNED_SHORT, (GLsizei)I.bi_n);
    I.bv_size = 0; I.bi_n = 0; I.bverts = 0;
}
static bool grow(void **p, uint32_t *cap, size_t need, size_t elem) {
    if (need <= *cap) return true;
    size_t nc = *cap ? *cap : 4096;
    while (nc < need) nc *= 2;
    void *q = realloc(*p, nc * elem);
    if (!q) { set_error(GL_OUT_OF_MEMORY); return false; }
    *p = q; *cap = (uint32_t)nc;
    return true;
}
static void imm_submit() {
    Imm &I = g.imm;
    uint32_t n = I.n, nidx; GLenum cls;
    if (!prim_class(I.mode, n, cls, nidx)) return;
    ImmLayout L; imm_layout(L, I.vary, I.rec, n);
    ImmConst C;
    memset(&C, 0, sizeof C);
    if (!(I.vary & AB_COLOR)) C.col = g.cur.color;
    if (!(I.vary & AB_NORMAL)) C.nrm = g.cur.normal;
    if (!(I.vary & AB_COLOR2)) C.col2 = g.cur.color2;
    if (!(I.vary & AB_FOG)) C.fog = g.cur.fog;
    for (int u = 0; u < MAX_TEX_UNITS; ++u) if (!(I.vary & (AB_TEX0 << u))) C.tex[u] = g.cur.tex[u];
    if (n > 65535u) {                            // oversize primitive: 32-bit indices, drawn alone
        if (I.pending) imm_flush();
        uint8_t *vd = (uint8_t *)malloc((size_t)n * L.stride);
        uint32_t *id = (uint32_t *)malloc((size_t)nidx * 4);
        if (vd && id) {
            imm_pack(vd, L, I.rec, n);
            uint32_t k = imm_indices<uint32_t>(id, I.mode, n, 0);
            imm_draw(L, C, cls, vd, (size_t)n * L.stride, id, (size_t)k * 4, GL_UNSIGNED_INT, (GLsizei)k);
        } else set_error(GL_OUT_OF_MEMORY);
        free(vd); free(id);
        return;
    }
    if (I.pending && (I.bmode != cls || memcmp(&I.blay, &L, sizeof L) || memcmp(&I.bconst, &C, sizeof C) || I.bverts + n > 65535u))
        imm_flush();
    if (!I.pending) { I.blay = L; I.bconst = C; I.bmode = cls; I.bv_size = 0; I.bverts = 0; I.bi_n = 0; }
    if (!grow((void **)&I.bv, &I.bv_cap, (size_t)I.bv_size + (size_t)n * L.stride, 1) ||
        !grow((void **)&I.bi, &I.bi_cap, (size_t)I.bi_n + nidx, 2)) return;
    imm_pack(I.bv + I.bv_size, L, I.rec, n);
    uint32_t k = imm_indices<uint16_t>(I.bi + I.bi_n, I.mode, n, I.bverts);
    I.bv_size += n * L.stride; I.bverts += n; I.bi_n += k;
    I.pending = true;
    g.hooks |= HOOK_FLUSH;
}
static void imm_begin(GLenum mode) {
    Imm &I = g.imm;
    if (UNLIKELY(g.dl.mode == GL_COMPILE)) g.dl.saved = g.cur;    // GL_COMPILE must not change current state
    I.inside = true; I.mode = mode; I.n = 0; I.vary = 0;
}
static void imm_end() {
    Imm &I = g.imm;
    I.inside = false;
    if (UNLIKELY(g.dl.mode)) {
        if (I.n) dl_capture_imm(I.mode, I.rec, I.n, I.vary);
        if (g.dl.mode == GL_COMPILE) { g.cur = g.dl.saved; return; }
    }
    if (I.n) imm_submit();
}

void rectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
    init_only();
    if (g.imm.inside) { set_error(GL_INVALID_OPERATION); return; }
    imm_begin(GL_POLYGON);
    imm_vertex(x1, y1, 0, 1); imm_vertex(x2, y1, 0, 1); imm_vertex(x2, y2, 0, 1); imm_vertex(x1, y2, 0, 1);
    imm_end();
}
void raster_pos(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { g.cur.raster = Vec4{x, y, z, w}; g.cur.raster_valid = true; }
void window_pos(GLfloat x, GLfloat y, GLfloat z) { g.cur.raster = Vec4{x, y, z, 1.0f}; g.cur.raster_valid = true; }

void vattrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if (index == 0 && g.imm.inside) { imm_vertex(x, y, z, w); return; }   // compat: attribute 0 provokes a vertex
    if (UNLIKELY(g.dl.mode) && dl_vattrib(index, x, y, z, w)) return;
    ORY_PROLOGUE();
    es.glVertexAttrib4f(index, x, y, z, w);
    if (index < LOC_COUNT) { g.es_const[index] = Vec4{x, y, z, w}; g.es_const_valid |= 1u << index; }
}
void vattrib4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    ORY_PROLOGUE(); es.glVertexAttribI4i(index, x, y, z, w);
    if (index < LOC_COUNT) g.es_const_valid &= ~(1u << index);
}
void vattrib4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    ORY_PROLOGUE(); es.glVertexAttribI4ui(index, x, y, z, w);
    if (index < LOC_COUNT) g.es_const_valid &= ~(1u << index);
}

// fetch one legacy-array element as floats (GL conversion rules); base = mapped VBO or null for client memory
bool ca_fetch(int c, GLint idx, GLfloat *o, const uint8_t *base) {
    const ClientArray &a = g.ca[c];
    if (!(g.ca_on & (1u << c))) return false;
    if (a.buf && !base) return false;
    GLsizei tsz = type_size(a.type), st = a.stride ? a.stride : a.size * tsz;
    const uint8_t *p = (a.buf ? base + (uintptr_t)a.ptr : (const uint8_t *)a.ptr) + (size_t)idx * (size_t)st;
    bool nrm = (c == CA_NORMAL || c == CA_COLOR || c == CA_COLOR2);
    o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 1;
    for (GLint k = 0; k < a.size && k < 4; ++k) {
        const uint8_t *q = p + (size_t)k * (size_t)tsz;
        switch (a.type) {
        case GL_FLOAT: { float f; memcpy(&f, q, 4); o[k] = f; break; }
        case GL_DOUBLE: { double d; memcpy(&d, q, 8); o[k] = (GLfloat)d; break; }
        case GL_UNSIGNED_BYTE: o[k] = nrm ? unorm_ub(*q) : (GLfloat)*q; break;
        case GL_BYTE: o[k] = nrm ? snorm_b((GLbyte)*q) : (GLfloat)(GLbyte)*q; break;
        case GL_SHORT: { GLshort v; memcpy(&v, q, 2); o[k] = nrm ? snorm_s(v) : (GLfloat)v; break; }
        case GL_UNSIGNED_SHORT: { GLushort v; memcpy(&v, q, 2); o[k] = nrm ? unorm_us(v) : (GLfloat)v; break; }
        case GL_INT: { GLint v; memcpy(&v, q, 4); o[k] = nrm ? snorm_i(v) : (GLfloat)v; break; }
        default: { GLuint v; memcpy(&v, q, 4); o[k] = nrm ? unorm_ui(v) : (GLfloat)v; break; }
        }
    }
    return true;
}

// ------------------------------------------------------------------ legacy client arrays -> VAO_CLIENT
// Sets pointers for rows [B, B+n) of the enabled legacy arrays the program reads.
// *es_base receives the ES vertex index corresponding to row B. Returns the enabled attribute mask.
static uint32_t setup_legacy(Vao &V, uint32_t reads, GLint B, GLsizei n, GLint *es_base) {
    struct A { GLuint loc; const ClientArray *c; GLsizei esz, st; GLboolean norm; };
    A arr[CA_COUNT]; int na = 0, nclient = 0; bool dbl = false;
    for (int c = 0; c < CA_COUNT; ++c) {
        if (!(g.ca_on & (1u << c))) continue;
        GLuint loc = ca_loc(c);
        if (!(reads & (1u << loc))) continue;
        const ClientArray &ca = g.ca[c];
        A &a = arr[na++];
        a.loc = loc; a.c = &ca; a.esz = ca.size * type_size(ca.type); a.st = ca.stride ? ca.stride : a.esz;
        a.norm = (c == CA_NORMAL || c == CA_COLOR || c == CA_COLOR2) && !is_float_type(ca.type) ? GL_TRUE : GL_FALSE;
        if (!ca.buf) { ++nclient; if (ca.type == GL_DOUBLE) dbl = true; }
    }
    uint32_t mask = 0;
    if (!nclient) {                               // all from VBOs: one binding per (buffer, stride) group
        bool done[CA_COUNT] = {};
        for (int i = 0; i < na; ++i) {
            if (done[i]) continue;
            AttrSrc as[CA_COUNT]; int n = 0;
            for (int j = i; j < na; ++j) {
                if (done[j] || arr[j].c->buf != arr[i].c->buf || arr[j].c->stride != arr[i].c->stride) continue;
                as[n++] = AttrSrc{arr[j].loc, arr[j].c->size, arr[j].c->type, arr[j].norm, (uintptr_t)arr[j].c->ptr};
                done[j] = true; mask |= 1u << arr[j].loc;
            }
            set_attrs(V, arr[i].c->buf, arr[i].c->stride, as, n);
        }
        *es_base = B;
        return mask;
    }
    // interleaved client block?
    if (nclient == na && !dbl) {
        const uint8_t *minp = nullptr; GLsizei S = arr[0].st; bool ok = true;
        for (int i = 0; i < na; ++i) {
            const uint8_t *p = (const uint8_t *)arr[i].c->ptr;
            if (arr[i].st != S) { ok = false; break; }
            if (!minp || p < minp) minp = p;
        }
        size_t maxend = 0;
        for (int i = 0; ok && i < na; ++i) {
            size_t rel = (size_t)((const uint8_t *)arr[i].c->ptr - minp);
            if (rel + (size_t)arr[i].esz > (size_t)S) ok = false;
            if (rel + (size_t)arr[i].esz > maxend) maxend = rel + (size_t)arr[i].esz;
        }
        if (ok) {
            size_t bytes = (size_t)(n - 1) * (size_t)S + maxend;
            size_t align = (S % 4) ? (size_t)S * 4 : (size_t)S;
            GLuint buf; size_t off;
            ring_upload(g.rv, minp + (size_t)B * (size_t)S, bytes, align, &buf, &off);
            AttrSrc as[CA_COUNT];
            for (int i = 0; i < na; ++i) {
                const A &a = arr[i];
                as[i] = AttrSrc{a.loc, a.c->size, a.c->type, a.norm, (uintptr_t)((const uint8_t *)a.c->ptr - minp)};
                mask |= 1u << a.loc;
            }
            set_attrs(V, buf, S, as, na);
            *es_base = (GLint)(off / (size_t)S);
            return mask;
        }
    }
    // separate / mixed: gather client arrays tightly (doubles -> floats)
    bool mixed = nclient != na;
    GLint rows0 = mixed ? 0 : B;                  // first row stored
    GLsizei rows = mixed ? B + n : n;
    for (int i = 0; i < na; ++i) {
        const A &a = arr[i];
        if (a.c->buf) {
            const AttrSrc s1{a.loc, a.c->size, a.c->type, a.norm, (uintptr_t)a.c->ptr};
            set_attrs(V, a.c->buf, a.c->stride, &s1, 1); mask |= 1u << a.loc; continue;
        }
        bool d = a.c->type == GL_DOUBLE;
        GLenum ty = d ? GL_FLOAT : a.c->type;
        GLsizei esz = d ? a.c->size * 4 : a.esz;
        size_t bytes = (size_t)rows * (size_t)esz;
        size_t off; GLuint buf = g.rv.buf;
        uint8_t *dst = ring_alloc(g.rv, bytes, 4, &off), *tmp = nullptr;
        if (!dst) { tmp = (uint8_t *)malloc(bytes); dst = tmp; if (!dst) { set_error(GL_OUT_OF_MEMORY); continue; } }
        const uint8_t *src = (const uint8_t *)a.c->ptr;
        for (GLint r = B; r < B + n; ++r) {
            uint8_t *o = dst + (size_t)(r - rows0) * (size_t)esz;
            const uint8_t *s = src + (size_t)r * (size_t)a.st;
            if (d) { for (GLint k = 0; k < a.c->size; ++k) { double v; memcpy(&v, s + 8 * k, 8); float f = (float)v; memcpy(o + 4 * k, &f, 4); } }
            else memcpy(o, s, (size_t)esz);
        }
        if (tmp) { ring_upload(g.rv, tmp, bytes, 4, &buf, &off); free(tmp); }
        else ring_commit(g.rv);
        const AttrSrc s1{a.loc, a.c->size, ty, a.norm, (uintptr_t)off};
        set_attrs(V, buf, esz, &s1, 1);
        mask |= 1u << a.loc;
    }
    *es_base = mixed ? B : 0;                     // mixed: absolute rows; separate: rows start at 0
    return mask;
}

static void draw_arrays_legacy(GLenum mode, GLint first, GLsizei count) {
    if (!(g.ca_on & (1u << CA_VERTEX))) return;
    GLenum esmode = mode; bool quads = false;
    switch (mode) {
    case GL_QUADS: count &= ~3; quads = true; break;
    case GL_QUAD_STRIP: count &= ~1; esmode = GL_TRIANGLE_STRIP; break;
    case GL_POLYGON: esmode = GL_TRIANGLE_FAN; break;
    default: if (mode > GL_POLYGON) { set_error(GL_INVALID_ENUM); return; }
    }
    if (count <= 0) return;
    uint32_t reads = program_prepare(mode == GL_POINTS);
    if (!reads) return;
    Vao &V = g.vao_client;
    es_bind_vao(V.id);
    GLint base = first;
    uint32_t am = setup_legacy(V, reads, first, count, &base);
    set_enabled(V, am);
    set_consts(reads & ~am, g.cur.color, g.cur.normal, g.cur.color2, g.cur.fog, g.cur.tex);
    if (quads) {
        if (!quad_ibo_ensure(count)) return;
        if (V.element != g.quad_ibo) { es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.quad_ibo); V.element = g.quad_ibo; }
        es.glDrawElementsBaseVertex(GL_TRIANGLES, count / 4 * 6, g.quad_ibo_type, nullptr, base);
    } else {
        es.glDrawArrays(esmode, base, count);
    }
}

GLuint app_element_buffer() {
    if (!g.b.app_element_known) {
        sync_app_vao();
        GLint v = 0; es.glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &v);
        g.b.app_element = (GLuint)v; g.b.app_element_known = true;
    }
    return g.b.app_element;
}
// indices -> 32-bit triangle/line list for primitive types ES lacks (and generic conversion)
static uint32_t convert_indices(uint32_t *out, GLenum mode, const void *src, GLenum type, GLsizei count) {
    uint32_t k = 0, n = (uint32_t)count;
    switch (mode) {
    case GL_QUADS:
        for (uint32_t q = 0; q + 3 < n; q += 4) {
            uint32_t a = idx_at(src, type, q), b = idx_at(src, type, q + 1), c = idx_at(src, type, q + 2), d = idx_at(src, type, q + 3);
            out[k++] = a; out[k++] = b; out[k++] = c; out[k++] = a; out[k++] = c; out[k++] = d;
        }
        break;
    case GL_QUAD_STRIP:
        for (uint32_t i = 0; i + 3 < n; i += 2) {
            uint32_t a = idx_at(src, type, i), b = idx_at(src, type, i + 1), c = idx_at(src, type, i + 2), d = idx_at(src, type, i + 3);
            out[k++] = a; out[k++] = b; out[k++] = d; out[k++] = a; out[k++] = d; out[k++] = c;
        }
        break;
    case GL_POLYGON:
        for (uint32_t i = 1; i + 1 < n; ++i) { out[k++] = idx_at(src, type, 0); out[k++] = idx_at(src, type, i); out[k++] = idx_at(src, type, i + 1); }
        break;
    default:
        for (uint32_t i = 0; i < n; ++i) out[k++] = idx_at(src, type, i);
    }
    return k;
}

static void draw_elements_any(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint rs, GLint re) {
    if (count <= 0) return;
    size_t isz = type == GL_UNSIGNED_INT ? 4 : type == GL_UNSIGNED_SHORT ? 2 : type == GL_UNSIGNED_BYTE ? 1 : 0;
    if (!isz) { set_error(GL_INVALID_ENUM); return; }
    bool legacy = !g.b.app_program || g.ca_on;
    if (legacy && !(g.ca_on & (1u << CA_VERTEX))) return;
    bool conv = mode == GL_QUADS || mode == GL_QUAD_STRIP || mode == GL_POLYGON;
    uint32_t reads = legacy ? program_prepare(mode == GL_POINTS) : app_program_prepare();
    if (!reads) return;
    bool client = false;
    if (legacy)
        for (int c = 0; c < CA_COUNT; ++c)
            if ((g.ca_on & (1u << c)) && (reads & (1u << ca_loc(c))) && !g.ca[c].buf) client = true;
    GLuint ebo = app_element_buffer();
    void *tmp = nullptr; const void *isrc = indices;
    if (ebo && (conv || (client && rs < 0))) {
        sync_app_vao();
        const void *m = es.glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)(uintptr_t)indices, (GLsizeiptr)(count * isz), GL_MAP_READ_BIT);
        if (!m) return;
        tmp = malloc((size_t)count * isz);
        if (tmp) memcpy(tmp, m, (size_t)count * isz);
        es.glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        if (!tmp) { set_error(GL_OUT_OF_MEMORY); return; }
        isrc = tmp;
    }
    GLint B = 0, E = 0;
    if (client) {
        if (rs >= 0) { B = rs; E = re; }
        else {
            uint32_t mn = 0xffffffffu, mx = 0;
            for (GLsizei i = 0; i < count; ++i) { uint32_t v = idx_at(isrc, type, (size_t)i); if (v < mn) mn = v; if (v > mx) mx = v; }
            B = (GLint)mn; E = (GLint)mx;
        }
    }
    Vao *V = nullptr;
    GLint bv = 0;
    if (legacy) {
        V = &g.vao_client; es_bind_vao(V->id);
        GLint base = 0;
        uint32_t am = setup_legacy(*V, reads, B, client ? E - B + 1 : 0, &base);
        set_enabled(*V, am);
        set_consts(reads & ~am, g.cur.color, g.cur.normal, g.cur.color2, g.cur.fog, g.cur.tex);
        bv = client ? base - B : 0;
    } else {
        sync_app_vao();
    }
    GLenum esmode = mode;
    if (conv || (!ebo && legacy) || (!ebo && !legacy)) {
        // upload (converted) indices into the index ring
        GLuint ib; size_t ioff; GLsizei n; GLenum it = type;
        if (conv) {
            uint32_t *ci = (uint32_t *)malloc((size_t)count * 6 * 4);
            if (!ci) { free(tmp); set_error(GL_OUT_OF_MEMORY); return; }
            n = (GLsizei)convert_indices(ci, mode, isrc, type, count);
            ring_upload(g.ri, ci, (size_t)n * 4, 4, &ib, &ioff);
            free(ci); it = GL_UNSIGNED_INT; esmode = GL_TRIANGLES;
        } else {
            n = count;
            ring_upload(g.ri, isrc, (size_t)count * isz, 4, &ib, &ioff);
        }
        if (legacy) {
            if (V->element != ib) { es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib); V->element = ib; }
            es.glDrawElementsBaseVertex(esmode, n, it, (const void *)ioff, bv);
        } else {
            es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
            es.glDrawElements(esmode, n, it, (const void *)ioff);
            es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        }
    } else if (legacy) {
        if (V->element != ebo) { es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); V->element = ebo; }
        es.glDrawElementsBaseVertex(mode, count, type, indices, bv);
    } else {
        es.glDrawElements(mode, count, type, indices);
    }
    free(tmp);
}

static void draw_arrays_generic(GLenum mode, GLint first, GLsizei count) {
    if (!app_program_prepare()) return;
    sync_app_vao();
    switch (mode) {
    case GL_QUADS: {
        count &= ~3;
        if (count <= 0 || !quad_ibo_ensure(count)) return;
        es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.quad_ibo);
        es.glDrawElementsBaseVertex(GL_TRIANGLES, count / 4 * 6, g.quad_ibo_type, nullptr, first);
        es.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app_element_buffer());
        return;
    }
    case GL_QUAD_STRIP: es.glDrawArrays(GL_TRIANGLE_STRIP, first, count & ~1); return;
    case GL_POLYGON: es.glDrawArrays(GL_TRIANGLE_FAN, first, count); return;
    default: es.glDrawArrays(mode, first, count); return;
    }
}
} // namespace ory

using namespace ory;

// ================================================================== immediate mode entry points
/* jar: GL11.glBegin(I)V */
OGL_EXPORT void glBegin(GLenum mode) {
    init_only();
    if (g.imm.inside) { set_error(GL_INVALID_OPERATION); return; }
    if (mode > GL_POLYGON) { set_error(GL_INVALID_ENUM); return; }
    imm_begin(mode);
}
/* jar: GL11.glEnd()V */
OGL_EXPORT void glEnd(void) {
    if (!g.imm.inside) { set_error(GL_INVALID_OPERATION); return; }
    imm_end();
}

// ================================================================== legacy client arrays
static inline void set_ca(int c, GLint size, GLenum type, GLsizei stride, const void *ptr) {
    ClientArray &a = g.ca[c];
    a.ptr = ptr; a.buf = g.b.app_array; a.size = size; a.type = type; a.stride = stride;
}
/* jar: GL11.nglVertexPointer(IIIJ)V */
OGL_EXPORT void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    if (size < 2 || size > 4 || stride < 0) { set_error(GL_INVALID_VALUE); return; }
    set_ca(CA_VERTEX, size, type, stride, pointer);
}
/* jar: GL11.nglNormalPointer(IIJ)V */
OGL_EXPORT void glNormalPointer(GLenum type, GLsizei stride, const void *pointer) {
    if (stride < 0) { set_error(GL_INVALID_VALUE); return; }
    set_ca(CA_NORMAL, 3, type, stride, pointer);
}
/* jar: GL11.nglColorPointer(IIIJ)V */
OGL_EXPORT void glColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    if ((size != 3 && size != 4) || stride < 0) { set_error(GL_INVALID_VALUE); return; }
    set_ca(CA_COLOR, size, type, stride, pointer);
}
/* jar: GL14.nglSecondaryColorPointer(IIIJ)V */
OGL_EXPORT void glSecondaryColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    if (size != 3 || stride < 0) { set_error(GL_INVALID_VALUE); return; }
    set_ca(CA_COLOR2, size, type, stride, pointer);
}
/* jar: GL14.nglFogCoordPointer(IIJ)V */
OGL_EXPORT void glFogCoordPointer(GLenum type, GLsizei stride, const void *pointer) {
    if (stride < 0) { set_error(GL_INVALID_VALUE); return; }
    set_ca(CA_FOG, 1, type, stride, pointer);
}
/* jar: GL11.nglTexCoordPointer(IIIJ)V */
OGL_EXPORT void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    if (size < 1 || size > 4 || stride < 0) { set_error(GL_INVALID_VALUE); return; }
    set_ca(CA_TEX0 + (int)g.f.client_active, size, type, stride, pointer);
}
/* jar: GL11.nglEdgeFlagPointer(IJ)V */
OGL_EXPORT void glEdgeFlagPointer(GLsizei stride, const void *pointer) { (void)stride; (void)pointer; }
/* jar: GL11.nglIndexPointer(IIJ)V */
OGL_EXPORT void glIndexPointer(GLenum type, GLsizei stride, const void *pointer) { (void)type; (void)stride; (void)pointer; }
static int ca_of(GLenum array) {
    switch (array) {
    case GL_VERTEX_ARRAY: return CA_VERTEX;
    case GL_NORMAL_ARRAY: return CA_NORMAL;
    case GL_COLOR_ARRAY: return CA_COLOR;
    case GL_SECONDARY_COLOR_ARRAY: return CA_COLOR2;
    case GL_FOG_COORD_ARRAY: return CA_FOG;
    case GL_TEXTURE_COORD_ARRAY: return CA_TEX0 + (int)g.f.client_active;
    case GL_INDEX_ARRAY: case GL_EDGE_FLAG_ARRAY: return -2;
    default: return -1;
    }
}
/* jar: GL11.glEnableClientState(I)V */
OGL_EXPORT void glEnableClientState(GLenum array) {
    int c = ca_of(array);
    if (c == -1) { set_error(GL_INVALID_ENUM); return; }
    if (c >= 0) g.ca_on |= 1u << c;
}
/* jar: GL11.glDisableClientState(I)V */
OGL_EXPORT void glDisableClientState(GLenum array) {
    int c = ca_of(array);
    if (c == -1) { set_error(GL_INVALID_ENUM); return; }
    if (c >= 0) g.ca_on &= ~(1u << c);
}
/* jar: GL11C.nglGetPointerv(IJ)V */
OGL_EXPORT void glGetPointerv(GLenum pname, void **params) {
    ORY_PROLOGUE();
    switch (pname) {
    case GL_VERTEX_ARRAY_POINTER: *params = (void *)g.ca[CA_VERTEX].ptr; return;
    case GL_NORMAL_ARRAY_POINTER: *params = (void *)g.ca[CA_NORMAL].ptr; return;
    case GL_COLOR_ARRAY_POINTER: *params = (void *)g.ca[CA_COLOR].ptr; return;
    case GL_SECONDARY_COLOR_ARRAY_POINTER: *params = (void *)g.ca[CA_COLOR2].ptr; return;
    case GL_FOG_COORD_ARRAY_POINTER: *params = (void *)g.ca[CA_FOG].ptr; return;
    case GL_TEXTURE_COORD_ARRAY_POINTER: *params = (void *)g.ca[CA_TEX0 + g.f.client_active].ptr; return;
    case GL_INDEX_ARRAY_POINTER: case GL_EDGE_FLAG_ARRAY_POINTER: case GL_FEEDBACK_BUFFER_POINTER: case GL_SELECTION_BUFFER_POINTER:
        *params = nullptr; return;
    default: es.glGetPointerv(pname, params);
    }
}
/* jar: GL11.nglInterleavedArrays(IIJ)V */
OGL_EXPORT void glInterleavedArrays(GLenum format, GLsizei stride, const void *pointer) {
    // table: tex comps, color comps (ub if 4U), normal, vertex comps, offsets computed
    int tc = 0, cc = 0, vc = 3; bool cub = false, nrm = false;
    switch (format) {
    case GL_V2F: vc = 2; break;
    case GL_V3F: break;
    case GL_C4UB_V2F: cc = 4; cub = true; vc = 2; break;
    case GL_C4UB_V3F: cc = 4; cub = true; break;
    case GL_C3F_V3F: cc = 3; break;
    case GL_N3F_V3F: nrm = true; break;
    case GL_C4F_N3F_V3F: cc = 4; nrm = true; break;
    case GL_T2F_V3F: tc = 2; break;
    case GL_T4F_V4F: tc = 4; vc = 4; break;
    case GL_T2F_C4UB_V3F: tc = 2; cc = 4; cub = true; break;
    case GL_T2F_C3F_V3F: tc = 2; cc = 3; break;
    case GL_T2F_N3F_V3F: tc = 2; nrm = true; break;
    case GL_T2F_C4F_N3F_V3F: tc = 2; cc = 4; nrm = true; break;
    case GL_T4F_C4F_N3F_V4F: tc = 4; cc = 4; nrm = true; vc = 4; break;
    default: set_error(GL_INVALID_ENUM); return;
    }
    GLsizei off = 0, oc, on, ov, ot = 0;
    off += tc * 4; oc = off; off += cc ? (cub ? 4 : cc * 4) : 0; on = off; off += nrm ? 12 : 0; ov = off; off += vc * 4;
    GLsizei st = stride ? stride : off;
    const uint8_t *p = (const uint8_t *)pointer;
    g.ca_on &= ~((1u << CA_NORMAL) | (1u << CA_COLOR) | (1u << (CA_TEX0 + g.f.client_active)) | (1u << CA_COLOR2) | (1u << CA_FOG));
    if (tc) { set_ca(CA_TEX0 + (int)g.f.client_active, tc, GL_FLOAT, st, p + ot); g.ca_on |= 1u << (CA_TEX0 + g.f.client_active); }
    if (cc) { set_ca(CA_COLOR, cc, cub ? GL_UNSIGNED_BYTE : GL_FLOAT, st, p + oc); g.ca_on |= 1u << CA_COLOR; }
    if (nrm) { set_ca(CA_NORMAL, 3, GL_FLOAT, st, p + on); g.ca_on |= 1u << CA_NORMAL; }
    set_ca(CA_VERTEX, vc, GL_FLOAT, st, p + ov); g.ca_on |= 1u << CA_VERTEX;
}
/* jar: GL11.glArrayElement(I)V */
OGL_EXPORT void glArrayElement(GLint i) {
    if (!g.imm.inside) return;
    GLfloat v[4];
    if (ca_fetch(CA_COLOR, i, v, nullptr)) cur_color(v[0], v[1], v[2], g.ca[CA_COLOR].size == 4 ? v[3] : 1.0f);
    if (ca_fetch(CA_NORMAL, i, v, nullptr)) cur_normal(v[0], v[1], v[2]);
    if (ca_fetch(CA_COLOR2, i, v, nullptr)) cur_color2(v[0], v[1], v[2]);
    if (ca_fetch(CA_FOG, i, v, nullptr)) cur_fogcoord(v[0]);
    for (int u = 0; u < MAX_TEX_UNITS; ++u) if (ca_fetch(CA_TEX0 + u, i, v, nullptr)) cur_texcoord((GLuint)u, v[0], v[1], v[2], v[3]);
    if (ca_fetch(CA_VERTEX, i, v, nullptr)) imm_vertex(v[0], v[1], v[2], v[3]);
}

// ================================================================== draw calls
/* jar: GL11C.glDrawArrays(III)V */
OGL_EXPORT void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    ORY_PROLOGUE();
    if (count < 0 || first < 0) { set_error(GL_INVALID_VALUE); return; }
    if (UNLIKELY(g.dl.mode)) { dl_capture_arrays(mode, first, count, 0, nullptr); if (g.dl.mode == GL_COMPILE) return; }
    if (!count) return;
    if (g.b.app_program && !g.ca_on) draw_arrays_generic(mode, first, count);
    else draw_arrays_legacy(mode, first, count);
}
/* jar: GL11C.nglDrawElements(IIIJ)V */
OGL_EXPORT void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    ORY_PROLOGUE();
    if (count < 0) { set_error(GL_INVALID_VALUE); return; }
    if (UNLIKELY(g.dl.mode)) { dl_capture_arrays(mode, 0, count, type, indices); if (g.dl.mode == GL_COMPILE) return; }
    draw_elements_any(mode, count, type, indices, -1, -1);
}
/* jar: GL12C.nglDrawRangeElements(IIIIIJ)V */
OGL_EXPORT void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices) {
    ORY_PROLOGUE();
    if (count < 0 || end < start) { set_error(GL_INVALID_VALUE); return; }
    if (UNLIKELY(g.dl.mode)) { dl_capture_arrays(mode, 0, count, type, indices); if (g.dl.mode == GL_COMPILE) return; }
    draw_elements_any(mode, count, type, indices, (GLint)start, (GLint)end);
}
/* jar: GL14C.nglMultiDrawArrays(IJJI)V */
OGL_EXPORT void glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount) {
    ORY_PROLOGUE();
    for (GLsizei i = 0; i < drawcount; ++i) {
        if (count[i] <= 0) continue;
        if (UNLIKELY(g.dl.mode)) { dl_capture_arrays(mode, first[i], count[i], 0, nullptr); if (g.dl.mode == GL_COMPILE) continue; }
        if (g.b.app_program && !g.ca_on) draw_arrays_generic(mode, first[i], count[i]);
        else draw_arrays_legacy(mode, first[i], count[i]);
    }
}
/* jar: GL14C.nglMultiDrawElements(IJIJI)V */
OGL_EXPORT void glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const void *const *indices, GLsizei drawcount) {
    ORY_PROLOGUE();
    for (GLsizei i = 0; i < drawcount; ++i) {
        if (UNLIKELY(g.dl.mode)) { dl_capture_arrays(mode, 0, count[i], type, indices[i]); if (g.dl.mode == GL_COMPILE) continue; }
        draw_elements_any(mode, count[i], type, indices[i], -1, -1);
    }
}

// ================================================================== buffer objects / VAO shadow
/* jar: GL15C.glBindBuffer(II)V */
OGL_EXPORT void glBindBuffer(GLenum target, GLuint buffer) {
    ORY_PROLOGUE();
    switch (target) {
    case GL_ARRAY_BUFFER: g.b.app_array = buffer; return;
    case GL_COPY_WRITE_BUFFER: g.b.app_copy_write = buffer; return;
    case GL_ELEMENT_ARRAY_BUFFER:
        sync_app_vao(); es.glBindBuffer(target, buffer);
        g.b.app_element = buffer; g.b.app_element_known = true; return;
    case GL_PIXEL_UNPACK_BUFFER: g.b.app_unpack = buffer; break;
    default: break;
    }
    es.glBindBuffer(target, buffer);
}
/* jar: GL15C.nglDeleteBuffers(IJ)V */
OGL_EXPORT void glDeleteBuffers(GLsizei n, const GLuint *buffers) {
    ORY_PROLOGUE();
    es.glDeleteBuffers(n, buffers);
    for (GLsizei i = 0; i < n; ++i) {
        GLuint id = buffers[i];
        if (!id) continue;
        if (g.b.app_array == id) g.b.app_array = 0;
        if (g.b.es_array == id) g.b.es_array = 0;
        if (g.b.app_copy_write == id) g.b.app_copy_write = 0;
        if (g.b.es_copy_write == id) g.b.es_copy_write = 0;
        if (g.b.app_element == id) g.b.app_element_known = false;
        if (g.b.app_unpack == id) g.b.app_unpack = 0;
        invalidate_buffer_refs(id);
    }
}
/* jar: GL15C.nglBufferData(IJJI)V */
OGL_EXPORT void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    ORY_PROLOGUE(); sync_target(target);
    StatUpload su_(g.st.buf_n, g.st.buf_max, g.st.buf_bytes, g.st.buf_us, data ? (uint64_t)size : 0);
    switch (usage) {                                     // desktop READ/COPY usages are hints; ES accepts all 9
    default: break;
    }
    es.glBufferData(target, size, data, usage);
}
/* jar: GL15C.nglBufferSubData(IJJJ)V */
OGL_EXPORT void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    ORY_PROLOGUE(); sync_target(target);
    StatUpload su_(g.st.buf_n, g.st.buf_max, g.st.buf_bytes, g.st.buf_us, (uint64_t)size);
    es.glBufferSubData(target, offset, size, data);
}
/* jar: GL15C.nglGetBufferSubData(IJJJ)V */
OGL_EXPORT void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data) {
    ORY_PROLOGUE(); sync_target(target);
    const void *m = es.glMapBufferRange(target, offset, size, GL_MAP_READ_BIT);
    if (!m) return;
    memcpy(data, m, (size_t)size);
    es.glUnmapBuffer(target);
}
/* jar: GL15C.nglMapBuffer(II)J */
OGL_EXPORT void *glMapBuffer(GLenum target, GLenum access) {
    ORY_PROLOGUE(); sync_target(target);
    GLint64 size = 0; es.glGetBufferParameteri64v(target, GL_BUFFER_SIZE, &size);
    GLbitfield bits = access == GL_READ_ONLY ? GL_MAP_READ_BIT : access == GL_WRITE_ONLY ? GL_MAP_WRITE_BIT : (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    if (size <= 0) return nullptr;
    return es.glMapBufferRange(target, 0, (GLsizeiptr)size, bits);
}
/* jar: GL30C.nglMapBufferRange(IJJI)J */
OGL_EXPORT void *glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    ORY_PROLOGUE(); sync_target(target); return es.glMapBufferRange(target, offset, length, access);
}
/* jar: GL15C.glUnmapBuffer(I)Z */
OGL_EXPORT GLboolean glUnmapBuffer(GLenum target) { ORY_PROLOGUE(); sync_target(target); return es.glUnmapBuffer(target); }
/* jar: GL30C.glFlushMappedBufferRange(IJJ)V */
OGL_EXPORT void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    ORY_PROLOGUE(); sync_target(target); es.glFlushMappedBufferRange(target, offset, length);
}
/* jar: GL15C.nglGetBufferParameteriv(IIJ)V */
OGL_EXPORT void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    ORY_PROLOGUE(); sync_target(target);
    if (pname == GL_BUFFER_ACCESS) {
        GLint f = 0; es.glGetBufferParameteriv(target, GL_BUFFER_ACCESS_FLAGS, &f);
        *params = (f & GL_MAP_READ_BIT) && (f & GL_MAP_WRITE_BIT) ? GL_READ_WRITE : (f & GL_MAP_READ_BIT) ? GL_READ_ONLY : GL_WRITE_ONLY;
        return;
    }
    es.glGetBufferParameteriv(target, pname, params);
}
/* jar: GL15C.nglGetBufferPointerv(IIJ)V */
OGL_EXPORT void glGetBufferPointerv(GLenum target, GLenum pname, void **params) {
    ORY_PROLOGUE(); sync_target(target); es.glGetBufferPointerv(target, pname, params);
}
/* jar: GL30C.glBindVertexArray(I)V */
OGL_EXPORT void glBindVertexArray(GLuint array) {
    ORY_PROLOGUE();
    if (array != g.b.app_vao) { g.b.app_vao = array; g.b.app_element_known = false; }
}
/* jar: GL30C.nglDeleteVertexArrays(IJ)V */
OGL_EXPORT void glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    ORY_PROLOGUE();
    for (GLsizei i = 0; i < n; ++i) {
        if (!arrays[i]) continue;
        if (arrays[i] == g.b.app_vao) { g.b.app_vao = 0; g.b.app_element_known = false; }
        if (arrays[i] == g.b.es_vao) g.b.es_vao = 0;
    }
    es.glDeleteVertexArrays(n, arrays);
}
/* jar: GL20C.nglVertexAttribPointer(IIIZIJ)V */
OGL_EXPORT void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    ORY_PROLOGUE(); sync_app_vao(); sync_app_array();
    es.glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}
/* jar: GL30C.nglVertexAttribIPointer(IIIIJ)V */
OGL_EXPORT void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) {
    ORY_PROLOGUE(); sync_app_vao(); sync_app_array();
    es.glVertexAttribIPointer(index, size, type, stride, pointer);
}
/* jar: GL20C.glEnableVertexAttribArray(I)V */
OGL_EXPORT void glEnableVertexAttribArray(GLuint index) { ORY_PROLOGUE(); sync_app_vao(); es.glEnableVertexAttribArray(index); }
/* jar: GL20C.glDisableVertexAttribArray(I)V */
OGL_EXPORT void glDisableVertexAttribArray(GLuint index) { ORY_PROLOGUE(); sync_app_vao(); es.glDisableVertexAttribArray(index); }
/* jar: GL20C.nglGetVertexAttribiv(IIJ)V */
OGL_EXPORT void glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) { ORY_PROLOGUE(); sync_app_vao(); es.glGetVertexAttribiv(index, pname, params); }
/* jar: GL20C.nglGetVertexAttribfv(IIJ)V */
OGL_EXPORT void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) { ORY_PROLOGUE(); sync_app_vao(); es.glGetVertexAttribfv(index, pname, params); }
/* jar: GL30C.nglGetVertexAttribIiv(IIJ)V */
OGL_EXPORT void glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params) { ORY_PROLOGUE(); sync_app_vao(); es.glGetVertexAttribIiv(index, pname, params); }
/* jar: GL30C.nglGetVertexAttribIuiv(IIJ)V */
OGL_EXPORT void glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params) { ORY_PROLOGUE(); sync_app_vao(); es.glGetVertexAttribIuiv(index, pname, params); }
/* jar: GL20C.nglGetVertexAttribPointerv(IIJ)V */
OGL_EXPORT void glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer) { ORY_PROLOGUE(); sync_app_vao(); es.glGetVertexAttribPointerv(index, pname, pointer); }
/* jar: GL20C.nglGetVertexAttribdv(IIJ)V */
OGL_EXPORT void glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params) {
    ORY_PROLOGUE(); sync_app_vao();
    GLfloat f[4] = {0, 0, 0, 0}; es.glGetVertexAttribfv(index, pname, f);
    int n = pname == GL_CURRENT_VERTEX_ATTRIB ? 4 : 1;
    for (int i = 0; i < n; ++i) params[i] = f[i];
}
