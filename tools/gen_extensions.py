#!/usr/bin/env python3
"""
gen_extensions.py — membangun string GL_EXTENSIONS dan MEMVALIDASINYA.

Gagal keras bila ada ekstensi yang diiklankan tetapi entry point-nya tidak
lengkap di daftar ekspor. Ini mencegah kelas bug paling menyakitkan: LWJGL
menyalakan sebuah jalur kode, lalu Minecraft crash di panggilan pertama.
"""
import json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "data")

chk = json.load(open(os.path.join(D, "lwjgl_checks.json")))
exp = set(json.load(open(os.path.join(D, "oryon_exports.json")))["exports"])

names = []
for line in open(os.path.join(D, "extensions.txt")):
    line = line.split("#")[0].strip()
    if line:
        names.append(line)

bad = []
for n in names:
    req = chk.get(n[3:])
    if req and not set(req) <= exp:
        bad.append((n, sorted(set(req) - exp)[:5], len(set(req) - exp)))
if bad:
    print("GAGAL: ekstensi diiklankan tanpa entry point lengkap", file=sys.stderr)
    for n, sample, k in bad:
        print("  %s: %d hilang, mis. %s" % (n, k, ", ".join(sample)), file=sys.stderr)
    sys.exit(1)

names.sort()
out = ["/* Dihasilkan oleh tools/gen_extensions.py - JANGAN DIEDIT MANUAL. */",
       "/* Divalidasi: tiap ekstensi di sini punya entry point lengkap. */", "",
       "#define ORYON_EXTENSIONS \\"]
out += ['    "%s " \\' % n for n in names[:-1]]
out += ['    "%s"' % names[-1], ""]
open(os.path.join(ROOT, "src", "state", "extensions.inc"), "w").write("\n".join(out))
print("ekstensi diiklankan:", len(names), "(tervalidasi)")
