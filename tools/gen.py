#!/usr/bin/env python3
# tools/gen.py -- Oryon code generator. Inputs: tools/db (jar-derived) + Khronos ES headers.
# Outputs: src/gen/{es_funcs.inc, gl_desktop.hpp, exports.cpp, ext_list.inc}, tools/db/gen_report.json
import os, re, json, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import khr, dlspec

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB = os.path.join(ROOT, 'tools', 'db'); SRC = os.path.join(ROOT, 'src'); GEN = os.path.join(SRC, 'gen')
os.makedirs(GEN, exist_ok=True)
J = lambda n: json.load(open(os.path.join(DB, n)))
sigs = J('sigs.json'); enums = J('enums.json'); caps = J('caps.json'); F = caps['features']
mc_req = set(J('mc_required_gl.json'))
ES = khr.gles32(); ESX = khr.gles_ext(); ESDEF = khr.gl_defines()

# ------------------------------------------------------------------ export set
CORE_FEATURES = ['GL11', 'GL12', 'GL13', 'GL14', 'GL15', 'GL20', 'GL21', 'GL30']
# desktop extensions advertised: (name, ES capability requirement or None)
EXTENSIONS = [
    ('ARB_multitexture', None), ('ARB_texture_env_combine', None), ('ARB_texture_env_add', None),
    ('ARB_texture_env_crossbar', None), ('ARB_texture_env_dot3', None), ('EXT_texture_env_combine', None),
    ('EXT_texture_env_add', None), ('ARB_texture_cube_map', None), ('ARB_texture_non_power_of_two', None),
    ('ARB_texture_mirrored_repeat', None), ('SGIS_texture_edge_clamp', None), ('EXT_texture_edge_clamp', None),
    ('EXT_bgra', None), ('ARB_depth_texture', None), ('EXT_packed_depth_stencil', None),
    ('ARB_vertex_buffer_object', None), ('ARB_pixel_buffer_object', None), ('ARB_map_buffer_range', None),
    ('ARB_vertex_array_object', None), ('ARB_framebuffer_object', None), ('EXT_framebuffer_object', None),
    ('EXT_framebuffer_blit', None), ('EXT_framebuffer_multisample', None),
    ('ARB_shader_objects', None), ('ARB_vertex_shader', None), ('ARB_fragment_shader', None), ('ARB_shading_language_100', None),
    ('EXT_blend_func_separate', None), ('EXT_blend_color', None), ('EXT_blend_minmax', None), ('EXT_blend_subtract', None),
    ('EXT_blend_equation_separate', None), ('NV_fog_distance', None),
    ('ARB_texture_rg', None), ('ARB_half_float_pixel', None), ('ARB_depth_buffer_float', None),
    ('EXT_texture_filter_anisotropic', 'aniso'), ('ARB_texture_border_clamp', 'border_clamp'),
]
EXPORT = set(mc_req)
for f in CORE_FEATURES:
    for l in F[f]['lists']: EXPORT.update(l)
for e, _ in EXTENSIONS:
    if e in F:
        for l in F[e]['lists']: EXPORT.update(l)
EXPORT = sorted(EXPORT)

# ------------------------------------------------------------------ handwritten exports (scan)
HW_RE = re.compile(r'OGL_EXPORT\s+([\w\s\*]+?)\s*\b(gl\w+)\s*\(([^)]*)\)\s*\{')
handwritten = {}
for fn in sorted(os.listdir(SRC)):
    if not fn.endswith(('.cpp', '.hpp')): continue
    txt = open(os.path.join(SRC, fn)).read()
    for m in HW_RE.finditer(txt):
        handwritten[m.group(2)] = {'file': fn, 'ret': m.group(1).strip(), 'params': m.group(3).strip()}

# ------------------------------------------------------------------ helpers
JNI_C = {'I': 'GLint', 'J': 'void *', 'F': 'GLfloat', 'D': 'GLdouble', 'S': 'GLshort', 'B': 'GLbyte', 'Z': 'GLboolean', 'C': 'GLushort', 'V': 'void'}
def c_type(nt, jni):
    if nt:
        t = nt.replace('GLvoid', 'void')
        return t
    return JNI_C.get(jni, 'void *')
