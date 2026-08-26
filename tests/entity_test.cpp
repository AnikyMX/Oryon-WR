/* Tes jalur render entitas.

   Lahir dari laporan perangkat: terrain tampil, tetapi tangan, entitas, dan
   item tidak. Ketiganya punya satu kesamaan yang terrain tidak punya -
   digambar lewat ModelRenderer, yang merekam SATU GAMBAR PER KOTAK MODEL ke
   dalam sebuah display list, lalu memanggilnya dengan pencahayaan menyala.

   Tes ini meniru urutan itu apa adanya:
     RenderHelper.enableStandardItemLighting()  (nilai persis dari 1.12.2)
     glNewList -> tiga TexturedQuad.draw() -> glEndList
     glCallList

   Kalau perekaman display list hanya menyimpan gambar terakhir, dua dari tiga
   kotak hilang - persis gejala yang dilaporkan. */

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "oryon/oryon.h"
#include "../src/driver/driver.h"
#include "../src/state/ff_state.h"
#include "../src/state/state.h"

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

static bool egl_up() {
    EGLDisplay d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                         EGL_DEFAULT_DISPLAY, NULL);
    if (d == EGL_NO_DISPLAY) d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (d == EGL_NO_DISPLAY) return false;
    EGLint mj = 0, mn = 0;
    if (!eglInitialize(d, &mj, &mn) || !eglBindAPI(EGL_OPENGL_ES_API)) return false;
    static const EGLint ca[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE };
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

enum { W = 64, H = 64 };
struct Px { unsigned char r, g, b, a; };

static Px pixel(int x, int y) {
    unsigned char p[4] = { 0, 0, 0, 0 };
    gles.glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    Px v = { p[0], p[1], p[2], p[3] };
    return v;
}
static bool near_(unsigned char a, int w, int t = 5) { int d = (int) a - w; return d >= -t && d <= t; }
static void clear_black() {
    gles.glClearColor(0, 0, 0, 1);
    gles.glClear(GL_COLOR_BUFFER_BIT);
}

/* Tata letak DefaultVertexFormats.OLDMODEL_POSITION_TEX_NORMAL:
   posisi 3f, koordinat tekstur 2f, normal 3 byte + 1 byte isian. */
struct MV {
    float x, y, z;
    float u, v;
    signed char nx, ny, nz, pad;
};

/* Arah cahaya ternormalisasi, disimpan supaya tes bisa menghitung sendiri
   warna yang diharapkan alih-alih memakai angka ajaib. */
static float g_l0[4], g_l1[4];

/* Nilai persis dari RenderHelper 1.12.2. */
static void enable_standard_item_lighting() {
    const float l0[4] = { 0.2f, 1.0f, -0.7f, 0.0f };
    const float l1[4] = { -0.2f, 1.0f, 0.7f, 0.0f };
    float *n0 = g_l0, *n1 = g_l1;
    const float m0 = sqrtf(l0[0]*l0[0] + l0[1]*l0[1] + l0[2]*l0[2]);
    const float m1 = sqrtf(l1[0]*l1[0] + l1[1]*l1[1] + l1[2]*l1[2]);
    for (int i = 0; i < 3; ++i) { n0[i] = l0[i] / m0; n1[i] = l1[i] / m1; }
    n0[3] = n1[3] = 0.0f;

    const float diffuse[4]  = { 0.6f, 0.6f, 0.6f, 1.0f };
    const float black[4]    = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float lm_amb[4]   = { 0.4f, 0.4f, 0.4f, 1.0f };

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightfv(GL_LIGHT0, GL_POSITION, n0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  black);
    glLightfv(GL_LIGHT0, GL_SPECULAR, black);
    glLightfv(GL_LIGHT1, GL_POSITION, n1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT1, GL_AMBIENT,  black);
    glLightfv(GL_LIGHT1, GL_SPECULAR, black);
    glShadeModel(GL_FLAT);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lm_amb);
}

