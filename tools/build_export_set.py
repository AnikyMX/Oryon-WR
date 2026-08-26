#!/usr/bin/env python3
"""
build_export_set.py — merakit DAFTAR EKSPOR final liboryon.so.

Masukan (semuanya ground-truth, bukan tebakan):
  data/mc_symbols.json    simbol GL yang benar-benar dipanggil Minecraft 1.12.2
  data/lwjgl_checks.json  simbol yang dituntut tiap flag kapabilitas LWJGL
  /usr/include/GL/*.h     prototipe C desktop GL (sumber tanda tangan)
  /usr/include/GLES*/*.h  permukaan GLES 3.2 (menentukan strategi tiap simbol)

Keluaran:
  data/oryon_exports.json  daftar ekspor + tanda tangan + klasifikasi strategi
"""
import json, os, re, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "data")

# Profil kapabilitas terpilih: Core GL 2.1 + ARB_framebuffer_object.
PROFILE = ["GL12", "GL13", "GL14", "GL15", "GL20", "GL21", "ARB_framebuffer_object"]
GL_VERSION_STRING = "2.1 Oryon"
GLSL_VERSION_STRING = "1.20"

RE_GL = re.compile(
    r"GLAPI\s+([\w\s\*]+?)\s*\b(?:GL)?APIENTRY\s+(gl\w+)\s*\(([^;]*?)\)\s*;", re.S)
RE_GLES = re.compile(
    r"GL_APICALL\s+([\w\s\*]+?)\s*\bGL_APIENTRY\s+(gl\w+)\s*\(([^;]*?)\)\s*;", re.S)


def scan(path, rx):
    out = {}
    with open(path, "r", errors="replace") as f:
        src = f.read()
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    for ret, name, args in rx.findall(src):
        ret = " ".join(ret.split())
        args = " ".join(args.split())
        out.setdefault(name, (ret, args))
    return out


gl_sig = {}
for h in ("/usr/include/GL/gl.h", "/usr/include/GL/glext.h"):
    for k, v in scan(h, RE_GL).items():
        gl_sig.setdefault(k, v)

gles_core = set(scan("/usr/include/GLES3/gl32.h", RE_GLES))
gles_ext = set(scan("/usr/include/GLES2/gl2ext.h", RE_GLES))

# ---------------------------------------------------------------- kategori ---
def _pfx(*p):
    return tuple(p)

GROUPS = [
 ("immediate", _pfx("glBegin", "glEnd", "glVertex", "glColor3", "glColor4",
                    "glSecondaryColor", "glNormal3", "glTexCoord", "glMultiTexCoord",
                    "glArrayElement", "glEdgeFlag", "glIndex", "glRect", "glFogCoord")),
 ("matrix",    _pfx("glMatrixMode", "glLoadIdentity", "glLoadMatrix", "glMultMatrix",
                    "glPushMatrix", "glPopMatrix", "glTranslate", "glRotate", "glScale",
                    "glOrtho", "glFrustum", "glLoadTransposeMatrix",
                    "glMultTransposeMatrix")),
 ("lighting",  _pfx("glLight", "glMaterial", "glColorMaterial", "glShadeModel")),
 ("fog",       _pfx("glFog",)),
 ("alphatest", _pfx("glAlphaFunc",)),
 ("texenv",    _pfx("glTexEnv", "glTexGen")),
 ("clientarr", _pfx("glVertexPointer", "glColorPointer", "glNormalPointer",
                    "glTexCoordPointer", "glIndexPointer", "glEdgeFlagPointer",
                    "glSecondaryColorPointer", "glFogCoordPointer",
                    "glEnableClientState", "glDisableClientState",
                    "glClientActiveTexture", "glInterleavedArrays")),
 ("displaylist", _pfx("glNewList", "glEndList", "glCallList", "glGenLists",
                      "glDeleteLists", "glListBase", "glIsList")),
 ("attribstack", _pfx("glPushAttrib", "glPopAttrib", "glPushClientAttrib",
                      "glPopClientAttrib")),
 ("legacy_misc", _pfx("glPolygonMode", "glLogicOp", "glPolygonStipple",
                      "glGetPolygonStipple", "glClipPlane", "glGetClipPlane",
                      "glPixelTransfer", "glPixelZoom", "glPixelMap", "glDrawPixels",
                      "glCopyPixels", "glRasterPos", "glWindowPos", "glBitmap",
                      "glAccum", "glClearAccum", "glClearIndex", "glIndexMask",
                      "glSelectBuffer", "glFeedbackBuffer", "glRenderMode",
                      "glInitNames", "glPushName", "glPopName", "glLoadName",
                      "glPassThrough", "glMap1", "glMap2", "glMapGrid", "glEvalCoord",
                      "glEvalMesh", "glEvalPoint", "glGetMap", "glLineStipple",
                      "glGetTexImage", "glAreTexturesResident", "glPrioritizeTextures",
                      "glTexImage1D", "glTexSubImage1D", "glCopyTexImage1D",
                      "glCopyTexSubImage1D", "glGetTexGen", "glGetTexEnv",
                      "glGetLight", "glGetMaterial", "glGetPixelMap")),
]

