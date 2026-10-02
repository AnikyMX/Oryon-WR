#!/usr/bin/env python3
# tools/test_perf.py -- ORYON_STATS performance probes on real Mesa EGL + GLES 3.2 (llvmpipe):
#   * GPU pass timeline (EGL fences) vs GL_TIME_ELAPSED_EXT timer on a light and a GPU-heavy scene that use Minecraft's
#     frame shape (clear framebuffer 0, render into an FBO, blit to framebuffer 0): they must agree on the heavy pass,
#     scale with load, attribute the time to the right framebuffer, and classify the frames (gpu-bound or not);
#   * CPU sampler attribution with tools/bench/perfprobe.c (app / Oryon / driver phases);
#   * opt-outs ORYON_STATS_GPU=0 and ORYON_STATS_PROFILE=0, unchanged pixels and no GL error with the probes on.
import os, sys, re, json, statistics, subprocess, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)

def child(kind, seconds):
    sys.path.insert(0, HERE)
    import ctypes
    from gltest import Ctx, E, P
    W = 256
    C = Ctx(W, W); gl = C.gl; e = E
    S = 1024 if kind == 'heavy' else 128
    tex = ctypes.c_uint32(0); gl.glGenTextures(1, P(ctypes.pointer(tex)))
    gl.glBindTexture(e['GL_TEXTURE_2D'], tex.value)
    gl.glTexImage2D(e['GL_TEXTURE_2D'], 0, e['GL_RGBA'], S, S, 0, e['GL_RGBA'], e['GL_UNSIGNED_BYTE'], None)
    for pn in ('GL_TEXTURE_MIN_FILTER', 'GL_TEXTURE_MAG_FILTER'): gl.glTexParameteri(e['GL_TEXTURE_2D'], e[pn], e['GL_LINEAR'])
    fbo = ctypes.c_uint32(0); gl.glGenFramebuffers(1, P(ctypes.pointer(fbo)))
    gl.glBindFramebuffer(e['GL_FRAMEBUFFER'], fbo.value)
    gl.glFramebufferTexture2D(e['GL_FRAMEBUFFER'], e['GL_COLOR_ATTACHMENT0'], e['GL_TEXTURE_2D'], tex.value, 0)
    gl.glBindFramebuffer(e['GL_FRAMEBUFFER'], 0)
    gl.glMatrixMode(e['GL_PROJECTION']); gl.glLoadIdentity(); gl.glOrtho(0, 1, 0, 1, -1, 1)
    gl.glMatrixMode(e['GL_MODELVIEW']); gl.glLoadIdentity()
    def quad(x0, y0, x1, y1):
        gl.glBegin(e['GL_QUADS'])
        gl.glTexCoord2f(0, 0); gl.glVertex2f(x0, y0); gl.glTexCoord2f(1, 0); gl.glVertex2f(x1, y0)
        gl.glTexCoord2f(1, 1); gl.glVertex2f(x1, y1); gl.glTexCoord2f(0, 1); gl.glVertex2f(x0, y1)
        gl.glEnd()
    t_end = time.time() + seconds; frames = 0
    while time.time() < t_end:
        gl.glClearColor(0, 0, 0, 1); gl.glClear(e['GL_COLOR_BUFFER_BIT'] | e['GL_DEPTH_BUFFER_BIT'])      # frame start
        gl.glBindFramebuffer(e['GL_FRAMEBUFFER'], fbo.value); gl.glViewport(0, 0, S, S)                     # "world" pass
        gl.glClearColor(0.1, 0.2, 0.3, 1); gl.glClear(e['GL_COLOR_BUFFER_BIT'])
        gl.glBindTexture(e['GL_TEXTURE_2D'], 0)
        if kind == 'heavy':
            gl.glEnable(e['GL_BLEND']); gl.glBlendFunc(e['GL_SRC_ALPHA'], e['GL_ONE_MINUS_SRC_ALPHA'])
            for i in range(40): gl.glColor4f(1, 0.5, 0.25, 0.05); quad(0, 0, 1, 1)
            gl.glDisable(e['GL_BLEND'])
        else:
            gl.glColor4f(1, 0.5, 0.25, 1); quad(0.4, 0.4, 0.6, 0.6)
        gl.glBindFramebuffer(e['GL_FRAMEBUFFER'], 0); gl.glViewport(0, 0, W, W)                             # blit pass
        gl.glEnable(e['GL_TEXTURE_2D']); gl.glBindTexture(e['GL_TEXTURE_2D'], tex.value); gl.glColor4f(1, 1, 1, 1)
        quad(0, 0, 1, 1)
        gl.glDisable(e['GL_TEXTURE_2D'])
        C.egl.swap(); frames += 1
        time.sleep(0.02 if kind == 'light' else 0.005)
    gl.glFinish()
    print('CHILD ' + json.dumps({'frames': frames, 'fbo': fbo.value, 'px': C.px(W // 2, W // 2), 'err': gl.glGetError()}), flush=True)

if len(sys.argv) > 1 and sys.argv[1] == '--child':
    child(sys.argv[2], float(sys.argv[3])); sys.exit(0)

fails = []
def check(ok, msg):
    if not ok: fails.append(msg)
def run(kind, seconds, extra=None):
    env = dict(os.environ); env.pop('ORYON_STATS', None); env['ORYON_STATS'] = '1'
    env['ORYON_CACHE_DIR'] = tempfile.mkdtemp(prefix='oryon-perf-')
    env.update(extra or {})
    p = subprocess.run([sys.executable, os.path.abspath(__file__), '--child', kind, str(seconds)], env=env,
                       capture_output=True, text=True, timeout=300)
    m = re.search(r'^CHILD (.*)$', p.stdout, re.M)
    return p.returncode, (json.loads(m.group(1)) if m else None), p.stderr

N = r'([0-9.]+)'
CPU = re.compile(r'\] cpu ' + N + r's: frame ' + N + r' ms \| render thread ' + N + r' ms/f \((\d+)%, cpu-bound (\d+)%\) \[(.*?)\] \| process '
                 + N + r' cores \| calls/f prog ' + N + r' unif ' + N + r' tex ' + N + r' vao ' + N + r' buf ' + N + r' attr ' + N)
SMP = re.compile(r'oryon (\d+) gl (\d+) jvm (\d+) java (\d+) libc (\d+) other (\d+) %, (\d+) ms sampled')
GPU = re.compile(r'\] gpu ' + N + r's: busy ' + N + r' ms/f \(max ' + N + r', gpu-bound (\d+)%\) \[(.*?)\] \| timer ' + N + r' ms/f \+ tail <=' + N
                 + r'.*? \| lag ' + N + r' ms \(max ' + N + r'\) \| fence floor ' + N + r' ms \| (\d+) frames')
def parse(err):
    cpu = [m for m in (CPU.search(l) for l in err.splitlines()) if m][1:]           # first window: start-up
    gpu = [m for m in (GPU.search(l) for l in err.splitlines()) if m][1:]
    return cpu, gpu
def fbsplit(s):
    return {int(a): float(b) for a, b in re.findall(r'fb(\d+) ([0-9.]+)', s)}
med = lambda v: statistics.median(v) if v else 0.0

# ---- light vs heavy scenes
rcl, rl, el = run('light', 4.2)
rch, rh, eh = run('heavy', 4.2)
check(rcl == 0 and rl and rch == 0 and rh, 'child failed: %s %s' % (el[-300:], eh[-300:]))
check('perf: gpu fence timeline EGL_KHR_fence_sync | gpu timer TIME_ELAPSED' in el, 'perf init line missing or GPU probes unavailable on Mesa')
cl, gl_ = parse(el); ch, gh = parse(eh)
check(len(cl) >= 2 and len(gl_) >= 2 and len(ch) >= 2 and len(gh) >= 2, 'expected >= 2 cpu+gpu windows per scene (%d %d %d %d)' % (len(cl), len(gl_), len(ch), len(gh)))
res = {}
if gl_ and gh and rl and rh:
    busy_l = med([float(m.group(2)) for m in gl_]); busy_h = med([float(m.group(2)) for m in gh])
    tim_h = med([float(m.group(6)) for m in gh]); tim_l = med([float(m.group(6)) for m in gl_])
    bound_h = med([int(m.group(4)) for m in gh]); bound_l = med([int(m.group(4)) for m in gl_])
    lag_l = med([float(m.group(8)) for m in gl_])
    fb_h = fbsplit(gh[-1].group(5)); fb_l = fbsplit(gl_[-1].group(5))
    res = dict(busy_light=busy_l, busy_heavy=busy_h, timer_light=tim_l, timer_heavy=tim_h, gpu_bound_heavy=bound_h, gpu_bound_light=bound_l,
               lag_light=lag_l, fb_heavy=fb_h)
    check(busy_h >= 20.0 and busy_h >= 10 * max(busy_l, 0.1), 'GPU busy must scale with load: light %.1f heavy %.1f ms/f' % (busy_l, busy_h))
    check(abs(tim_h - busy_h) <= 0.25 * busy_h, 'fence timeline and GPU timer disagree on the heavy pass: %.1f vs %.1f ms/f' % (busy_h, tim_h))
    check(tim_l <= 2.0, 'light scene timer should be tiny: %.1f ms/f' % tim_l)
    check(bound_h >= 50 and bound_l <= 10, 'gpu-bound classification: heavy %s%% light %s%%' % (bound_h, bound_l))
    check(lag_l <= 2.0, 'light scene: GPU should keep up (lag %.1f ms)' % lag_l)
    tot = sum(fb_h.values())
    check(tot > 0 and fb_h.get(rh['fbo'], 0) >= 0.8 * tot, 'heavy GPU time must be attributed to the FBO pass fb%d: %s' % (rh['fbo'], fb_h))
    check(set(fb_l) >= {0, rl['fbo']}, 'light scene should list fb0 and fb%d: %s' % (rl['fbo'], fb_l))
if cl:
    m = cl[-1]
    check(float(m.group(8)) >= 1 and float(m.group(10)) >= 1, 'state counters: expected program and texture calls per frame')
    s = SMP.search(m.group(6))
    check(s is not None and 95 <= sum(int(x) for x in s.groups()[:6]) <= 105, 'sample shares must sum to ~100%%: %s' % m.group(6))
    fps_ms = [float(x.group(2)) for x in cl]
    check(all(15.0 <= v <= 40.0 for v in fps_ms), 'light frame time should reflect the 20 ms sleep: %s' % fps_ms)
check(rl and rh and rl['px'] == [255, 128, 64, 255] and rl['err'] == 0 and rh['err'] == 0, 'rendering/GL error with probes on: %s %s' % (rl, rh))

# ---- opt-outs
rco, ro, eo = run('light', 2.3, {'ORYON_STATS_GPU': '0', 'ORYON_STATS_PROFILE': '0'})
check(rco == 0 and ro and ro['px'] == [255, 128, 64, 255] and ro['err'] == 0, 'opt-out run failed: %s' % eo[-300:])
check('busy n/a (off (ORYON_STATS_GPU=0))' in eo and 'timer n/a' in eo, 'ORYON_STATS_GPU=0 must disable the GPU probes')
check('sampler off (ORYON_STATS_PROFILE=0)' in eo, 'ORYON_STATS_PROFILE=0 must disable the CPU sampler')

# ---- CPU sampler attribution (C probe: app / oryon / gl phases)
probe = os.path.join(ROOT, 'tools', 'bench', 'perfprobe'); src = probe + '.c'
if not os.path.exists(probe) or os.path.getmtime(probe) < os.path.getmtime(src):
    subprocess.run(['cc', '-O2', '-o', probe, src, '-ldl', '-lEGL'], check=True)
shim = os.path.join(ROOT, 'tools', 'bench', 'drawshim.so')
if not os.path.exists(shim):
    subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-o', shim, shim[:-3] + '.c', '-ldl'], check=True)
so = os.path.join(ROOT, os.environ.get('ORYON_SO', 'build-host/liboryon.so'))
env = dict(os.environ, ORYON_STATS='1', ORYON_GLES_LIB=shim, SHIM_NODRAW='1', ORYON_CACHE_DIR=tempfile.mkdtemp(prefix='oryon-probe-'))
p = subprocess.run([probe, so, '3.2'], env=env, capture_output=True, text=True, timeout=300)
phase = None; seen = {}; shares = {}
for line in p.stderr.splitlines():
    mm = re.match(r'PHASE (\w+)', line)
    if mm: phase = mm.group(1); seen[phase] = 0; continue
    m = CPU.search(line)
    if m and phase in ('app', 'oryon', 'gl'):
        seen[phase] += 1
        if seen[phase] == 1: continue                     # window straddles the phase change
        s = SMP.search(m.group(6))
        if s: shares.setdefault(phase, []).append(dict(zip(('oryon', 'gl', 'jvm', 'java', 'libc', 'other'), map(int, s.groups()[:6]))))
pick = lambda ph, k: med([d[k] for d in shares.get(ph, [])])
check(p.returncode == 0 and 'PHASE end err=0x0' in p.stderr, 'perfprobe failed: %s' % p.stderr[-300:])
check(pick('app', 'other') >= 80, 'app phase should be "other": %s' % shares.get('app'))
check(pick('oryon', 'oryon') >= 50, 'immediate-mode phase should be "oryon": %s' % shares.get('oryon'))
check(pick('gl', 'gl') >= 70, 'texture-upload phase should be "gl": %s' % shares.get('gl'))

print('== PERF PROBES TEST ==')
print('gpu busy light %.1f / heavy %.1f ms/f | timer light %.1f / heavy %.1f | gpu-bound heavy %s%% light %s%% | lag light %.1f ms | heavy split %s'
      % (res.get('busy_light', 0), res.get('busy_heavy', 0), res.get('timer_light', 0), res.get('timer_heavy', 0),
         res.get('gpu_bound_heavy', 0), res.get('gpu_bound_light', 0), res.get('lag_light', 0), res.get('fb_heavy')))
print('cpu sampler: app other %s%% | oryon phase oryon %s%% | gl phase gl %s%%' % (pick('app', 'other'), pick('oryon', 'oryon'), pick('gl', 'gl')))
print('RESULT:', 'PASS' if not fails else 'FAIL')
for f in fails: print('  -', f)
sys.exit(1 if fails else 0)
