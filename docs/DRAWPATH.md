# Jalur Gambar

Tahap 5 menyambungkan semuanya: state fixed-function jadi program, data vertex
jadi atribut generik, primitif GL jadi primitif GLES. Setelah tahap ini Oryon
benar-benar menggambar.

## Tiga sumber vertex, satu muara

| sumber | dipakai Minecraft untuk | penanganan |
|---|---|---|
| Mode langsung `glBegin`/`glEnd` | GUI, teks, debug | dirakit ke buffer CPU, diunggah sekali per `glEnd` |
| Array sisi klien (memori Java) | `WorldVertexBufferUploader`, entitas | dideteksi interleaved, diunggah sekali per gambar |
| Array bersumber VBO | terrain saat `vboSupported` | tidak diunggah sama sekali, hanya diarahkan |

GLES 3.2 tidak mengenal dua yang pertama, jadi keduanya berakhir di buffer
streaming yang sama dengan pola orphaning (`glBufferData(NULL)` lalu
`glBufferSubData`).

## Terjemahan primitif: lebih murah dari dugaan

Dari 10 mode primitif GL, hanya satu yang benar-benar butuh kerja:

| mode GL | GLES | biaya |
|---|---|---|
| `GL_QUADS` | `GL_TRIANGLES` + index buffer | satu index buffer statis, dibangkitkan sekali lalu tumbuh 2x |
| `GL_QUAD_STRIP` | `GL_TRIANGLE_STRIP` | **nol** - urutan vertexnya identik |
| `GL_POLYGON` | `GL_TRIANGLE_FAN` | **nol** - `GL_POLYGON` dijamin cembung |
| sisanya | sama | nol |

`GL_QUAD_STRIP` sering dianggap perlu index buffer sendiri. Tidak: quad ke-n
sebuah quad strip adalah `(2n, 2n+1, 2n+3, 2n+2)`, dan dua segitiga pertama
sebuah triangle strip menutupi luasan yang sama dengan arah putaran yang sama.
Diagonalnya berbeda, tapi untuk quad planar - dan seluruh quad Minecraft planar
- hasilnya tidak bisa dibedakan. Tes `render_test` membuktikannya dengan piksel.

Index buffer quad dibangun untuk vertex mulai 0, dan `first` dilipat ke dalam
offset penunjuk atribut. Dengan begitu satu index buffer statis melayani semua
gambar, tanpa perlu `glDrawElementsBaseVertex` yang baru ada di GLES 3.2.

## Normalisasi: satu aturan yang mudah salah

`glVertexAttribPointer` punya bendera `normalized`. Aturan fixed-function:

| array | tipe integer dinormalkan? |
|---|---|
| `glColorPointer` | **ya** - 255 berarti 1.0 |
| `glNormalPointer` | **ya** |
| `glVertexPointer` | tidak |
| `glTexCoordPointer` | **tidak** |

Yang terakhir kritis. Lightmap Minecraft adalah `GL_SHORT` bernilai 0..240,
lalu diskalakan matriks tekstur (`scale(1/256)` kemudian `translate(8,8,8)` di
`EntityRenderer.enableLightmap`). Menormalkannya akan membuat semua koordinat
mendekati nol dan dunia gelap gulita.

## Deteksi interleaved

`WorldVertexBufferUploader` menunjuk posisi, warna, dan dua koordinat tekstur ke
dalam **satu** `ByteBuffer` dengan stride yang sama. Kalau tiap array diunggah
sendiri-sendiri, memori yang sama tersalin empat kali.

Karena itu jalur klien memeriksa: apakah semua array punya stride yang sama dan
semua penunjuknya jatuh dalam satu jendela selebar stride? Kalau ya, itu satu
buffer interleaved - satu unggahan, dan tiap atribut cukup diberi offset di
dalam stride. Kalau tidak, jalur umum per-array yang dipakai (tidak pernah
terjadi di 1.12.2, tapi harus benar untuk mod).

## Jarak kabut: radial, bukan |z|

Spesifikasi GL membolehkan dua tafsir untuk koordinat kabut: jarak sebenarnya
dari mata, atau hampiran `|z_eye|`. Oryon memakai **jarak radial**, karena
itulah yang membuat kabut Minecraft membentuk bola di sekeliling pemain seperti
yang diharapkan. Hampiran `|z_eye|` akan membuat kabut menipis di tepi layar.

## Vertex mode langsung

Tata letak tetap 56 byte - posisi, warna, normal, dua koordinat tekstur -
ditulis penuh untuk tiap vertex. Untuk terrain itu akan boros, tapi Minecraft
tidak memakai mode langsung untuk terrain; hanya untuk GUI dan debug, di mana
kejelasan menang atas bandwidth. Atribut yang benar-benar dinyalakan tetap
mengikuti FFKey, jadi shader tidak pernah menerima atribut yang tidak dipakainya.

## Batas lingkup yang disengaja

`glDrawElements` **tidak diekspor**. Minecraft 1.12.2 tidak pernah memanggilnya
- `WorldVertexBufferUploader` memakai `glDrawArrays` - dan tidak ada flag
kapabilitas dalam profil yang menuntutnya. Mesinnya sudah ada di
`draw_elements()`; menambahkan ekspornya tiga baris bila lingkup diperluas.

Varian mode langsung yang tidak dipanggil 1.12.2 (`glVertex2d`, `glColor3ub`,
dan 80 lainnya) tetap berupa stub. Semuanya hanya ada di daftar ekspor karena
dituntut flag kapabilitas, bukan karena ada yang memanggilnya.
