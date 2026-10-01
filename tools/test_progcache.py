#!/usr/bin/env python3
# tools/test_progcache.py -- persistent FFP program cache on real Mesa GLES 3.2:
#   run 1 compiles and stores, run 2 loads every program at init and compiles nothing while drawing (same pixels),
#   corrupt/stale files are dropped/rebuilt, app GL errors survive cache misses, ORYON_NO_PROGRAM_CACHE disables it.
import os, sys, re, glob, json, shutil, subprocess, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__))

def child(seconds):
    sys.path.insert(0, HERE)
    import ctypes
    from gltest import Ctx, E, P
    C = Ctx(32, 32); gl = C.gl; e = E
    gl.glMatrixMode(e['GL_PROJECTION']); gl.glLoadIdentity(); gl.glOrtho(0, 32, 0, 32, -10, 10)
    gl.glMatrixMode(e['GL_MODELVIEW']); gl.glLoadIdentity()
    tex = ctypes.c_uint32(0); gl.glGenTextures(1, P(ctypes.pointer(tex)))
    gl.glBindTexture(e['GL_TEXTURE_2D'], tex.value)
    px = (ctypes.c_uint32 * 4)(0xFF2040C0, 0xFF2040C0, 0xFF2040C0, 0xFF2040C0)
    gl.glTexImage2D(e['GL_TEXTURE_2D'], 0, e['GL_RGBA'], 2, 2, 0, e['GL_BGRA'], e['GL_UNSIGNED_INT_8_8_8_8_REV'], P(px))
    for pn in ('GL_TEXTURE_MIN_FILTER', 'GL_TEXTURE_MAG_FILTER'): gl.glTexParameteri(e['GL_TEXTURE_2D'], e[pn], e['GL_NEAREST'])
    def quad(x0, x1):
        gl.glBegin(e['GL_QUADS'])
        for (x, y, s, t) in [(x0, 0, 0, 0), (x1, 0, 1, 0), (x1, 32, 1, 1), (x0, 32, 0, 1)]:
            gl.glNormal3f(0, 0, 1); gl.glTexCoord2f(s, t); gl.glVertex3f(x, y, 0)
        gl.glEnd()
    def scene():                                   # four distinct FFP keys
        gl.glClear(e['GL_COLOR_BUFFER_BIT'])
        gl.glDisable(e['GL_TEXTURE_2D']); gl.glDisable(e['GL_FOG']); gl.glDisable(e['GL_LIGHTING']); gl.glDisable(e['GL_ALPHA_TEST'])
        gl.glColor4f(1, 0, 0, 1); quad(0, 8)
        gl.glEnable(e['GL_TEXTURE_2D']); gl.glColor4f(1, 1, 1, 1); quad(8, 16)
        gl.glEnable(e['GL_FOG']); gl.glFogi(e['GL_FOG_MODE'], e['GL_LINEAR']); gl.glFogf(e['GL_FOG_START'], 0); gl.glFogf(e['GL_FOG_END'], 100)
        quad(16, 24)
        gl.glEnable(e['GL_LIGHTING']); gl.glEnable(e['GL_LIGHT0']); gl.glEnable(e['GL_COLOR_MATERIAL']); gl.glEnable(e['GL_ALPHA_TEST'])
        gl.glAlphaFunc(e['GL_GREATER'], 0.1); quad(24, 32)
    t_end = time.time() + seconds
    scene(); out = [C.px(x, 16) for x in (4, 12, 20, 28)]
    while time.time() < t_end: scene(); time.sleep(0.02)
    gl.glDisable(e['GL_LIGHTING']); gl.glDisable(e['GL_FOG']); gl.glDisable(e['GL_ALPHA_TEST'])
    gl.glEnable(0x7FFFFFFF)                        # leaves GL_INVALID_ENUM pending in the driver
    gl.glShadeModel(e['GL_FLAT']); quad(0, 4)      # new key -> cache miss path while the app error is pending
    err1, err2 = gl.glGetError(), gl.glGetError()
    print('CHILD ' + json.dumps({'px': out, 'err': [err1, err2]}), flush=True)

