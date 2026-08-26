/* Tes tahap 5: menggambar sungguhan.

   Semua yang dibangun tahap 1-4 bertemu di sini. Setiap kasus menggambar ke
   FBO 64x64 lewat titik ekspor Oryon yang sebenarnya, lalu membaca balik
   pikselnya. Kalau terjemahan GL_QUADS salah, atau warna ubyte ternormalkan
   keliru, atau matriks tekstur tidak terpasang - pikselnya yang bicara. */

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "oryon/oryon.h"
#include "../src/driver/driver.h"
#include "../src/state/ff_state.h"
#include "../src/state/state.h"
#include "../src/draw/vertexpipe.h"

extern "C" {
#define ORYON_SYM(strat, ret, name, params, args) ret name params;
#include "oryon/gl_symbols.inc"
}

using namespace oryon;

static int g_pass, g_fail;

static void ok(bool cond, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (cond) { ++g_pass; fputs("  ok    ", stdout); }
    else      { ++g_fail; fputs("  GAGAL ", stdout); }
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
}

/* ------------------------------------------------------------------ EGL --- */

static bool egl_up() {
    EGLDisplay d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                         EGL_DEFAULT_DISPLAY, NULL);
    if (d == EGL_NO_DISPLAY) d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (d == EGL_NO_DISPLAY) return false;
    EGLint mj = 0, mn = 0;
    if (!eglInitialize(d, &mj, &mn) || !eglBindAPI(EGL_OPENGL_ES_API)) return false;

    static const EGLint ca[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg; EGLint n = 0;
    if (!eglChooseConfig(d, ca, &cfg, 1, &n) || n < 1) return false;
    static const EGLint xa[] = { EGL_CONTEXT_MAJOR_VERSION, 3,
                                 EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE };
    EGLContext c = eglCreateContext(d, cfg, EGL_NO_CONTEXT, xa);
    if (c == EGL_NO_CONTEXT) return false;
    if (!eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c)) {
        static const EGLint pb[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
        EGLSurface s = eglCreatePbufferSurface(d, cfg, pb);
        if (s == EGL_NO_SURFACE || !eglMakeCurrent(d, s, s, c)) return false;
    }
    return true;
}

/* --------------------------------------------------------------- bantuan -- */

enum { W = 64, H = 64 };
static GLuint s_fbo, s_rb, s_tex_a, s_tex_b;

struct Px { unsigned char r, g, b, a; };

static Px pixel(int x, int y) {
    unsigned char p[4] = { 0, 0, 0, 0 };
    gles.glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    Px v = { p[0], p[1], p[2], p[3] };
    return v;
}

static bool near_(unsigned char a, int want, int tol = 3) {
    const int d = (int) a - want;
    return d >= -tol && d <= tol;
}

static bool px_is(Px p, int r, int g, int b, int a = 255) {
    return near_(p.r, r) && near_(p.g, g) && near_(p.b, b) && near_(p.a, a);
}

static void clear_black() {
    gles.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gles.glClear(GL_COLOR_BUFFER_BIT);
}

static GLuint make_tex(const unsigned char *rgba, int w, int h) {
    GLuint t = 0;
    gles.glGenTextures(1, &t);
    gles.glBindTexture(GL_TEXTURE_2D, t);
    gles.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, rgba);
    gles.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gles.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gles.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

/* Satu quad menutupi [16,48] x [16,48] dalam koordinat piksel. */
static const float Q0 = 16.0f, Q1 = 48.0f;

static void imm_quad(float r, float g, float b, float a,
                     bool with_uv, float u0 = 0.0f, float v0 = 0.0f) {
    glBegin(GL_QUADS);
    glColor4f(r, g, b, a);
    if (with_uv) glTexCoord2f(u0, v0);
    glVertex3f(Q0, Q0, 0.0f);
    if (with_uv) glTexCoord2f(u0, v0);
    glVertex3f(Q1, Q0, 0.0f);
    if (with_uv) glTexCoord2f(u0, v0);
    glVertex3f(Q1, Q1, 0.0f);
    if (with_uv) glTexCoord2f(u0, v0);
    glVertex3f(Q0, Q1, 0.0f);
    glEnd();
}

