#!/usr/bin/env python3
# tools/jarscan.py -- Oryon: build symbol/call-site DB from the two source-of-truth jars.
# Output: tools/db/*.json (never printed). Prints only summary counts.
import sys, os, json, re, zipfile, hashlib
from collections import defaultdict, Counter
sys.path.insert(0, os.path.dirname(__file__))
import jvm

UP = os.environ.get('ORYON_JARS', '/root/.claude/uploads/af115f22-38a8-5494-9373-115a23ce2354/')
MC_JAR = os.environ.get('ORYON_MC_JAR', UP + '86ebe279-1.12.2.jar')
LW_JAR = os.environ.get('ORYON_LWJGL_JAR', UP + '98d7d626-lwjgl-glfw-classes.jar')
DB = os.path.join(os.path.dirname(__file__), 'db')
os.makedirs(DB, exist_ok=True)

def sha(p): return hashlib.sha256(open(p, 'rb').read()).hexdigest()

lw = jvm.load_jar(LW_JAR)
mc = jvm.load_jar(MC_JAR)
OGL = 'org/lwjgl/opengl/'
CAPS = lw[OGL + 'GLCapabilities']

# ---------------------------------------------------------------- enums
enums = {}; enum_conflicts = 0
for n, cf in lw.items():
    if not n.startswith(OGL): continue
    for f in cf.fields:
        if f.const is None or not f.name.startswith('GL_') or not isinstance(f.const, int): continue
        v = f.const & 0xffffffffffffffff if f.desc == 'J' else f.const & 0xffffffff
        if f.name in enums and enums[f.name] != v: enum_conflicts += 1
        enums[f.name] = v

# ---------------------------------------------------------------- capability function table + features
cap_funcs = [f.name for f in CAPS.fields if f.desc == 'J' and f.name.startswith('gl')]
cap_flags = [f.name for f in CAPS.fields if f.desc == 'Z']
features = {}
for m in CAPS.methods:
    if not m.name.startswith('check_'): continue
    feat = m.name[6:]
    lists = []  # each checkFunctions call: list of names
    cur = []
    ins = list(jvm.decode(m.code))
    for pc, op, arg in ins:
        if op in (0x12, 0x13):
            c = CAPS.const(arg)
            if isinstance(c, str) and c.startswith('gl'): cur.append(c)
        elif op in (0xb6, 0xb8, 0xb9):
            o, n, d = CAPS.ref(arg)
            if n == 'checkFunctions':
                lists.append(cur); cur = []
    # fc-dependent: detect if method tests the boolean 'fc' param (slot 3) -> iload_3
    uses_fc = any(op == 0x1d for pc, op, arg in ins)
    features[feat] = {'lists': lists, 'uses_fc': uses_fc}
# Deprecated split: for features with 2 lists and fc usage, which list is guarded by fc?
# Heuristic resolved from bytecode order: record raw lists; generator treats union as required (fc=false).

# ---------------------------------------------------------------- natives: GL name -> JNI signature (+NativeType)
natives = {}
for n, cf in lw.items():
    if not n.startswith(OGL): continue
    for m in cf.methods:
        if not m.is_native: continue
        nm = m.name
        gname = 'gl' + nm[3:] if nm.startswith('ngl') else nm
        if not gname.startswith('gl'): continue
        ps, r = jvm.parse_desc(m.desc)
        rec = {'owner': n[len(OGL):], 'java': nm, 'desc': m.desc, 'params': ps, 'ret': r,
               'pnt': m.pnt, 'rnt': m.rnt}
        natives.setdefault(gname, []).append(rec)

# ---------------------------------------------------------------- LWJGL method resolver -> GL natives reached
def find_method(owner, name, desc, depth=0):
    cf = lw.get(owner)
    while cf is not None and depth < 12:
        m = cf.method(name, desc)
        if m is not None: return cf, m
        for itf in cf.interfaces:
            r = find_method(itf, name, desc, depth + 1)
            if r: return r
        cf = lw.get(cf.super) if cf.super else None
        depth += 1
    return None

