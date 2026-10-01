// Oryon -- application shaders/programs: desktop GLSL translation at glShaderSource, link-time attribute
// aliasing (first user attribute -> location 0 when gl_Vertex is unused, as desktop drivers do), uniform
// initializer defaults, and versioned upload of the fixed-function builtins (gl_ModelViewMatrix, ...).
#include "oryon.hpp"
#include "bind.hpp"
#include "mathx.hpp"
#include "glsl.hpp"
#include <stdio.h>

namespace ory {
namespace {
struct AppProg {
    GLuint id; AppProg *next;
    GLuint shaders[8]; int nsh;
    char bound[8][64]; int nbound;                  // explicit glBindAttribLocation names
    uint32_t reads;                                 // legacy locations read
    GLint u_mv, u_p, u_mvp, u_nm, u_tm, u_mvi, u_pi, u_mvpi, u_fogc, u_fogd, u_fogs, u_foge, u_fogsc;
    uint32_t s_mv, s_p, s_mvp_mv, s_mvp_p, s_nm, s_mvi, s_pi, s_mvpi_mv, s_mvpi_p, s_fog, s_tm;
};
ShaderInfo *g_sh[256];
AppProg *g_pr[256];
AppProg *g_cur = nullptr;

ShaderInfo *sh_get(GLuint id, bool create) {
    ShaderInfo **b = &g_sh[id & 255];
    for (ShaderInfo *s = *b; s; s = s->next) if (s->id == id) return s;
    if (!create) return nullptr;
    ShaderInfo *s = (ShaderInfo *)calloc(1, sizeof(ShaderInfo));
    if (!s) return nullptr;
    s->id = id; s->next = *b; *b = s;
    return s;
}
void sh_free(GLuint id) {
    ShaderInfo **b = &g_sh[id & 255];
    for (ShaderInfo **p = b; *p; p = &(*p)->next)
        if ((*p)->id == id) { ShaderInfo *s = *p; *p = s->next; free(s->defs); free(s); return; }
}
AppProg *pr_get(GLuint id, bool create) {
    AppProg **b = &g_pr[id & 255];
    for (AppProg *p = *b; p; p = p->next) if (p->id == id) return p;
    if (!create) return nullptr;
    AppProg *p = (AppProg *)calloc(1, sizeof(AppProg));
    if (!p) return nullptr;
    p->id = id; p->next = *b; *b = p;
    p->u_mv = p->u_p = p->u_mvp = p->u_nm = p->u_tm = p->u_mvi = p->u_pi = p->u_mvpi = -1;
    p->u_fogc = p->u_fogd = p->u_fogs = p->u_foge = p->u_fogsc = -1;
    p->reads = 1u << LOC_POS;
    return p;
}
void pr_free(GLuint id) {
    AppProg **b = &g_pr[id & 255];
    for (AppProg **p = b; *p; p = &(*p)->next)
        if ((*p)->id == id) { AppProg *a = *p; *p = a->next; if (g_cur == a) g_cur = nullptr; free(a); return; }
}
void apply_defaults(GLuint prog, const ShaderInfo *s) {
    for (int i = 0; i < s->ndefs; ++i) {
        const UniformDefault &d = s->defs[i];
        GLint loc = es.glGetUniformLocation(prog, d.name);
        if (loc < 0) continue;
        const char *t = d.tname;
        if (!strncmp(t, "mat", 3)) {
            int dim = t[3] - '0'; GLfloat m[16] = {0};
            if (d.count == 1) for (int k = 0; k < dim; ++k) m[k * dim + k] = d.v[0];
            else memcpy(m, d.v, sizeof(GLfloat) * (size_t)(d.count < 16 ? d.count : 16));
            if (dim == 2) es.glUniformMatrix2fv(loc, 1, GL_FALSE, m);
            else if (dim == 3) es.glUniformMatrix3fv(loc, 1, GL_FALSE, m);
            else es.glUniformMatrix4fv(loc, 1, GL_FALSE, m);
        } else if (!strcmp(t, "int") || !strcmp(t, "bool") || !strncmp(t, "ivec", 4) || !strncmp(t, "bvec", 4)) {
            GLint iv[4] = {0}; for (int k = 0; k < d.count && k < 4; ++k) iv[k] = (GLint)d.v[k];
            int c = t[0] == 'i' && t[1] == 'n' ? 1 : t[0] == 'b' && t[1] == 'o' ? 1 : t[4] - '0';
            if (c == 1) es.glUniform1iv(loc, 1, iv); else if (c == 2) es.glUniform2iv(loc, 1, iv);
            else if (c == 3) es.glUniform3iv(loc, 1, iv); else es.glUniform4iv(loc, 1, iv);
        } else {
            int c = !strcmp(t, "float") ? 1 : t[3] - '0';
            GLfloat v[4] = {0};
            for (int k = 0; k < 4; ++k) v[k] = d.count == 1 ? d.v[0] : (k < d.count ? d.v[k] : 0.0f);
            if (c == 1) es.glUniform1fv(loc, 1, v); else if (c == 2) es.glUniform2fv(loc, 1, v);
            else if (c == 3) es.glUniform3fv(loc, 1, v); else es.glUniform4fv(loc, 1, v);
        }
    }
}
GLint uloc(GLuint p, const char *n) { return es.glGetUniformLocation(p, n); }
} // namespace

uint32_t app_program_prepare() {
    GLuint id = g.b.app_program;
    es_use_program(id);
    AppProg *P = g_cur && g_cur->id == id ? g_cur : (g_cur = pr_get(id, false));
    if (!P) return (1u << LOC_POS) | (1u << LOC_COLOR) | (1u << LOC_NORMAL) | (0xFFu << LOC_TEX0);
    Matrices &M = g.m;
    if (P->u_mv >= 0 && P->s_mv != M.mv_ver) { es.glUniformMatrix4fv(P->u_mv, 1, GL_FALSE, M.mv[M.mv_top].m); P->s_mv = M.mv_ver; }
    if (P->u_p >= 0 && P->s_p != M.p_ver) { es.glUniformMatrix4fv(P->u_p, 1, GL_FALSE, M.p[M.p_top].m); P->s_p = M.p_ver; }
    if ((P->u_mvp >= 0 && (P->s_mvp_mv != M.mv_ver || P->s_mvp_p != M.p_ver)) ||
        (P->u_mvpi >= 0 && (P->s_mvpi_mv != M.mv_ver || P->s_mvpi_p != M.p_ver))) {
        if (M.mvp_mv != M.mv_ver || M.mvp_p != M.p_ver) { mat_mul(M.mvp.m, M.p[M.p_top].m, M.mv[M.mv_top].m); M.mvp_mv = M.mv_ver; M.mvp_p = M.p_ver; }
        if (P->u_mvp >= 0) { es.glUniformMatrix4fv(P->u_mvp, 1, GL_FALSE, M.mvp.m); P->s_mvp_mv = M.mv_ver; P->s_mvp_p = M.p_ver; }
        if (P->u_mvpi >= 0) { GLfloat inv[16]; mat_invert(inv, M.mvp.m); es.glUniformMatrix4fv(P->u_mvpi, 1, GL_FALSE, inv); P->s_mvpi_mv = M.mv_ver; P->s_mvpi_p = M.p_ver; }
    }
    if (P->u_nm >= 0 && P->s_nm != M.mv_ver) { GLfloat nm[9]; mat_normal(nm, M.mv[M.mv_top].m, false); es.glUniformMatrix3fv(P->u_nm, 1, GL_FALSE, nm); P->s_nm = M.mv_ver; }
    if (P->u_mvi >= 0 && P->s_mvi != M.mv_ver) { GLfloat inv[16]; mat_invert(inv, M.mv[M.mv_top].m); es.glUniformMatrix4fv(P->u_mvi, 1, GL_FALSE, inv); P->s_mvi = M.mv_ver; }
    if (P->u_pi >= 0 && P->s_pi != M.p_ver) { GLfloat inv[16]; mat_invert(inv, M.p[M.p_top].m); es.glUniformMatrix4fv(P->u_pi, 1, GL_FALSE, inv); P->s_pi = M.p_ver; }
    if (P->u_tm >= 0) {
        uint32_t sum = 0; for (int u = 0; u < MAX_TEX_UNITS; ++u) sum += M.t_ver[u] * (uint32_t)(u + 1);
        if (P->s_tm != sum) {
            GLfloat tm[16 * MAX_TEX_UNITS];
            for (int u = 0; u < MAX_TEX_UNITS; ++u) memcpy(tm + 16 * u, M.t[u][M.t_top[u]].m, 64);
            es.glUniformMatrix4fv(P->u_tm, MAX_TEX_UNITS, GL_FALSE, tm); P->s_tm = sum;
        }
    }
    if (P->u_fogc >= 0 || P->u_fogd >= 0) {
        const Ffp &f = g.f;
        if (P->s_fog != f.v_fog) {
            if (P->u_fogc >= 0) es.glUniform4fv(P->u_fogc, 1, &f.fog_color.x);
            if (P->u_fogd >= 0) es.glUniform1f(P->u_fogd, f.fog_density);
            if (P->u_fogs >= 0) es.glUniform1f(P->u_fogs, f.fog_start);
            if (P->u_foge >= 0) es.glUniform1f(P->u_foge, f.fog_end);
            if (P->u_fogsc >= 0) es.glUniform1f(P->u_fogsc, f.fog_end != f.fog_start ? 1.0f / (f.fog_end - f.fog_start) : 0.0f);
            P->s_fog = f.v_fog;
        }
    }
    return P->reads;
}
} // namespace ory

