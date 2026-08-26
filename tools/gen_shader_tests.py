#!/usr/bin/env python3
"""
gen_shader_tests.py — mengemas shader bawaan Minecraft menjadi tests/mc_shaders.inc.

Berkas hasilnya TIDAK di-commit (lihat .gitignore): isinya aset Mojang. Tes
tetap berjalan tanpa berkas ini dengan memakai shader sintetis yang memakai
konstruksi yang sama; kalau jar tersedia, tes memakai sumber aslinya.
"""
import json, os, sys, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JAR = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("MC_JAR")
if not JAR:
    sys.exit("pakai: gen_shader_tests.py <1.12.2.jar>")

z = zipfile.ZipFile(JAR)
names = sorted(n for n in z.namelist() if n.endswith((".vsh", ".fsh")))

def esc(s):
    out = []
    for line in s.splitlines():
        out.append('    "%s\\n"' % line.replace("\\", "\\\\").replace('"', '\\"'))
    return "\n".join(out) or '    ""'

lines = ["/* Dihasilkan oleh tools/gen_shader_tests.py - JANGAN DI-COMMIT. */",
         "/* %d shader dari assets/minecraft/shaders. */" % len(names), ""]
for i, n in enumerate(names):
    src = z.read(n).decode("utf-8", "replace")
    lines.append("static const char kMcShader%d[] =" % i)
    lines.append(esc(src) + ";")
    lines.append("")
lines.append("static const struct { const char *name; const char *src; bool frag; } "
             "kMcShaders[] = {")
for i, n in enumerate(names):
    lines.append('    { "%s", kMcShader%d, %s },'
                 % (n.split("/")[-1], i, "true" if n.endswith(".fsh") else "false"))
lines += ["};", ""]
z.close()

out = os.path.join(ROOT, "tests", "mc_shaders.inc")
open(out, "w").write("\n".join(lines))
print("tests/mc_shaders.inc:", len(names), "shader,", os.path.getsize(out), "byte")
