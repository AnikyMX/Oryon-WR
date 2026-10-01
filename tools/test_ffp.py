#!/usr/bin/env python3
# tools/test_ffp.py -- fixed-function rendering tests on real Mesa GLES 3.2 (llvmpipe), driven through
# liboryon.so exactly like LWJGL (dlopen + dlsym). Scenarios mirror 1.12.2 call-site patterns.
import os, sys, ctypes, struct, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gltest import *

C = Ctx(64, 64); gl = C.gl; e = E
res = []
def check(name, ok, detail=''):
    res.append((name, ok, detail))
def geti(pn):
    v = ctypes.c_int32(0); gl.glGetIntegerv(e[pn], P(ctypes.pointer(v))); return v.value
def reset():
    gl.glViewport(0, 0, 64, 64)
    gl.glMatrixMode(e['GL_PROJECTION']); gl.glLoadIdentity(); gl.glOrtho(0, 64, 0, 64, -100, 100)
    gl.glMatrixMode(e['GL_MODELVIEW']); gl.glLoadIdentity()
    gl.glClearColor(0, 0, 0, 1); gl.glClear(e['GL_COLOR_BUFFER_BIT'] | e['GL_DEPTH_BUFFER_BIT'])
    for cap in ('GL_TEXTURE_2D', 'GL_ALPHA_TEST', 'GL_LIGHTING', 'GL_FOG', 'GL_BLEND', 'GL_DEPTH_TEST', 'GL_COLOR_MATERIAL', 'GL_CULL_FACE'):
        gl.glDisable(e[cap])
    gl.glColor4f(1, 1, 1, 1)
def quad(x0, y0, x1, y1, z=0.0):
    gl.glBegin(e['GL_QUADS'])
    gl.glVertex3f(x0, y0, z); gl.glVertex3f(x1, y0, z); gl.glVertex3f(x1, y1, z); gl.glVertex3f(x0, y1, z)
    gl.glEnd()
def mktex(w, h, ints, fmt='GL_BGRA', typ='GL_UNSIGNED_INT_8_8_8_8_REV', internal='GL_RGBA'):
    t = ctypes.c_uint32(0); gl.glGenTextures(1, P(ctypes.pointer(t)))
    gl.glBindTexture(e['GL_TEXTURE_2D'], t.value)
    for pn, v in (('GL_TEXTURE_MIN_FILTER', 'GL_NEAREST'), ('GL_TEXTURE_MAG_FILTER', 'GL_NEAREST'),
                  ('GL_TEXTURE_WRAP_S', 'GL_CLAMP'), ('GL_TEXTURE_WRAP_T', 'GL_CLAMP')):
        gl.glTexParameteri(e['GL_TEXTURE_2D'], e[pn], e[v])
    data = (ctypes.c_uint32 * len(ints))(*ints)
    gl.glTexImage2D(e['GL_TEXTURE_2D'], 0, e[internal], w, h, 0, e[fmt], e[typ], P(data))
    return t.value

# ---- T1 immediate quad
reset(); gl.glColor4f(1, 0, 0, 1); quad(8, 8, 56, 56)
check('T1 immediate GL_QUADS', close(C.px(32, 32), (255, 0, 0, 255)) and close(C.px(2, 2), (0, 0, 0, 255)), (C.px(32, 32), C.px(2, 2)))

# ---- T2 BGRA/8888_REV texture + alpha test + batched TRIANGLE_STRIP chars (FontRenderer pattern)
reset()
tex = mktex(2, 2, [0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0x00FFFFFF])
gl.glEnable(e['GL_TEXTURE_2D']); gl.glEnable(e['GL_ALPHA_TEST']); gl.glAlphaFunc(e['GL_GREATER'], 0.1)
for (x0, y0, s0, t0) in [(0, 0, 0, 0), (32, 0, 0.5, 0), (0, 32, 0, 0.5), (32, 32, 0.5, 0.5)]:
    gl.glBegin(e['GL_TRIANGLE_STRIP'])
    gl.glTexCoord2f(s0, t0); gl.glVertex3f(x0, y0, 0)
    gl.glTexCoord2f(s0, t0 + 0.5); gl.glVertex3f(x0, y0 + 32, 0)
    gl.glTexCoord2f(s0 + 0.5, t0); gl.glVertex3f(x0 + 32, y0, 0)
    gl.glTexCoord2f(s0 + 0.5, t0 + 0.5); gl.glVertex3f(x0 + 32, y0 + 32, 0)
    gl.glEnd()
