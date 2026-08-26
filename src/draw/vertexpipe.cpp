#include "vertexpipe.h"
#include "../driver/driver.h"
#include "../shadergen/shadergen.h"
#include "../state/state.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

namespace oryon {

ArrayState g_arr;

namespace {

GLuint     s_vao, s_stream_vbo, s_quad_ib, s_scratch_ib;
GLsizeiptr s_stream_cap;
unsigned   s_quad_cap;
uint32_t   s_attrib_on;

ImmVertex *s_imm;
unsigned   s_imm_count, s_imm_cap;
GLenum     s_imm_mode;
bool       s_imm_active;

/* ----------------------------------------------------- display list ------ */

struct RecAttrib {
    GLuint    buffer;
    unsigned  loc;
    GLint     size;
    GLenum    type;
    GLboolean norm;
    GLsizei   stride;
    GLintptr  offset;
};

struct RecDraw {
    GLenum    mode;
    GLsizei   count;
    RecAttrib attr[3 + FF_MAX_TEX];
    unsigned  nattr;
    GLuint    own_vbo;
    uint16_t  attr_tex;
    bool      has_color, has_normal;
};

struct DList {
    RecDraw *draws;
    unsigned n, cap;
    bool     used;
};

DList   *s_lists;
unsigned s_list_cap;
RecDraw *s_rec;             /* bukan nol saat sebuah list sedang dikompilasi */
unsigned s_rec_list;
bool     s_rec_execute;     /* GL_COMPILE_AND_EXECUTE */

/* -------------------------------------------------------------- utilitas -- */

GLsizei type_size(GLenum t) {
    switch (t) {
    case GL_BYTE: case GL_UNSIGNED_BYTE:   return 1;
    case GL_SHORT: case GL_UNSIGNED_SHORT: case GL_HALF_FLOAT: return 2;
    case GL_DOUBLE:                        return 8;
    default:                               return 4;
    }
}

/* Aturan GL: warna dan normal bertipe integer dinormalkan; posisi dan
   koordinat tekstur tidak. Lightmap Minecraft adalah GL_SHORT bernilai 0..240
   yang lalu diskalakan matriks tekstur - menormalkannya akan membuat dunia
   gelap gulita. */
GLboolean normalized_for(unsigned loc, GLenum type) {
    if (type == GL_FLOAT || type == GL_DOUBLE || type == GL_HALF_FLOAT) return GL_FALSE;
    return (loc == ATTR_COLOR || loc == ATTR_NORMAL) ? GL_TRUE : GL_FALSE;
}

void attrib_on(unsigned loc, GLint size, GLenum type, GLboolean norm,
               GLsizei stride, const void *off) {
    if (!(s_attrib_on & (1u << loc))) {
        gles.glEnableVertexAttribArray(loc);
        s_attrib_on |= 1u << loc;
    }
    gles.glVertexAttribPointer(loc, size, type, norm, stride, off);
}

void attribs_trim(uint32_t keep) {
    uint32_t drop = s_attrib_on & ~keep;
    for (unsigned loc = 0; drop; ++loc, drop >>= 1)
        if (drop & 1u) gles.glDisableVertexAttribArray(loc);
    s_attrib_on &= keep;
}

/* Menerapkan atribut sekarang, atau merekamnya ke dalam display list. */
void emit_attrib(GLuint buffer, unsigned loc, GLint size, GLenum type,
                 GLboolean norm, GLsizei stride, GLintptr offset, uint32_t *keep) {
    if (s_rec) {
        if (s_rec->nattr >= 3 + FF_MAX_TEX) return;
        RecAttrib &r = s_rec->attr[s_rec->nattr++];
        r.buffer = buffer; r.loc = loc; r.size = size; r.type = type;
        r.norm = norm; r.stride = stride; r.offset = offset;
        return;
    }
    gles.glBindBuffer(GL_ARRAY_BUFFER, buffer);
    attrib_on(loc, size, type, norm, stride, (const void *) offset);
    *keep |= 1u << loc;
}

bool imm_reserve(unsigned n) {
    if (n <= s_imm_cap) return true;
    unsigned cap = s_imm_cap ? s_imm_cap : 1024;
    while (cap < n) cap *= 2;
    ImmVertex *p = (ImmVertex *) realloc(s_imm, (size_t) cap * sizeof(ImmVertex));
    if (!p) return false;
    s_imm = p;
    s_imm_cap = cap;
    return true;
}

/* Orphaning: minta buffer baru ke driver, lalu isi. Pola baku untuk data
   sekali pakai; menghindari driver menunggu frame sebelumnya selesai. */
void stream_upload(const void *data, GLsizeiptr size) {
    gles.glBindBuffer(GL_ARRAY_BUFFER, s_stream_vbo);
    if (size > s_stream_cap) {
        gles.glBufferData(GL_ARRAY_BUFFER, size, data, GL_STREAM_DRAW);
        s_stream_cap = size;
    } else {
        gles.glBufferData(GL_ARRAY_BUFFER, s_stream_cap, 0, GL_STREAM_DRAW);
        gles.glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }
}

/* Buffer permanen milik sebuah display list: diunggah sekali, dipakai
   berkali-kali. Itulah seluruh alasan display list ada. */
GLuint static_upload(const void *data, GLsizeiptr size) {
    GLuint vbo = 0;
    gles.glGenBuffers(1, &vbo);
    if (!vbo) return 0;
    gles.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gles.glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    return vbo;
}

bool quad_indices(unsigned quads) {
    if (quads <= s_quad_cap) return true;
    unsigned n = s_quad_cap ? s_quad_cap : 512;
    while (n < quads) n *= 2;
    uint32_t *idx = (uint32_t *) malloc((size_t) n * 6 * sizeof(uint32_t));
    if (!idx) return false;
    for (unsigned q = 0; q < n; ++q) {
        const uint32_t b = q * 4;
        uint32_t *o = idx + (size_t) q * 6;
        o[0] = b; o[1] = b + 1; o[2] = b + 2;
        o[3] = b; o[4] = b + 2; o[5] = b + 3;
    }
    gles.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_quad_ib);
    gles.glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                      (GLsizeiptr) n * 6 * (GLsizeiptr) sizeof(uint32_t),
                      idx, GL_STATIC_DRAW);
    free(idx);
    s_quad_cap = n;
    return true;
}

