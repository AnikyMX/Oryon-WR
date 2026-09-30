// Oryon -- textures: desktop formats -> ES 3.2 with zero-copy byte-order handling via texture swizzle,
// legacy L/A/LA/I formats, GL_CLAMP, 1D-as-2D, proxy textures, glGetTexImage / BGRA readback.
#include "oryon.hpp"
#include "bind.hpp"

namespace ory {

enum : uint8_t { LEG_NONE = 0, LEG_L, LEG_A, LEG_LA, LEG_I };

struct Xlat { GLint ifmt; GLenum fmt, type; uint8_t order, legacy, alpha_one, cls, chans; bool ok; };

static inline GLenum es_target(GLenum t) { return (t == GL_TEXTURE_1D || t == GL_TEXTURE_RECTANGLE) ? GL_TEXTURE_2D : t; }
static inline int target_index(GLenum t) {
    switch (t) { case GL_TEXTURE_3D: return 1; case GL_TEXTURE_CUBE_MAP: return 2; case GL_TEXTURE_2D_ARRAY: return 3; default: return 0; }
}
static inline bool is_cube_face(GLenum t) { return t >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && t <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; }
static GLuint bound_tex(GLenum target) {
    GLuint u = g.f.active;
    if (u >= 32) return 0;
    GLenum t = is_cube_face(target) ? GL_TEXTURE_CUBE_MAP : es_target(target);
    return g.tex_bind[u][target_index(t)];
}
static TexInfo *tex_info(GLuint name, bool create) {
    if (!name) return nullptr;
    if (name >= g.tex_cap) {
        if (!create || name >= (1u << 22)) return nullptr;
        GLuint nc = g.tex_cap ? g.tex_cap : 1024;
        while (nc <= name) nc *= 2;
        TexInfo *t = (TexInfo *)realloc(g.tex, (size_t)nc * sizeof(TexInfo));
        if (!t) return nullptr;
        memset(t + g.tex_cap, 0, (size_t)(nc - g.tex_cap) * sizeof(TexInfo));
        g.tex = t; g.tex_cap = nc;
    }
    TexInfo *i = &g.tex[name];
    if (!i->valid && !create) return nullptr;
    if (!i->valid) { memset(i, 0, sizeof *i); i->valid = 1; i->swz = 0x3210; }   // identity swizzle (R,G,B,A)
    return i;
}
uint8_t tex_fmt_class(GLuint name) {
    TexInfo *i = (name && name < g.tex_cap && g.tex[name].valid) ? &g.tex[name] : nullptr;
    return i ? i->fmt : FMT_RGBA;
}

// source (format,type) + desktop internal format -> ES upload triple + storage description
static void xlat(GLint internal, GLenum format, GLenum type, Xlat &x) {
    memset(&x, 0, sizeof x); x.ok = true;
    GLenum sf = format, st = type;
    switch (format) {
    case GL_BGRA: sf = GL_RGBA; x.order = 1; break;
    case GL_BGR: sf = GL_RGB; x.order = 1; break;
    case GL_LUMINANCE: sf = GL_RED; x.legacy = LEG_L; break;
    case GL_ALPHA: sf = GL_RED; x.legacy = LEG_A; break;
    case GL_LUMINANCE_ALPHA: sf = GL_RG; x.legacy = LEG_LA; break;
    case GL_RED: case GL_RG: case GL_RGB: case GL_RGBA: case GL_DEPTH_COMPONENT: case GL_DEPTH_STENCIL:
    case GL_RED_INTEGER: case GL_RG_INTEGER: case GL_RGB_INTEGER: case GL_RGBA_INTEGER: break;
    default: x.ok = false; return;
    }
    if (type == GL_UNSIGNED_INT_8_8_8_8_REV) { if (sf != GL_RGBA) { x.ok = false; return; } st = GL_UNSIGNED_BYTE; }
    else if (type == GL_UNSIGNED_INT_8_8_8_8) { if (sf != GL_RGBA) { x.ok = false; return; } st = GL_UNSIGNED_BYTE; x.order = x.order ? 3 : 2; }
    else if (type == GL_BYTE || type == GL_UNSIGNED_BYTE_3_3_2 || type == GL_UNSIGNED_SHORT_4_4_4_4_REV ||
             type == GL_UNSIGNED_SHORT_1_5_5_5_REV || type == GL_UNSIGNED_SHORT_5_6_5_REV) { x.ok = false; return; }
    x.chans = sf == GL_RED ? 1 : sf == GL_RG ? 2 : sf == GL_RGB ? 3 : 4;
    bool ub = st == GL_UNSIGNED_BYTE, fl = st == GL_FLOAT, hf = st == GL_HALF_FLOAT;
    GLint in = internal;
    switch (internal) {
    case 4: case GL_RGBA: case GL_RGBA8: case GL_RGB10_A2: case GL_RGBA12: case GL_RGBA2:
        if (sf == GL_RGB) in = ub ? GL_RGB8 : fl ? GL_RGB32F : hf ? GL_RGB16F : GL_RGB8;
        else if (sf == GL_RGBA) in = ub ? GL_RGBA8 : fl ? GL_RGBA32F : hf ? GL_RGBA16F : (st == GL_UNSIGNED_SHORT_4_4_4_4 ? GL_RGBA4 : st == GL_UNSIGNED_SHORT_5_5_5_1 ? GL_RGB5_A1 : GL_RGBA8);
        else if (sf == GL_RED) in = GL_R8; else if (sf == GL_RG) in = GL_RG8;
        break;
    case 3: case GL_RGB: case GL_RGB8: case GL_R3_G3_B2: case GL_RGB4: case GL_RGB10: case GL_RGB12:
        if (sf == GL_RGBA) { in = ub ? GL_RGBA8 : fl ? GL_RGBA32F : hf ? GL_RGBA16F : GL_RGBA8; x.alpha_one = 1; }
        else in = ub ? GL_RGB8 : fl ? GL_RGB32F : hf ? GL_RGB16F : (st == GL_UNSIGNED_SHORT_5_6_5 ? GL_RGB565 : GL_RGB8);
        break;
    case GL_RGB5: case GL_RGB565:
        if (sf == GL_RGBA) { in = GL_RGBA8; x.alpha_one = 1; } else in = (st == GL_UNSIGNED_SHORT_5_6_5 || ub) ? GL_RGB565 : GL_RGB8;
        break;
    case GL_RGBA4: in = (sf == GL_RGBA && (ub || st == GL_UNSIGNED_SHORT_4_4_4_4)) ? GL_RGBA4 : GL_RGBA8; break;
    case GL_RGB5_A1: in = (sf == GL_RGBA && (ub || st == GL_UNSIGNED_SHORT_5_5_5_1)) ? GL_RGB5_A1 : GL_RGBA8; break;
    case 1: case GL_LUMINANCE: case GL_LUMINANCE8: case GL_LUMINANCE4: case GL_LUMINANCE12: case GL_LUMINANCE16:
        if (!x.legacy) x.legacy = LEG_L;
        in = sf == GL_RED ? GL_R8 : sf == GL_RG ? GL_RG8 : sf == GL_RGB ? GL_RGB8 : GL_RGBA8; x.legacy = LEG_L; break;
    case GL_ALPHA: case GL_ALPHA8: case GL_ALPHA4: case GL_ALPHA12: case GL_ALPHA16:
        in = sf == GL_RED ? GL_R8 : sf == GL_RG ? GL_RG8 : sf == GL_RGB ? GL_RGB8 : GL_RGBA8; x.legacy = LEG_A; break;
    case 2: case GL_LUMINANCE_ALPHA: case GL_LUMINANCE8_ALPHA8: case GL_LUMINANCE4_ALPHA4: case GL_LUMINANCE12_ALPHA12: case GL_LUMINANCE16_ALPHA16:
        in = sf == GL_RED ? GL_R8 : sf == GL_RG ? GL_RG8 : sf == GL_RGB ? GL_RGB8 : GL_RGBA8; x.legacy = LEG_LA; break;
    case GL_INTENSITY: case GL_INTENSITY8: case GL_INTENSITY4: case GL_INTENSITY12: case GL_INTENSITY16:
        in = sf == GL_RED ? GL_R8 : sf == GL_RG ? GL_RG8 : sf == GL_RGB ? GL_RGB8 : GL_RGBA8; x.legacy = LEG_I; break;
    case GL_DEPTH_COMPONENT:
        in = fl ? GL_DEPTH_COMPONENT32F : st == GL_UNSIGNED_SHORT ? GL_DEPTH_COMPONENT16 : GL_DEPTH_COMPONENT24;
        if (!fl && st != GL_UNSIGNED_SHORT) st = GL_UNSIGNED_INT;
        break;
    case GL_DEPTH_COMPONENT16: if (st != GL_UNSIGNED_SHORT) st = GL_UNSIGNED_INT; break;
    case GL_DEPTH_COMPONENT24: st = GL_UNSIGNED_INT; break;
    case GL_DEPTH_COMPONENT32: in = fl ? GL_DEPTH_COMPONENT32F : GL_DEPTH_COMPONENT24; if (!fl) st = GL_UNSIGNED_INT; break;
    case GL_DEPTH_STENCIL: case GL_DEPTH24_STENCIL8: in = GL_DEPTH24_STENCIL8; sf = GL_DEPTH_STENCIL; st = GL_UNSIGNED_INT_24_8; break;
    case GL_SRGB: case GL_SRGB8: in = sf == GL_RGBA ? GL_SRGB8_ALPHA8 : GL_SRGB8; if (sf == GL_RGBA) x.alpha_one = 1; break;
    case GL_SRGB_ALPHA: case GL_SRGB8_ALPHA8: in = GL_SRGB8_ALPHA8; break;
    case GL_RGBA16:
        if ((g.escaps & ORY_ESCAP_NORM16) && st == GL_UNSIGNED_SHORT) in = GL_RGBA16_EXT;
        else { in = GL_RGBA16F; if (st != GL_FLOAT && st != GL_HALF_FLOAT) st = GL_FLOAT; }
        break;
    case GL_RGB16: in = (g.escaps & ORY_ESCAP_NORM16) && st == GL_UNSIGNED_SHORT ? GL_RGB16_EXT : GL_RGB16F; break;
    default: break;                                  // sized ES formats pass unchanged
    }
    x.ifmt = in; x.fmt = sf; x.type = st;
    x.cls = x.legacy == LEG_A ? FMT_ALPHA : x.legacy == LEG_I ? FMT_INTENSITY :
            (x.legacy == LEG_L || x.alpha_one || in == GL_RGB8 || in == GL_RGB565 || in == GL_RGB16F || in == GL_RGB32F || in == GL_SRGB8) ? FMT_RGB : FMT_RGBA;
}

// swizzle packed as 4 x 4-bit selectors: 0..3 = stored R,G,B,A ; 4 = ZERO ; 5 = ONE
static uint16_t make_swz(const Xlat &x) {
    // logical channel (0..3 = R,G,B,A) -> stored channel, per byte order
    static const uint8_t kPos[4][4] = {{0, 1, 2, 3}, {2, 1, 0, 3}, {3, 2, 1, 0}, {1, 2, 3, 0}};
    uint8_t sel[4] = {0, 1, 2, 3};
    bool narrow = x.chans <= 2;                      // L/A/LA stored in R/RG
    switch (x.legacy) {
    case LEG_L: sel[0] = sel[1] = sel[2] = 0; sel[3] = 5; break;
    case LEG_A: sel[0] = sel[1] = sel[2] = 4; sel[3] = narrow ? 0 : 3; break;
    case LEG_LA: sel[0] = sel[1] = sel[2] = 0; sel[3] = narrow ? 1 : 3; break;
    case LEG_I: sel[0] = sel[1] = sel[2] = sel[3] = 0; break;
    default: break;
    }
    if (x.alpha_one) sel[3] = 5;
    uint16_t r = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t s = sel[i];
        if (s < 4 && !(narrow && x.legacy)) s = kPos[x.order & 3][s];
        r |= (uint16_t)(s << (4 * i));
    }
    return r;
}
static void apply_swz(GLenum target, TexInfo *ti, uint16_t swz) {
    if (!ti || ti->swz == swz) return;
    static const GLenum kSel[6] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO, GL_ONE};
    static const GLenum kPn[4] = {GL_TEXTURE_SWIZZLE_R, GL_TEXTURE_SWIZZLE_G, GL_TEXTURE_SWIZZLE_B, GL_TEXTURE_SWIZZLE_A};
    for (int i = 0; i < 4; ++i) {
        uint8_t a = (swz >> (4 * i)) & 15, b = (ti->swz >> (4 * i)) & 15;
        if (a != b) es.glTexParameteri(target, kPn[i], (GLint)kSel[a]);
    }
    ti->swz = swz;
}
static void note_fmt(GLuint name, uint8_t fmt) {
    for (int u = 0; u < MAX_TEX_UNITS; ++u)
        if (g.tex_bind[u][0] == name && g.f.tu[u].fmt != fmt) { g.f.tu[u].fmt = fmt; if (g.f.tu[u].enabled) g.f.key_dirty = true; }
}