p = [C.px(16, 16), C.px(48, 16), C.px(16, 48), C.px(48, 48)]
check('T2 BGRA swizzle + alpha test + strip batching', close(p[0], (255, 0, 0, 255)) and close(p[1], (0, 255, 0, 255)) and
      close(p[2], (0, 0, 255, 255)) and close(p[3], (0, 0, 0, 255)), p)

# ---- T3 client arrays interleaved POSITION_TEX_COLOR (stride 24), GL_QUADS, first offset
reset()
vd = b''
for (x0, x1, col) in [(0, 32, (255, 0, 0, 255)), (32, 64, (0, 0, 255, 255))]:
    for (x, y) in [(x0, 0), (x1, 0), (x1, 64), (x0, 64)]:
        vd += struct.pack('<fffff4B', x, y, 0, 0, 0, *col)
buf = ctypes.create_string_buffer(vd, len(vd)); a = ctypes.addressof(buf)
gl.glVertexPointer(3, e['GL_FLOAT'], 24, a); gl.glTexCoordPointer(2, e['GL_FLOAT'], 24, a + 12); gl.glColorPointer(4, e['GL_UNSIGNED_BYTE'], 24, a + 20)
for s in ('GL_VERTEX_ARRAY', 'GL_COLOR_ARRAY', 'GL_TEXTURE_COORD_ARRAY'): gl.glEnableClientState(e[s])
gl.glDrawArrays(e['GL_QUADS'], 0, 8)
p1 = [C.px(16, 32), C.px(48, 32)]
gl.glClear(e['GL_COLOR_BUFFER_BIT']); gl.glDrawArrays(e['GL_QUADS'], 4, 4)
p2 = [C.px(16, 32), C.px(48, 32)]
# element path: client u16 indices, QUADS converted on CPU
gl.glClear(e['GL_COLOR_BUFFER_BIT'])
idx = (ctypes.c_uint16 * 4)(0, 1, 2, 3)
gl.glDrawElements(e['GL_QUADS'], 4, e['GL_UNSIGNED_SHORT'], P(idx))
p3 = [C.px(16, 32), C.px(48, 32)]
for s in ('GL_VERTEX_ARRAY', 'GL_COLOR_ARRAY', 'GL_TEXTURE_COORD_ARRAY'): gl.glDisableClientState(e[s])
check('T3 client arrays interleaved QUADS (+first, +DrawElements QUADS)',
      close(p1[0], (255, 0, 0, 255)) and close(p1[1], (0, 0, 255, 255)) and close(p2[0], (0, 0, 0, 255)) and close(p2[1], (0, 0, 255, 255))
      and close(p3[0], (255, 0, 0, 255)) and close(p3[1], (0, 0, 0, 255)), (p1, p2, p3))