/* GL_QUAD_STRIP dan GL_POLYGON identik dengan padanan GLES-nya: urutan vertex
   quad strip menutupi luasan yang sama dengan triangle strip, dan GL_POLYGON
   dijamin cembung sehingga triangle fan tepat. Hanya GL_QUADS yang benar-benar
   butuh index buffer. */
GLenum gles_mode(GLenum m) {
    switch (m) {
    case GL_QUAD_STRIP: return GL_TRIANGLE_STRIP;
    case GL_POLYGON:    return GL_TRIANGLE_FAN;
    default:            return m;
    }
}

void draw_now(GLenum mode, GLsizei count) {
    if (mode == GL_QUADS) {
        const unsigned quads = (unsigned) count >> 2;
        if (!quads || !quad_indices(quads)) return;
        gles.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_quad_ib);
        gles.glDrawElements(GL_TRIANGLES, (GLsizei) (quads * 6), GL_UNSIGNED_INT, 0);
        gles.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_arr.element_buffer);
        return;
    }
    gles.glDrawArrays(gles_mode(mode), 0, count);
}

void list_append(const RecDraw &r) {
    DList &l = s_lists[s_rec_list - 1];
    if (l.n >= l.cap) {
        const unsigned cap = l.cap ? l.cap * 2 : 8;
        RecDraw *p = (RecDraw *) realloc(l.draws, (size_t) cap * sizeof(RecDraw));
        if (!p) return;
        l.draws = p;
        l.cap = cap;
    }
    l.draws[l.n++] = r;
}

