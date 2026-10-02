#!/usr/bin/env python3
# tools/test_calls.py -- ES call budget of Minecraft 1.12.2's VBO chunk path on real Mesa EGL + GLES 3.2 (ORYON_STATS=1).
# The child replays the per-chunk sequence of bvf.a(Lamm;)V as resolved from the 1.12.2 jar: glPushMatrix, glTranslatef,
# glMultMatrix, glBindBuffer(GL_ARRAY_BUFFER), the array pointers of bvf.a()V (vertex 3/FLOAT/28/0, colour 4/UBYTE/28/12,
# texcoord 2/FLOAT/28/16, lightmap unit texcoord 2/SHORT/28/24 between two glClientActiveTexture calls),
# glDrawArrays(GL_QUADS, 0, n), glPopMatrix; then glBindBuffer(GL_ARRAY_BUFFER, 0) after the layer.
# Checks: with ES 3.1 vertex attribute binding a chunk costs one glBindVertexBuffer, one uniform upload and the draw
# (no glVertexAttribPointer, no format/binding change); ORYON_NO_VERTEX_BINDING=1 falls back to 4 attribute pointers per
# chunk; both render identical pixels without GL errors. ORYON_SO_PREV=<old .so> prints the old budget for comparison.
import os, sys, re, json, statistics, subprocess, tempfile, struct, time
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
CHUNKS = 64

