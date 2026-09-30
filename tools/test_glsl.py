#!/usr/bin/env python3
# tools/test_glsl.py -- desktop GLSL -> ES translation check with the real Mesa GLSL ES compiler:
# every shader/program shipped in 1.12.2.jar is compiled+linked through liboryon.so (glShaderSource path),
# then one post-process program is used exactly like ShaderManager/Shader.render (Position via glVertexPointer).
import os, sys, re, json, zipfile, ctypes, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gltest import *

MC = os.environ.get('ORYON_MC_JAR', '/root/.claude/uploads/af115f22-38a8-5494-9373-115a23ce2354/86ebe279-1.12.2.jar')
z = zipfile.ZipFile(MC)
C = Ctx(64, 64); gl = C.gl; e = E
def shader(stage, src):
    s = gl.glCreateShader(e['GL_VERTEX_SHADER'] if stage == 'v' else e['GL_FRAGMENT_SHADER'])
    b = src.encode(); arr = (ctypes.c_char_p * 1)(b)
    gl.glShaderSource(s, 1, P(arr), None); gl.glCompileShader(s)
    ok = ctypes.c_int32(0); gl.glGetShaderiv(s, e['GL_COMPILE_STATUS'], P(ctypes.pointer(ok)))
    return s, ok.value == 1
def log_of(s):
    buf = ctypes.create_string_buffer(4096); gl.glGetShaderInfoLog(s, 4096, None, P(buf)); return buf.value.decode()
names = sorted(n for n in z.namelist() if re.search(r'shaders/program/.*\.(vsh|fsh)$', n))
comp = {}; fail_cat = {}
for n in names:
    s, ok = shader('v' if n.endswith('.vsh') else 'f', z.read(n).decode('utf-8', 'replace'))
    comp[n.split('/')[-1]] = (s, ok)
    if not ok:
        for ln in log_of(s).splitlines():
            m = re.search(r'error: (.*)', ln)
            if m:
                k = re.sub(r"`[^`']*'|\"[^\"]*\"|'[^']*'", '<x>', m.group(1))[:70]
                fail_cat[k] = fail_cat.get(k, 0) + 1
progs = sorted(n for n in z.namelist() if re.search(r'shaders/program/.*\.json$', n))
linked = 0; progmap = {}
for pn in progs:
    j = json.loads(z.read(pn))
    vs = comp.get(j.get('vertex', '') + '.vsh'); fs = comp.get(j.get('fragment', '') + '.fsh')
    if not vs or not fs or not vs[1] or not fs[1]: continue
    p = gl.glCreateProgram(); gl.glAttachShader(p, vs[0]); gl.glAttachShader(p, fs[0]); gl.glLinkProgram(p)
    ok = ctypes.c_int32(0); gl.glGetProgramiv(p, e['GL_LINK_STATUS'], P(ctypes.pointer(ok)))
    if ok.value: linked += 1; progmap[pn.split('/')[-1][:-5]] = (p, j)
print('MC shaders compiled via liboryon: %d/%d | programs linked: %d/%d' % (sum(1 for v in comp.values() if v[1]), len(comp), linked, len(progs)))
for k, v in sorted(fail_cat.items(), key=lambda x: -x[1])[:8]: print('  %3d x %s' % (v, k))