void submit(GLenum mode, GLsizei count) {
    if (s_rec) {
        /* Satu display list bisa berisi BANYAK gambar. ModelRenderer milik
           Minecraft merekam satu gambar per kotak model - model biped punya
           enam - jadi menyimpan hanya satu berarti entitas, tangan, dan item
           tidak pernah muncul. Tiap submit disimpan di sini, lalu slot rekaman
           dikosongkan untuk gambar berikutnya. */
        s_rec->mode = mode;
        s_rec->count = count;
        list_append(*s_rec);
        s_rec->nattr = 0;
        s_rec->own_vbo = 0;
        s_rec->count = 0;
        return;
    }
    draw_now(mode, count);
}

/* Memasang program: milik Minecraft bila ada, kalau tidak program
   fixed-function yang dibangkitkan dari state sekarang. */
bool bind_program(const FFKey &k) {
    if (g_user_program) return true;
    return ffp_bind(k);
}

}  /* namespace */

/* ------------------------------------------------------------------ init -- */

bool draw_init() {
    if (s_vao) return true;
    gles.glGenVertexArrays(1, &s_vao);
    gles.glGenBuffers(1, &s_stream_vbo);
    gles.glGenBuffers(1, &s_quad_ib);
    gles.glGenBuffers(1, &s_scratch_ib);
    if (!s_vao || !s_stream_vbo || !s_quad_ib) return false;
    gles.glBindVertexArray(s_vao);
    return true;
}

/* ---------------------------------------------------------- mode langsung -- */

bool imm_active() { return s_imm_active; }

void imm_begin(GLenum mode) {
    s_imm_mode = mode;
    s_imm_count = 0;
    s_imm_active = true;
}

void imm_vertex(float x, float y, float z) {
    if (!s_imm_active || !imm_reserve(s_imm_count + 1)) return;
    const FFAttribState &a = g_ff.a;
    ImmVertex &v = s_imm[s_imm_count++];
    v.pos[0] = x; v.pos[1] = y; v.pos[2] = z;
    memcpy(v.color, a.cur_color, sizeof v.color);
    memcpy(v.normal, a.cur_normal, sizeof v.normal);
    v.tex[0][0] = a.cur_tex[0][0]; v.tex[0][1] = a.cur_tex[0][1];
    v.tex[1][0] = a.cur_tex[1][0]; v.tex[1][1] = a.cur_tex[1][1];
}

void imm_end() {
    s_imm_active = false;
    if (!s_imm_count || !draw_init()) return;

    const FFAttribState &a = g_ff.a;
    uint16_t attr_tex = 0;
    for (int u = 0; u < 2; ++u)
        if (g_user_program || (a.tex[u].enabled && !a.tex[u].gen_mask))
            attr_tex |= (uint16_t) (1u << u);

    FFKey k;
    ff_key(&k, attr_tex, true, a.lighting);
    if (!bind_program(k)) return;

    gles.glBindVertexArray(s_vao);

    const GLsizeiptr bytes = (GLsizeiptr) s_imm_count * (GLsizeiptr) sizeof(ImmVertex);
    GLuint vbo;
    if (s_rec) {
        vbo = static_upload(s_imm, bytes);
        s_rec->own_vbo = vbo;
        s_rec->attr_tex = attr_tex;
        s_rec->has_color = true;
        s_rec->has_normal = (k.flags & FF_ATTR_NORMAL) != 0;
    } else {
        stream_upload(s_imm, bytes);
        vbo = s_stream_vbo;
    }

    const GLsizei st = (GLsizei) sizeof(ImmVertex);
    uint32_t keep = 0;
    emit_attrib(vbo, ATTR_POS, 3, GL_FLOAT, GL_FALSE, st,
                (GLintptr) offsetof(ImmVertex, pos), &keep);
    emit_attrib(vbo, ATTR_COLOR, 4, GL_FLOAT, GL_FALSE, st,
                (GLintptr) offsetof(ImmVertex, color), &keep);
    if (k.flags & FF_ATTR_NORMAL)
        emit_attrib(vbo, ATTR_NORMAL, 3, GL_FLOAT, GL_FALSE, st,
                    (GLintptr) offsetof(ImmVertex, normal), &keep);
    for (int u = 0; u < 2; ++u) {
        if (!((attr_tex >> u) & 1u)) continue;
        emit_attrib(vbo, (unsigned) (ATTR_TEX0 + u), 2, GL_FLOAT, GL_FALSE, st,
                    (GLintptr) (offsetof(ImmVertex, tex) + (size_t) u * 2 * sizeof(float)),
                    &keep);
    }
    if (!s_rec) attribs_trim(keep);

    submit(s_imm_mode, (GLsizei) s_imm_count);
    if (!s_rec) gles.glBindBuffer(GL_ARRAY_BUFFER, g_arr.array_buffer);
}

