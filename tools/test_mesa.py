#!/usr/bin/env python3
# tools/test_mesa.py -- real-driver test: Mesa EGL + GLES 3.2 (llvmpipe). Loads liboryon.so exactly like the
# launcher/LWJGL: dlopen() + dlsym(). Simulates LWJGL 3.3.6 GL.create()/createCapabilities() from the jar DB.
import os, sys, re, json, ctypes
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eglctx, jvm
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB = os.path.join(ROOT, 'tools', 'db'); J = lambda n: json.load(open(os.path.join(DB, n)))
E = J('enums.json'); caps = J('caps.json'); F = caps['features']
SO = os.path.join(ROOT, os.environ.get('ORYON_SO', 'build-host/liboryon.so'))
LW_JAR = os.environ.get('ORYON_LWJGL_JAR', '/root/.claude/uploads/af115f22-38a8-5494-9373-115a23ce2354/98d7d626-lwjgl-glfw-classes.jar')
fails = []
def check(cond, what):
    if not cond: fails.append(what)
    return cond

lib = ctypes.CDLL(SO, mode=ctypes.RTLD_LOCAL)            # dlopen
def sym(n):                                              # dlsym
    try: return getattr(lib, n)
    except AttributeError: return None

# --- GL.create(): GetProcAddress chain on LINUX (from GL$1 bytecode): all must be absent -> pure dlsym path
gpa = [n for n in ('glXGetProcAddress', 'glXGetProcAddressARB', 'eglGetProcAddress', 'OSMesaGetProcAddress') if sym(n)]
check(not gpa, 'GetProcAddress symbol exported: %s' % gpa)

ctx = eglctx.Context(64, 64)
P = ctypes.c_void_p; I = ctypes.c_int
glGetError = sym('glGetError'); glGetError.restype = ctypes.c_uint
glGetString = sym('glGetString'); glGetString.restype = ctypes.c_char_p; glGetString.argtypes = [ctypes.c_uint]
glGetIntegerv = sym('glGetIntegerv'); glGetIntegerv.argtypes = [ctypes.c_uint, ctypes.POINTER(I)]
glGetStringi = sym('glGetStringi'); glGetStringi.restype = ctypes.c_char_p; glGetStringi.argtypes = [ctypes.c_uint, ctypes.c_uint]
def geti(pn):
    v = I(-1); glGetIntegerv(E[pn], ctypes.byref(v)); return v.value

# --- createCapabilities(forwardCompatible=false)
check(glGetError() == 0, 'context in error state before caps')
ver = glGetString(E['GL_VERSION'])
check(ver is not None, 'no GL_VERSION')
lwj = jvm.load_jar(LW_JAR, want_code=True, prefix='org/lwjgl/system/APIUtil')
A = lwj['org/lwjgl/system/APIUtil']
pat = [A.const(a) for m in A.methods if m.code for pc, op, a in jvm.decode(m.code) if op in (0x12, 0x13) and isinstance(A.const(a), str) and 'd+' in A.const(a)][0]
mv = re.search(pat, ver.decode())
check(mv is not None, 'GL_VERSION not parseable by LWJGL APIUtil pattern')
major, minor = (int(mv.group(1)), int(mv.group(2))) if mv else (0, 0)
if major >= 3:
    check(geti('GL_MAJOR_VERSION') == major and geti('GL_MINOR_VERSION') == minor, 'GL_MAJOR/MINOR mismatch')
VERS = [(1, 0), (1, 1), (1, 2), (1, 3), (1, 4), (1, 5), (2, 0), (2, 1), (3, 0), (3, 1), (3, 2), (3, 3), (4, 0), (4, 1), (4, 2), (4, 3), (4, 4), (4, 5), (4, 6)]
supported = {'OpenGL%d%d' % v for v in VERS if v <= (major, minor)}
if major >= 3:
    n = geti('GL_NUM_EXTENSIONS'); exts = [glGetStringi(E['GL_EXTENSIONS'], i) for i in range(n)]
    check(all(exts), 'glGetStringi returned NULL'); supported |= {e.decode() for e in exts if e}
else:
    supported |= set(glGetString(E['GL_EXTENSIONS']).decode().split())
fc = bool(geti('GL_CONTEXT_FLAGS') & E['GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT'])
check(not fc, 'forward-compatible context reported')
# Flags follow the jar's own semantics (caps.json 'semantics', derived from bytecode): with the Pojav-family
# Checks, a feature is flagged when advertised (entry points are not checked) and GL30..GL33 are always flagged.
flags = {}; null_adv = []; null_always = []
for feat, d in F.items():
    token = ('OpenGL' + feat[2:]) if re.match(r'^GL\d\d$', feat) else ('GL_' + feat)
    fns_ok = all(sym(x) for l in d['lists'] for x in l)
    if token in supported:
        flags[token] = fns_ok if d.get('needs_fns', True) else True
        if not fns_ok: null_adv.append(token)
    elif d.get('always'):
        flags[token] = True
        if not fns_ok: null_always.append(token)
for tok in supported:
    if tok.startswith('GL_') and tok[3:] not in F: flags[tok] = True     # extension without entry points
check(not null_adv, 'advertised features with NULL entry points: %s' % null_adv)
render = J('mc_caps_read.json')['render_path']
missing_flags = [f for f in render if not flags.get(f)]
check(not missing_flags, 'MC render-path flags false: %s' % missing_flags)
check(glGetError() == 0, 'GL error after createCapabilities')

# --- passthrough rendering smoke test (real llvmpipe rasterization)
f4 = ctypes.c_float
sym('glViewport').argtypes = [I, I, I, I]
sym('glClearColor').argtypes = [f4, f4, f4, f4]
sym('glClear').argtypes = [ctypes.c_uint]
sym('glReadPixels').argtypes = [I, I, I, I, ctypes.c_uint, ctypes.c_uint, P]
sym('glViewport')(0, 0, 64, 64)
sym('glClearColor')(0.25, 0.5, 0.75, 1.0)
sym('glClear')(E['GL_COLOR_BUFFER_BIT'])
px = (ctypes.c_ubyte * 4)()
sym('glReadPixels')(32, 32, 1, 1, E['GL_RGBA'], E['GL_UNSIGNED_BYTE'], ctypes.cast(px, P))
check(abs(px[0] - 64) <= 1 and abs(px[1] - 128) <= 1 and abs(px[2] - 191) <= 1, 'clear color readback %s' % list(px))
check(glGetError() == 0, 'GL error after smoke test')

print('== MESA TEST (EGL %d.%d, %s) ==' % (ctx.egl_version[0], ctx.egl_version[1], glGetString(E['GL_RENDERER']).decode()))
print('GL_VERSION "%s" -> LWJGL parse %d.%d | exts %d | fc=%s' % (ver.decode(), major, minor, len([s for s in supported if s.startswith('GL_')]), fc))
print('LWJGL flags true: %d/%d (always-true without entry points: %s) | MC render-path flags true: %d/%d' % (
      sum(flags.values()), len(flags), ' '.join(sorted(null_always)) or 'none', len(render) - len(missing_flags), len(render)))
print('GetProcAddress exports: %s | clear readback: %s' % (gpa or 'none (dlsym path)', list(px)))
ctx.close()
print('RESULT:', 'PASS' if not fails else 'FAIL')
for f_ in fails: print('  -', f_)
sys.exit(1 if fails else 0)
