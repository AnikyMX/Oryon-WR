# tools/jvm.py -- minimal JVM class-file parser + bytecode scanner (Oryon)
# Pure Python3, no deps. Used for programmatic cross-reference against jars.
import struct

ACC_PUBLIC=0x0001; ACC_STATIC=0x0008; ACC_FINAL=0x0010; ACC_NATIVE=0x0100
ACC_INTERFACE=0x0200; ACC_ABSTRACT=0x0400

def _mutf8(b):
    try:
        return b.decode('utf-8')
    except UnicodeDecodeError:
        return b.replace(b'\xc0\x80', b'\x00').decode('utf-8', 'replace')

class Member:
    __slots__ = ('access','name','desc','code','const','pnt','rnt','exc','maxl')
    def __init__(s): s.code=None; s.const=None; s.pnt=None; s.rnt=None; s.exc=None; s.maxl=0
    @property
    def is_static(s): return bool(s.access & ACC_STATIC)
    @property
    def is_native(s): return bool(s.access & ACC_NATIVE)

class ClassFile:
    __slots__ = ('name','super','interfaces','access','fields','methods','cp','version','_mi')
    def method(s, name, desc):
        if s._mi is None:
            s._mi = {(m.name, m.desc): m for m in s.methods}
        return s._mi.get((name, desc))
    def utf8(s, i): return s.cp[i][1]
    def cls(s, i): return s.cp[s.cp[i][1]][1]
    def ref(s, i):
        t, ci, nti = s.cp[i]
        nt = s.cp[nti]
        return (s.cls(ci), s.cp[nt[1]][1], s.cp[nt[2]][1])
    def const(s, i):
        e = s.cp[i]; t = e[0]
        if t in (3, 4, 5, 6): return e[1]
        if t == 8: return s.cp[e[1]][1]
        if t == 7: return ('class', s.cp[e[1]][1])
        return ('cp', t)

def _annots(cf, d, o):
    # parse one annotation -> (type, {name: value}), new offset
    ti, n = struct.unpack_from('>HH', d, o); o += 4
    vals = {}
    for _ in range(n):
        ni, = struct.unpack_from('>H', d, o); o += 2
        v, o = _elem(cf, d, o)
        vals[cf.cp[ni][1]] = v
    return (cf.cp[ti][1], vals), o

def _elem(cf, d, o):
    tag = chr(d[o]); o += 1
    if tag in 'BCDFIJSZs':
        i, = struct.unpack_from('>H', d, o); o += 2
        e = cf.cp[i]
        return (e[1] if tag == 's' else e[1]), o
    if tag == 'e':
        a, b = struct.unpack_from('>HH', d, o); o += 4
        return ('enum', cf.cp[a][1], cf.cp[b][1]), o
    if tag == 'c':
        i, = struct.unpack_from('>H', d, o); o += 2
        return ('class', cf.cp[i][1]), o
    if tag == '@':
        return _annots(cf, d, o)
    if tag == '[':
        n, = struct.unpack_from('>H', d, o); o += 2
        arr = []
        for _ in range(n):
            v, o = _elem(cf, d, o); arr.append(v)
        return arr, o
    raise ValueError('bad element tag ' + tag)

def _native_type(annl):
    for t, vals in annl:
        if t == 'Lorg/lwjgl/system/NativeType;':
            return vals.get('value')
    return None