// generic pixel -> logical RGBA (stored order handled by caller) helpers for readback
static void pack_pixels(void *dst, const uint8_t *rgba, GLint w, GLint h, GLenum format, GLenum type, GLint align, GLint row_len) {
    int comps; bool bgr = false;
    switch (format) {
    case GL_RGBA: comps = 4; break; case GL_BGRA: comps = 4; bgr = true; break;
    case GL_RGB: comps = 3; break; case GL_BGR: comps = 3; bgr = true; break;
    case GL_RED: case GL_LUMINANCE: comps = 1; break; case GL_ALPHA: comps = -1; break;
    case GL_RG: case GL_LUMINANCE_ALPHA: comps = 2; break;
    default: set_error(GL_INVALID_ENUM); return;
    }
    bool rev = type == GL_UNSIGNED_INT_8_8_8_8_REV, fwd = type == GL_UNSIGNED_INT_8_8_8_8;
    if (type != GL_UNSIGNED_BYTE && !rev && !fwd) { set_error(GL_INVALID_OPERATION); return; }
    int bpp = comps < 0 ? 1 : comps;
    size_t rowpix = row_len > 0 ? (size_t)row_len : (size_t)w;
    size_t rowbytes = rowpix * (size_t)bpp;
    if (align > 1) rowbytes = (rowbytes + (size_t)align - 1) / (size_t)align * (size_t)align;
    for (GLint y = 0; y < h; ++y) {
        uint8_t *o = (uint8_t *)dst + (size_t)y * rowbytes;
        const uint8_t *s = rgba + (size_t)y * (size_t)w * 4;
        for (GLint x = 0; x < w; ++x, s += 4) {
            uint8_t r = s[0], gg = s[1], b = s[2], a = s[3];
            if (comps == 4) {
                uint8_t c[4] = {bgr ? b : r, gg, bgr ? r : b, a};
                if (fwd) { o[0] = c[3]; o[1] = c[2]; o[2] = c[1]; o[3] = c[0]; } else memcpy(o, c, 4);
                o += 4;
            } else if (comps == 3) { o[0] = bgr ? b : r; o[1] = gg; o[2] = bgr ? r : b; o += 3; }
            else if (comps == 2) { o[0] = r; o[1] = format == GL_LUMINANCE_ALPHA ? a : gg; o += 2; }
            else if (comps == 1) { *o++ = r; }
            else { *o++ = a; }
        }
    }
}
// stored RGBA8 bytes -> logical RGBA using the texture's swizzle
static void unswizzle(uint8_t *px, size_t n, uint16_t swz) {
    if (swz == 0x3210) return;
    for (size_t i = 0; i < n; ++i, px += 4) {
        uint8_t s[4] = {px[0], px[1], px[2], px[3]}, o[4];
        for (int c = 0; c < 4; ++c) { uint8_t sel = (swz >> (4 * c)) & 15; o[c] = sel < 4 ? s[sel] : sel == 4 ? 0 : 255; }
        memcpy(px, o, 4);
    }
}
static bool read_rgba(GLuint fbo_read_restore, GLint x, GLint y, GLint w, GLint h, uint8_t *out) {
    (void)fbo_read_restore;
    GLint pa = 4, prl = 0, psr = 0, psp = 0, pbo = 0;
    es.glGetIntegerv(GL_PACK_ALIGNMENT, &pa); es.glGetIntegerv(GL_PACK_ROW_LENGTH, &prl);
    es.glGetIntegerv(GL_PACK_SKIP_ROWS, &psr); es.glGetIntegerv(GL_PACK_SKIP_PIXELS, &psp);
    es.glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
    if (pbo) es.glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    es.glPixelStorei(GL_PACK_ALIGNMENT, 4); es.glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    es.glPixelStorei(GL_PACK_SKIP_ROWS, 0); es.glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    es.glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out);
    es.glPixelStorei(GL_PACK_ALIGNMENT, pa); es.glPixelStorei(GL_PACK_ROW_LENGTH, prl);
    es.glPixelStorei(GL_PACK_SKIP_ROWS, psr); es.glPixelStorei(GL_PACK_SKIP_PIXELS, psp);
    if (pbo) es.glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)pbo);
    return true;
}
} // namespace ory

