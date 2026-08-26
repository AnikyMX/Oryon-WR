#!/usr/bin/env python3
"""
regen.py — menjalankan seluruh pembangkit sesuai urutan ketergantungannya.

Tahap 1-2 (ekstraksi dari jar) hanya perlu dijalankan ulang bila jar berubah;
hasilnya sudah ada di data/*.json dan ikut di-commit, sehingga repositori bisa
dibangun tanpa menyertakan berkas Minecraft maupun LWJGL.

    python3 tools/regen.py                 # tahap 3 saja (dari data/)
    python3 tools/regen.py --jars A.jar B.jar   # ekstraksi ulang dari nol
"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = os.path.join(ROOT, "tools")

EXTRACT = [
    ("extract_lwjgl_symbols.py", "LWJGL"),   # butuh lwjgl jar
    ("extract_mc_callsites.py", "MC"),       # butuh minecraft jar
]
DERIVE = [
    "resolve_symbols.py",     # call site -> simbol kanonik
    "build_export_set.py",    # + profil kapabilitas -> daftar ekspor
    "gen_gl_header.py",       # include/oryon/gl.h
    "gen_driver.py",          # src/driver/gles_procs.inc
    "gen_exports.py",         # gl_symbols.inc, hub_table.inc, oryon.map, SYMBOLS.md
    "gen_extensions.py",      # src/state/extensions.inc (+ validasi)
    "gen_sources.py",         # src/gl/gen_*.cpp
    "gen_tests.py",           # tests/checks.inc
]


def run(script, *args):
    print("\n>>> %s %s" % (script, " ".join(args)))
    r = subprocess.run([sys.executable, os.path.join(T, script), *args], cwd=ROOT)
    if r.returncode:
        sys.exit("GAGAL pada %s" % script)


jars = []
if "--jars" in sys.argv:
    jars = sys.argv[sys.argv.index("--jars") + 1:]
    if len(jars) != 2:
        sys.exit("pakai: --jars <lwjgl.jar> <minecraft.jar>")

if jars:
    run(EXTRACT[0][0], jars[0])
    run(EXTRACT[1][0], jars[1], "1.12.2")
    run("extract_mc_enums.py", jars[1])
    run("audit_enums.py", jars[1])      # gagal bila ada enum non-GLES tak tertangani
else:
    print("(melewati ekstraksi jar; memakai data/*.json yang sudah ada)")

for s in DERIVE:
    run(s)

print("\nSemua pembangkit selesai.")
