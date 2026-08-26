#!/usr/bin/env python3
"""
extract_mc_enums.py — ARGUMEN KONSTAN di tiap call site GL Minecraft.

Mengetahui simbol apa yang dipanggil belum cukup: generator shader perlu tahu
NILAI apa yang dilewatkan. glEnable(GL_FOG) berarti kabut harus ada; glFogi
dengan GL_EXP2 berarti hanya satu mode kabut yang perlu ditulis.

Caranya: telusuri bytecode, simpan jendela konstanta yang baru saja didorong ke
stack, lalu saat bertemu invokestatic ke org/lwjgl/opengl/*, ambil sebanyak
jumlah argumen int milik method itu. Heuristik, tetapi hanya melihat konstanta
yang benar-benar berdampingan dengan panggilan - jadi kebisingannya rendah.

Keluaran: data/mc_enums.json
"""
import collections, json, os, re, sys, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jclass

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "data")
JAR = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("MC_JAR")

# nama enum GL dari header mandiri yang sudah kita bangkitkan
ENUM = {}
for m in re.finditer(r"^#define\s+(GL_\w+)\s+(0[xX][0-9A-Fa-f]+|\d+)\s*$",
                     open(os.path.join(ROOT, "include/oryon/gl.h")).read(), re.M):
    v = int(m.group(2), 0)
    ENUM.setdefault(v, []).append(m.group(1))

ICONST = {0x02: -1, 0x03: 0, 0x04: 1, 0x05: 2, 0x06: 3, 0x07: 4, 0x08: 5}


def int_args(desc):
    """Berapa argumen bertipe int di deskriptor method."""
    inner = desc[1:desc.index(")")]
    n, i = 0, 0
    while i < len(inner):
        c = inner[i]
        if c == "L":
            i = inner.index(";", i) + 1
        elif c == "[":
            i += 1
            continue
        else:
            if c == "I":
                n += 1
            i += 1
    return n



# ---------------------------------------------------------------- pass 2 ----
# GlStateManager milik Minecraft meneruskan nilai dari pemanggilnya, sehingga
# konstanta jarang berdampingan dengan call site. Pass kedua ini memindai SEMUA
# konstanta int di kelas yang menyentuh org/lwjgl/opengl, lalu menyaringnya ke
# daftar enum yang benar-benar mengubah bentuk shader fixed-function.

INTERESTING = """
GL_LINEAR GL_EXP GL_EXP2
GL_MODULATE GL_REPLACE GL_DECAL GL_BLEND GL_ADD GL_COMBINE
GL_OBJECT_LINEAR GL_EYE_LINEAR GL_SPHERE_MAP GL_REFLECTION_MAP GL_NORMAL_MAP
GL_NEVER GL_LESS GL_EQUAL GL_LEQUAL GL_GREATER GL_NOTEQUAL GL_GEQUAL GL_ALWAYS
GL_ALPHA_TEST GL_LIGHTING GL_FOG GL_TEXTURE_2D GL_COLOR_MATERIAL GL_NORMALIZE
GL_RESCALE_NORMAL GL_TEXTURE_GEN_S GL_TEXTURE_GEN_T GL_TEXTURE_GEN_R
GL_TEXTURE_GEN_Q GL_COLOR_LOGIC_OP GL_POLYGON_OFFSET_FILL GL_CULL_FACE
GL_DEPTH_TEST GL_SCISSOR_TEST GL_STENCIL_TEST GL_LINE_SMOOTH GL_DITHER
GL_LIGHT0 GL_LIGHT1 GL_LIGHT2 GL_LIGHT3
GL_FLAT GL_SMOOTH
GL_POINTS GL_LINES GL_LINE_STRIP GL_TRIANGLES GL_TRIANGLE_STRIP GL_TRIANGLE_FAN
GL_QUADS GL_QUAD_STRIP GL_POLYGON GL_LINE_LOOP
GL_AMBIENT GL_DIFFUSE GL_SPECULAR GL_POSITION GL_AMBIENT_AND_DIFFUSE
GL_LIGHT_MODEL_AMBIENT GL_SHININESS GL_EMISSION
GL_TEXTURE_ENV GL_TEXTURE_ENV_MODE GL_TEXTURE_FILTER_CONTROL GL_TEXTURE_LOD_BIAS
GL_FRONT GL_BACK GL_FRONT_AND_BACK GL_LINE GL_FILL
""".split()


def pass2(jarpath):
    import zipfile
    want = {}
    for n in INTERESTING:
        for v, names in ENUM.items():
            if n in names:
                want.setdefault(v, n)
    z = zipfile.ZipFile(jarpath)
    hits = collections.Counter()
    classes = collections.defaultdict(set)
    for e in z.namelist():
        if not e.endswith(".class"):
            continue
        try:
            cf = jclass.parse(z.read(e), keep_code=True)
        except Exception:
            continue
        if not any(r[1].startswith("org/lwjgl/opengl/") for r in cf.refs):
            continue
        for _, _, code in cf.methods:
            if not code:
                continue
            for _, op, a in jclass.walk(code):
                v = jclass.push_value(cf, op, a)
                if v is not None and v in want:
                    hits[want[v]] += 1
                    classes[want[v]].add(e)
    z.close()
    return hits, classes

z = zipfile.ZipFile(JAR)
per_call = collections.defaultdict(collections.Counter)
n_cls = 0

for e in z.namelist():
    if not e.endswith(".class"):
        continue
    try:
        cf = jclass.parse(z.read(e), keep_code=True)
    except Exception:
        continue
    n_cls += 1
    for _, _, code in cf.methods:
        if not code:
            continue
        window = []
        for _, op, a in jclass.walk(code):
            if op in (0x12, 0x13, 0x10, 0x11) or op in ICONST:
                window.append(jclass.push_value(cf, op, a))
            elif op in (0xb8, 0xb6, 0xb7, 0xb9):   # invoke*
                idx = int.from_bytes(a[:2], "big")
                ent = cf.cp[idx]
                if ent and ent[0] in (9, 10, 11):
                    owner = cf.cls_name(ent[1])
                    nat = cf.cp[ent[2]]
                    name, desc = cf.utf8(nat[1]), cf.utf8(nat[2])
                    if owner and owner.startswith("org/lwjgl/opengl/"):
                        k = owner.rsplit("/", 1)[-1] + "." + name
                        take = int_args(desc)
                        vals = [v for v in window[-take:] if isinstance(v, int)] if take else []
                        for v in vals:
                            per_call[k][v] += 1
                window = []
            else:
                if op not in (0x00,):              # instruksi lain memutus jendela
                    window = window[-4:]
z.close()

out = {}
for call, cnt in sorted(per_call.items()):
    rows = []
    for v, n in cnt.most_common():
        names = ENUM.get(v, [])
        rows.append({"value": v, "hex": hex(v), "names": names, "count": n})
    out[call] = rows

hits, classes = pass2(JAR)
tokens = {k: {"count": v, "classes": len(classes[k])} for k, v in hits.most_common()}

json.dump({"source": os.path.basename(JAR), "classes": n_cls,
           "calls": out, "tokens": tokens},
          open(os.path.join(D, "mc_enums.json"), "w"), indent=1)
print("kelas dipindai:", n_cls)
print("call site dengan argumen konstan:", len(out))
print("token GL fixed-function terdeteksi:", len(tokens))
print("-> data/mc_enums.json")
