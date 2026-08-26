# Tekstur, Display List, dan Shader Minecraft

Tahap 6 menutup tiga celah terakhir antara Oryon dan Minecraft yang benar-benar
berjalan.

## 1. Unggahan piksel: BGRA yang tidak ada di GLES

`TextureUtil` mengunggah tekstur seperti ini:

```java
glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, w, h, 0,
             GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, intBuffer);
```

Bukan pilihan sembarangan: `BufferedImage` Java menyimpan piksel sebagai int
`0xAARRGGBB`, dan kombinasi itulah yang membacanya tanpa konversi. GLES tidak
punya `GL_BGRA` maupun `GL_UNSIGNED_INT_8_8_8_8_REV`.

Kuncinya: dengan `_REV`, komponen pertama menempati bit terendah. Untuk format
`GL_BGRA` berarti B di bit 0-7, dan di memori little-endian urutan byte-nya
menjadi **B, G, R, A** - persis sama dengan `GL_BGRA` + `GL_UNSIGNED_BYTE`.
Karena itu keduanya ditangani identik:

| kondisi | jalur |
|---|---|
| driver punya `GL_EXT_texture_format_BGRA8888` | diteruskan apa adanya, **tanpa salinan** |
| tidak punya | R dan B ditukar di CPU ke buffer kerja |

Ekstensi itu umum di Adreno dan Mali, jadi jalur cepatnya biasanya yang terpakai.
`GL_UNPACK_ROW_LENGTH` dan `GL_UNPACK_ALIGNMENT` dihormati saat menukar.

## 2. `GL_CLAMP`

Muncul enam kali di 1.12.2 dan **tidak ada sama sekali** di GLES. Secara harfiah
`GL_CLAMP` berarti clamp-to-border, tetapi tanpa warna border yang disetel
hasilnya sama dengan clamp-to-edge - dan itulah yang dimaksud Minecraft.
Dipetakan ke `GL_CLAMP_TO_EDGE`.

Diteruskan begitu saja, GLES akan memasang `GL_INVALID_ENUM`, dan error itu
lalu muncul di `glGetError` Minecraft berikutnya - jauh dari penyebabnya.

## 3. Display list

Dipakai 1.12.2 untuk langit dan bintang: geometri statis yang digambar ulang
tiap frame. Yang direkam Oryon adalah **gambar beserta datanya**, bukan
perintah GL sembarang, karena hanya itu yang pernah ditaruh 1.12.2 di dalam
sebuah list.

Data vertexnya diunggah sekali ke buffer permanen - itulah nilai sesungguhnya
sebuah display list. Pemutaran ulang memakai **state saat dipanggil**, bukan
state saat direkam, sehingga langit ikut bergerak bersama kamera.

## 4. Shader Minecraft sendiri: GLSL 1.20 -> GLSL ES 3.20

35 shader di `assets/minecraft/shaders`. Yang benar-benar dipakai, dari
pemindaian sumbernya:

| konstruksi | jumlah |
|---|---|
| `#version 120` | 34 |
| `uniform` | 111 |
| `texture2D` | 103 |
| `varying` | 63 |
| `sampler2D` | 33 |
| `gl_FragColor` | 26 |
| `attribute` | 9 |
| `#extension` | 1 |

Yang **tidak** dipakai sama sekali: `gl_ModelViewProjectionMatrix`, `gl_Vertex`,
`gl_Color`, `gl_Normal`, `gl_TexCoord`, `gl_FrontColor`, `gl_FragData`.
Seluruh shader memakai uniform dan atribut bernama sendiri. Itulah yang membuat
penerjemah ini kecil: tidak ada satu pun uniform bawaan fixed-function yang
perlu disuntikkan.

### Yang diterjemahkan

| dari | ke |
|---|---|
| `#version 120` | `#version 320 es` + baris `precision` |
| `attribute` | `in` |
| `varying` | `out` (vertex) / `in` (fragment) |
| `texture2D` | `texture` |
| `gl_FragColor` | `layout(location = 0) out vec4 oryon_FragColor` |
| `#extension GL_EXT_gpu_shader4` | dibuang - fiturnya sudah inti di ES 3 |
| `sample` sebagai nama variabel | `sample_` - kata cadangan di ES 3 |

### Dua hal yang hanya ketahuan dari sumber aslinya

**Nilai bawaan uniform.** GLSL 1.20 membolehkan `uniform float Saturation = 1.5;`,
GLSL ES tidak. Ada 20 deklarasi semacam itu. Membuangnya begitu saja akan
membuat efek pasca-proses tampil salah - bukan gagal, yang justru lebih sulit
dilacak. Oryon mengangkat nilainya saat penerjemahan lalu memasangnya kembali
dengan `glUniform*` tepat setelah program tertaut.

**Konversi int ke float implisit.** GLSL 1.20 melakukannya, GLSL ES tidak. Di
35 shader hanya ada 10 literal integer telanjang, tetapi tiga di antaranya
menggagalkan kompilasi: `clamp(f, 0.5, 2)`, `2 - abs(...)`, dan `c != 0`.
Sisanya berada di dalam pemanggilan makro `OffsetVec(1,0)` yang bisa mengembang
menjadi `ivec2` - mempromosikannya justru akan merusak. Karena itu promosi
dilewati pada baris yang menyebut tipe integer dan di dalam daftar argumen
makro.

Tanpa kedua penanganan ini, 7 dari 35 shader gagal dikompilasi. Dengan
keduanya: **35 dari 35**, diverifikasi terhadap kompiler GLSL sungguhan.

### Atribut ke lokasi 0

Shader Minecraft membaca posisi dari `attribute vec4 Position`, tetapi datanya
dikirim lewat `glVertexPointer`. Di driver desktop itu bekerja karena atribut
generik 0 beralias dengan `gl_Vertex`. Oryon tidak bergantung pada
keberuntungan linker: nama atribut pertama dicatat saat penerjemahan, lalu
diikat eksplisit ke lokasi 0 dengan `glBindAttribLocation` sebelum menautkan.

### Program pengguna menggantikan pipeline tetap

Selama `glUseProgram` bukan nol, jalur gambar tidak boleh memasang program
fixed-function - persis seperti GL sungguhan. Tanpa penjagaan ini, setiap
`glDrawArrays` akan menimpa shader yang baru saja dipasang Minecraft.
`glUseProgram(0)` mengembalikannya.