# ---- T4 chunk VBO POSITION_TEX_LMAP_COLOR (stride 28) + lightmap unit 1 with texture matrix
reset()
t0 = mktex(1, 1, [0xFFC86432])                                   # ARGB: R=200 G=100 B=50
lm = [0xFF000000] * 256; lm[15 * 16 + 15] = 0xFF808080          # lightmap texel (15,15) = gray 128
gl.glActiveTexture(e['GL_TEXTURE1']); t1 = mktex(16, 16, lm); gl.glEnable(e['GL_TEXTURE_2D'])
gl.glMatrixMode(e['GL_TEXTURE']); gl.glLoadIdentity(); s = 1 / 256.0; gl.glScalef(s, s, s); gl.glTranslatef(8, 8, 8); gl.glMatrixMode(e['GL_MODELVIEW'])
gl.glActiveTexture(e['GL_TEXTURE0']); gl.glBindTexture(e['GL_TEXTURE_2D'], t0); gl.glEnable(e['GL_TEXTURE_2D'])
vd = b''.join(struct.pack('<fffffhh4B', x, y, 0, 0.5, 0.5, 240, 240, 255, 255, 255, 255) for (x, y) in [(0, 0), (64, 0), (64, 64), (0, 64)])
vbo = ctypes.c_uint32(0); gl.glGenBuffers(1, P(ctypes.pointer(vbo)))
gl.glBindBuffer(e['GL_ARRAY_BUFFER'], vbo.value); gl.glBufferData(e['GL_ARRAY_BUFFER'], len(vd), vd, e['GL_STATIC_DRAW'])
gl.glVertexPointer(3, e['GL_FLOAT'], 28, 0); gl.glTexCoordPointer(2, e['GL_FLOAT'], 28, 12); gl.glColorPointer(4, e['GL_UNSIGNED_BYTE'], 28, 24)
gl.glClientActiveTexture(e['GL_TEXTURE1']); gl.glTexCoordPointer(2, e['GL_SHORT'], 28, 20); gl.glEnableClientState(e['GL_TEXTURE_COORD_ARRAY'])
gl.glClientActiveTexture(e['GL_TEXTURE0'])
for s_ in ('GL_VERTEX_ARRAY', 'GL_COLOR_ARRAY', 'GL_TEXTURE_COORD_ARRAY'): gl.glEnableClientState(e[s_])
gl.glDrawArrays(e['GL_QUADS'], 0, 4)
p = C.px(32, 32)
check('T4 chunk VBO + lightmap unit1 (short coords, texture matrix)', close(p, (100, 50, 25, 255), 2), p)
gl.glClientActiveTexture(e['GL_TEXTURE1']); gl.glDisableClientState(e['GL_TEXTURE_COORD_ARRAY']); gl.glClientActiveTexture(e['GL_TEXTURE0'])
for s_ in ('GL_VERTEX_ARRAY', 'GL_COLOR_ARRAY', 'GL_TEXTURE_COORD_ARRAY'): gl.glDisableClientState(e[s_])
gl.glBindBuffer(e['GL_ARRAY_BUFFER'], 0)
gl.glActiveTexture(e['GL_TEXTURE1']); gl.glDisable(e['GL_TEXTURE_2D']); gl.glMatrixMode(e['GL_TEXTURE']); gl.glLoadIdentity(); gl.glMatrixMode(e['GL_MODELVIEW'])
gl.glActiveTexture(e['GL_TEXTURE0'])

# ---- T5 lighting (RenderHelper-style: LIGHT0 directional, color material AMBIENT_AND_DIFFUSE)
reset()
gl.glEnable(e['GL_LIGHTING']); gl.glEnable(e['GL_LIGHT0']); gl.glEnable(e['GL_COLOR_MATERIAL'])
gl.glColorMaterial(e['GL_FRONT_AND_BACK'], e['GL_AMBIENT_AND_DIFFUSE'])
gl.glLightfv(e['GL_LIGHT0'], e['GL_POSITION'], P(farr(0, 0, 1, 0)))
gl.glLightfv(e['GL_LIGHT0'], e['GL_DIFFUSE'], P(farr(0.6, 0.6, 0.6, 1)))
gl.glLightfv(e['GL_LIGHT0'], e['GL_AMBIENT'], P(farr(0, 0, 0, 1)))
gl.glLightfv(e['GL_LIGHT0'], e['GL_SPECULAR'], P(farr(0, 0, 0, 1)))
gl.glLightModelfv(e['GL_LIGHT_MODEL_AMBIENT'], P(farr(0.4, 0.4, 0.4, 1)))
gl.glShadeModel(e['GL_FLAT'])
gl.glColor4f(1, 0.5, 0.25, 1); gl.glNormal3f(0, 0.6, 0.8); quad(0, 0, 64, 64)
pa = C.px(32, 32)
# light positioned under a rotation (eye-space transform at glLight time)
gl.glClear(e['GL_COLOR_BUFFER_BIT'])
gl.glPushMatrix(); gl.glRotatef(90, 1, 0, 0); gl.glLightfv(e['GL_LIGHT0'], e['GL_POSITION'], P(farr(0, 0, 1, 0))); gl.glPopMatrix()
gl.glNormal3f(0, -1, 0); quad(0, 0, 64, 64)
pb = C.px(32, 32)
gl.glShadeModel(e['GL_SMOOTH'])
check('T5 lighting + color material (+eye-space light position)', close(pa, (224, 112, 56, 255)) and close(pb, (255, 128, 64, 255)), (pa, pb))
gl.glDisable(e['GL_LIGHTING']); gl.glDisable(e['GL_LIGHT0']); gl.glDisable(e['GL_COLOR_MATERIAL'])