def parse(data, want_code=True):
    o = 0
    magic, minor, major, n = struct.unpack_from('>IHHH', data, 0); o = 10
    if magic != 0xCAFEBABE: raise ValueError('not a class')
    cf = ClassFile(); cf.version = major; cf._mi = None
    cp = [None] * n; i = 1
    while i < n:
        t = data[o]; o += 1
        if t == 1:
            L, = struct.unpack_from('>H', data, o); o += 2
            cp[i] = (1, _mutf8(data[o:o+L])); o += L
        elif t == 3: cp[i] = (3, struct.unpack_from('>i', data, o)[0]); o += 4
        elif t == 4: cp[i] = (4, struct.unpack_from('>f', data, o)[0]); o += 4
        elif t == 5: cp[i] = (5, struct.unpack_from('>q', data, o)[0]); o += 8; i += 1
        elif t == 6: cp[i] = (6, struct.unpack_from('>d', data, o)[0]); o += 8; i += 1
        elif t in (7, 8, 16, 19, 20): cp[i] = (t, struct.unpack_from('>H', data, o)[0]); o += 2
        elif t in (9, 10, 11, 12, 17, 18):
            a, b = struct.unpack_from('>HH', data, o); o += 4; cp[i] = (t, a, b)
        elif t == 15:
            k, r = struct.unpack_from('>BH', data, o); o += 3; cp[i] = (15, k, r)
        else: raise ValueError('bad cp tag %d' % t)
        i += 1
    cf.cp = cp
    acc, this, sup, ni = struct.unpack_from('>HHHH', data, o); o += 8
    cf.access = acc; cf.name = cf.cls(this); cf.super = cf.cls(sup) if sup else None
    cf.interfaces = [cf.cls(struct.unpack_from('>H', data, o + 2*k)[0]) for k in range(ni)]; o += 2*ni
    def members(o, is_method):
        cnt, = struct.unpack_from('>H', data, o); o += 2
        out = []
        for _ in range(cnt):
            m = Member()
            m.access, a, b, na = struct.unpack_from('>HHHH', data, o); o += 8
            m.name = cp[a][1]; m.desc = cp[b][1]
            for _ in range(na):
                an, al = struct.unpack_from('>HI', data, o); o += 6
                aname = cp[an][1]; body = data[o:o+al]
                if aname == 'Code' and want_code and is_method:
                    ms, ml, cl = struct.unpack_from('>HHI', body, 0)
                    m.code = body[8:8+cl]; m.maxl = ml
                    ne, = struct.unpack_from('>H', body, 8 + cl); q = 10 + cl; m.exc = []
                    for _ in range(ne):
                        sp, ep, hp, ct = struct.unpack_from('>HHHH', body, q); q += 8
                        m.exc.append((sp, ep, hp, cf.cls(ct) if ct else None))
                elif aname == 'ConstantValue' and not is_method:
                    m.const = cf.const(struct.unpack_from('>H', body, 0)[0])
                elif aname in ('RuntimeInvisibleParameterAnnotations', 'RuntimeVisibleParameterAnnotations'):
                    try:
                        np_ = body[0]; q = 1; res = []
                        for _ in range(np_):
                            k, = struct.unpack_from('>H', body, q); q += 2; al_ = []
                            for _ in range(k):
                                an_, q = _annots(cf, body, q); al_.append(an_)
                            res.append(_native_type(al_))
                        if m.pnt is None or aname.startswith('RuntimeInvisible'): m.pnt = res
                    except Exception: pass
                elif aname in ('RuntimeInvisibleAnnotations', 'RuntimeVisibleAnnotations'):
                    try:
                        k, = struct.unpack_from('>H', body, 0); q = 2; al_ = []
                        for _ in range(k):
                            an_, q = _annots(cf, body, q); al_.append(an_)
                        nt = _native_type(al_)
                        if nt is not None: m.rnt = nt
                    except Exception: pass
                o += al
            out.append(m)
        return out, o
    cf.fields, o = members(o, False)
    cf.methods, o = members(o, True)
    return cf

# ---------------- descriptors ----------------
def parse_desc(d):
    i = 1; ps = []
    while d[i] != ')':
        j = i
        while d[j] == '[': j += 1
        if d[j] == 'L': j = d.index(';', j)
        ps.append(d[i:j+1]); i = j + 1
    return ps, d[i+1:]

def tsize(t): return 2 if t in ('J', 'D') else 1

# ---------------- bytecode decode ----------------
_LEN = {}
for op in range(0x00, 0x10): _LEN[op] = 1
_LEN.update({0x10: 2, 0x11: 3, 0x12: 2, 0x13: 3, 0x14: 3})
for op in range(0x15, 0x1a): _LEN[op] = 2
for op in range(0x1a, 0x36): _LEN[op] = 1
for op in range(0x36, 0x3b): _LEN[op] = 2
for op in range(0x3b, 0x84): _LEN[op] = 1
_LEN[0x84] = 3
for op in range(0x85, 0x99): _LEN[op] = 1
for op in range(0x99, 0xa9): _LEN[op] = 3
_LEN[0xa9] = 2
for op in range(0xac, 0xb2): _LEN[op] = 1
for op in range(0xb2, 0xb9): _LEN[op] = 3
_LEN.update({0xb9: 5, 0xba: 5, 0xbb: 3, 0xbc: 2, 0xbd: 3, 0xbe: 1, 0xbf: 1, 0xc0: 3, 0xc1: 3,
             0xc2: 1, 0xc3: 1, 0xc5: 4, 0xc6: 3, 0xc7: 3, 0xc8: 5, 0xc9: 5, 0xca: 1, 0xfe: 1, 0xff: 1})

