#!/usr/bin/env python3
# tools/annotate.py -- (re)write "/* jar: Owner.method(desc) */" above every handwritten OGL_EXPORT from tools/db/sigs.json.
# Keeps handwritten sources traceable to the jar without typing references from memory.
import os, re, json, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dlspec
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sigs = json.load(open(os.path.join(ROOT, 'tools', 'db', 'sigs.json')))
RE = re.compile(r'(?:/\*\s*jar:[^*]*\*/[ \t]*\n)?(?P<def>OGL_EXPORT\s+[\w\s\*]+?\s*\b(?P<name>gl\w+)\s*\()')
fixed = 0; unknown = []
for fn in sorted(os.listdir(os.path.join(ROOT, 'src'))):
    if not fn.endswith(('.cpp', '.hpp')): continue
    p = os.path.join(ROOT, 'src', fn); s = open(p).read()
    def rep(m):
        global fixed
        n = m.group('name'); sg = sigs.get(n)
        if sg is None: unknown.append(n); return m.group(0)
        fixed += 1
        return '/* jar: %s.%s%s */\n%s' % (sg['owner'], sg['java'], sg['desc'], m.group('def'))
    s2 = RE.sub(rep, s)
    if s2 != s: open(p, 'w').write(s2)
print('annotate: %d handwritten exports annotated from jar DB | unknown names: %d %s' % (fixed, len(unknown), unknown[:5]))

# ORY_DL hook as first statement of every handwritten compilable command
DL = set(dlspec.DL_SCALAR) | set(dlspec.DL_POINTER)
DEF = re.compile(r'OGL_EXPORT\s+void\s+(?P<name>gl\w+)\s*\((?P<params>[^)]*)\)\s*\{')
hooked = 0
for fn in sorted(os.listdir(os.path.join(ROOT, 'src'))):
    if not fn.endswith('.cpp'): continue
    p = os.path.join(ROOT, 'src', fn); s = open(p).read(); out = []; last = 0
    for m in DEF.finditer(s):
        n = m.group('name')
        if n not in DL: continue
        body = s[m.end():m.end() + 80]
        if 'ORY_DL(' in body.split(';')[0]: continue
        ps = m.group('params').strip()
        names = [] if ps in ('', 'void') else [re.search(r'(\w+)\s*$', x.strip()).group(1) for x in ps.split(',')]
        out.append(s[last:m.end()]); out.append(' ORY_DL(%s%s);' % (n, (', ' + ', '.join(names)) if names else ''))
        last = m.end(); hooked += 1
    out.append(s[last:])
    s2 = ''.join(out)
    if s2 != s: open(p, 'w').write(s2)
print('annotate: ORY_DL hooks inserted: %d' % hooked)
