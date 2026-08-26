# Oryon Wrapper

Renderer Minecraft Java Edition di Android. GL desktop diterjemahkan ke
**OpenGL ES 3.2**, dikemas sebagai satu shared object: `liboryon.so`.

```
-Dorg.lwjgl.opengl.libname=/path/ke/liboryon.so
```

Itu satu-satunya integrasi yang dibutuhkan.

- **Tanpa JNI.** LWJGL memuatnya lewat `dlopen()` + `dlsym()` langsung.
- **Tanpa EGL.** Context, surface, dan swap tetap milik `libpojavexec.so`.
- **Target:** Minecraft 1.7.10 - 1.16.5, fokus penuh **1.12.2**.
- **Bahasa:** C++, tanpa STL, tanpa exception, tanpa RTTI.

## Status

**Tahap 6 selesai**, plus perbaikan dari uji perangkat pertama (Mali-G52, Android 11).
Lihat `docs/DEVICE.md`.

| tahap | isi | status |
|---|---|---|
| 1 | Struktur project | selesai |
| 2 | Ground-truth simbol & call site | selesai |
| 3 | CMake, CI, hub, driver, tes host | selesai |
| 4 | State fixed-function + generator shader | selesai |
| 5 | Mode langsung, client array, jalur gambar | selesai |
| 6 | Tekstur, display list, GLSL Minecraft | selesai |
| 6b | Tekstur proxy + audit enum (dari log perangkat) | selesai |

## Angka

| | |
|---|---|
| Ekspor | **352** simbol + 4 hub getProcAddress |
| Dipanggil langsung oleh 1.12.2 | 163 |
| Passthrough murni ke GLES 3.2 | 117 |
| Alias ARB/EXT | 33 |
| Belum diimplementasikan | 111 |
| Ditulis tangan | 98 |
| Ukuran `.so` (arm64, Release) | ~169 KB belum di-strip, `.text` 65 KB |
| Ketergantungan runtime | `libc`, `libdl` |

## Dari mana angka-angka itu

Tidak ada satu pun daftar simbol yang ditulis tangan. Semuanya diturunkan:

| sumber | menghasilkan |
|---|---|
| `1.12.2.jar` | 165 call site GL nyata, dibaca dari constant pool 3310 kelas |
| `lwjglglfwclasses.jar` | 2225 simbol kanonik + tuntutan tiap flag kapabilitas |
| Header GL sistem | prototipe C yang tepat untuk 352 ekspor |
| Header GLES 3.2 sistem | strategi tiap simbol: langsung, alias, terjemah, atau emulasi |

Lihat `docs/ARCHITECTURE.md` untuk kontraknya, `docs/SYMBOLS.md` untuk
daftarnya, dan `docs/BUILD.md` untuk cara membangun.

## Tes

```sh
./tests/run.sh
```

Dua rangkaian, keduanya di atas driver GLES 3.2 sungguhan (Mesa):

- **Kontrak bootstrap LWJGL** - memuat `liboryon.so` persis seperti LWJGL,
  menjalankan urutan `GL.createCapabilities()` yang sebenarnya, memastikan nama
  di luar lingkup mengembalikan NULL, dan membangun FBO lalu membaca balik
  pikselnya.
- **State fixed-function + shader** - tumpukan matriks diuji angka per angka,
  penurunan `FFKey` diuji lewat panggilan GL asli, dan setiap shader yang
  dibangkitkan benar-benar dikompilasi serta ditautkan driver.
- **Menggambar** - quad digambar ke FBO lewat mode langsung, client array, dan
  VBO, lalu pikselnya dibaca balik dan dibandingkan: modulate, replace, uji
  alpha, kabut, dua unit tekstur, matriks tekstur, dan terjemahan
  `GL_QUADS`/`GL_QUAD_STRIP`/`GL_POLYGON`.
- **Tekstur, display list, shader** - unggahan BGRA ala `TextureUtil` diperiksa
  per komponen, `GL_CLAMP` dipetakan, display list direkam lalu diputar ulang
  dengan matriks terkini, dan **35 dari 35 shader bawaan Minecraft** benar-benar
  dikompilasi oleh driver setelah diterjemahkan ke GLSL ES 3.20.