using namespace ory;

/* jar: GL11C.glBindTexture(II)V */
OGL_EXPORT void glBindTexture(GLenum target, GLuint texture) { ORY_DL(glBindTexture, target, texture);
    ORY_PROLOGUE();
    GLenum t = es_target(target);
    es.glBindTexture(t, texture);
    GLuint u = g.f.active;
    if (u < 32) g.tex_bind[u][target_index(t)] = texture;
    if (u < (GLuint)MAX_TEX_UNITS && t == GL_TEXTURE_2D) {
        uint8_t f = tex_fmt_class(texture);
        if (f != g.f.tu[u].fmt) { g.f.tu[u].fmt = f; if (g.f.tu[u].enabled) g.f.key_dirty = true; }
    }
}
/* jar: GL11C.nglDeleteTextures(IJ)V */
OGL_EXPORT void glDeleteTextures(GLsizei n, const GLuint *textures) {
    ORY_PROLOGUE();
    es.glDeleteTextures(n, textures);
    for (GLsizei i = 0; i < n; ++i) {
        GLuint id = textures[i];
        if (id && id < g.tex_cap) g.tex[id].valid = 0;
        for (auto &b : g.tex_bind) for (GLuint &x : b) if (x == id) x = 0;
    }
}

static void tex_image(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void *pixels) {
    if (target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_1D) {
        bool ok = w >= 0 && h >= 0 && w <= g.es_max_tex_size && h <= g.es_max_tex_size && level >= 0;
        g.proxy2d.w = ok ? w : 0; g.proxy2d.h = ok ? h : 0; g.proxy2d.internal = ok ? (GLenum)internal : 0;
        return;
    }
    if (border) { set_error(GL_INVALID_VALUE); return; }
    Xlat x; xlat(internal, format, type, x);
    if (!x.ok) { set_error(GL_INVALID_ENUM); return; }
    GLenum t = es_target(target);
    es.glTexImage2D(t, level, x.ifmt, w, h, 0, x.fmt, x.type, pixels);
    GLuint name = bound_tex(target);
    if (TexInfo *ti = tex_info(name, true)) {
        if (level == 0) { ti->w = w; ti->h = h; ti->internal = internal; }
        ti->fmt = x.cls; ti->order = x.order; ti->legacy = x.legacy; ti->alpha_one = x.alpha_one;
        apply_swz(is_cube_face(target) ? GL_TEXTURE_CUBE_MAP : t, ti, make_swz(x));
        note_fmt(name, x.cls);
        if (ti->gen_mipmap && level == 0 && pixels) es.glGenerateMipmap(is_cube_face(target) ? GL_TEXTURE_CUBE_MAP : t);
    }
}
/* jar: GL11C.nglTexImage2D(IIIIIIIIJ)V */
OGL_EXPORT void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) {
    ORY_PROLOGUE(); tex_image(target, level, internalformat, width, height, border, format, type, pixels);
}
/* jar: GL11C.nglTexImage1D(IIIIIIIJ)V */
OGL_EXPORT void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels) {
    ORY_PROLOGUE(); tex_image(target == GL_PROXY_TEXTURE_1D ? GL_PROXY_TEXTURE_2D : GL_TEXTURE_1D, level, internalformat, width, 1, border, format, type, pixels);
}