# ---- T6 matrix stack queries (ActiveRenderInfo / ClippingHelper read MODELVIEW/PROJECTION)
reset()
gl.glTranslatef(1, 2, 3); gl.glRotatef(30, 0, 1, 0); gl.glScalef(2, 2, 2)
m = (ctypes.c_float * 16)(); gl.glGetFloatv(e['GL_MODELVIEW_MATRIX'], P(m))
ref = mmul(mmul(trans(1, 2, 3), rot(30, 0, 1, 0)), scale(2, 2, 2))
ok_mv = all(abs(m[i] - ref[i]) < 1e-5 for i in range(16))
gl.glPushMatrix(); d2 = geti('GL_MODELVIEW_STACK_DEPTH'); gl.glPopMatrix(); d1 = geti('GL_MODELVIEW_STACK_DEPTH')
pm = (ctypes.c_float * 16)(); gl.glGetFloatv(e['GL_PROJECTION_MATRIX'], P(pm))
ok_p = abs(pm[0] - 2 / 64) < 1e-6 and abs(pm[12] + 1) < 1e-6 and abs(pm[10] + 2 / 200) < 1e-6
check('T6 glGetFloatv MODELVIEW/PROJECTION + stack depth', ok_mv and ok_p and d2 == 2 and d1 == 1, (ok_mv, ok_p, d2, d1))

# ---- T7 texenv COMBINE (EntityRenderer/RenderLivingBase hurt-overlay style)
reset()
w0 = mktex(1, 1, [0xFFFFFFFF]); gl.glEnable(e['GL_TEXTURE_2D'])
TE = e['GL_TEXTURE_ENV']
def env(pn, v): gl.glTexEnvi(TE, e[pn], e[v])
env('GL_TEXTURE_ENV_MODE', 'GL_COMBINE'); env('GL_COMBINE_RGB', 'GL_MODULATE'); env('GL_SOURCE0_RGB', 'GL_TEXTURE'); env('GL_SOURCE1_RGB', 'GL_PRIMARY_COLOR')
env('GL_OPERAND0_RGB', 'GL_SRC_COLOR'); env('GL_OPERAND1_RGB', 'GL_SRC_COLOR'); env('GL_COMBINE_ALPHA', 'GL_REPLACE'); env('GL_SOURCE0_ALPHA', 'GL_TEXTURE'); env('GL_OPERAND0_ALPHA', 'GL_SRC_ALPHA')
gl.glActiveTexture(e['GL_TEXTURE1']); w1 = mktex(1, 1, [0xFFFFFFFF]); gl.glEnable(e['GL_TEXTURE_2D'])
env('GL_TEXTURE_ENV_MODE', 'GL_COMBINE'); env('GL_COMBINE_RGB', 'GL_INTERPOLATE'); env('GL_SOURCE0_RGB', 'GL_CONSTANT'); env('GL_SOURCE1_RGB', 'GL_PREVIOUS')
env('GL_SOURCE2_RGB', 'GL_CONSTANT'); env('GL_OPERAND0_RGB', 'GL_SRC_COLOR'); env('GL_OPERAND1_RGB', 'GL_SRC_COLOR'); env('GL_OPERAND2_RGB', 'GL_SRC_ALPHA')
env('GL_COMBINE_ALPHA', 'GL_REPLACE'); env('GL_SOURCE0_ALPHA', 'GL_PREVIOUS'); env('GL_OPERAND0_ALPHA', 'GL_SRC_ALPHA')
gl.glTexEnvfv(TE, e['GL_TEXTURE_ENV_COLOR'], P(farr(1, 0, 0, 0.3)))
gl.glActiveTexture(e['GL_TEXTURE0'])
gl.glColor4f(0, 0.5, 1, 1)
gl.glBegin(e['GL_QUADS'])
for (x, y) in [(0, 0), (64, 0), (64, 64), (0, 64)]:
    gl.glTexCoord2f(0.5, 0.5); gl.glMultiTexCoord2f(e['GL_TEXTURE1'], 0.5, 0.5); gl.glVertex2f(x, y)
