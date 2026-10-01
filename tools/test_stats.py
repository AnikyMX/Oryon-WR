#!/usr/bin/env python3
# tools/test_stats.py -- ORYON_STATS diagnostics on real Mesa GLES 3.2: the per-second summary line must appear only
# when enabled, count frames only for colour clears of framebuffer 0, and attribute every event class.
import os, sys, re, ctypes, struct, subprocess, time
HERE = os.path.dirname(os.path.abspath(__file__))

def child(seconds):
    sys.path.insert(0, HERE)
    from gltest import Ctx, E, P
    C = Ctx(64, 64); gl = C.gl; e = E
    # an FBO whose colour clears must NOT count as frames
    fbo = ctypes.c_uint32(0); gl.glGenFramebuffers(1, P(ctypes.pointer(fbo)))
    rb = ctypes.c_uint32(0); gl.glGenRenderbuffers(1, P(ctypes.pointer(rb)))
    gl.glBindRenderbuffer(e['GL_RENDERBUFFER'], rb.value)
    gl.glRenderbufferStorage(e['GL_RENDERBUFFER'], e['GL_RGBA8'], 16, 16)
    gl.glBindFramebuffer(e['GL_FRAMEBUFFER'], fbo.value)
    gl.glFramebufferRenderbuffer(e['GL_FRAMEBUFFER'], e['GL_COLOR_ATTACHMENT0'], e['GL_RENDERBUFFER'], rb.value)
    gl.glBindFramebuffer(e['GL_FRAMEBUFFER'], 0)
    tex = ctypes.c_uint32(0); gl.glGenTextures(1, P(ctypes.pointer(tex)))
    gl.glBindTexture(e['GL_TEXTURE_2D'], tex.value)
    px = (ctypes.c_uint32 * 64)(*([0xFF336699] * 64))
    gl.glTexImage2D(e['GL_TEXTURE_2D'], 0, e['GL_RGBA'], 8, 8, 0, e['GL_BGRA'], e['GL_UNSIGNED_INT_8_8_8_8_REV'], P(px))
    vbo = ctypes.c_uint32(0); gl.glGenBuffers(1, P(ctypes.pointer(vbo)))
    vd = b''.join(struct.pack('<3f', x, y, 0) for (x, y) in [(0, 0), (64, 0), (64, 64), (0, 64)])
    vbuf = ctypes.create_string_buffer(vd, len(vd))
    gl.glMatrixMode(e['GL_PROJECTION']); gl.glLoadIdentity(); gl.glOrtho(0, 64, 0, 64, -1, 1)
    gl.glMatrixMode(e['GL_MODELVIEW']); gl.glLoadIdentity()
    lst = gl.glGenLists(1)
    frames = 0; t_end = time.time() + seconds
    while time.time() < t_end:
        gl.glClear(e['GL_COLOR_BUFFER_BIT'] | e['GL_DEPTH_BUFFER_BIT'])           # frame boundary (FB 0)
        gl.glBindFramebuffer(e['GL_FRAMEBUFFER'], fbo.value)
        gl.glClear(e['GL_COLOR_BUFFER_BIT'])                                       # FBO clear: not a frame
        gl.glBindFramebuffer(e['GL_FRAMEBUFFER'], 0)
        if frames % 2: gl.glEnable(e['GL_TEXTURE_2D'])                              # two FFP programs
        else: gl.glDisable(e['GL_TEXTURE_2D'])
        gl.glBegin(e['GL_QUADS'])
        for (x, y) in [(0, 0), (64, 0), (64, 64), (0, 64)]: gl.glVertex2f(x, y)
        gl.glEnd()
        gl.glTexSubImage2D(e['GL_TEXTURE_2D'], 0, 0, 0, 8, 8, e['GL_BGRA'], e['GL_UNSIGNED_INT_8_8_8_8_REV'], P(px))
        gl.glBindBuffer(e['GL_ARRAY_BUFFER'], vbo.value)
        gl.glBufferData(e['GL_ARRAY_BUFFER'], len(vd), P(vbuf), e['GL_STATIC_DRAW'])
        gl.glBindBuffer(e['GL_ARRAY_BUFFER'], 0)
        if frames == 0:
            gl.glNewList(lst, e['GL_COMPILE'])
            gl.glBegin(e['GL_TRIANGLES']); gl.glVertex2f(0, 0); gl.glVertex2f(8, 0); gl.glVertex2f(0, 8); gl.glEnd()
            gl.glEndList()
        gl.glCallList(lst)
        C.px(1, 1)                                                                 # readback
        frames += 1
        time.sleep(0.02)
    gl.glFinish()
    print('CHILD_FRAMES %d' % frames, flush=True)