/* ------------------------------------------------------------ client array */

namespace {

struct Slot {
    const ClientArray *a;
    unsigned loc;
};

/* Menyiapkan seluruh atribut untuk sebuah gambar. `first` dilipat ke dalam
   offset penunjuk, sehingga penggambaran selalu mulai dari vertex 0 - itu yang
   membuat satu index buffer quad statis cukup untuk semua kasus. */
bool setup_attribs(const Slot *slots, unsigned n, GLint first, GLsizei count) {
    uint32_t keep = 0;

    const unsigned char *base = 0;
    GLsizei ilv_stride = 0;
    bool any_client = false, interleaved = true;

    for (unsigned i = 0; i < n; ++i) {
        const ClientArray &c = *slots[i].a;
        if (c.buffer) continue;
        const unsigned char *p = (const unsigned char *) c.ptr;
        if (!any_client) { base = p; ilv_stride = c.stride; any_client = true; }
        else {
            if (c.stride != ilv_stride) interleaved = false;
            if (p < base) {
                if ((size_t) (base - p) >= (size_t) ilv_stride) interleaved = false;
                base = p;
            } else if ((size_t) (p - base) >= (size_t) ilv_stride) {
                interleaved = false;
            }
        }
    }

    GLintptr off[3 + FF_MAX_TEX];
    GLuint   buf[3 + FF_MAX_TEX];
    for (unsigned i = 0; i < n; ++i) { off[i] = 0; buf[i] = 0; }

    GLuint dest = 0;
    if (any_client && interleaved) {
        const unsigned char *from = base + (size_t) first * (size_t) ilv_stride;
        const GLsizeiptr bytes = (GLsizeiptr) ilv_stride * count;
        if (s_rec) { dest = static_upload(from, bytes); s_rec->own_vbo = dest; }
        else       { stream_upload(from, bytes); dest = s_stream_vbo; }
        for (unsigned i = 0; i < n; ++i)
            if (!slots[i].a->buffer) {
                off[i] = (GLintptr) ((const unsigned char *) slots[i].a->ptr - base);
                buf[i] = dest;
            }
    } else if (any_client) {
        /* Jalur umum: tiap array terpisah. Tidak dipakai Minecraft, tapi harus
           benar kalau ada mod yang memakai array terpisah. */
        GLsizeiptr total = 0;
        for (unsigned i = 0; i < n; ++i) {
            const ClientArray &c = *slots[i].a;
            if (c.buffer) continue;
            off[i] = (GLintptr) total;
            total += ((GLsizeiptr) c.stride * count + 3) & ~(GLsizeiptr) 3;
        }
        if (s_rec) { dest = static_upload(0, total); s_rec->own_vbo = dest; }
        else {
            gles.glBindBuffer(GL_ARRAY_BUFFER, s_stream_vbo);
            if (total > s_stream_cap) {
                gles.glBufferData(GL_ARRAY_BUFFER, total, 0, GL_STREAM_DRAW);
                s_stream_cap = total;
            } else {
                gles.glBufferData(GL_ARRAY_BUFFER, s_stream_cap, 0, GL_STREAM_DRAW);
            }
            dest = s_stream_vbo;
        }
        gles.glBindBuffer(GL_ARRAY_BUFFER, dest);
        for (unsigned i = 0; i < n; ++i) {
            const ClientArray &c = *slots[i].a;
            if (c.buffer) continue;
            const unsigned char *from =
                (const unsigned char *) c.ptr + (size_t) first * (size_t) c.stride;
            gles.glBufferSubData(GL_ARRAY_BUFFER, off[i],
                                 (GLsizeiptr) c.stride * count, from);
            buf[i] = dest;
        }
    }

    for (unsigned i = 0; i < n; ++i) {
        const ClientArray &c = *slots[i].a;
        const unsigned loc = slots[i].loc;
        const GLboolean norm = normalized_for(loc, c.type);
        if (c.buffer) {
            emit_attrib(c.buffer, loc, c.size, c.type, norm, c.stride,
                        (GLintptr) (size_t) c.ptr + (GLintptr) first * c.stride, &keep);
        } else {
            emit_attrib(buf[i], loc, c.size, c.type, norm, c.stride, off[i], &keep);
        }
    }
    if (!s_rec) {
        attribs_trim(keep);
        gles.glBindBuffer(GL_ARRAY_BUFFER, g_arr.array_buffer);
    }
    return true;
}

unsigned collect(Slot *slots, uint16_t *attr_tex, bool *has_color, bool *has_normal) {
    const FFAttribState &a = g_ff.a;
    const bool user = (g_user_program != 0);
    unsigned n = 0;
    if (!g_arr.vertex.enabled) return 0;
    slots[n].a = &g_arr.vertex; slots[n].loc = ATTR_POS; ++n;

    *has_color = g_arr.color.enabled;
    if (*has_color) { slots[n].a = &g_arr.color; slots[n].loc = ATTR_COLOR; ++n; }

    *has_normal = g_arr.normal.enabled;
    if (*has_normal) { slots[n].a = &g_arr.normal; slots[n].loc = ATTR_NORMAL; ++n; }

    *attr_tex = 0;
    for (int u = 0; u < FF_MAX_TEX; ++u) {
        if (!g_arr.tex[u].enabled) continue;
        if (!user && (!a.tex[u].enabled || a.tex[u].gen_mask)) continue;
        *attr_tex |= (uint16_t) (1u << u);
        slots[n].a = &g_arr.tex[u];
        slots[n].loc = (unsigned) (ATTR_TEX0 + u);
        ++n;
    }
    return n;
}

}  /* namespace */