if len(sys.argv) > 1 and sys.argv[1] == '--child':
    child(float(sys.argv[2])); sys.exit(0)

fails = []
def check(ok, msg):
    if not ok: fails.append(msg)
def run(extra, seconds=0.0):
    env = dict(os.environ); env.update(extra)
    p = subprocess.run([sys.executable, os.path.abspath(__file__), '--child', str(seconds)], env=env, capture_output=True, text=True, timeout=180)
    m = re.search(r'^CHILD (.*)$', p.stdout, re.M)
    return p.returncode, (json.loads(m.group(1)) if m else None), p.stderr
def cache_line(err):
    m = re.search(r'program cache (\S+): (\d+) loaded, (\d+) rebuilt, (\d+) dropped', err)
    return tuple(int(x) for x in m.groups()[1:]) if m else None

D = tempfile.mkdtemp(prefix='oryon-pc-')
rc1, r1, e1 = run({'ORYON_CACHE_DIR': D})
files = sorted(glob.glob(os.path.join(D, 'ffp-*.bin')))
check(rc1 == 0 and r1, 'run 1 failed: %s' % e1[-300:])
check(cache_line(e1) == (0, 0, 0), 'run 1 should start with an empty cache: %s' % (cache_line(e1),))
check(len(files) >= 5, 'expected >= 5 cached programs after run 1, got %d' % len(files))
check(r1 and r1['err'] == [0x0500, 0], 'app GL error must survive a cache miss: %s' % (r1 and r1['err']))

rc2, r2, e2 = run({'ORYON_CACHE_DIR': D, 'ORYON_STATS': '1'}, 2.2)
cl = cache_line(e2)
check(rc2 == 0 and r2, 'run 2 failed: %s' % e2[-300:])
check(cl == (len(files), 0, 0), 'run 2 should load all %d programs at init: %s' % (len(files), cl))
stats = re.findall(r'\] stats [0-9.]+s: .*?ffp prog \+(\d+)', e2)
check(len(stats) >= 1 and all(int(x) == 0 for x in stats), 'run 2 must not compile FFP programs while drawing: %s' % stats)
check(r1 and r2 and r1['px'] == r2['px'], 'pixels differ between compiled and cached programs: %s vs %s' % (r1 and r1['px'], r2 and r2['px']))

with open(files[0], 'r+b') as f: f.truncate(20)                       # corrupt
with open(files[1], 'r+b') as f: f.seek(24); b = f.read(1); f.seek(24); f.write(bytes([b[0] ^ 0xFF]))   # stale source hash
rc3, r3, e3 = run({'ORYON_CACHE_DIR': D})
cl3 = cache_line(e3)
check(rc3 == 0 and r3 and r3['px'] == r1['px'], 'run 3 rendering changed after corrupt/stale files')
check(cl3 is not None and cl3[1] == 1 and cl3[2] == 1 and cl3[0] == len(files) - 2, 'run 3 expected 1 rebuilt + 1 dropped: %s' % (cl3,))
check(len(glob.glob(os.path.join(D, 'ffp-*.bin'))) == len(files), 'dropped program must be recompiled and stored again')
check(not glob.glob(os.path.join(D, '*.tmp')), 'temporary files left behind')

D2 = tempfile.mkdtemp(prefix='oryon-pc-off-')
rc4, r4, e4 = run({'ORYON_CACHE_DIR': D2, 'ORYON_NO_PROGRAM_CACHE': '1'})
check(rc4 == 0 and r4 and r4['px'] == r1['px'] and 'program cache' not in e4 and not os.listdir(D2), 'ORYON_NO_PROGRAM_CACHE=1 must disable the cache')
shutil.rmtree(D, ignore_errors=True); shutil.rmtree(D2, ignore_errors=True)

print('== PROGRAM CACHE TEST ==')
print('run1: %d programs stored | run2 init: %s | run3 init: %s | pixels %s' % (len(files), cl, cl3, r1 and r1['px']))
print('RESULT:', 'PASS' if not fails else 'FAIL')
for f in fails: print('  -', f)
sys.exit(1 if fails else 0)
