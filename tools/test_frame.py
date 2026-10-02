#!/usr/bin/env python3
# tools/test_frame.py -- frame-structure optimisations on real Mesa EGL + GLES 3.2:
#   * framebuffer-0 clear deferral (src/fb.cpp): every scenario is rendered with the default (deferred) and with
#     ORYON_NO_DEFER_CLEAR=1; pixels must be identical and match the expected values. Scenarios: Minecraft's frame
#     (bib.az: clear framebuffer 0, bind its framebuffer, render, unbind, full-screen copy with alpha write off; captured
#     clear colour/depth used, current ones restored), no framebuffer object with immediate-mode batches flushed
#     inside glEnd, framebuffer 0 made readable through GL_READ_FRAMEBUFFER, scissored / partial-mask / stencil clears
#     (not deferred), deleting the bound framebuffer, glClear inside a display list, a re-armed clear followed by an
#     immediate-mode batch and another switch (glBegin and glRectf), invalid mask error;
#   * render-thread scheduling (src/sched.cpp): ORYON_BIG_CORES pin + nice -10 on the render thread, threads created
#     afterwards start at nice 0 (SCHED_RESET_ON_FORK) and get their CPU mask back, opt-outs.
import os, sys, re, json, subprocess, tempfile, threading, time
HERE = os.path.dirname(os.path.abspath(__file__))
W = 64

