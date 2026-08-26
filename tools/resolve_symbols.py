#!/usr/bin/env python3
"""
resolve_symbols.py — memetakan call site LWJGL milik Minecraft ke SIMBOL GL nyata.

LWJGL punya overload Java yang bukan nama simbol (glUniform1, glFog, glGetInteger, ...).
Pemetaan digerakkan oleh DESKRIPTOR method yang sudah diekstrak, bukan tebakan:
  FloatBuffer -> ...fv | IntBuffer -> ...iv | pengembalian skalar -> ...v
Setiap hasil diverifikasi terhadap 2225 simbol kanonik GLCapabilities.

Keluaran: data/mc_symbols.json
"""
import json, os, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "data")

lw = json.load(open(os.path.join(D, "lwjgl_symbols.json")))
mc = json.load(open(os.path.join(D, "mc_callsites.json")))
CANON = set(lw["symbols"])

GLPFX = ("GL1", "GL2", "GL3", "GL4", "ARB", "EXT", "NV", "AMD", "ATI", "APPLE")

# basis -> bentuk vektor. Hanya untuk nama yang BUKAN simbol kanonik.
_VEC = {
    "glFog": "glFog", "glLight": "glLight", "glLightModel": "glLightModel",
    "glTexEnv": "glTexEnv", "glTexGen": "glTexGen", "glMaterial": "glMaterial",
}

# Nama yang sudah membawa huruf tipe di dalamnya; cukup ditambah "v".
_ALREADY_TYPED = ("glGetFloat", "glGetInteger", "glGetBoolean", "glGetDouble",
                  "glGetInteger64")


def suffix_for(descs):
    """Tentukan akhiran dari tipe buffer di deskriptor."""
    s = set()
    for d in descs:
        if "FloatBuffer" in d:
            s.add("f")
        if "IntBuffer" in d:
            s.add("i")
        if "DoubleBuffer" in d:
            s.add("d")
        if "ShortBuffer" in d:
            s.add("s")
        if "Buffer" not in d:
            s.add("*")          # overload skalar
    return s


def resolve(name, descs):
    """-> (himpunan simbol, alasan)"""
    if name in CANON:
        return {name}, "langsung"

    base, tail = name, ""
    for t in ("ARB", "EXT", "NV", "ATI", "AMD", "APPLE"):
        if name.endswith(t):
            base, tail = name[:-len(t)], t
            break

    sfx = suffix_for(descs)
    out = set()

    # nama sudah bertipe: glGetFloat -> glGetFloatv
    if base in _ALREADY_TYPED:
        c = base + "v" + tail
        if c in CANON:
            return {c}, "nama sudah bertipe -> +v"

    # glGetXxxi / glGetXxxiARB  ->  glGetXxxiv (+tail)
    if base.startswith("glGet") and base.endswith("i"):
        c = base + "v" + tail
        if c in CANON:
            return {c}, "skalar->vektor (i -> iv)"

    # glUniformN / glUniformMatrixN
    if base.startswith("glUniformMatrix"):
        for k in ("f", "d"):
            if k in sfx or k == "f":
                c = base + k + "v" + tail
                if c in CANON:
                    out.add(c)
        if out:
            return out, "matriks uniform -> bentuk fv"
    if base.startswith("glUniform"):
        for k in sorted(sfx - {"*"}) or ["f"]:
            c = base + k + "v" + tail
            if c in CANON:
                out.add(c)
        if out:
            return out, "uniform vektor"

    # glFog/glLight/glTexEnv/glGetInteger/... -> +{f,i,d}v
    if base in _VEC:
        for k in sorted(sfx - {"*"}) or ["f"]:
            c = base + k + "v" + tail
            if c in CANON:
                out.add(c)
        if "*" in sfx:                       # overload skalar tetap pakai bentuk v
            for k in sorted(sfx - {"*"}) or ["i", "f"]:
                c = base + k + "v" + tail
                if c in CANON:
                    out.add(c)
        if out:
            return out, "overload Java -> bentuk vektor"

    # glMultMatrix -> glMultMatrixf/d
    if base in ("glMultMatrix", "glLoadMatrix"):
        for k in sorted(sfx - {"*"}) or ["f"]:
            c = base + k + tail
            if c in CANON:
                out.add(c)
        if out:
            return out, "matriks -> bentuk bertipe"

    return set(), "TIDAK TERSELESAIKAN"


syms, why, per_class, unresolved = {}, {}, collections.defaultdict(set), []
for key, desc_list in sorted(mc["method_descs"].items()):
    cls, m = key.split(".", 1)
    if not cls.startswith(GLPFX):
        continue
    got, reason = resolve(m, desc_list)
    if not got:
        unresolved.append(key)
        continue
    for s in got:
        syms.setdefault(s, []).append(key)
        per_class[cls].add(s)
    why[key] = {"symbols": sorted(got), "reason": reason}

BOOTSTRAP = ["glGetError", "glGetString", "glGetIntegerv"]   # wajib atau crash
for b in BOOTSTRAP:
    syms.setdefault(b, []).append("<bootstrap LWJGL>")

data = {
    "canonical_source": lw["source"],
    "callsite_source": mc["source"],
    "bootstrap": BOOTSTRAP,
    "symbol_count": len(syms),
    "symbols": {k: sorted(set(v)) for k, v in sorted(syms.items())},
    "resolution": why,
    "per_class": {k: sorted(v) for k, v in sorted(per_class.items())},
    "unresolved": unresolved,
}
json.dump(data, open(os.path.join(D, "mc_symbols.json"), "w"), indent=1)

print("simbol GL yang benar-benar dipanggil MC :", len(syms))
print("call site terselesaikan                 :", len(why))
print("TIDAK terselesaikan                     :", len(unresolved), unresolved)
print("-> data/mc_symbols.json")
print()
for k, v in sorted(per_class.items()):
    print(f"  {k:24s} {len(v):3d}")
