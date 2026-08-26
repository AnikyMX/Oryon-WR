/* Jalur tekstur.

   Dua hal yang wajib diterjemahkan, keduanya terbaca dari bytecode 1.12.2:

     1. Unggahan piksel. TextureUtil memakai GL_BGRA + GL_UNSIGNED_INT_8_8_8_8_REV
        karena itulah tata letak int ARGB milik Java. GLES tidak punya keduanya.
     2. GL_CLAMP. Muncul 6 kali di 1.12.2 dan tidak ada di GLES sama sekali;
        yang ada hanya GL_CLAMP_TO_EDGE.

   ORYON_IMPL(glBindTexture)
   ORYON_IMPL(glTexImage2D)
   ORYON_IMPL(glTexSubImage2D)
   ORYON_IMPL(glTexImage3D)
   ORYON_IMPL(glTexSubImage3D)
   ORYON_IMPL(glCopyTexSubImage2D)
   ORYON_IMPL(glCompressedTexImage2D)
   ORYON_IMPL(glCompressedTexSubImage2D)
   ORYON_IMPL(glTexParameteri)
   ORYON_IMPL(glTexParameterf)
   ORYON_IMPL(glPixelStorei)
   ORYON_IMPL(glReadPixels)
   ORYON_IMPL(glGetTexLevelParameteriv)
   ORYON_IMPL(glGetTexImage)
*/

#include "oryon/oryon.h"
#include "../driver/driver.h"
#include "../state/ff_state.h"
#include "../state/state.h"

#include <stdlib.h>
#include <string.h>

using namespace oryon;

namespace {

bool      s_bgra_ext;          /* driver punya GL_EXT_texture_format_BGRA8888 */
bool      s_probed;
unsigned char *s_scratch;
size_t    s_scratch_cap;

GLint     s_unpack_align = 4;
GLint     s_unpack_row   = 0;
GLuint    s_bound_tex[FF_MAX_TEX];

void probe() {
    if (s_probed) return;
    s_probed = true;
    /* glGetStringi ada sejak GLES 3.0; permukaan yang kita pakai selalu >= 3.0. */
    GLint n = 0;
    gles.glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; ++i) {
        const char *e = (const char *) gles.glGetStringi(GL_EXTENSIONS, (GLuint) i);
        if (e && strcmp(e, "GL_EXT_texture_format_BGRA8888") == 0) {
            s_bgra_ext = true;
            break;
        }
    }
    ORYON_LOG("GL_EXT_texture_format_BGRA8888: %s",
              s_bgra_ext ? "ada, unggahan BGRA tanpa salinan" : "tidak ada, tukar R/B di CPU");
}

unsigned char *scratch(size_t n) {
    if (n <= s_scratch_cap) return s_scratch;
    unsigned char *p = (unsigned char *) realloc(s_scratch, n);
    if (!p) return 0;
    s_scratch = p;
    s_scratch_cap = n;
    return p;
}

/* Tata letak piksel setelah diterjemahkan ke GLES. */
struct Fmt {
    GLenum internal;
    GLenum format;
    GLenum type;
    bool   swap_rb;   /* perlu tukar byte R dan B di CPU */
};

/* GL_UNSIGNED_INT_8_8_8_8_REV dengan format GL_BGRA menaruh B di bit terendah,
   sehingga di memori little-endian urutannya B,G,R,A - sama persis dengan
   GL_BGRA + GL_UNSIGNED_BYTE. Itu sebabnya keduanya ditangani sama. */
bool resolve(GLint internalformat, GLenum format, GLenum type, Fmt *f) {
    probe();
    f->swap_rb = false;

    const bool bgra_src = (format == GL_BGRA) &&
                          (type == GL_UNSIGNED_BYTE ||
                           type == GL_UNSIGNED_INT_8_8_8_8_REV);
    if (bgra_src) {
        if (s_bgra_ext) {
            f->internal = GL_BGRA_EXT;
            f->format   = GL_BGRA_EXT;
            f->type     = GL_UNSIGNED_BYTE;
            return true;
        }
        f->internal = GL_RGBA8;
        f->format   = GL_RGBA;
        f->type     = GL_UNSIGNED_BYTE;
        f->swap_rb  = true;
        return true;
    }

    f->format = format;
    f->type   = (type == GL_UNSIGNED_INT_8_8_8_8_REV ||
                 type == GL_UNSIGNED_INT_8_8_8_8) ? GL_UNSIGNED_BYTE : type;

    switch (internalformat) {
    case 4: case GL_RGBA: case GL_RGBA8: f->internal = GL_RGBA8; break;
    case 3: case GL_RGB:  case GL_RGB8:  f->internal = GL_RGB8;  break;
    case 1: case GL_LUMINANCE:           f->internal = GL_LUMINANCE; break;
    case 2: case GL_LUMINANCE_ALPHA:     f->internal = GL_LUMINANCE_ALPHA; break;
    case GL_ALPHA:                       f->internal = GL_ALPHA; break;
    default:                             f->internal = (GLenum) internalformat; break;
    }
    return true;
}