def jar_ref(g):
    s = sigs[g]; return '%s.%s%s' % (s['owner'], s['java'], s['desc'])
def jar_params(g):
    s = sigs[g]; pnt = s['pnt'] or [None] * len(s['params'])
    return [c_type(pnt[i], s['params'][i]) for i in range(len(s['params']))]
def jar_ret(g):
    s = sigs[g]; return c_type(s['rnt'], s['ret']) if s['ret'] != 'V' else 'void'
def arg_name(p):
    m = re.search(r'(\w+)\s*(\[\s*\d*\s*\])?\s*$', p); return m.group(1)
def default_ret(r):
    r = r.strip()
    if r == 'void': return ''
    if '*' in r: return 'return nullptr;'
    return 'return 0;'

# ------------------------------------------------------------------ variant families (immediate-mode attribute setters etc.)
FAM = re.compile(r'^gl(?P<base>Vertex|Color|SecondaryColor|TexCoord|MultiTexCoord|Normal|RasterPos|WindowPos|FogCoord|Index|Rect|EvalCoord|EvalPoint|EdgeFlag|VertexAttribI|VertexAttrib)(?P<n>[1-4])?(?P<N>N)?(?P<t>ub|us|ui|b|s|i|f|d)?(?P<v>v)?(?P<suf>ARB|EXT|NV)?$')
CT = {'b': 'GLbyte', 's': 'GLshort', 'i': 'GLint', 'f': 'GLfloat', 'd': 'GLdouble', 'ub': 'GLubyte', 'us': 'GLushort', 'ui': 'GLuint'}
def cvt(t, e, norm):
    if t == 'f': return e
    if not norm or t == 'd': return '(GLfloat)(%s)' % e
    return {'ub': 'ory::unorm_ub(%s)', 'us': 'ory::unorm_us(%s)', 'ui': 'ory::unorm_ui(%s)',
            'b': 'ory::snorm_b(%s)', 's': 'ory::snorm_s(%s)', 'i': 'ory::snorm_i(%s)'}[t] % e
def variant(g):
    m = FAM.match(g)
    if not m or g not in sigs: return None
    base, n, N, t, v = m.group('base'), m.group('n'), m.group('N'), m.group('t'), m.group('v')
    n = int(n) if n else None
    jp = sigs[g]['params']
    if base in ('Index', 'EvalCoord', 'EvalPoint', 'EdgeFlag'):
        ps = ['%s p%d' % (t_, i) for i, t_ in enumerate(jar_params(g))]
        return ('noop', ps, '')
    if t is None: return None
    T = CT[t]; lead = []; lead_expr = ''
    if base == 'MultiTexCoord': lead = ['GLenum target']
    if base in ('VertexAttrib', 'VertexAttribI'): lead = ['GLuint index']
    if base == 'Rect':
        if v: ps = ['const %s *v1' % T, 'const %s *v2' % T]; vals = ['v1[0]', 'v1[1]', 'v2[0]', 'v2[1]']
        else: ps = ['%s x1' % T, '%s y1' % T, '%s x2' % T, '%s y2' % T]; vals = ['x1', 'y1', 'x2', 'y2']
        body = 'ory::rectf(%s);' % ', '.join(cvt(t, x, False) for x in vals)
        return ('variant', ps, body)
    if base in ('FogCoord',): n = 1
    if base in ('Normal', 'SecondaryColor', 'WindowPos') and n is None: n = 3
    if n is None: return None
    names = ['x', 'y', 'z', 'w'][:n]
    if v: ps = lead + ['const %s *v' % T]; vals = ['v[%d]' % i for i in range(n)]
    else: ps = lead + ['%s %s' % (T, x) for x in names]; vals = names
    if len(ps) != len(jp): return None   # jar arity must match
    norm = base in ('Color', 'SecondaryColor', 'Normal') or N == 'N'
    if base == 'VertexAttribI':
        fn = 'ory::vattrib4ui' if t in ('ub', 'us', 'ui') else 'ory::vattrib4i'
        cast = 'GLuint' if t in ('ub', 'us', 'ui') else 'GLint'
        d = ['0', '0', '0', '1']
        a = ['(%s)(%s)' % (cast, vals[i]) if i < n else d[i] for i in range(4)]
        return ('variant', ps, '%s(index, %s);' % (fn, ', '.join(a)))
    cv = [cvt(t, x, norm) for x in vals]
    if base in ('Vertex', 'RasterPos', 'TexCoord', 'MultiTexCoord', 'VertexAttrib'):
        d = ['0.0f', '0.0f', '0.0f', '1.0f']
        a = [cv[i] if i < n else d[i] for i in range(4)]
        if base == 'Vertex': return ('variant', ps, 'ory::imm_vertex(%s);' % ', '.join(a))
        if base == 'RasterPos': return ('variant', ps, 'ory::raster_pos(%s);' % ', '.join(a))
        if base == 'TexCoord': return ('variant', ps, 'ory::cur_texcoord(0u, %s);' % ', '.join(a))
        if base == 'MultiTexCoord': return ('variant', ps, 'ory::cur_texcoord((GLuint)(target - GL_TEXTURE0), %s);' % ', '.join(a))
        return ('variant', ps, 'ory::vattrib4f(index, %s);' % ', '.join(a))
    if base == 'Color':
        a = cv + (['1.0f'] if n == 3 else [])
        return ('variant', ps, 'ory::cur_color(%s);' % ', '.join(a)) if n in (3, 4) else None
    if base == 'SecondaryColor': return ('variant', ps, 'ory::cur_color2(%s);' % ', '.join(cv[:3]))
    if base == 'Normal': return ('variant', ps, 'ory::cur_normal(%s);' % ', '.join(cv[:3]))
    if base == 'WindowPos':
        a = cv + (['0.0f'] if n == 2 else [])
        return ('variant', ps, 'ory::window_pos(%s);' % ', '.join(a))
    if base == 'FogCoord': return ('variant', ps, 'ory::cur_fogcoord(%s);' % cv[0])
    return None

