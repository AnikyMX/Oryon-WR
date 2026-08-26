# Fixed-Function ke GLSL ES 320

GLES 3.2 tidak punya pipeline fixed-function sama sekali. Tidak ada tumpukan
matriks, tidak ada `GL_ALPHA_TEST`, tidak ada kabut, tidak ada pencahayaan,
tidak ada `GL_TEXTURE_2D` sebagai kapabilitas. Semua itu harus dibangkitkan
kembali sebagai shader.

Pertanyaannya bukan "bagaimana meniru OpenGL fixed-function", melainkan
**"bagian mana yang benar-benar dipakai Minecraft 1.12.2"** - karena setiap
fitur yang ditulis tapi tak terpakai adalah ALU yang terbakar di setiap piksel.

## Lingkup ditentukan bytecode, bukan spesifikasi

`tools/extract_mc_enums.py` menelusuri bytecode seluruh kelas 1.12.2 yang
menyentuh `org/lwjgl/opengl`, lalu mencocokkan konstanta int yang didorong ke
stack dengan nama enum GL.

> **Koreksi.** Versi pertama pemindai ini hanya membaca `bipush` dan `sipush`,
> sehingga melewatkan setiap enum GL bernilai >= 0x8000 - yang oleh javac
> ditaruh di constant pool dan diambil dengan `ldc`. Itu separuh permukaan GL.
> Setelah diperbaiki, tiga token tambahan muncul: `GL_COMBINE`,
> `GL_RESCALE_NORMAL`, dan `GL_TEXTURE_LOD_BIAS`. Kesimpulan di bawah sudah
> memakai data yang diperbaiki.

| token | muncul | kelas |
|---|---|---|
| `GL_POINTS` | 410 | 10 |
| `GL_LINES` | 324 | 10 |
| `GL_LINE_LOOP` | 113 | 8 |
| `GL_LINE_STRIP` | 44 | 6 |
| `GL_TRIANGLES` | 35 | 6 |
| `GL_QUAD_STRIP` | 33 | 7 |
| `GL_TEXTURE_ENV` | 33 | 2 |
| `GL_TRIANGLE_STRIP` | 23 | 5 |
| `GL_QUADS` | 21 | 5 |
| `GL_TEXTURE_2D` | 16 | 4 |
| `GL_TRIANGLE_FAN` | 15 | 5 |
| `GL_POLYGON` | 11 | 4 |
| `GL_GREATER` | 8 | 2 |
| `GL_LIGHT0` | 8 | 3 |
| `GL_MODULATE` | 6 | 2 |
| `GL_SMOOTH` | 5 | 4 |
| `GL_LINEAR` | 5 | 3 |
| `GL_EYE_LINEAR` | 4 | 1 |
| `GL_OBJECT_LINEAR` | 4 | 1 |
| `GL_TEXTURE_ENV_MODE` | 3 | 2 |
| `GL_COMBINE` | 3 | 2 |
| `GL_REPLACE` | 2 | 1 |
| `GL_AMBIENT` | 2 | 2 |
| `GL_DIFFUSE` | 2 | 1 |
| `GL_SPECULAR` | 2 | 1 |
| `GL_FRONT` | 2 | 2 |

## Combiner tekstur: dipakai, tapi hanya untuk nilai bawaan

`GL_COMBINE` muncul tiga kali. Menelusuri call site-nya menunjukkan seluruhnya
berada di rutin reset state, dan nilainya persis **nilai bawaan GL**:

```
GL_TEXTURE_ENV_MODE = GL_MODULATE
GL_COMBINE_RGB      = GL_MODULATE
GL_SOURCE0_RGB      = GL_TEXTURE      GL_OPERAND0_RGB = GL_SRC_COLOR
GL_SOURCE1_RGB      = GL_PREVIOUS     GL_OPERAND1_RGB = GL_SRC_COLOR
GL_RGB_SCALE        = 1.0             GL_ALPHA_SCALE  = 1.0
```

Konfigurasi itu menghasilkan hasil yang identik dengan `GL_MODULATE` biasa.
Karena itu Oryon melacak state combiner lalu **meringkasnya** menjadi satu mode
efektif (`effective_env()` di `impl_ffstate.cpp`) alih-alih membangkitkan cabang
shader tersendiri. Konfigurasi di luar dua pola yang dikenali dicatat di build
debug, bukan ditebak diam-diam.

Yang **tidak** ditemukan sama sekali, dan karena itu tidak ditulis:

