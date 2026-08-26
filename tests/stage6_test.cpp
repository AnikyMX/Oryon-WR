/* Tes tahap 6: tekstur, display list, dan shader Minecraft sendiri.

   Bagian shader memakai sumber asli dari assets/minecraft/shaders bila
   tests/mc_shaders.inc tersedia (dibangkitkan oleh tools/gen_shader_tests.py,
   tidak di-commit). Tanpa itu, dipakai shader sintetis yang memakai konstruksi
   yang sama persis: attribute, varying, texture2D, gl_FragColor, #extension,
   dan `sample` sebagai nama variabel. */

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
#include "../src/shadergen/shadergen.h"
#include "../src/shadergen/translate.h"

extern "C" {
#define ORYON_SYM(strat, ret, name, params, args) ret name params;
#include "oryon/gl_symbols.inc"
}

#if defined(ORYON_HAVE_MC_SHADERS)
#include "mc_shaders.inc"
#endif

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
static bool near_(unsigned char a, int w, int t = 3) { int d = (int) a - w; return d >= -t && d <= t; }
static bool px_is(Px p, int r, int g, int b) { return near_(p.r, r) && near_(p.g, g) && near_(p.b, b); }
static void clear_black() {
    gles.glClearColor(0, 0, 0, 1);
    gles.glClear(GL_COLOR_BUFFER_BIT);
}

static const float Q0 = 16.0f, Q1 = 48.0f;
static void quad(float u0, float v0, float u1, float v1) {
    glBegin(GL_QUADS);
    glColor4f(1, 1, 1, 1);
    glTexCoord2f(u0, v0); glVertex3f(Q0, Q0, 0);
    glTexCoord2f(u1, v0); glVertex3f(Q1, Q0, 0);
    glTexCoord2f(u1, v1); glVertex3f(Q1, Q1, 0);
    glTexCoord2f(u0, v1); glVertex3f(Q0, Q1, 0);
    glEnd();
}

/* Shader sintetis: memakai konstruksi GLSL 1.20 yang sama dengan milik
   Minecraft, termasuk `sample` sebagai nama variabel dan satu #extension. */
static const char kSynthVs[] =
    "#version 120\n"
    "attribute vec4 Position;\n"
    "uniform mat4 ProjMat;\n"
    "uniform vec2 OutSize;\n"
    "varying vec2 texCoord;\n"
    "void main(){\n"
    "    gl_Position = ProjMat * vec4(Position.xy, 0.0, 1.0);\n"
    "    texCoord = Position.xy / OutSize;\n"
    "}\n";

static const char kSynthFs[] =
    "#version 120\n"
    "#extension GL_EXT_gpu_shader4 : enable\n"
    "uniform sampler2D DiffuseSampler;\n"
    "varying vec2 texCoord;\n"
    "// komentar yang menyebut attribute dan texture2D, tidak boleh diubah\n"
    "void main() {\n"
    "    vec4 sample = texture2D(DiffuseSampler, texCoord);\n"
    "    gl_FragColor = vec4(sample.rgb, 1.0);\n"
    "}\n";