gl.glEnd()
p = C.px(32, 32)
check('T7 texenv COMBINE (MODULATE -> INTERPOLATE CONSTANT)', close(p, (77, 89, 179, 255), 2), p)
gl.glActiveTexture(e['GL_TEXTURE1']); env('GL_TEXTURE_ENV_MODE', 'GL_MODULATE'); gl.glDisable(e['GL_TEXTURE_2D'])
gl.glActiveTexture(e['GL_TEXTURE0']); env('GL_TEXTURE_ENV_MODE', 'GL_MODULATE')

# ---- T8 glPushAttrib(GL_ENABLE_BIT|GL_LIGHTING_BIT) = 8256 as used by 1.12.2
reset()
for c in ('GL_LIGHTING', 'GL_FOG', 'GL_ALPHA_TEST', 'GL_BLEND'): gl.glEnable(e[c])
gl.glLightfv(e['GL_LIGHT0'], e['GL_DIFFUSE'], P(farr(0.1, 0.2, 0.3, 1)))
gl.glPushAttrib(8256)
for c in ('GL_LIGHTING', 'GL_FOG', 'GL_ALPHA_TEST', 'GL_BLEND'): gl.glDisable(e[c])
gl.glLightfv(e['GL_LIGHT0'], e['GL_DIFFUSE'], P(farr(1, 1, 1, 1)))
gl.glPopAttrib()
en = [gl.glIsEnabled(e[c]) for c in ('GL_LIGHTING', 'GL_FOG', 'GL_ALPHA_TEST', 'GL_BLEND')]
d = (ctypes.c_float * 4)(); gl.glGetLightfv(e['GL_LIGHT0'], e['GL_DIFFUSE'], P(d))
check('T8 glPushAttrib/glPopAttrib (ENABLE|LIGHTING)', en == [1, 1, 1, 1] and close([x * 100 for x in d], [10, 20, 30, 100], 0.01), (en, list(d)))

# ---- T9 readback: glGetTexImage BGRA/8888_REV round trip + glReadPixels GL_BGRA
reset()
src = [0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0x80FFFFFF]
tr = mktex(2, 2, src)
out = (ctypes.c_uint32 * 4)()
gl.glGetTexImage(e['GL_TEXTURE_2D'], 0, e['GL_BGRA'], e['GL_UNSIGNED_INT_8_8_8_8_REV'], P(out))
gl.glColor4f(1, 0, 0, 1); quad(0, 0, 64, 64)
b = (ctypes.c_ubyte * 4)(); gl.glReadPixels(5, 5, 1, 1, e['GL_BGRA'], e['GL_UNSIGNED_BYTE'], P(b))
check('T9 glGetTexImage BGRA round-trip + glReadPixels BGRA', list(out) == src and tuple(b) == (0, 0, 255, 255), ([hex(x) for x in out], tuple(b)))

# ---- T10 proxy texture size probe (Minecraft.getGLMaximumTextureSize pattern)
got = -1; i = 16384
while i > 0:
    gl.glTexImage2D(e['GL_PROXY_TEXTURE_2D'], 0, e['GL_RGBA'], i, i, 0, e['GL_RGBA'], e['GL_UNSIGNED_BYTE'], None)
    w = ctypes.c_int32(0); gl.glGetTexLevelParameteriv(e['GL_PROXY_TEXTURE_2D'], 0, e['GL_TEXTURE_WIDTH'], P(ctypes.pointer(w)))
    if w.value != 0: got = i; break
    i >>= 1
check('T10 GL_PROXY_TEXTURE_2D max size probe', got >= 2048, got)