def decode(code):
    # yields (pc, op, operand)
    pc = 0; n = len(code)
    while pc < n:
        op = code[pc]
        if op == 0xaa:  # tableswitch
            p = (pc + 4) & ~3
            dflt, lo, hi = struct.unpack_from('>iii', code, p)
            offs = struct.unpack_from('>%di' % (hi - lo + 1), code, p + 12)
            yield pc, op, (dflt, [pc + x for x in offs] + [pc + dflt])
            pc = p + 12 + 4 * (hi - lo + 1); continue
        if op == 0xab:  # lookupswitch
            p = (pc + 4) & ~3
            dflt, np_ = struct.unpack_from('>ii', code, p)
            pairs = struct.unpack_from('>%di' % (2 * np_), code, p + 8)
            yield pc, op, (dflt, [pc + pairs[2*k+1] for k in range(np_)] + [pc + dflt])
            pc = p + 8 + 8 * np_; continue
        if op == 0xc4:  # wide
            op2 = code[pc + 1]
            if op2 == 0x84:
                idx, c = struct.unpack_from('>Hh', code, pc + 2); yield pc, 0x84, (idx, c); pc += 6
            else:
                idx, = struct.unpack_from('>H', code, pc + 2); yield pc, op2, idx; pc += 4
            continue
        L = _LEN.get(op)
        if L is None: raise ValueError('bad opcode %#x' % op)
        if L == 1: arg = None
        elif op in (0x10,): arg = struct.unpack_from('>b', code, pc + 1)[0]
        elif op == 0x11: arg = struct.unpack_from('>h', code, pc + 1)[0]
        elif op in (0x12, 0xbc) or (0x15 <= op <= 0x19) or (0x36 <= op <= 0x3a) or op == 0xa9: arg = code[pc + 1]
        elif op == 0x84: arg = (code[pc + 1], struct.unpack_from('>b', code, pc + 2)[0])
        elif 0x99 <= op <= 0xa8 or op in (0xc6, 0xc7): arg = pc + struct.unpack_from('>h', code, pc + 1)[0]
        elif op in (0xc8, 0xc9): arg = pc + struct.unpack_from('>i', code, pc + 1)[0]
        elif op == 0xc5: arg = (struct.unpack_from('>H', code, pc + 1)[0], code[pc + 3])
        else: arg = struct.unpack_from('>H', code, pc + 1)[0]
        yield pc, op, arg
        pc += L

# ---------------- abstract interpretation (linear, constant tracking) ----------------
# value forms: ('i',v) ('f',v) ('l',v) ('d',v) ('s',str) ('c',cls) ('p',idx) ('this',) ('fld',owner,name) None
_I = lambda v: ('i', ((v + 0x80000000) & 0xffffffff) - 0x80000000)
_BIN_I = {0x60: lambda a, b: a + b, 0x64: lambda a, b: a - b, 0x68: lambda a, b: a * b,
          0x7e: lambda a, b: a & b, 0x80: lambda a, b: a | b, 0x82: lambda a, b: a ^ b,
          0x78: lambda a, b: a << (b & 31), 0x7a: lambda a, b: a >> (b & 31)}

