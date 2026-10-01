# Oryon — desktop OpenGL → OpenGL ES 3.2 (`liboryon.so`)

Wrapper renderer Minecraft Java Edition di Android (fokus **1.12.2**, target s.d. 1.16.5). Dimuat launcher dengan
`dlopen()` dan di-resolve LWJGL dengan `dlsym()` — **tanpa JNI**, tanpa libc++ (hanya libc/libm/libdl/liblog).

## Integrasi launcher
- LWJGL 3.3.6 (`lwjgl-glfw-classes.jar`) memuat library GL dari properti **`org.lwjgl.opengl.libname`** → isi dengan path `liboryon.so`.
- Buat konteks EGL **OpenGL ES 3.x (disarankan 3.2)** dan jadikan current sebelum `GL.createCapabilities()`.
- `liboryon.so` sengaja tidak mengekspor `glXGetProcAddress`/`eglGetProcAddress`/`OSMesaGetProcAddress`, sehingga LWJGL
  memakai `dlsym` langsung (urutan lookup diverifikasi dari bytecode `org/lwjgl/opengl/GL$1`).
- Versi yang dilaporkan: `3.0 Oryon …` (compatibility). Flag LWJGL GL11–GL30 + 38 ekstensi bernilai true, sehingga
  MC 1.12.2 memilih jalur core (FBO GL30, shader GL20, VBO GL15). Fork LWJGL launcher (`Checks.checkFunctions` selalu
  true, `reportMissing` → true) membuat flag = "diiklankan" dan GL30–GL33 selalu true; GL31–GL33 belum diekspor Oryon
  (MC 1.12.2 tidak membaca/memanggilnya — dicek `validate.py`).
- Env opsional: `ORYON_GLES_LIB`, `ORYON_EGL_LIB` (driver kustom), `ORYON_NO_BUFFER_STORAGE=1`, `ORYON_DUMP_GLSL=1`,
  `ORYON_STATS=1` (satu baris ringkasan per detik: fps, frame terburuk, draw ES, streaming, kompilasi program,
  link GLSL, display list, tunggu fence, upload tekstur/buffer, readback). Env boolean aktif bila diisi selain
  `0`/`false`/`off`/`no`. Log Oryon juga ditulis ke stderr agar masuk latestlog launcher.
- Asumsi: satu konteks GL aktif (Forge splash multi-thread sebaiknya dimatikan, seperti launcher lain).

## Build
- CI: `.github/workflows/build.yml` (4 langkah) → artifact `liboryon-arm64-v8a`.
- Lokal/uji: `cmake -S . -B build-host -G Ninja -DCMAKE_CXX_COMPILER=clang++ && cmake --build build-host`

## Alur kerja (Python3; sumber kebenaran = kedua jar)
`tools/jarscan.py` (jar → `tools/db`) → `tools/gen.py` (→ `src/gen`) → `tools/annotate.py` (anotasi jar + hook display list)
→ `tools/validate.py` (cross-reference) → `tools/test_mesa.py`, `test_ffp.py`, `test_dlist.py`, `test_glsl.py` (Mesa EGL + GLES 3.2),
`tools/test_stats.py` (diagnostik), `tools/bench/` (overhead CPU). Path jar: `ORYON_MC_JAR`, `ORYON_LWJGL_JAR`.

## Modul
| File | Isi |
|---|---|
| `gles.cpp` | loader GLES (dlopen/dlsym), init konteks lazy (hook tanpa biaya hot-path) |
| `stats.cpp` | diagnostik opsional `ORYON_STATS` (nol instruksi tambahan di jalur draw saat mati) |
| `core.cpp` | error model, string/versi, kueri virtual `glGet*` |
| `matrix.cpp` | stack MODELVIEW/PROJECTION/TEXTURE (CPU) |
| `ffp.cpp`, `ffp_prog.cpp` | state fixed-function → kunci kanonik → GLSL ES 3.20 ter-cache, uniform ber-versi |
| `vertex.cpp` | immediate mode + batching, client array, ring streaming (persistent/fallback), QUADS via IBO + BaseVertex |
| `texture.cpp` | BGRA/8888_REV zero-copy via swizzle, format legacy, proxy, readback |
| `dlist.cpp` | display list: op stream + geometri di-merge ke VBO/IBO/VAO per list |
| `glsl.cpp`, `shader.cpp` | translator GLSL 1.10–1.50 → ES 3.20, builtin FFP, aliasing atribut |
| `attrib.cpp`, `misc.cpp` | push/pop attrib, pemetaan kecil desktop→ES |

## Batasan saat ini
Accum/stipple/selection/feedback/evaluator/pixel-map/`glBitmap`/`glDrawPixels` masih stub (tidak dipakai MC);
`glLogicOp` belum diemulasi; `GL_SAMPLES_PASSED` → `GL_ANY_SAMPLES_PASSED` (hasil 0/1); `glPolygonMode(GL_LINE)` butuh `GL_NV_polygon_mode`.