def child(what):
    sys.path.insert(0, HERE)
    import ctypes
    from gltest import Ctx, E, P
    C = Ctx(W, W); gl = C.gl; e = E
    CB, DB, SB = e['GL_COLOR_BUFFER_BIT'], e['GL_DEPTH_BUFFER_BIT'], e['GL_STENCIL_BUFFER_BIT']
    FB, RFB = e['GL_FRAMEBUFFER'], e['GL_READ_FRAMEBUFFER']
    def quad(x0, y0, x1, y1, z=0.0, tex=False):
        gl.glBegin(e['GL_QUADS'])
        for (x, y, s, t) in ((x0, y0, 0, 0), (x1, y0, 1, 0), (x1, y1, 1, 1), (x0, y1, 0, 1)):
            if tex: gl.glTexCoord2f(s, t)
            gl.glVertex3f(x, y, z)
        gl.glEnd()
    def make_fbo():
        tex = ctypes.c_uint32(0); gl.glGenTextures(1, P(ctypes.pointer(tex)))
        gl.glBindTexture(e['GL_TEXTURE_2D'], tex.value)
        gl.glTexImage2D(e['GL_TEXTURE_2D'], 0, e['GL_RGBA8'], W, W, 0, e['GL_RGBA'], e['GL_UNSIGNED_BYTE'], None)
        for pn in ('GL_TEXTURE_MIN_FILTER', 'GL_TEXTURE_MAG_FILTER'): gl.glTexParameteri(e['GL_TEXTURE_2D'], e[pn], e['GL_NEAREST'])
        gl.glBindTexture(e['GL_TEXTURE_2D'], 0)
        rb = ctypes.c_uint32(0); gl.glGenRenderbuffers(1, P(ctypes.pointer(rb)))
        gl.glBindRenderbuffer(e['GL_RENDERBUFFER'], rb.value)
        gl.glRenderbufferStorage(e['GL_RENDERBUFFER'], e['GL_DEPTH_COMPONENT24'], W, W)
        f = ctypes.c_uint32(0); gl.glGenFramebuffers(1, P(ctypes.pointer(f)))
        gl.glBindFramebuffer(FB, f.value)
        gl.glFramebufferTexture2D(FB, e['GL_COLOR_ATTACHMENT0'], e['GL_TEXTURE_2D'], tex.value, 0)
        gl.glFramebufferRenderbuffer(FB, e['GL_DEPTH_ATTACHMENT'], e['GL_RENDERBUFFER'], rb.value)
        gl.glBindFramebuffer(FB, 0)
        return f.value, tex.value
    px = lambda x, y: list(C.px(x, y))
    out = {}
    if what == 'mc':                                   # Minecraft 1.12.2 frame shape, several frames
        fbo, tex = make_fbo()
        for frame in range(3):
            gl.glClearColor(1, 0, 0, 0.5); gl.glClearDepth(1.0); gl.glClear(CB | DB)        # framebuffer 0 (deferred)
            gl.glBindFramebuffer(FB, fbo); gl.glViewport(0, 0, W, W)
            gl.glClearColor(0, 0, 1, 1); gl.glClearDepth(0.5); gl.glClear(CB | DB)          # its own framebuffer
            gl.glColor4f(0, 1, 0, 1); quad(-1, -1, 0, 1)
            gl.glBindFramebuffer(FB, 0)
            gl.glColorMask(1, 1, 1, 0); gl.glDepthMask(0); gl.glDisable(e['GL_DEPTH_TEST'])
            gl.glEnable(e['GL_TEXTURE_2D']); gl.glBindTexture(e['GL_TEXTURE_2D'], tex); gl.glColor4f(1, 1, 1, 1)
            quad(-1, -1, 1, 1, tex=True)
            gl.glDisable(e['GL_TEXTURE_2D']); gl.glBindTexture(e['GL_TEXTURE_2D'], 0)
            gl.glColorMask(1, 1, 1, 1); gl.glDepthMask(1)
            gl.glEnable(e['GL_DEPTH_TEST']); gl.glDepthFunc(e['GL_LESS'])
            gl.glColor4f(1, 1, 0, 1); quad(0.25, -0.25, 0.75, 0.25, z=0.9)   # passes only if depth was cleared to 1.0
            gl.glDisable(e['GL_DEPTH_TEST'])
            if frame < 2: C.egl.swap()
        out['frame'] = [px(16, 32), px(48, 56), px(40, 32)]
        gl.glClear(CB)                                 # current clear colour (blue) must have been restored
        out['restored_color'] = px(8, 8)
        gl.glClear(DB); gl.glEnable(e['GL_DEPTH_TEST'])
        gl.glColor4f(1, 1, 0, 1); quad(-1, -1, 1, 1, z=0.9)                   # depth cleared to the restored 0.5: fails
        gl.glDisable(e['GL_DEPTH_TEST'])
        out['restored_depth'] = px(8, 8)
    elif what == 'nofbo':                              # no framebuffer object; batches flushed inside glEnd
        gl.glClearColor(1, 0, 0, 1); gl.glClear(CB)
        gl.glColor4f(0, 1, 0, 1); quad(-0.5, -0.5, 0.5, 0.5)
        gl.glBegin(e['GL_LINES']); gl.glVertex3f(-1, 0.890625, 0); gl.glVertex3f(1, 0.890625, 0); gl.glEnd()   # row 60 centre; different class: flushes the quad
        gl.glColor4f(0, 0, 1, 1)
        gl.glBegin(e['GL_TRIANGLES']); gl.glVertex3f(-1, -1, 0); gl.glVertex3f(-0.8, -1, 0); gl.glVertex3f(-1, -0.8, 0); gl.glEnd()
        out['px'] = [px(32, 32), px(60, 4), px(1, 1), px(30, 60)]
    elif what == 'read':                               # framebuffer 0 made readable while another one is bound
        fbo, tex = make_fbo()
        gl.glClearColor(1, 0, 0, 1); gl.glClear(CB)
        gl.glBindFramebuffer(FB, fbo)
        gl.glClearColor(0, 0, 1, 1); gl.glClear(CB)
        gl.glBindFramebuffer(RFB, 0); r0 = px(5, 5)
        gl.glBindFramebuffer(RFB, fbo); r1 = px(5, 5)
        gl.glBindFramebuffer(FB, 0); r2 = px(5, 5)
        out['px'] = [r0, r1, r2]
    elif what == 'scissor':                            # scissored clear: not deferred
        fbo, tex = make_fbo()
        gl.glClearColor(0, 0, 0, 1); gl.glClear(CB); px(0, 0)
        gl.glEnable(e['GL_SCISSOR_TEST']); gl.glScissor(0, 0, 16, 16)
        gl.glClearColor(1, 0, 0, 1); gl.glClear(CB)
        gl.glBindFramebuffer(FB, fbo); gl.glDisable(e['GL_SCISSOR_TEST']); gl.glClear(CB)
        gl.glBindFramebuffer(FB, 0)
        out['px'] = [px(8, 8), px(40, 40)]
    elif what == 'mask':                               # partial colour mask: not deferred
        fbo, tex = make_fbo()
        gl.glClearColor(0, 0, 0, 1); gl.glClear(CB); px(0, 0)
        gl.glColorMask(1, 0, 1, 1); gl.glClearColor(1, 1, 1, 1); gl.glClear(CB)
        gl.glBindFramebuffer(FB, fbo); gl.glColorMask(1, 1, 1, 1); gl.glClear(CB)
        gl.glBindFramebuffer(FB, 0)
        out['px'] = [px(8, 8)]
    elif what == 'stencil':                            # stencil in the mask: not deferred
        fbo, tex = make_fbo()
        gl.glClearStencil(5); gl.glClearColor(1, 0, 0, 1); gl.glClear(CB | SB)
        gl.glBindFramebuffer(FB, fbo); gl.glClear(CB)
        gl.glBindFramebuffer(FB, 0)
        gl.glEnable(e['GL_STENCIL_TEST']); gl.glStencilFunc(e['GL_EQUAL'], 5, 0xFF)
        gl.glColor4f(0, 1, 0, 1); quad(-1, -1, 0, 1)
        gl.glDisable(e['GL_STENCIL_TEST'])
        out['px'] = [px(16, 32), px(48, 32)]
    elif what == 'delete':                             # deleting the bound framebuffer reverts to framebuffer 0
        fbo, tex = make_fbo()
        gl.glClearColor(1, 0, 0, 1); gl.glClear(CB)
        gl.glBindFramebuffer(FB, fbo)
        f = ctypes.c_uint32(fbo); gl.glDeleteFramebuffers(1, P(ctypes.pointer(f)))
        gl.glColor4f(0, 1, 0, 1); quad(-0.5, -0.5, 0.5, 0.5)
        out['px'] = [px(32, 32), px(2, 2)]
    elif what == 'dlist':                              # glClear replayed from a display list, then its geometry
        lst = gl.glGenLists(1)
        gl.glNewList(lst, e['GL_COMPILE'])
        gl.glClear(CB); gl.glColor4f(0, 1, 0, 1); quad(-0.5, -0.5, 0.5, 0.5)
        gl.glEndList()
        gl.glClearColor(1, 0, 0, 1); gl.glCallList(lst)
        out['px'] = [px(32, 32), px(2, 2)]
    elif what in ('rearm', 'rect'):                   # deferred clear re-armed, immediate-mode batch, switch away again
        fbo, tex = make_fbo()
        gl.glClearColor(1, 0, 0, 1); gl.glClear(CB)
        gl.glBindFramebuffer(FB, fbo)                  # deferred
        gl.glBindFramebuffer(FB, 0)                    # re-armed, framebuffer 0 not used yet
        gl.glColor4f(0, 1, 0, 1)
        if what == 'rect': gl.glRectf(-0.5, -0.5, 0.5, 0.5)
        else: quad(-0.5, -0.5, 0.5, 0.5)               # batch pending (no entry point ran yet)
        gl.glBindFramebuffer(FB, fbo)                  # the clear must run before the batch is drawn into framebuffer 0
        gl.glBindFramebuffer(FB, 0)
        out['px'] = [px(32, 32), px(2, 2)]
    elif what == 'error':
        gl.glGetError(); gl.glClear(0x00000001)
        out['err'] = gl.glGetError()
    out['err_end'] = gl.glGetError()
    print('CHILD ' + json.dumps(out), flush=True)

