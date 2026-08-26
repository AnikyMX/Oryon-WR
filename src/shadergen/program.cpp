/* Cache program + unggah uniform.

   Cache memakai alamat terbuka dengan probing linear atas 128 slot. Minecraft
   1.12.2 secara realistis memakai belasan kombinasi state, jadi tabel ini
   praktis tidak pernah penuh dan pencarian selalu selesai di probe pertama. */

#include "shadergen.h"
#include "../driver/driver.h"
#include "../state/ff_state.h"
#include "../util/mat4.h"

#include <string.h>

namespace oryon {

GLuint g_user_program;

namespace {

enum { CACHE_BITS = 7, CACHE_SIZE = 1 << CACHE_BITS, SRC_CAP = 8192 };

FFProgram g_cache[CACHE_SIZE];
char      g_src[SRC_CAP];
GLuint    g_bound;

inline uint32_t hash_key(const FFKey &k) {
    const unsigned char *p = (const unsigned char *) &k;
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < sizeof(FFKey); ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* "u_lightPos3" tanpa snprintf: indeks kita selalu satu digit. */
void name_i(char *dst, const char *base, int i) {
    char *d = dst;
    while (*base) *d++ = *base++;
    *d++ = (char) ('0' + i);
    *d = 0;
}

GLuint compile(GLenum type, const char *src) {
    GLuint s = gles.glCreateShader(type);
    if (!s) return 0;
    const GLchar *p = src;
    gles.glShaderSource(s, 1, &p, 0);
    gles.glCompileShader(s);
    GLint ok = 0;
    gles.glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
#if defined(ORYON_DEBUG)
        char logbuf[1024];
        GLsizei n = 0;
        gles.glGetShaderInfoLog(s, sizeof logbuf, &n, logbuf);
        ORYON_LOG("kompilasi shader gagal:\n%s\n--- sumber ---\n%s", logbuf, src);
#endif
        gles.glDeleteShader(s);
        return 0;
    }
    return s;
}

void locate(FFProgram &p) {
    char n[24];
    const GLuint g = p.prog;
    p.u_mvp       = gles.glGetUniformLocation(g, "u_mvp");
    p.u_mv        = gles.glGetUniformLocation(g, "u_mv");
    p.u_nrm       = gles.glGetUniformLocation(g, "u_nrm");
    p.u_color     = gles.glGetUniformLocation(g, "u_color");
    p.u_alpha_ref = gles.glGetUniformLocation(g, "u_alphaRef");
    p.u_fog_color   = gles.glGetUniformLocation(g, "u_fogColor");
    p.u_fog_start   = gles.glGetUniformLocation(g, "u_fogStart");
    p.u_fog_end     = gles.glGetUniformLocation(g, "u_fogEnd");
    p.u_fog_density = gles.glGetUniformLocation(g, "u_fogDensity");
    p.u_lm_ambient  = gles.glGetUniformLocation(g, "u_lmAmbient");
    for (int i = 0; i < FF_MAX_LIGHTS; ++i) {
        name_i(n, "u_lightPos",  i); p.u_light_pos[i]  = gles.glGetUniformLocation(g, n);
        name_i(n, "u_lightDiff", i); p.u_light_diff[i] = gles.glGetUniformLocation(g, n);
        name_i(n, "u_lightAmb",  i); p.u_light_amb[i]  = gles.glGetUniformLocation(g, n);
    }
    for (int u = 0; u < FF_MAX_TEX; ++u) {
        name_i(n, "u_texMat", u); p.u_tex_mat[u] = gles.glGetUniformLocation(g, n);
        name_i(n, "u_tgS",    u); p.u_tg_s[u]    = gles.glGetUniformLocation(g, n);
        name_i(n, "u_tgT",    u); p.u_tg_t[u]    = gles.glGetUniformLocation(g, n);
    }
    /* Sampler dipatok sekali ke nomor unitnya; setelah ini tidak pernah berubah. */
    gles.glUseProgram(g);
    for (int u = 0; u < FF_MAX_TEX; ++u) {
        name_i(n, "u_tex", u);
        GLint loc = gles.glGetUniformLocation(g, n);
        if (loc >= 0) gles.glUniform1i(loc, u);
    }
    gles.glUseProgram(g_bound);
    p.mat_serial = 0;
    p.uni_serial = 0;
}

FFProgram *build(const FFKey &k, FFProgram &slot) {
    if (!ffp_gen_vertex(k, g_src, SRC_CAP)) return 0;
    GLuint vs = compile(GL_VERTEX_SHADER, g_src);
    if (!vs) return 0;
    if (!ffp_gen_fragment(k, g_src, SRC_CAP)) { gles.glDeleteShader(vs); return 0; }
    GLuint fs = compile(GL_FRAGMENT_SHADER, g_src);
    if (!fs) { gles.glDeleteShader(vs); return 0; }

    GLuint pr = gles.glCreateProgram();
    gles.glAttachShader(pr, vs);
    gles.glAttachShader(pr, fs);
    gles.glLinkProgram(pr);
    gles.glDeleteShader(vs);
    gles.glDeleteShader(fs);

    GLint ok = 0;
    gles.glGetProgramiv(pr, GL_LINK_STATUS, &ok);
    if (!ok) {
#if defined(ORYON_DEBUG)
        char logbuf[1024];
        GLsizei n = 0;
        gles.glGetProgramInfoLog(pr, sizeof logbuf, &n, logbuf);
        ORYON_LOG("tautan program gagal:\n%s", logbuf);
#endif
        gles.glDeleteProgram(pr);
        return 0;
    }

    slot.key  = k;
    slot.prog = pr;
    slot.used = true;
    locate(slot);
    return &slot;
}

}  /* namespace */

FFProgram *ffp_program(const FFKey &k) {
    uint32_t i = hash_key(k) & (CACHE_SIZE - 1);
    for (unsigned probe = 0; probe < CACHE_SIZE; ++probe) {
        FFProgram &s = g_cache[i];
        if (!s.used) return build(k, s);
        if (memcmp(&s.key, &k, sizeof k) == 0) return &s;
        i = (i + 1) & (CACHE_SIZE - 1);
    }
    return 0;   /* tabel penuh: mustahil dalam praktik, tapi jangan berputar */
}

void ffp_invalidate() {
    g_bound = 0;
    /* Program pengguna mungkin menimpa uniform kita, jadi seluruh cache
       pengunggahan ikut dibatalkan. */
    for (unsigned i = 0; i < CACHE_SIZE; ++i) {
        g_cache[i].mat_serial = 0;
        g_cache[i].uni_serial = 0;
    }
}

void ffp_reset() {
    memset(g_cache, 0, sizeof g_cache);
    g_bound = 0;
}

bool ffp_bind(const FFKey &k) {
    FFProgram *p = ffp_program(k);
    if (!p) return false;

    if (g_bound != p->prog) {
        gles.glUseProgram(p->prog);
        g_bound = p->prog;
    }

    /* Blok matriks: hanya diunggah ulang bila ada matriks yang berubah. */
    if (p->mat_serial != g_ff.serial) {
        p->mat_serial = g_ff.serial;
        if (p->u_mvp >= 0) {
            float mvp[16];
            mat4_mul(mvp, g_ff.pr[g_ff.pr_top], g_ff.mv[g_ff.mv_top]);
            gles.glUniformMatrix4fv(p->u_mvp, 1, GL_FALSE, mvp);
        }
        if (p->u_mv >= 0)
            gles.glUniformMatrix4fv(p->u_mv, 1, GL_FALSE, g_ff.mv[g_ff.mv_top]);
        if (p->u_nrm >= 0) {
            float nrm[9];
            mat4_normal_matrix(nrm, g_ff.mv[g_ff.mv_top]);
            gles.glUniformMatrix3fv(p->u_nrm, 1, GL_FALSE, nrm);
        }
        for (int u = 0; u < FF_MAX_TEX; ++u)
            if (p->u_tex_mat[u] >= 0)
                gles.glUniformMatrix4fv(p->u_tex_mat[u], 1, GL_FALSE, ff_tex_matrix(u));
    }

    /* Blok sisanya: warna, uji alpha, kabut, cahaya, bidang texgen.
       Dulu semuanya diunggah pada setiap draw call - sampai 50 panggilan
       glUniform per gambar. Sekarang hanya saat state-nya benar-benar berubah. */
    if (p->uni_serial != g_ff.uni_serial) {
        p->uni_serial = g_ff.uni_serial;
        if (p->u_color >= 0)     gles.glUniform4fv(p->u_color, 1, g_ff.a.cur_color);
        if (p->u_alpha_ref >= 0) gles.glUniform1f(p->u_alpha_ref, g_ff.a.alpha_ref);

        if (p->u_fog_color >= 0) {
            gles.glUniform4fv(p->u_fog_color, 1, g_ff.a.fog_color);
            gles.glUniform1f(p->u_fog_start, g_ff.a.fog_start);
            gles.glUniform1f(p->u_fog_end, g_ff.a.fog_end);
            gles.glUniform1f(p->u_fog_density, g_ff.a.fog_density);
        }

        if (p->u_lm_ambient >= 0) {
            gles.glUniform4fv(p->u_lm_ambient, 1, g_ff.a.lm_ambient);
            for (int i = 0; i < FF_MAX_LIGHTS; ++i) {
                if (p->u_light_pos[i] < 0) continue;
                gles.glUniform4fv(p->u_light_pos[i],  1, g_ff.a.light[i].position);
                gles.glUniform4fv(p->u_light_diff[i], 1, g_ff.a.light[i].diffuse);
                gles.glUniform4fv(p->u_light_amb[i],  1, g_ff.a.light[i].ambient);
            }
        }

        for (int u = 0; u < FF_MAX_TEX; ++u) {
            const TexEnv &t = g_ff.a.tex[u];
            if (p->u_tg_s[u] >= 0) gles.glUniform4fv(p->u_tg_s[u], 1, t.gen_plane[0]);
            if (p->u_tg_t[u] >= 0) gles.glUniform4fv(p->u_tg_t[u], 1, t.gen_plane[1]);
        }
    }
    return true;
}

}  /* namespace oryon */