if len(sys.argv) > 1 and sys.argv[1] == '--child':
    child(float(sys.argv[2])); sys.exit(0)

fails = []
def check(ok, msg):
    if not ok: fails.append(msg)
def run(env_extra, seconds):
    env = dict(os.environ); env.pop('ORYON_STATS', None); env.update(env_extra)
    p = subprocess.run([sys.executable, os.path.abspath(__file__), '--child', str(seconds)], env=env,
                       capture_output=True, text=True, timeout=120)
    return p.returncode, p.stdout, p.stderr

NUM = r'([0-9.]+)'
PAT = re.compile(r'\[Oryon\] stats ' + NUM + r's: (\d+) frames \(' + NUM + r' fps, worst ' + NUM + r' ms\) \| ES draws ' + NUM +
                 r'/f \| stream ' + NUM + r' KB/f \| ffp prog \+(\d+) \(' + NUM + r' ms, max ' + NUM + r'\) \| glsl link \+(\d+) \(' +
                 NUM + r' ms, max ' + NUM + r'\) \| dlist \+(\d+) \(' + NUM + r' ms\) \| ring wait (\d+) \(' + NUM +
                 r' ms\) \| tex up (\d+) \(' + NUM + r' Mpx\) \| buf up (\d+) \(' + NUM + r' MB\) \| readback (\d+) \(' + NUM + r' ms\)')

rc, out, err = run({'ORYON_STATS': '1'}, 2.6)
check(rc == 0, 'child failed rc=%d: %s' % (rc, err[-400:]))
lines = [l for l in err.splitlines() if re.search(r'\] stats [0-9]', l)]
rows = [PAT.search(l) for l in lines]
check(len(lines) >= 2, 'expected >= 2 stats lines, got %d' % len(lines))
check(all(rows), 'malformed stats line: %s' % [l for l, r in zip(lines, rows) if not r][:1])
check(any('stats enabled' in l for l in err.splitlines()), 'missing "stats enabled" init line')
if rows and all(rows):
    r0 = rows[0]
    frames = [int(r.group(2)) for r in rows]; fps = [float(r.group(3)) for r in rows]
    check(all(30 <= f <= 55 for f in fps), 'fps outside 30..55 with a 20 ms sleep per frame (FBO clears must not count): %s' % fps)
    check(all(float(r.group(4)) >= 19.0 for r in rows), 'worst frame gap below the 20 ms sleep')
    check(float(r0.group(5)) >= 1.0, 'ES draws per frame should be >= 1')
    check(float(r0.group(6)) > 0, 'stream bytes per frame should be > 0')
    check(int(r0.group(7)) >= 2, 'first window should build >= 2 FFP programs (texture on/off)')
    check(sum(int(r.group(7)) for r in rows[1:]) == 0, 'FFP programs must be cached after the first window')
    check(int(r0.group(13)) == 1, 'exactly one display list built in the first window')
    check(all(int(r.group(17)) >= frames[i] - 1 for i, r in enumerate(rows)), 'tex uploads should be >= frames - 1')
    check(all(int(r.group(19)) >= frames[i] - 1 for i, r in enumerate(rows)), 'buffer uploads should be >= frames - 1')
    check(all(int(r.group(21)) >= frames[i] - 1 for i, r in enumerate(rows)), 'readbacks should be >= frames - 1')
rc2, out2, err2 = run({}, 1.3)
check(rc2 == 0 and not re.search(r'\] stats ', err2), 'stats must stay silent when ORYON_STATS is unset')
rc3, out3, err3 = run({'ORYON_STATS': '0'}, 1.3)
check(rc3 == 0 and not re.search(r'\] stats ', err3), 'ORYON_STATS=0 must disable stats')

print('== STATS TEST ==')
for l in lines[:2]: print(l.split('] ', 1)[1])
print('RESULT:', 'PASS' if not fails else 'FAIL')
for f in fails: print('  -', f)
sys.exit(1 if fails else 0)