def child_sched():
    sys.path.insert(0, HERE)
    from gltest import Ctx, E
    C = Ctx(16, 16); gl = C.gl
    frame = lambda: (gl.glClear(E['GL_COLOR_BUFFER_BIT']), gl.glFlush())
    for _ in range(3): frame()
    me = threading.get_native_id()
    res = {'rt_aff': sorted(os.sched_getaffinity(0)), 'rt_nice': os.getpriority(os.PRIO_PROCESS, me)}
    ev = threading.Event(); info = {}
    def worker():
        tid = threading.get_native_id()
        info['tid'] = tid; info['aff0'] = sorted(os.sched_getaffinity(0)); info['nice'] = os.getpriority(os.PRIO_PROCESS, tid)
        ev.wait(30)
    t = threading.Thread(target=worker); t.start()
    while 'tid' not in info: time.sleep(0.01)
    for _ in range(40): frame()
    res.update(child_aff0=info['aff0'], child_nice=info['nice'], child_aff=sorted(os.sched_getaffinity(info['tid'])),
               rt_aff_after=sorted(os.sched_getaffinity(0)))
    ev.set(); t.join()
    print('CHILD ' + json.dumps(res), flush=True)

if len(sys.argv) > 2 and sys.argv[1] == '--child':
    child_sched() if sys.argv[2] == 'sched' else child(sys.argv[2]); sys.exit(0)

fails = []
def check(ok, msg):
    if not ok: fails.append(msg)
