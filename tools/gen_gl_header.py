#!/usr/bin/env python3
"""
gen_gl_header.py — membangun include/oryon/gl.h yang MANDIRI.

Android NDK tidak menyediakan header GL desktop, dan menyertakan header GLES
sistem di TU yang sama akan bentrok. Maka Oryon membawa definisinya sendiri,
diambil apa adanya dari header sistem sandbox supaya nilainya mustahil melenceng.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

GUARDS = [
    "GL_VERSION_1_2", "GL_VERSION_1_3", "GL_VERSION_1_4", "GL_VERSION_1_5",
    "GL_VERSION_2_0", "GL_VERSION_2_1", "GL_VERSION_3_0",
    "GL_VERSION_3_1", "GL_VERSION_3_2", "GL_VERSION_3_3", "GL_VERSION_4_3",
    "GL_KHR_debug", "GL_ARB_seamless_cube_map", "GL_ARB_texture_multisample",
    "GL_ARB_sync", "GL_ARB_uniform_buffer_object", "GL_ARB_sampler_objects",
    "GL_ARB_multitexture", "GL_ARB_vertex_buffer_object",
    "GL_ARB_shader_objects", "GL_ARB_vertex_shader", "GL_ARB_fragment_shader",
    "GL_ARB_framebuffer_object", "GL_EXT_framebuffer_object",
    "GL_EXT_blend_func_separate", "GL_EXT_blend_minmax", "GL_EXT_blend_subtract",
    "GL_ARB_texture_env_combine", "GL_ARB_texture_cube_map",
    "GL_ARB_texture_non_power_of_two", "GL_ARB_depth_texture", "GL_ARB_shadow",
    "GL_ARB_multisample", "GL_EXT_texture_lod_bias",
    "GL_EXT_packed_depth_stencil", "GL_ARB_map_buffer_range",
    "GL_ARB_vertex_array_object", "GL_ARB_ES2_compatibility",
    "GL_ARB_ES3_compatibility", "GL_EXT_texture_filter_anisotropic",
    "GL_ARB_occlusion_query", "GL_ARB_texture_rectangle",
]

TYPES = """\
typedef unsigned int    GLenum;
typedef unsigned char   GLboolean;
typedef unsigned int    GLbitfield;
typedef void            GLvoid;
typedef signed char     GLbyte;
typedef short           GLshort;
typedef int             GLint;
typedef unsigned char   GLubyte;
typedef unsigned short  GLushort;
typedef unsigned int    GLuint;
typedef int             GLsizei;
typedef float           GLfloat;
typedef float           GLclampf;
typedef double          GLdouble;
typedef double          GLclampd;
typedef char            GLchar;
typedef char            GLcharARB;
typedef unsigned int    GLhandleARB;   /* bukan Apple: sama dengan GLuint */
typedef ptrdiff_t       GLintptr;
typedef ptrdiff_t       GLsizeiptr;
typedef ptrdiff_t       GLintptrARB;
typedef ptrdiff_t       GLsizeiptrARB;
typedef int64_t         GLint64;
typedef uint64_t        GLuint64;
typedef struct __GLsync *GLsync;
typedef void (*GLDEBUGPROC)(GLenum source, GLenum type, GLuint id,\n                            GLenum severity, GLsizei length,\n                            const GLchar *message, const void *userParam);
"""

RE_DEF = re.compile(r"^#define\s+(GL_[A-Za-z0-9_]+)\s+(\S+)\s*$", re.M)


def flat(path):
    out = {}
    src = open(path, errors="replace").read()
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    for n, v in RE_DEF.findall(src):
        out.setdefault(n, v)
    return out


def guarded(path, wanted):
    """Ambil #define hanya dari dalam blok #ifndef <guard> ... #endif."""
    out, stack = {}, []
    src = open(path, errors="replace").read()
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    for line in src.splitlines():
        t = line.strip()
        if t.startswith(("#ifndef", "#ifdef", "#if")):
            g = t.split()[1] if len(t.split()) > 1 else ""
            stack.append(g)
            continue
        if t.startswith("#endif"):
            if stack:
                stack.pop()
            continue
        if not any(g in wanted for g in stack):
            continue
        m = RE_DEF.match(t)
        if m and m.group(1) not in wanted:
            out.setdefault(m.group(1), m.group(2))
    return out


defs = {}
defs.update(flat("/usr/include/GL/gl.h"))                    # GL 1.1 inti
for k, v in guarded("/usr/include/GL/glext.h", set(GUARDS)).items():
    defs.setdefault(k, v)

# hanya nilai numerik / literal sederhana
clean = {}
for k, v in defs.items():
    if re.fullmatch(r"(0[xX][0-9A-Fa-f]+|-?\d+u?|0[xX][0-9A-Fa-f]+u?ll?)", v):
        clean[k] = v

hdr = ["/* Dihasilkan oleh tools/gen_gl_header.py - JANGAN DIEDIT MANUAL. */",
       "/* Header GL mandiri: tidak bergantung pada header GL atau GLES sistem. */",
       "#ifndef ORYON_GL_H", "#define ORYON_GL_H", "",
       "#include <stddef.h>", "#include <stdint.h>", "",
       "#ifdef __cplusplus", 'extern "C" {', "#endif", "",
       "/* ------------------------------------------------------------ tipe -- */",
       TYPES,
       "/* -------------------------------------------------------- konstanta -- */"]
for k in sorted(clean):
    hdr.append("#define %-46s %s" % (k, clean[k]))
# Enum khusus GLES yang tidak ada di header desktop. Nilainya identik dengan
# padanan desktop-nya; dicantumkan terpisah supaya asal-usulnya jelas.
GLES_EXTRA = {
    "GL_BGRA_EXT": "0x80E1",          # GL_EXT_texture_format_BGRA8888
    "GL_BGRA8_EXT": "0x93A1",
    "GL_RGB565": "0x8D62",
    "GL_HALF_FLOAT_OES": "0x8D61",
}
hdr += ["", "/* --------------------------------------- khusus GLES -- */"]
for k in sorted(GLES_EXTRA):
    if k not in clean:
        hdr.append("#define %-46s %s" % (k, GLES_EXTRA[k]))

hdr += ["", "#ifdef __cplusplus", "}", "#endif", "", "#endif /* ORYON_GL_H */", ""]

out = os.path.join(ROOT, "include", "oryon", "gl.h")
open(out, "w").write("\n".join(hdr))
print("konstanta GL :", len(clean))
print("->", out, os.path.getsize(out), "byte")
for probe in ("GL_QUADS", "GL_VERSION", "GL_EXTENSIONS", "GL_ALPHA_TEST", "GL_MODELVIEW",
              "GL_ARRAY_BUFFER", "GL_FRAMEBUFFER", "GL_MAP_WRITE_BIT", "GL_TEXTURE_MAX_LEVEL",
              "GL_BGRA", "GL_UNSIGNED_INT_8_8_8_8_REV", "GL_CLAMP_TO_EDGE", "GL_FRAGMENT_SHADER"):
    print("  %-30s %s" % (probe, clean.get(probe, "*** HILANG ***")))
