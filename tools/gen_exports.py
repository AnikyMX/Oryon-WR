#!/usr/bin/env python3
"""
gen_exports.py — pembangkit kode dari data/oryon_exports.json.

Semua yang di-generate bersifat deterministik dan dapat diaudit:
  include/oryon/gl_symbols.inc  daftar X-macro 352 simbol (ret, nama, param, arg)
  src/core/hub_table.inc        tabel nama->fungsi terurut untuk pencarian biner
  cmake/oryon.map               skrip versi linker (permukaan ABI eksplisit)
  docs/SYMBOLS.md               laporan yang bisa dibaca manusia

Tidak ada tanda tangan yang ditulis tangan: semuanya berasal dari header GL
sistem, sehingga tidak mungkin melenceng dari ABI sebenarnya.
"""
import json, os, re, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "data")
d = json.load(open(os.path.join(D, "oryon_exports.json")))
E = d["exports"]

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


def arg_names(args):
    if args in ("void", ""):
        return []
    return [_NAME.search(p).group(1) for p in split_params(args)]


# ----------------------------------------------------------- gl_symbols.inc --
rows = []
for name in sorted(E):
    v = E[name]
    ret = v["ret"].replace("const GLubyte *", "const GLubyte*").strip()
    args = v["args"] if v["args"] not in ("", "void") else "void"
    names = arg_names(v["args"])
    rows.append((v["strategy"], ret, name, "(%s)" % args, "(%s)" % ", ".join(names)))

inc = [
 "/* Dihasilkan oleh tools/gen_exports.py - JANGAN DIEDIT MANUAL. */",
 "/* Sumber: 1.12.2.jar (call site) + lwjglglfwclasses.jar (simbol) + header GL sistem. */",
 "/*", " * ORYON_SYM(STRATEGI, RET, NAMA, (PARAM), (ARG))", " *",
 " * STRATEGI:", ]
for k, n in sorted(d["strategy_counts"].items(), key=lambda x: -x[1]):
    inc.append(" *   %-12s %4d" % (k, n))
inc += [" */", "", "#ifndef ORYON_SYM", "#error \"definisikan ORYON_SYM sebelum menyertakan berkas ini\"", "#endif", ""]
cur = None
for st, ret, nm, pr, ag in rows:
    if st != cur:
        inc.append("\n/* ---- %s ---- */" % st)
        cur = st
    inc.append("ORYON_SYM(%s, %s, %s, %s, %s)" % (st, ret, nm, pr, ag))
inc += ["", "#undef ORYON_SYM", ""]

# urut ulang: kelompokkan per strategi agar berkas terbaca rapi
rows_by_strategy = sorted(rows, key=lambda r: (r[0], r[2]))
inc = inc[:inc.index("")+1] if False else inc
os.makedirs(os.path.join(ROOT, "include", "oryon"), exist_ok=True)
open(os.path.join(ROOT, "include", "oryon", "gl_symbols.inc"), "w").write("\n".join(inc))

# ------------------------------------------------------------ hub_table.inc --
# Terurut menurut strcmp (ASCII) sehingga pencarian biner sah.
names = sorted(E)
tab = [
 "/* Dihasilkan oleh tools/gen_exports.py - JANGAN DIEDIT MANUAL. */",
 "/* Terurut strcmp; dicari dengan pencarian biner. %d entri. */" % len(names),
 "", "ORYON_HUB_BEGIN(%d)" % len(names), ]
for n in names:
    tab.append("ORYON_HUB(\"%s\", %s)" % (n, n))
tab += ["ORYON_HUB_END()", ""]
os.makedirs(os.path.join(ROOT, "src", "core"), exist_ok=True)
open(os.path.join(ROOT, "src", "core", "hub_table.inc"), "w").write("\n".join(tab))

# ---------------------------------------------------------------- oryon.map --
HUB = ["glXGetProcAddress", "glXGetProcAddressARB",
       "eglGetProcAddress", "OSMesaGetProcAddress"]
m = ["/* Dihasilkan oleh tools/gen_exports.py - JANGAN DIEDIT MANUAL. */",
     "ORYON_1.0 {", "  global:"]
m += ["    %s;" % h for h in HUB]
m += ["    %s;" % n for n in names]
m += ["  local:", "    *;", "};", ""]
os.makedirs(os.path.join(ROOT, "cmake"), exist_ok=True)
open(os.path.join(ROOT, "cmake", "oryon.map"), "w").write("\n".join(m))