_memo = {}
def gl_targets(owner, name, desc, depth=0, stack=()):
    key = (owner, name, desc)
    if key in _memo: return _memo[key]
    r = find_method(owner, name, desc)
    if r is None: return None
    cf, m = r
    out = set()
    if m.is_native:
        g = 'gl' + m.name[3:] if m.name.startswith('ngl') else m.name
        if g.startswith('gl'): out.add(g)
        _memo[key] = out; return out
    if m.code is None or depth > 6 or key in stack:
        _memo[key] = out; return out
    def inv(op, o, n, d, a, rc, pc):
        if o.startswith('org/lwjgl/opengl/') and not n.startswith('<'):
            t = gl_targets(o, n, d, depth + 1, stack + (key,))
            if t: out.update(t)
    def fld(kind, o, n, d, pc):
        if kind == 'getfield' and o == CAPS.name and d == 'J' and n.startswith('gl'): out.add(n)
    jvm.scan(cf, m, on_invoke=inv, on_field=fld)
    _memo[key] = out
    return out

# ---------------------------------------------------------------- MC scan: calls + thunks + field-constant propagation
calls = []        # (caller, op, owner, name, desc, args)
field_reads = Counter()
ctor_sites = defaultdict(list)      # (cls) -> list of args at new C(...)
ctor_field_param = {}               # (cls, field) -> param index (set in <init>)
for cn, cf in mc.items():
    for m in cf.methods:
        if m.code is None: continue
        caller = (cn, m.name, m.desc)
        def inv(op, o, n, d, a, rc, pc, caller=caller):
            if n == '<init>' and o in mc: ctor_sites[(o, d)].append(a)
            calls.append((caller, op, o, n, d, a))
        def put(o, n, d, v, ob, pc, m=m, cn=cn):
            if m.name == '<init>' and ob == ('this',) and o == cn and v and v[0] == 'p':
                ctor_field_param[(o, n)] = (m.desc, v[1])
        def fld(kind, o, n, d, pc):
            if o.startswith('org/lwjgl/') and kind in ('getfield', 'getstatic'): field_reads[(o, n, d)] += 1
        jvm.scan(cf, m, on_invoke=inv, on_putfield=put, on_field=fld)

def is_lw(o): return o.startswith('org/lwjgl/')
# thunks: MC methods that call LWJGL GL methods with args depending on params
thunk = defaultdict(list)   # caller -> [(o,n,d,args)]
for caller, op, o, n, d, a in calls:
    if is_lw(o) and o.startswith(OGL):
        thunk[caller].append((o, n, d, a))
mc_calls_by_caller = defaultdict(list)
for caller, op, o, n, d, a in calls:
    mc_calls_by_caller[caller].append((o, n, d, a))

def field_values(owner, fname):
    k = (owner, fname)
    if k not in ctor_field_param: return None
    cdesc, pi = ctor_field_param[k]
    vals = set(); dyn = False
    for args in ctor_sites.get((owner, cdesc), []):
        v = args[pi] if pi < len(args) else None
        if v and v[0] == 'i': vals.add(v[1])
        else: dyn = True
    return vals, dyn

# effective GL call records with constant args (resolve 'p' through callers, 'fld' through ctor sites)
usage = defaultdict(lambda: defaultdict(lambda: {'c': set(), 'dyn': False}))
direct_sites = Counter(); lw_refs = Counter()
def add_usage(key, args):
    for i, v in enumerate(args):
        slot = usage[key][i]
        if v is None: slot['dyn'] = True
        elif v[0] == 'i': slot['c'].add(v[1])
        elif v[0] == 'f': slot['c'].add(v[1])
        elif v[0] == 'fld':
            fv = field_values(v[1], v[2])
            if fv is None: slot['dyn'] = True
            else:
                slot['c'].update(fv[0]); slot['dyn'] |= fv[1]
        else: slot['dyn'] = True
callers_of = defaultdict(list)
for caller, op, o, n, d, a in calls:
    callers_of[(o, n, d)].append((caller, a))
