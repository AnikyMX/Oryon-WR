#!/usr/bin/env python3
# tools/test_indexed.py -- indexed draw paths on real Mesa EGL + GLES 3.2, after Oryon started passing index ranges
# (glDrawRangeElements[BaseVertex]) whenever it knows them. Each case draws a green quad whose indices address
# vertices 10..13 of a 20-vertex array whose other vertices are red and cover the whole cell, so a wrong range or base
# vertex shows as red or missing pixels: client arrays + client indices, client arrays + element buffer, VBO arrays +
# element buffer, glDrawRangeElements with VBO arrays, GL_QUADS conversion from client indices and from an element
# buffer, glMultiDrawElements, and the application-program path (glDrawRangeElements, glDrawArrays(GL_QUADS)).
import os, sys, struct, ctypes
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gltest import Ctx, E, P
W = 96
C = Ctx(W, W); gl = C.gl; e = E
fails = []
def check(ok, msg):
    if not ok: fails.append(msg)
CELLS = [(cx, cy) for cy in range(3) for cx in range(3)]
def cell_vertices(ci):
    # 20 vertices: 0..9 red, covering the cell; 10..13 green inner quad; 14..19 red
    cx, cy = CELLS[ci]; x0 = -1 + cx * 2 / 3; y0 = -1 + cy * 2 / 3; s = 2 / 3
    red = (255, 0, 0, 255); grn = (0, 255, 0, 255)
    full = [(x0, y0), (x0 + s, y0), (x0 + s, y0 + s), (x0, y0 + s)]
    inner = [(x0 + s * .25, y0 + s * .25), (x0 + s * .75, y0 + s * .25), (x0 + s * .75, y0 + s * .75), (x0 + s * .25, y0 + s * .75)]
    vs = [(full[i % 4], red) for i in range(10)] + [(p, grn) for p in inner] + [(full[i % 4], red) for i in range(6)]
    return b''.join(struct.pack('<2f4B', p[0], p[1], *c) for p, c in vs)
TRI = [10, 11, 12, 10, 12, 13]; QUAD = [10, 11, 12, 13]
def u16(lst): return (ctypes.c_uint16 * len(lst))(*lst)
def buf(target, data):
    b = ctypes.c_uint32(0); gl.glGenBuffers(1, P(ctypes.pointer(b))); gl.glBindBuffer(target, b.value)
    cb = ctypes.create_string_buffer(bytes(data), len(bytes(data)))
    gl.glBufferData(target, len(bytes(data)), P(cb), e['GL_STATIC_DRAW']); return b.value
gl.glClearColor(0, 0, 0, 1); gl.glClear(e['GL_COLOR_BUFFER_BIT'])
gl.glMatrixMode(e['GL_PROJECTION']); gl.glLoadIdentity(); gl.glMatrixMode(e['GL_MODELVIEW']); gl.glLoadIdentity()
gl.glEnableClientState(e['GL_VERTEX_ARRAY']); gl.glEnableClientState(e['GL_COLOR_ARRAY'])
AB, EB = e['GL_ARRAY_BUFFER'], e['GL_ELEMENT_ARRAY_BUFFER']
keep = []
def client_arrays(ci):
    vd = ctypes.create_string_buffer(cell_vertices(ci)); keep.append(vd)
    gl.glBindBuffer(AB, 0)
    gl.glVertexPointer(2, e['GL_FLOAT'], 12, P(vd)); gl.glColorPointer(4, e['GL_UNSIGNED_BYTE'], 12, P(ctypes.addressof(vd) + 8))
def vbo_arrays(ci):
    buf(AB, cell_vertices(ci))
    gl.glVertexPointer(2, e['GL_FLOAT'], 12, P(0)); gl.glColorPointer(4, e['GL_UNSIGNED_BYTE'], 12, P(8))