def child(seconds):
    sys.path.insert(0, HERE)
    import ctypes
    from gltest import Ctx, E, P, farr
    W = 128
    C = Ctx(W, W); gl = C.gl; e = E
    t = (ctypes.c_uint32 * 2)(); gl.glGenTextures(2, P(t))
    for i, u in enumerate(('GL_TEXTURE0', 'GL_TEXTURE1')):                       # block atlas, lightmap (white)
        gl.glActiveTexture(e[u]); gl.glBindTexture(e['GL_TEXTURE_2D'], t[i])
        px = (ctypes.c_uint32 * 4)(*([0xFFFFFFFF] * 4))
        gl.glTexImage2D(e['GL_TEXTURE_2D'], 0, e['GL_RGBA'], 2, 2, 0, e['GL_RGBA'], e['GL_UNSIGNED_BYTE'], P(px))
        for pn in ('GL_TEXTURE_MIN_FILTER', 'GL_TEXTURE_MAG_FILTER'): gl.glTexParameteri(e['GL_TEXTURE_2D'], e[pn], e['GL_NEAREST'])
        gl.glEnable(e['GL_TEXTURE_2D'])
        if i == 1:                                                                # lightmap texture matrix (1/256 scale, +8)
            gl.glMatrixMode(e['GL_TEXTURE']); gl.glLoadIdentity(); gl.glScalef(1 / 256, 1 / 256, 1 / 256); gl.glTranslatef(8, 8, 8)
            gl.glMatrixMode(e['GL_MODELVIEW'])
    gl.glActiveTexture(e['GL_TEXTURE0'])
    gl.glEnable(e['GL_FOG']); gl.glFogi(e['GL_FOG_MODE'], e['GL_LINEAR']); gl.glFogf(e['GL_FOG_START'], 50.0); gl.glFogf(e['GL_FOG_END'], 100.0)
    gl.glEnable(e['GL_ALPHA_TEST']); gl.glAlphaFunc(e['GL_GREATER'], 0.1)
    gl.glMatrixMode(e['GL_PROJECTION']); gl.glLoadIdentity(); gl.glOrtho(0, 8, 0, 8, -10, 10)
    gl.glMatrixMode(e['GL_MODELVIEW']); gl.glLoadIdentity()
    vbo = (ctypes.c_uint32 * CHUNKS)(); gl.glGenBuffers(CHUNKS, P(vbo))
    cols = []
    for i in range(CHUNKS):
        c = ((i * 37 + 40) & 255, (i * 73 + 20) & 255, (i * 151 + 60) & 255)
        cols.append(c)
        vd = b''.join(struct.pack('<3f4B2f2h', x, y, 0, c[0], c[1], c[2], 255, x, y, 240, 240)
                      for x, y in ((0.05, 0.05), (0.95, 0.05), (0.95, 0.95), (0.05, 0.95)))
        gl.glBindBuffer(e['GL_ARRAY_BUFFER'], vbo[i])
        b = ctypes.create_string_buffer(vd, len(vd))
        gl.glBufferData(e['GL_ARRAY_BUFFER'], len(vd), P(b), e['GL_STATIC_DRAW'])
    gl.glBindBuffer(e['GL_ARRAY_BUFFER'], 0)
    chunk_mv = farr(1.000001, 0, 0, 0, 0, 1.000001, 0, 0, 0, 0, 1.000001, 0, 0, 0, 0, 1)     # RenderChunk modelview
    t_end = time.time() + seconds; frames = 0
    while time.time() < t_end:
        gl.glClear(e['GL_COLOR_BUFFER_BIT'] | e['GL_DEPTH_BUFFER_BIT'])                  # frame start (framebuffer 0)
        gl.glEnableClientState(e['GL_VERTEX_ARRAY'])
        gl.glClientActiveTexture(e['GL_TEXTURE0']); gl.glEnableClientState(e['GL_TEXTURE_COORD_ARRAY'])
        gl.glClientActiveTexture(e['GL_TEXTURE1']); gl.glEnableClientState(e['GL_TEXTURE_COORD_ARRAY'])
        gl.glClientActiveTexture(e['GL_TEXTURE0']); gl.glEnableClientState(e['GL_COLOR_ARRAY'])
        for i in range(CHUNKS):
            gl.glPushMatrix()
            gl.glTranslatef(float(i % 8), float(i // 8), 0.0)
            gl.glMultMatrixf(chunk_mv)
            gl.glBindBuffer(e['GL_ARRAY_BUFFER'], vbo[i])
            gl.glVertexPointer(3, e['GL_FLOAT'], 28, P(0)); gl.glColorPointer(4, e['GL_UNSIGNED_BYTE'], 28, P(12))
            gl.glTexCoordPointer(2, e['GL_FLOAT'], 28, P(16))
            gl.glClientActiveTexture(e['GL_TEXTURE1']); gl.glTexCoordPointer(2, e['GL_SHORT'], 28, P(24))
            gl.glClientActiveTexture(e['GL_TEXTURE0'])
            gl.glDrawArrays(e['GL_QUADS'], 0, 4)
            gl.glPopMatrix()
        gl.glBindBuffer(e['GL_ARRAY_BUFFER'], 0)
        gl.glDisableClientState(e['GL_VERTEX_ARRAY']); gl.glDisableClientState(e['GL_COLOR_ARRAY'])
        gl.glClientActiveTexture(e['GL_TEXTURE1']); gl.glDisableClientState(e['GL_TEXTURE_COORD_ARRAY'])
        gl.glClientActiveTexture(e['GL_TEXTURE0']); gl.glDisableClientState(e['GL_TEXTURE_COORD_ARRAY'])
        C.egl.swap(); frames += 1
    gl.glFinish()
    px = [C.px(int((i % 8 + 0.5) * W / 8), int((i // 8 + 0.5) * W / 8)) for i in range(CHUNKS)]
    ok = all(tuple(p[:3]) == c for p, c in zip(px, cols))
    print('CHILD ' + json.dumps({'frames': frames, 'px': px, 'colours_ok': ok, 'err': gl.glGetError()}), flush=True)

if len(sys.argv) > 1 and sys.argv[1] == '--child':
    child(float(sys.argv[2])); sys.exit(0)

fails = []
def check(ok, msg):
    if not ok: fails.append(msg)
N = r'([0-9.]+)'
CALLS = re.compile(r'\] calls ' + N + r's: per frame draw ' + N + r' prog ' + N + r' unif ' + N + r' tex ' + N + r' vao ' + N
                   + r' bind ' + N + r' attr ' + N + r' vbuf ' + N + r' fmt ' + N)
OLD = re.compile(r'calls/f prog ' + N + r' unif ' + N + r' tex ' + N + r' vao ' + N + r' buf ' + N + r' attr ' + N)   # c268434 format
DRAWS = re.compile(r'\] stats [0-9.]+s: .*? ES draws ' + N + r'/f')
KEYS = ('draw', 'prog', 'unif', 'tex', 'vao', 'bind', 'attr', 'vbuf', 'fmt')

def run(extra, so=None, seconds=2.6):
    env = dict(os.environ); env['ORYON_STATS'] = '1'; env['ORYON_STATS_PROFILE'] = '0'
    for k in ('ORYON_NO_VERTEX_BINDING', 'ORYON_STATS_GPU'): env.pop(k, None)
    env['ORYON_CACHE_DIR'] = tempfile.mkdtemp(prefix='oryon-calls-')
    if so: env['ORYON_SO'] = so
    env.update(extra)
    p = subprocess.run([sys.executable, os.path.abspath(__file__), '--child', str(seconds)], env=env, capture_output=True, text=True, timeout=300)
    m = re.search(r'^CHILD (.*)$', p.stdout, re.M)
    res = json.loads(m.group(1)) if m else None
    lines = p.stderr.splitlines()
    wins = []
    for l in lines:
        c = CALLS.search(l)
        if c: wins.append(dict(zip(KEYS, map(float, c.groups()[1:]))))
    if not wins:                                    # older build: counters in the cpu line, draws in the stats line
        dr = [float(d.group(1)) for d in map(DRAWS.search, lines) if d]
        ol = [o for o in map(OLD.search, lines) if o]
        for d, o in zip(dr, ol):
            v = list(map(float, o.groups()))
            wins.append(dict(draw=d, prog=v[0], unif=v[1], tex=v[2], vao=v[3], bind=v[4], attr=v[5], vbuf=0.0, fmt=0.0))
    wins = wins[1:]                                 # first window holds the start-up frames
    per = {}
    if wins:
        for k in KEYS: per[k] = statistics.median(w[k] for w in wins)
    mode = re.search(r'\| vertex (attrib-\w+) \|', p.stderr)
    return p.returncode, res, per, (mode.group(1) if mode else None), p.stderr

def budget(per):
    d = per.get('draw') or 1.0
    r = {k: per[k] / d for k in KEYS if k != 'draw'}
    r['calls'] = sum(per[k] for k in KEYS if k not in ('vao', 'tex')) / d     # ES calls per chunk draw
    return r

rc_b, res_b, per_b, mode_b, err_b = run({})
rc_p, res_p, per_p, mode_p, err_p = run({'ORYON_NO_VERTEX_BINDING': '1'})
check(rc_b == 0 and res_b, 'binding-mode child failed: %s' % err_b[-400:])
check(rc_p == 0 and res_p, 'pointer-mode child failed: %s' % err_p[-400:])
check(mode_b == 'attrib-binding', 'init log should report vertex attrib-binding on Mesa ES 3.2 (got %s)' % mode_b)
check(mode_p == 'attrib-pointer', 'ORYON_NO_VERTEX_BINDING=1 should report vertex attrib-pointer (got %s)' % mode_p)
check(bool(per_b) and bool(per_p), 'calls lines missing (binding %s, pointer %s)' % (per_b, per_p))
if per_b and per_p:
    bb, bp = budget(per_b), budget(per_p)
    check(abs(per_b['draw'] - CHUNKS) <= 1 and abs(per_p['draw'] - CHUNKS) <= 1, 'expected %d draws per frame: %s / %s' % (CHUNKS, per_b['draw'], per_p['draw']))
    check(bb['attr'] <= 0.05 and bb['fmt'] <= 0.05, 'binding mode must not respecify attributes per chunk: %s' % bb)
    check(0.9 <= bb['vbuf'] <= 1.1, 'binding mode: one glBindVertexBuffer per chunk expected: %.2f' % bb['vbuf'])
    check(bb['unif'] <= 1.1 and bb['prog'] <= 0.05 and bb['bind'] <= 0.05, 'binding mode: one uniform upload, no program/buffer bind per chunk: %s' % bb)
    check(3.9 <= bp['attr'] <= 4.1 and per_p['vbuf'] == 0 and per_p['fmt'] == 0, 'pointer fallback: 4 attribute pointers per chunk, no binding calls: %s' % bp)
    check(bb['calls'] <= 3.2, 'binding mode: <= 3 ES calls per chunk draw expected: %.2f' % bb['calls'])
if res_b and res_p:
    check(res_b['colours_ok'] and res_p['colours_ok'], 'chunk colours wrong: binding %s pointer %s' % (res_b['colours_ok'], res_p['colours_ok']))
    check(res_b['px'] == res_p['px'], 'binding and pointer modes must render identical pixels')
    check(res_b['err'] == 0 and res_p['err'] == 0, 'GL error: %s / %s' % (res_b['err'], res_p['err']))

print('== CALL BUDGET TEST (MC 1.12.2 VBO chunk path, %d chunks/frame) ==' % CHUNKS)
def show(name, per):
    if not per: print('%-9s n/a' % name); return
    b = budget(per)
    print('%-9s per chunk: ES calls %.2f | attr %.2f vbuf %.2f fmt %.2f unif %.2f bind %.2f prog %.2f | draws/f %.0f'
          % (name, b['calls'], b['attr'], b['vbuf'], b['fmt'], b['unif'], b['bind'], b['prog'], per['draw']))
show('binding', per_b); show('pointer', per_p)
prev = os.environ.get('ORYON_SO_PREV')
if prev:
    rc_o, res_o, per_o, _, err_o = run({}, so=os.path.abspath(prev))
    show('previous', per_o)
    if res_o and res_b: check(res_o['px'] == res_b['px'], 'previous build renders different pixels')
print('RESULT:', 'PASS' if not fails else 'FAIL')
for f in fails: print('  -', f)
sys.exit(1 if fails else 0)