# ---- render with the 'blit' program like net.minecraft.client.shader.Shader#render
res = []
if 'blit' in progmap:
    p, j = progmap['blit']
    tex = ctypes.c_uint32(0); gl.glGenTextures(1, P(ctypes.pointer(tex))); gl.glBindTexture(e['GL_TEXTURE_2D'], tex.value)
    gl.glTexParameteri(e['GL_TEXTURE_2D'], e['GL_TEXTURE_MIN_FILTER'], e['GL_NEAREST']); gl.glTexParameteri(e['GL_TEXTURE_2D'], e['GL_TEXTURE_MAG_FILTER'], e['GL_NEAREST'])
    data = (ctypes.c_uint32 * 4)(0xFF3080C0, 0xFF3080C0, 0xFF3080C0, 0xFF3080C0)
    gl.glTexImage2D(e['GL_TEXTURE_2D'], 0, e['GL_RGBA'], 2, 2, 0, e['GL_BGRA'], e['GL_UNSIGNED_INT_8_8_8_8_REV'], P(data))
    gl.glUseProgram(p)
    def uni(name): return gl.glGetUniformLocation(p, name.encode())
    ortho = farr(2 / 64, 0, 0, 0, 0, 2 / 64, 0, 0, 0, 0, -2 / 1000, 0, -1, -1, -1, 1)   # glOrtho(0,64,0,64,0.1,1000)-like
    if uni('ProjMat') >= 0: gl.glUniformMatrix4fv(uni('ProjMat'), 1, 0, P(ortho))
    for n_, v in (('InSize', (64, 64)), ('OutSize', (64, 64))):
        if uni(n_) >= 0: gl.glUniform2f(uni(n_), *v)
    if uni('DiffuseSampler') >= 0: gl.glUniform1i(uni('DiffuseSampler'), 0)
    cm = uni('ColorModulate')
    for u in j.get('uniforms', []):                     # ShaderManager: values from the program JSON
        l = uni(u['name']); vals = u.get('values', [])
        if l < 0 or u['name'] in ('ProjMat', 'InSize', 'OutSize'): continue
        if u.get('type') == 'float':
            f = [getattr(gl, 'glUniform%df' % len(vals))] if 1 <= len(vals) <= 4 else []
            if f: f[0](l, *vals)
    gl.glViewport(0, 0, 64, 64); gl.glClearColor(0, 0, 0, 1); gl.glClear(e['GL_COLOR_BUFFER_BIT'])
    vd = b''.join(struct.pack('<fff4B', x, y, 500, 255, 255, 255, 255) for (x, y) in [(0, 64), (64, 64), (64, 0), (0, 0)])
    buf = ctypes.create_string_buffer(vd, len(vd)); a = ctypes.addressof(buf)
    gl.glVertexPointer(3, e['GL_FLOAT'], 16, a); gl.glColorPointer(4, e['GL_UNSIGNED_BYTE'], 16, a + 12)
    gl.glEnableClientState(e['GL_VERTEX_ARRAY']); gl.glEnableClientState(e['GL_COLOR_ARRAY'])
    gl.glDrawArrays(e['GL_QUADS'], 0, 4)
    gl.glDisableClientState(e['GL_VERTEX_ARRAY']); gl.glDisableClientState(e['GL_COLOR_ARRAY'])
    gl.glUseProgram(0)
    px = C.px(32, 32)
    res.append(('blit program (Position <- glVertexPointer, uniform defaults, QUADS)', close(px, (48, 128, 192, 255), 2), (px, cm)))

# ---- classic GLSL 1.20 with fixed-function builtins, drawn with immediate mode (shaderpack/mod pattern)
VS120 = """#version 120
varying vec2 uv;
void main() {
    gl_Position = ftransform();
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
    uv = gl_MultiTexCoord0.st * 2 - 1;
}"""
FS120 = """#version 120
uniform sampler2D tex;
uniform float gain = 1;
varying vec2 uv;
float half(float x) { return x / 2; }
void main() {
    vec4 c = texture2D(tex, gl_TexCoord[0].st) * gl_Color;
    float k = 1;
    int n = 2;
    for (int i = 0; i < n; i++) k += 0;
    if (uv.x > 2) discard;
    gl_FragColor = vec4(c.rgb * k * gain + half(0), 1);
}"""
vs, okv = shader('v', VS120); fs, okf = shader('f', FS120)
p2 = gl.glCreateProgram(); gl.glAttachShader(p2, vs); gl.glAttachShader(p2, fs); gl.glLinkProgram(p2)
ok = ctypes.c_int32(0); gl.glGetProgramiv(p2, e['GL_LINK_STATUS'], P(ctypes.pointer(ok)))
if okv and okf and ok.value:
    gl.glViewport(0, 0, 64, 64)
    gl.glMatrixMode(e['GL_PROJECTION']); gl.glLoadIdentity(); gl.glOrtho(0, 64, 0, 64, -1, 1)
    gl.glMatrixMode(e['GL_TEXTURE']); gl.glLoadIdentity(); gl.glScalef(0.5, 0.5, 1)
    gl.glMatrixMode(e['GL_MODELVIEW']); gl.glLoadIdentity()
    t2 = ctypes.c_uint32(0); gl.glGenTextures(1, P(ctypes.pointer(t2))); gl.glBindTexture(e['GL_TEXTURE_2D'], t2.value)
    gl.glTexParameteri(e['GL_TEXTURE_2D'], e['GL_TEXTURE_MIN_FILTER'], e['GL_NEAREST']); gl.glTexParameteri(e['GL_TEXTURE_2D'], e['GL_TEXTURE_MAG_FILTER'], e['GL_NEAREST'])
    d2 = (ctypes.c_uint32 * 4)(0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFF000000)           # only texel (0,0) white
    gl.glTexImage2D(e['GL_TEXTURE_2D'], 0, e['GL_RGBA'], 2, 2, 0, e['GL_BGRA'], e['GL_UNSIGNED_INT_8_8_8_8_REV'], P(d2))
    gl.glUseProgram(p2); gl.glUniform1i(gl.glGetUniformLocation(p2, b'tex'), 0)
    gl.glClear(e['GL_COLOR_BUFFER_BIT'])
    gl.glColor4f(1, 0.5, 0.25, 1)
    gl.glBegin(e['GL_QUADS'])
    for (x, y, s_, t_) in [(0, 0, 0, 0), (64, 0, 1, 0), (64, 64, 1, 1), (0, 64, 0, 1)]:
        gl.glTexCoord2f(s_, t_); gl.glVertex2f(x, y)
    gl.glEnd()
    gl.glUseProgram(0)
    gl.glMatrixMode(e['GL_TEXTURE']); gl.glLoadIdentity(); gl.glMatrixMode(e['GL_MODELVIEW'])
    px_in, px_out = C.px(20, 20), C.px(50, 50)          # texture matrix halves coords: whole quad samples texel (0,0)
    res.append(('GLSL 1.20 builtins (ftransform, gl_TextureMatrix, gl_TexCoord, gl_Color, int literals, uniform init)',
                close(px_in, (255, 128, 64, 255), 2) and close(px_out, (255, 128, 64, 255), 2), (px_in, px_out)))
