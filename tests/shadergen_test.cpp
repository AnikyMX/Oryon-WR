/* Tes tahap 4: state fixed-function + generator shader.

   Bukan tes unit murni - setiap shader yang dibangkitkan benar-benar
   dikompilasi dan ditautkan oleh driver GLES 3.2 sungguhan (Mesa di sandbox,
   Adreno/Mali di perangkat). Sumber GLSL yang "kelihatan benar" tapi ditolak
   kompiler adalah kegagalan, dan tes ini yang menangkapnya. */

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "oryon/oryon.h"
#include "../src/state/ff_state.h"
#include "../src/state/state.h"
#include "../src/shadergen/shadergen.h"
#include "../src/driver/driver.h"
#include "../src/util/mat4.h"

/* Deklarasi seluruh titik ekspor, dari daftar yang sama dengan yang dipakai .so. */
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

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLContext g_ctx = EGL_NO_CONTEXT;

/* Sengaja TIDAK memakai eglGetProcAddress: biner ini ikut mendefinisikan
   eglGetProcAddress milik Oryon, jadi memanggilnya di sini akan melingkar. */
static bool egl_up() {
    g_dpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                  EGL_DEFAULT_DISPLAY, NULL);
    if (g_dpy == EGL_NO_DISPLAY) g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_dpy == EGL_NO_DISPLAY) return false;

    EGLint maj = 0, min = 0;
    if (!eglInitialize(g_dpy, &maj, &min)) return false;
    if (!eglBindAPI(EGL_OPENGL_ES_API)) return false;

    static const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(g_dpy, cfg_attr, &cfg, 1, &n) || n < 1) return false;

    static const EGLint ctx_attr[][5] = {
        { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE },
        { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE },
    };
    for (int i = 0; i < 2 && g_ctx == EGL_NO_CONTEXT; ++i)
        g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attr[i]);
    if (g_ctx == EGL_NO_CONTEXT) return false;

    if (!eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g_ctx)) {
        static const EGLint pb[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
        EGLSurface s = eglCreatePbufferSurface(g_dpy, cfg, pb);
        if (s == EGL_NO_SURFACE || !eglMakeCurrent(g_dpy, s, s, g_ctx)) return false;
    }
    return true;
}

/* ----------------------------------------------------------- daftar kunci - */

struct Case { const char *name; FFKey k; };

static FFKey mk(uint16_t tex, uint8_t flags, uint8_t lights = 0,
                uint8_t fog = 0, uint8_t af = 0, uint16_t attr_tex = 0,
                uint16_t gen = 0, uint16_t gen_eye = 0,
                uint16_t texmat = 0, uint16_t replace = 0) {
    FFKey k;
    memset(&k, 0, sizeof k);
    k.tex_enable  = tex;
    k.attr_tex    = attr_tex;
    k.tex_gen     = gen;
    k.tex_gen_eye = gen_eye;
    k.tex_matrix  = texmat;
    k.tex_replace = replace;
    k.flags       = flags;
    k.light_mask  = lights;
    k.fog_mode    = fog;
    k.alpha_func  = af;
    return k;
}