# ---- T11 fog LINEAR / EXP / NV radial
reset()
gl.glEnable(e['GL_FOG']); gl.glFogi(e['GL_FOG_MODE'], e['GL_LINEAR']); gl.glFogf(e['GL_FOG_START'], 0); gl.glFogf(e['GL_FOG_END'], 10)
gl.glFogfv(e['GL_FOG_COLOR'], P(farr(0, 0, 1, 1)))
gl.glTranslatef(0, 0, -5); gl.glColor4f(1, 0, 0, 1); quad(0, 0, 64, 64)
fl = C.px(32, 32)
gl.glFogi(e['GL_FOG_MODE'], e['GL_EXP']); gl.glFogf(e['GL_FOG_DENSITY'], 0.2); quad(0, 0, 64, 64)
fe = C.px(32, 32)
gl.glFogi(e['GL_FOG_MODE'], e['GL_LINEAR']); gl.glFogi(e['GL_FOG_DISTANCE_MODE_NV'], e['GL_EYE_RADIAL_NV']); quad(0, 0, 64, 64)
fr = C.px(32, 32)
gl.glFogi(e['GL_FOG_DISTANCE_MODE_NV'], e['GL_EYE_PLANE_ABSOLUTE_NV'])
f = math.exp(-1.0)
check('T11 fog LINEAR/EXP/NV radial', close(fl, (128, 0, 128, 255)) and close(fe, (round(255 * f), 0, round(255 * (1 - f)), 255)) and close(fr, (0, 0, 255, 255)), (fl, fe, fr))
gl.glDisable(e['GL_FOG'])