# --------------------------------------------------------------- override ---
# Simbol yang NAMANYA ada di GLES 3.2 tetapi SEMANTIKNYA berbeda, atau yang
# Oryon wajib jadi sumber kebenarannya. Passthrough di sini = bug diam-diam.
OVERRIDE = {
    # Oryon yang menjawab, bukan driver.
    "glGetString": "query", "glGetError": "query", "glGetIntegerv": "query",
    "glGetFloatv": "query", "glGetBooleanv": "query", "glGetDoublev": "query",
    "glIsEnabled": "query", "glGetTexLevelParameteriv": "query",
    "glGetTexParameteriv": "query", "glGetTexParameterfv": "query",
    # GL_ALPHA_TEST / GL_LIGHTING / GL_FOG / GL_TEXTURE_2D tidak ada di GLES.
    "glEnable": "capbits", "glDisable": "capbits",
    # GL_QUADS + pengikatan program fixed-function terjadi di sini.
    "glDrawArrays": "draw", "glDrawElements": "draw",
    "glDrawRangeElements": "draw", "glMultiDrawArrays": "draw",
    "glMultiDrawElements": "draw",
    # Konversi format piksel & parameter tekstur.
    "glTexImage2D": "texture", "glTexSubImage2D": "texture",
    "glCopyTexImage2D": "texture", "glCopyTexSubImage2D": "texture",
    "glTexImage3D": "texture", "glTexSubImage3D": "texture",
    "glCompressedTexImage2D": "texture", "glCompressedTexSubImage2D": "texture",
    "glTexParameteri": "texture", "glTexParameterf": "texture",
    "glTexParameteriv": "texture", "glTexParameterfv": "texture",
    "glBindTexture": "texture", "glActiveTexture": "texture",
    "glReadPixels": "texture", "glPixelStorei": "texture",
    # Pelacakan pengikatan buffer untuk jalur client array.
    "glBindBuffer": "buffer", "glBufferData": "buffer",
    "glBufferSubData": "buffer", "glMapBuffer": "buffer",
    "glUnmapBuffer": "buffer", "glGetBufferSubData": "buffer",
    "glGetBufferParameteriv": "buffer",
    # GLSL 1.20 -> GLSL ES 3.20.
    "glShaderSource": "shader",
    # Bertipe double, GLES hanya punya varian float.
    "glClearDepth": "translate", "glDepthRange": "translate",
}

ALIAS_SUFFIX = ("ARB", "EXT", "OES", "NV", "ATI", "AMD", "APPLE", "SGIS", "IBM")


def base_of(name):
    for s in ALIAS_SUFFIX:
        if name.endswith(s) and len(name) > len(s) + 2:
            return name[:-len(s)], s
    return name, ""


def classify(name):
    if name in OVERRIDE:
        return OVERRIDE[name], ""
    if name in gles_core:
        return "direct", name
    b, sfx = base_of(name)
    if sfx and b in gles_core:
        return "alias", b
    if sfx and name in gles_ext:
        return "direct_ext", name
    for cat, pfxs in GROUPS:
        if name.startswith(pfxs):
            return cat, ""
    if sfx:
        for cat, pfxs in GROUPS:
            if b.startswith(pfxs):
                return cat, ""
    return "translate", ""


# ------------------------------------------------------------------ rakit ----
mcs = json.load(open(os.path.join(D, "mc_symbols.json")))
chk = json.load(open(os.path.join(D, "lwjgl_checks.json")))

mc_syms = set(mcs["symbols"])
flag_syms = set()
for f in PROFILE:
    flag_syms |= set(chk.get(f, []))

export = sorted(mc_syms | flag_syms)
rows, buckets, nosig = {}, collections.Counter(), []
for s in export:
    cat, target = classify(s)
    sig = gl_sig.get(s)
    if sig is None:
        nosig.append(s)
    rows[s] = {
        "ret": sig[0] if sig else None,
        "args": sig[1] if sig else None,
        "strategy": cat,
        "gles": target,
        "called_by_mc": s in mc_syms,
        "needed_by_flag": sorted(f for f in PROFILE if s in set(chk.get(f, []))),
    }
    buckets[cat] += 1

out = {
    "profile": PROFILE,
    "gl_version_string": GL_VERSION_STRING,
    "glsl_version_string": GLSL_VERSION_STRING,
    "export_count": len(export),
    "from_mc_callsites": len(mc_syms),
    "from_capability_flags": len(flag_syms - mc_syms),
    "strategy_counts": dict(buckets.most_common()),
    "missing_signature": nosig,
    "exports": rows,
}
json.dump(out, open(os.path.join(D, "oryon_exports.json"), "w"), indent=1)

print("EKSPOR TOTAL          :", len(export))
print("  dari call-site MC   :", len(mc_syms))
print("  pelengkap flag      :", len(flag_syms - mc_syms))
print("  tanpa prototipe     :", len(nosig), nosig[:10])
print()
print("STRATEGI:")
for k, v in buckets.most_common():
    print(f"  {k:14s} {v:4d}")
print("\n-> data/oryon_exports.json")
