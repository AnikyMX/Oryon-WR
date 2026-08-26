#!/usr/bin/env python3
"""
gen_tests.py — membangkitkan tests/checks.inc dari ground-truth LWJGL.

Tes host memakai daftar yang SAMA dengan yang dipakai GLCapabilities di jalur
nyata, jadi lulus di sini berarti flag kapabilitas yang dijanjikan memang akan
menyala di perangkat.
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "data")
d = json.load(open(os.path.join(D, "oryon_exports.json")))
chk = json.load(open(os.path.join(D, "lwjgl_checks.json")))

exports = sorted(d["exports"])
# nama yang HARUS mengembalikan NULL: simbol GL sah tapi di luar lingkup.
lw = json.load(open(os.path.join(D, "lwjgl_symbols.json")))
outside = [s for s in lw["symbols"] if s not in set(exports)]
outside = outside[::max(1, len(outside) // 40)][:40]

out = ["/* Dihasilkan oleh tools/gen_tests.py - JANGAN DIEDIT MANUAL. */", ""]
out.append("static const char *const kExports[] = {")
out += ['    "%s",' % s for s in exports]
out += ["    0", "};", ""]
out.append("static const char *const kMustBeNull[] = {")
out += ['    "%s",' % s for s in outside]
out += ["    0", "};", ""]
for flag in d["profile"]:
    syms = chk.get(flag, [])
    ident = flag.replace(".", "_")
    out.append("static const char *const kReq_%s[] = {" % ident)
    out += ['    "%s",' % s for s in syms]
    out += ["    0", "};", ""]
out.append("static const struct { const char *name; const char *const *syms; } kFlags[] = {")
out += ['    { "%s", kReq_%s },' % (f, f.replace(".", "_")) for f in d["profile"]]
out += ["    { 0, 0 }", "};", ""]
open(os.path.join(ROOT, "tests", "checks.inc"), "w").write("\n".join(out))
print("tests/checks.inc:", len(exports), "ekspor,", len(outside), "harus NULL,",
      len(d["profile"]), "flag")