/* Satu "kotak model": begin -> vertex -> draw, seperti TexturedQuad.draw(). */
static void draw_box(const MV *v) {
    glVertexPointer(3, GL_FLOAT, sizeof(MV), &v[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(MV), &v[0].u);
    glNormalPointer(GL_BYTE, sizeof(MV), &v[0].nx);
    glDrawArrays(GL_QUADS, 0, 4);
}

static void make_quad(MV *v, float x0, float x1, int nx, int ny, int nz) {
    const float y0 = 24.0f, y1 = 40.0f;
    const float xs[4] = { x0, x1, x1, x0 };
    const float ys[4] = { y0, y0, y1, y1 };
    for (int i = 0; i < 4; ++i) {
        v[i].x = xs[i]; v[i].y = ys[i]; v[i].z = 0.0f;
        v[i].u = 0.5f;  v[i].v = 0.5f;
        v[i].nx = (signed char) nx;
        v[i].ny = (signed char) ny;
        v[i].nz = (signed char) nz;
        v[i].pad = 0;
    }
}

int main() {
    puts("== Context + FBO ==");
    if (!egl_up()) { puts("  LINGKUNGAN TIDAK MENDUKUNG - dilewati"); return 77; }
    const bool inited = ensure_init();
    ok(inited, "Oryon siap: %s", g_state.renderer);

    GLuint rb = 0, fbo = 0;
    gles.glGenRenderbuffers(1, &rb);
    gles.glBindRenderbuffer(GL_RENDERBUFFER, rb);
    gles.glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, W, H);
    gles.glGenFramebuffers(1, &fbo);
    gles.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gles.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_RENDERBUFFER, rb);
    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();

    /* Tiga kotak: normal ke atas (paling terang), ke bawah (paling gelap),
       dan ke depan (di antaranya). Perbedaan ketiganya membuktikan
       pencahayaan benar-benar dihitung, bukan sekadar warna diteruskan. */
    static MV up[4], down[4], fwd[4];
    make_quad(up,    4.0f, 20.0f, 0,  127, 0);
    make_quad(down, 24.0f, 40.0f, 0, -127, 0);
    make_quad(fwd,  44.0f, 60.0f, 0,    0, 127);

    enable_standard_item_lighting();
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);   /* seperti Minecraft; tekstur mati */
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    /* --------------------------------------------- A. tanpa display list -- */
    puts("\n== A. Jalur langsung (seperti RenderItem) ==");
    clear_black();
    draw_box(up);
    draw_box(down);
    draw_box(fwd);
    ok(gles.glGetError() == GL_NO_ERROR, "tanpa error GL");

    const Px a_up = pixel(12, 32), a_dn = pixel(32, 32), a_fw = pixel(52, 32);
    /* lit = 0.4 + 0.6*dot(n,L0) + 0.6*dot(n,L1), lalu dijepit ke 1.0 */
    /* Nilai harapan dihitung dari rumus yang sama dengan shader:
       lit = lm_ambient + sum(diffuse_i * max(dot(n, L_i), 0)), dijepit ke 1. */
    const float dot_up = (g_l0[1] > 0 ? g_l0[1] : 0) + (g_l1[1] > 0 ? g_l1[1] : 0);
    const int exp_up = (int) (255.0f * (0.4f + 0.6f * dot_up) + 0.5f);
    const int exp_dn = (int) (255.0f * 0.4f + 0.5f);
    const int exp_fw = (int) (255.0f * (0.4f + 0.6f * g_l1[2]) + 0.5f);

    ok(a_up.r > 240 && exp_up >= 255, "normal ke atas terang penuh (%u)", a_up.r);
    ok(near_(a_dn.r, exp_dn), "normal ke bawah = ambien saja (%u, hitungan %d)",
       a_dn.r, exp_dn);
    ok(near_(a_fw.r, exp_fw), "normal ke depan sesuai hitungan (%u, hitungan %d)",
       a_fw.r, exp_fw);
    ok(a_up.r != a_dn.r && a_dn.r != a_fw.r,
       "ketiganya berbeda - pencahayaan memang dihitung");

    /* --------------------------------------------- B. lewat display list -- */
    puts("\n== B. Lewat display list (seperti ModelRenderer) ==");
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    draw_box(up);
    draw_box(down);
    draw_box(fwd);
    glEndList();
    ok(gles.glGetError() == GL_NO_ERROR, "kompilasi list tanpa error");

    clear_black();
    glCallList(list);
    const Px b_up = pixel(12, 32), b_dn = pixel(32, 32), b_fw = pixel(52, 32);

    ok(b_up.r > 0, "kotak PERTAMA tergambar (%u)", b_up.r);
    ok(b_dn.r > 0, "kotak KEDUA tergambar (%u)", b_dn.r);
    ok(b_fw.r > 0, "kotak KETIGA tergambar (%u)", b_fw.r);
    ok(near_(b_up.r, a_up.r) && near_(b_dn.r, a_dn.r) && near_(b_fw.r, a_fw.r),
       "hasil list sama persis dengan jalur langsung (%u,%u,%u vs %u,%u,%u)",
       b_up.r, b_dn.r, b_fw.r, a_up.r, a_dn.r, a_fw.r);

    /* Model biped punya enam kotak; sebuah list harus sanggup menampung
       jauh lebih banyak dari itu. */
    puts("\n== C. List dengan banyak kotak (model biped punya 6) ==");
    GLuint big = glGenLists(1);
    glNewList(big, GL_COMPILE);
    static MV strip[12][4];
    for (int i = 0; i < 12; ++i) {
        make_quad(strip[i], (float) (i * 5) + 1.0f, (float) (i * 5) + 4.0f, 0, 127, 0);
        draw_box(strip[i]);
    }
    glEndList();
    clear_black();
    glCallList(big);
    int drawn = 0;
    for (int i = 0; i < 12; ++i)
        if (pixel(i * 5 + 2, 32).r > 200) ++drawn;
    ok(drawn == 12, "%d/12 kotak tergambar dari satu list", drawn);
    ok(gles.glGetError() == GL_NO_ERROR, "tanpa error GL");

    /* ------------------------------- D. lightmap tanpa array koordinat --- */
    puts("\n== D. Lightmap unit 1 tanpa array koordinat ==");
    {
        /* Inilah bentuk sebenarnya jalur entitas Minecraft:
             unit 0 = tekstur entitas, punya array koordinat
             unit 1 = lightmap, TIDAK punya array - koordinatnya dikirim lewat
                      OpenGlHelper.setLightmapTextureCoords() alias
                      glMultiTexCoord2f(GL_TEXTURE1, ...)

           Aturan GL: unit yang menyala tanpa array memakai koordinat BERJALAN.
           Kalau Oryon memakai (0,0) sebagai gantinya, lightmap tersampel di
           sudut tergelapnya dan seluruh entitas jadi hitam - persis gejala
           yang dilaporkan dari perangkat. Format vertex terrain punya DUA
           koordinat, jadi terrain tidak pernah kena. */
        const unsigned char skin[4]   = { 255, 128, 0, 255 };
        /* 2x2: hanya texel (1,1) yang terang, sisanya gelap - seperti lightmap. */
        const unsigned char lmap[16]  = { 0,0,0,255,   0,0,0,255,
                                          0,0,0,255,   255,255,255,255 };
        GLuint t_skin = 0, t_lmap = 0;
        gles.glGenTextures(1, &t_skin);
        gles.glGenTextures(1, &t_lmap);
        for (int i = 0; i < 2; ++i) {
            gles.glActiveTexture(GL_TEXTURE0 + i);
            gles.glBindTexture(GL_TEXTURE_2D, i ? t_lmap : t_skin);
            gles.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, i ? 2 : 1, i ? 2 : 1, 0,
                              GL_RGBA, GL_UNSIGNED_BYTE, i ? lmap : skin);
            gles.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            gles.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gles.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gles.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        glActiveTexture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        /* Hanya unit 0 yang punya array koordinat - persis seperti
           OLDMODEL_POSITION_TEX_NORMAL. */
        glClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        static MV lit_quad[4];
        make_quad(lit_quad, 4.0f, 60.0f, 0, 127, 0);   /* normal ke atas: terang penuh */

        GLuint elist = glGenLists(1);
        glNewList(elist, GL_COMPILE);
        draw_box(lit_quad);
        glEndList();

        /* Koordinat lightmap menunjuk texel terang. */
        glMultiTexCoord2f(GL_TEXTURE1, 0.75f, 0.75f);
        clear_black();
        glCallList(elist);
        const Px bright = pixel(32, 32);
        ok(bright.r > 200 && bright.g > 100 && bright.b < 40,
           "lightmap terang -> tekstur entitas terlihat (%u,%u,%u)",
           bright.r, bright.g, bright.b);

        /* Koordinat lightmap menunjuk texel gelap: HARUS hitam. Kalau tetap
           terang, berarti koordinat berjalan tidak dipakai sama sekali. */
        glMultiTexCoord2f(GL_TEXTURE1, 0.25f, 0.25f);
        clear_black();
        glCallList(elist);
        const Px dark = pixel(32, 32);
        ok(dark.r < 20 && dark.g < 20,
           "lightmap gelap -> gelap (%u,%u,%u)", dark.r, dark.g, dark.b);
        ok(bright.r != dark.r,
           "koordinat berjalan glMultiTexCoord2f benar-benar berpengaruh");

        glDeleteLists(elist, 1);
        glActiveTexture(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glDeleteLists(list, 1);
    glDeleteLists(big, 1);
    glDisable(GL_LIGHTING);

    printf("\n===== %d lulus, %d gagal =====\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