# ------------------------------------------------------------------ aliases (ARB/EXT -> core)
MANUAL_ALIAS = {
    'glCreateShaderObjectARB': 'glCreateShader', 'glCreateProgramObjectARB': 'glCreateProgram',
    'glAttachObjectARB': 'glAttachShader', 'glDetachObjectARB': 'glDetachShader',
    'glUseProgramObjectARB': 'glUseProgram', 'glGetAttachedObjectsARB': 'glGetAttachedShaders',
}
def alias_target(g):
    t = MANUAL_ALIAS.get(g)
    if t is None:
        m = re.match(r'^(gl\w+?)(ARB|EXT)$', g)
        if not m: return None
        t = m.group(1)
    if t not in sigs or t not in EXPORT_ALL: return None
    a, b = sigs[g], sigs[t]
    if a['params'] != b['params'] or a['ret'] != b['ret']: return None
    return t

# ------------------------------------------------------------------ classify
EXPORT_ALL = set(EXPORT)
cat = {}; code = []
PASS_PROLOGUE = True
for g in EXPORT:
    if g in handwritten: cat[g] = 'handwritten'; continue
    v = variant(g)
    if v:
        kind, ps, body = v
        cat[g] = kind
        r = jar_ret(g)
        code.append('/* jar: %s */\nOGL_EXPORT %s %s(%s) { %s%s }' % (jar_ref(g), r, g, ', '.join(ps) if ps else 'void',
                    body if kind == 'variant' else ''.join('(void)%s; ' % arg_name(p) for p in ps), '' if kind == 'variant' else default_ret(r)))
        continue
    t = alias_target(g)
    if t:
        cat[g] = 'alias:' + t
        continue  # emitted after, needs target signature
    if g in ES:
        r, ps = ES[g]
        if len(ps) != len(sigs[g]['params']): raise SystemExit('ES/jar arity mismatch: ' + g)
        args = ', '.join(arg_name(p) for p in ps)
        cat[g] = 'passthrough'
        dl = ('ORY_DL(%s%s); ' % (g, (', ' + args) if args else '')) if g in dlspec.DL_SCALAR else ''
        code.append('/* jar: %s */\nOGL_EXPORT %s %s(%s) { %sORY_PROLOGUE(); %sory::es.%s(%s); }' % (
            jar_ref(g), r, g, ', '.join(ps) if ps else 'void', dl, '' if r == 'void' else 'return ', g, args))
        continue
    cat[g] = 'stub'
    ps = jar_params(g); r = jar_ret(g)
    pl = ['%s p%d' % (t_, i) for i, t_ in enumerate(ps)]
    code.append('/* jar: %s */\nOGL_EXPORT %s %s(%s) { %s%s }' % (jar_ref(g), r, g, ', '.join(pl) if pl else 'void',
                ''.join('(void)p%d; ' % i for i in range(len(pl))), default_ret(r)))