/* Menukar R dan B ke buffer kerja. Baris sumber mengikuti GL_UNPACK_ROW_LENGTH
   dan GL_UNPACK_ALIGNMENT; keluarannya selalu rapat. */
const void *swizzle(const void *src, GLsizei w, GLsizei h) {
    if (!src || w <= 0 || h <= 0) return src;
    const size_t out_row = (size_t) w * 4;
    size_t in_row = (size_t) (s_unpack_row ? s_unpack_row : w) * 4;
    if (s_unpack_align > 1) {
        const size_t a = (size_t) s_unpack_align;
        in_row = (in_row + a - 1) / a * a;
    }
    unsigned char *dst = scratch(out_row * (size_t) h);
    if (!dst) return src;

    const unsigned char *in = (const unsigned char *) src;
    for (GLsizei y = 0; y < h; ++y) {
        const unsigned char *s = in + (size_t) y * in_row;
        unsigned char *d = dst + (size_t) y * out_row;
        for (GLsizei x = 0; x < w; ++x, s += 4, d += 4) {
            d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3];
        }
    }
    return dst;
}

GLint fix_wrap(GLint v) {
    /* GL_CLAMP (clamp-to-border tanpa border) tidak ada di GLES. Yang paling
       mendekati - dan yang dimaksud Minecraft - adalah CLAMP_TO_EDGE. */
    return (v == GL_CLAMP) ? GL_CLAMP_TO_EDGE : v;
}

}  /* namespace */

/* ---------------------------------------------------------- pengikatan --- */

ORYON_API void glBindTexture(GLenum target, GLuint texture) {
    if (!ensure_init()) return;
    if (target == GL_TEXTURE_2D) s_bound_tex[g_ff.a.active_tex] = texture;
    gles.glBindTexture(target, texture);
}

/* ------------------------------------------------------------- unggahan -- */

ORYON_API void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                            GLsizei width, GLsizei height, GLint border,
                            GLenum format, GLenum type, const GLvoid *pixels) {
    if (!ensure_init()) return;
    Fmt f;
    resolve(internalformat, format, type, &f);
    const void *data = f.swap_rb ? swizzle(pixels, width, height) : pixels;
    if (f.swap_rb && data != pixels) {
        gles.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        gles.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    gles.glTexImage2D(target, level, (GLint) f.internal, width, height, border,
                      f.format, f.type, data);
    if (f.swap_rb && data != pixels) {
        gles.glPixelStorei(GL_UNPACK_ROW_LENGTH, s_unpack_row);
        gles.glPixelStorei(GL_UNPACK_ALIGNMENT, s_unpack_align);
    }
}

ORYON_API void glTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                               GLint yoffset, GLsizei width, GLsizei height,
                               GLenum format, GLenum type, const GLvoid *pixels) {
    if (!ensure_init()) return;
    Fmt f;
    resolve(GL_RGBA, format, type, &f);
    const void *data = f.swap_rb ? swizzle(pixels, width, height) : pixels;
    if (f.swap_rb && data != pixels) {
        gles.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        gles.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    gles.glTexSubImage2D(target, level, xoffset, yoffset, width, height,
                         f.format, f.type, data);
    if (f.swap_rb && data != pixels) {
        gles.glPixelStorei(GL_UNPACK_ROW_LENGTH, s_unpack_row);
        gles.glPixelStorei(GL_UNPACK_ALIGNMENT, s_unpack_align);
    }
}

ORYON_API void glTexImage3D(GLenum target, GLint level, GLint internalformat,
                            GLsizei width, GLsizei height, GLsizei depth,
                            GLint border, GLenum format, GLenum type,
                            const GLvoid *pixels) {
    if (!ensure_init()) return;
    Fmt f;
    resolve(internalformat, format, type, &f);
    gles.glTexImage3D(target, level, (GLint) f.internal, width, height, depth,
                      border, f.format, f.type, pixels);
}

ORYON_API void glTexSubImage3D(GLenum target, GLint level, GLint xoffset,
                               GLint yoffset, GLint zoffset, GLsizei width,
                               GLsizei height, GLsizei depth, GLenum format,
                               GLenum type, const GLvoid *pixels) {
    if (!ensure_init()) return;
    Fmt f;
    resolve(GL_RGBA, format, type, &f);
    gles.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height,
                         depth, f.format, f.type, pixels);
}

ORYON_API void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                   GLint yoffset, GLint x, GLint y,
                                   GLsizei width, GLsizei height) {
    if (ensure_init())
        gles.glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

ORYON_API void glCompressedTexImage2D(GLenum target, GLint level,
                                      GLenum internalformat, GLsizei width,
                                      GLsizei height, GLint border,
                                      GLsizei imageSize, const void *data) {
    if (ensure_init())
        gles.glCompressedTexImage2D(target, level, internalformat, width, height,
                                    border, imageSize, data);
}

ORYON_API void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                         GLint yoffset, GLsizei width, GLsizei height,
                                         GLenum format, GLsizei imageSize,
                                         const void *data) {
    if (ensure_init())
        gles.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                       height, format, imageSize, data);
}

