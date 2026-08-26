"""
jclass.py — pembaca minimal berkas .class Java (constant pool, field, method, ref).
Murni python3, tanpa dependensi. Dipakai sebagai alat ekstraksi ground-truth Oryon.

Hanya mem-parse sampai bagian yang dibutuhkan; atribut dilewati dengan skip panjang.
"""
import struct

# tag -> ukuran tetap (byte) setelah tag. Utf8(1), Long(5), Double(6) ditangani khusus.
_FIXED = {3: 4, 4: 4, 7: 2, 8: 2, 9: 4, 10: 4, 11: 4, 12: 4,
          15: 3, 16: 2, 17: 4, 18: 4, 19: 2, 20: 2}
_REF_TAGS = (9, 10, 11)  # Fieldref, Methodref, InterfaceMethodref

_u2 = struct.Struct(">H").unpack_from
_u4 = struct.Struct(">I").unpack_from


class ClassFile:
    __slots__ = ("cp", "this_name", "super_name", "fields", "methods", "refs")

    def utf8(self, i):
        e = self.cp[i]
        return e[1] if e and e[0] == 1 else None

    def cls_name(self, i):
        e = self.cp[i]
        return self.utf8(e[1]) if e and e[0] == 7 else None


def parse(buf, keep_code=False):
    if buf[:4] != b"\xca\xfe\xba\xbe":
        raise ValueError("bukan berkas .class")
    p = 8
    (cpc,) = _u2(buf, p); p += 2
    cp = [None] * cpc
    i = 1
    while i < cpc:
        tag = buf[p]; p += 1
        if tag == 1:
            (n,) = _u2(buf, p); p += 2
            cp[i] = (1, buf[p:p + n].decode("utf-8", "replace")); p += n
        elif tag in (5, 6):
            cp[i] = (tag, None); p += 8; i += 1          # dua slot
        else:
            sz = _FIXED.get(tag)
            if sz is None:
                raise ValueError("tag CP tak dikenal: %d" % tag)
            if tag in _REF_TAGS or tag == 12:
                a, b = _u2(buf, p)[0], _u2(buf, p + 2)[0]
                cp[i] = (tag, a, b)
            elif tag in (7, 8, 16, 19, 20):
                cp[i] = (tag, _u2(buf, p)[0])
            elif tag == 3:      # Integer: nilainya PENTING - enum GL >= 0x8000
                cp[i] = (3, int.from_bytes(buf[p:p + 4], "big", signed=True))
            elif tag == 4:      # Float
                cp[i] = (4, struct.unpack_from(">f", buf, p)[0])
            else:
                cp[i] = (tag, None)
            p += sz
        i += 1

    cf = ClassFile()
    cf.cp = cp
    p += 2                                                # access_flags
    (this_i,) = _u2(buf, p); p += 2
    (super_i,) = _u2(buf, p); p += 2
    cf.this_name = cf.cls_name(this_i)
    cf.super_name = cf.cls_name(super_i) if super_i else None
    (nif,) = _u2(buf, p); p += 2 + 2 * nif

    def members(want_code):
        nonlocal p
        (n,) = _u2(buf, p); p += 2
        out = []
        for _ in range(n):
            p += 2
            (ni,) = _u2(buf, p); p += 2
            (di,) = _u2(buf, p); p += 2
            (na,) = _u2(buf, p); p += 2
            code = None
            for _ in range(na):
                (ani,) = _u2(buf, p); p += 2
                (alen,) = _u4(buf, p); p += 4
                if want_code and code is None and cf.utf8(ani) == "Code":
                    (clen,) = _u4(buf, p + 4)
                    code = buf[p + 8:p + 8 + clen]
                p += alen
            out.append((cf.utf8(ni), cf.utf8(di), code) if want_code
                       else (cf.utf8(ni), cf.utf8(di)))
        return out

    cf.fields = members(False)
    cf.methods = members(keep_code)

    # refs: (tag, owner, name, desc) dari seluruh constant pool
    refs = []
    for e in cp:
        if e and e[0] in _REF_TAGS:
            owner = cf.cls_name(e[1])
            nat = cp[e[2]]
            if owner and nat and nat[0] == 12:
                refs.append((e[0], owner, cf.utf8(nat[1]), cf.utf8(nat[2])))
    cf.refs = refs
    return cf


