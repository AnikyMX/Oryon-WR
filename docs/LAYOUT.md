# Layout Direktori — Oryon Wrapper

```
oryon-wrapper/
├── .github/workflows/
├── cmake/
├── docs/
├── include/oryon/
├── src/core/
├── src/driver/
├── src/gl/
├── src/state/
├── src/shadergen/
├── src/util/
├── tools/
├── data/
├── tests/
├── third_party/
├── CMakeLists.txt
├── .gitignore
└── README.md
```

## Peran Tiap Direktori

- `.github/workflows/` — Definisi CI. Hanya satu file build.yml dengan 4 step.
- `cmake/` — Toolchain helper & modul CMake tambahan.
- `docs/` — Dokumen desain: arsitektur, kontrak ABI, catatan riset.
- `include/oryon/` — Header internal lintas-modul (ABI dalam, bukan publik).
- `src/core/` — Entry point .so, init/teardown, tabel dispatch, hub getProcAddress.
- `src/driver/` — dlopen/dlsym driver GLES 3.2 + EGL milik sistem. Nol wrapper.
- `src/gl/` — Titik ekspor gl* (dibagi per domain: state, texture, buffer, draw, ...).
- `src/state/` — State machine GL: matrix stack, fixed-function, enable bits.
- `src/shadergen/` — Generator GLSL ES 320 dari state fixed-function + cache program.
- `src/util/` — Hash, arena allocator, log (dikompilasi habis di release).
- `tools/` — Skrip python3: ekstraksi simbol LWJGL & call-site 1.12.2, codegen.
- `data/` — Ground-truth hasil ekstraksi (JSON). Di-commit, bukan artefak build.
- `tests/` — Tes host memakai Mesa EGL + GLES di sandbox.
- `third_party/` — Header khianat pihak ketiga (KHR/EGL/GLES) bila NDK tidak cukup.
