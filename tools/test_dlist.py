#!/usr/bin/env python3
# tools/test_dlist.py -- display-list tests on Mesa GLES 3.2 through liboryon.so (dlopen/dlsym).
# Patterns: ModelRenderer.compileDisplayList (Tessellator QUADS inside GL_COMPILE), RenderList, nested lists.
import os, sys, ctypes, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gltest import *

C = Ctx(64, 64); gl = C.gl; e = E
res = []
def check(n, ok, d=''): res.append((n, ok, d))
def reset():
    gl.glViewport(0, 0, 64, 64)
    gl.glMatrixMode(e['GL_PROJECTION']); gl.glLoadIdentity(); gl.glOrtho(0, 64, 0, 64, -100, 100)
    gl.glMatrixMode(e['GL_MODELVIEW']); gl.glLoadIdentity()
    gl.glClearColor(0, 0, 0, 1); gl.glClear(e['GL_COLOR_BUFFER_BIT'])
    gl.glColor4f(1, 1, 1, 1)
def ptn_quad_bytes(x0, y0, x1, y1):      # DefaultVertexFormats.POSITION_TEX_NORMAL: 3f pos, 2f uv, 3b normal + pad = 24 bytes
    return b''.join(struct.pack('<fffffbbbx', x, y, 0, 0, 0, 0, 0, 127) for (x, y) in [(x0, y0), (x1, y0), (x1, y1), (x0, y1)])

# ---- DL1: ModelRenderer pattern: two Tessellator draws (client arrays, QUADS) in GL_COMPILE; color/matrix at call time
reset()
lst = gl.glGenLists(1)
gl.glNewList(lst, e['GL_COMPILE'])
for (x0, x1) in [(0, 16), (16, 32)]:
    vd = ptn_quad_bytes(x0, 0, x1, 16)
    buf = ctypes.create_string_buffer(vd, len(vd)); a = ctypes.addressof(buf)
    gl.glVertexPointer(3, e['GL_FLOAT'], 24, a); gl.glTexCoordPointer(2, e['GL_FLOAT'], 24, a + 12); gl.glNormalPointer(e['GL_BYTE'], 24, a + 20)
    for s_ in ('GL_VERTEX_ARRAY', 'GL_TEXTURE_COORD_ARRAY', 'GL_NORMAL_ARRAY'): gl.glEnableClientState(e[s_])
    gl.glDrawArrays(e['GL_QUADS'], 0, 4)
    for s_ in ('GL_VERTEX_ARRAY', 'GL_TEXTURE_COORD_ARRAY', 'GL_NORMAL_ARRAY'): gl.glDisableClientState(e[s_])
    del buf                                            # data must have been dereferenced at compile time
gl.glEndList()
nothing = C.px(8, 8)
gl.glColor4f(1, 0, 0, 1); gl.glCallList(lst)
a1, a2 = C.px(8, 8), C.px(24, 8)
gl.glColor4f(0, 0, 1, 1); gl.glPushMatrix(); gl.glTranslatef(32, 32, 0); gl.glCallList(lst); gl.glPopMatrix()
b1 = C.px(40, 40)
check('DL1 compile-time capture, merged geometry, color+matrix from call time',
      close(nothing, (0, 0, 0, 255)) and close(a1, (255, 0, 0, 255)) and close(a2, (255, 0, 0, 255)) and close(b1, (0, 0, 255, 255)),
      (nothing, a1, a2, b1))

# ---- DL2: immediate block + state commands inside the list (glColor outside begin/end, glTranslatef)
reset()
l2 = gl.glGenLists(1)
gl.glColor4f(1, 1, 1, 1)
gl.glNewList(l2, e['GL_COMPILE'])
gl.glColor4f(0, 1, 0, 1); gl.glTranslatef(32, 0, 0)
gl.glBegin(e['GL_QUADS']); gl.glVertex2f(0, 0); gl.glVertex2f(16, 0); gl.glVertex2f(16, 16); gl.glVertex2f(0, 16); gl.glEnd()
gl.glEndList()
cur = (ctypes.c_float * 4)(); gl.glGetFloatv(e['GL_CURRENT_COLOR'], P(cur)); before = list(cur)
gl.glCallList(l2)
gl.glGetFloatv(e['GL_CURRENT_COLOR'], P(cur)); after = list(cur)
m = (ctypes.c_float * 16)(); gl.glGetFloatv(e['GL_MODELVIEW_MATRIX'], P(m))
check('DL2 recorded glColor/glTranslatef + immediate block (GL_COMPILE leaves state untouched)',
      close(C.px(40, 8), (0, 255, 0, 255)) and close(C.px(8, 8), (0, 0, 0, 255)) and before == [1, 1, 1, 1] and after == [0, 1, 0, 1] and m[12] == 32,
      (C.px(40, 8), before, after, m[12]))

# ---- DL3: nested lists, glCallLists with glListBase + GL_UNSIGNED_BYTE offsets, GL_COMPILE_AND_EXECUTE
reset()
base = gl.glGenLists(3)
for i, col in enumerate([(1, 0, 0), (0, 1, 0), (0, 0, 1)]):
    gl.glNewList(base + i, e['GL_COMPILE'])
    gl.glColor3f(*col); gl.glRectf(i * 16, 0, i * 16 + 16, 16)
    gl.glEndList()