for caller, op, o, n, d, a in calls:
    if not is_lw(o): continue
    lw_refs[(o, n, d)] += 1
    if not o.startswith(OGL): continue
    direct_sites[(o, n, d)] += 1
    if any(v and v[0] == 'p' for v in a):
        # propagate one+two levels through MC callers
        subs = callers_of.get(caller, [])
        if not subs: add_usage((o, n, d), [None if (v and v[0] == 'p') else v for v in a]); continue
        for c2, a2 in subs:
            eff = []
            for v in a:
                if v and v[0] == 'p':
                    w = a2[v[1]] if v[1] < len(a2) else None
                    if w and w[0] == 'p':
                        subs3 = callers_of.get(c2, [])
                        if subs3:
                            for c3, a3 in subs3:
                                w3 = a3[w[1]] if w[1] < len(a3) else None
                                add_usage((o, n, d), [w3 if x is v else (x if not (x and x[0] == 'p') else None) for x in a])
                            eff = None; break
                        w = None
                    eff.append(w)
                else: eff.append(v)
            if eff is not None: add_usage((o, n, d), eff)
    else:
        add_usage((o, n, d), a)

# ---------------------------------------------------------------- link validation MC -> lwjgl jar
unresolved = []
lw_method_refs = [k for k in lw_refs if not k[1].startswith('<')]
for (o, n, d) in lw_method_refs:
    if find_method(o, n, d) is None: unresolved.append((o, n, d))
unresolved_fields = []
for (o, n, d) in field_reads:
    cf = lw.get(o); ok = False
    while cf is not None:
        if any(f.name == n and f.desc == d for f in cf.fields): ok = True; break
        cf = lw.get(cf.super) if cf.super else None
    if not ok: unresolved_fields.append((o, n, d))

# ---------------------------------------------------------------- MC GL method -> native GL functions
mc_gl = {}
for (o, n, d), cnt in direct_sites.items():
    t = gl_targets(o, n, d)
    mc_gl['%s.%s%s' % (o[len(OGL):], n, d)] = {'sites': cnt, 'gl': sorted(t) if t else [],
        'args': {str(i): {'c': sorted(s['c']), 'dyn': s['dyn']} for i, s in usage[(o, n, d)].items()}}
required = sorted({g for v in mc_gl.values() for g in v['gl']})

# capability flags read by MC (ContextCapabilities / GLCapabilities fields)
caps_read = sorted({'%s.%s' % (o.split('/')[-1], n) for (o, n, d) in field_reads if d == 'Z' and o.startswith(OGL)})
per_method_caps = defaultdict(set)
for cn, cf in mc.items():
    for m in cf.methods:
        if m.code is None: continue
        def _fl(k, o, n, d, pc, key=(cn, m.name, m.desc)):
            if o == OGL + 'ContextCapabilities' and d == 'Z': per_method_caps[key].add(n)
        jvm.scan(cf, m, on_field=_fl)
render_caps = sorted({f for v in per_method_caps.values() if len(v) < 40 for f in v})
# map lwjglx ContextCapabilities -> GLCapabilities flags
CC = lw.get(OGL + 'ContextCapabilities')
cc_map = {}
if CC:
    for m in CC.methods:
        if m.code is None: continue
        seq = []
        for pc, op, arg in jvm.decode(m.code):
            if op in (0xb4, 0xb2):
                o, n, d = CC.ref(arg)
                if o == CAPS.name and d == 'Z': seq.append(n)
            elif op == 0xb5:
                o, n, d = CC.ref(arg)
                if o == CC.name and d == 'Z' and seq: cc_map[n] = seq.pop()
# ---------------------------------------------------------------- shaders in MC assets
zm = zipfile.ZipFile(MC_JAR)
shader_info = {'files': 0, 'versions': Counter(), 'builtins': Counter(), 'attributes': Counter(), 'funcs': Counter()}
BUILTIN = re.compile(r'\bgl_[A-Za-z_]+\b')
FUNCS = re.compile(r'\b(texture2D|texture2DLod|texture2DProj|shadow2D|texture3D|textureCube|ftransform|texelFetch|texture)\s*\(')
ATTR = re.compile(r'\battribute\s+\w+\s+(\w+)')
for nme in zm.namelist():
    if not re.search(r'shaders/.*\.(vsh|fsh|glsl)$', nme): continue
    s = zm.read(nme).decode('utf-8', 'replace'); shader_info['files'] += 1
    vm = re.search(r'#version\s+(\d+)', s); shader_info['versions'][vm.group(1) if vm else 'none'] += 1
    for b in BUILTIN.findall(s): shader_info['builtins'][b] += 1
    for f in FUNCS.findall(s): shader_info['funcs'][f] += 1
    for a in ATTR.findall(s): shader_info['attributes'][a] += 1