int main(int argc, char **argv) {
    const bool dump = (argc > 1 && strcmp(argv[1], "--dump") == 0);
    printf("== Context GLES 3.2 (Mesa) ==\n");
    if (!egl_up()) { puts("  LINGKUNGAN TIDAK MENDUKUNG - dilewati"); return 77; }
    ok(ensure_init(), "driver GLES dimuat, state Oryon siap");
    printf("  renderer: %s\n", g_state.renderer);

    /* --------------------------------------------------- tumpukan matriks - */
    puts("\n== Tumpukan matriks ==");
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(1.0f, 2.0f, 3.0f);
    float m[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    ok(m[12] == 1.0f && m[13] == 2.0f && m[14] == 3.0f,
       "glTranslatef terbaca balik lewat glGetFloatv (%.1f, %.1f, %.1f)",
       m[12], m[13], m[14]);

    glPushMatrix();
    glTranslatef(10.0f, 0.0f, 0.0f);
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    ok(m[12] == 11.0f, "push + translate menumpuk (x = %.1f)", m[12]);
    glPopMatrix();
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    ok(m[12] == 1.0f, "pop mengembalikan matriks (x = %.1f)", m[12]);

    glPushMatrix();
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    ok(m[0] > -0.001f && m[0] < 0.001f && m[1] > 0.999f,
       "rotasi 90 derajat pada Z benar (m0=%.3f m1=%.3f)", m[0], m[1]);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 480, 0, -1, 1);
    glGetFloatv(GL_PROJECTION_MATRIX, m);
    ok(m[0] > 0.00312f && m[0] < 0.00313f && m[5] < -0.00416f,
       "glOrtho menghasilkan proyeksi layar (m0=%.5f m5=%.5f)", m[0], m[5]);
    glMatrixMode(GL_MODELVIEW);

    GLint depth = 0;
    glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &depth);
    ok(depth == 1, "kedalaman tumpukan kembali ke 1 (%d)", depth);

    /* ------------------------------------------------- penurunan FFKey ---- */
    puts("\n== Penurunan FFKey dari panggilan GL ==");
    FFKey k;
    ff_key(&k, 0, false, false);
    ok(k.flags == 0 && k.tex_enable == 0, "state bersih -> kunci kosong");

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);
    ff_key(&k, 1, true, false);
    ok(k.tex_enable == 1 && (k.flags & FF_ALPHA_TEST) &&
       k.alpha_func == (GL_GREATER - GL_NEVER),
       "TEXTURE_2D + ALPHA_TEST(GREATER) masuk ke kunci");

    glAlphaFunc(GL_ALWAYS, 0.0f);
    ff_key(&k, 1, true, false);
    ok(!(k.flags & FF_ALPHA_TEST),
       "GL_ALWAYS menghapus uji alpha dari kunci - tanpa cabang sia-sia di shader");

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    ff_key(&k, 1, true, true);
    ok((k.flags & FF_FOG) && k.fog_mode == FOG_LINEAR && k.light_mask == 0x3,
       "kabut LINEAR + LIGHT0/LIGHT1 masuk ke kunci (mask=0x%X)", k.light_mask);

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);

    /* --------------------------------------- kompilasi shader sungguhan --- */
    puts("\n== Kompilasi shader oleh driver sungguhan ==");
    const Case cases[] = {
        { "polos, warna vertex",        mk(0, FF_ATTR_COLOR) },
        { "polos, warna uniform",       mk(0, 0) },
        { "1 tekstur modulate",         mk(0x1, FF_ATTR_COLOR, 0,0,0, 0x1) },
        { "1 tekstur replace",          mk(0x1, FF_ATTR_COLOR, 0,0,0, 0x1, 0,0,0, 0x1) },
        { "2 tekstur (terrain+lightmap)", mk(0x3, FF_ATTR_COLOR, 0,0,0, 0x3) },
        { "uji alpha GREATER",          mk(0x1, FF_ATTR_COLOR|FF_ALPHA_TEST, 0,0,4, 0x1) },
        { "kabut LINEAR",               mk(0x1, FF_ATTR_COLOR|FF_FOG, 0, FOG_LINEAR, 0, 0x1) },
        { "kabut EXP",                  mk(0x1, FF_ATTR_COLOR|FF_FOG, 0, FOG_EXP, 0, 0x1) },
        { "kabut EXP2",                 mk(0x1, FF_ATTR_COLOR|FF_FOG, 0, FOG_EXP2, 0, 0x1) },
        { "cahaya 2 arah + normal",     mk(0x1, FF_ATTR_COLOR|FF_ATTR_NORMAL|FF_LIGHTING|FF_COLOR_MATERIAL|FF_NORMALIZE, 0x3, 0,0, 0x1) },
        { "shading FLAT",               mk(0x1, FF_ATTR_COLOR|FF_FLAT, 0,0,0, 0x1) },
        { "texgen OBJECT_LINEAR",       mk(0x1, FF_ATTR_COLOR, 0,0,0, 0, 0x1, 0x0) },
        { "texgen EYE_LINEAR + texmat", mk(0x1, FF_ATTR_COLOR, 0,0,0, 0, 0x1, 0x1, 0x1) },
        { "semuanya sekaligus",         mk(0x3, FF_ATTR_COLOR|FF_ATTR_NORMAL|FF_LIGHTING|
                                              FF_COLOR_MATERIAL|FF_NORMALIZE|FF_FOG|
                                              FF_ALPHA_TEST|FF_FLAT, 0x3, FOG_EXP2, 4,
                                              0x3, 0x2, 0x2, 0x2, 0x2) },
    };
    const int ncase = (int) (sizeof cases / sizeof cases[0]);

    static char vs[8192], fs[8192];
    int biggest = 0;
    for (int i = 0; i < ncase; ++i) {
        unsigned nv = ffp_gen_vertex(cases[i].k, vs, sizeof vs);
        unsigned nf = ffp_gen_fragment(cases[i].k, fs, sizeof fs);
        FFProgram *p = ffp_program(cases[i].k);
        if ((int) (nv + nf) > biggest) biggest = (int) (nv + nf);
        ok(nv > 0 && nf > 0 && p && p->prog,
           "%-28s  vs=%4u B  fs=%4u B  prog=%u", cases[i].name, nv, nf,
           p ? p->prog : 0u);
    }
    printf("  sumber GLSL terbesar: %d byte (batas %u)\n", biggest, (unsigned) sizeof vs);

    /* ------------------------------------------------------ cache program - */
    puts("\n== Cache program ==");
    FFProgram *a1 = ffp_program(cases[0].k);
    FFProgram *a2 = ffp_program(cases[0].k);
    ok(a1 == a2 && a1->prog == a2->prog, "kunci sama -> program yang sama persis");
    FFProgram *b1 = ffp_program(cases[4].k);
    ok(b1 != a1 && b1->prog != a1->prog, "kunci beda -> program berbeda");
    ok(sizeof(FFKey) == 16, "FFKey tepat 16 byte (%u)", (unsigned) sizeof(FFKey));

    /* ---------------------------------------------- shader minimal ramping */
    puts("\n== Shader jalur panas tetap ramping ==");
    FFKey plain = mk(0x3, FF_ATTR_COLOR, 0, 0, 0, 0x3);
    unsigned nv = ffp_gen_vertex(plain, vs, sizeof vs);
    ok(!strstr(vs, "u_mv") || strstr(vs, "u_mvp") == strstr(vs, "u_mv"),
       "tanpa kabut/cahaya, matriks ruang mata tidak ikut ditulis");
    ok(!strstr(vs, "lightDiff"), "tanpa pencahayaan, tidak ada kode cahaya sama sekali");
    ffp_gen_fragment(plain, fs, sizeof fs);
    ok(!strstr(fs, "discard"), "tanpa uji alpha, tidak ada discard di fragment shader");
    ok(!strstr(fs, "u_fogColor"), "tanpa kabut, tidak ada uniform kabut");
    printf("  terrain 2-tekstur: vs=%u B, fs=%u B\n", nv,
           (unsigned) strlen(fs));

    /* -------------------------------------------------- pemasangan uniform */
    puts("\n== ffp_bind: pasang program + unggah uniform ==");
    while (gles.glGetError() != GL_NO_ERROR) { }
    ok(ffp_bind(cases[13].k), "ffp_bind pada kunci terberat berhasil");
    ok(gles.glGetError() == GL_NO_ERROR, "tanpa error GL setelah unggah uniform");
    ok(ffp_bind(cases[13].k), "bind kedua (jalur cache) berhasil");
    ok(gles.glGetError() == GL_NO_ERROR, "tetap tanpa error GL");

    /* ------------------------------------- penjagaan unggahan uniform ---- */
    puts("\n== Unggahan uniform hanya saat state berubah ==");
    {
        FFKey k = cases[9].k;                 /* kunci dengan cahaya + warna */
        FFProgram *p = ffp_program(k);
        ok(p != 0, "program tersedia");
        if (p) {
            ffp_bind(k);
            ok(p->uni_serial == g_ff.uni_serial && p->mat_serial == g_ff.serial,
               "setelah bind pertama, kedua serial sinkron");

            const uint32_t before = g_ff.uni_serial;
            ffp_bind(k);
            ok(g_ff.uni_serial == before && p->uni_serial == before,
               "bind ulang tanpa perubahan state tidak mengunggah apa pun");

            glColor4f(0.25f, 0.5f, 0.75f, 1.0f);
            ok(g_ff.uni_serial != before, "glColor4f menandai uniform kotor");
            ffp_bind(k);
            ok(p->uni_serial == g_ff.uni_serial, "bind berikutnya menyusul serial baru");

            const uint32_t m = g_ff.serial;
            glMatrixMode(GL_MODELVIEW);
            glTranslatef(1.0f, 0.0f, 0.0f);
            ok(g_ff.serial != m, "glTranslatef menandai matriks kotor");
            ok(p->uni_serial == g_ff.uni_serial,
               "perubahan matriks TIDAK ikut mengotori blok uniform");
            glLoadIdentity();
        }
    }

    if (dump) {
        for (int i = 0; i < ncase; ++i) {
            ffp_gen_vertex(cases[i].k, vs, sizeof vs);
            ffp_gen_fragment(cases[i].k, fs, sizeof fs);
            printf("\n/* ================= %s ================= */\n", cases[i].name);
            printf("/* --- vertex --- */\n%s\n/* --- fragment --- */\n%s", vs, fs);
        }
    }

    printf("\n===== %d lulus, %d gagal =====\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