static GLuint compile_via_oryon(GLenum stage, const char *src, char *log, int logn) {
    GLuint sh = gles.glCreateShader(stage);
    const GLchar *p = src;
    glShaderSource(sh, 1, &p, 0);          /* lewat Oryon: diterjemahkan di sini */
    gles.glCompileShader(sh);
    GLint okc = 0;
    gles.glGetShaderiv(sh, GL_COMPILE_STATUS, &okc);
    if (!okc && log) {
        GLsizei n = 0;
        gles.glGetShaderInfoLog(sh, logn, &n, log);
    }
    return okc ? sh : 0;
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
    ok(gles.glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
       "FBO 64x64 lengkap");

    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();

    /* ------------------------------------- A. unggahan BGRA ala TextureUtil */
    puts("\n== A. glTexImage2D BGRA + UNSIGNED_INT_8_8_8_8_REV ==");
    /* Persis tata letak TextureUtil: int ARGB Java. 0xFF20A0E0 -> R=0x20 G=0xA0 B=0xE0 */
    static const unsigned int argb[4] = { 0xFF20A0E0u, 0xFF20A0E0u,
                                          0xFF20A0E0u, 0xFF20A0E0u };
    GLuint tex = 0;
    gles.glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_BGRA,
                 GL_UNSIGNED_INT_8_8_8_8_REV, argb);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ok(gles.glGetError() == GL_NO_ERROR, "tanpa error GL saat unggah");

    clear_black();
    glEnable(GL_TEXTURE_2D);
    quad(0.5f, 0.5f, 0.5f, 0.5f);
    Px p = pixel(32, 32);
    ok(px_is(p, 0x20, 0xA0, 0xE0),
       "int ARGB 0xFF20A0E0 -> piksel (%u,%u,%u), R dan B tidak tertukar", p.r, p.g, p.b);

    /* ------------------------------------- B. glTexSubImage2D BGRA -------- */
    puts("\n== B. glTexSubImage2D BGRA ==");
    static const unsigned int argb2[4] = { 0xFFE0A020u, 0xFFE0A020u,
                                           0xFFE0A020u, 0xFFE0A020u };
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_BGRA,
                    GL_UNSIGNED_INT_8_8_8_8_REV, argb2);
    clear_black();
    quad(0.5f, 0.5f, 0.5f, 0.5f);
    p = pixel(32, 32);
    ok(px_is(p, 0xE0, 0xA0, 0x20), "sub-unggahan juga benar (%u,%u,%u)", p.r, p.g, p.b);

    /* ------------------------------------- C. GL_CLAMP -> CLAMP_TO_EDGE --- */
    puts("\n== C. GL_CLAMP (tidak ada di GLES) ==");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    ok(gles.glGetError() == GL_NO_ERROR, "GL_CLAMP diterima tanpa GL_INVALID_ENUM");
    clear_black();
    quad(2.0f, 2.0f, 2.0f, 2.0f);      /* jauh di luar [0,1] */
    p = pixel(32, 32);
    ok(px_is(p, 0xE0, 0xA0, 0x20), "sampel di luar [0,1] terjepit ke tepi (%u,%u,%u)",
       p.r, p.g, p.b);

    /* ------------------------------------- D. glGetTexImage --------------- */
    puts("\n== D. glGetTexImage lewat pembacaan FBO ==");
    {
        unsigned char back[16];
        memset(back, 0, sizeof back);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, back);
        ok(back[0] == 0xE0 && back[1] == 0xA0 && back[2] == 0x20,
           "tekstur terbaca balik: (%u,%u,%u)", back[0], back[1], back[2]);
    }
    glDisable(GL_TEXTURE_2D);

    /* ------------------------------------- E. display list ---------------- */
    puts("\n== E. Display list ==");
    GLuint list = glGenLists(2);
    ok(list != 0, "glGenLists(2) -> %u", list);
    glNewList(list, GL_COMPILE);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 0.0f, 1.0f, 1.0f);
    glVertex3f(0, 0, 0); glVertex3f(16, 0, 0); glVertex3f(16, 16, 0); glVertex3f(0, 16, 0);
    glEnd();
    glEndList();
    ok(gles.glGetError() == GL_NO_ERROR, "kompilasi list tanpa error");

    clear_black();
    glCallList(list);
    ok(px_is(pixel(8, 8), 255, 0, 255) && px_is(pixel(40, 40), 0, 0, 0),
       "glCallList menggambar quad yang direkam");

    /* State SEKARANG harus berlaku saat pemutaran ulang - itulah yang membuat
       langit ikut bergerak bersama kamera. */
    clear_black();
    glPushMatrix();
    glTranslatef(32.0f, 32.0f, 0.0f);
    glCallList(list);
    glPopMatrix();
    ok(px_is(pixel(40, 40), 255, 0, 255) && px_is(pixel(8, 8), 0, 0, 0),
       "matriks saat pemanggilan ikut berlaku, bukan matriks saat perekaman");

    clear_black();
    glCallList(list);
    glCallList(list);
    ok(px_is(pixel(8, 8), 255, 0, 255), "list bisa dipanggil berulang");
    glDeleteLists(list, 2);

    /* ------------------------------------- F. penerjemah GLSL ------------- */
    puts("\n== F. Penerjemah GLSL 1.20 -> ES 3.20 ==");
    {
        char out[8192], attrib[64];
        GlslDefault defs[32];
        unsigned nd = 0;
        unsigned n = glsl_translate(kSynthVs, (unsigned) strlen(kSynthVs), false,
                                    out, sizeof out, attrib, sizeof attrib,
                                    defs, 32, &nd);
        ok(n > 0, "vertex diterjemahkan (%u byte)", n);
        ok(strstr(out, "#version 320 es") == out, "versi ditulis ulang");
        ok(strstr(out, "in vec4 Position") != 0, "attribute -> in");
        ok(strstr(out, "out vec2 texCoord") != 0, "varying -> out (vertex)");
        ok(strcmp(attrib, "Position") == 0, "nama atribut terdeteksi: \"%s\"", attrib);

        n = glsl_translate(kSynthFs, (unsigned) strlen(kSynthFs), true,
                           out, sizeof out, attrib, sizeof attrib, defs, 32, &nd);
        ok(n > 0, "fragment diterjemahkan (%u byte)", n);
        ok(strstr(out, "in vec2 texCoord") != 0, "varying -> in (fragment)");
        ok(strstr(out, "layout(location = 0) out vec4 oryon_FragColor") != 0,
           "gl_FragColor jadi keluaran yang dideklarasikan");
        ok(strstr(out, "texture(DiffuseSampler") != 0, "texture2D -> texture");
        ok(strstr(out, "sample_") != 0 && strstr(out, "vec4 sample ") == 0,
           "`sample` (kata cadangan ES3) diganti namanya");
        ok(strstr(out, "#extension") == 0, "baris #extension dibuang");
        ok(strstr(out, "komentar yang menyebut attribute dan texture2D") != 0,
           "isi komentar tidak ikut diubah");
    }

    /* ------------------------------------- G. kompilasi sungguhan --------- */
    puts("\n== G. Kompilasi oleh driver ==");
    {
        char log[1024] = { 0 };
        GLuint vs = compile_via_oryon(GL_VERTEX_SHADER, kSynthVs, log, sizeof log);
        ok(vs != 0, "vertex sintetis terkompilasi%s%s", vs ? "" : ": ", vs ? "" : log);
        GLuint fs = compile_via_oryon(GL_FRAGMENT_SHADER, kSynthFs, log, sizeof log);
        ok(fs != 0, "fragment sintetis terkompilasi%s%s", fs ? "" : ": ", fs ? "" : log);

        if (vs && fs) {
            GLuint prog = gles.glCreateProgram();
            gles.glAttachShader(prog, vs);
            gles.glAttachShader(prog, fs);
            glLinkProgram(prog);                 /* lewat Oryon: mengikat atribut 0 */
            GLint linked = 0;
            gles.glGetProgramiv(prog, GL_LINK_STATUS, &linked);
            ok(linked != 0, "program tertaut");
            GLint loc = gles.glGetAttribLocation(prog, "Position");
            ok(loc == ATTR_POS, "atribut \"Position\" terikat ke lokasi %d", loc);
            GLint uloc = gles.glGetUniformLocation(prog, "ProjMat");
            ok(uloc >= 0, "uniform \"ProjMat\" masih bisa dicari dari sisi Java");

            /* ---------------------------- H. gambar dengan program pengguna */
            puts("\n== H. Menggambar dengan program milik Minecraft ==");
            clear_black();
            glUseProgram(prog);
            float ortho[16];
            glGetFloatv(GL_PROJECTION_MATRIX, ortho);
            gles.glUniformMatrix4fv(uloc, 1, GL_FALSE, ortho);
            gles.glUniform2f(gles.glGetUniformLocation(prog, "OutSize"), W, H);
            gles.glUniform1i(gles.glGetUniformLocation(prog, "DiffuseSampler"), 0);
            gles.glActiveTexture(GL_TEXTURE0);
            gles.glBindTexture(GL_TEXTURE_2D, tex);

            struct V { float x, y, z; };
            static const V vv[4] = { { Q0, Q0, 0 }, { Q1, Q0, 0 },
                                     { Q1, Q1, 0 }, { Q0, Q1, 0 } };
            glEnableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_COLOR_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glVertexPointer(3, GL_FLOAT, sizeof(V), &vv[0].x);
            glDrawArrays(GL_QUADS, 0, 4);
            ok(gles.glGetError() == GL_NO_ERROR, "tanpa error GL");
            p = pixel(32, 32);
            ok(px_is(p, 0xE0, 0xA0, 0x20),
               "program pengguna menggambar, glVertexPointer sampai ke \"Position\" (%u,%u,%u)",
               p.r, p.g, p.b);

            glUseProgram(0);
            clear_black();
            glEnable(GL_TEXTURE_2D);
            quad(0.5f, 0.5f, 0.5f, 0.5f);
            ok(px_is(pixel(32, 32), 0xE0, 0xA0, 0x20),
               "glUseProgram(0) kembali ke pipeline tetap");
            glDisable(GL_TEXTURE_2D);
        }
    }

    /* ------------------------------- J. jalur yang membuat crash di perangkat */
    puts("\n== J. Minecraft.getGLMaximumTextureSize() ==");
    {
        /* Salinan persis loop di Minecraft.java. Sebelum tekstur proxy
           ditangani, loop ini selalu berakhir dengan -1, atlas jadi 0x0, dan
           Stitcher melempar "Unable to fit: minecraft:blocks/lava_flow". */
        int found = -1;
        for (int i = 16384; i > 0; i >>= 1) {
            glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGBA, i, i, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, 0);
            GLint w = 0;
            glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
            if (w != 0) { found = i; break; }
        }
        GLint driver_max = 0;
        gles.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &driver_max);
        ok(found > 0, "ukuran tekstur maksimum = %d (dulu -1)", found);
        ok(found <= driver_max, "tidak melampaui GL_MAX_TEXTURE_SIZE driver (%d)",
           driver_max);
        ok(gles.glGetError() == GL_NO_ERROR,
           "jalur proxy tidak menyisakan GL_INVALID_ENUM");

        GLint rgba = 0;
        glGetIntegerv(GL_RGBA_MODE, &rgba);
        GLint pm[2] = { 0, 0 };
        glGetIntegerv(GL_POLYGON_MODE, pm);
        ok(rgba == GL_TRUE, "GL_RGBA_MODE dijawab Oryon, bukan diteruskan");
        ok(pm[0] == GL_FILL && pm[1] == GL_FILL, "GL_POLYGON_MODE dijawab Oryon");
        ok(gles.glGetError() == GL_NO_ERROR,
           "kueri warisan tidak menyisakan error di driver");
    }

#if defined(ORYON_HAVE_MC_SHADERS)
    puts("\n== I. Seluruh shader bawaan Minecraft 1.12.2 ==");
    {
        int good = 0, bad = 0;
        char log[1024];
        const char *first_bad = 0;
        for (unsigned i = 0; i < sizeof kMcShaders / sizeof kMcShaders[0]; ++i) {
            log[0] = 0;
            GLuint sh = compile_via_oryon(kMcShaders[i].frag ? GL_FRAGMENT_SHADER
                                                             : GL_VERTEX_SHADER,
                                          kMcShaders[i].src, log, sizeof log);
            if (sh) { ++good; gles.glDeleteShader(sh); }
            else {
                ++bad;
                if (!first_bad) first_bad = kMcShaders[i].name;
                printf("       %-22s %s\n", kMcShaders[i].name, log);
            }
        }
        ok(bad == 0, "%d/%d shader asli terkompilasi di GLES 3.2", good, good + bad);
    }
#else
    puts("\n== I. Shader asli Minecraft: dilewati (tests/mc_shaders.inc tidak ada) ==");
#endif

    printf("\n===== %d lulus, %d gagal =====\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