void draw_arrays(GLenum mode, GLint first, GLsizei count) {
    if (count <= 0 || !draw_init()) return;

    Slot slots[3 + FF_MAX_TEX];
    uint16_t attr_tex = 0;
    bool has_color = false, has_normal = false;
    const unsigned n = collect(slots, &attr_tex, &has_color, &has_normal);
    if (!n) return;

    FFKey k;
    ff_key(&k, attr_tex, has_color, has_normal);
    if (!bind_program(k)) return;

    if (s_rec) {
        s_rec->attr_tex = attr_tex;
        s_rec->has_color = has_color;
        s_rec->has_normal = has_normal;
    }
    gles.glBindVertexArray(s_vao);
    if (!setup_attribs(slots, n, first, count)) return;
    submit(mode, count);
}

void draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    if (count <= 0 || !draw_init()) return;
    if (mode == GL_QUADS) {
        set_error(GL_INVALID_OPERATION);
        return;
    }

    Slot slots[3 + FF_MAX_TEX];
    uint16_t attr_tex = 0;
    bool has_color = false, has_normal = false;
    const unsigned n = collect(slots, &attr_tex, &has_color, &has_normal);
    if (!n) return;

    FFKey k;
    ff_key(&k, attr_tex, has_color, has_normal);
    if (!bind_program(k)) return;

    gles.glBindVertexArray(s_vao);
    if (!setup_attribs(slots, n, 0, count)) return;

    if (g_arr.element_buffer) {
        gles.glDrawElements(gles_mode(mode), count, type, indices);
        return;
    }
    const GLsizei isz = type_size(type);
    gles.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_scratch_ib);
    gles.glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) isz * count,
                      indices, GL_STREAM_DRAW);
    gles.glDrawElements(gles_mode(mode), count, type, 0);
    gles.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_arr.element_buffer);
}

/* ------------------------------------------------------- display list ----- */

