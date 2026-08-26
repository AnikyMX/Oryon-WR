# Arsitektur Oryon Wrapper

Status: **Tahap 6 selesai** — tekstur, display list, dan shader Minecraft sendiri berjalan. 119 pemeriksaan lulus terhadap driver GLES 3.2 sungguhan, termasuk 35 dari 35 shader bawaan 1.12.2 yang benar-benar dikompilasi.

---

## 1. Kontrak pemuatan

```
Minecraft 1.12.2 (Java, terobfuskasi)
        v  memanggil org.lwjgl.opengl.GL11 / GL15 / GL20 / ARBFramebufferObject
LWJGL 3 (lwjglglfwclasses.jar, varian Pojav)
        v  GL.create() -> Library.loadNative("org.lwjgl.opengl", <libname>)
        v  dlopen(nama, RTLD_LAZY | RTLD_GLOBAL)
liboryon.so                                  <-- lingkup project ini
        v  penunjuk fungsi hasil dlsym
libGLESv3 / libGLESv2 sistem (driver Android sungguhan)
```

`-Dorg.lwjgl.opengl.libname=liboryon.so` adalah satu-satunya titik masuk.
**Tidak ada kode glue JNI.** Semua yang diekspor adalah simbol C polos.

### Batas tanggung jawab

| Milik `libpojavexec.so` | Milik Oryon |
|---|---|
| Membuat display & context EGL | Seluruh perintah `gl*` |
| Surface, `eglSwapBuffers`, vsync | Terjemahan GL desktop -> GLES 3.2 |
| Menjadikan context current | State fixed-function & pembangkitan shader |

Oryon **tidak pernah** memanggil `eglCreateContext`, `eglMakeCurrent`, atau
`eglSwapBuffers`. Ia mengasumsikan sudah ada context GLES 3.2 yang current pada
thread pemanggil. Konsekuensi: Oryon tidak boleh punya state global yang
mengikat thread selain lewat TLS.

---

## 2. Hub resolusi simbol

Urutan pencarian LWJGL sudah diverifikasi langsung dari bytecode `GL$1.<init>`:

```
glXGetProcAddress -> glXGetProcAddressARB -> wglGetProcAddress
   -> eglGetProcAddress -> OSMesaGetProcAddress   -> lalu fallback dlsym()
```

Oryon mengekspor keempat nama yang relevan; semuanya menunjuk ke satu
implementasi. Aturan yang tidak bisa ditawar:

1. **Nama tak dikenal WAJIB mengembalikan `NULL`.** LWJGL memakai `NULL` untuk
   menyimpulkan sebuah flag kapabilitas bernilai false. Mengembalikan penunjuk
   asal-asalan akan membuat `Checks.check()` melempar di titik panggilan pertama.
2. Pencarian memakai **pencarian biner** atas tabel statis 352 entri yang sudah
   terurut `strcmp` (`src/core/hub_table.inc`). Hanya ~9 `strcmp` per query,
   dan hanya terjadi saat inisialisasi.
3. `eglGetProcAddress` milik Oryon meneruskan nama ber-awalan `egl` ke libEGL
   sistem. Alasannya: `dlopen` memakai `RTLD_GLOBAL`, sehingga simbol Oryon
   masuk ke ruang nama global dan bisa membayangi milik Pojav. Meneruskan
   membuat perilaku tetap benar apa pun urutan pemuatan.

---

## 3. Kontrak bootstrap LWJGL (diekstrak dari `GL.createCapabilities`)

Urutan nyata, dari bytecode. Melanggar salah satu = crash sebelum frame pertama:

| # | Tuntutan | Akibat bila gagal |
|---|---|---|
| 1 | `glGetError`, `glGetString`, `glGetIntegerv` harus resolve | `"Core OpenGL functions could not be found."` |
| 2 | `glGetError()` mengembalikan 0 | context dianggap error state |
| 3 | `glGetString(GL_VERSION)` bukan NULL | `"There is no OpenGL context current in the current thread."` |
| 4 | Versi terurai >= 1.1 | `"OpenGL 1.1 is required."` |
| 5 | Versi < 3.0 -> daftar ekstensi dibaca dari `glGetString(GL_EXTENSIONS)` | jalur `glGetStringi` tidak terpakai |

Karena Oryon melaporkan `GL_VERSION = "2.1 Oryon"`, langkah 5 memakai string
ekstensi tunggal. `glGetStringi` tidak diperlukan.

---

## 4. Profil kapabilitas terpilih

- `GL_VERSION` = `2.1 Oryon`
- `GL_SHADING_LANGUAGE_VERSION` = `1.20`
- Flag TRUE: `GL12`, `GL13`, `GL14`, `GL15`, `GL20`, `GL21`, `ARB_framebuffer_object`

Konsekuensi pada Minecraft 1.12.2 (`OpenGlHelper.initializeTextures`):