# ---------------------------------------------------------------- unified signatures for all GL functions
def _canon(g):
    recs = natives[g]
    n = [r for r in recs if r['java'].startswith('ngl')]
    if n: return n[0]
    p = [r for r in recs if all(t in 'IJFDSBZC' for t in r['params'])]
    return p[0] if p else recs[0]
def _wrapper_pnt(g, r):
    cf = lw[OGL + r['owner']]
    for m in cf.methods:
        if m.name == g and m.pnt and not m.is_native:
            ps, _ = jvm.parse_desc(m.desc)
            if len(ps) == len(r['params']) and all(x is not None for x in m.pnt): return m.pnt, m.rnt
    return None
sigs = {}
for g in natives:
    r = _canon(g); pnt = r['pnt']; rnt = r['rnt']; src = 'native'
    if not r['params']: pnt = []; src = 'noparams' if not pnt else src
    if pnt is None or any(x is None for x in pnt):
        w = _wrapper_pnt(g, r)
        if w: pnt, rnt = w[0], (rnt or w[1]); src = 'wrapper'
        else: pnt = None; src = 'jni'
    sigs[g] = {'owner': r['owner'], 'java': r['java'], 'desc': r['desc'], 'params': r['params'], 'ret': r['ret'],
               'pnt': pnt, 'rnt': rnt, 'src': src}

# ---------------------------------------------------------------- write DB
def dump(name, obj):
    with open(os.path.join(DB, name), 'w') as f: json.dump(obj, f, indent=0, sort_keys=True, default=list)
dump('meta.json', {'mc_jar_sha256': sha(MC_JAR), 'lwjgl_jar_sha256': sha(LW_JAR),
                   'lwjgl_version': [c.const for c in lw['org/lwjgl/Version'].fields if c.name.startswith('VERSION_')]})
dump('enums.json', enums)
dump('caps.json', {'functions': cap_funcs, 'flags': cap_flags, 'features': features})
dump('natives.json', natives)
dump('mc_gl_calls.json', mc_gl)
dump('mc_required_gl.json', required)
dump('mc_caps_read.json', {'read': caps_read, 'render_path': render_caps, 'mapping': 'ContextCapabilities copies GLCapabilities booleans by field name (reflection)'})
dump('mc_lwjgl_refs.json', {'methods': ['%s.%s%s' % k for k in sorted(lw_refs)], 'unresolved': ['%s.%s%s' % k for k in unresolved],
                            'fields': ['%s.%s:%s' % k for k in sorted(field_reads)], 'unresolved_fields': ['%s.%s:%s' % k for k in unresolved_fields]})
dump('sigs.json', sigs)
dump('mc_shaders.json', {k: (dict(v) if isinstance(v, Counter) else v) for k, v in shader_info.items()})

nat_with_pnt = sum(1 for v in natives.values() for r in v if r['pnt'])
print('== jarscan summary ==')
print('lwjgl classes %d | mc classes %d' % (len(lw), len(mc)))
print('enums %d (conflicts %d) | cap functions %d | flags %d | features %d' % (len(enums), enum_conflicts, len(cap_funcs), len(cap_flags), len(features)))
print('GL natives %d (with NativeType annotations: %d) | natives not in caps: %d' % (len(natives), nat_with_pnt, sum(1 for g in natives if g not in set(cap_funcs))))
print('MC -> lwjgl method refs %d, unresolved %d | field refs %d, unresolved %d' % (len(lw_method_refs), len(unresolved), len(field_reads), len(unresolved_fields)))
print('MC GL call targets (java methods) %d, sites %d | distinct native GL functions required %d' % (len(mc_gl), sum(direct_sites.values()), len(required)))
print('MC GL java methods without native resolution: %d' % sum(1 for v in mc_gl.values() if not v['gl']))
print('capability flags read by MC: %d | render-path flags: %d (ContextCapabilities = GLCapabilities by name)' % (len(caps_read), len(render_caps)))
print('shader files %d | versions %s' % (shader_info['files'], dict(shader_info['versions'])))