def run(what, extra=None):
    env = dict(os.environ); env['ORYON_CACHE_DIR'] = tempfile.mkdtemp(prefix='oryon-frame-')
    for k in ('ORYON_NO_DEFER_CLEAR', 'ORYON_BIG_CORES', 'ORYON_NO_AFFINITY', 'ORYON_RENDER_NICE'): env.pop(k, None)
    env.update(extra or {})
    p = subprocess.run([sys.executable, os.path.abspath(__file__), '--child', what], env=env, capture_output=True, text=True, timeout=300)
    m = re.search(r'^CHILD (.*)$', p.stdout, re.M)
    return (json.loads(m.group(1)) if m else None), p.stderr

R, G, B, Y = [255, 0, 0], [0, 255, 0], [0, 0, 255], [255, 255, 0]
expect = {
    'mc': lambda o: o['frame'] == [G + [128], B + [128], Y + [255]] and o['restored_color'] == B + [255] and o['restored_depth'] == B + [255],
    'nofbo': lambda o: o['px'][0] == G + [255] and o['px'][1] == R + [255] and o['px'][2] == B + [255] and o['px'][3][:3] == G,
    'read': lambda o: o['px'] == [R + [255], B + [255], R + [255]],
    'scissor': lambda o: o['px'] == [R + [255], [0, 0, 0, 255]],
    'mask': lambda o: o['px'] == [[255, 0, 255, 255]],
    'stencil': lambda o: o['px'] == [G + [255], R + [255]],
    'delete': lambda o: o['px'] == [G + [255], R + [255]],
    'dlist': lambda o: o['px'] == [G + [255], R + [255]],
    'rearm': lambda o: o['px'] == [G + [255], R + [255]],
    'rect': lambda o: o['px'] == [G + [255], R + [255]],
    'error': lambda o: o['err'] == 0x0501,
}
rows = []
for what, ok in expect.items():
    d, ed = run(what)
    i, ei = run(what, {'ORYON_NO_DEFER_CLEAR': '1'})
    check(d is not None and i is not None, '%s: child failed: %s %s' % (what, ed[-300:], ei[-300:]))
    if d and i:
        check(ok(d), '%s: deferred mode pixels wrong: %s' % (what, d))
        check(ok(i), '%s: immediate mode pixels wrong: %s' % (what, i))
        check(d == i, '%s: deferred and immediate modes differ: %s vs %s' % (what, d, i))
        check(d['err_end'] == 0 and i['err_end'] == 0, '%s: GL error left behind' % what)
    rows.append('%s %s' % (what, 'ok' if d and i and ok(d) and d == i else 'FAIL'))
    if what == 'mc':
        check('fb0 clear deferred' in ed and 'fb0 clear immediate' in ei, 'init line must report the clear mode')

# render-thread scheduling (Linux host: 2+ CPUs required for the pin scenario)
ncpu = len(os.sched_getaffinity(0))
s, es_ = run('sched', {'ORYON_BIG_CORES': '0x1'})
check(s is not None, 'sched child failed: %s' % es_[-300:])
if s:
    can_nice = os.geteuid() == 0
    if ncpu >= 2:
        check(s['rt_aff'] == [0] and s['rt_aff_after'] == [0], 'render thread must be pinned to CPU 0: %s' % s)
        check(s['child_aff0'] == [0], 'a thread created by the pinned render thread inherits the pin first: %s' % s)
        check(s['child_aff'] == list(range(ncpu)), 'inherited pin must be removed from new threads: %s' % s)
    if can_nice:
        check(s['rt_nice'] == -10 and s['child_nice'] == 0, 'render thread nice -10, new threads nice 0: %s' % s)
    check(re.search(r'render thread \d+: nice 0 -> -10 \| cpus 0x[0-9a-f]+ -> 0x1, fastest by ORYON_BIG_CORES', es_) is not None or not can_nice or ncpu < 2,
          'scheduling log line missing: %s' % [l for l in es_.splitlines() if 'render thread' in l])
o, eo = run('sched', {'ORYON_BIG_CORES': '0x1', 'ORYON_NO_AFFINITY': '1', 'ORYON_RENDER_NICE': '0'})
check(o is not None and o['rt_aff'] == list(range(ncpu)) and o['rt_nice'] == 0, 'opt-outs must leave the render thread alone: %s' % o)
check('(ORYON_RENDER_NICE=0)' in eo and '(ORYON_NO_AFFINITY)' in eo, 'opt-out reasons must be logged')

print('== FRAME STRUCTURE TEST ==')
print('fb0 clear deferral: ' + ' | '.join(rows))
print('scheduling: %s' % (s and {k: s[k] for k in ('rt_aff', 'rt_nice', 'child_aff0', 'child_aff', 'child_nice')}))
print('RESULT:', 'PASS' if not fails else 'FAIL')
for f in fails: print('  -', f)
sys.exit(1 if fails else 0)