cases = []
# 0: client arrays + client indices (triangles)
client_arrays(0); ix = u16(TRI); keep.append(ix); gl.glBindBuffer(EB, 0)
gl.glDrawElements(e['GL_TRIANGLES'], 6, e['GL_UNSIGNED_SHORT'], P(ix)); cases.append('client+client')
# 1: client arrays + element buffer
client_arrays(1); buf(EB, u16(TRI)); gl.glDrawElements(e['GL_TRIANGLES'], 6, e['GL_UNSIGNED_SHORT'], P(0)); cases.append('client+ebo')
# 2: VBO arrays + element buffer
vbo_arrays(2); buf(EB, u16(TRI)); gl.glDrawElements(e['GL_TRIANGLES'], 6, e['GL_UNSIGNED_SHORT'], P(0)); cases.append('vbo+ebo')
# 3: glDrawRangeElements, VBO arrays + client indices
vbo_arrays(3); gl.glBindBuffer(EB, 0); ix = u16(TRI); keep.append(ix)
gl.glDrawRangeElements(e['GL_TRIANGLES'], 10, 13, 6, e['GL_UNSIGNED_SHORT'], P(ix)); cases.append('range vbo+client')
# 4: GL_QUADS from client indices (converted)
client_arrays(4); gl.glBindBuffer(EB, 0); ix = u16(QUAD); keep.append(ix)
gl.glDrawElements(e['GL_QUADS'], 4, e['GL_UNSIGNED_SHORT'], P(ix)); cases.append('quads client')
# 5: GL_QUADS from an element buffer (converted), VBO arrays
vbo_arrays(5); buf(EB, u16(QUAD)); gl.glDrawElements(e['GL_QUADS'], 4, e['GL_UNSIGNED_SHORT'], P(0)); cases.append('quads vbo+ebo')
# 6: glMultiDrawElements, client arrays, two halves of the quad as separate draws
client_arrays(6); gl.glBindBuffer(EB, 0)
a, b = u16(TRI[:3]), u16(TRI[3:]); keep += [a, b]
cnt = (ctypes.c_int32 * 2)(3, 3); ptrs = (ctypes.c_void_p * 2)(ctypes.addressof(a), ctypes.addressof(b))
gl.glMultiDrawElements(e['GL_TRIANGLES'], P(cnt), e['GL_UNSIGNED_SHORT'], P(ptrs), 2); cases.append('multidraw')
gl.glDisableClientState(e['GL_VERTEX_ARRAY']); gl.glDisableClientState(e['GL_COLOR_ARRAY'])
gl.glBindBuffer(EB, 0); gl.glBindBuffer(AB, 0)
# 7/8: application program path
def shader(kind, srcs):
    s = gl.glCreateShader(e[kind]); b = ctypes.create_string_buffer(srcs.encode())
    pp = ctypes.c_char_p(ctypes.addressof(b)); gl.glShaderSource(s, 1, P(ctypes.pointer(pp)), None); gl.glCompileShader(s); return s
prog = gl.glCreateProgram()
gl.glAttachShader(prog, shader('GL_VERTEX_SHADER', 'attribute vec2 pos; attribute vec4 col; varying vec4 c; void main() { c = col; gl_Position = vec4(pos, 0.0, 1.0); }'))
gl.glAttachShader(prog, shader('GL_FRAGMENT_SHADER', 'varying vec4 c; void main() { gl_FragColor = c; }'))
gl.glBindAttribLocation(prog, 0, b'pos'); gl.glBindAttribLocation(prog, 1, b'col'); gl.glLinkProgram(prog); gl.glUseProgram(prog)
gl.glEnableVertexAttribArray(0); gl.glEnableVertexAttribArray(1)
buf(AB, cell_vertices(7))
gl.glVertexAttribPointer(0, 2, e['GL_FLOAT'], 0, 12, P(0)); gl.glVertexAttribPointer(1, 4, e['GL_UNSIGNED_BYTE'], 1, 12, P(8))
buf(EB, u16(TRI)); gl.glDrawRangeElements(e['GL_TRIANGLES'], 10, 13, 6, e['GL_UNSIGNED_SHORT'], P(0)); cases.append('program range')
vd = cell_vertices(8); inner = vd[10 * 12: 14 * 12]                  # quad vertices only: glDrawArrays(GL_QUADS, first=2)
buf(AB, vd[:2 * 12] + inner)
gl.glVertexAttribPointer(0, 2, e['GL_FLOAT'], 0, 12, P(0)); gl.glVertexAttribPointer(1, 4, e['GL_UNSIGNED_BYTE'], 1, 12, P(8))
gl.glBindBuffer(EB, 0); gl.glDrawArrays(e['GL_QUADS'], 2, 4); cases.append('program quads')
gl.glUseProgram(0); gl.glDisableVertexAttribArray(0); gl.glDisableVertexAttribArray(1)
err = gl.glGetError()
res = []
for ci, name in enumerate(cases):
    cx, cy = CELLS[ci]; px0 = int((cx + 0.5) * W / 3); py0 = int((cy + 0.5) * W / 3)
    centre = C.px(px0, py0); edge = C.px(int((cx + 0.08) * W / 3), int((cy + 0.08) * W / 3))
    ok = tuple(centre) == (0, 255, 0, 255) and tuple(edge) == (0, 0, 0, 255)
    res.append('%s %s' % (name, 'ok' if ok else 'FAIL %s %s' % (centre, edge)))
    check(ok, '%s: centre %s edge %s (expected green centre, black edge)' % (name, centre, edge))
check(err == 0, 'GL error 0x%x' % err)
print('== INDEXED DRAW PATHS TEST ==')
print(' | '.join(res))
print('RESULT:', 'PASS' if not fails else 'FAIL')
for f in fails: print('  -', f)
sys.exit(1 if fails else 0)