namespace {

bool lists_reserve(unsigned n) {
    if (n <= s_list_cap) return true;
    unsigned cap = s_list_cap ? s_list_cap : 32;
    while (cap < n) cap *= 2;
    DList *p = (DList *) realloc(s_lists, (size_t) cap * sizeof(DList));
    if (!p) return false;
    memset(p + s_list_cap, 0, (size_t) (cap - s_list_cap) * sizeof(DList));
    s_lists = p;
    s_list_cap = cap;
    return true;
}

void list_clear(DList &l) {
    for (unsigned i = 0; i < l.n; ++i)
        if (l.draws[i].own_vbo) gles.glDeleteBuffers(1, &l.draws[i].own_vbo);
    free(l.draws);
    l.draws = 0;
    l.n = l.cap = 0;
}

}  /* namespace */

GLuint list_gen(GLsizei range) {
    if (range <= 0) return 0;
    /* Cari deretan id yang berurutan dan belum terpakai. */
    for (unsigned start = 0; ; ++start) {
        if (!lists_reserve(start + (unsigned) range + 1)) return 0;
        bool free_run = true;
        for (GLsizei i = 0; i < range; ++i)
            if (s_lists[start + i].used) { free_run = false; break; }
        if (!free_run) continue;
        for (GLsizei i = 0; i < range; ++i) {
            s_lists[start + i].used = true;
            s_lists[start + i].draws = 0;
            s_lists[start + i].n = s_lists[start + i].cap = 0;
        }
        return start + 1;                       /* id GL bermula dari 1 */
    }
}

void list_delete(GLuint first, GLsizei range) {
    for (GLsizei i = 0; i < range; ++i) {
        const unsigned idx = first + (unsigned) i;
        if (idx == 0 || idx > s_list_cap) continue;
        DList &l = s_lists[idx - 1];
        list_clear(l);
        l.used = false;
    }
}

bool list_compiling() { return s_rec != 0; }

bool list_new(GLuint list, GLenum mode) {
    if (!draw_init() || list == 0 || list > s_list_cap) return false;
    DList &l = s_lists[list - 1];
    list_clear(l);
    l.used = true;
    s_rec_list = list;
    s_rec_execute = (mode == GL_COMPILE_AND_EXECUTE);
    /* Slot rekaman disiapkan lambat: satu RecDraw per submit. */
    s_rec = (RecDraw *) calloc(1, sizeof(RecDraw));
    return s_rec != 0;
}

void list_end() {
    if (!s_rec) return;
    /* Setiap gambar sudah disimpan saat submit; di sini hanya membereskan. */
    free(s_rec);
    s_rec = 0;
    if (s_rec_execute) list_call(s_rec_list);
}

/* Diputar ulang dengan state SEKARANG, bukan state saat direkam. Minecraft
   hanya menaruh geometri di dalam display list-nya (langit, bintang), jadi
   inilah tafsir yang benar - dan yang membuat matriks serta kabut terkini ikut
   berlaku pada langit. */
void list_call(GLuint list) {
    if (list == 0 || list > s_list_cap || !draw_init()) return;
    const DList &l = s_lists[list - 1];
    if (!l.used || !l.n) return;

    gles.glBindVertexArray(s_vao);
    for (unsigned d = 0; d < l.n; ++d) {
        const RecDraw &r = l.draws[d];
        FFKey k;
        ff_key(&k, r.attr_tex, r.has_color, r.has_normal);
        if (!bind_program(k)) continue;

        uint32_t keep = 0;
        for (unsigned i = 0; i < r.nattr; ++i) {
            const RecAttrib &a = r.attr[i];
            gles.glBindBuffer(GL_ARRAY_BUFFER, a.buffer);
            attrib_on(a.loc, a.size, a.type, a.norm, a.stride, (const void *) a.offset);
            keep |= 1u << a.loc;
        }
        attribs_trim(keep);
        draw_now(r.mode, r.count);
    }
    gles.glBindBuffer(GL_ARRAY_BUFFER, g_arr.array_buffer);
}

}  /* namespace oryon */
