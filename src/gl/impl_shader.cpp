/* Program dan shader milik Minecraft sendiri.

   1.12.2 memuat 35 shader GLSL 1.20 dari assets/minecraft/shaders untuk efek
   pasca-proses. Semuanya lewat sini: diterjemahkan ke GLSL ES 3.20, lalu
   atribut pertamanya diikat ke lokasi 0 supaya glVertexPointer tetap
   menyuapinya - persis perilaku alias atribut 0 di driver desktop.

   ORYON_IMPL(glShaderSource)
   ORYON_IMPL(glShaderSourceARB)
   ORYON_IMPL(glLinkProgram)
   ORYON_IMPL(glLinkProgramARB)
   ORYON_IMPL(glUseProgram)
   ORYON_IMPL(glUseProgramObjectARB)
   ORYON_IMPL(glCreateShaderObjectARB)
   ORYON_IMPL(glCreateProgramObjectARB)
   ORYON_IMPL(glAttachObjectARB)
   ORYON_IMPL(glDeleteObjectARB)
   ORYON_IMPL(glGetObjectParameterivARB)
   ORYON_IMPL(glGetInfoLogARB)
*/

#include "oryon/oryon.h"
#include "../driver/driver.h"
#include "../shadergen/shadergen.h"
#include "../shadergen/translate.h"
#include "../state/state.h"

#include <stdlib.h>
#include <string.h>

using namespace oryon;

namespace {

/* Shader terbesar milik Minecraft di bawah 5 KB dan paling banyak membawa
   tujuh uniform ber-nilai-bawaan, jadi batas ini sudah lapang berkali lipat.
   Angkanya sengaja ditekan: seluruh buffer ini menempati .bss selamanya. */
enum { SRC_CAP = 32768, ATTR_CAP = 64, MAP_SIZE = 16, DEF_CAP = 12 };

char  s_join[SRC_CAP];
char  s_xlat[SRC_CAP];

/* shader -> nama atribut pertamanya, dipakai saat menautkan program. */
struct ShaderInfo {
    GLuint      shader;
    char        attrib[ATTR_CAP];
    GlslDefault defs[DEF_CAP];
    unsigned    ndef;
};
ShaderInfo s_map[MAP_SIZE];

ShaderInfo *slot_for(GLuint shader) {
    ShaderInfo *free_slot = 0;
    for (int i = 0; i < MAP_SIZE; ++i) {
        if (s_map[i].shader == shader) return &s_map[i];
        if (!s_map[i].shader && !free_slot) free_slot = &s_map[i];
    }
    return free_slot;
}

ShaderInfo *recall(GLuint shader) {
    for (int i = 0; i < MAP_SIZE; ++i)
        if (s_map[i].shader == shader) return &s_map[i];
    return 0;
}

void source(GLuint shader, GLsizei count, const GLchar *const *string,
            const GLint *length) {
    if (!ensure_init() || !string || count <= 0) return;

    unsigned n = 0;
    for (GLsizei i = 0; i < count; ++i) {
        if (!string[i]) continue;
        const unsigned len = (length && length[i] >= 0)
                           ? (unsigned) length[i] : (unsigned) strlen(string[i]);
        if (n + len >= SRC_CAP) break;
        memcpy(s_join + n, string[i], len);
        n += len;
    }
    s_join[n] = 0;

    GLint type = 0;
    gles.glGetShaderiv(shader, GL_SHADER_TYPE, &type);
    const bool fragment = ((GLenum) type == GL_FRAGMENT_SHADER);

    char attrib[ATTR_CAP];
    GlslDefault defs[DEF_CAP];
    unsigned ndef = 0;
    const unsigned m = glsl_translate(s_join, n, fragment, s_xlat, SRC_CAP,
                                      attrib, ATTR_CAP, defs, DEF_CAP, &ndef);
    if (!m) {
        ORYON_LOG("terjemahan GLSL gagal (%u byte), sumber asli diteruskan", n);
        const GLchar *p = s_join;
        gles.glShaderSource(shader, 1, &p, 0);
        return;
    }
    ShaderInfo *info = slot_for(shader);
    if (info) {
        info->shader = shader;
        info->attrib[0] = 0;
        if (!fragment && attrib[0]) {
            strncpy(info->attrib, attrib, ATTR_CAP - 1);
            info->attrib[ATTR_CAP - 1] = 0;
        }
        info->ndef = ndef;
        for (unsigned i = 0; i < ndef; ++i) info->defs[i] = defs[i];
    }

    const GLchar *p = s_xlat;
    gles.glShaderSource(shader, 1, &p, 0);
}

void link(GLuint program) {
    if (!ensure_init()) return;
    /* Minecraft menyuapi shader-nya lewat glVertexPointer, yang di Oryon
       selalu berujung di lokasi atribut 0. Di driver desktop hal ini bekerja
       karena atribut generik 0 beralias dengan gl_Vertex; di sini diikat
       secara eksplisit supaya tidak bergantung pada keberuntungan linker. */
    GLuint shaders[8];
    GLsizei n = 0;
    gles.glGetAttachedShaders(program, 8, &n, shaders);
    for (GLsizei i = 0; i < n; ++i) {
        const ShaderInfo *info = recall(shaders[i]);
        if (info && info->attrib[0]) {
            gles.glBindAttribLocation(program, ATTR_POS, info->attrib);
            break;
        }
    }
    gles.glLinkProgram(program);

    GLint linked = 0;
    gles.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) return;