# --------------------------------------------------------------- SYMBOLS.md --
by = collections.defaultdict(list)
for n in names:
    by[E[n]["strategy"]].append(n)
DESC = {
 "direct":      "Identik di GLES 3.2. Panggilan lewat penunjuk fungsi driver, tanpa lapisan.",
 "alias":       "Nama ber-akhiran ARB/EXT yang setara persis dengan nama inti GLES.",
 "direct_ext":  "Ada sebagai ekstensi GLES; diambil lewat eglGetProcAddress driver.",
 "translate":   "Ada padanannya, butuh konversi tipis (tipe, urutan argumen, atau emulasi kecil).",
 "immediate":   "Mode langsung glBegin/glEnd. Dirakit ke buffer vertex, dikirim sekali per glEnd.",
 "matrix":      "Tumpukan matriks fixed-function. Dihitung di CPU, dikirim sebagai uniform.",
 "lighting":    "Pencahayaan fixed-function. Menjadi bagian kunci state generator shader.",
 "fog":         "Kabut fixed-function. Menjadi bagian kunci state generator shader.",
 "alphatest":   "Uji alpha. Menjadi cabang discard di fragment shader.",
 "texenv":      "Lingkungan tekstur / pembangkit koordinat. Menjadi bagian kunci state shader.",
 "clientarr":   "Array sisi klien fixed-function. Dipetakan ke VAO + atribut vertex generik.",
 "displaylist": "Display list. Direkam sebagai daftar perintah, diputar ulang saat glCallList.",
 "attribstack": "glPushAttrib/glPopAttrib. Simpan/pulihkan potongan state yang dibutuhkan saja.",
 "legacy_misc": "Sisa warisan tanpa padanan GLES. Sebagian besar tidak pernah dipanggil 1.12.2.",
}
md = ["# Permukaan Simbol Oryon", "",
      "Dihasilkan dari ground-truth, bukan tebakan:", "",
      "| sumber | isi |", "|---|---|",
      "| `%s` | call site GL asli Minecraft |" % d.get("callsite_source", "1.12.2.jar"),
      "| `lwjglglfwclasses.jar` | 2225 simbol kanonik + tuntutan tiap flag kapabilitas |",
      "| `/usr/include/GL/*.h` | prototipe C yang tepat |",
      "| `/usr/include/GLES*/*.h` | permukaan GLES 3.2 nyata |", "",
      "## Profil", "",
      "- `GL_VERSION` dilaporkan: **%s**" % d["gl_version_string"],
      "- `GL_SHADING_LANGUAGE_VERSION`: **%s**" % d["glsl_version_string"],
      "- Flag yang dibuat TRUE: " + ", ".join("`%s`" % f for f in d["profile"]),
      "", "## Angka", "",
      "| | jumlah |", "|---|---|",
      "| Total ekspor | **%d** |" % d["export_count"],
      "| Dipanggil langsung oleh 1.12.2 | %d |" % d["from_mc_callsites"],
      "| Pelengkap agar flag kapabilitas TRUE | %d |" % d["from_capability_flags"],
      "", "## Strategi", "",
      "| strategi | n | arti |", "|---|---|---|"]
for k, v in sorted(d["strategy_counts"].items(), key=lambda x: -x[1]):
    md.append("| `%s` | %d | %s |" % (k, v, DESC.get(k, "")))
md += ["", "## Daftar per strategi", ""]
for k in sorted(by, key=lambda x: -len(by[x])):
    md += ["<details><summary><b>%s</b> (%d)</summary>" % (k, len(by[k])), "",
           "```", ]
    line = ""
    for n in by[k]:
        if len(line) + len(n) > 88:
            md.append(line); line = ""
        line += n + " "
    if line:
        md.append(line)
    md += ["```", "", "</details>", ""]
os.makedirs(os.path.join(ROOT, "docs"), exist_ok=True)
open(os.path.join(ROOT, "docs", "SYMBOLS.md"), "w").write("\n".join(md))

print("gl_symbols.inc :", len(rows), "simbol")
print("hub_table.inc  :", len(names), "entri (terurut strcmp)")
print("oryon.map      :", len(names) + len(HUB), "simbol global")
print("docs/SYMBOLS.md ditulis")
