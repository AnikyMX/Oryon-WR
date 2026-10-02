// Oryon -- fixed-function emulation: canonical state key -> generated GLSL ES 3.20 -> cached program.
// Uniforms are uploaded only when their state version changed since this program last saw it.
#include "oryon.hpp"
#include "mathx.hpp"
#include "bind.hpp"
#include "ffp_prog.hpp"
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ory {
bool g_prog_points = false;

// ------------------------------------------------------------------ string builder (no STL)
namespace {
struct SB {
    char *p; size_t n, cap;
    void add(const char *s) { size_t l = strlen(s); if (n + l + 1 > cap) return; memcpy(p + n, s, l); n += l; p[n] = 0; }
    void addf(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
        va_list ap; va_start(ap, fmt);
        int l = vsnprintf(p + n, cap - n, fmt, ap);
        va_end(ap);
        if (l > 0 && n + (size_t)l < cap) n += (size_t)l; else p[n] = 0;
    }
};
char g_vs[32768], g_fs[32768];

enum { ENV_OFF = 0, ENV_REPLACE, ENV_MODULATE, ENV_DECAL, ENV_BLEND, ENV_ADD, ENV_COMBINE };
enum { GEN_OBJ = 1, GEN_EYE, GEN_SPHERE, GEN_NORMAL, GEN_REFLECT };

uint32_t env_code(GLenum m) {
    switch (m) { case GL_REPLACE: return ENV_REPLACE; case GL_DECAL: return ENV_DECAL; case GL_BLEND: return ENV_BLEND;
    case GL_ADD: return ENV_ADD; case GL_COMBINE: return ENV_COMBINE; default: return ENV_MODULATE; }
}
uint32_t gen_code(GLenum m) {
    switch (m) { case GL_OBJECT_LINEAR: return GEN_OBJ; case GL_SPHERE_MAP: return GEN_SPHERE; case GL_NORMAL_MAP: return GEN_NORMAL;
    case GL_REFLECTION_MAP: return GEN_REFLECT; default: return GEN_EYE; }
}
uint64_t comb_func(GLenum f) {
    switch (f) { case GL_REPLACE: return 0; case GL_MODULATE: return 1; case GL_ADD: return 2; case GL_ADD_SIGNED: return 3;
    case GL_INTERPOLATE: return 4; case GL_SUBTRACT: return 5; case GL_DOT3_RGB: return 6; case GL_DOT3_RGBA: return 7; default: return 1; }
}
uint64_t comb_src(GLenum s) {
    if (s >= GL_TEXTURE0 && s < GL_TEXTURE0 + MAX_TEX_UNITS) return 4 + (s - GL_TEXTURE0);
    switch (s) { case GL_CONSTANT: return 1; case GL_PRIMARY_COLOR: return 2; case GL_PREVIOUS: return 3; default: return 0; }
}
uint64_t comb_op(GLenum o) {
    switch (o) { case GL_ONE_MINUS_SRC_COLOR: return 1; case GL_SRC_ALPHA: return 2; case GL_ONE_MINUS_SRC_ALPHA: return 3; default: return 0; }
}
uint64_t scale_code(GLfloat s) { return s >= 3.0f ? 2 : s >= 1.5f ? 1 : 0; }

// Canonical form: keys whose generated shaders are equivalent become identical, so fewer programs are compiled.
//  - fog: the equation (linear/exp/exp2) is selected by a uniform, so the mode is not part of the key;
//  - COMBINE: TEXTUREn of the unit itself = TEXTURE, PRIMARY_COLOR on the first enabled unit = PREVIOUS, unused
//    arguments are zeroed, commutative arguments are ordered, and configurations equal to MODULATE / REPLACE become
//    those modes (Minecraft's RenderLivingBase.unsetBrightness leaves units 0/1 in such a COMBINE state).
void canon_key(FfpKey &k) {
    k.fog_mode = 0;
    int first = -1;
    for (int u = 0; u < k.nunits; ++u) {
        uint32_t w = k.tu[u];
        if (!(w & 7)) { k.comb[u] = 0; continue; }
        if (first < 0) first = u;
        if (((w >> 3) & 7) != ENV_COMBINE) { k.comb[u] = 0; continue; }
        const uint64_t c = k.comb[u];
        uint32_t fr = c & 7, fa = (c >> 3) & 7, scr = (c >> 39) & 3, sca = (c >> 41) & 3;
        uint32_t sr[3], sa[3], orr[3], oa[3];
        for (int i = 0; i < 3; ++i) {
            sr[i] = (c >> (6 + 4 * i)) & 15; sa[i] = (c >> (18 + 4 * i)) & 15;
            orr[i] = (c >> (30 + 2 * i)) & 3; oa[i] = (c >> (36 + i)) & 1;
            if (sr[i] == 4u + (uint32_t)u) sr[i] = 0;               // TEXTUREn of this unit == TEXTURE
            if (sa[i] == 4u + (uint32_t)u) sa[i] = 0;
            if (u == first && sr[i] == 2) sr[i] = 3;                // PRIMARY_COLOR == PREVIOUS on the first unit
            if (u == first && sa[i] == 2) sa[i] = 3;
        }
        auto nargs = [](uint32_t f) { return f == 0 ? 1 : f == 4 ? 3 : 2; };
        if (fr == 7) { fa = 0; sca = 0; for (int i = 0; i < 3; ++i) { sa[i] = 0; oa[i] = 0; } }   // DOT3_RGBA: alpha from RGB
        for (int i = nargs(fr); i < 3; ++i) { sr[i] = 0; orr[i] = 0; }
        if (fr != 7) for (int i = nargs(fa); i < 3; ++i) { sa[i] = 0; oa[i] = 0; }
        auto comm = [](uint32_t f) { return f == 1 || f == 2 || f == 3 || f == 6 || f == 7; };
        if (comm(fr) && (sr[1] << 2 | orr[1]) < (sr[0] << 2 | orr[0])) { uint32_t t = sr[0]; sr[0] = sr[1]; sr[1] = t; t = orr[0]; orr[0] = orr[1]; orr[1] = t; }
        if (fr != 7 && comm(fa) && (sa[1] << 1 | oa[1]) < (sa[0] << 1 | oa[0])) { uint32_t t = sa[0]; sa[0] = sa[1]; sa[1] = t; t = oa[0]; oa[0] = oa[1]; oa[1] = t; }
        const uint32_t fmt = (w >> 6) & 3;
        const bool modulate = fr == 1 && fa == 1 && !scr && !sca && sr[0] == 0 && sr[1] == 3 && !orr[0] && !orr[1] &&
                              sa[0] == 0 && sa[1] == 3 && !oa[0] && !oa[1];
        const bool replace = fr == 0 && fa == 0 && !scr && !sca && sr[0] == 0 && !orr[0] && sa[0] == 0 && !oa[0];
        if (modulate && fmt != FMT_ALPHA) { k.tu[u] = (w & ~(7u << 3)) | (ENV_MODULATE << 3); k.comb[u] = 0; continue; }
        if (replace && (fmt == FMT_RGBA || fmt == FMT_INTENSITY)) { k.tu[u] = (w & ~(7u << 3)) | (ENV_REPLACE << 3); k.comb[u] = 0; continue; }
        uint64_t n = (uint64_t)fr | ((uint64_t)fa << 3) | ((uint64_t)scr << 39) | ((uint64_t)sca << 41);
        for (int i = 0; i < 3; ++i)
            n |= ((uint64_t)sr[i] << (6 + 4 * i)) | ((uint64_t)sa[i] << (18 + 4 * i)) | ((uint64_t)orr[i] << (30 + 2 * i)) | ((uint64_t)oa[i] << (36 + i));
        k.comb[u] = n;
    }
}

void build_key(FfpKey &k, bool points) {
    memset(&k, 0, sizeof k);
    const Ffp &f = g.f;
    bool need_n = false;
    if (f.lighting) {
        k.flags |= FK_LIGHTING;
        k.light_on = f.light_mask;
        bool lspec = false;
        for (int i = 0; i < MAX_LIGHTS; ++i) {
            if (!(f.light_mask & (1u << i))) continue;
            if (f.l[i].pos.w != 0.0f) { k.light_pos |= (uint8_t)(1u << i); if (f.l[i].spot_cut != 180.0f) k.light_spot |= (uint8_t)(1u << i); }
            if (f.l[i].spe.x != 0 || f.l[i].spe.y != 0 || f.l[i].spe.z != 0) lspec = true;
        }
        if (f.color_material) {
            k.flags |= FK_CM;
            k.cm_mode = f.cm_mode == GL_AMBIENT ? 1 : f.cm_mode == GL_DIFFUSE ? 2 : f.cm_mode == GL_SPECULAR ? 3 : f.cm_mode == GL_EMISSION ? 4 : 5;
            k.cm_face = f.cm_face == GL_FRONT ? 1 : f.cm_face == GL_BACK ? 2 : 3;
        }
        if (f.two_side) k.flags |= FK_TWOSIDE;
        if (f.local_viewer) k.flags |= FK_LOCALVIEW;
        if (f.sep_spec) k.flags |= FK_SEPSPEC;
        const Vec4 &ms = f.mat[0].spe, &mb = f.mat[1].spe;
        bool mspec = ms.x != 0 || ms.y != 0 || ms.z != 0 || (f.two_side && (mb.x != 0 || mb.y != 0 || mb.z != 0)) ||
                     (f.color_material && f.cm_mode == GL_SPECULAR);
        if (lspec && mspec) k.flags |= FK_SPECULAR;
        need_n = true;
    }
    if (f.alpha_test && f.alpha_func != GL_ALWAYS) { k.flags |= FK_ALPHA; k.alpha_func = (uint8_t)(f.alpha_func - GL_NEVER); }
    if (f.fog) {
        k.flags |= FK_FOG;
        k.fog_mode = f.fog_mode == GL_LINEAR ? 1 : f.fog_mode == GL_EXP2 ? 3 : 2;
        k.fog_src = f.fog_src == GL_FOG_COORD ? 3 : f.fog_dist == GL_EYE_RADIAL_NV ? 2 : f.fog_dist == GL_EYE_PLANE ? 1 : 0;
    }
    if (f.shade == GL_FLAT) k.flags |= FK_FLAT;
    if (points) k.flags |= FK_POINTS;
    if (f.color_sum && !f.lighting) k.flags |= FK_COLORSUM;
    k.clip = f.clip_mask;
    for (int u = 0; u < MAX_TEX_UNITS; ++u) {
        const TexUnit &t = f.tu[u];
        uint32_t tgt = (t.enabled & TGT_CUBE) ? 4 : (t.enabled & TGT_3D) ? 3 : (t.enabled & TGT_2D) ? 2 : (t.enabled & TGT_1D) ? 1 : 0;
        if (!tgt) continue;
        uint32_t env = env_code(t.env.mode);
        uint32_t w = tgt | (env << 3) | ((uint32_t)t.fmt << 6) | ((uint32_t)t.gen.on << 8);
        for (int c = 0; c < 4; ++c) if (t.gen.on & (1u << c)) {
            uint32_t gc = gen_code(t.gen.mode[c]);
            w |= gc << (12 + 3 * c);
            if (gc == GEN_SPHERE || gc == GEN_NORMAL || gc == GEN_REFLECT) need_n = true;
        }
        if (t.gen.on & 8u) w |= 1u << 24;
        k.tu[u] = w;
        if (env == ENV_COMBINE) {
            const TexEnv &e = t.env;
            uint64_t c = comb_func(e.comb_rgb) | (comb_func(e.comb_a) << 3);
            for (int i = 0; i < 3; ++i) {
                c |= comb_src(e.src_rgb[i]) << (6 + 4 * i);
                c |= comb_src(e.src_a[i]) << (18 + 4 * i);
                c |= comb_op(e.op_rgb[i]) << (30 + 2 * i);
                c |= (uint64_t)(e.op_a[i] == GL_ONE_MINUS_SRC_ALPHA ? 1 : 0) << (36 + i);
            }
            c |= scale_code(e.scale_rgb) << 39;
            c |= scale_code(e.scale_a) << 41;
            k.comb[u] = c;
        }
        k.nunits = (uint8_t)(u + 1);
    }
    if (need_n) {
        if (f.normalize) k.flags |= FK_NORMALIZE;
        else if (f.rescale) k.flags |= FK_RESCALE;
    }
    canon_key(k);
}

uint32_t key_hash(const FfpKey &k) {
    const uint8_t *p = (const uint8_t *)&k; uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof k; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

// ------------------------------------------------------------------ GLSL generation
const char *kCoord = "stpq";
bool g_ffp_highp = false;               // ORYON_FFP_HIGHP=1: highp vec4 varyings everywhere (pre-0.1 layout)
// Varyings are sized to what the fragment stage reads (tilers write them to memory per vertex): s,t for 1D/2D,
// s,t,p for 3D/cube, all four for projective lookups; colours and fog distance are mediump.
int tc_n(uint32_t w) { return g_ffp_highp || ((w >> 24) & 1) ? 4 : ((w & 7) == 3 || (w & 7) == 4) ? 3 : 2; }
const char *tc_sw(int n) { return n == 4 ? "" : n == 3 ? ".xyz" : ".xy"; }
const char *lowp_v() { return g_ffp_highp ? "" : "mediump "; }
void gen_vs(SB &s, const FfpKey &k, uint32_t &attr_mask, uint8_t &xf_n) {
    bool lit = k.flags & FK_LIGHTING;
    bool need_n = lit, need_eye = false, need_refl = false;
    for (int u = 0; u < k.nunits; ++u) {
        uint32_t w = k.tu[u]; if (!(w & 7)) continue;
        for (int c = 0; c < 4; ++c) if ((w >> (8 + c)) & 1) {
            uint32_t gc = (w >> (12 + 3 * c)) & 7;
            if (gc == GEN_EYE) need_eye = true;
            if (gc == GEN_SPHERE || gc == GEN_REFLECT) { need_eye = need_n = need_refl = true; }
            if (gc == GEN_NORMAL) need_n = true;
        }
    }
    if (lit && (k.light_pos || (k.flags & FK_LOCALVIEW))) need_eye = true;
    if ((k.flags & FK_FOG) && k.fog_src != 3) need_eye = true;
    if (k.clip) need_eye = true;
    const char *fl = (k.flags & FK_FLAT) ? "flat " : "";
    s.add("#version 320 es\nprecision highp float;\nlayout(location=0) in vec4 a_pos;\n");
    attr_mask = 1u << LOC_POS;
    if (need_n) { s.add("layout(location=2) in vec3 a_nrm;\n"); attr_mask |= 1u << LOC_NORMAL; }
    s.add("layout(location=3) in vec4 a_col;\n"); attr_mask |= 1u << LOC_COLOR;
    if (k.flags & FK_COLORSUM) { s.add("layout(location=4) in vec4 a_col2;\n"); attr_mask |= 1u << LOC_COLOR2; }
    if ((k.flags & FK_FOG) && k.fog_src == 3) { s.add("layout(location=5) in float a_fog;\n"); attr_mask |= 1u << LOC_FOG; }
    for (int u = 0; u < k.nunits; ++u) {
        uint32_t w = k.tu[u]; if (!(w & 7)) continue;
        if (((w >> 8) & 15) != 15) { s.addf("layout(location=%d) in vec4 a_t%d;\n", LOC_TEX0 + u, u); attr_mask |= 1u << (LOC_TEX0 + u); }
        s.addf("uniform mat4 u_tm%d;\nout highp vec%d v_t%d;\n", u, tc_n(w), u);
        if ((w >> 8) & 15) s.addf("uniform vec4 u_tg%d[8];\n", u);
    }
    xf_n = need_n ? 3 : need_eye ? 2 : 1;
    s.addf("uniform mat4 u_xf[%d];\n", (int)xf_n);
    int nl = 0; for (int i = 0; i < MAX_LIGHTS; ++i) if (k.light_on & (1u << i)) ++nl;
    if (lit) { if (nl) s.addf("uniform vec4 u_L[%d];\n", nl * 6); s.add("uniform vec4 u_M[11];\n"); }
    if (k.clip) s.add("uniform vec4 u_clip[6];\n");
    if (k.flags & FK_POINTS) s.add("uniform float u_psz;\n");
    s.addf("%sout %svec4 v_c;\n", fl, lowp_v());
    if (lit && (k.flags & FK_TWOSIDE)) s.addf("%sout %svec4 v_bc;\n", fl, lowp_v());
    bool sec = ((k.flags & FK_SEPSPEC) && (k.flags & FK_SPECULAR)) || (k.flags & FK_COLORSUM);
    if (sec) s.addf("%sout %svec3 v_s;\n", fl, lowp_v());
    if (k.flags & FK_FOG) s.addf("out %s float v_fz;\n", g_ffp_highp ? "highp" : "mediump");
    for (int j = 0; j < MAX_CLIP_PLANES; ++j) if (k.clip & (1u << j)) s.addf("out highp float v_cd%d;\n", j);
    s.add("void main() {\n  gl_Position = u_xf[0] * a_pos;\n");
    if (need_eye) s.add("  vec4 ep = u_xf[1] * a_pos;\n");
    if (need_n) {
        s.add("  vec3 n = mat3(u_xf[2]) * a_nrm;\n");
        if (k.flags & FK_NORMALIZE) s.add("  n = normalize(n);\n");
    }
    if (lit) {
        int cm = (k.flags & FK_CM) ? k.cm_mode : 0;
        for (int side = 0; side < ((k.flags & FK_TWOSIDE) ? 2 : 1); ++side) {
            bool cmside = cm && (k.cm_face & (side ? 2 : 1));
            int mb = side ? 5 : 0;
            char mA[16], mD[16], mS[16], mE[16];
            snprintf(mA, sizeof mA, (cmside && (cm == 1 || cm == 5)) ? "a_col" : "u_M[%d]", mb + 0);
            snprintf(mD, sizeof mD, (cmside && (cm == 2 || cm == 5)) ? "a_col" : "u_M[%d]", mb + 1);
            snprintf(mS, sizeof mS, (cmside && cm == 3) ? "a_col" : "u_M[%d]", mb + 2);
            snprintf(mE, sizeof mE, (cmside && cm == 4) ? "a_col" : "u_M[%d]", mb + 3);
            const char *N = side ? "(-n)" : "n";
            s.addf("  {\n    vec3 c = %s.rgb + u_M[10].rgb * %s.rgb;\n", mE, mA);
            if (k.flags & FK_SPECULAR) {
                s.add("    vec3 sp = vec3(0.0);\n");
                s.add((k.flags & FK_LOCALVIEW) ? "    vec3 V = -normalize(ep.xyz);\n" : "    const vec3 V = vec3(0.0, 0.0, 1.0);\n");
            }
            s.add("    vec3 L; float at, nl;\n");
            int j = 0;
            for (int i = 0; i < MAX_LIGHTS; ++i) {
                if (!(k.light_on & (1u << i))) continue;
                int b = j * 6;
                if (k.light_pos & (1u << i)) {
                    s.addf("    { vec3 d = u_L[%d].xyz - ep.xyz; float dd = length(d); L = d / dd;"
                           " at = 1.0 / (u_L[%d].x + u_L[%d].y * dd + u_L[%d].z * dd * dd); }\n", b + 3, b + 5, b + 5, b + 5);
                    if (k.light_spot & (1u << i))
                        s.addf("    { float sd = dot(-L, u_L[%d].xyz); at *= sd < u_L[%d].w ? 0.0 : pow(max(sd, 1e-30), u_L[%d].w); }\n", b + 4, b + 4, b + 5);
                } else {
                    s.addf("    L = u_L[%d].xyz; at = 1.0;\n", b + 3);
                }
                s.addf("    nl = max(dot(%s, L), 0.0);\n    c += at * (u_L[%d].rgb * %s.rgb + nl * u_L[%d].rgb * %s.rgb);\n", N, b, mA, b + 1, mD);
                if (k.flags & FK_SPECULAR)
                    s.addf("    if (nl > 0.0) sp += at * pow(max(dot(%s, normalize(L + V)), 1e-30), u_M[%d].x) * u_L[%d].rgb * %s.rgb;\n",
                           N, mb + 4, b + 2, mS);
                ++j;
            }
            if ((k.flags & FK_SPECULAR) && !(k.flags & FK_SEPSPEC)) s.add("    c += sp;\n");
            s.addf("    %s = vec4(clamp(c, 0.0, 1.0), %s.a);\n", side ? "v_bc" : "v_c", mD);
            if (!side && (k.flags & FK_SEPSPEC) && (k.flags & FK_SPECULAR)) s.add("    v_s = clamp(sp, 0.0, 1.0);\n");
            s.add("  }\n");
        }
    } else {
        s.add("  v_c = a_col;\n");
    }
    if (k.flags & FK_COLORSUM) s.add("  v_s = a_col2.rgb;\n");
    if (need_refl) s.add("  vec3 su = normalize(ep.xyz); vec3 sr = reflect(su, n);"
                         " float sm = 2.0 * sqrt(sr.x * sr.x + sr.y * sr.y + (sr.z + 1.0) * (sr.z + 1.0));\n");
    for (int u = 0; u < k.nunits; ++u) {
        uint32_t w = k.tu[u]; if (!(w & 7)) continue;
        uint32_t on = (w >> 8) & 15;
        if (!on) { s.addf("  v_t%d = (u_tm%d * a_t%d)%s;\n", u, u, u, tc_sw(tc_n(w))); continue; }
        if (on == 15) s.add("  { vec4 t = vec4(0.0, 0.0, 0.0, 1.0);\n");
        else s.addf("  { vec4 t = a_t%d;\n", u);
        for (int c = 0; c < 4; ++c) {
            if (!(on & (1u << c))) continue;
            uint32_t gc = (w >> (12 + 3 * c)) & 7;
            switch (gc) {
            case GEN_OBJ: s.addf("    t.%c = dot(a_pos, u_tg%d[%d]);\n", kCoord[c], u, c); break;
            case GEN_EYE: s.addf("    t.%c = dot(ep, u_tg%d[%d]);\n", kCoord[c], u, 4 + c); break;
            case GEN_SPHERE: s.addf("    t.%c = %s / sm + 0.5;\n", kCoord[c], c == 0 ? "sr.x" : c == 1 ? "sr.y" : "0.0"); break;
            case GEN_NORMAL: s.addf("    t.%c = n.%c;\n", kCoord[c], "xyzx"[c]); break;
            case GEN_REFLECT: s.addf("    t.%c = sr.%c;\n", kCoord[c], "xyzx"[c]); break;
            }
        }
        s.addf("    v_t%d = (u_tm%d * t)%s; }\n", u, u, tc_sw(tc_n(w)));
    }
    if (k.flags & FK_FOG) {
        static const char *kFz[] = {"abs(ep.z)", "-ep.z", "length(ep.xyz)", "a_fog"};
        s.addf("  v_fz = %s;\n", kFz[k.fog_src & 3]);
    }
    for (int j = 0; j < MAX_CLIP_PLANES; ++j) if (k.clip & (1u << j)) s.addf("  v_cd%d = dot(ep, u_clip[%d]);\n", j, j);
    if (k.flags & FK_POINTS) s.add("  gl_PointSize = u_psz;\n");
    s.add("}\n");
}

const char *src_expr(uint64_t code, int u, char *buf) {
    switch (code) {
    case 0: snprintf(buf, 16, "t%d", u); return buf;
    case 1: snprintf(buf, 16, "u_ec%d", u); return buf;
    case 2: return "pc";
    case 3: return "c";
    default: snprintf(buf, 16, "t%d", (int)(code - 4)); return buf;
    }
}

void gen_fs(SB &s, const FfpKey &k) {
    bool lit = k.flags & FK_LIGHTING;
    const char *fl = (k.flags & FK_FLAT) ? "flat " : "";
    s.add("#version 320 es\nprecision mediump float;\nprecision mediump sampler3D;\nprecision mediump sampler2D;\nprecision mediump samplerCube;\n");
    s.addf("%sin vec4 v_c;\n", fl);
    if (lit && (k.flags & FK_TWOSIDE)) s.addf("%sin vec4 v_bc;\n", fl);
    bool sec = ((k.flags & FK_SEPSPEC) && (k.flags & FK_SPECULAR)) || (k.flags & FK_COLORSUM);
    if (sec) s.addf("%sin vec3 v_s;\n", fl);
    if (k.flags & FK_FOG) s.addf("in %s float v_fz;\nuniform vec4 u_fog[2];\n", g_ffp_highp ? "highp" : "mediump");
    for (int j = 0; j < MAX_CLIP_PLANES; ++j) if (k.clip & (1u << j)) s.addf("in highp float v_cd%d;\n", j);
    uint32_t used = 0;           // units whose texture is sampled (incl. crossbar)
    for (int u = 0; u < k.nunits; ++u) {
        uint32_t w = k.tu[u]; if (!(w & 7)) continue;
        used |= 1u << u;
        if (((w >> 3) & 7) == ENV_COMBINE) {
            uint64_t c = k.comb[u];
            for (int i = 0; i < 3; ++i) {
                uint64_t a = (c >> (6 + 4 * i)) & 15, b = (c >> (18 + 4 * i)) & 15;
                if (a >= 4) used |= 1u << (a - 4);
                if (b >= 4) used |= 1u << (b - 4);
            }
        }
    }
    for (int u = 0; u < MAX_TEX_UNITS; ++u) {
        if (!(used & (1u << u))) continue;
        uint32_t w = u < k.nunits ? k.tu[u] : 0, tgt = w & 7;
        s.addf("in highp vec%d v_t%d;\nuniform %s u_s%d;\n", tc_n(w), u, tgt == 4 ? "samplerCube" : tgt == 3 ? "sampler3D" : "sampler2D", u);
        uint32_t env = (w >> 3) & 7;
        bool need_ec = env == ENV_BLEND;
        if (env == ENV_COMBINE)
            for (int i = 0; i < 3; ++i) if (((k.comb[u] >> (6 + 4 * i)) & 15) == 1 || ((k.comb[u] >> (18 + 4 * i)) & 15) == 1) need_ec = true;
        if (need_ec) s.addf("uniform vec4 u_ec%d;\n", u);
    }
    if (k.flags & FK_ALPHA) s.add("uniform float u_aref;\n");
    s.add("out vec4 o_c;\nvoid main() {\n");
    for (int j = 0; j < MAX_CLIP_PLANES; ++j) if (k.clip & (1u << j)) s.addf("  if (v_cd%d < 0.0) discard;\n", j);
    s.add((lit && (k.flags & FK_TWOSIDE)) ? "  vec4 pc = gl_FrontFacing ? v_c : v_bc;\n" : "  vec4 pc = v_c;\n");
    s.add("  vec4 c = pc;\n");
    for (int u = 0; u < MAX_TEX_UNITS; ++u) {
        if (!(used & (1u << u))) continue;
        uint32_t w = u < k.nunits ? k.tu[u] : 0, tgt = w & 7;
        bool proj = (w >> 24) & 1;
        const int tn = tc_n(w);
        if (tgt == 4) s.addf("  vec4 t%d = texture(u_s%d, v_t%d%s);\n", u, u, u, tn == 3 ? "" : ".stp");
        else if (tgt == 3) s.addf("  vec4 t%d = %s(u_s%d, v_t%d%s);\n", u, proj ? "textureProj" : "texture", u, u, proj || tn == 3 ? "" : ".stp");
        else if (tgt == 1) {
            if (proj) s.addf("  vec4 t%d = texture(u_s%d, vec2(v_t%d.s / v_t%d.q, 0.0));\n", u, u, u, u);
            else s.addf("  vec4 t%d = texture(u_s%d, vec2(v_t%d.s, 0.0));\n", u, u, u);
        }
        else s.addf("  vec4 t%d = %s(u_s%d, v_t%d%s);\n", u, proj ? "textureProj" : "texture", u, u, proj || tn == 2 ? "" : ".st");
    }
    for (int u = 0; u < k.nunits; ++u) {
        uint32_t w = k.tu[u]; if (!(w & 7)) continue;
        uint32_t env = (w >> 3) & 7, fmt = (w >> 6) & 3;
        switch (env) {
        case ENV_REPLACE:
            if (fmt == FMT_RGB) s.addf("  c.rgb = t%d.rgb;\n", u);
            else if (fmt == FMT_ALPHA) s.addf("  c.a = t%d.a;\n", u);
            else s.addf("  c = t%d;\n", u);
            break;
        case ENV_MODULATE:
            if (fmt == FMT_RGB) s.addf("  c.rgb *= t%d.rgb;\n", u);
            else if (fmt == FMT_ALPHA) s.addf("  c.a *= t%d.a;\n", u);
            else s.addf("  c *= t%d;\n", u);
            break;
        case ENV_DECAL:
            if (fmt == FMT_RGB) s.addf("  c.rgb = t%d.rgb;\n", u);
            else if (fmt == FMT_RGBA) s.addf("  c.rgb = mix(c.rgb, t%d.rgb, t%d.a);\n", u, u);
            break;
        case ENV_BLEND:
            if (fmt == FMT_INTENSITY) s.addf("  c = mix(c, u_ec%d, t%d);\n", u, u);
            else if (fmt == FMT_ALPHA) s.addf("  c.a *= t%d.a;\n", u);
            else { s.addf("  c.rgb = mix(c.rgb, u_ec%d.rgb, t%d.rgb);\n", u, u); if (fmt == FMT_RGBA) s.addf("  c.a *= t%d.a;\n", u); }
            break;
        case ENV_ADD:
            if (fmt == FMT_INTENSITY) s.addf("  c = min(c + t%d, 1.0);\n", u);
            else if (fmt == FMT_ALPHA) s.addf("  c.a *= t%d.a;\n", u);
            else { s.addf("  c.rgb = min(c.rgb + t%d.rgb, 1.0);\n", u); if (fmt == FMT_RGBA) s.addf("  c.a *= t%d.a;\n", u); }
            break;
        case ENV_COMBINE: {
            uint64_t cb = k.comb[u];
            uint64_t fr = cb & 7, fa = (cb >> 3) & 7;
            char b0[16], b1[16], b2[16];
            s.add("  {\n");
            for (int i = 0; i < 3; ++i) {
                const char *sr = src_expr((cb >> (6 + 4 * i)) & 15, u, i == 0 ? b0 : i == 1 ? b1 : b2);
                uint64_t op = (cb >> (30 + 2 * i)) & 3;
                static const char *kOp[] = {"%s.rgb", "(1.0 - %s.rgb)", "vec3(%s.a)", "vec3(1.0 - %s.a)"};
                s.add("    vec3 r"); s.addf("%d = ", i); s.addf(kOp[op], sr); s.add(";\n");
            }
            for (int i = 0; i < 3; ++i) {
                const char *sa = src_expr((cb >> (18 + 4 * i)) & 15, u, i == 0 ? b0 : i == 1 ? b1 : b2);
                bool inv = (cb >> (36 + i)) & 1;
                s.addf(inv ? "    float a%d = 1.0 - %s.a;\n" : "    float a%d = %s.a;\n", i, sa);
            }
            static const char *kFr[] = {"r0", "r0 * r1", "r0 + r1", "r0 + r1 - 0.5", "mix(r1, r0, r2)", "r0 - r1",
                                        "vec3(4.0 * dot(r0 - 0.5, r1 - 0.5))", "vec3(4.0 * dot(r0 - 0.5, r1 - 0.5))"};
            static const char *kFa[] = {"a0", "a0 * a1", "a0 + a1", "a0 + a1 - 0.5", "mix(a1, a0, a2)", "a0 - a1", "a0", "a0"};
            static const char *kSc[] = {"", " * 2.0", " * 4.0"};
            s.addf("    vec3 cr = (%s)%s;\n", kFr[fr], kSc[(cb >> 39) & 3]);
            if (fr == 7) s.add("    float ca = cr.r;\n");
            else s.addf("    float ca = (%s)%s;\n", kFa[fa], kSc[(cb >> 41) & 3]);
            s.add("    c = clamp(vec4(cr, ca), 0.0, 1.0);\n  }\n");
            break;
        }
        default: break;
        }
    }
    if (sec) s.add("  c.rgb = min(c.rgb + v_s, 1.0);\n");
    if (k.flags & FK_FOG) {
        s.add("  float f;\n  if (u_fog[1].w < 0.5) f = v_fz * u_fog[1].x + u_fog[1].y;\n"
              "  else { float fd = u_fog[1].z * v_fz; f = exp(-(u_fog[1].w < 1.5 ? fd : fd * fd)); }\n");
        s.add("  c.rgb = mix(u_fog[0].rgb, c.rgb, clamp(f, 0.0, 1.0));\n");
    }
    if (k.flags & FK_ALPHA) {
        static const char *kA[] = {"true", "!(c.a < u_aref)", "!(c.a == u_aref)", "!(c.a <= u_aref)",
                                   "!(c.a > u_aref)", "!(c.a != u_aref)", "!(c.a >= u_aref)", "false"};
        s.addf("  if (%s) discard;\n", kA[k.alpha_func & 7]);
    }
    s.add("  o_c = c;\n}\n");
}

GLuint compile(GLenum type, const char *src) {
    GLuint sh = es.glCreateShader(type);
    es.glShaderSource(sh, 1, &src, nullptr);
    es.glCompileShader(sh);
    GLint ok = 0; es.glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; GLsizei l = 0; es.glGetShaderInfoLog(sh, sizeof log, &l, log);
        ory::log("FFP %s compile failed: %s\n%s", type == GL_VERTEX_SHADER ? "VS" : "FS", log, src);
        es.glDeleteShader(sh); return 0;
    }
    return sh;
}

Program *g_bucket[256];

// ------------------------------------------------------------------ persistent program binary cache
// One file per key: <dir>/ffp-<fnv64(key)>.bin = CacheHdr + key + driver binary. A binary is reused only when the
// FNV-64 of (driver strings + generated VS + FS) matches, so a driver update or a generator change rebuilds instead
// of loading stale code. Known keys are loaded at context init, so they never compile during play.
struct CacheHdr { char magic[4]; uint32_t ver, key_size, fmt, len, pad; uint64_t src_hash; };
static_assert(sizeof(CacheHdr) == 32, "cache header layout");
const uint32_t kCacheVer = 1;
char g_cache_dir[512];
bool g_cache_on = false;
uint64_t g_drv_hash = 0;
GLint g_bin_fmt[16]; int g_nbin_fmt = 0;

uint64_t fnv64(const void *d, size_t n, uint64_t h = 1469598103934665603ull) {
    const uint8_t *p = (const uint8_t *)d;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}
uint64_t src_hash() { return fnv64(g_fs, strlen(g_fs), fnv64(g_vs, strlen(g_vs), g_drv_hash)); }
void cache_path(const FfpKey &k, char *out, size_t n) {
    snprintf(out, n, "%s/ffp-%016llx.bin", g_cache_dir, (unsigned long long)fnv64(&k, sizeof k));
}
bool fmt_known(GLenum f) { for (int i = 0; i < g_nbin_fmt; ++i) if ((GLenum)g_bin_fmt[i] == f) return true; return false; }
// Oryon's own cache calls must not change what the app's glGetError() reports.
struct ErrGuard {
    GLenum pending;
    ErrGuard() : pending(es.glGetError()) {}
    ~ErrGuard() { while (es.glGetError() != GL_NO_ERROR) {} if (pending) set_error(pending); }
};
// Reads header + key + binary (malloc'd, caller frees); nullptr if missing or malformed.
void *cache_read(const char *path, CacheHdr &H, FfpKey &K) {
    FILE *f = fopen(path, "rb");
    if (!f) return nullptr;
    void *bin = nullptr;
    if (fread(&H, sizeof H, 1, f) == 1 && !memcmp(H.magic, "ORYP", 4) && H.ver == kCacheVer && H.key_size == sizeof(FfpKey) &&
        H.len > 0 && H.len <= (16u << 20) && fread(&K, sizeof K, 1, f) == 1 && (bin = malloc(H.len)) != nullptr &&
        fread(bin, 1, H.len, f) != H.len) { free(bin); bin = nullptr; }
    fclose(f);
    return bin;
}
GLuint program_from_binary(GLenum fmt, const void *bin, GLsizei len) {
    if (!fmt_known(fmt)) return 0;
    GLuint p = es.glCreateProgram();
    es.glProgramBinary(p, fmt, bin, len);
    GLint ok = 0; es.glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { es.glDeleteProgram(p); return 0; }
    return p;
}
void cache_store(GLuint p, const FfpKey &k, uint64_t sh) {
    GLint len = 0; es.glGetProgramiv(p, GL_PROGRAM_BINARY_LENGTH, &len);
    if (len <= 0 || len > (16 << 20)) return;
    void *bin = malloc((size_t)len);
    if (!bin) return;
    GLsizei got = 0; GLenum fmt = 0;
    es.glGetProgramBinary(p, len, &got, &fmt, bin);
    if (got > 0) {
        char path[600], tmp[610];
        cache_path(k, path, sizeof path); snprintf(tmp, sizeof tmp, "%s.tmp", path);
        CacheHdr H; memcpy(H.magic, "ORYP", 4);
        H.ver = kCacheVer; H.key_size = sizeof(FfpKey); H.fmt = fmt; H.len = (uint32_t)got; H.pad = 0; H.src_hash = sh;
        if (FILE *f = fopen(tmp, "wb")) {
            bool ok = fwrite(&H, sizeof H, 1, f) == 1 && fwrite(&k, sizeof k, 1, f) == 1 && fwrite(bin, 1, (size_t)got, f) == (size_t)got;
            ok = fclose(f) == 0 && ok;
            if (!ok || rename(tmp, path) != 0) unlink(tmp);
        }
    }
    free(bin);
}

// Compact description of a key for slow-compile diagnostics (ORYON_STATS only).
void key_desc(const FfpKey &k, char *out, size_t n) {
    SB s{out, 0, n}; out[0] = 0;
    static const char *kTgt[] = {"-", "1D", "2D", "3D", "CUBE", "?", "?", "?"};
    static const char *kEnv[] = {"off", "replace", "modulate", "decal", "blend", "add", "combine", "?"};
    if (k.flags & FK_LIGHTING) s.addf("light x%d%s%s%s ", __builtin_popcount(k.light_on), (k.flags & FK_CM) ? " +colormat" : "",
                                      (k.flags & FK_SPECULAR) ? " +spec" : "", (k.flags & FK_TWOSIDE) ? " +2side" : "");
    for (int u = 0; u < k.nunits; ++u) {
        uint32_t w = k.tu[u]; if (!(w & 7)) continue;
        s.addf("t%d:%s/%s%s ", u, kTgt[w & 7], kEnv[(w >> 3) & 7], ((w >> 8) & 15) ? "+texgen" : "");
    }
    if (k.flags & FK_FOG) s.addf("fog%s ", k.fog_src == 2 ? ":radial" : k.fog_src == 3 ? ":coord" : "");
    if (k.flags & FK_ALPHA) s.add("alphatest ");
    if (k.flags & FK_FLAT) s.add("flat ");
    if (k.flags & FK_COLORSUM) s.add("colorsum ");
    if (k.flags & FK_POINTS) s.add("points ");
    if (k.clip) s.addf("clip:%x ", k.clip);
    if (!s.n) s.add("colour only");
}

uint32_t gen_sources(const FfpKey &k, uint8_t &xfn) {
    SB vs{g_vs, 0, sizeof g_vs}, fs{g_fs, 0, sizeof g_fs};
    g_vs[0] = g_fs[0] = 0;
    uint32_t amask = 0;
    gen_vs(vs, k, amask, xfn);
    gen_fs(fs, k);
    return amask;
}
// Compiles + links g_vs/g_fs (timed as an FFP program build).
GLuint compile_program(const FfpKey &k) {
    StatTimer st_(g.st.ffp_n, g.st.ffp_us, &g.st.ffp_max);
    if (UNLIKELY(env_on("ORYON_DUMP_FFP"))) ory::log("FFP program sources:\n%s\n----\n%s", g_vs, g_fs);
    GLuint v = compile(GL_VERTEX_SHADER, g_vs), f = compile(GL_FRAGMENT_SHADER, g_fs);
    if (!v || !f) { if (v) es.glDeleteShader(v); if (f) es.glDeleteShader(f); return 0; }
    GLuint p = es.glCreateProgram();
    if (g_cache_on) es.glProgramParameteri(p, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    es.glAttachShader(p, v); es.glAttachShader(p, f);
    es.glLinkProgram(p);
    es.glDetachShader(p, v); es.glDetachShader(p, f);
    es.glDeleteShader(v); es.glDeleteShader(f);
    GLint ok = 0; es.glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; GLsizei l = 0; es.glGetProgramInfoLog(p, sizeof log, &l, log);
        ory::log("FFP link failed: %s", log); es.glDeleteProgram(p); return 0;
    }
    if (UNLIKELY(g.st.on)) {
        const uint64_t d = now_us() - st_.t0;
        if (d >= g.stats_slow_us) {
            char kd[256]; key_desc(k, kd, sizeof kd);
            ory::log("slow ffp compile %.1f ms (vs %u B, fs %u B): %s", d / 1000.0, (unsigned)strlen(g_vs), (unsigned)strlen(g_fs), kd);
        }
    }
    return p;
}
Program *finish(GLuint p, const FfpKey &k, uint32_t h, uint32_t amask, uint8_t xfn) {
    Program *P = (Program *)calloc(1, sizeof(Program));
    P->id = p; P->hash = h; P->key = k; P->attr_mask = amask;
    P->u_xf = es.glGetUniformLocation(p, "u_xf"); P->n_xf = xfn;
    P->u_L = es.glGetUniformLocation(p, "u_L");
    P->u_M = es.glGetUniformLocation(p, "u_M");
    P->u_fog = es.glGetUniformLocation(p, "u_fog");
    P->u_aref = es.glGetUniformLocation(p, "u_aref");
    P->u_psz = es.glGetUniformLocation(p, "u_psz");
    P->u_clip = es.glGetUniformLocation(p, "u_clip");
    es_use_program(p);
    char nm[16];
    for (int u = 0; u < MAX_TEX_UNITS; ++u) {
        snprintf(nm, sizeof nm, "u_tm%d", u); P->u_tm[u] = es.glGetUniformLocation(p, nm);
        snprintf(nm, sizeof nm, "u_ec%d", u); P->u_ec[u] = es.glGetUniformLocation(p, nm);
        snprintf(nm, sizeof nm, "u_tg%d", u); P->u_tg[u] = es.glGetUniformLocation(p, nm);
        snprintf(nm, sizeof nm, "u_s%d", u); GLint ls = es.glGetUniformLocation(p, nm);
        if (ls >= 0) es.glUniform1i(ls, u);
    }
    for (int i = 0; i < MAX_LIGHTS; ++i) if (k.light_on & (1u << i)) P->lidx[P->nl++] = (uint8_t)i;
    return P;
}
void insert(Program *P) { Program **b = &g_bucket[P->hash & 255]; P->next = *b; *b = P; }
bool cached_in_memory(const FfpKey &k, uint32_t h) {
    for (Program *p = g_bucket[h & 255]; p; p = p->next) if (p->hash == h && !memcmp(&p->key, &k, sizeof k)) return true;
    return false;
}
// Warm-up of one key: load its binary when the source hash matches, else rebuild (at startup) and store.
bool warm_key(const FfpKey &K, const CacheHdr *H, const void *bin, int &loaded, int &rebuilt) {
    const uint32_t kh = key_hash(K);
    if (cached_in_memory(K, kh)) return true;
    uint8_t xfn = 1;
    const uint32_t amask = gen_sources(K, xfn);
    const uint64_t sh = src_hash();
    GLuint p = (H && bin && H->src_hash == sh) ? program_from_binary(H->fmt, bin, (GLsizei)H->len) : 0;
    if (p) ++loaded;
    else if ((p = compile_program(K)) != 0) { cache_store(p, K, sh); ++rebuilt; }
    else return false;
    insert(finish(p, K, kh, amask, xfn));
    return true;
}

__attribute__((noinline, cold)) Program *create(const FfpKey &k, uint32_t h) {
    uint8_t xfn = 1;
    const uint32_t amask = gen_sources(k, xfn);
    GLuint p = 0;
    if (g_cache_on) {
        ErrGuard eg_;
        const uint64_t sh = src_hash();
        char path[600]; cache_path(k, path, sizeof path);
        CacheHdr H; FfpKey K;
        if (void *bin = cache_read(path, H, K)) {
            if (H.src_hash == sh && !memcmp(&K, &k, sizeof k)) p = program_from_binary(H.fmt, bin, (GLsizei)H.len);
            free(bin);
        }
        if (!p && (p = compile_program(k)) != 0) cache_store(p, k, sh);
    } else {
        p = compile_program(k);
    }
    return p ? finish(p, k, h, amask, xfn) : nullptr;
}

Program *lookup(const FfpKey &k) {
    uint32_t h = key_hash(k);
    Program **b = &g_bucket[h & 255];
    for (Program *p = *b; p; p = p->next)
        if (p->hash == h && !memcmp(&p->key, &k, sizeof k)) return p;
    Program *p = create(k, h);
    if (p) insert(p);
    return p;
}

void upload(Program *P) {
    Matrices &M = g.m; Ffp &f = g.f;
    if (P->u_xf >= 0 && (P->s_xf_mv != M.mv_ver || P->s_xf_p != M.p_ver)) {      // MVP [, MV [, normal]]: one call
        if (M.mvp_mv != M.mv_ver || M.mvp_p != M.p_ver) {
            mat_mul(M.mvp.m, M.p[M.p_top].m, M.mv[M.mv_top].m); M.mvp_mv = M.mv_ver; M.mvp_p = M.p_ver;
        }
        GLfloat d[48];
        memcpy(d, M.mvp.m, 64);
        if (P->n_xf > 1) memcpy(d + 16, M.mv[M.mv_top].m, 64);
        if (P->n_xf > 2) {
            const bool rs = (P->key.flags & FK_RESCALE) != 0;
            if (M.nm_mv != M.mv_ver || M.nm_rescale != rs) { mat_normal(M.nm, M.mv[M.mv_top].m, rs); M.nm_mv = M.mv_ver; M.nm_rescale = rs; }
            GLfloat *o = d + 32;
            for (int c = 0; c < 3; ++c) { o[c * 4] = M.nm[c * 3]; o[c * 4 + 1] = M.nm[c * 3 + 1]; o[c * 4 + 2] = M.nm[c * 3 + 2]; o[c * 4 + 3] = 0.0f; }
            o[12] = o[13] = o[14] = 0.0f; o[15] = 1.0f;
        }
        es.glUniformMatrix4fv(P->u_xf, P->n_xf, GL_FALSE, d);
        P->s_xf_mv = M.mv_ver; P->s_xf_p = M.p_ver;
    }
    for (int u = 0; u < P->key.nunits; ++u)
        if (P->u_tm[u] >= 0 && P->s_t[u] != M.t_ver[u]) {
            es.glUniformMatrix4fv(P->u_tm[u], 1, GL_FALSE, M.t[u][M.t_top[u]].m); P->s_t[u] = M.t_ver[u];
        }
    if (P->u_L >= 0 && P->s_light != f.v_light) {
        GLfloat d[MAX_LIGHTS * 24];
        for (int j = 0; j < P->nl; ++j) {
            const Light &l = f.l[P->lidx[j]]; GLfloat *o = d + j * 24;
            memcpy(o, &l.amb, 16); memcpy(o + 4, &l.dif, 16); memcpy(o + 8, &l.spe, 16);
            if (l.pos.w == 0.0f) {
                GLfloat len = sqrtf(l.pos.x * l.pos.x + l.pos.y * l.pos.y + l.pos.z * l.pos.z); if (len == 0) len = 1;
                o[12] = l.pos.x / len; o[13] = l.pos.y / len; o[14] = l.pos.z / len; o[15] = 0;
            } else { o[12] = l.pos.x / l.pos.w; o[13] = l.pos.y / l.pos.w; o[14] = l.pos.z / l.pos.w; o[15] = 1; }
            GLfloat dl = sqrtf(l.dir.x * l.dir.x + l.dir.y * l.dir.y + l.dir.z * l.dir.z); if (dl == 0) dl = 1;
            o[16] = l.dir.x / dl; o[17] = l.dir.y / dl; o[18] = l.dir.z / dl;
            o[19] = cosf(l.spot_cut * 0.017453292519943295f);
            o[20] = l.k0; o[21] = l.k1; o[22] = l.k2; o[23] = l.spot_exp;
        }
        es.glUniform4fv(P->u_L, P->nl * 6, d); P->s_light = f.v_light;
    }
    if (P->u_M >= 0 && P->s_mat != f.v_mat) {
        GLfloat d[44];
        for (int s = 0; s < 2; ++s) {
            const Material &m = f.mat[s]; GLfloat *o = d + s * 20;
            memcpy(o, &m.amb, 16); memcpy(o + 4, &m.dif, 16); memcpy(o + 8, &m.spe, 16); memcpy(o + 12, &m.emi, 16);
            o[16] = m.shin; o[17] = o[18] = o[19] = 0;
        }
        memcpy(d + 40, &f.model_amb, 16);
        es.glUniform4fv(P->u_M, 11, d); P->s_mat = f.v_mat;
    }
    if (P->u_fog >= 0 && P->s_fog != f.v_fog) {
        GLfloat d[8]; memcpy(d, &f.fog_color, 16);
        GLfloat den = f.fog_end - f.fog_start;
        d[4] = den != 0.0f ? -1.0f / den : 0.0f; d[5] = den != 0.0f ? f.fog_end / den : 1.0f; d[6] = f.fog_density;
        d[7] = f.fog_mode == GL_EXP ? 1.0f : f.fog_mode == GL_EXP2 ? 2.0f : 0.0f;
        es.glUniform4fv(P->u_fog, 2, d); P->s_fog = f.v_fog;
    }
    if (P->u_aref >= 0 && P->s_alpha != f.v_alpha) { es.glUniform1f(P->u_aref, f.alpha_ref); P->s_alpha = f.v_alpha; }
    if (P->s_env != f.v_env) {
        for (int u = 0; u < P->key.nunits; ++u) if (P->u_ec[u] >= 0) es.glUniform4fv(P->u_ec[u], 1, &f.tu[u].env.color.x);
        P->s_env = f.v_env;
    }
    if (P->s_gen != f.v_gen) {
        for (int u = 0; u < P->key.nunits; ++u) if (P->u_tg[u] >= 0) {
            GLfloat d[32]; memcpy(d, f.tu[u].gen.obj, 64); memcpy(d + 16, f.tu[u].gen.eye, 64);
            es.glUniform4fv(P->u_tg[u], 8, d);
        }
        P->s_gen = f.v_gen;
    }
    if (P->u_clip >= 0 && P->s_clip != f.v_clip) { es.glUniform4fv(P->u_clip, MAX_CLIP_PLANES, &f.clip[0].x); P->s_clip = f.v_clip; }
    if (P->u_psz >= 0 && P->s_point != f.v_point) { es.glUniform1f(P->u_psz, f.point_size); P->s_point = f.v_point; }
}
} // namespace

Program *ffp_prepare(bool points) {
    Ffp &f = g.f;
    if (f.key_dirty || points != g_prog_points || !f.prog) {
        FfpKey k; build_key(k, points);
        f.prog = lookup(k);
        f.key_dirty = false; g_prog_points = points;
    }
    Program *P = f.prog;
    if (!P) return nullptr;
    es_use_program(P->id);
    upload(P);
    return P;
}

// Called once from ctx_init (current context, before the app draws). Chooses the cache directory, then loads every
// cached program for this driver; a stale or rejected binary is rebuilt from its key right away (still at startup).
void ffp_cache_init() {
    g_ffp_highp = env_on("ORYON_FFP_HIGHP");
    if (env_on("ORYON_NO_PROGRAM_CACHE")) return;
    GLint nf = 0;
    es.glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &nf);
    if (nf <= 0) { log("program cache off: driver exposes no program binary format"); return; }
    GLint *fm = (GLint *)calloc((size_t)nf, sizeof(GLint));
    if (!fm) return;
    es.glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, fm);
    g_nbin_fmt = nf < 16 ? nf : 16;
    memcpy(g_bin_fmt, fm, (size_t)g_nbin_fmt * sizeof(GLint));
    free(fm);
    const char *d = getenv("ORYON_CACHE_DIR"), *t = getenv("TMPDIR"), *hm = getenv("HOME");
    if (d && *d) snprintf(g_cache_dir, sizeof g_cache_dir, "%s", d);
    else if (t && *t) snprintf(g_cache_dir, sizeof g_cache_dir, "%s/oryon", t);
    else if (hm && *hm) {
        char parent[512]; snprintf(parent, sizeof parent, "%s/.cache", hm); mkdir(parent, 0700);
        snprintf(g_cache_dir, sizeof g_cache_dir, "%s/.cache/oryon", hm);
    } else { log("program cache off: no ORYON_CACHE_DIR, TMPDIR or HOME"); return; }
    mkdir(g_cache_dir, 0700);
    if (access(g_cache_dir, W_OK | X_OK) != 0) { log("program cache off: %s not writable", g_cache_dir); return; }
    const char *ids[] = {(const char *)es.glGetString(GL_VENDOR), (const char *)es.glGetString(GL_RENDERER),
                         (const char *)es.glGetString(GL_VERSION), (const char *)es.glGetString(GL_SHADING_LANGUAGE_VERSION), ORYON_VERSION};
    uint64_t h = fnv64(&kCacheVer, sizeof kCacheVer);
    for (const char *id : ids) if (id) h = fnv64(id, strlen(id) + 1, h);
    g_drv_hash = h;
    g_cache_on = true;
    const uint64_t t0 = now_us();
    int loaded = 0, rebuilt = 0, migrated = 0, dropped = 0;
    if (DIR *dp = opendir(g_cache_dir)) {
        char path[800];
        int seen = 0;
        while (struct dirent *de = readdir(dp)) {
            const char *n = de->d_name; size_t l = strlen(n);
            if (strncmp(n, "ffp-", 4)) continue;
            snprintf(path, sizeof path, "%s/%s", g_cache_dir, n);
            if (l > 4 && !strcmp(n + l - 4, ".tmp")) { unlink(path); continue; }      // interrupted write
            if (l < 8 || strcmp(n + l - 4, ".bin") || ++seen > 512) continue;
            CacheHdr H; FfpKey K;
            void *bin = cache_read(path, H, K);
            if (!bin) { unlink(path); ++dropped; continue; }
            FfpKey C = K; canon_key(C);
            if (memcmp(&C, &K, sizeof K)) {                  // key written by an older Oryon: move to its canonical form
                free(bin); unlink(path); ++migrated;
                char cp[600]; cache_path(C, cp, sizeof cp);
                CacheHdr H2; FfpKey K2;
                void *b2 = cache_read(cp, H2, K2);
                const bool own = b2 && !memcmp(&K2, &C, sizeof C);
                warm_key(C, own ? &H2 : nullptr, own ? b2 : nullptr, loaded, rebuilt);
                free(b2);
                continue;
            }
            if (!warm_key(K, &H, bin, loaded, rebuilt)) { unlink(path); ++dropped; }
            free(bin);
        }
        closedir(dp);
    }
    while (es.glGetError() != GL_NO_ERROR) {}
    log("program cache %s: %d loaded, %d rebuilt, %d migrated, %d dropped in %.1f ms", g_cache_dir, loaded, rebuilt, migrated, dropped,
        (now_us() - t0) / 1000.0);
}
} // namespace ory
