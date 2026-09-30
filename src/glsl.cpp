// Oryon -- GLSL translator. Token-level rewrite with light semantic analysis:
//  * #version 1xx -> 320 es, #extension hoisted (require->enable), precision prologue, #line to keep line numbers
//  * attribute/varying -> in/out, texture2D/3D/Cube/Proj/Lod/shadow2D -> ES 3 texture functions
//  * gl_FragColor/gl_FragData, fixed-function builtins -> declared ins/outs/uniforms at aliased locations
//  * identifiers reserved in ES 3.20 renamed, uniform initializers stripped (applied after link)
//  * integer literals promoted to float where the context is float (ES has no implicit int->float)
#include "oryon.hpp"
#include "glsl.hpp"
#include <stdio.h>
#include <stdarg.h>

namespace ory {
namespace {
enum TokT : uint8_t { T_ID, T_INT, T_FLOAT, T_OP, T_WS, T_PP, T_END };
struct Tok { TokT t; uint32_t b, e; };           // [b,e) in source

struct Buf {
    char *p = nullptr; size_t n = 0, cap = 0;
    bool reserve(size_t add) {
        if (n + add + 1 <= cap) return true;
        size_t nc = cap ? cap : 4096;
        while (nc < n + add + 1) nc *= 2;
        char *q = (char *)realloc(p, nc);
        if (!q) return false;
        p = q; cap = nc; return true;
    }
    void put(const char *s, size_t l) { if (reserve(l)) { memcpy(p + n, s, l); n += l; p[n] = 0; } }
    void put(const char *s) { put(s, strlen(s)); }
    void putf(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
        char tmp[512]; va_list ap; va_start(ap, fmt); int l = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
        if (l > 0) put(tmp, (size_t)(l < (int)sizeof tmp ? l : (int)sizeof tmp - 1));
    }
};

inline bool is_id0(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
inline bool is_id(char c) { return is_id0(c) || (c >= '0' && c <= '9'); }
inline bool is_dig(char c) { return c >= '0' && c <= '9'; }

int tokenize(const char *s, Tok *out, int max) {
    int n = 0; uint32_t i = 0; bool line_start = true;
    while (s[i] && n < max - 1) {
        uint32_t b = i; char c = s[i];
        if (c == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') ++i; out[n++] = {T_WS, b, i}; continue; }
        if (c == '/' && s[i + 1] == '*') { i += 2; while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) ++i; if (s[i]) i += 2; out[n++] = {T_WS, b, i}; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            while (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n') { if (s[i] == '\n') line_start = true; ++i; }
            out[n++] = {T_WS, b, i}; continue;
        }
        if (c == '#' && line_start) {                 // whole preprocessor line (with continuations)
            while (s[i] && !(s[i] == '\n' && s[i - 1] != '\\')) ++i;
            out[n++] = {T_PP, b, i}; continue;
        }
        line_start = false;
        if (is_id0(c)) { while (is_id(s[i])) ++i; out[n++] = {T_ID, b, i}; continue; }
        if (is_dig(c) || (c == '.' && is_dig(s[i + 1]))) {
            bool fl = false;
            if (c == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) { i += 2; while (is_id(s[i])) ++i; out[n++] = {T_INT, b, i}; continue; }
            while (is_dig(s[i])) ++i;
            if (s[i] == '.') { fl = true; ++i; while (is_dig(s[i])) ++i; }
            if (s[i] == 'e' || s[i] == 'E') { fl = true; ++i; if (s[i] == '+' || s[i] == '-') ++i; while (is_dig(s[i])) ++i; }
            if (s[i] == 'f' || s[i] == 'F' || s[i] == 'l' || s[i] == 'L') { fl = true; ++i; }
            if (s[i] == 'u' || s[i] == 'U') ++i;
            out[n++] = {fl ? T_FLOAT : T_INT, b, i}; continue;
        }
        static const char *k3[] = {"<<=", ">>="};
        static const char *k2[] = {"<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "^^", "++", "--", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="};
        uint32_t l = 1;
        for (const char *o : k3) if (!strncmp(s + i, o, 3)) { l = 3; break; }
        if (l == 1) for (const char *o : k2) if (!strncmp(s + i, o, 2)) { l = 2; break; }
        i += l; out[n++] = {T_OP, b, i};
    }
    out[n] = {T_END, i, i};
    return n;
}

struct Ctx2 {
    const char *s; Tok *t; int n;
    bool eq(int i, const char *w) const { size_t l = strlen(w); return i >= 0 && i < n && t[i].e - t[i].b == l && !strncmp(s + t[i].b, w, l); }
    bool op(int i, char c) const { return i >= 0 && i < n && t[i].t == T_OP && t[i].e - t[i].b == 1 && s[t[i].b] == c; }
    int next(int i) const { ++i; while (i < n && (t[i].t == T_WS || t[i].t == T_PP)) ++i; return i; }
    int prev(int i) const { --i; while (i >= 0 && (t[i].t == T_WS || t[i].t == T_PP)) --i; return i; }
    void word(int i, char *out, size_t cap) const {
        size_t l = t[i].e - t[i].b; if (l >= cap) l = cap - 1; memcpy(out, s + t[i].b, l); out[l] = 0;
    }
};

const char *const kEsOnly[] = {"sample", "patch", "buffer", "shared", "coherent", "volatile", "restrict", "readonly",
    "writeonly", "resource", "atomic_uint", "precise", "superp", "subroutine", "active", "common", "partition", "filter",
    "half", "fixed", "hvec2", "hvec3", "hvec4", "fvec2", "fvec3", "fvec4", "input", "output", "long", "short", "unsigned",
    "sizeof", "cast", "namespace", "using", "class", "union", "enum", "typedef", "template", "this", "goto", "inline",
    "noinline", "public", "static", "extern", "external", "interface", "asm", nullptr};
const char *const kIntFns[] = {"textureSize", "floatBitsToInt", "floatBitsToUint", "bitCount", "findLSB", "findMSB", nullptr};
const char *const kFloatFns[] = {"texture", "texture2D", "texture3D", "textureCube", "texture2DProj", "texture2DLod", "textureLod",
    "textureProj", "texelFetch", "shadow2D", "sin", "cos", "tan", "asin", "acos", "atan", "radians", "degrees", "pow", "exp", "log",
    "exp2", "log2", "sqrt", "inversesqrt", "floor", "ceil", "fract", "mod", "mix", "step", "smoothstep", "length", "distance", "dot",
    "cross", "normalize", "reflect", "refract", "faceforward", "dFdx", "dFdy", "fwidth", "intBitsToFloat", "uintBitsToFloat", nullptr};
const char *const kEs130[] = {"smooth", "flat", "switch", "case", "default", "uint", "uvec2", "uvec3", "uvec4", "lowp", "mediump",
    "highp", "precision", "texture", "textureLod", "textureProj", "textureGrad", "texelFetch", "textureSize", "textureOffset",
    "round", "trunc", "roundEven", "isnan", "isinf", "sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "modf", "layout", nullptr};
const char *const kIntTypes[] = {"int", "ivec2", "ivec3", "ivec4", "uint", "uvec2", "uvec3", "uvec4", nullptr};
const char *const kBoolTypes[] = {"bool", "bvec2", "bvec3", "bvec4", nullptr};
const char *const kFloatTypes[] = {"float", "vec2", "vec3", "vec4", "mat2", "mat3", "mat4", "mat2x2", "mat2x3", "mat2x4",
    "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3", "mat4x4", nullptr};
bool in_list(const char *w, size_t l, const char *const *lst) {
    for (; *lst; ++lst) if (strlen(*lst) == l && !strncmp(*lst, w, l)) return true;
    return false;
}
struct Sym { char name[48]; uint8_t kind; };      // kind: 1 int-like, 2 float-like, 3 function returning int, 4 function returning float
} // namespace

bool glsl_reserved(const char *name) {
    size_t l = strlen(name);
    return in_list(name, l, kEsOnly) || in_list(name, l, kEs130);
}

char *glsl_translate(const char *src, GLenum stage, ShaderInfo *info) {
    // ---- version check
    const char *v = strstr(src, "#version");
    int version = 110; bool es_src = false;
    if (v) {
        version = atoi(v + 8 + strspn(v + 8, " \t"));
        const char *eol = strchr(v, '\n');
        const char *es = strstr(v, " es");
        if (es && (!eol || es < eol)) es_src = true;
        if (version >= 300 && version != 330 && version < 400 && es_src) es_src = true;
    }
    if (es_src || version >= 300) {
        if (es_src) return nullptr;              // already GLSL ES
    }
    size_t len = strlen(src);
    int maxt = (int)len + 16;
    Tok *t = (Tok *)malloc(sizeof(Tok) * (size_t)maxt);
    if (!t) return nullptr;
    int n = tokenize(src, t, maxt);
    Ctx2 C{src, t, n};
    bool vs = stage == GL_VERTEX_SHADER;
    // ---- pass 1: symbols (int/float typed identifiers, function return types), builtin usage, attributes
    Sym *sym = (Sym *)calloc(512, sizeof(Sym)); int nsym = 0;
    auto add_sym = [&](int i, uint8_t kind) {
        if (nsym >= 512) return;
        size_t l = t[i].e - t[i].b; if (l >= sizeof sym[0].name) return;
        memcpy(sym[nsym].name, src + t[i].b, l); sym[nsym].name[l] = 0; sym[nsym].kind = kind; ++nsym;
    };
    auto find_sym = [&](int i) -> uint8_t {
        size_t l = t[i].e - t[i].b;
        for (int k = nsym - 1; k >= 0; --k) if (strlen(sym[k].name) == l && !strncmp(sym[k].name, src + t[i].b, l)) return sym[k].kind;
        return 0;
    };
    uint32_t used = 0; uint8_t mtex = 0; int fragdata_max = -1; bool fragcolor = false;
    uint16_t texcoord_const = 0; bool texcoord_dyn = false;
    info->nattrs = 0;
    for (int i = 0; i < n; ++i) {
        if (t[i].t != T_ID) continue;
        const char *w = src + t[i].b; size_t l = t[i].e - t[i].b;
        bool it = in_list(w, l, kIntTypes), ft = in_list(w, l, kFloatTypes), bt = in_list(w, l, kBoolTypes);
        if (it || ft || bt) {
            int j = C.next(i);
            if (C.op(j, '[')) { while (j < n && !C.op(j, ']')) ++j; j = C.next(j); }
            if (j < n && t[j].t == T_ID) {
                int k = C.next(j);
                if (C.op(k, '(')) add_sym(j, it ? 3 : bt ? 6 : 4);    // function definition/prototype
                else {
                    add_sym(j, it ? 1 : bt ? 5 : 2);
                    // further declarators: "float a = 1.0, b;" (skip initializers at depth 0)
                    int d = 0;
                    for (int m = k; m < n; m = C.next(m)) {
                        if (C.op(m, '(') || C.op(m, '[')) ++d;
                        else if (C.op(m, ')') || C.op(m, ']')) --d;
                        else if (d == 0 && (C.op(m, ';') || C.op(m, '{') || C.op(m, ')'))) break;
                        else if (d == 0 && C.op(m, ',')) { int q = C.next(m); if (q < n && t[q].t == T_ID) add_sym(q, it ? 1 : bt ? 5 : 2); }
                        if (d < 0) break;
                    }
                }
            }
        }
        if (vs && C.eq(i, "attribute")) {
            int j = C.next(i); while (j < n && t[j].t == T_ID && (C.eq(j, "highp") || C.eq(j, "mediump") || C.eq(j, "lowp"))) j = C.next(j);
            int k = C.next(j);
            if (k < n && t[k].t == T_ID && info->nattrs < 16) C.word(k, info->attrs[info->nattrs++], 64);
        }
        if (vs && version >= 130 && C.eq(i, "in")) {                   // GLSL 1.30+ vertex inputs at global scope
            int p = C.prev(i);
            if (p < 0 || C.op(p, ';') || C.op(p, '}') || t[p].t == T_PP) {
                int j = C.next(i); while (j < n && t[j].t == T_ID && (C.eq(j, "highp") || C.eq(j, "mediump") || C.eq(j, "lowp"))) j = C.next(j);
                int k = C.next(j);
                if (k < n && t[k].t == T_ID && C.op(C.next(k), ';') && info->nattrs < 16) C.word(k, info->attrs[info->nattrs++], 64);
            }
        }
        if (l > 3 && w[0] == 'g' && w[1] == 'l' && w[2] == '_') {
#define GBU(NAME, BIT) if (C.eq(i, NAME)) used |= BIT;
            GBU("gl_Vertex", GB_VERTEX) GBU("gl_Normal", GB_NORMAL) GBU("gl_FogCoord", GB_FOGCOORD)
            GBU("gl_ModelViewMatrix", GB_MV) GBU("gl_ProjectionMatrix", GB_P) GBU("gl_ModelViewProjectionMatrix", GB_MVP)
            GBU("gl_NormalMatrix", GB_NM) GBU("gl_TextureMatrix", GB_TM) GBU("gl_ModelViewMatrixInverse", GB_MVI)
            GBU("gl_ProjectionMatrixInverse", GB_PI) GBU("gl_ModelViewProjectionMatrixInverse", GB_MVPI) GBU("gl_Fog", GB_FOG)
#undef GBU
            if (C.eq(i, "ftransform")) used |= GB_MVP | GB_VERTEX;
            if (vs && C.eq(i, "gl_Color")) used |= GB_COLOR;
            if (vs && C.eq(i, "gl_SecondaryColor")) used |= GB_SECCOLOR;
            if (l == 17 && !strncmp(w, "gl_MultiTexCoord", 16) && is_dig(w[16])) mtex |= (uint8_t)(1u << (w[16] - '0'));
            if (C.eq(i, "gl_FragColor")) fragcolor = true;
            if (C.eq(i, "gl_FragData")) {
                int j = C.next(i), k = C.next(j);
                if (C.op(j, '[') && t[k].t == T_INT) { int ix = atoi(src + t[k].b); if (ix > fragdata_max) fragdata_max = ix; }
                else fragdata_max = 7;
            }
            if (C.eq(i, "gl_TexCoord")) {
                int j = C.next(i), k = C.next(j), m = C.next(k);
                if (C.op(j, '[') && t[k].t == T_INT && C.op(m, ']')) texcoord_const |= (uint16_t)(1u << (atoi(src + t[k].b) & 7));
                else texcoord_dyn = true;
            }
        }
        if (C.eq(i, "ftransform")) used |= GB_MVP | GB_VERTEX;
    }
    info->builtins = used; info->multitex = mtex;
    // ---- output
    Buf o;
    o.put("#version 320 es\n");
    for (int i = 0; i < n; ++i) {                               // hoisted #extension (require -> enable)
        if (t[i].t != T_PP) continue;
        const char *p = src + t[i].b + 1; p += strspn(p, " \t");
        if (strncmp(p, "extension", 9)) continue;
        char line[256]; size_t l = t[i].e - t[i].b; if (l >= sizeof line) l = sizeof line - 1;
        memcpy(line, src + t[i].b, l); line[l] = 0;
        if (char *r = strstr(line, "require")) memcpy(r, "enable ", 7);
        if (strstr(line, "GL" "_ARB_") || strstr(line, "GL" "_EXT_gpu_shader4") || strstr(line, "GL" "_NV_")) continue;   // desktop-only
        o.put(line); o.put("\n");
    }
    o.put("precision highp float;\nprecision highp int;\nprecision mediump sampler2D;\nprecision mediump sampler3D;\n"
          "precision mediump samplerCube;\nprecision mediump sampler2DShadow;\nprecision mediump sampler2DArray;\n");
    if (vs) {
        if (used & GB_VERTEX) o.putf("layout(location=%d) in vec4 oryon_Vertex;\n", LOC_POS);
        if (used & GB_NORMAL) o.putf("layout(location=%d) in vec3 oryon_Normal;\n", LOC_NORMAL);
        if (used & GB_COLOR) o.putf("layout(location=%d) in vec4 oryon_Color;\n", LOC_COLOR);
        if (used & GB_SECCOLOR) o.putf("layout(location=%d) in vec4 oryon_SecondaryColor;\n", LOC_COLOR2);
        if (used & GB_FOGCOORD) o.putf("layout(location=%d) in float oryon_FogCoord;\n", LOC_FOG);
        for (int u = 0; u < 8; ++u) if (mtex & (1u << u)) o.putf("layout(location=%d) in vec4 oryon_MultiTexCoord%d;\n", LOC_TEX0 + u, u);
        o.put("out vec4 oryon_FrontColor;\nout vec4 oryon_BackColor;\nout vec4 oryon_FrontSecondaryColor;\nout vec4 oryon_BackSecondaryColor;\n"
              "out float oryon_FogFragCoord;\n");
        for (int u = 0; u < 8; ++u) o.putf("out vec4 oryon_TexCoord%d;\n", u);
    } else {
        bool col = false, sec = false, fogc = false;
        for (int i = 0; i < n; ++i) if (t[i].t == T_ID) {
            if (C.eq(i, "gl_Color")) col = true; else if (C.eq(i, "gl_SecondaryColor")) sec = true; else if (C.eq(i, "gl_FogFragCoord")) fogc = true;
        }
        if (col) o.put("in vec4 oryon_FrontColor;\n");
        if (sec) o.put("in vec4 oryon_FrontSecondaryColor;\n");
        if (fogc) o.put("in float oryon_FogFragCoord;\n");
        for (int u = 0; u < 8; ++u) if ((texcoord_const & (1u << u)) || texcoord_dyn) o.putf("in vec4 oryon_TexCoord%d;\n", u);
        if (fragcolor) o.put("layout(location=0) out vec4 oryon_FragColor;\n");
        if (fragdata_max >= 0) o.putf("out vec4 oryon_FragData[%d];\n", fragdata_max + 1);
    }
    if (used & GB_MV) o.put("uniform mat4 oryon_ModelViewMatrix;\n");
    if (used & GB_P) o.put("uniform mat4 oryon_ProjectionMatrix;\n");
    if (used & GB_MVP) o.put("uniform mat4 oryon_ModelViewProjectionMatrix;\n");
    if (used & GB_NM) o.put("uniform mat3 oryon_NormalMatrix;\n");
    if (used & GB_TM) o.put("uniform mat4 oryon_TextureMatrix[8];\n");
    if (used & GB_MVI) o.put("uniform mat4 oryon_ModelViewMatrixInverse;\n");
    if (used & GB_PI) o.put("uniform mat4 oryon_ProjectionMatrixInverse;\n");
    if (used & GB_MVPI) o.put("uniform mat4 oryon_ModelViewProjectionMatrixInverse;\n");
    if (used & GB_FOG) o.put("struct oryon_FogParameters { vec4 color; float density; float start; float end; float scale; };\nuniform oryon_FogParameters oryon_Fog;\n");
    o.put("#line 1\n");
    // ---- pass 2: rewrite
    struct Call { int depth; bool intctx; bool floatfn; int arg; int wrap_close; };
    Call calls[64]; int ncalls = 0; int depth = 0, brk = 0;
    uint8_t ret_kind = 2;                          // current function return type (1 int, 2 float)
    uint8_t decl_kind = 0;                         // declaration statement base type
    uint8_t target_kind = 0;                       // kind of the assignment target of the current statement
    int uni_depth = -1;                            // stripping a uniform initializer
    bool in_uniform = false; char uni_name[64] = {0}; char uni_type[16] = {0};
    int ndefs = 0; UniformDefault defs[32];
    GLfloat defv[16]; int ndefv = 0; bool def_ok = true;
    uint8_t *lit = (uint8_t *)calloc((size_t)n + 1, 1);   // decided kind of integer literals (1 int, 2 float)
    auto fn_kind = [&](int f) -> uint8_t {
        if (f < 0 || t[f].t != T_ID) return 0;
        const char *fw = src + t[f].b; size_t fl = t[f].e - t[f].b;
        if (in_list(fw, fl, kIntTypes) || in_list(fw, fl, kIntFns)) return 1;
        if (in_list(fw, fl, kFloatTypes) || in_list(fw, fl, kFloatFns)) return 2;
        uint8_t k = find_sym(f);
        return k == 3 ? 1 : k == 4 ? 2 : 0;
    };
    // kind (1 int, 2 float, 0 unknown) of the operand whose LAST token is i
    struct KindFn { static uint8_t run(const Ctx2 &C, int i, uint8_t *lit, const decltype(fn_kind) &fk,
                                       const decltype(find_sym) &fs, int guard) {
        const Tok *t = C.t; const char *src = C.s;
        if (i < 0 || i >= C.n || guard > 32) return 0;
        if (t[i].t == T_FLOAT) return 2;
        if (t[i].t == T_INT) return lit[i];
        if (C.op(i, ')') || C.op(i, ']')) {
            char open = C.op(i, ')') ? '(' : '[', close = src[t[i].b];
            int d = 0, j = i;
            for (; j >= 0; j = C.prev(j)) {
                if (t[j].t == T_OP && src[t[j].b] == close && t[j].e - t[j].b == 1) ++d;
                else if (t[j].t == T_OP && src[t[j].b] == open && t[j].e - t[j].b == 1) { if (--d == 0) break; }
            }
            if (j < 0) return 0;
            int f = C.prev(j);
            if (open == '[') return run(C, f, lit, fk, fs, guard + 1);            // element of an array / vector
            if (f >= 0 && t[f].t == T_ID) { uint8_t k = fk(f); if (k) return k; }
            if (f >= 0 && t[f].t == T_ID) return 0;
            return run(C, C.prev(i), lit, fk, fs, guard + 1);                     // parenthesized expression
        }
        if (t[i].t == T_ID) {
            int p = C.prev(i);
            if (C.op(p, '.')) return run(C, C.prev(p), lit, fk, fs, guard + 1);    // swizzle / member: base type
            if (C.eq(i, "gl_VertexID") || C.eq(i, "gl_InstanceID")) return 1;
            uint8_t k = fs(i); if (k == 1 || k == 3) return 1; if (k == 2 || k == 4) return 2;
            if (t[i].e - t[i].b > 3 && !strncmp(src + t[i].b, "gl_", 3)) return 2;
            return 0;
        }
        return 0;
    } };
    auto kind_at = [&](int i) -> uint8_t { return KindFn::run(C, i, lit, fn_kind, find_sym, 0); };
    auto is_binop = [&](int i) -> bool {
        if (i < 0 || t[i].t != T_OP) return false;
        char c = src[t[i].b]; size_t l = t[i].e - t[i].b;
        if (l == 1) return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '<' || c == '>' || c == '=' || c == '&' || c == '|' || c == '^' || c == '?' || c == ':';
        return !(c == '+' && src[t[i].b + 1] == '+') && !(c == '-' && src[t[i].b + 1] == '-') && c != '&' && c != '|' && c != '^';
    };
    for (int i = 0; i < n; ++i) {
        const Tok &k = t[i];
        const char *w = src + k.b; size_t l = k.e - k.b;
        if (k.t == T_PP) {
            const char *p = w + 1; p += strspn(p, " \t");
            if (!strncmp(p, "version", 7) || !strncmp(p, "extension", 9)) { o.put("\n"); continue; }
            o.put(w, l); continue;
        }
        if (k.t == T_WS) {                                          // keep newlines for line numbering
            for (size_t q = 0; q < l; ++q) if (w[q] == '\n') o.put("\n"); else if (w[q] == ' ' || w[q] == '\t') o.put(" ");
            continue;
        }
        if (k.t == T_OP) {
            char c = w[0];
            if (uni_depth >= 0) {                                   // inside a stripped uniform initializer
                if (c == '(' ) ++depth;
                else if (c == ')') --depth;
                else if (c == ';' && depth == uni_depth) {
                    uni_depth = -1;
                    if (def_ok && ndefv > 0 && ndefs < 32) {
                        UniformDefault &d = defs[ndefs++];
                        memset(&d, 0, sizeof d);
                        strncpy(d.name, uni_name, sizeof d.name - 1);
                        d.count = ndefv; memcpy(d.v, defv, sizeof(GLfloat) * (size_t)ndefv);
                        strncpy(d.tname, uni_type, sizeof d.tname - 1);
                    }
                    o.put(";");
                }
                if (c == '-' && i + 1 < n && (t[C.next(i)].t == T_INT || t[C.next(i)].t == T_FLOAT)) {
                    int j = C.next(i); if (ndefv < 16) defv[ndefv++] = -(GLfloat)atof(src + t[j].b); i = j;
                }
                continue;
            }
            if (c == '(') {
                ++depth;
                int p = C.prev(i);
                if (ncalls < 64) {
                    Call cl{depth, false, false, 0, 0};
                    if (p >= 0 && t[p].t == T_ID) {
                        const char *pw = src + t[p].b; size_t pl = t[p].e - t[p].b;
                        cl.intctx = in_list(pw, pl, kIntTypes) || C.eq(p, "texelFetch") || C.eq(p, "textureSize") || find_sym(p) == 3;
                        cl.floatfn = !cl.intctx;
                        if (C.eq(p, "shadow2D") || C.eq(p, "shadow2DProj")) cl.wrap_close = 1;
                    }
                    calls[ncalls++] = cl;
                }
            } else if (c == ')') {
                if (ncalls && calls[ncalls - 1].depth == depth) { if (calls[ncalls - 1].wrap_close) o.put(")"); --ncalls; }
                --depth;
            } else if (c == '[') ++brk;
            else if (c == ']') --brk;
            else if (c == ',' && ncalls && calls[ncalls - 1].depth == depth) calls[ncalls - 1].arg++;
            else if (c == ';') { decl_kind = 0; target_kind = 0; in_uniform = false; }
            else if (c == '{' && depth == 0) decl_kind = 0;
            if (depth == 0 && ((c == '=' && l == 1) || ((c == '+' || c == '-' || c == '*' || c == '/') && l == 2 && w[1] == '=')))
                target_kind = decl_kind ? decl_kind : kind_at(C.prev(i));
            if (c == '=' && in_uniform && depth == 0 && l == 1) {      // uniform initializer: strip, remember value
                uni_depth = depth; ndefv = 0; def_ok = true;
                continue;
            }
            o.put(w, l); continue;
        }
        if (k.t == T_INT) {
            if (uni_depth >= 0) { if (ndefv < 16) defv[ndefv++] = (GLfloat)atof(w); continue; }
            bool keep = false;
            if (w[0] == '0' && l > 1 && (w[1] == 'x' || w[1] == 'X')) keep = true;
            else if (w[l - 1] == 'u' || w[l - 1] == 'U') keep = true;
            else if (brk > 0) keep = true;
            else {
                int p = C.prev(i), q = C.next(i);
                if (p >= 0 && (C.eq(p, "case") || C.eq(p, "return"))) keep = C.eq(p, "case") || ret_kind == 1;
                else {
                    // operand on the other side of an adjacent binary operator
                    uint8_t other = 0;
                    int pp = p;
                    if (pp >= 0 && (C.op(pp, '-') || C.op(pp, '+'))) {       // unary sign?
                        int p2 = C.prev(pp);
                        if (p2 < 0 || (t[p2].t == T_OP && !C.op(p2, ')') && !C.op(p2, ']'))) pp = p2;
                    }
                    if (pp >= 0 && is_binop(pp) && !C.op(pp, '?')) {
                        int a = C.prev(pp);
                        if (C.op(pp, '=') && decl_kind) other = decl_kind;
                        else other = kind_at(a);
                        if (!other && a >= 0 && C.op(a, ']')) {               // array element: element type of the array
                            int b2 = a; int d2 = 0;
                            while (b2 >= 0) { if (C.op(b2, ']')) ++d2; else if (C.op(b2, '[')) { if (--d2 == 0) break; } --b2; }
                            other = kind_at(C.prev(b2));
                        }
                    }
                    if (!other && q < n && is_binop(q) && !C.op(q, '=') && !C.op(q, '?') && !C.op(q, ':')) other = kind_at(C.next(q));
                    if (other == 1) keep = true;
                    else if (other == 2) keep = false;
                    else if (ncalls && calls[ncalls - 1].depth == depth && calls[ncalls - 1].intctx) keep = true;
                    else if (decl_kind == 1 && depth == 0) keep = true;
                    else if (p >= 0 && C.op(p, '(') && ncalls && calls[ncalls - 1].depth == depth && !calls[ncalls - 1].floatfn) keep = true;
                    else {
                        // min/max/clamp/abs/sign: integer if a sibling argument is integer, else the statement target type
                        keep = false;
                        if (ncalls && calls[ncalls - 1].depth == depth) {
                            int d = 0, open = -1;
                            for (int m = C.prev(i); m >= 0; m = C.prev(m)) {
                                if (C.op(m, ')')) ++d; else if (C.op(m, '(')) { if (d == 0) { open = m; break; } --d; }
                                else if (d == 0 && kind_at(m) == 1 && t[m].t == T_ID) { keep = true; break; }
                            }
                            int f = open >= 0 ? C.prev(open) : -1;
                            bool amb = f >= 0 && (C.eq(f, "max") || C.eq(f, "min") || C.eq(f, "clamp") || C.eq(f, "abs") || C.eq(f, "sign"));
                            if (!keep && amb && target_kind == 1) keep = true;
                        }
                    }
                }
            }
            lit[i] = keep ? 1 : 2;
            o.put(w, l);
            if (!keep) o.put(".0");
            continue;
        }
        if (k.t == T_FLOAT) {
            if (uni_depth >= 0) { if (ndefv < 16) defv[ndefv++] = (GLfloat)atof(w); continue; }
            o.put(w, l); continue;
        }
        // identifiers
        if (uni_depth >= 0) {
            if (!(in_list(w, l, kFloatTypes) || in_list(w, l, kIntTypes) || in_list(w, l, kBoolTypes))) def_ok = false;
            continue;
        }
        if (in_list(w, l, kIntTypes) || in_list(w, l, kFloatTypes) || in_list(w, l, kBoolTypes)) {
            uint8_t kk = in_list(w, l, kIntTypes) ? 1 : in_list(w, l, kBoolTypes) ? 0 : 2;
            int j = C.next(i);
            if (j < n && t[j].t == T_ID && C.op(C.next(j), '(') && depth == 0) ret_kind = kk;   // function definition
            else if (depth == 0 || decl_kind == 0) decl_kind = kk;
            if (in_uniform && depth == 0) { size_t cl = l < sizeof uni_type - 1 ? l : sizeof uni_type - 1; memcpy(uni_type, w, cl); uni_type[cl] = 0; }
        }
        if (C.eq(i, "uniform") && depth == 0) { in_uniform = true; }
        else if (in_uniform && depth == 0 && C.op(C.next(i), '=')) { C.word(i, uni_name, sizeof uni_name); }
        if (C.eq(i, "attribute")) { o.put(vs ? "in" : "attribute"); continue; }
        if (C.eq(i, "varying")) { o.put(vs ? "out" : "in"); continue; }
        if (C.eq(i, "noperspective")) continue;
        if (C.eq(i, "texture2D") || C.eq(i, "texture3D") || C.eq(i, "textureCube") || C.eq(i, "texture2DRect") || C.eq(i, "shadow2D")) { o.put("texture"); continue; }
        if (C.eq(i, "texture2DProj") || C.eq(i, "texture3DProj") || C.eq(i, "shadow2DProj") || C.eq(i, "texture2DRectProj")) { o.put("textureProj"); continue; }
        if (C.eq(i, "texture2DLod") || C.eq(i, "texture3DLod") || C.eq(i, "textureCubeLod")) { o.put("textureLod"); continue; }
        if (C.eq(i, "texture2DProjLod")) { o.put("textureProjLod"); continue; }
        if (C.eq(i, "texture2DGradARB") || C.eq(i, "texture2DGrad")) { o.put("textureGrad"); continue; }
        if (C.eq(i, "sampler2DRect")) { o.put("sampler2D"); continue; }
        if (C.eq(i, "ftransform")) {                                         // ftransform() -> (MVP * vertex)
            int a = C.next(i), b = C.next(a);
            if (C.op(a, '(') && C.op(b, ')')) { o.put("(oryon_ModelViewProjectionMatrix * oryon_Vertex)"); i = b; continue; }
        }
        if (l > 3 && w[0] == 'g' && w[1] == 'l' && w[2] == '_') {
            if (C.eq(i, "gl_FragColor")) { o.put("oryon_FragColor"); continue; }
            if (C.eq(i, "gl_FragData")) { o.put("oryon_FragData"); continue; }
            if (C.eq(i, "gl_TexCoord")) {
                int j = C.next(i), q = C.next(j), m = C.next(q);
                if (C.op(j, '[') && t[q].t == T_INT && C.op(m, ']')) { o.putf("oryon_TexCoord%d", atoi(src + t[q].b) & 7); i = m; continue; }
                o.put("oryon_TexCoord0"); continue;
            }
            if (C.eq(i, "gl_Color")) { o.put(vs ? "oryon_Color" : "oryon_FrontColor"); continue; }
            if (C.eq(i, "gl_SecondaryColor")) { o.put(vs ? "oryon_SecondaryColor" : "oryon_FrontSecondaryColor"); continue; }
            if (C.eq(i, "gl_ClipVertex")) { o.put("oryon_FrontSecondaryColor"); continue; }   // write-only sink
            static const char *const kMap[] = {"gl_Vertex", "gl_Normal", "gl_FogCoord", "gl_FrontColor", "gl_BackColor",
                "gl_FrontSecondaryColor", "gl_BackSecondaryColor", "gl_FogFragCoord", "gl_ModelViewMatrix", "gl_ProjectionMatrix",
                "gl_ModelViewProjectionMatrix", "gl_NormalMatrix", "gl_TextureMatrix", "gl_ModelViewMatrixInverse",
                "gl_ProjectionMatrixInverse", "gl_ModelViewProjectionMatrixInverse", "gl_Fog", nullptr};
            bool done = false;
            for (const char *const *m = kMap; *m; ++m) if (C.eq(i, *m)) { o.put("oryon_"); o.put(w + 3, l - 3); done = true; break; }
            if (!done && l == 17 && !strncmp(w, "gl_MultiTexCoord", 16)) { o.put("oryon_"); o.put(w + 3, l - 3); done = true; }
            if (done) continue;
        }
        if (in_list(w, l, kEsOnly) || (version < 130 && in_list(w, l, kEs130))) o.put("oryon_");
        bool wrap = false;
        if (brk == 0 && find_sym(i) == 1) {
            int p = C.prev(i), q = C.next(i);
            bool simple = !C.op(p, '.') && !C.op(q, '[') && !C.op(q, '.') && !C.op(q, '(') && !(ncalls && calls[ncalls - 1].depth == depth && calls[ncalls - 1].intctx);
            auto arith = [&](int x) { return x >= 0 && x < n && t[x].t == T_OP && is_binop(x) && !C.op(x, '?') && !C.op(x, ':') && !C.op(x, '%') &&
                                             !C.op(x, '&') && !C.op(x, '|') && !C.op(x, '^') && !C.eq(x, "<<") && !C.eq(x, ">>"); };
            if (simple) {
                if (arith(p)) {
                    bool asg = C.op(p, '=') || C.eq(p, "+=") || C.eq(p, "-=") || C.eq(p, "*=") || C.eq(p, "/=");
                    uint8_t other = asg ? target_kind : kind_at(C.prev(p));
                    if (other == 2) wrap = true;
                }
                if (!wrap && arith(q) && !C.op(q, '=')) {
                    int r = C.next(q);
                    uint8_t other = t[r].t == T_FLOAT ? 2 : (t[r].t == T_ID && C.op(C.next(r), '(')) ? fn_kind(r) :
                                    (t[r].t == T_ID && !C.op(C.next(r), '[') && !C.op(C.next(r), '.')) ? kind_at(r) : 0;
                    if (other == 2) wrap = true;
                }
            }
        }
        if (wrap) o.put("float(");
        o.put(w, l);
        if (wrap) o.put(")");
    }
    free(t); free(sym); free(lit);
    if (ndefs) {
        info->defs = (UniformDefault *)malloc(sizeof(UniformDefault) * (size_t)ndefs);
        if (info->defs) { memcpy(info->defs, defs, sizeof(UniformDefault) * (size_t)ndefs); info->ndefs = ndefs; }
    }
    info->translated = true;
    return o.p;
}
} // namespace ory