# aliases: signature from target's definition source (ES prototype, handwritten, or jar)
def sig_of(t):
    if t in handwritten:
        h = handwritten[t]; ps = [p.strip() for p in h['params'].split(',')] if h['params'].strip() not in ('', 'void') else []
        return h['ret'], ps
    if cat.get(t) == 'passthrough': return ES[t][0], ES[t][1]
    v = variant(t)
    if v: return jar_ret(t), v[1]
    ps = jar_params(t); return jar_ret(t), ['%s p%d' % (x, i) for i, x in enumerate(ps)]
for g in EXPORT:
    c = cat[g]
    if not c.startswith('alias:'): continue
    t = c[6:]; r, ps = sig_of(t)
    args = ', '.join(arg_name(p) for p in ps)
    code.append('/* jar: %s -> %s */\nOGL_EXPORT %s %s(%s) { %s%s(%s); }' % (jar_ref(g), t, r, g, ', '.join(ps) if ps else 'void',
                '' if r == 'void' else 'return ', t, args))

HDR = '// GENERATED by tools/gen.py -- DO NOT EDIT. Source of truth: 1.12.2.jar + lwjgl-glfw-classes.jar (tools/db)\n'
decls = []
for g in EXPORT:
    c = cat[g]
    if c.startswith('alias:') and c[6:] in handwritten:
        h = handwritten[c[6:]]
        decls.append('OGL_EXPORT %s %s(%s);' % (h['ret'], c[6:], h['params'] or 'void'))
decls = sorted(set(decls))
with open(os.path.join(GEN, 'exports.cpp'), 'w') as f:
    f.write(HDR + '#include "../oryon.hpp"\n#include "../imm.hpp"\n\n// handwritten alias targets (other translation units)\n' +
            '\n'.join(decls) + '\n\n' + '\n'.join(code) + '\n')

# ------------------------------------------------------------------ display-list recorders
SCALAR = set('IJFDSBZC') - {'J'}
dl_decl = []; dl_code = []; dl_ext = []
for g in dlspec.DL_SCALAR:
    if g not in sigs or any(t not in SCALAR for t in sigs[g]['params']): raise SystemExit('DL_SCALAR non-scalar per jar: ' + g)
    if g not in EXPORT_ALL: raise SystemExit('DL_SCALAR not exported: ' + g)
    r, ps = sig_of(g)
    names = [arg_name(x) for x in ps]
    dl_decl.append('bool dlr_%s(%s);' % (g, ', '.join(ps) if ps else 'void'))
    dl_ext.append('OGL_EXPORT %s %s(%s);' % (r, g, ', '.join(ps) if ps else 'void'))
    if ps:
        fields = ' '.join('%s;' % x for x in ps)
        dl_code.append('namespace { struct A_%s { %s }; }\n'
                       'static void dlx_%s(const void *p_) { const A_%s *s_ = (const A_%s *)p_; ::%s(%s); }\n'
                       'bool dlr_%s(%s) { A_%s s_ = {%s}; dl_push_call(dlx_%s, &s_, sizeof s_); return g.dl.mode == GL_COMPILE; }'
                       % (g, fields, g, g, g, g, ', '.join('s_->' + n for n in names), g, ', '.join(ps), g, ', '.join(names), g))
    else:
        dl_code.append('static void dlx_%s(const void *) { ::%s(); }\n'
                       'bool dlr_%s(void) { dl_push_call(dlx_%s, nullptr, 0); return g.dl.mode == GL_COMPILE; }' % (g, g, g, g))