/* ------------------------------------------------------------- parameter - */

ORYON_API void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (!ensure_init()) return;
    switch (pname) {
    case GL_TEXTURE_WRAP_S: case GL_TEXTURE_WRAP_T: case GL_TEXTURE_WRAP_R:
        gles.glTexParameteri(target, pname, fix_wrap(param));
        return;
    case GL_GENERATE_MIPMAP:
        /* Dihapus di GLES; 1.12.2 tidak memakainya, mipmap diunggah manual. */
        return;
    default:
        gles.glTexParameteri(target, pname, param);
        return;
    }
}

ORYON_API void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    if (!ensure_init()) return;
    if (pname == GL_TEXTURE_LOD_BIAS) {
        /* GLES tidak punya bias LOD per-tekstur. 1.12.2 hanya menuliskan 0
           sebagai bagian dari reset state, jadi diam saja bila memang 0. */
        if (param != 0.0f) ORYON_LOG("GL_TEXTURE_LOD_BIAS %.2f diabaikan", param);
        return;
    }
    if (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T ||
        pname == GL_TEXTURE_WRAP_R) {
        gles.glTexParameterf(target, pname, (GLfloat) fix_wrap((GLint) param));
        return;
    }
    gles.glTexParameterf(target, pname, param);
}

ORYON_API void glPixelStorei(GLenum pname, GLint param) {
    if (!ensure_init()) return;
    if (pname == GL_UNPACK_ALIGNMENT)      s_unpack_align = param;
    else if (pname == GL_UNPACK_ROW_LENGTH) s_unpack_row = param;
    switch (pname) {
    case GL_UNPACK_ALIGNMENT: case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_SKIP_PIXELS: case GL_UNPACK_SKIP_ROWS: case GL_UNPACK_SKIP_IMAGES:
    case GL_UNPACK_IMAGE_HEIGHT:
    case GL_PACK_ALIGNMENT: case GL_PACK_ROW_LENGTH:
    case GL_PACK_SKIP_PIXELS: case GL_PACK_SKIP_ROWS:
        gles.glPixelStorei(pname, param);
        return;
    default:
        return;   /* pname warisan (SWAP_BYTES, LSB_FIRST, ...) tidak ada di GLES */
    }
}

/* ------------------------------------------------------------- pembacaan - */

ORYON_API void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                            GLenum format, GLenum type, GLvoid *pixels) {
    if (!ensure_init() || !pixels) return;
    const bool bgra = (format == GL_BGRA) &&
                      (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_INT_8_8_8_8_REV);
    gles.glReadPixels(x, y, width, height, bgra ? GL_RGBA : format,
                      bgra ? GL_UNSIGNED_BYTE : type, pixels);
    if (bgra && !s_bgra_ext) {
        unsigned char *p = (unsigned char *) pixels;
        const size_t n = (size_t) width * (size_t) height;
        for (size_t i = 0; i < n; ++i, p += 4) {
            const unsigned char t = p[0];
            p[0] = p[2];
            p[2] = t;
        }
    }
}

ORYON_API void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                                        GLint *params) {
    if (!ensure_init() || !params) return;
    if (gles.glGetTexLevelParameteriv) {
        gles.glGetTexLevelParameteriv(target, level, pname, params);
        return;
    }
    *params = 0;   /* GLES 3.0: tidak ada. Jarang, dan 0 lebih baik dari sampah. */
}

/* Tidak ada padanannya di GLES. Tekstur dilampirkan ke FBO sementara lalu
   dibaca dengan glReadPixels - satu-satunya jalan yang tersedia. */
ORYON_API void glGetTexImage(GLenum target, GLint level, GLenum format,
                             GLenum type, GLvoid *pixels) {
    if (!ensure_init() || !pixels) return;
    if (target != GL_TEXTURE_2D) { set_error(GL_INVALID_ENUM); return; }

    GLint w = 0, h = 0;
    if (gles.glGetTexLevelParameteriv) {
        gles.glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &w);
        gles.glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &h);
    }
    if (w <= 0 || h <= 0) { set_error(GL_INVALID_OPERATION); return; }

    GLint prev_fbo = 0;
    gles.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

    GLuint fbo = 0;
    gles.glGenFramebuffers(1, &fbo);
    gles.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gles.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, s_bound_tex[g_ff.a.active_tex], level);
    if (gles.glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        glReadPixels(0, 0, w, h, format, type, pixels);
    else
        set_error(GL_INVALID_OPERATION);

    gles.glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) prev_fbo);
    gles.glDeleteFramebuffers(1, &fbo);
}
