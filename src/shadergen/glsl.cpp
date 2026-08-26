/* Pembangkit sumber GLSL ES 320 dari FFKey.

   Yang TIDAK ditulis sama sekali bila state tidak memintanya: pencahayaan,
   kabut, uji alpha, texgen, matriks tekstur, bahkan ruang mata. Itulah arti
   "minim emulasi" di sini - shader untuk gambar terrain biasa hanya berisi
   satu perkalian matriks dan satu pengambilan tekstur. */

#include "shadergen.h"
#include <string.h>

namespace oryon {
namespace {

struct Buf {
    char *p;
    char *end;
    bool  ovf;
};

inline void put(Buf &b, const char *s) {
    while (*s) {
        if (b.p >= b.end) { b.ovf = true; return; }
        *b.p++ = *s++;
    }
}

inline void putd(Buf &b, int d) {            /* 0..9 saja; indeks kita kecil */
    if (b.p >= b.end) { b.ovf = true; return; }
    *b.p++ = (char) ('0' + d);
}

/* "nama<i>" */
inline void putn(Buf &b, const char *s, int i) { put(b, s); putd(b, i); }

inline bool bit(unsigned mask, int i) { return (mask >> i) & 1u; }

/* Ruang mata dibutuhkan oleh kabut, pencahayaan, dan texgen EYE_LINEAR. */
inline bool needs_eye(const FFKey &k) {
    return (k.flags & (FF_FOG | FF_LIGHTING)) != 0 ||
           (k.tex_gen & k.tex_gen_eye) != 0;
}

const char *const kAlphaOp[8] = {
    /* GL_NEVER    */ "false",
    /* GL_LESS     */ "c.a <  u_alphaRef",
    /* GL_EQUAL    */ "c.a == u_alphaRef",
    /* GL_LEQUAL   */ "c.a <= u_alphaRef",
    /* GL_GREATER  */ "c.a >  u_alphaRef",
    /* GL_NOTEQUAL */ "c.a != u_alphaRef",
    /* GL_GEQUAL   */ "c.a >= u_alphaRef",
    /* GL_ALWAYS   */ "true"
};

}  /* namespace */

unsigned ffp_gen_vertex(const FFKey &k, char *out, unsigned cap) {
    Buf b = { out, out + cap - 1, false };
    const bool eye = needs_eye(k);
    const bool lit = (k.flags & FF_LIGHTING) != 0;

    put(b, "#version 320 es\n");

    put(b, "layout(location=0) in vec4 a_pos;\n");
    if (k.flags & FF_ATTR_COLOR)  put(b, "layout(location=1) in vec4 a_color;\n");
    if (k.flags & FF_ATTR_NORMAL) put(b, "layout(location=2) in vec3 a_normal;\n");
    for (int u = 0; u < FF_MAX_TEX; ++u)
        if (bit(k.attr_tex, u)) {
            put(b, "layout(location="); putd(b, 3 + u);
            put(b, ") in vec4 a_tex"); putd(b, u); put(b, ";\n");
        }

    put(b, "uniform mat4 u_mvp;\n");
    if (eye) put(b, "uniform mat4 u_mv;\n");
    if (!(k.flags & FF_ATTR_COLOR)) put(b, "uniform vec4 u_color;\n");
    if (lit) {
        put(b, "uniform mat3 u_nrm;\nuniform vec4 u_lmAmbient;\n");
        for (int i = 0; i < FF_MAX_LIGHTS; ++i) {
            if (!bit(k.light_mask, i)) continue;
            putn(b, "uniform vec4 u_lightPos", i); put(b, ";\n");
            putn(b, "uniform vec4 u_lightDiff", i); put(b, ";\n");
            putn(b, "uniform vec4 u_lightAmb", i); put(b, ";\n");
        }
    }
    for (int u = 0; u < FF_MAX_TEX; ++u) {
        if (!bit(k.tex_enable, u)) continue;
        if (bit(k.tex_matrix, u)) { putn(b, "uniform mat4 u_texMat", u); put(b, ";\n"); }
        if (bit(k.tex_gen, u)) {
            putn(b, "uniform vec4 u_tgS", u); put(b, ";\n");
            putn(b, "uniform vec4 u_tgT", u); put(b, ";\n");
        }
    }

    put(b, (k.flags & FF_FLAT) ? "flat out vec4 v_color;\n" : "out vec4 v_color;\n");
    for (int u = 0; u < FF_MAX_TEX; ++u)
        if (bit(k.tex_enable, u)) { putn(b, "out vec2 v_tex", u); put(b, ";\n"); }
    if (k.flags & FF_FOG) put(b, "out float v_fogDist;\n");

    put(b, "void main() {\n");
    if (eye) put(b, "  vec4 eye = u_mv * a_pos;\n");
    put(b, "  gl_Position = u_mvp * a_pos;\n");
    put(b, (k.flags & FF_ATTR_COLOR) ? "  vec4 c = a_color;\n" : "  vec4 c = u_color;\n");

    if (lit) {
        /* GL_COLOR_MATERIAL dengan GL_AMBIENT_AND_DIFFUSE - satu-satunya mode
           yang dipakai Minecraft. Spekular sengaja tidak ada: RenderHelper
           menyetelnya hitam, jadi menuliskannya hanya membakar ALU. */
        put(b, "  vec3 n = ");
        if (k.flags & FF_ATTR_NORMAL) put(b, "u_nrm * a_normal;\n");
        else                          put(b, "vec3(0.0, 0.0, 1.0);\n");
        if (k.flags & FF_NORMALIZE)   put(b, "  n = normalize(n);\n");
        put(b, "  vec3 lit = u_lmAmbient.rgb * c.rgb;\n");
        for (int i = 0; i < FF_MAX_LIGHTS; ++i) {
            if (!bit(k.light_mask, i)) continue;
            putn(b, "  {\n    vec3 L = (u_lightPos", i);
            putn(b, ".w == 0.0) ? normalize(u_lightPos", i);
            putn(b, ".xyz) : normalize(u_lightPos", i);
            put(b, ".xyz - eye.xyz);\n");
            put(b, "    float d = max(dot(n, L), 0.0);\n");
            putn(b, "    lit += u_lightAmb", i);
            putn(b, ".rgb * c.rgb + u_lightDiff", i);
            put(b, ".rgb * c.rgb * d;\n  }\n");
        }
        put(b, "  c = vec4(min(lit, vec3(1.0)), c.a);\n");
    }
    put(b, "  v_color = c;\n");

    for (int u = 0; u < FF_MAX_TEX; ++u) {
        if (!bit(k.tex_enable, u)) continue;
        put(b, "  {\n    vec4 t = ");
        if (bit(k.tex_gen, u)) {
            const bool e = bit(k.tex_gen_eye, u);
            put(b, "vec4(dot(u_tgS"); putd(b, u); put(b, e ? ", eye)" : ", a_pos)");
            put(b, ", dot(u_tgT"); putd(b, u); put(b, e ? ", eye)" : ", a_pos)");
            put(b, ", 0.0, 1.0);\n");
        } else if (bit(k.attr_tex, u)) {
            putn(b, "a_tex", u); put(b, ";\n");
        } else {
            put(b, "vec4(0.0, 0.0, 0.0, 1.0);\n");
        }
        if (bit(k.tex_matrix, u)) { putn(b, "    t = u_texMat", u); put(b, " * t;\n"); }
        putn(b, "    v_tex", u); put(b, " = t.xy;\n  }\n");
    }

    if (k.flags & FF_FOG) put(b, "  v_fogDist = length(eye.xyz);\n");
    put(b, "}\n");

    if (b.ovf) return 0;
    *b.p = 0;
    return (unsigned) (b.p - out);
}

unsigned ffp_gen_fragment(const FFKey &k, char *out, unsigned cap) {
    Buf b = { out, out + cap - 1, false };

    put(b, "#version 320 es\nprecision highp float;\nprecision highp sampler2D;\n");
    put(b, (k.flags & FF_FLAT) ? "flat in vec4 v_color;\n" : "in vec4 v_color;\n");
    for (int u = 0; u < FF_MAX_TEX; ++u)
        if (bit(k.tex_enable, u)) {
            putn(b, "in vec2 v_tex", u); put(b, ";\n");
            putn(b, "uniform sampler2D u_tex", u); put(b, ";\n");
        }
    if (k.flags & FF_FOG)
        put(b, "in float v_fogDist;\nuniform vec4 u_fogColor;\n"
               "uniform float u_fogStart;\nuniform float u_fogEnd;\n"
               "uniform float u_fogDensity;\n");
    if (k.flags & FF_ALPHA_TEST) put(b, "uniform float u_alphaRef;\n");
    put(b, "layout(location=0) out vec4 fragColor;\n");

    put(b, "void main() {\n  vec4 c = v_color;\n");
    for (int u = 0; u < FF_MAX_TEX; ++u) {
        if (!bit(k.tex_enable, u)) continue;
        put(b, "  {\n    vec4 t = texture(u_tex"); putd(b, u);
        putn(b, ", v_tex", u); put(b, ");\n");
        put(b, bit(k.tex_replace, u) ? "    c = t;\n" : "    c *= t;\n");
        put(b, "  }\n");
    }
    if (k.flags & FF_ALPHA_TEST) {
        put(b, "  if (!("); put(b, kAlphaOp[k.alpha_func & 7]); put(b, ")) discard;\n");
    }
    if (k.flags & FF_FOG) {
        put(b, "  float f;\n");
        switch (k.fog_mode) {
        case FOG_EXP:
            put(b, "  f = exp(-u_fogDensity * v_fogDist);\n"); break;
        case FOG_EXP2:
            put(b, "  f = exp(-(u_fogDensity * v_fogDist) * "
                   "(u_fogDensity * v_fogDist));\n"); break;
        default:
            put(b, "  f = (u_fogEnd - v_fogDist) / (u_fogEnd - u_fogStart);\n"); break;
        }
        put(b, "  c.rgb = mix(u_fogColor.rgb, c.rgb, clamp(f, 0.0, 1.0));\n");
    }
    put(b, "  fragColor = c;\n}\n");

    if (b.ovf) return 0;
    *b.p = 0;
    return (unsigned) (b.p - out);
}

}  /* namespace oryon */
