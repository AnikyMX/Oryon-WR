#!/usr/bin/env python3
# tools/validate.py -- programmatic cross-reference of src/**/*.cpp|hpp against the jars (tools/db) + ES headers.
# Prints a short report only (no raw jar data).
import os, re, sys, json, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import khr, jvm, dlspec
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB = os.path.join(ROOT, 'tools', 'db'); SRC = os.path.join(ROOT, 'src')
J = lambda n: json.load(open(os.path.join(DB, n)))
sigs = J('sigs.json'); enums = J('enums.json'); caps = J('caps.json'); F = caps['features']
mc_req = set(J('mc_required_gl.json')); rep = J('gen_report.json'); cat = rep['cat']
ES = khr.gles32(); ESDEF = khr.gl_defines()
LW_JAR = os.environ.get('ORYON_LWJGL_JAR', '/root/.claude/uploads/af115f22-38a8-5494-9373-115a23ce2354/98d7d626-lwjgl-glfw-classes.jar')
lw = jvm.load_jar(LW_JAR, want_code=False, prefix='org/lwjgl/opengl/') if os.path.exists(LW_JAR) else None
errors = []; warns = []

def files():
    for root, _, fs in os.walk(SRC):
        for f in fs:
            if f.endswith(('.cpp', '.hpp', '.inc')): yield os.path.join(root, f)

EXP_RE = re.compile(r'(/\*\s*jar:\s*(?P<ref>[^*]+?)\s*\*/\s*)?OGL_EXPORT\s+(?P<ret>[\w\s\*]+?)\s*\b(?P<name>gl\w+)\s*\((?P<params>[^)]*)\)\s*\{')
exports = {}
for p in files():
    for m in EXP_RE.finditer(open(p).read()):
        n = m.group('name')
        if n in exports: errors.append('duplicate export ' + n)
        exports[n] = (os.path.relpath(p, ROOT), m.group('ret').strip(), m.group('params').strip(), m.group('ref'))

def cat_of(t):
    t = t.replace('const', ' ').strip()
    if '*' in t or t in ('GLsizeiptr', 'GLintptr', 'GLint64', 'GLuint64', 'GLsync', 'GLDEBUGPROC', 'GLsizeiptrARB',
                         'GLintptrARB', 'GLint64EXT', 'GLuint64EXT', 'GLDEBUGPROCARB', 'GLDEBUGPROCAMD', 'GLeglImageOES'):
        return 'J'
    t = t.split()[-1] if t.split() else t
    return {'GLenum': 'I', 'GLint': 'I', 'GLuint': 'I', 'GLsizei': 'I', 'GLbitfield': 'I', 'GLhandleARB': 'I',
            'int': 'I', 'unsigned': 'I', 'GLfloat': 'F', 'GLclampf': 'F', 'float': 'F', 'GLdouble': 'D', 'GLclampd': 'D',
            'double': 'D', 'GLshort': 'S', 'GLushort': 'S', 'GLhalf': 'S', 'GLhalfARB': 'S', 'GLbyte': 'B', 'GLubyte': 'B',
            'GLchar': 'B', 'GLcharARB': 'B', 'GLboolean': 'Z', 'void': 'V'}.get(t, '?')
JCAT = {'I': 'I', 'J': 'J', 'F': 'F', 'D': 'D', 'S': 'S', 'C': 'S', 'B': 'B', 'Z': 'Z', 'V': 'V'}
def ptype(p):
    p = p.strip()
    if p in ('', 'void'): return None
    m = re.match(r'^(.*?)(\w+)\s*$', p)
    return m.group(1).strip() if m and m.group(1).strip() else p
def norm(t):
    t = re.sub(r'\s+', ' ', t.replace('GLvoid', 'void')).strip()
    base = re.sub(r'\bconst\b|\*', ' ', t).split()
    return (' '.join(base), t.count('*'), 'const' in t)