def scan(cf, m, on_invoke=None, on_putfield=None, on_field=None):
    code = m.code
    if code is None: return
    ins = list(decode(code))
    targets = set()
    stores = {}
    for pc, op, arg in ins:
        if 0x99 <= op <= 0xa8 or op in (0xc6, 0xc7, 0xc8): targets.add(arg)
        elif op in (0xaa, 0xab): targets.update(arg[1])
        if 0x36 <= op <= 0x3a: stores[arg] = stores.get(arg, 0) + 1
        elif 0x3b <= op <= 0x4e: s_ = (op - 0x3b) % 4; stores[s_] = stores.get(s_, 0) + 1
        elif op == 0x84: stores[arg[0]] = stores.get(arg[0], 0) + 1
    ps, _ = parse_desc(m.desc)
    loc = {}; slot = 0
    if not m.is_static: loc[0] = ('this',); slot = 1
    for k, t in enumerate(ps):
        if stores.get(slot, 0) == 0: loc[slot] = ('p', k)
        slot += tsize(t)
    single = {s for s, c in stores.items() if c == 1}
    st = []  # entries: (value, size)
    def push(v, sz=1): st.append((v, sz))
    def pop():
        return st.pop() if st else (None, 1)
    def popv(): return pop()[0]
    def popn(k):
        r = [popv() for _ in range(k)]; r.reverse(); return r
    dead = False
    for pc, op, arg in ins:
        if pc in targets: st = []; dead = False
        elif dead: st = []; dead = False
        if op == 0x00: pass
        elif op == 0x01: push(None)
        elif 0x02 <= op <= 0x08: push(('i', op - 3))
        elif op in (0x09, 0x0a): push(('l', op - 9), 2)
        elif 0x0b <= op <= 0x0d: push(('f', float(op - 0x0b)))
        elif op in (0x0e, 0x0f): push(('d', float(op - 0x0e)), 2)
        elif op in (0x10, 0x11): push(('i', arg))
        elif op in (0x12, 0x13):
            c = cf.const(arg); t = cf.cp[arg][0]
            push({3: ('i', c), 4: ('f', c), 8: ('s', c)}.get(t, ('c', c[1]) if t == 7 else None))
        elif op == 0x14:
            c = cf.const(arg); t = cf.cp[arg][0]
            push(('l', c) if t == 5 else ('d', c), 2)
        elif 0x15 <= op <= 0x19 or 0x1a <= op <= 0x2d:
            if op <= 0x19: s_ = arg; kind = op - 0x15
            else: s_ = (op - 0x1a) % 4; kind = (op - 0x1a) // 4
            v = loc.get(s_)
            push(v, 2 if kind in (1, 3) else 1)
        elif 0x2e <= op <= 0x35:
            popn(2); push(None, 2 if op in (0x2f, 0x31) else 1)
        elif 0x36 <= op <= 0x3a or 0x3b <= op <= 0x4e:
            s_ = arg if op <= 0x3a else (op - 0x3b) % 4
            v = popv()
            loc[s_] = v if (s_ in single and v is not None and v[0] in ('i', 'f', 's', 'l', 'd')) else None
        elif 0x4f <= op <= 0x56: popn(3)
        elif op == 0x57: pop()
        elif op == 0x58:
            a = pop()
            if a[1] == 1: pop()
        elif op == 0x59:
            a = pop(); st.append(a); st.append(a)
        elif op == 0x5a:
            a = pop(); b = pop(); st += [a, b, a]
        elif op == 0x5b:
            a = pop(); b = pop()
            if b[1] == 2: st += [a, b, a]
            else:
                c = pop(); st += [a, c, b, a]
        elif op == 0x5c:
            a = pop()
            if a[1] == 2: st += [a, a]
            else:
                b = pop(); st += [b, a, b, a]
        elif op == 0x5d:
            a = pop()
            if a[1] == 2:
                b = pop(); st += [a, b, a]
            else:
                b = pop(); c = pop(); st += [b, a, c, b, a]
        elif op == 0x5e:
            a = pop()
            if a[1] == 2:
                b = pop()
                if b[1] == 2: st += [a, b, a]
                else:
                    c = pop(); st += [a, c, b, a]
            else:
                b = pop(); c = pop()
                if c[1] == 2: st += [b, a, c, b, a]
                else:
                    d = pop(); st += [b, a, d, c, b, a]
        elif op == 0x5f:
            a = pop(); b = pop(); st += [a, b]
        elif 0x60 <= op <= 0x83:
            base = (op - 0x60) % 4 if op < 0x74 else None
            if op in _BIN_I:
                b = popv(); a = popv()
                push(_I(_BIN_I[op](a[1], b[1])) if (a and b and a[0] == 'i' and b[0] == 'i') else None)
            elif op in (0x7c,):  # iushr
                b = popv(); a = popv()
                push(_I((a[1] & 0xffffffff) >> (b[1] & 31)) if (a and b and a[0] == 'i' and b[0] == 'i') else None)
            elif 0x74 <= op <= 0x77:  # neg
                a = pop(); v = a[0]
                if v and v[0] in ('i', 'f', 'l', 'd'):
                    push((v[0], -v[1]) if v[0] != 'i' else _I(-v[1]), a[1])
                else: push(None, a[1])
            elif op in (0x79, 0x7b, 0x7d):  # lshl lshr lushr
                popv(); popv(); push(None, 2)
            elif op in (0x7f, 0x81, 0x83):  # land lor lxor
                popv(); popv(); push(None, 2)
            else:
                b = pop(); a = pop()
                k = (op - 0x60) % 4
                push(None, 2 if k in (1, 3) else 1)
        elif op == 0x84: loc[arg[0]] = None
        elif 0x85 <= op <= 0x93:
            a = popv()
            res_sz = {0x85: 2, 0x86: 1, 0x87: 2, 0x88: 1, 0x89: 1, 0x8a: 2, 0x8b: 1, 0x8c: 2, 0x8d: 2,
                      0x8e: 1, 0x8f: 2, 0x90: 1, 0x91: 1, 0x92: 1, 0x93: 1}[op]
            nv = None
            if a is not None and a[0] in ('i', 'f', 'l', 'd'):
                x = a[1]
                if op == 0x86: nv = ('f', float(x))
                elif op == 0x87: nv = ('d', float(x))
                elif op == 0x85: nv = ('l', x)
                elif op in (0x8b, 0x8e, 0x88): nv = _I(int(x))
                elif op == 0x8d: nv = ('d', float(x))
                elif op == 0x90: nv = ('f', float(x))
                elif op == 0x91: nv = _I(((int(x) + 128) & 0xff) - 128)
                elif op == 0x92: nv = _I(int(x) & 0xffff)
                elif op == 0x93: nv = _I(((int(x) + 32768) & 0xffff) - 32768)
            push(nv, res_sz)
        elif 0x94 <= op <= 0x98: popn(2); push(None)
        elif 0x99 <= op <= 0x9e or op in (0xc6, 0xc7): popv()
        elif 0x9f <= op <= 0xa6: popn(2)
        elif op in (0xa7, 0xc8): dead = True
        elif op in (0xaa, 0xab): popv(); dead = True
        elif 0xac <= op <= 0xb0: popv(); dead = True
        elif op == 0xb1: dead = True
        elif op in (0xb2, 0xb3, 0xb4, 0xb5):
            owner, name, desc = cf.ref(arg)
            if op == 0xb2:
                push(('fld', owner, name), tsize(desc))
                if on_field: on_field('getstatic', owner, name, desc, pc)
            elif op == 0xb3:
                popv()
                if on_field: on_field('putstatic', owner, name, desc, pc)
            elif op == 0xb4:
                ob = popv()
                push(('fld', owner, name) if ob == ('this',) else None, tsize(desc))
                if on_field: on_field('getfield', owner, name, desc, pc)
            else:
                v = popv(); ob = popv()
                if on_putfield: on_putfield(owner, name, desc, v, ob, pc)
                if on_field: on_field('putfield', owner, name, desc, pc)
        elif op in (0xb6, 0xb7, 0xb8, 0xb9):
            owner, name, desc = cf.ref(arg)
            ps2, r = parse_desc(desc)
            args = popn(len(ps2))
            recv = None if op == 0xb8 else popv()
            if on_invoke: on_invoke(op, owner, name, desc, args, recv, pc)
            if r != 'V': push(None, tsize(r))
        elif op == 0xba:
            bsm, nti = cf.cp[arg][1], cf.cp[arg][2]
            nt = cf.cp[nti]; desc = cf.cp[nt[2]][1]
            ps2, r = parse_desc(desc); popn(len(ps2))
            if r != 'V': push(None, tsize(r))
        elif op == 0xbb: push(None)
        elif op in (0xbc, 0xbd): popv(); push(None)
        elif op == 0xbe: popv(); push(None)
        elif op == 0xbf: popv(); dead = True
        elif op == 0xc0: pass
        elif op == 0xc1: popv(); push(None)
        elif op in (0xc2, 0xc3): popv()
        elif op == 0xc5: popn(arg[1]); push(None)
        elif op in (0xa8, 0xa9, 0xc9): st = []
        else: pass

def load_jar(path, want_code=True, prefix=None):
    import zipfile
    z = zipfile.ZipFile(path); out = {}
    for n in z.namelist():
        if not n.endswith('.class'): continue
        if prefix and not n.startswith(prefix): continue
        try:
            cf = parse(z.read(n), want_code)
        except Exception as e:
            continue
        out[cf.name] = cf
    return out