int main() {
    puts("== Context + FBO ==");
    if (!egl_up()) { puts("  LINGKUNGAN TIDAK MENDUKUNG - dilewati"); return 77; }
    const bool inited = ensure_init();
    ok(inited, "Oryon siap: %s", g_state.renderer);

    gles.glGenRenderbuffers(1, &s_rb);
    gles.glBindRenderbuffer(GL_RENDERBUFFER, s_rb);
    gles.glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, W, H);
    gles.glGenFramebuffers(1, &s_fbo);
    gles.glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    gles.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_RENDERBUFFER, s_rb);
    ok(gles.glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
       "FBO 64x64 lengkap");

    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const unsigned char orange[4] = { 255, 128, 0, 255 };
    const unsigned char cyan[4]   = { 128, 255, 255, 255 };
    /* 2x2: (0,0) merah, (1,0) hijau, (0,1) biru, (1,1) putih */
    const unsigned char quad_tex[16] = { 255,0,0,255,  0,255,0,255,
                                         0,0,255,255,  255,255,255,255 };
    s_tex_a = make_tex(orange, 1, 1);
    s_tex_b = make_tex(cyan, 1, 1);
    GLuint tex_2x2 = make_tex(quad_tex, 2, 2);

    /* ---------------------------------------------- A. mode langsung ------ */
    puts("\n== A. Mode langsung, GL_QUADS ==");
    clear_black();
    imm_quad(1.0f, 0.0f, 0.0f, 1.0f, false);
    ok(gles.glGetError() == GL_NO_ERROR, "tanpa error GL");
    ok(px_is(pixel(32, 32), 255, 0, 0), "tengah quad merah");
    ok(px_is(pixel(4, 4), 0, 0, 0), "luar quad tetap hitam");
    ok(px_is(pixel(18, 18), 255, 0, 0) && px_is(pixel(46, 18), 255, 0, 0) &&
       px_is(pixel(46, 46), 255, 0, 0) && px_is(pixel(18, 46), 255, 0, 0),
       "keempat sudut tertutup - pola index quad benar");

    /* ---------------------------------------------- B. tekstur modulate --- */
    puts("\n== B. Tekstur + GL_MODULATE ==");
    clear_black();
    gles.glActiveTexture(GL_TEXTURE0);
    gles.glBindTexture(GL_TEXTURE_2D, s_tex_a);
    glEnable(GL_TEXTURE_2D);
    imm_quad(1.0f, 1.0f, 1.0f, 1.0f, true, 0.5f, 0.5f);
    ok(px_is(pixel(32, 32), 255, 128, 0), "warna putih x tekstur oranye = oranye");
    clear_black();
    imm_quad(0.5f, 1.0f, 1.0f, 1.0f, true, 0.5f, 0.5f);
    ok(px_is(pixel(32, 32), 128, 128, 0), "warna (0.5,1,1) x oranye = modulate benar");

    /* ---------------------------------------------- C. GL_REPLACE --------- */
    puts("\n== C. GL_REPLACE ==");
    clear_black();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    imm_quad(0.0f, 0.0f, 1.0f, 1.0f, true, 0.5f, 0.5f);
    ok(px_is(pixel(32, 32), 255, 128, 0), "REPLACE mengabaikan warna vertex");
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* ---------------------------------------------- D. uji alpha ---------- */
    puts("\n== D. Uji alpha ==");
    clear_black();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    imm_quad(0.0f, 1.0f, 0.0f, 0.25f, false);
    ok(px_is(pixel(32, 32), 0, 0, 0), "alpha 0.25 < ref 0.5 -> dibuang");
    imm_quad(0.0f, 1.0f, 0.0f, 0.75f, false);
    {
        Px p = pixel(32, 32);
        ok(near_(p.r, 0) && near_(p.g, 255) && near_(p.b, 0) && near_(p.a, 191),
           "alpha 0.75 > ref 0.5 -> tergambar, alpha ikut tertulis (%u)", p.a);
    }
    glDisable(GL_ALPHA_TEST);

    /* ---------------------------------------------- E. kabut -------------- */
    puts("\n== E. Kabut LINEAR ==");
    clear_black();
    {
        const float fogc[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, fogc);
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, 90.62f);
        /* Kabut Oryon memakai jarak radial |eye|, bukan hampiran |z_eye|.
           Di piksel (32,32) jaraknya sekitar 46, jadi rentang 0..90.62
           memberi faktor kabut mendekati 0.5. */
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -1.0f);
        imm_quad(1.0f, 0.0f, 0.0f, 1.0f, false);
        glPopMatrix();
        Px p = pixel(32, 32);
        ok(p.r > 100 && p.r < 155 && p.b > 100 && p.b < 155,
           "merah berbaur setengah ke kabut biru: (%u,%u,%u)", p.r, p.g, p.b);
        glDisable(GL_FOG);
    }

    /* ---------------------------------------------- F. client array ------- */
    puts("\n== F. Client array + GL_QUADS ==");
    clear_black();
    struct V { float x, y, z; unsigned char r, g, b, a; float u, v; };
    static const V verts[4] = {
        { Q0, Q0, 0, 255, 0, 255, 255, 0.5f, 0.5f },
        { Q1, Q0, 0, 255, 0, 255, 255, 0.5f, 0.5f },
        { Q1, Q1, 0, 255, 0, 255, 255, 0.5f, 0.5f },
        { Q0, Q1, 0, 255, 0, 255, 255, 0.5f, 0.5f },
    };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(V), &verts[0].x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(V), &verts[0].r);
    glDrawArrays(GL_QUADS, 0, 4);
    ok(gles.glGetError() == GL_NO_ERROR, "tanpa error GL");
    ok(px_is(pixel(32, 32), 255, 0, 255), "warna ubyte ternormalkan benar (magenta)");
    ok(px_is(pixel(4, 4), 0, 0, 0), "luar quad tetap hitam");

    /* ---------------------------------------------- G. interleaved + tex -- */
    puts("\n== G. Client array interleaved dengan tekstur ==");
    clear_black();
    gles.glBindTexture(GL_TEXTURE_2D, s_tex_a);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(V), &verts[0].u);
    glDrawArrays(GL_QUADS, 0, 4);
    ok(px_is(pixel(32, 32), 255, 0, 0),
       "magenta x oranye = (255,0,0) - satu unggahan interleaved");

    /* ---------------------------------------------- H. dua tekstur -------- */
    puts("\n== H. Dua unit tekstur (jalur terrain + lightmap) ==");
    clear_black();
    struct V2 { float x, y, z; unsigned char r, g, b, a; float u, v; float lu, lv; };
    static const V2 v2[4] = {
        { Q0, Q0, 0, 255, 255, 255, 255, 0.5f, 0.5f, 0.5f, 0.5f },
        { Q1, Q0, 0, 255, 255, 255, 255, 0.5f, 0.5f, 0.5f, 0.5f },
        { Q1, Q1, 0, 255, 255, 255, 255, 0.5f, 0.5f, 0.5f, 0.5f },
        { Q0, Q1, 0, 255, 255, 255, 255, 0.5f, 0.5f, 0.5f, 0.5f },
    };
    glVertexPointer(3, GL_FLOAT, sizeof(V2), &v2[0].x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(V2), &v2[0].r);
    glClientActiveTexture(GL_TEXTURE0);
    glTexCoordPointer(2, GL_FLOAT, sizeof(V2), &v2[0].u);
    glClientActiveTexture(GL_TEXTURE1);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(V2), &v2[0].lu);
    glActiveTexture(GL_TEXTURE1);
    gles.glBindTexture(GL_TEXTURE_2D, s_tex_b);
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glDrawArrays(GL_QUADS, 0, 4);
    ok(px_is(pixel(32, 32), 128, 128, 0),
       "oranye x cyan = (128,128,0) - dua unit termodulasi");

    /* ---------------------------------------------- I. matriks tekstur ---- */
    puts("\n== I. Matriks tekstur (pola lightmap Minecraft) ==");
    clear_black();
    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    glClientActiveTexture(GL_TEXTURE1);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glClientActiveTexture(GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE0);
    gles.glBindTexture(GL_TEXTURE_2D, tex_2x2);
    glVertexPointer(3, GL_FLOAT, sizeof(V), &verts[0].x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(V), &verts[0].r);
    glTexCoordPointer(2, GL_FLOAT, sizeof(V), &verts[0].u);
    /* Koordinat (0.5,0.5) tanpa matriks jatuh di batas; geser ke kuadran kiri
       bawah dulu, lalu buktikan matriks memindahkannya ke kanan. */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(-0.25f, -0.25f, 0.0f);
    glMatrixMode(GL_MODELVIEW);
    glDrawArrays(GL_QUADS, 0, 4);
    Px left = pixel(32, 32);
    clear_black();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0.25f, -0.25f, 0.0f);
    glMatrixMode(GL_MODELVIEW);
    glDrawArrays(GL_QUADS, 0, 4);
    Px right = pixel(32, 32);
    ok(left.r > 200 && left.g < 60 && left.b < 60,
       "matriks tekstur geser -0.25 -> texel merah (%u,%u,%u)", left.r, left.g, left.b);
    ok(right.g > 200 && right.r < 60 && right.b < 60,
       "matriks tekstur geser +0.25 -> texel hijau (%u,%u,%u)", right.r, right.g, right.b);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* ---------------------------------------------- J. VBO --------------- */
    puts("\n== J. Client array bersumber VBO ==");
    clear_black();
    gles.glBindTexture(GL_TEXTURE_2D, s_tex_a);
    {
        GLuint vbo = 0;
        gles.glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) sizeof verts, verts, GL_STATIC_DRAW);
        glVertexPointer(3, GL_FLOAT, sizeof(V), (const void *) 0);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(V), (const void *) offsetof(V, r));
        glTexCoordPointer(2, GL_FLOAT, sizeof(V), (const void *) offsetof(V, u));
        glDrawArrays(GL_QUADS, 0, 4);
        ok(gles.glGetError() == GL_NO_ERROR, "tanpa error GL");
        ok(px_is(pixel(32, 32), 255, 0, 0), "gambar dari VBO memberi piksel yang sama");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    /* ---------------------------------------------- K. QUAD_STRIP/POLYGON  */
    puts("\n== K. GL_QUAD_STRIP dan GL_POLYGON ==");
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    clear_black();
    glBegin(GL_QUAD_STRIP);
    glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
    glVertex3f(Q0, Q0, 0.0f);
    glVertex3f(Q0, Q1, 0.0f);
    glVertex3f(Q1, Q0, 0.0f);
    glVertex3f(Q1, Q1, 0.0f);
    glEnd();
    ok(px_is(pixel(32, 32), 255, 255, 0) && px_is(pixel(18, 18), 255, 255, 0) &&
       px_is(pixel(46, 46), 255, 255, 0) && px_is(pixel(4, 4), 0, 0, 0),
       "GL_QUAD_STRIP -> GL_TRIANGLE_STRIP menutupi luasan yang sama");

    clear_black();
    glBegin(GL_POLYGON);
    glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
    glVertex3f(Q0, Q0, 0.0f);
    glVertex3f(Q1, Q0, 0.0f);
    glVertex3f(Q1, Q1, 0.0f);
    glVertex3f(Q0, Q1, 0.0f);
    glEnd();
    ok(px_is(pixel(32, 32), 0, 255, 255) && px_is(pixel(18, 46), 0, 255, 255) &&
       px_is(pixel(4, 4), 0, 0, 0),
       "GL_POLYGON -> GL_TRIANGLE_FAN menutupi luasan yang sama");

    /* ---------------------------------------------- L. batch besar -------- */
    puts("\n== L. Batch besar (jalur terrain) ==");
    clear_black();
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
    for (int i = 0; i < 1024; ++i) {
        const float x = (float) (i % 32) * 2.0f;
        const float y = (float) (i / 32) * 2.0f;
        glVertex3f(x,        y,        0.0f);
        glVertex3f(x + 2.0f, y,        0.0f);
        glVertex3f(x + 2.0f, y + 2.0f, 0.0f);
        glVertex3f(x,        y + 2.0f, 0.0f);
    }
    glEnd();
    ok(gles.glGetError() == GL_NO_ERROR, "4096 vertex dalam satu batch, tanpa error");
    ok(px_is(pixel(1, 1), 0, 0, 255) && px_is(pixel(62, 62), 0, 0, 255),
       "index buffer quad tumbuh dengan benar sampai 1024 quad");

    printf("\n===== %d lulus, %d gagal =====\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
