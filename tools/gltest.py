# tools/gltest.py -- test helpers: load liboryon.so via dlopen/dlsym (ctypes), Mesa EGL GLES 3.2 context,
# GL entry points typed automatically from the jar-derived signature DB (tools/db/sigs.json).
import os, sys, json, ctypes, struct, math
import tempfile
# Hermetic program cache per test process (exercises compile + store); tests may override ORYON_CACHE_DIR.
os.environ.setdefault('ORYON_CACHE_DIR', tempfile.mkdtemp(prefix='oryon-cache-'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eglctx
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB = os.path.join(ROOT, 'tools', 'db')
E = json.load(open(os.path.join(DB, 'enums.json')))
SIGS = json.load(open(os.path.join(DB, 'sigs.json')))
_T = {'I': ctypes.c_int32, 'J': ctypes.c_void_p, 'F': ctypes.c_float, 'D': ctypes.c_double, 'S': ctypes.c_int16,
      'B': ctypes.c_int8, 'Z': ctypes.c_uint8, 'C': ctypes.c_uint16}
_R = dict(_T); _R['V'] = None

class GL:
    def __init__(self, so):
        self.lib = ctypes.CDLL(so, mode=ctypes.RTLD_LOCAL)
        self._c = {}
    def __getattr__(self, n):
        if n in self._c: return self._c[n]
        f = getattr(self.lib, n)
        s = SIGS[n]
        f.argtypes = [_T[t] for t in s['params']]
        f.restype = _R[s['ret']]
        self._c[n] = f
        return f

def P(x): return ctypes.cast(x, ctypes.c_void_p)
def farr(*v): return (ctypes.c_float * len(v))(*v)
def darr(*v): return (ctypes.c_double * len(v))(*v)

class Ctx:
    def __init__(self, w=64, h=64):
        self.egl = eglctx.Context(w, h)
        so = os.path.join(ROOT, os.environ.get('ORYON_SO', 'build-host/liboryon.so'))
        self.gl = GL(so)
        self.w, self.h = w, h
    def px(self, x, y):
        buf = (ctypes.c_ubyte * 4)()
        self.gl.glReadPixels(x, y, 1, 1, E['GL_RGBA'], E['GL_UNSIGNED_BYTE'], P(buf))
        return tuple(buf)
    def close(self): self.egl.close()

def close(a, b, tol=3): return all(abs(int(x) - int(y)) <= tol for x, y in zip(a, b))

# column-major matrix helpers (reference math for assertions)
def mmul(a, b):
    r = [0.0] * 16
    for c in range(4):
        for i in range(4): r[c * 4 + i] = sum(a[k * 4 + i] * b[c * 4 + k] for k in range(4))
    return r
def ident(): return [1.0 if i % 5 == 0 else 0.0 for i in range(16)]
def trans(x, y, z): m = ident(); m[12], m[13], m[14] = x, y, z; return m
def scale(x, y, z): m = ident(); m[0], m[5], m[10] = x, y, z; return m
def rot(a, x, y, z):
    l = math.sqrt(x * x + y * y + z * z); x, y, z = x / l, y / l, z / l
    c, s = math.cos(math.radians(a)), math.sin(math.radians(a)); ic = 1 - c
    return [x*x*ic+c, y*x*ic+z*s, x*z*ic-y*s, 0, x*y*ic-z*s, y*y*ic+c, y*z*ic+x*s, 0, x*z*ic+y*s, y*z*ic-x*s, z*z*ic+c, 0, 0, 0, 0, 1]