abi_ok = 0; nt_mismatch = 0; ref_ok = 0; ref_bad = 0
for n, (f, ret, params, ref) in sorted(exports.items()):
    if n not in sigs: errors.append('%s: not a GL function in LWJGL jar' % n); continue
    s = sigs[n]
    pl = [x for x in (params.split(',') if params and params != 'void' else []) if x.strip()]
    pt = [ptype(x) for x in pl]
    if len(pt) != len(s['params']): errors.append('%s: arity %d != jar %d' % (n, len(pt), len(s['params']))); continue
    bad = False
    for i, (ct, jt) in enumerate(zip(pt, s['params'])):
        if cat_of(ct) != JCAT.get(jt, '?'): errors.append('%s: param %d C "%s" vs JNI %s' % (n, i, ct, jt)); bad = True
    rc = cat_of(ret); rj = JCAT.get(s['ret'], '?')
    if rc != rj and not (rj == 'J' and rc == 'J'): errors.append('%s: return C "%s" vs JNI %s' % (n, ret, s['ret'])); bad = True
    if not bad: abi_ok += 1
    if s['pnt']:
        for i, (ct, nt) in enumerate(zip(pt, s['pnt'])):
            if nt and norm(ct)[:2] != norm(nt)[:2]: nt_mismatch += 1
    if n in ES:
        er, ep = ES[n]
        if len(ep) != len(pt) or any(cat_of(ptype(a)) != cat_of(b) for a, b in zip(ep, pt)) or cat_of(er) != rc:
            errors.append('%s: signature differs from GLES prototype' % n)
    if ref and lw is not None:
        head = ref.split('->')[0].strip()
        m = re.match(r'^(\w+)\.(\w+)(\(.*)$', head)
        ok = False
        if m:
            cf = lw.get('org/lwjgl/opengl/' + m.group(1))
            ok = cf is not None and cf.method(m.group(2), m.group(3)) is not None
        if ok: ref_ok += 1
        else: ref_bad += 1; errors.append('%s: jar reference not found (%s)' % (n, head))

# strict: same-name ES functions must match the ES prototype exactly (safe even if headers expose prototypes)
def _nt(p):
    p = p.strip()
    if p in ('', 'void'): return None
    m = re.match(r'^(.*?)(\w+)\s*$', p); t = m.group(1) if m and m.group(1).strip() else p
    t = t.replace('GLvoid', 'void')
    return (' '.join(re.sub(r'\bconst\b|\*', ' ', t).split()), t.count('*'), len(re.findall(r'\bconst\b', t)))
strict_es = 0
for n, (f, ret, params, ref) in exports.items():
    if n not in ES: continue
    er, ep = ES[n]
    mine = [_nt(x) for x in params.split(',')] if params.strip() not in ('', 'void') else []
    if mine != [_nt(x) for x in ep]: strict_es += 1; errors.append('%s: C prototype differs from GLES header' % n)

# enums
used = set()
for p in files():
    if p.endswith('gl_desktop.hpp'): continue
    used |= set(re.findall(r'\bGL_[A-Z0-9_]+\b', open(p).read()))
used -= {'GL_APIENTRY', 'GL_APICALL', 'GL_APIENTRYP', 'GL_GLES_PROTOTYPES', 'GL_GLEXT_PROTOTYPES'}
enum_unknown = sorted(u for u in used if u not in enums and u not in ESDEF)
enum_conflict = sorted(u for u in used if u in enums and u in ESDEF and enums[u] != ESDEF[u])
gd = open(os.path.join(SRC, 'gen', 'gl_desktop.hpp')).read()
gd_bad = [n for n, v in re.findall(r'#define (GL_\w+) 0x([0-9A-F]+)', gd) if enums.get(n) != int(v, 16)]
for u in enum_unknown: errors.append('unknown enum ' + u)
for u in enum_conflict: errors.append('enum value conflict jar/ES ' + u)
for u in gd_bad: errors.append('gl_desktop.hpp value != jar ' + u)