outer = gl.glGenLists(1)
gl.glNewList(outer, e['GL_COMPILE_AND_EXECUTE'])
gl.glTranslatef(0, 32, 0); gl.glCallList(base + 2)
gl.glEndList()
immediate = C.px(40, 40)
gl.glLoadIdentity()
gl.glListBase(base); offs = (ctypes.c_ubyte * 2)(0, 1); gl.glCallLists(2, e['GL_UNSIGNED_BYTE'], P(offs)); gl.glListBase(0)
gl.glCallList(outer)
check('DL3 nested + glCallLists(listBase) + COMPILE_AND_EXECUTE',
      close(immediate, (0, 0, 255, 255)) and close(C.px(8, 8), (255, 0, 0, 255)) and close(C.px(24, 8), (0, 255, 0, 255)) and close(C.px(40, 8), (0, 0, 0, 255))
      and close(C.px(40, 40), (0, 0, 255, 255)), (immediate, C.px(8, 8), C.px(24, 8), C.px(40, 40)))

# ---- DL4: VBO-sourced arrays captured at compile time (RenderList/VBO off pattern), buffer deleted afterwards
reset()
vd = b''.join(struct.pack('<fff4B', x, y, 0, 255, 255, 0, 255) for (x, y) in [(0, 0), (64, 0), (64, 64), (0, 64)])
vbo = ctypes.c_uint32(0); gl.glGenBuffers(1, P(ctypes.pointer(vbo)))
gl.glBindBuffer(e['GL_ARRAY_BUFFER'], vbo.value); gl.glBufferData(e['GL_ARRAY_BUFFER'], len(vd), vd, e['GL_STATIC_DRAW'])
gl.glVertexPointer(3, e['GL_FLOAT'], 16, 0); gl.glColorPointer(4, e['GL_UNSIGNED_BYTE'], 16, 12)
gl.glEnableClientState(e['GL_VERTEX_ARRAY']); gl.glEnableClientState(e['GL_COLOR_ARRAY'])
l4 = gl.glGenLists(1); gl.glNewList(l4, e['GL_COMPILE']); gl.glDrawArrays(e['GL_QUADS'], 0, 4); gl.glEndList()
gl.glDisableClientState(e['GL_VERTEX_ARRAY']); gl.glDisableClientState(e['GL_COLOR_ARRAY'])
gl.glBindBuffer(e['GL_ARRAY_BUFFER'], 0); gl.glDeleteBuffers(1, P(ctypes.pointer(vbo)))
gl.glCallList(l4)
check('DL4 VBO arrays dereferenced at compile time (buffer deleted before call)', close(C.px(32, 32), (255, 255, 0, 255)), C.px(32, 32))

# ---- DL5: pointer-argument recorders (glLightfv/glMaterialfv) + lighting inside a list
reset()
l5 = gl.glGenLists(1)
gl.glNewList(l5, e['GL_COMPILE'])
gl.glEnable(e['GL_LIGHTING']); gl.glEnable(e['GL_LIGHT0'])
gl.glLightfv(e['GL_LIGHT0'], e['GL_POSITION'], P(farr(0, 0, 1, 0)))
gl.glLightfv(e['GL_LIGHT0'], e['GL_DIFFUSE'], P(farr(0.5, 0.5, 0.5, 1)))
gl.glLightfv(e['GL_LIGHT0'], e['GL_AMBIENT'], P(farr(0, 0, 0, 1)))
gl.glLightModelfv(e['GL_LIGHT_MODEL_AMBIENT'], P(farr(0, 0, 0, 1)))
gl.glMaterialfv(e['GL_FRONT_AND_BACK'], e['GL_DIFFUSE'], P(farr(1, 1, 1, 1)))
gl.glNormal3f(0, 0, 1)
gl.glRectf(0, 0, 64, 64)
gl.glDisable(e['GL_LIGHTING'])
gl.glEndList()
lit_during_compile = gl.glIsEnabled(e['GL_LIGHTING'])
gl.glCallList(l5)
check('DL5 glLightfv/glMaterialfv recorded by value, lighting in list', close(C.px(32, 32), (128, 128, 128, 255)) and lit_during_compile == 0, (C.px(32, 32), lit_during_compile))

# ---- DL6: glIsList / glDeleteLists / glGenLists reuse
ok = gl.glIsList(l5) == 1
gl.glDeleteLists(l5, 1)
ok = ok and gl.glIsList(l5) == 0
reset(); gl.glCallList(l5)                              # deleted list: no-op
ok = ok and close(C.px(32, 32), (0, 0, 0, 255))
check('DL6 glIsList/glDeleteLists', ok)
err = gl.glGetError()
check('DL7 no GL error', err == 0, hex(err))
C.close()
fails = [r for r in res if not r[1]]
for n, ok, d in res: print(('PASS ' if ok else 'FAIL ') + n + ('' if ok else '  -> %s' % (d,)))
print('RESULT: %d/%d PASS' % (len(res) - len(fails), len(res)))
sys.exit(1 if fails else 0)