# ---- T12 batching stress + ordering with interleaved state changes
reset()
gl.glColor4f(0, 1, 0, 1)
for k in range(64):
    x = (k % 8) * 8; y = (k // 8) * 8
    quad(x, y, x + 4, y + 4)
gl.glColor4f(1, 1, 0, 1); quad(60, 60, 64, 64)               # constant change -> new batch
ok = close(C.px(1, 1), (0, 255, 0, 255)) and close(C.px(6, 6), (0, 0, 0, 255)) and close(C.px(57, 57), (0, 255, 0, 255)) and close(C.px(62, 62), (255, 255, 0, 255))
check('T12 immediate batching (65 primitives) + constant-change split', ok, (C.px(1, 1), C.px(6, 6), C.px(62, 62)))

# ---- T14 fog EXP2 + LINEAR/EXP2/EXP switches reuse one program (equation selected by uniform)
import glob
def nprog(): return len(glob.glob(os.path.join(os.environ['ORYON_CACHE_DIR'], 'ffp-*.bin')))
reset()
gl.glEnable(e['GL_FOG']); gl.glFogfv(e['GL_FOG_COLOR'], P(farr(0, 0, 1, 1))); gl.glFogf(e['GL_FOG_DENSITY'], 0.3)
gl.glFogf(e['GL_FOG_START'], 0); gl.glFogf(e['GL_FOG_END'], 10)
gl.glTranslatef(0, 0, -5); gl.glColor4f(1, 0, 0, 1)
gl.glFogi(e['GL_FOG_MODE'], e['GL_LINEAR']); quad(0, 0, 64, 64); n0 = nprog(); fa = C.px(32, 32)
gl.glFogi(e['GL_FOG_MODE'], e['GL_EXP2']); quad(0, 0, 64, 64); fb = C.px(32, 32)
gl.glFogi(e['GL_FOG_MODE'], e['GL_EXP']); quad(0, 0, 64, 64); fc = C.px(32, 32); n1 = nprog()
x2, x1 = math.exp(-(0.3 * 5) ** 2), math.exp(-0.3 * 5)
check('T14 fog EXP2 + mode switches without a new program',
      close(fa, (128, 0, 128, 255)) and close(fb, (round(255 * x2), 0, round(255 * (1 - x2)), 255)) and
      close(fc, (round(255 * x1), 0, round(255 * (1 - x1)), 255)) and n1 == n0, (fa, fb, fc, n0, n1))
gl.glFogi(e['GL_FOG_MODE'], e['GL_LINEAR']); gl.glDisable(e['GL_FOG'])

# ---- T15 RenderLivingBase.setBrightness / unsetBrightness texenv sequences as recovered from the 1.12.2 bytecode
#      (units 0, 1 = lightmap, 2 = brightness texture). After unset, units 0/1 stay in a COMBINE state equal to
#      MODULATE: same pixels and no new program compared with plain MODULATE.
reset()
gl.glActiveTexture(e['GL_TEXTURE0']); mktex(1, 1, [0xFF80C040]); gl.glEnable(e['GL_TEXTURE_2D'])
gl.glActiveTexture(e['GL_TEXTURE1']); mktex(1, 1, [0xFFC0C0C0]); gl.glEnable(e['GL_TEXTURE_2D'])
gl.glActiveTexture(e['GL_TEXTURE0'])
def draw_lm():
    gl.glColor4f(1, 1, 1, 1)
    gl.glBegin(e['GL_QUADS'])
    for (x, y) in [(0, 0), (64, 0), (64, 64), (0, 64)]:
        gl.glTexCoord2f(0.5, 0.5); gl.glMultiTexCoord2f(e['GL_TEXTURE1'], 0.5, 0.5); gl.glVertex2f(x, y)
    gl.glEnd()
draw_lm(); ref = C.px(32, 32); n0 = nprog()
def seq(*kv):
    for k, v in zip(kv[0::2], kv[1::2]): env(k, v)
gl.glActiveTexture(e['GL_TEXTURE0']); gl.glEnable(e['GL_TEXTURE_2D'])
seq('GL_TEXTURE_ENV_MODE', 'GL_COMBINE', 'GL_COMBINE_RGB', 'GL_MODULATE', 'GL_SOURCE0_RGB', 'GL_TEXTURE0', 'GL_SOURCE1_RGB', 'GL_PRIMARY_COLOR',
    'GL_OPERAND0_RGB', 'GL_SRC_COLOR', 'GL_OPERAND1_RGB', 'GL_SRC_COLOR', 'GL_COMBINE_ALPHA', 'GL_REPLACE', 'GL_SOURCE0_ALPHA', 'GL_TEXTURE0',
    'GL_OPERAND0_ALPHA', 'GL_SRC_ALPHA')
gl.glActiveTexture(e['GL_TEXTURE1']); gl.glEnable(e['GL_TEXTURE_2D'])
seq('GL_TEXTURE_ENV_MODE', 'GL_COMBINE', 'GL_COMBINE_RGB', 'GL_INTERPOLATE', 'GL_SOURCE0_RGB', 'GL_CONSTANT', 'GL_SOURCE1_RGB', 'GL_PREVIOUS',
    'GL_SOURCE2_RGB', 'GL_CONSTANT', 'GL_OPERAND0_RGB', 'GL_SRC_COLOR', 'GL_OPERAND1_RGB', 'GL_SRC_COLOR', 'GL_OPERAND2_RGB', 'GL_SRC_ALPHA',
    'GL_COMBINE_ALPHA', 'GL_REPLACE', 'GL_SOURCE0_ALPHA', 'GL_PREVIOUS', 'GL_OPERAND0_ALPHA', 'GL_SRC_ALPHA')
gl.glTexEnvfv(TE, e['GL_TEXTURE_ENV_COLOR'], P(farr(1, 0, 0, 0.3)))
gl.glActiveTexture(e['GL_TEXTURE2']); mktex(1, 1, [0xFFFFFFFF]); gl.glEnable(e['GL_TEXTURE_2D'])
seq('GL_TEXTURE_ENV_MODE', 'GL_COMBINE', 'GL_COMBINE_RGB', 'GL_MODULATE', 'GL_SOURCE0_RGB', 'GL_PREVIOUS', 'GL_SOURCE1_RGB', 'GL_TEXTURE1',
    'GL_OPERAND0_RGB', 'GL_SRC_COLOR', 'GL_OPERAND1_RGB', 'GL_SRC_COLOR', 'GL_COMBINE_ALPHA', 'GL_REPLACE', 'GL_SOURCE0_ALPHA', 'GL_PREVIOUS',
    'GL_OPERAND0_ALPHA', 'GL_SRC_ALPHA')
gl.glActiveTexture(e['GL_TEXTURE0'])
draw_lm(); hurt = C.px(32, 32); n_hurt = nprog()
gl.glActiveTexture(e['GL_TEXTURE0']); gl.glEnable(e['GL_TEXTURE_2D'])
seq('GL_TEXTURE_ENV_MODE', 'GL_COMBINE', 'GL_COMBINE_RGB', 'GL_MODULATE', 'GL_SOURCE0_RGB', 'GL_TEXTURE0', 'GL_SOURCE1_RGB', 'GL_PRIMARY_COLOR',
    'GL_OPERAND0_RGB', 'GL_SRC_COLOR', 'GL_OPERAND1_RGB', 'GL_SRC_COLOR', 'GL_COMBINE_ALPHA', 'GL_MODULATE', 'GL_SOURCE0_ALPHA', 'GL_TEXTURE0',
    'GL_SOURCE1_ALPHA', 'GL_PRIMARY_COLOR', 'GL_OPERAND0_ALPHA', 'GL_SRC_ALPHA', 'GL_OPERAND1_ALPHA', 'GL_SRC_ALPHA')
gl.glActiveTexture(e['GL_TEXTURE1'])
seq('GL_TEXTURE_ENV_MODE', 'GL_COMBINE', 'GL_COMBINE_RGB', 'GL_MODULATE', 'GL_OPERAND0_RGB', 'GL_SRC_COLOR', 'GL_OPERAND1_RGB', 'GL_SRC_COLOR',
    'GL_SOURCE0_RGB', 'GL_TEXTURE', 'GL_SOURCE1_RGB', 'GL_PREVIOUS', 'GL_COMBINE_ALPHA', 'GL_MODULATE', 'GL_OPERAND0_ALPHA', 'GL_SRC_ALPHA',
    'GL_SOURCE0_ALPHA', 'GL_TEXTURE')
gl.glActiveTexture(e['GL_TEXTURE2']); gl.glDisable(e['GL_TEXTURE_2D'])
seq('GL_TEXTURE_ENV_MODE', 'GL_COMBINE', 'GL_COMBINE_RGB', 'GL_MODULATE', 'GL_OPERAND0_RGB', 'GL_SRC_COLOR', 'GL_OPERAND1_RGB', 'GL_SRC_COLOR',
    'GL_SOURCE0_RGB', 'GL_TEXTURE', 'GL_SOURCE1_RGB', 'GL_PREVIOUS', 'GL_COMBINE_ALPHA', 'GL_MODULATE', 'GL_OPERAND0_ALPHA', 'GL_SRC_ALPHA',
    'GL_SOURCE0_ALPHA', 'GL_TEXTURE')
gl.glActiveTexture(e['GL_TEXTURE0'])
draw_lm(); after = C.px(32, 32); n1 = nprog()
t0c, lm = (128 / 255, 192 / 255, 64 / 255), 192 / 255
exp_hurt = tuple(round(255 * ((0.3 * r + 0.7 * c) * lm)) for r, c in zip((1, 0, 0), t0c)) + (255,)
check('T15 setBrightness/unsetBrightness: hurt tint, then MODULATE-equal COMBINE reuses the MODULATE program',
      close(hurt, exp_hurt, 2) and close(after, ref, 1) and n_hurt == n0 + 1 and n1 == n_hurt, (ref, hurt, exp_hurt, after, n0, n_hurt, n1))
for u in ('GL_TEXTURE2', 'GL_TEXTURE1', 'GL_TEXTURE0'):
    gl.glActiveTexture(e[u]); env('GL_TEXTURE_ENV_MODE', 'GL_MODULATE'); gl.glDisable(e['GL_TEXTURE_2D'])

err = gl.glGetError()
check('T13 no GL error after suite', err == 0, hex(err))
C.close()
fails = [r for r in res if not r[1]]
for n, ok, d in res: print(('PASS ' if ok else 'FAIL ') + n + ('' if ok else '  -> %s' % (d,)))
print('RESULT: %d/%d PASS' % (len(res) - len(fails), len(res)))
sys.exit(1 if fails else 0)