# ---------------------------------------------------------------- bytecode ---
# Panjang operand tiap opcode (byte, tidak termasuk opcode itu sendiri).
_OPLEN = [0] * 256
for _o in (0x10, 0x12, 0x15, 0x16, 0x17, 0x18, 0x19,
           0x36, 0x37, 0x38, 0x39, 0x3a, 0xa9, 0xbc):
    _OPLEN[_o] = 1
for _o in (0x11, 0x13, 0x14, 0x84, 0xb2, 0xb3, 0xb4, 0xb5,
           0xb6, 0xb7, 0xb8, 0xbb, 0xbd, 0xc0, 0xc1):
    _OPLEN[_o] = 2
for _o in range(0x99, 0xa9):          # ifeq..jsr
    _OPLEN[_o] = 2
for _o in (0xc6, 0xc7):                # ifnull / ifnonnull
    _OPLEN[_o] = 2
_OPLEN[0xc5] = 3                       # multianewarray
for _o in (0xb9, 0xba, 0xc8, 0xc9):    # invokeinterface/invokedynamic/goto_w/jsr_w
    _OPLEN[_o] = 4


def walk(code):
    """Iterasi instruksi: yield (pc, opcode, operand_bytes). Penuh, bukan tebakan."""
    n, p = len(code), 0
    while p < n:
        op = code[p]
        s = p + 1
        if op == 0xc4:                                   # wide
            wop = code[s]
            ln = 4 if wop == 0x84 else 2
            yield p, op, code[s:s + 1 + ln]
            p = s + 1 + ln
        elif op in (0xaa, 0xab):                         # tableswitch / lookupswitch
            q = s + ((4 - (s & 3)) & 3)
            if op == 0xaa:
                lo = int.from_bytes(code[q + 4:q + 8], "big", signed=True)
                hi = int.from_bytes(code[q + 8:q + 12], "big", signed=True)
                end = q + 12 + 4 * (hi - lo + 1)
            else:
                npairs = int.from_bytes(code[q + 4:q + 8], "big", signed=True)
                end = q + 8 + 8 * npairs
            yield p, op, code[q:end]
            p = end
        else:
            ln = _OPLEN[op]
            yield p, op, code[s:s + ln]
            p = s + ln


def method_strings(cf, code):
    """String literal (ldc / ldc_w) yang dimuat method, sesuai urutan eksekusi."""
    out = []
    for _, op, a in walk(code):
        if op == 0x12:
            i = a[0]
        elif op == 0x13:
            i = int.from_bytes(a, "big")
        else:
            continue
        e = cf.cp[i]
        if e and e[0] == 8:
            s = cf.utf8(e[1])
            if s is not None:
                out.append(s)
    return out


def method_refs(cf, code):
    """(opcode, owner, name, desc) untuk tiap invoke/getfield di dalam method."""
    out = []
    for _, op, a in walk(code):
        if op not in (0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9):
            continue
        i = int.from_bytes(a[:2], "big")
        e = cf.cp[i]
        if not e or e[0] not in _REF_TAGS:
            continue
        nat = cf.cp[e[2]]
        out.append((op, cf.cls_name(e[1]), cf.utf8(nat[1]), cf.utf8(nat[2])))
    return out


_ICONST = {0x02: -1, 0x03: 0, 0x04: 1, 0x05: 2, 0x06: 3, 0x07: 4, 0x08: 5}


def push_value(cf, op, a):
    """Nilai int yang didorong instruksi ini, atau None.

    ldc WAJIB ikut dibaca: seluruh enum GL >= 0x8000 (GL_BGRA, GL_TEXTURE0,
    GL_COMBINE, ...) melebihi jangkauan sipush, jadi javac menaruhnya di
    constant pool. Melewatkannya membuat separuh permukaan GL tak terlihat.
    """
    if op in _ICONST:
        return _ICONST[op]
    if op in (0x10, 0x11):                      # bipush / sipush
        return int.from_bytes(a, "big", signed=True)
    if op in (0x12, 0x13):                      # ldc / ldc_w
        i = a[0] if op == 0x12 else int.from_bytes(a, "big")
        e = cf.cp[i] if i < len(cf.cp) else None
        return e[1] if e and e[0] == 3 else None
    return None