using namespace ory;

/* jar: GL20C.glCreateShader(I)I */
OGL_EXPORT GLuint glCreateShader(GLenum type) {
    ORY_PROLOGUE();
    GLuint s = es.glCreateShader(type);
    if (s) if (ShaderInfo *i = sh_get(s, true)) { i->stage = type; i->translated = false; }
    return s;
}
/* jar: GL20C.glDeleteShader(I)V */
static bool sh_referenced(GLuint shader) {
    for (AppProg *b : g_pr) for (AppProg *p = b; p; p = p->next) for (int i = 0; i < p->nsh; ++i) if (p->shaders[i] == shader) return true;
    return false;
}
/* jar: GL20C.glDeleteShader(I)V */
OGL_EXPORT void glDeleteShader(GLuint shader) {
    ORY_PROLOGUE();
    es.glDeleteShader(shader);
    if (shader && !sh_referenced(shader)) sh_free(shader);   // attached shaders keep their info until detached
}
/* jar: GL20C.nglShaderSource(IIJJ)V */
OGL_EXPORT void glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length) {
    ORY_PROLOGUE();
    if (count <= 0 || !string) { es.glShaderSource(shader, count, string, length); return; }
    size_t tot = 0;
    for (GLsizei i = 0; i < count; ++i) tot += string[i] ? (length && length[i] >= 0 ? (size_t)length[i] : strlen(string[i])) : 0;
    char *src = (char *)malloc(tot + 1);
    if (!src) { set_error(GL_OUT_OF_MEMORY); return; }
    size_t o = 0;
    for (GLsizei i = 0; i < count; ++i) {
        if (!string[i]) continue;
        size_t l = length && length[i] >= 0 ? (size_t)length[i] : strlen(string[i]);
        memcpy(src + o, string[i], l); o += l;
    }
    src[o] = 0;
    ShaderInfo *info = sh_get(shader, true);
    GLint type = 0;
    if (info && !info->stage) { es.glGetShaderiv(shader, GL_SHADER_TYPE, &type); info->stage = (GLenum)type; }
    if (info) { free(info->defs); info->defs = nullptr; info->ndefs = 0; info->translated = false; info->builtins = 0; info->nattrs = 0; }
    char *tr = info ? glsl_translate(src, info->stage, info) : nullptr;
    const GLchar *p = tr ? tr : src;
    es.glShaderSource(shader, 1, &p, nullptr);
    if (env_on("ORYON_DUMP_GLSL") && tr) log("translated shader %u:\n%s", shader, tr);
    free(tr); free(src);
}
/* jar: GL20C.glAttachShader(II)V */
OGL_EXPORT void glAttachShader(GLuint program, GLuint shader) {
    ORY_PROLOGUE();
    es.glAttachShader(program, shader);
    if (AppProg *P = pr_get(program, true)) {
        for (int i = 0; i < P->nsh; ++i) if (P->shaders[i] == shader) return;
        if (P->nsh < 8) P->shaders[P->nsh++] = shader;
    }
}
/* jar: GL20C.glDetachShader(II)V */
OGL_EXPORT void glDetachShader(GLuint program, GLuint shader) {
    ORY_PROLOGUE();
    es.glDetachShader(program, shader);
    if (AppProg *P = pr_get(program, false))
        for (int i = 0; i < P->nsh; ++i) if (P->shaders[i] == shader) { P->shaders[i] = P->shaders[--P->nsh]; break; }
}
static const char *mapped_name(const GLchar *name, char *buf, size_t cap) {
    if (glsl_reserved(name)) { snprintf(buf, cap, "oryon_%s", name); return buf; }
    return name;
}
/* jar: GL20C.nglBindAttribLocation(IIJ)V */
OGL_EXPORT void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {
    ORY_PROLOGUE();
    char b[80];
    es.glBindAttribLocation(program, index, mapped_name(name, b, sizeof b));
    if (AppProg *P = pr_get(program, true)) if (P->nbound < 8) { strncpy(P->bound[P->nbound], name, 63); P->bound[P->nbound][63] = 0; ++P->nbound; }
}
/* jar: GL20C.glLinkProgram(I)V */
OGL_EXPORT void glLinkProgram(GLuint program) {
    ORY_PROLOGUE();
    StatTimer st_(g.st.link_n, g.st.link_us, &g.st.link_max);
    AppProg *P = pr_get(program, true);
    uint32_t reads = 1u << LOC_POS;
    if (P) {
        for (int i = 0; i < P->nsh; ++i) {
            const ShaderInfo *s = sh_get(P->shaders[i], false);
            if (!s || s->stage != GL_VERTEX_SHADER) continue;
            uint32_t b = s->builtins;
            if (b & GB_NORMAL) reads |= 1u << LOC_NORMAL;
            if (b & GB_COLOR) reads |= 1u << LOC_COLOR;
            if (b & GB_SECCOLOR) reads |= 1u << LOC_COLOR2;
            if (b & GB_FOGCOORD) reads |= 1u << LOC_FOG;
            reads |= (uint32_t)s->multitex << LOC_TEX0;
            if (!(b & GB_VERTEX) && s->nattrs > 0) {          // desktop aliasing: first attribute feeds from glVertexPointer
                bool explicit_ = false;
                for (int k = 0; k < P->nbound; ++k) if (!strcmp(P->bound[k], s->attrs[0])) explicit_ = true;
                bool zero_taken = false;
                for (int k = 0; k < P->nbound; ++k) zero_taken |= es.glGetAttribLocation(program, P->bound[k]) == 0;
                if (!explicit_ && !zero_taken) { char nb[80]; es.glBindAttribLocation(program, 0, mapped_name(s->attrs[0], nb, sizeof nb)); }
            }
        }
    }
    es.glLinkProgram(program);
    GLint ok = 0; es.glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok || !P) return;
    P->reads = reads;
    P->u_mv = uloc(program, "oryon_ModelViewMatrix"); P->u_p = uloc(program, "oryon_ProjectionMatrix");
    P->u_mvp = uloc(program, "oryon_ModelViewProjectionMatrix"); P->u_nm = uloc(program, "oryon_NormalMatrix");
    P->u_tm = uloc(program, "oryon_TextureMatrix"); P->u_mvi = uloc(program, "oryon_ModelViewMatrixInverse");
    P->u_pi = uloc(program, "oryon_ProjectionMatrixInverse"); P->u_mvpi = uloc(program, "oryon_ModelViewProjectionMatrixInverse");
    P->u_fogc = uloc(program, "oryon_Fog.color"); P->u_fogd = uloc(program, "oryon_Fog.density");
    P->u_fogs = uloc(program, "oryon_Fog.start"); P->u_foge = uloc(program, "oryon_Fog.end"); P->u_fogsc = uloc(program, "oryon_Fog.scale");
    P->s_mv = P->s_p = P->s_mvp_mv = P->s_mvp_p = P->s_nm = P->s_mvi = P->s_pi = P->s_mvpi_mv = P->s_mvpi_p = P->s_fog = P->s_tm = 0;
    bool defaults = false;
    for (int i = 0; i < P->nsh; ++i) if (const ShaderInfo *s = sh_get(P->shaders[i], false)) defaults |= s->ndefs > 0;
    if (defaults) {
        GLuint prev = g.b.es_program;
        es.glUseProgram(program);
        for (int i = 0; i < P->nsh; ++i) if (const ShaderInfo *s = sh_get(P->shaders[i], false)) apply_defaults(program, s);
        es.glUseProgram(prev);
    }
}
/* jar: GL20C.glUseProgram(I)V */
OGL_EXPORT void glUseProgram(GLuint program) {
    ORY_PROLOGUE();
    g.b.app_program = program;
    if (program) es_use_program(program);            // eager: glUniform* then targets the right program
}
/* jar: GL20C.glDeleteProgram(I)V */
OGL_EXPORT void glDeleteProgram(GLuint program) {
    ORY_PROLOGUE();
    es.glDeleteProgram(program);
    if (program && program != g.b.app_program) pr_free(program);
}
/* jar: GL20C.nglGetUniformLocation(IJ)I */
OGL_EXPORT GLint glGetUniformLocation(GLuint program, const GLchar *name) {
    ORY_PROLOGUE();
    GLint l = es.glGetUniformLocation(program, name);
    if (l < 0 && glsl_reserved(name)) { char b[80]; l = es.glGetUniformLocation(program, mapped_name(name, b, sizeof b)); }
    return l;
}
/* jar: GL20C.nglGetAttribLocation(IJ)I */
OGL_EXPORT GLint glGetAttribLocation(GLuint program, const GLchar *name) {
    ORY_PROLOGUE();
    GLint l = es.glGetAttribLocation(program, name);
    if (l < 0 && glsl_reserved(name)) { char b[80]; l = es.glGetAttribLocation(program, mapped_name(name, b, sizeof b)); }
    return l;
}
/* jar: ARBShaderObjects.glDeleteObjectARB(I)V */
OGL_EXPORT void glDeleteObjectARB(GLhandleARB obj) {
    ORY_PROLOGUE();
    if (es.glIsShader(obj)) es.glDeleteShader(obj);
    else if (es.glIsProgram(obj)) { es.glDeleteProgram(obj); if (obj != g.b.app_program) pr_free(obj); }
}
/* jar: ARBShaderObjects.glGetHandleARB(I)I */
OGL_EXPORT GLhandleARB glGetHandleARB(GLenum pname) {
    ORY_PROLOGUE();
    return pname == GL_PROGRAM_OBJECT_ARB ? g.b.app_program : 0;
}
static void object_param(GLhandleARB obj, GLenum pname, GLint *params) {
    bool sh = es.glIsShader(obj) == GL_TRUE;
    switch (pname) {
    case GL_OBJECT_TYPE_ARB: *params = sh ? GL_SHADER_OBJECT_ARB : GL_PROGRAM_OBJECT_ARB; return;
    case GL_OBJECT_SUBTYPE_ARB: if (sh) es.glGetShaderiv(obj, GL_SHADER_TYPE, params); else *params = 0; return;
    default: break;
    }
    if (sh) es.glGetShaderiv(obj, pname, params); else es.glGetProgramiv(obj, pname, params);
}
/* jar: ARBShaderObjects.nglGetObjectParameterivARB(IIJ)V */
OGL_EXPORT void glGetObjectParameterivARB(GLhandleARB obj, GLenum pname, GLint *params) { ORY_PROLOGUE(); object_param(obj, pname, params); }
/* jar: ARBShaderObjects.nglGetObjectParameterfvARB(IIJ)V */
OGL_EXPORT void glGetObjectParameterfvARB(GLhandleARB obj, GLenum pname, GLfloat *params) {
    ORY_PROLOGUE(); GLint i = 0; object_param(obj, pname, &i); *params = (GLfloat)i;
}
/* jar: ARBShaderObjects.nglGetInfoLogARB(IIJJ)V */
OGL_EXPORT void glGetInfoLogARB(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog) {
    ORY_PROLOGUE();
    if (es.glIsShader(obj)) es.glGetShaderInfoLog(obj, maxLength, length, infoLog);
    else es.glGetProgramInfoLog(obj, maxLength, length, infoLog);
}