for g in dlspec.DL_POINTER:
    if g not in EXPORT_ALL: raise SystemExit('DL_POINTER not exported: ' + g)
    r, ps = sig_of(g)
    dl_decl.append('bool dlr_%s(%s);   // src/dlist.cpp' % (g, ', '.join(ps)))
with open(os.path.join(GEN, 'dlist_gen.hpp'), 'w') as f:
    f.write(HDR + '#pragma once\nnamespace ory {\n' + '\n'.join(dl_decl) + '\n} // namespace ory\n')
with open(os.path.join(GEN, 'dlist_gen.cpp'), 'w') as f:
    f.write(HDR + '#include "../oryon.hpp"\n\n' + '\n'.join(sorted(set(dl_ext))) + '\n\nnamespace ory {\n' + '\n'.join(dl_code) + '\n} // namespace ory\n')

# ------------------------------------------------------------------ ES function table
ES_EXT_USED = ['glGetQueryObjectui64vEXT', 'glBufferStorageEXT', 'glMultiDrawArraysEXT', 'glMultiDrawElementsEXT', 'glPolygonModeNV',
               'glPolygonOffsetClampEXT', 'glBindFragDataLocationEXT', 'glClipControlEXT']
with open(os.path.join(GEN, 'es_funcs.inc'), 'w') as f:
    f.write(HDR + '// ES_FN(ret, name, params, ext)\n')
    for n in sorted(ES):
        r, ps = ES[n]; f.write('ES_FN(%s, %s, (%s), 0)\n' % (r, n, ', '.join(ps) if ps else 'void'))
    for n in ES_EXT_USED:
        if n in ESX:
            r, ps = ESX[n]; f.write('ES_FN(%s, %s, (%s), 1)\n' % (r, n, ', '.join(ps) if ps else 'void'))

# ------------------------------------------------------------------ extension list (only if all entry points exported)
adv = []
for e, req in EXTENSIONS:
    names = [x for l in F.get(e, {'lists': []})['lists'] for x in l]
    if all(x in EXPORT_ALL for x in names): adv.append((e, req))
with open(os.path.join(GEN, 'ext_list.inc'), 'w') as f:
    f.write(HDR + '// ORY_EXT(name, es_requirement)\n')
    for e, req in adv: f.write('ORY_EXT("GL_%s", %s)\n' % (e, 'ORY_ESCAP_' + req.upper() if req else '0'))

# ------------------------------------------------------------------ desktop enums used by sources (values from jar)
IGN = {'GL_APIENTRY', 'GL_APICALL', 'GL_APIENTRYP', 'GL_GLES_PROTOTYPES', 'GL_GLEXT_PROTOTYPES'}
used = set()
for root, _, files in os.walk(SRC):
    for fn in files:
        if fn.endswith(('.cpp', '.hpp', '.inc')) and fn != 'gl_desktop.hpp':
            used |= set(re.findall(r'\bGL_[A-Z0-9_]+\b', open(os.path.join(root, fn)).read()))
used -= IGN
missing = sorted(u for u in used if u not in ESDEF and u not in enums)
lines = [HDR, '#pragma once\n']
for u in sorted(used):
    if u in ESDEF: continue
    if u in enums: lines.append('#ifndef %s\n#define %s 0x%X\n#endif\n' % (u, u, enums[u]))
with open(os.path.join(GEN, 'gl_desktop.hpp'), 'w') as f: f.write(''.join(lines))

from collections import Counter
cc = Counter(c.split(':')[0] for c in cat.values())
rep = {'export_count': len(EXPORT), 'categories': dict(cc), 'cat': cat, 'extensions': [e for e, _ in adv],
       'unknown_enums': missing, 'handwritten': handwritten}
json.dump(rep, open(os.path.join(DB, 'gen_report.json'), 'w'), indent=0, sort_keys=True)
print('gen: exports %d | %s | ext advertised %d/%d | desktop enums used %d | unknown enums %d' % (
    len(EXPORT), dict(cc), len(adv), len(EXTENSIONS), sum(1 for u in used if u not in ESDEF), len(missing)))
if missing: print('  UNKNOWN ENUMS:', missing[:20])