    /* Memasang kembali nilai bawaan uniform yang GLSL ES tidak izinkan ditulis
       di deklarasi. Tanpa langkah ini Saturation, ConvergeX, dan kawan-kawan
       akan bernilai nol, dan efek pasca-proses tampil salah - bukan gagal,
       yang justru lebih sulit dilacak. */
    GLint prev = 0;
    gles.glGetIntegerv(GL_CURRENT_PROGRAM, &prev);
    gles.glUseProgram(program);
    for (GLsizei i = 0; i < n; ++i) {
        const ShaderInfo *info = recall(shaders[i]);
        if (!info) continue;
        for (unsigned d = 0; d < info->ndef; ++d) {
            const GlslDefault &g = info->defs[d];
            const GLint loc = gles.glGetUniformLocation(program, g.name);
            if (loc < 0) continue;
            switch (g.comps) {
            case 1: gles.glUniform1f(loc, g.v[0]); break;
            case 2: gles.glUniform2f(loc, g.v[0], g.v[1]); break;
            case 3: gles.glUniform3f(loc, g.v[0], g.v[1], g.v[2]); break;
            default: gles.glUniform4f(loc, g.v[0], g.v[1], g.v[2], g.v[3]); break;
            }
        }
    }
    gles.glUseProgram((GLuint) prev);
}

void use(GLuint program) {
    if (!ensure_init()) return;
    g_user_program = program;
    ffp_invalidate();
    gles.glUseProgram(program);
}

}  /* namespace */

ORYON_API void glShaderSource(GLuint shader, GLsizei count,
                              const GLchar *const *string, const GLint *length) {
    source(shader, count, string, length);
}

ORYON_API void glShaderSourceARB(GLhandleARB shader, GLsizei count,
                                 const GLcharARB **string, const GLint *length) {
    source((GLuint) shader, count, (const GLchar *const *) string, length);
}

ORYON_API void glLinkProgram(GLuint program)          { link(program); }
ORYON_API void glLinkProgramARB(GLhandleARB program)  { link((GLuint) program); }
ORYON_API void glUseProgram(GLuint program)           { use(program); }
ORYON_API void glUseProgramObjectARB(GLhandleARB p)   { use((GLuint) p); }

/* ------------------------------------------------- model objek ARB ------- */
/* ARB memakai satu ruang handle untuk shader dan program. GLES memisahkannya,
   jadi tujuannya ditentukan saat dipanggil lewat glIsShader/glIsProgram. */

ORYON_API GLhandleARB glCreateShaderObjectARB(GLenum shaderType) {
    if (!ensure_init()) return 0;
    return (GLhandleARB) gles.glCreateShader(shaderType);
}

ORYON_API GLhandleARB glCreateProgramObjectARB(void) {
    if (!ensure_init()) return 0;
    return (GLhandleARB) gles.glCreateProgram();
}

ORYON_API void glAttachObjectARB(GLhandleARB containerObj, GLhandleARB obj) {
    if (ensure_init()) gles.glAttachShader((GLuint) containerObj, (GLuint) obj);
}

ORYON_API void glDeleteObjectARB(GLhandleARB obj) {
    if (!ensure_init()) return;
    const GLuint h = (GLuint) obj;
    if (gles.glIsShader(h))       gles.glDeleteShader(h);
    else if (gles.glIsProgram(h)) gles.glDeleteProgram(h);
}

ORYON_API void glGetObjectParameterivARB(GLhandleARB obj, GLenum pname,
                                         GLint *params) {
    if (!ensure_init() || !params) return;
    const GLuint h = (GLuint) obj;
    const bool is_shader = gles.glIsShader(h) != 0;
    switch (pname) {
    case GL_OBJECT_COMPILE_STATUS_ARB:
        gles.glGetShaderiv(h, GL_COMPILE_STATUS, params); return;
    case GL_OBJECT_LINK_STATUS_ARB:
        gles.glGetProgramiv(h, GL_LINK_STATUS, params); return;
    case GL_OBJECT_INFO_LOG_LENGTH_ARB:
        if (is_shader) gles.glGetShaderiv(h, GL_INFO_LOG_LENGTH, params);
        else           gles.glGetProgramiv(h, GL_INFO_LOG_LENGTH, params);
        return;
    default:
        if (is_shader) gles.glGetShaderiv(h, pname, params);
        else           gles.glGetProgramiv(h, pname, params);
        return;
    }
}

ORYON_API void glGetInfoLogARB(GLhandleARB obj, GLsizei maxLength,
                               GLsizei *length, GLcharARB *infoLog) {
    if (!ensure_init() || !infoLog) return;
    const GLuint h = (GLuint) obj;
    if (gles.glIsShader(h)) gles.glGetShaderInfoLog(h, maxLength, length, infoLog);
    else                    gles.glGetProgramInfoLog(h, maxLength, length, infoLog);
}
