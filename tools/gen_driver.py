#!/usr/bin/env python3
"""
gen_driver.py — membangun tabel penunjuk fungsi driver GLES 3.2.

Diambil apa adanya dari GLES3/gl32.h sandbox. Seluruh permukaan inti GLES 3.2
dimuat sekali saat init (satu dlsym per simbol, mikrodetik) sehingga tidak ada
lagi pencarian simbol di jalur gambar. Simbol yang tidak ada di perangkat
dibiarkan NULL, bukan menggagalkan pemuatan.
"""
import os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RE = re.compile(r"GL_APICALL\s+([\w\s\*]+?)\s*\bGL_APIENTRY\s+(gl\w+)\s*\(([^;]*?)\)\s*;", re.S)
_NAME = re.compile(r"([A-Za-z_]\w*)\s*((?:\[\s*\w*\s*\])*)$")


def split_params(s):
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


src = open("/usr/include/GLES3/gl32.h", errors="replace").read()
src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
funcs = {}
for ret, name, args in RE.findall(src):
    ret = " ".join(ret.split())
    args = " ".join(args.split())
    funcs.setdefault(name, (ret, args))

types = set()
for ret, args in funcs.values():
    for t in re.findall(r"\b(GL\w+)\b", ret + " " + args):
        types.add(t)
hdr = open(os.path.join(ROOT, "include/oryon/gl.h")).read()
known = set(re.findall(r"\b(GL\w+)\s*[;)]", hdr))
missing = sorted(t for t in types if t not in known)

lines = ["/* Dihasilkan oleh tools/gen_driver.py - JANGAN DIEDIT MANUAL. */",
         "/* Permukaan inti GLES 3.2 dari GLES3/gl32.h. %d fungsi. */" % len(funcs),
         "", "#ifndef ORYON_GLES",
         "#error \"definisikan ORYON_GLES sebelum menyertakan berkas ini\"",
         "#endif", ""]
for n in sorted(funcs):
    ret, args = funcs[n]
    a = args if args not in ("", "void") else "void"
    names = [] if a == "void" else [_NAME.search(p).group(1) for p in split_params(a)]
    lines.append("ORYON_GLES(%s, %s, (%s), (%s))" % (ret, n, a, ", ".join(names)))
lines += ["", "#undef ORYON_GLES", ""]

out = os.path.join(ROOT, "src", "driver", "gles_procs.inc")
open(out, "w").write("\n".join(lines))
print("fungsi GLES 3.2 :", len(funcs))
print("tipe tak dikenal:", missing or "tidak ada")
print("->", out)