| Keputusan MC | Hasil | Kenapa penting |
|---|---|---|
| `vboSupported = OpenGL15` | **VBO inti** | `glBindBuffer`/`glBufferData` peta 1:1 ke GLES |
| `shadersSupported = OpenGL21` | **shader inti GL20** | `glCreateShader`/`glUniform*` peta 1:1 ke GLES |
| `framebufferType = ARB` | **nama FBO inti** | `glBindFramebuffer` tanpa akhiran, langsung GLES |
| `arbMultitexture = false` | `glActiveTexture` inti | tanpa lapisan alias ARB |

Jalur ARB/EXT tetap diekspor sebagai alias tipis supaya tetap aman bila sebuah
flag ternyata false di perangkat tertentu.

---

## 5. Permukaan simbol

**352 ekspor** = 163 call site nyata 1.12.2 + 189 pelengkap agar flag di atas TRUE.

| strategi | n | beban runtime |
|---|---|---|
| `direct` | 145 | nol (satu panggilan tak langsung) |
| `immediate` | 92 | perakitan vertex, dikirim per glEnd |
| `alias` | 33 | nol (alias waktu tautan) |
| `translate` | 23 | konversi tipis |
| `legacy_misc` | 19 | dingin; sebagian besar tak pernah dipanggil |
| `matrix` | 15 | aritmetika CPU, unggah uniform saat dirty |
| `clientarr` | 6 | pemetaan ke VAO sekali per perubahan |
| `texenv` | 5 | hanya mengubah kunci state |
| `displaylist` | 4 | rekam/putar-ulang daftar perintah |
| `lighting` | 4 | hanya mengubah kunci state |
| `fog` | 3 | hanya mengubah kunci state |
| `attribstack` | 2 | salin potongan state |
| `alphatest` | 1 | hanya mengubah kunci state |

Rincian lengkap: `docs/SYMBOLS.md`. Semua daftar dihasilkan mesin oleh
`tools/gen_exports.py`; tidak ada tanda tangan yang ditulis tangan.

---

## 6. Aturan kompilasi & tautan

```
-fvisibility=hidden          semua simbol tersembunyi secara bawaan
__attribute__((visibility("default")))  hanya pada 352 + 4 ekspor
-Wl,--no-undefined           Bionic tidak punya lazy binding: simbol
                             menggantung = gagal dlopen, bukan gagal saat dipanggil
-Wl,--version-script=oryon.map  permukaan ABI eksplisit dan bisa diaudit
-fno-exceptions -fno-rtti    tidak ada jalur C++ yang melintasi batas ABI
```

Aturan tambahan yang mengikat seluruh kode:

- Tidak ada alokasi pada jalur gambar panas. Buffer vertex dan program shader
  dialokasikan sekali lalu dipakai ulang.
- Tidak ada `std::string` / `std::map` di jalur per-panggilan.
- Tiap fungsi `direct` adalah satu panggilan lewat penunjuk fungsi, tanpa
  pembungkus, tanpa pemeriksaan.

---

## 7. Peta lapisan

```
src/core/       entry .so, hub getProcAddress, tabel dispatch
src/driver/     dlopen/dlsym libGLESv3, tabel penunjuk fungsi driver
src/gl/         352 titik ekspor, dikelompokkan per domain
src/state/      tumpukan matriks, bit fixed-function, kunci state
src/shadergen/  GLSL ES 320 dari kunci state + cache program
src/util/       hash, arena, log (habis dikompilasi di release)
```

---

## 7b. Fixed-function

Lihat `docs/FIXEDFUNC.md`. Ringkasnya: state fixed-function diringkas menjadi
`FFKey` 16 byte, satu kunci menghasilkan satu program GLSL ES 320, dan lingkup
fiturnya ditentukan oleh konstanta GL yang benar-benar muncul di bytecode
1.12.2 - bukan oleh spesifikasi OpenGL.

## 7c. Jalur gambar

Lihat `docs/DRAWPATH.md`. Mode langsung, array sisi klien, dan array bersumber
VBO bermuara ke satu perakit vertex. Dari sepuluh mode primitif GL, hanya
`GL_QUADS` yang butuh index buffer; `GL_QUAD_STRIP` dan `GL_POLYGON` ternyata
identik dengan padanan GLES-nya.

## 7d. Tekstur dan shader Minecraft

Lihat `docs/TEXTURES_SHADERS.md`. Unggahan `GL_BGRA` +
`GL_UNSIGNED_INT_8_8_8_8_REV` milik `TextureUtil`, `GL_CLAMP` yang tidak ada di
GLES, display list untuk langit, dan penerjemah GLSL 1.20 -> ES 3.20 untuk 35
shader bawaan.

## 8. Yang sengaja BELUM dikerjakan

- 1.13 - 1.16.5: butuh core profile 3.2/3.3 dan VAO wajib. Ditambahkan sebagai
  delta terpisah setelah 1.12.2 menggambar frame.
- OptiFine / shader pack: butuh permukaan GLSL jauh lebih luas.
- `glGetTexImage` untuk tekstur terkompresi.
