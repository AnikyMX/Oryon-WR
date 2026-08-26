#!/usr/bin/env python3
"""
extract_lwjgl_symbols.py — sumber kebenaran SIMBOL GL.

LWJGL 3 menyimpan setiap alamat entry point GL sebagai field `public final long`
di org/lwjgl/opengl/GLCapabilities. Nama field == nama simbol GL persis.
Kelas GLxxC/ARBxxx merujuk field itu lewat Fieldref, sehingga kita bisa
memetakan: kelas LWJGL -> himpunan simbol GL yang benar-benar dipakainya.

Keluaran: data/lwjgl_symbols.json
"""
import json, os, sys, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jclass

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JAR = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("LWJGL_JAR")
OUT = os.path.join(ROOT, "data", "lwjgl_symbols.json")
CAPS = "org/lwjgl/opengl/GLCapabilities"

z = zipfile.ZipFile(JAR)

# 1) daftar simbol GL kanonik = field long di GLCapabilities
caps = jclass.parse(z.read(CAPS + ".class"))
symbols = sorted(n for n, d in caps.fields if d == "J" and n.startswith("gl"))
flags = sorted(n for n, d in caps.fields if d == "Z")

# 2) peta kelas -> simbol, dan method -> simbol (via Fieldref ke GLCapabilities)
class_syms, method_owner = {}, {}
for entry in z.namelist():
    if not entry.startswith("org/lwjgl/opengl/") or not entry.endswith(".class"):
        continue
    cf = jclass.parse(z.read(entry))
    used = sorted({r[2] for r in cf.refs
                   if r[0] == 9 and r[1] == CAPS and r[3] == "J"})
    if used:
        class_syms[cf.this_name.rsplit("/", 1)[-1]] = used
    for mname, mdesc in cf.methods:
        method_owner.setdefault(cf.this_name.rsplit("/", 1)[-1], set()).add(mname)

z.close()

data = {
    "source": os.path.basename(JAR),
    "symbol_count": len(symbols),
    "symbols": symbols,
    "capability_flags": flags,
    "class_symbols": class_syms,
    "class_methods": {k: sorted(v) for k, v in method_owner.items()},
}
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as f:
    json.dump(data, f, indent=1)

print("simbol GL kanonik :", len(symbols))
print("flag kapabilitas  :", len(flags))
print("kelas ber-simbol  :", len(class_syms))
print("-> " + OUT)
