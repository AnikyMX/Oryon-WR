#!/usr/bin/env python3
"""
audit_enums.py — setiap enum GL yang dipakai Minecraft, disilangkan dengan GLES.

Ini lahir dari satu bug nyata: Minecraft memanggil
`glTexImage2D(GL_PROXY_TEXTURE_2D, ...)` untuk mengukur tekstur maksimum.
Oryon meneruskannya apa adanya, GLES menolaknya dengan GL_INVALID_ENUM, ukuran
atlas menjadi -1x-1, dan game crash saat menjahit tekstur.

Enum yang tidak ada di GLES tidak boleh pernah sampai ke driver. Alat ini
mendaftar semuanya dan menandai mana yang belum disebut di dalam src/, supaya
kelas bug ini terlihat sebagai daftar, bukan sebagai kejutan.

Keluaran: data/mc_gl_enums.json
"""
import collections, json, os, re, sys, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jclass

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "data")
JAR = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("MC_JAR")
if not JAR:
    sys.exit("pakai: audit_enums.py <1.12.2.jar>")

DEF = re.compile(r"^#define\s+(GL_\w+)\s+(0[xX][0-9A-Fa-f]+|\d+)\s*$", re.M)

gl = {}
for m in DEF.finditer(open(os.path.join(ROOT, "include/oryon/gl.h")).read()):
    gl.setdefault(int(m.group(2), 0), []).append(m.group(1))

gles = set()
for h in ("/usr/include/GLES3/gl32.h", "/usr/include/GLES2/gl2ext.h"):
    if os.path.exists(h):
        for m in DEF.finditer(open(h).read()):
            gles.add(m.group(1))

# Apa saja yang sudah disebut namanya di dalam sumber Oryon.
src_text = []
for base, _, files in os.walk(os.path.join(ROOT, "src")):
    for f in files:
        if f.endswith((".cpp", ".h", ".inc")):
            src_text.append(open(os.path.join(base, f), errors="replace").read())
src_all = "\n".join(src_text)

z = zipfile.ZipFile(JAR)
count = collections.Counter()
where = collections.defaultdict(set)
for e in z.namelist():
    if not e.endswith(".class"):
        continue
    try:
        cf = jclass.parse(z.read(e), keep_code=True)
    except Exception:
        continue
    if not any(r[1].startswith("org/lwjgl/") for r in cf.refs):
        continue
    for _, _, code in cf.methods:
        if not code:
            continue
        for _, op, a in jclass.walk(code):
            v = jclass.push_value(cf, op, a)
            if v is not None and v in gl and v >= 0x200:
                count[v] += 1
                where[v].add(e)
z.close()

# Enum yang memang tidak perlu ditangani, dengan alasannya masing-masing.
# Daftar ini pendek dan setiap barisnya harus bisa dipertanggungjawabkan.
BENIGN = {
    "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER":
        "nilai KEMBALIAN glCheckFramebufferStatus, dipetakan ke string; bukan masukan",
    "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER":
        "nilai KEMBALIAN glCheckFramebufferStatus, dipetakan ke string; bukan masukan",
    "GL_COEFF":
        "positif palsu: 2560 dipakai sebagai lebar layar di konstruktor DisplayMode",
    "GL_HINT_BIT":
        "bit mask glPushAttrib; granularitas mask memang diabaikan",
    "GL_COPY":
        "argumen glLogicOp; GLES tidak punya operasi logika, jadi no-op",
}

rows, missing = [], []
for v, c in count.most_common():
    names = gl[v]
    in_gles = any(n in gles for n in names)
    handled = any(re.search(r"\b%s\b" % n, src_all) for n in names)
    row = {"value": hex(v), "names": names[:3], "count": c,
           "classes": len(where[v]), "in_gles": in_gles, "handled": handled}
    rows.append(row)
    benign = any(n in BENIGN for n in names)
    row["benign"] = benign
    if not in_gles and not handled and not benign:
        missing.append(row)

json.dump({"source": os.path.basename(JAR), "total": len(rows),
           "benign": BENIGN,
           "not_in_gles": sum(1 for r in rows if not r["in_gles"]),
           "unhandled": missing, "enums": rows},
          open(os.path.join(D, "mc_gl_enums.json"), "w"), indent=1)

print("enum GL dipakai Minecraft :", len(rows))
print("tidak ada di GLES         :", sum(1 for r in rows if not r["in_gles"]))
print("dikecualikan (beralasan)  :", sum(1 for r in rows if r.get("benign")))
print("belum ditangani           :", len(missing))
for r in missing:
    print("  %-10s x%-4d %s" % (r["value"], r["count"], ", ".join(r["names"])))
if missing:
    sys.exit(1)