static void tex_sub_image(GLenum target, GLint level, GLint xo, GLint yo, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels) {
    GLenum t = es_target(target);
    GLuint name = bound_tex(target);
    TexInfo *ti = tex_info(name, false);
    Xlat x; xlat(ti ? ti->internal : GL_RGBA, format, type, x);
    if (!x.ok) { set_error(GL_INVALID_ENUM); return; }
    uint8_t want = ti ? ti->order : 0;
    if (x.order == want || x.chans < 3 || g.b.app_unpack) {
        es.glTexSubImage2D(t, level, xo, yo, w, h, x.fmt, x.type, pixels);
    } else {
        // byte order differs from the texture's storage order: reorder on CPU (rare path)
        GLint rl = g.unpack_row_length > 0 ? g.unpack_row_length : w, al = g.unpack_alignment;
        int bpp = x.chans;
        size_t srow = (size_t)rl * (size_t)bpp; srow = (srow + (size_t)al - 1) / (size_t)al * (size_t)al;
        const uint8_t *src = (const uint8_t *)pixels + (size_t)g.unpack_skip_rows * srow + (size_t)g.unpack_skip_pixels * (size_t)bpp;
        uint8_t *tmp = (uint8_t *)malloc((size_t)w * (size_t)h * (size_t)bpp);
        if (!tmp) { set_error(GL_OUT_OF_MEMORY); return; }
        static const uint8_t kPos[4][4] = {{0, 1, 2, 3}, {2, 1, 0, 3}, {3, 2, 1, 0}, {1, 2, 3, 0}};
        for (GLsizei yy = 0; yy < h; ++yy)
            for (GLsizei xx = 0; xx < w; ++xx) {
                const uint8_t *s = src + (size_t)yy * srow + (size_t)xx * (size_t)bpp;
                uint8_t *o = tmp + ((size_t)yy * (size_t)w + (size_t)xx) * (size_t)bpp;
                uint8_t logical[4];
                for (int c = 0; c < bpp; ++c) logical[c] = s[kPos[x.order & 3][c]];
                for (int c = 0; c < bpp; ++c) o[kPos[want & 3][c]] = logical[c];
            }
        es.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); es.glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        es.glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0); es.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        es.glTexSubImage2D(t, level, xo, yo, w, h, x.fmt, x.type, tmp);
        es.glPixelStorei(GL_UNPACK_ROW_LENGTH, g.unpack_row_length); es.glPixelStorei(GL_UNPACK_SKIP_ROWS, g.unpack_skip_rows);
        es.glPixelStorei(GL_UNPACK_SKIP_PIXELS, g.unpack_skip_pixels); es.glPixelStorei(GL_UNPACK_ALIGNMENT, g.unpack_alignment);
        free(tmp);
    }
    if (ti && ti->gen_mipmap && level == 0) es.glGenerateMipmap(is_cube_face(target) ? GL_TEXTURE_CUBE_MAP : t);
}
/* jar: GL11C.nglTexSubImage2D(IIIIIIIIJ)V */
OGL_EXPORT void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
    ORY_PROLOGUE(); tex_sub_image(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
/* jar: GL11C.nglTexSubImage1D(IIIIIIJ)V */
OGL_EXPORT void glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels) {
    ORY_PROLOGUE(); tex_sub_image(GL_TEXTURE_1D, level, xoffset, 0, width, 1, format, type, pixels); (void)target;
}
/* jar: GL11C.glCopyTexImage2D(IIIIIIII)V */
OGL_EXPORT void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    ORY_PROLOGUE();
    GLenum in = internalformat;
    switch (internalformat) {
    case 4: case GL_RGBA8: in = GL_RGBA; break;
    case 3: case GL_RGB8: in = GL_RGB; break;
    case 1: case GL_LUMINANCE8: in = GL_LUMINANCE; break;
    case GL_ALPHA8: in = GL_ALPHA; break;
    case 2: case GL_LUMINANCE8_ALPHA8: in = GL_LUMINANCE_ALPHA; break;
    case GL_DEPTH_COMPONENT: case GL_DEPTH_COMPONENT24: case GL_DEPTH_COMPONENT32: in = GL_DEPTH_COMPONENT24; break;
    default: break;
    }
    es.glCopyTexImage2D(es_target(target), level, in, x, y, width, height, border);
    GLuint name = bound_tex(target);
    if (TexInfo *ti = tex_info(name, true)) {
        if (level == 0) { ti->w = width; ti->h = height; ti->internal = (GLint)internalformat; }
        ti->fmt = FMT_RGBA; ti->order = 0; ti->legacy = 0; ti->alpha_one = 0;
        apply_swz(es_target(target), ti, 0x3210);
        note_fmt(name, FMT_RGBA);
    }
}
/* jar: GL11C.glCopyTexImage1D(IIIIIII)V */
OGL_EXPORT void glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    (void)target; glCopyTexImage2D(GL_TEXTURE_2D, level, internalformat, x, y, width, 1, border);
}
/* jar: GL11C.glCopyTexSubImage1D(IIIIII)V */
OGL_EXPORT void glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    ORY_PROLOGUE(); (void)target; es.glCopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, 0, x, y, width, 1);
}
/* jar: GL11C.glCopyTexSubImage2D(IIIIIIII)V */
OGL_EXPORT void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    ORY_PROLOGUE(); es.glCopyTexSubImage2D(es_target(target), level, xoffset, yoffset, x, y, width, height);
}