else:
    res.append(('GLSL 1.20 builtins compile/link', False, (okv, okf, ok.value, log_of(vs)[:200], log_of(fs)[:200])))

# ---- integer-context corpus (compile-only, fragment stage, GLSL 1.30 and 1.20 flavours)
CORPUS = [
 "ivec2 p = ivec2(gl_FragCoord.xy) * 2; o = vec4(p.x);",
 "int i = 3; int idx = int(mod(float(i), 4.0)) + 1; o = vec4(idx);",
 "float arr[3]; arr[0] = 1; arr[1] = arr[0] * 2; arr[2] = 0; o = vec4(arr[1]);",
 "float x = 0.3; bool c = x > 0; x = c ? 1 : 0; o = vec4(x);",
 "int a = 3 / 2; int b = a % 2; o = vec4(float(a + b));",
 "int i = 2; float f = float(i) / 3; if (i == 2) f += 1; o = vec4(f);",
 "ivec2 sz = textureSize(s, 0); vec4 t = texelFetch(s, ivec2(1, 1), 0); o = t + vec4(sz.x);",
 "float w[4]; for (int k = 0; k < 4; k++) w[k] = k * 0.25; o = vec4(w[2] * 2);",
 "vec2 uv = gl_FragCoord.xy / vec2(1920, 1080) * 2 - 1; o = vec4(uv, 0, 1);",
 "mat3 m = mat3(1); vec3 v = m * vec3(1, 2, 3) - 1; o = vec4(v, 1);",
 "int mode = 1; float r = 0; switch (mode) { case 1: r = 1; break; default: r = 2; } o = vec4(r);",
 "float k = max(0, 1 - length(gl_FragCoord.xy) / 10); int j = max(2, 3); o = vec4(k + float(j));",
]
HDR130 = "#version 130\nuniform sampler2D s;\nout vec4 fragColor;\nfloat g(float x) { return x * 2; }\nint h(int n) { return n + 1; }\nvoid main() { vec4 o; "
okc = 0
for body in CORPUS:
    src = HDR130 + body + " fragColor = o + vec4(g(1), float(h(2)), 0, 0); }"
    sh_, ok_ = shader('f', src)
    okc += ok_
    if not ok_ and os.environ.get('ORYON_VERBOSE'): print(log_of(sh_)[:300])
res.append(('integer-context corpus (%d snippets)' % len(CORPUS), okc == len(CORPUS), '%d/%d' % (okc, len(CORPUS))))
err = gl.glGetError(); res.append(('no GL error', err == 0, hex(err)))
C.close()
for n, ok, d in res: print(('PASS ' if ok else 'FAIL ') + n + ('' if ok else '  -> %s' % (d,)))
allc = all(v[1] for v in comp.values()) and linked == len(progs)
print('RESULT:', 'PASS' if allc and all(r[1] for r in res) else 'FAIL')
