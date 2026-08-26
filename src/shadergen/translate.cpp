/* GLSL 1.20 -> GLSL ES 3.20.

   Lingkupnya ditentukan oleh 35 shader di assets/minecraft/shaders, bukan oleh
   spesifikasi GLSL. Yang benar-benar dipakai Minecraft 1.12.2:

     #version 120, attribute, varying, uniform, texture2D, gl_FragColor,
     gl_Position, sampler2D, satu baris #extension

   Yang TIDAK dipakai sama sekali - dan karena itu tidak diterjemahkan:
     gl_ModelViewProjectionMatrix, gl_Vertex, gl_Color, gl_Normal, gl_TexCoord,
     gl_FrontColor, gl_FragData, texture2DLod, layout(), in/out gaya baru.

   Semua shader memakai uniform dan atribut bernama sendiri, sehingga tidak ada
   satu pun uniform bawaan fixed-function yang perlu disuntikkan. Itu membuat
   penerjemah ini jauh lebih kecil daripada penerjemah GLSL umumnya. */

#include "translate.h"
#include <stdlib.h>
#include <string.h>

namespace oryon {
namespace {

struct Buf { char *p, *end; bool ovf; };

void put(Buf &b, const char *s, unsigned n) {
    if (b.p + n > b.end) { b.ovf = true; return; }
    memcpy(b.p, s, n);
    b.p += n;
}
void puts_(Buf &b, const char *s) { put(b, s, (unsigned) strlen(s)); }

bool ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

struct Rep { const char *from; const char *to; };

/* Kata cadangan GLSL ES 3.x yang masih identifier sah di GLSL 120.
   Pemindaian atas 35 shader Minecraft menunjukkan tidak satu pun dipakai
   sebagai nama uniform/attribute/varying - hanya `sample` sebagai variabel
   lokal di blur.fsh - sehingga mengganti namanya aman terhadap
   glGetUniformLocation di sisi Java. */
const Rep kReserved[] = {
    { "sample", "sample_" }, { "patch", "patch_" }, { "subroutine", "subroutine_" },
    { "common", "common_" }, { "partition", "partition_" }, { "active", "active_" },
    { "filter", "filter_" }, { "resource", "resource_" }, { "superp", "superp_" },
    { "input", "input_" },   { "output", "output_" },     { "sizeof", "sizeof_" },
    { "cast", "cast_" },     { "namespace", "namespace_" }, { "using", "using_" },
    { 0, 0 }
};

const Rep kCommon[] = {
    { "texture2D", "texture" },
    { "texture2DProj", "textureProj" },
    { "texture2DLod", "textureLod" },
    { "shadow2D", "texture" },
    { "gl_FragColor", "oryon_FragColor" },
    { 0, 0 }
};

const char *lookup(const Rep *t, const char *s, unsigned n) {
    for (const Rep *r = t; r->from; ++r)
        if (strlen(r->from) == n && memcmp(r->from, s, n) == 0) return r->to;
    return 0;
}

bool line_is(const char *p, const char *end, const char *directive) {
    while (p < end && (*p == ' ' || *p == '\t')) ++p;
    if (p >= end || *p != '#') return false;
    ++p;
    while (p < end && (*p == ' ' || *p == '\t')) ++p;
    const unsigned n = (unsigned) strlen(directive);
    return (unsigned) (end - p) >= n && memcmp(p, directive, n) == 0;
}


/* Pra-pass: memotong `= <nilai>` dari deklarasi uniform dan mencatat nilainya.
   Deklarasi Minecraft selalu satu per baris dengan literal sederhana
   (`= 1.5` atau `= vec3(0.3, 0.59, 0.11)`), jadi pemindaian per baris sudah
   tepat - tidak perlu parser ekspresi. */
unsigned strip_uniform_inits(const char *src, unsigned len, char *out, unsigned cap,
                             GlslDefault *defs, unsigned def_cap, unsigned *ndef) {
    unsigned w = 0;
    *ndef = 0;
    const char *p = src, *end = src + len;

    while (p < end) {
        const char *nl = p;
        while (nl < end && *nl != '\n') ++nl;
        const char *line_end = nl;

        const char *q = p;
        while (q < line_end && (*q == ' ' || *q == '\t')) ++q;

        bool handled = false;
        if ((unsigned) (line_end - q) > 8 && memcmp(q, "uniform", 7) == 0 &&
            (q[7] == ' ' || q[7] == '\t')) {
            const char *eq = q;
            while (eq < line_end && *eq != '=' && *eq != ';') ++eq;
            if (eq < line_end && *eq == '=') {
                const char *ne = eq;
                while (ne > q && (ne[-1] == ' ' || ne[-1] == '\t')) --ne;
                const char *ns = ne;
                while (ns > q && ident_char(ns[-1])) --ns;

                if (*ndef < def_cap && (unsigned) (ne - ns) < sizeof(defs->name)) {
                    GlslDefault &d = defs[*ndef];
                    memcpy(d.name, ns, (size_t) (ne - ns));
                    d.name[ne - ns] = 0;
                    d.comps = 0;
                    const char *v = eq + 1;
                    while (v < line_end && d.comps < 4) {
                        const bool num = (*v >= '0' && *v <= '9') ||
                            ((*v == '-' || *v == '.') && v + 1 < line_end &&
                             v[1] >= '0' && v[1] <= '9');
                        if (num) {
                            char *stop = 0;
                            d.v[d.comps++] = strtof(v, &stop);
                            v = stop ? stop : v + 1;
                        } else {
                            ++v;
                        }
                    }
                    if (d.comps) ++(*ndef);
                }

                const unsigned keep = (unsigned) (ne - p);
                if (w + keep + 2 > cap) return 0;
                memcpy(out + w, p, keep);
                w += keep;
                out[w++] = ';';
                out[w++] = '\n';
                handled = true;
            }
        }

        if (!handled) {
            const unsigned n = (unsigned) (nl - p) + (nl < end ? 1u : 0u);
            if (w + n > cap) return 0;
            memcpy(out + w, p, n);
            w += n;
        }
        p = (nl < end) ? nl + 1 : end;
    }
    return w;
}


/* GLSL 1.20 mengonversi int ke float secara implisit; GLSL ES 3.x tidak.
   Di 35 shader Minecraft hanya ada 10 literal integer telanjang, dan tiga di
   antaranya benar-benar menggagalkan kompilasi (`clamp(f, 0.5, 2)`,
   `2 - abs(...)`, `c != 0`). Sisanya berada di dalam pemanggilan makro
   `OffsetVec(1,0)` yang bisa mengembang menjadi ivec2 - mempromosikannya justru
   akan merusak. Karena itu promosi dilewati pada baris yang menyebut tipe
   integer dan di dalam daftar argumen makro. */

struct MacroSet { char name[16][32]; unsigned n; };

void collect_macros(const char *src, unsigned len, MacroSet *m) {
    m->n = 0;
    const char *p = src, *end = src + len;
    while (p < end) {
        const char *nl = p;
        while (nl < end && *nl != '\n') ++nl;
        const char *q = p;
        while (q < nl && (*q == ' ' || *q == '\t')) ++q;
        if (q + 7 < nl && memcmp(q, "#define", 7) == 0) {
            q += 7;
            while (q < nl && (*q == ' ' || *q == '\t')) ++q;
            const char *ns = q;
            while (q < nl && ident_char(*q)) ++q;
            if (q < nl && *q == '(' && (unsigned) (q - ns) < 32 && m->n < 16) {
                memcpy(m->name[m->n], ns, (size_t) (q - ns));
                m->name[m->n][q - ns] = 0;
                ++m->n;
            }
        }
        p = (nl < end) ? nl + 1 : end;
    }
}

bool is_macro(const MacroSet &m, const char *s, unsigned n) {
    for (unsigned i = 0; i < m.n; ++i)
        if (strlen(m.name[i]) == n && memcmp(m.name[i], s, n) == 0) return true;
    return false;
}

bool word_at(const char *p, const char *end, const char *w) {
    const unsigned n = (unsigned) strlen(w);
    if ((unsigned) (end - p) < n) return false;
    if (memcmp(p, w, n) != 0) return false;
    return !(p + n < end && ident_char(p[n]));
}

/* Apakah baris ini menyebut tipe integer? Kalau ya, literal di dalamnya
   dibiarkan apa adanya. */
bool line_declares_int(const char *p, const char *end) {
    const char *nl = p;
    while (nl < end && *nl != '\n') ++nl;
    for (const char *q = p; q < nl; ++q) {
        if (q > p && ident_char(q[-1])) continue;
        if (word_at(q, nl, "int") || word_at(q, nl, "uint") ||
            word_at(q, nl, "ivec2") || word_at(q, nl, "ivec3") ||
            word_at(q, nl, "ivec4") || word_at(q, nl, "uvec2") ||
            word_at(q, nl, "uvec3") || word_at(q, nl, "uvec4"))
            return true;
    }
    return false;
}

}  /* namespace */

unsigned glsl_translate(const char *src, unsigned len, bool fragment,
                        char *out, unsigned cap, char *attrib_out,
                        unsigned attrib_cap, GlslDefault *defs,
                        unsigned def_cap, unsigned *def_count) {
    if (!src || !out || cap < 256) return 0;
    if (attrib_out && attrib_cap) attrib_out[0] = 0;

    unsigned ndef = 0;
    static char s_pre[32768];
    if (defs && def_cap && len < sizeof s_pre - 64) {
        const unsigned n = strip_uniform_inits(src, len, s_pre, sizeof s_pre,
                                               defs, def_cap, &ndef);
        if (n) { src = s_pre; len = n; }
    }
    if (def_count) *def_count = ndef;

    /* Sudah GLSL ES? Biarkan apa adanya. */
    if (len >= 12 && memcmp(src, "#version 3", 10) == 0 && strstr(src, " es")) {
        if (len + 1 > cap) return 0;
        memcpy(out, src, len);
        out[len] = 0;
        return len;
    }

    const bool uses_fragcolor = fragment &&
        strstr(src, "gl_FragColor") != 0;

    Buf b = { out, out + cap - 1, false };
    puts_(b, "#version 320 es\n");
    puts_(b, "precision highp float;\nprecision highp int;\n");
    if (fragment) puts_(b, "precision highp sampler2D;\n");
    if (uses_fragcolor) puts_(b, "layout(location = 0) out vec4 oryon_FragColor;\n");

    const char *p = src, *end = src + len;
    bool want_attrib_name = false;

    MacroSet macros;
    collect_macros(src, len, &macros);
    unsigned macro_depth = 0;
    bool int_line = line_declares_int(p, end);

    while (p < end) {
        const char c = *p;
        if (c == '\n') int_line = line_declares_int(p + 1, end);
        if (macro_depth) {
            if (c == '(') ++macro_depth;
            else if (c == ')') --macro_depth;
        }

        /* Komentar disalin apa adanya - isinya tidak boleh ikut diganti. */
        if (c == '/' && p + 1 < end && p[1] == '/') {
            const char *q = p;
            while (q < end && *q != '\n') ++q;
            put(b, p, (unsigned) (q - p));
            p = q;
            continue;
        }
        if (c == '/' && p + 1 < end && p[1] == '*') {
            const char *q = p + 2;
            while (q + 1 < end && !(q[0] == '*' && q[1] == '/')) ++q;
            q = (q + 1 < end) ? q + 2 : end;
            put(b, p, (unsigned) (q - p));
            p = q;
            continue;
        }

        /* Baris pengarah: #version dibuang (sudah ditulis ulang), #extension
           dibuang karena fitur GL_EXT_gpu_shader4 sudah inti di ES 3. */
        if (c == '#') {
            const char *q = p;
            while (q < end && *q != '\n') ++q;
            if (!line_is(p, q, "version") && !line_is(p, q, "extension"))
                put(b, p, (unsigned) (q - p));
            p = q;
            continue;
        }

        if (ident_char(c) && !(c >= '0' && c <= '9')) {
            const char *q = p;
            while (q < end && ident_char(*q)) ++q;
            const unsigned n = (unsigned) (q - p);

            const char *rep = 0;
            if (n == 9 && memcmp(p, "attribute", 9) == 0) {
                rep = "in";
                want_attrib_name = true;
            } else if (n == 7 && memcmp(p, "varying", 7) == 0) {
                rep = fragment ? "in" : "out";
            } else {
                rep = lookup(kCommon, p, n);
                if (!rep) rep = lookup(kReserved, p, n);
            }

            if (rep) puts_(b, rep);
            else     put(b, p, n);

            if (!rep && is_macro(macros, p, n)) {
                const char *r = q;
                while (r < end && (*r == ' ' || *r == '\t')) ++r;
                if (r < end && *r == '(') macro_depth = 1;
            }

            /* Nama atribut = identifier ketiga setelah kata `attribute`
               (attribute <tipe> <nama>). */
            if (want_attrib_name && !rep && attrib_out && attrib_cap) {
                static const char *const kTypes[] = {
                    "vec2", "vec3", "vec4", "float", "int", "mat2", "mat3", "mat4", 0
                };
                bool is_type = false;
                for (int i = 0; kTypes[i]; ++i)
                    if (strlen(kTypes[i]) == n && memcmp(kTypes[i], p, n) == 0) is_type = true;
                if (!is_type && attrib_out[0] == 0 && n + 1 <= attrib_cap) {
                    memcpy(attrib_out, p, n);
                    attrib_out[n] = 0;
                    want_attrib_name = false;
                }
            }
            p = q;
            continue;
        }

        if (c >= '0' && c <= '9' && !macro_depth && !int_line &&
            !(p > src && (ident_char(p[-1]) || p[-1] == '.'))) {
            const char *q = p;
            while (q < end && *q >= '0' && *q <= '9') ++q;
            const bool bare = !(q < end && (*q == '.' || *q == 'e' || *q == 'E' ||
                                            *q == 'x' || *q == 'X' || ident_char(*q)));
            const char *before = p;
            while (before > src && (before[-1] == ' ' || before[-1] == '\t')) --before;
            const char *after = q;
            while (after < end && (*after == ' ' || *after == '\t')) ++after;
            const bool subscript = (before > src && before[-1] == '[') ||
                                   (after < end && *after == ']');
            put(b, p, (unsigned) (q - p));
            if (bare && !subscript) puts_(b, ".0");
            p = q;
            continue;
        }

        put(b, p, 1);
        ++p;
    }

    if (b.ovf) return 0;
    *b.p = 0;
    return (unsigned) (b.p - out);
}

}  /* namespace oryon */