| tidak ada | konsekuensi |
|---|---|
| `GL_DECAL`, `GL_ADD`, `GL_BLEND`, `GL_INTERPOLATE` (texenv) | hanya `GL_MODULATE` dan `GL_REPLACE` yang dibangkitkan |
| `GL_SPHERE_MAP`, `GL_REFLECTION_MAP` | texgen hanya `GL_OBJECT_LINEAR` dan `GL_EYE_LINEAR` |
| `GL_SPECULAR` bernilai bukan hitam | komponen spekular tidak pernah ditulis ke shader |
| `GL_LIGHT2`..`GL_LIGHT7` sebagai literal | tetap didukung lewat mask, tapi Minecraft hanya menyalakan dua |

## FFKey: 16 byte yang menentukan satu shader

```c
struct FFKey {
    uint16_t tex_enable;    // GL_TEXTURE_2D per unit
    uint16_t tex_replace;   // GL_REPLACE vs GL_MODULATE
    uint16_t tex_gen;       // texgen aktif
    uint16_t tex_gen_eye;   // GL_EYE_LINEAR vs GL_OBJECT_LINEAR
    uint16_t tex_matrix;    // matriks tekstur bukan identitas
    uint16_t attr_tex;      // atribut koordinat tersedia
    uint8_t  light_mask;
    uint8_t  flags;         // lighting, color material, normalize, fog,
                            // alpha test, flat, attr color, attr normal
    uint8_t  fog_mode;      // linear / exp / exp2
    uint8_t  alpha_func;    // 0..7
};
```

Kunci sama berarti program sama. Cache memakai alamat terbuka 128 slot dengan
hash FNV-1a atas 16 byte itu; Minecraft realistis memakai belasan kombinasi,
jadi pencarian praktis selalu selesai di probe pertama.

Satu keputusan kecil yang berdampak besar: `glAlphaFunc(GL_ALWAYS, ...)`
menghapus bit uji alpha dari kunci. Minecraft memanggil itu terus-menerus untuk
mematikan uji alpha, dan tanpa penanganan ini setiap shader akan membawa cabang
`discard` yang tidak pernah benar.

## Yang dibangkitkan, dan yang tidak

Shader untuk terrain biasa - dua tekstur, warna vertex, tanpa kabut - keluar
seperti ini, dan tidak ada satu baris pun yang tidak dipakai:

```glsl
#version 320 es
layout(location=0) in vec4 a_pos;
layout(location=1) in vec4 a_color;
layout(location=3) in vec4 a_tex0;
layout(location=4) in vec4 a_tex1;
uniform mat4 u_mvp;
out vec4 v_color;
out vec2 v_tex0;
out vec2 v_tex1;
void main() {
  gl_Position = u_mvp * a_pos;
  vec4 c = a_color;
  v_color = c;
  { vec4 t = a_tex0; v_tex0 = t.xy; }
  { vec4 t = a_tex1; v_tex1 = t.xy; }
}
```

Matriks ruang mata (`u_mv`) baru muncul kalau ada kabut, pencahayaan, atau
texgen `EYE_LINEAR`. Matriks normal baru muncul kalau ada pencahayaan. Kabut,
uji alpha, dan texgen tidak menyisakan jejak apa pun saat mati.

| kombinasi state | vertex | fragment |
|---|---|---|
| polos, warna vertex | 206 B | 175 B |
| terrain 2 tekstur | 406 B | 375 B |
| kabut LINEAR | 404 B | 523 B |
| cahaya 2 arah + normal | 1148 B | 275 B |
| semua fitur menyala | 1438 B | 703 B |

## Lokasi atribut dipatok

```
0  a_pos      3..10  a_tex0..a_tex7
1  a_color
2  a_normal
```

Karena tetap di seluruh program, mengganti shader tidak pernah memaksa menata
ulang atribut vertex - VAO yang sama tetap sah. Lokasi 0 juga yang membuat
shader milik Minecraft sendiri tetap disuapi `glVertexPointer`; lihat
`docs/TEXTURES_SHADERS.md`.

## Aturan GL yang mudah terlewat, dan ditangani di sini

| aturan | tempat |
|---|---|
| `glLightfv(GL_POSITION)` ditransformasi modelview **saat ditetapkan** | `impl_ffstate.cpp` |
| Koefisien `GL_EYE_PLANE` dikalikan invers modelview **saat ditetapkan** | `impl_ffstate.cpp` |
| `glGetFloatv(GL_MODELVIEW_MATRIX)` harus persis sama dengan yang dipakai shader | `impl_query.cpp` |
| `glPushAttrib` menyimpan enable + lighting, bukan matriks | `impl_ffstate.cpp` |
| Kapabilitas tak dikenal GLES tidak boleh diteruskan (memasang `GL_INVALID_ENUM`) | `impl_ffstate.cpp` |

Yang ketiga adalah yang paling mudah luput: `ActiveRenderInfo` membaca matriks
tiap frame untuk menempatkan partikel dan memilih entitas. Kalau Oryon menjawab
dengan nilai driver, partikel akan melayang lepas dari dunianya.
