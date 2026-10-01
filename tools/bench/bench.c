/* tools/bench/bench.c -- CPU-side overhead benchmark of liboryon.so on Mesa EGL + GLES 3.2 (llvmpipe).
 * Loads the library exactly like the launcher/LWJGL: dlopen() + dlsym(). Viewport is 1x1 so rasterization
 * cost is negligible and the numbers reflect API/submission overhead. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef unsigned int E; typedef int I; typedef float F;
#define GL_TRIANGLE_STRIP 0x0005
#define GL_QUADS 0x0007
#define GL_TEXTURE_2D 0x0DE1
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_FLOAT 0x1406
#define GL_SHORT 0x1402
#define GL_UNSIGNED_BYTE 0x1401
#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY 0x8076
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_COMPILE 0x1300
#define GL_ALPHA_TEST 0x0BC0
#define GL_GREATER 0x0204
#define GL_RGBA 0x1908
#define GL_BGRA 0x80E1
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_NEAREST 0x2600
#define GL_NORMAL_ARRAY 0x8075
#define GL_BYTE 0x1400
#define GL_LIGHTING 0x0B50
#define GL_FOG 0x0B60
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_COMBINE 0x8570

static void *L;
#define S(ret, name, args) static ret (*name) args;
#define LOAD(name) name = (__typeof__(name))dlsym(L, #name); if (!name) { fprintf(stderr, "missing %s\n", #name); exit(1); }
S(void, glDepthMask, (unsigned char)) S(void, glPushMatrix, (void)) S(void, glPopMatrix, (void)) S(void, glTranslatef, (F, F, F))
S(void, glRotatef, (F, F, F, F)) S(void, glBegin, (E)) S(void, glEnd, (void)) S(void, glTexCoord2f, (F, F)) S(void, glVertex3f, (F, F, F))
S(void, glColor4f, (F, F, F, F)) S(void, glVertexPointer, (I, E, I, const void *)) S(void, glTexCoordPointer, (I, E, I, const void *))
S(void, glColorPointer, (I, E, I, const void *)) S(void, glNormalPointer, (E, I, const void *)) S(void, glEnableClientState, (E))
S(void, glDisableClientState, (E)) S(void, glDrawArrays, (E, I, I)) S(void, glGenBuffers, (I, unsigned *)) S(void, glBindBuffer, (E, unsigned))
S(void, glBufferData, (E, intptr_t, const void *, E)) S(unsigned, glGenLists, (I)) S(void, glNewList, (unsigned, E)) S(void, glEndList, (void))
S(void, glCallList, (unsigned)) S(void, glFinish, (void)) S(void, glViewport, (I, I, I, I)) S(void, glMatrixMode, (E)) S(void, glLoadIdentity, (void))
S(void, glOrtho, (double, double, double, double, double, double)) S(void, glEnable, (E)) S(void, glDisable, (E)) S(void, glBindTexture, (E, unsigned))
S(void, glGenTextures, (I, unsigned *)) S(void, glTexImage2D, (E, I, I, I, I, I, E, E, const void *)) S(void, glTexParameteri, (E, E, I))
S(unsigned, glGetError, (void)) S(void, glAlphaFunc, (E, F)) S(void, glClientActiveTexture, (E)) S(void, glTexEnvi, (E, E, I))

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; }
static unsigned long (*draws)(void);
static unsigned long d0;
static void dstart(void) { d0 = draws ? draws() : 0; }
static unsigned long dget(void) { return draws ? draws() - d0 : 0; }

int main(int argc, char **argv) {
    const char *so = argc > 1 ? argv[1] : "build-host/liboryon.so";
    const char *shim = getenv("ORYON_GLES_LIB");
    EGLDisplay dpy = ((PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT"))(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    eglInitialize(dpy, NULL, NULL); eglBindAPI(EGL_OPENGL_ES_API);
    EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLConfig cfg; EGLint nc; eglChooseConfig(dpy, ca, &cfg, 1, &nc);
    EGLint pa[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE}; EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pa);
    EGLint xa[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, xa); eglMakeCurrent(dpy, surf, surf, ctx);
    L = dlopen(so, RTLD_NOW | RTLD_LOCAL);
    if (!L) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    LOAD(glDepthMask) LOAD(glPushMatrix) LOAD(glPopMatrix) LOAD(glTranslatef) LOAD(glRotatef) LOAD(glBegin) LOAD(glEnd) LOAD(glTexCoord2f)
    LOAD(glVertex3f) LOAD(glColor4f) LOAD(glVertexPointer) LOAD(glTexCoordPointer) LOAD(glColorPointer) LOAD(glNormalPointer)
    LOAD(glEnableClientState) LOAD(glDisableClientState) LOAD(glDrawArrays) LOAD(glGenBuffers) LOAD(glBindBuffer) LOAD(glBufferData)
    LOAD(glGenLists) LOAD(glNewList) LOAD(glEndList) LOAD(glCallList) LOAD(glFinish) LOAD(glViewport) LOAD(glMatrixMode) LOAD(glLoadIdentity)
    LOAD(glOrtho) LOAD(glEnable) LOAD(glDisable) LOAD(glBindTexture) LOAD(glGenTextures) LOAD(glTexImage2D) LOAD(glTexParameteri)
    LOAD(glGetError) LOAD(glAlphaFunc) LOAD(glClientActiveTexture) LOAD(glTexEnvi)
    if (shim) { void *h = dlopen(shim, RTLD_NOW | RTLD_LOCAL); if (h) draws = (unsigned long (*)(void))dlsym(h, "shim_draw_count"); }
    void *es = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_LOCAL);
    void (*es_depthmask)(unsigned char) = (void (*)(unsigned char))dlsym(es, "glDepthMask");

    glGetError();
    glViewport(0, 0, 1, 1);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, 1, 0, 1, -1000, 1000); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    double t; const int N = 5000000;
    /* 1: passthrough overhead */
    t = now(); for (int i = 0; i < N; ++i) es_depthmask(i & 1); double tes = (now() - t) / N;
    t = now(); for (int i = 0; i < N; ++i) glDepthMask(i & 1); double tor = (now() - t) / N;
    printf("passthrough glDepthMask: oryon %.2f ns/call | direct GLES %.2f ns/call | overhead %+.2f ns\n", tor, tes, tor - tes);
    /* 2: matrix stack ops */
    t = now(); for (int i = 0; i < 1000000; ++i) { glPushMatrix(); glTranslatef(1, 2, 3); glRotatef(30, 0, 1, 0); glPopMatrix(); }
    printf("matrix push+translate+rotate+pop: %.1f ns/sequence\n", (now() - t) / 1e6);
    /* 3: FontRenderer pattern: glBegin(TRIANGLE_STRIP) + 4x(glTexCoord2f+glVertex3f) + glEnd per char */
    unsigned tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    unsigned px[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, px);
    glEnable(GL_TEXTURE_2D); glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.1f); glColor4f(1, 1, 1, 1);
    const int FR = 20, CH = 5000;
    glFinish(); dstart(); t = now();
    for (int f = 0; f < FR; ++f) {
        for (int c = 0; c < CH; ++c) {
            float x = (c % 100) * 0.01f, y = (c / 100) * 0.01f;
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(0, 0); glVertex3f(x, y, 0); glTexCoord2f(0, 1); glVertex3f(x, y + 0.01f, 0);
            glTexCoord2f(1, 0); glVertex3f(x + 0.01f, y, 0); glTexCoord2f(1, 1); glVertex3f(x + 0.01f, y + 0.01f, 0);
            glEnd();
        }
        glFinish();
    }
    printf("FontRenderer pattern: %.1f ns/char (incl. GPU finish) | ES draws/frame: %.1f for %d glBegin/glEnd\n", (now() - t) / (FR * CH), dget() / (double)FR, CH);
    glDisable(GL_ALPHA_TEST); glDisable(GL_TEXTURE_2D);
    /* 4: Tessellator pattern: client arrays POSITION_TEX_COLOR (24 B), glDrawArrays(GL_QUADS, 0, 4) each */
    struct V { float x, y, z, u, v; unsigned char c[4]; } q[4] = {{0, 0, 0, 0, 0, {255, 255, 255, 255}}, {1, 0, 0, 1, 0, {255, 255, 255, 255}},
                                                                     {1, 1, 0, 1, 1, {255, 255, 255, 255}}, {0, 1, 0, 0, 1, {255, 255, 255, 255}}};
    glVertexPointer(3, GL_FLOAT, 24, &q[0].x); glTexCoordPointer(2, GL_FLOAT, 24, &q[0].u); glColorPointer(4, GL_UNSIGNED_BYTE, 24, q[0].c);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    const int TD = 2000;
    glFinish(); dstart(); t = now();
    for (int f = 0; f < FR; ++f) { for (int i = 0; i < TD; ++i) glDrawArrays(GL_QUADS, 0, 4); glFinish(); }
    printf("Tessellator pattern (client arrays, QUADS): %.1f ns/draw | ES draws/frame: %.0f\n", (now() - t) / (FR * TD), dget() / (double)FR);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_COLOR_ARRAY);
    /* 5: chunk VBO pattern: push/translate, pointers with VBO offsets, glDrawArrays(GL_QUADS), pop */
    enum { NB = 64, QN = 1024 };
    unsigned vbo[NB]; glGenBuffers(NB, vbo);
    struct CV { float x, y, z, u, v; short l[2]; unsigned char c[4]; } *cv = malloc(sizeof(struct CV) * 4 * QN);
    for (int i = 0; i < 4 * QN; ++i) { cv[i].x = (i & 1) ? 0.001f : 0; cv[i].y = (i & 2) ? 0.001f : 0; cv[i].z = 0; cv[i].u = cv[i].v = 0; cv[i].l[0] = cv[i].l[1] = 240; memset(cv[i].c, 255, 4); }
    for (int b = 0; b < NB; ++b) { glBindBuffer(GL_ARRAY_BUFFER, vbo[b]); glBufferData(GL_ARRAY_BUFFER, sizeof(struct CV) * 4 * QN, cv, GL_STATIC_DRAW); }
    glEnableClientState(GL_COLOR_ARRAY);
    glFinish(); dstart(); t = now();
    for (int f = 0; f < FR; ++f) {
        for (int b = 0; b < NB; ++b) {
            glPushMatrix(); glTranslatef(0.001f * b, 0, 0);
            glBindBuffer(GL_ARRAY_BUFFER, vbo[b]);
            glVertexPointer(3, GL_FLOAT, 28, (void *)0); glColorPointer(4, GL_UNSIGNED_BYTE, 28, (void *)24);
            glDrawArrays(GL_QUADS, 0, 4 * QN);
            glPopMatrix();
        }
        glFinish();
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0); glDisableClientState(GL_COLOR_ARRAY);
    printf("chunk VBO pattern (%d quads/draw): %.1f ns/draw incl. GPU | ES draws/frame: %.0f\n", QN, (now() - t) / (FR * NB), dget() / (double)FR);
    /* 6: display list (ModelRenderer): 10 boxes x 6 quads compiled, called with push/translate/pop */
    unsigned lst = glGenLists(1);
    struct NV { float x, y, z, u, v; signed char n[3], pad; } box[4];
    glNewList(lst, GL_COMPILE);
    for (int bx = 0; bx < 60; ++bx) {
        for (int k = 0; k < 4; ++k) { box[k].x = (k == 1 || k == 2) ? 0.001f : 0; box[k].y = k >= 2 ? 0.001f : 0; box[k].z = bx * 1e-4f; box[k].u = box[k].v = 0; box[k].n[0] = 0; box[k].n[1] = 0; box[k].n[2] = 127; }
        glVertexPointer(3, GL_FLOAT, 24, &box[0].x); glTexCoordPointer(2, GL_FLOAT, 24, &box[0].u); glNormalPointer(GL_BYTE, 24, box[0].n);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_NORMAL_ARRAY);
        glDrawArrays(GL_QUADS, 0, 4);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_NORMAL_ARRAY);
    }
    glEndList();
    const int LC = 5000;
    glFinish(); dstart(); t = now();
    for (int f = 0; f < FR; ++f) { for (int i = 0; i < LC; ++i) { glPushMatrix(); glTranslatef(0, 0, 0.0001f * i); glCallList(lst); glPopMatrix(); } glFinish(); }
    printf("display list (60 quads compiled from 60 Tessellator draws): %.1f ns/call | ES draws/call: %.2f\n", (now() - t) / (FR * LC), dget() / (double)(FR * LC));
    /* 7: state churn (entity rendering): per draw toggle LIGHTING / FOG / texenv MODULATE vs COMBINE (GL defaults make the
     *    COMBINE state equal to MODULATE, like RenderLivingBase.unsetBrightness), then one client-array quad */
    glVertexPointer(3, GL_FLOAT, 24, &q[0].x); glTexCoordPointer(2, GL_FLOAT, 24, &q[0].u); glColorPointer(4, GL_UNSIGNED_BYTE, 24, q[0].c);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY); glEnable(GL_TEXTURE_2D);
    const int SD = 2000;
    for (int pass = 0; pass < 2; ++pass) {                         /* pass 0 builds every program, pass 1 is timed */
        glFinish(); dstart(); t = now();
        for (int f = 0; f < (pass ? FR : 1); ++f) {
            for (int i = 0; i < SD; ++i) {
                if (i & 1) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
                if (i & 2) glEnable(GL_FOG); else glDisable(GL_FOG);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (i & 4) ? GL_COMBINE : GL_MODULATE);
                glDrawArrays(GL_QUADS, 0, 4);
            }
            glFinish();
        }
    }
    printf("state churn (lighting/fog/texenv toggled per draw): %.1f ns/draw | ES draws/frame: %.0f\n", (now() - t) / (FR * SD), dget() / (double)FR);
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); glDisable(GL_TEXTURE_2D);
    printf("GL error at end: 0x%x\n", glGetError());
    return 0;
}