# coverage
mc_missing = sorted(mc_req - set(exports))
mc_stub = sorted(n for n in mc_req if cat.get(n, '') in ('stub', 'noop'))
flags = {}
for f in ['GL11', 'GL12', 'GL13', 'GL14', 'GL15', 'GL20', 'GL21', 'GL30'] + [e for e in rep['extensions']]:
    key = f
    names = [x for l in F.get(key, {'lists': []})['lists'] for x in l]
    flags[f] = all(x in exports for x in names)

# display-list hooks: every compilable command must start with ORY_DL(<name>...) and have a recorder
dl_missing = []
alltext = {p: open(p).read() for p in files()}
for n in set(dlspec.DL_SCALAR) | set(dlspec.DL_POINTER):
    ok = False
    for txt in alltext.values():
        m = re.search(r'OGL_EXPORT\s+void\s+' + n + r'\s*\([^)]*\)\s*\{(.{0,120})', txt, re.S)
        if m: ok = ('ORY_DL(' + n) in m.group(1); break
    rec = any(('bool dlr_' + n + '(') in t or ('DL_REC' in t and '(' + n + ',' in t) for t in alltext.values())
    if not (ok and rec): dl_missing.append(n)
for n in dl_missing: errors.append('display-list hook/recorder missing: ' + n)

# ELF dynamic symbol table vs sources
so = os.path.join(ROOT, os.environ.get('ORYON_SO', 'build-host/liboryon.so'))
elf_extra = elf_missing = None
if os.path.exists(so):
    out = subprocess.run(['nm', '-D', '--defined-only', so], capture_output=True, text=True).stdout
    syms = {l.split()[-1] for l in out.splitlines() if l.strip() and l.split()[-2] in ('T', 'W', 'i')}
    elf_extra = sorted(syms - set(exports)); elf_missing = sorted(set(exports) - syms)
    if elf_extra: errors.append('unexpected exported symbols: %d' % len(elf_extra))
    if elf_missing: errors.append('exports missing in ELF: %d' % len(elf_missing))

print('== VALIDATION (src vs 1.12.2.jar + lwjgl-glfw-classes.jar) ==')
print('exports: %d | ABI match jar JNI: %d | jar refs verified: %d (bad %d) | NativeType soft diffs: %d' % (len(exports), abi_ok, ref_ok, ref_bad, nt_mismatch))
print('same-name GLES prototypes: %d checked, %d differ' % (sum(1 for n in exports if n in ES), strict_es))
print('enums used: %d | unknown: %d | jar/ES conflicts: %d | gl_desktop.hpp bad: %d' % (len(used), len(enum_unknown), len(enum_conflict), len(gd_bad)))
print('MC 1.12.2 GL functions: %d | exported: %d | still stub/noop: %d' % (len(mc_req), len(mc_req) - len(mc_missing), len(mc_stub)))
print('LWJGL flags satisfiable: core %s | ext %d/%d' % (' '.join('%s=%s' % (k, 'Y' if v else 'N') for k, v in flags.items() if k.startswith('GL')),
      sum(1 for k, v in flags.items() if not k.startswith('GL') and v), sum(1 for k in flags if not k.startswith('GL'))))
if elf_extra is not None: print('ELF dynsym: extra %d | missing %d' % (len(elf_extra), len(elf_missing)))
print('display-list compilable commands: %d | hook+recorder OK: %d' % (len(set(dlspec.DL_SCALAR) | set(dlspec.DL_POINTER)), len(set(dlspec.DL_SCALAR) | set(dlspec.DL_POINTER)) - len(dl_missing)))
print('categories:', rep['categories'])
if errors:
    print('ERRORS (%d):' % len(errors))
    for e in errors[:25]: print('  -', e)
if '--stubs' in sys.argv: print('MC stub/noop:', mc_stub)
sys.exit(1 if errors else 0)
