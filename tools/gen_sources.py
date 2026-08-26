#!/usr/bin/env python3
"""
gen_sources.py — membangkitkan titik ekspor yang belum ditulis tangan.

Sumber tunggal kebenaran soal "apa yang sudah diimplementasikan" adalah penanda
ORYON_IMPL(<nama>) di dalam src/gl/impl_*.cpp. Berkas yang dibangkitkan tidak
akan pernah menabrak implementasi tangan, jadi -Wl,--no-undefined selalu puas
tanpa risiko simbol ganda.

  gen_direct.cpp  passthrough murni ke driver (satu panggilan tak langsung)
  gen_alias.cpp   nama ARB/EXT -> nama inti GLES
  gen_stub.cpp    sisanya; berteriak di build debug bila benar-benar dipanggil
"""
import json, os, re, sys

def _split(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "data")
GLDIR = os.path.join(ROOT, "src", "gl")

d = json.load(open(os.path.join(D, "oryon_exports.json")))
E = d["exports"]

impl = set()
for fn in sorted(os.listdir(GLDIR)):
    if fn.startswith("impl_") and fn.endswith(".cpp"):
        txt = open(os.path.join(GLDIR, fn)).read()
        impl |= set(re.findall(r"ORYON_IMPL\((\w+)\)", txt))

shadowed = sorted(n for n, v in E.items()
                  if v["strategy"] == "alias" and n not in impl and v["gles"] in impl)
if shadowed:
    print("GAGAL: alias ini akan melewati implementasi tangan targetnya: %s"
          % ", ".join(shadowed), file=sys.stderr)
    print("       Tulis tangan juga aliasnya, atau hapus implementasi targetnya.",
          file=sys.stderr)
    sys.exit(1)

stray = sorted(impl - set(E))
if stray:
    print("GAGAL: ORYON_IMPL menandai simbol yang tidak diekspor: %s" % ", ".join(stray),
          file=sys.stderr)
    print("       Tambahkan ke daftar ekspor lewat data/, atau hapus implementasinya.",
          file=sys.stderr)
    sys.exit(1)

HEAD = ("/* Dihasilkan oleh tools/gen_sources.py - JANGAN DIEDIT MANUAL. */\n"
        "/* %s */\n\n"
        '#include "oryon/oryon.h"\n'
        '#include "../driver/driver.h"\n\n'
        "using namespace oryon;\n\n")


def zero(ret):
    return "" if ret.strip() == "void" else "    return (%s) 0;\n" % ret


direct, alias, stub = [], [], []
for name in sorted(E):
    v = E[name]
    if name in impl:
        continue
    ret, params, st = v["ret"], v["args"], v["strategy"]
    args = ", ".join(re.search(r"([A-Za-z_]\w*)\s*((?:\[\s*\w*\s*\])*)$", p).group(1)
                     for p in _split(params)) if params not in ("", "void") else ""
    sig = "ORYON_API %s %s(%s)" % (ret, name, params if params else "void")
    if st == "direct":
        direct.append("%s {\n    return gles.%s(%s);\n}\n" % (sig, name, args))
    elif st == "alias":
        direct_target = v["gles"]
        alias.append("%s {\n    return gles.%s(%s);\n}\n" % (sig, direct_target, args))
    else:
        stub.append("%s {\n    ORYON_LOG(\"belum diimplementasikan: %s [%s]\");\n%s}\n"
                    % (sig, name, st, zero(ret)))

for fname, note, body in (
        ("gen_direct.cpp", "Passthrough murni: identik di GLES 3.2.", direct),
        ("gen_alias.cpp",  "Alias ARB/EXT ke nama inti GLES 3.2.", alias),
        ("gen_stub.cpp",   "Belum diimplementasikan. Diganti bertahap oleh impl_*.cpp.", stub)):
    open(os.path.join(GLDIR, fname), "w").write(HEAD % note + "\n".join(body))
    print("%-16s %4d fungsi" % (fname, len(body)))

print("ditulis tangan  : %4d (%s)" % (len(impl), ", ".join(sorted(impl)[:6]) + (" ..." if len(impl) > 6 else "")))
print("total           : %4d / %d" % (len(direct) + len(alias) + len(stub) + len(impl), len(E)))
