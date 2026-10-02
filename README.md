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
  `ORYON_DUMP_FFP=1` (cetak sumber VS/FS program FFP yang dikompilasi), `ORYON_NO_VERTEX_BINDING=1` (kembali ke
  `glVertexAttribPointer` per atribut alih-alih vertex attribute binding ES 3.1), `ORYON_FFP_HIGHP=1` (varying texcoord
  FFP selalu vec4 highp; default ukuran sesuai pemakaian, warna/fog mediump),
  `ORYON_STATS=1` (satu baris ringkasan per detik: fps, frame terburuk, draw ES, streaming, kompilasi program,
  link GLSL, display list, tunggu fence, upload tekstur/buffer, readback). Env boolean aktif bila diisi selain
  `0`/`false`/`off`/`no`. Log Oryon juga ditulis ke stderr agar masuk latestlog launcher.
  `ORYON_STATS_SLOW_MS=<ms>` (default 16): dengan `ORYON_STATS`, setiap kompilasi program FFP yang lebih lama dicatat
  beserta deskripsi state-nya.
- Probe performa (hanya dengan `ORYON_STATS=1`, tiga baris tambahan per detik):
  - `cpu`: waktu frame, waktu CPU render thread per frame (+ % frame cpu-bound), `runnable-waiting` (render thread
    siap jalan tetapi menunggu CPU, dari `/proc/.../schedstat`), `big cores` (porsi sampel di core tercepat menurut
    `cpuinfo_max_freq`), `present` (dari pindah ke framebuffer 0 di akhir frame sampai awal frame berikutnya: blit,
    swap launcher, limiter fps, tick game; wall dan CPU), pembagian sampel CPU render thread (oryon / gl driver / jvm /
    java JIT / libc / other; SIGPROF tiap 1 ms waktu CPU, granularitas tick kernel), CPU proses (core).
  - `calls`: panggilan ES per frame: draw, program, uniform, texture, VAO, bind buffer, attrib pointer, `vbuf`
    (`glBindVertexBuffer`), `fmt` (`glVertexAttribFormat`/`Binding`).
  - `gpu`: waktu sibuk GPU per frame dari timeline fence EGL per render pass (batas pass = ganti framebuffer gambar
    dan awal frame; pass yang dikirim saat GPU idle dikurangi floor round-trip fence = persentil 25 pass idle tanpa
    draw), pembagian per framebuffer, % frame gpu-bound, dan lag (sisa kerja GPU saat frame berikutnya mulai).
    Timer `GL_TIME_ELAPSED_EXT` hanya dengan `ORYON_STATS_TIMER=1` (di Mali hasilnya ikut mencakup waktu CPU merekam
    pass; pass terakhir sebelum swap dicetak terpisah sebagai batas atas `tail`).
  - `ORYON_STATS_GPU=0` mematikan fence/timer query, `ORYON_STATS_PROFILE=0` mematikan sampler CPU.
- Cache program FFP persisten: binary driver disimpan per kunci state di `ORYON_CACHE_DIR` (default `$TMPDIR/oryon`,
  lalu `$HOME/.cache/oryon`) dan dimuat saat init konteks, jadi kombinasi state yang pernah muncul tidak dikompilasi
  lagi saat bermain. Binary hanya dipakai bila hash (string driver + VS + FS hasil generator) cocok; file rusak dibuang,
  file basi dibangun ulang. Matikan dengan `ORYON_NO_PROGRAM_CACHE=1`.
- Asumsi: satu konteks GL aktif (Forge splash multi-thread sebaiknya dimatikan, seperti launcher lain).

## Build
- CI: `.github/workflows/build.yml` (4 langkah) → artifact `liboryon-arm64-v8a`.
- Lokal/uji: `cmake -S . -B build-host -G Ninja -DCMAKE_CXX_COMPILER=clang++ && cmake --build build-host`

## Alur kerja (Python3; sumber kebenaran = kedua jar)
`tools/jarscan.py` (jar → `tools/db`) → `tools/gen.py` (→ `src/gen`) → `tools/annotate.py` (anotasi jar + hook display list)
→ `tools/validate.py` (cross-reference) → `tools/test_mesa.py`, `test_ffp.py`, `test_dlist.py`, `test_glsl.py` (Mesa EGL + GLES 3.2),
`tools/test_stats.py` (diagnostik), `tools/test_progcache.py` (cache program), `tools/test_perf.py` (probe GPU/CPU),
`tools/test_calls.py` (anggaran panggilan ES jalur chunk VBO MC 1.12.2; `ORYON_SO_PREV` untuk pembanding),
`tools/check_android.py` (kompilasi `-Werror` untuk aarch64-linux-android24 dengan header bionic, `ORYON_BIONIC`),
`tools/bench/` (overhead CPU). Path jar: `ORYON_MC_JAR`, `ORYON_LWJGL_JAR`.

## Modul
| File | Isi |
|---|---|
| `gles.cpp` | loader GLES (dlopen/dlsym), init konteks lazy (hook tanpa biaya hot-path) |
| `stats.cpp` | diagnostik opsional `ORYON_STATS` (nol instruksi tambahan di jalur draw saat mati) |
| `perf.cpp` | probe `ORYON_STATS`: timeline fence GPU per pass, timer query opsional, sampler CPU, schedstat, segmen present, penghitung panggilan ES |
| `core.cpp` | error model, string/versi, kueri virtual `glGet*` |
| `matrix.cpp` | stack MODELVIEW/PROJECTION/TEXTURE (CPU) |
| `ffp.cpp`, `ffp_prog.cpp` | state fixed-function → kunci kanonik (COMBINE setara MODULATE/REPLACE dilebur, mode fog via uniform) → GLSL ES 3.20 ter-cache (memori + disk), uniform ber-versi, matriks MVP/MV/normal dalam satu `mat4[]`, varying berukuran pas (warna/fog mediump) |
| `vertex.cpp` | immediate mode + batching, client array, ring streaming (persistent/fallback), QUADS via IBO + BaseVertex, vertex attribute binding ES 3.1 (satu `glBindVertexBuffer` per ganti VBO) |
| `texture.cpp` | BGRA/8888_REV zero-copy via swizzle, format legacy, proxy, readback |
| `dlist.cpp` | display list: op stream + geometri di-merge ke VBO/IBO/VAO per list |
| `glsl.cpp`, `shader.cpp` | translator GLSL 1.10–1.50 → ES 3.20, builtin FFP, aliasing atribut |
| `attrib.cpp`, `misc.cpp` | push/pop attrib, pemetaan kecil desktop→ES |

## Batasan saat ini
Accum/stipple/selection/feedback/evaluator/pixel-map/`glBitmap`/`glDrawPixels` masih stub (tidak dipakai MC);
`glLogicOp` belum diemulasi; `GL_SAMPLES_PASSED` → `GL_ANY_SAMPLES_PASSED` (hasil 0/1); `glPolygonMode(GL_LINE)` butuh `GL_NV_polygon_mode`.