// ------------------------------------------------------------------ parameters
static bool tex_param_emul(GLenum target, GLenum pname, GLfloat v) {
    GLuint name = bound_tex(target);
    switch (pname) {
    case GL_GENERATE_MIPMAP: if (TexInfo *ti = tex_info(name, true)) ti->gen_mipmap = v != 0.0f; return true;
    case GL_TEXTURE_LOD_BIAS: g.lod_bias_unused = v; return true;
    case GL_TEXTURE_PRIORITY: case GL_TEXTURE_RESIDENT: return true;
    case GL_DEPTH_TEXTURE_MODE: {
        if (TexInfo *ti = tex_info(name, true)) {
            GLenum m = (GLenum)v; ti->depth_mode = (uint8_t)(m == GL_ALPHA ? 2 : m == GL_INTENSITY ? 3 : m == GL_RED ? 4 : 1);
            uint16_t s = m == GL_ALPHA ? 0x0444 : m == GL_INTENSITY ? 0x0000 : m == GL_RED ? 0x5440 : 0x5000;
            apply_swz(es_target(target), ti, s);
        }
        return true;
    }
    case GL_TEXTURE_MAX_ANISOTROPY_EXT: return !(g.escaps & ORY_ESCAP_ANISO);
    default: return false;
    }
}
/* jar: GL11C.glTexParameteri(III)V */
static void texparam_i(GLenum target, GLenum pname, GLint param) {
    if (tex_param_emul(target, pname, (GLfloat)param)) return;
    if ((pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R) && param == GL_CLAMP) param = GL_CLAMP_TO_EDGE;
    es.glTexParameteri(es_target(target), pname, param);
}
static void texparam_f(GLenum target, GLenum pname, GLfloat param) {
    if (tex_param_emul(target, pname, param)) return;
    if ((pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R) && (GLenum)param == GL_CLAMP) param = (GLfloat)GL_CLAMP_TO_EDGE;
    es.glTexParameterf(es_target(target), pname, param);
}
/* jar: GL11C.glTexParameteri(III)V */
OGL_EXPORT void glTexParameteri(GLenum target, GLenum pname, GLint param) { ORY_DL(glTexParameteri, target, pname, param); ORY_PROLOGUE(); texparam_i(target, pname, param); }
/* jar: GL11C.glTexParameterf(IIF)V */
OGL_EXPORT void glTexParameterf(GLenum target, GLenum pname, GLfloat param) { ORY_DL(glTexParameterf, target, pname, param); ORY_PROLOGUE(); texparam_f(target, pname, param); }
/* jar: GL11C.nglTexParameteriv(IIJ)V */
OGL_EXPORT void glTexParameteriv(GLenum target, GLenum pname, const GLint *params) { ORY_DL(glTexParameteriv, target, pname, params);
    ORY_PROLOGUE();
    if (pname != GL_TEXTURE_BORDER_COLOR && pname != GL_TEXTURE_SWIZZLE_RGBA) { texparam_i(target, pname, params[0]); return; }
    es.glTexParameteriv(es_target(target), pname, params);
}
/* jar: GL11C.nglTexParameterfv(IIJ)V */
OGL_EXPORT void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) { ORY_DL(glTexParameterfv, target, pname, params);
    ORY_PROLOGUE();
    if (pname != GL_TEXTURE_BORDER_COLOR) { texparam_f(target, pname, params[0]); return; }
    es.glTexParameterfv(es_target(target), pname, params);
}
/* jar: GL11C.nglGetTexParameteriv(IIJ)V */
OGL_EXPORT void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    ORY_PROLOGUE();
    TexInfo *ti = tex_info(bound_tex(target), false);
    switch (pname) {
    case GL_GENERATE_MIPMAP: *params = ti ? ti->gen_mipmap : 0; return;
    case GL_TEXTURE_PRIORITY: *params = 1; return;
    case GL_TEXTURE_RESIDENT: *params = GL_TRUE; return;
    case GL_DEPTH_TEXTURE_MODE: *params = GL_LUMINANCE; return;
    default: es.glGetTexParameteriv(es_target(target), pname, params);
    }
}
/* jar: GL11C.nglGetTexParameterfv(IIJ)V */
OGL_EXPORT void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {
    ORY_PROLOGUE();
    if (pname == GL_TEXTURE_LOD_BIAS) { *params = g.lod_bias_unused; return; }
    if (pname == GL_GENERATE_MIPMAP || pname == GL_TEXTURE_PRIORITY || pname == GL_TEXTURE_RESIDENT || pname == GL_DEPTH_TEXTURE_MODE) {
        GLint i; glGetTexParameteriv(target, pname, &i); *params = (GLfloat)i; return;
    }
    es.glGetTexParameterfv(es_target(target), pname, params);
}
/* jar: GL11C.nglGetTexLevelParameteriv(IIIJ)V */
OGL_EXPORT void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
    ORY_PROLOGUE();
    if (target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_1D) {
        switch (pname) {
        case GL_TEXTURE_WIDTH: *params = g.proxy2d.w; return;
        case GL_TEXTURE_HEIGHT: *params = target == GL_PROXY_TEXTURE_1D ? 1 : g.proxy2d.h; return;
        case GL_TEXTURE_INTERNAL_FORMAT: *params = (GLint)g.proxy2d.internal; return;
        default: *params = 0; return;
        }
    }
    es.glGetTexLevelParameteriv(es_target(target), level, pname, params);
}
/* jar: GL11C.nglGetTexLevelParameterfv(IIIJ)V */
OGL_EXPORT void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params) {
    ORY_PROLOGUE(); GLint i = 0; glGetTexLevelParameteriv(target, level, pname, &i); *params = (GLfloat)i; (void)level;
}
/* jar: GL11C.nglGetTexImage(IIIIJ)V */
OGL_EXPORT void glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels) {
    ORY_PROLOGUE();
    GLenum t = es_target(target);
    GLuint name = bound_tex(target);
    if (!name) { set_error(GL_INVALID_OPERATION); return; }
    GLint w = 0, h = 0;
    es.glGetTexLevelParameteriv(is_cube_face(target) ? target : t, level, GL_TEXTURE_WIDTH, &w);
    es.glGetTexLevelParameteriv(is_cube_face(target) ? target : t, level, GL_TEXTURE_HEIGHT, &h);
    if (w <= 0 || h <= 0) return;
    uint8_t *tmp = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
    if (!tmp) { set_error(GL_OUT_OF_MEMORY); return; }
    GLint prev = 0; es.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev);
    if (!g.read_fbo) es.glGenFramebuffers(1, &g.read_fbo);
    es.glBindFramebuffer(GL_READ_FRAMEBUFFER, g.read_fbo);
    es.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, is_cube_face(target) ? target : t, name, level);
    read_rgba(0, 0, 0, w, h, tmp);
    es.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    es.glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev);
    TexInfo *ti = tex_info(name, false);
    unswizzle(tmp, (size_t)w * (size_t)h, ti ? ti->swz : 0x3210);
    pack_pixels(pixels, tmp, w, h, format, type, g.pack_alignment, g.pack_row_length);
    free(tmp);
}
/* jar: GL11C.nglReadPixels(IIIIIIJ)V */
OGL_EXPORT void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
    ORY_PROLOGUE();
    bool native = (format == GL_RGBA && type == GL_UNSIGNED_BYTE) || type == GL_FLOAT || type == GL_UNSIGNED_INT ||
                  format == GL_RGBA_INTEGER || format == GL_DEPTH_COMPONENT || format == GL_RED || format == GL_RG;
    if (native) { es.glReadPixels(x, y, width, height, format, type, pixels); return; }
    if (width <= 0 || height <= 0) return;
    GLint pbo = 0; es.glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
    if (pbo) { es.glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels); return; }
    uint8_t *tmp = (uint8_t *)malloc((size_t)width * (size_t)height * 4);
    if (!tmp) { set_error(GL_OUT_OF_MEMORY); return; }
    read_rgba(0, x, y, width, height, tmp);
    pack_pixels(pixels, tmp, width, height, format, type, g.pack_alignment, g.pack_row_length);
    free(tmp);
}
/* jar: GL11C.glPixelStorei(II)V */
OGL_EXPORT void glPixelStorei(GLenum pname, GLint param) {
    ORY_PROLOGUE();
    switch (pname) {
    case GL_UNPACK_ROW_LENGTH: g.unpack_row_length = param; break;
    case GL_UNPACK_SKIP_ROWS: g.unpack_skip_rows = param; break;
    case GL_UNPACK_SKIP_PIXELS: g.unpack_skip_pixels = param; break;
    case GL_UNPACK_ALIGNMENT: g.unpack_alignment = param; break;
    case GL_PACK_ALIGNMENT: g.pack_alignment = param; break;
    case GL_PACK_ROW_LENGTH: g.pack_row_length = param; break;
    case GL_UNPACK_SWAP_BYTES: case GL_UNPACK_LSB_FIRST: case GL_PACK_SWAP_BYTES: case GL_PACK_LSB_FIRST:
    case GL_PACK_IMAGE_HEIGHT: case GL_PACK_SKIP_IMAGES:
        return;                                          // desktop-only packing modes: accepted, no effect
    default: break;
    }
    es.glPixelStorei(pname, param);
}
/* jar: GL11C.glPixelStoref(IF)V */
OGL_EXPORT void glPixelStoref(GLenum pname, GLfloat param) { glPixelStorei(pname, (GLint)param); }
