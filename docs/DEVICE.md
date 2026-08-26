# Uji Perangkat

Xiaomi M2003J15SC (Redmi Note 9), Mali-G52, Android 11, Zalith Launcher 1.4.1.4,
Minecraft 1.12.2 + OptiFine HD U G5.

## Yang sudah bekerja

Log perangkat mengonfirmasi seluruh Tahap 1-3 di perangkat sungguhan:

```
[OptiFine] OpenGL: Oryon on Mali-G52, version 2.1 Oryon, Oryon
[Shaders] Capabilities:  2.0  2.1  3.0  3.2  -
GL Caps: Using GL 1.3 multitexturing.
         Using GL 1.3 texture combiners.
         Using framebuffer objects because OpenGL 3.0 is supported.
         Shaders are available because OpenGL 2.1 is supported.
         VBOs are available because OpenGL 1.5 is supported.
Using VBOs: Yes
```

`liboryon.so` dimuat lewat `-Dorg.lwjgl.opengl.libname`, hub getProcAddress
menjawab, dan setiap jalur render yang dipilih Minecraft adalah jalur yang
memang ditargetkan Oryon: VBO inti, shader inti, FBO nama inti.

Satu catatan menarik: `OpenGL30` ternyata bernilai TRUE di perangkat meski
Oryon melaporkan versi 2.1 - kemungkinan besar karena patcher milik launcher.
Kebetulan tidak berbahaya: sepuluh fungsi FBO yang dipakai `OpenGlHelper` di
jalur GL30 adalah nama inti tanpa akhiran, dan semuanya memang sudah diekspor.

## Yang membuat crash

```
[OptiFine] Maximum texture size: -1x-1
1280: Invalid enum @ Pre startup
cdo: Unable to fit: minecraft:blocks/lava_flow, size: 32x32,
     atlas: 0x0, atlasMax: -1x-1
```

Penyebabnya **tekstur proxy**. `Minecraft.getGLMaximumTextureSize()` mengukur
atlas seperti ini:

```java
for (int i = 16384; i > 0; i >>= 1) {
    GlStateManager.glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGBA, i, i, 0,
                                GL_RGBA, GL_UNSIGNED_BYTE, (IntBuffer) null);
    int j = GlStateManager.glGetTexLevelParameteri(GL_PROXY_TEXTURE_2D, 0,
                                                   GL_TEXTURE_WIDTH);
    if (j != 0) return i;
}
return -1;
```

Tekstur proxy adalah fitur GL desktop yang **tidak ada sama sekali** di GLES.
Oryon meneruskan `GL_PROXY_TEXTURE_2D` apa adanya, driver menolaknya dengan
`GL_INVALID_ENUM` (itulah "1280" di log), kueri baliknya selalu 0, loop habis,
dan fungsi mengembalikan -1. Atlas berukuran 0x0, lalu Stitcher menyerah.

Sekarang proxy dijawab Oryon sendiri tanpa pernah menyentuh driver: ukuran
diterima bila muat dalam `GL_MAX_TEXTURE_SIZE`, ditolak bila tidak. Loop yang
sama kini mengembalikan ukuran sebenarnya, dan `tests/stage6_test.cpp`
menjalankan salinan persis loop itu sebagai tes regresi.

## Kelas bugnya, bukan cuma bug-nya

Satu enum terlewat berarti mungkin ada yang lain. `tools/audit_enums.py`
memindai setiap konstanta GL yang didorong Minecraft, menyilangkannya dengan
permukaan enum GLES 3.2, lalu **menggagalkan build** bila ada enum non-GLES
yang namanya belum pernah disebut di dalam `src/`.

Hasil pemindaian pertama: 126 enum dipakai, 65 di antaranya tidak ada di GLES,
dan 14 belum tertangani. Dua lagi ternyata bug diam-diam yang belum sempat
terlihat:

| enum | akibat bila diteruskan |
|---|---|
| `GL_RGBA_MODE` | `GL_INVALID_ENUM` yang lalu dilaporkan Minecraft sebagai "GL ERROR" acak |
| `GL_POLYGON_MODE` | sama, plus jawaban sampah untuk kueri mode poligon |

Lima sisanya dikecualikan dengan alasan tertulis di dalam alatnya: dua adalah
nilai kembalian `glCheckFramebufferStatus` (bukan masukan), satu positif palsu,
satu bit mask `glPushAttrib`, dan satu argumen `glLogicOp` yang memang no-op.

Audit ini sekarang berjalan sebagai bagian dari `tools/regen.py`.

## Uji kedua: berjalan

Build 0.6.1, perangkat yang sama. Tidak ada crash report.

```
[OptiFine] OpenGL: Oryon 0.6.1 on Mali-G52, version 2.1 Oryon, Oryon
[OptiFine] Maximum texture size: 8192x8192        <- dulu -1x-1
Created: 1024x512 textures-atlas                  <- dulu crash di sini
[OptiFine] Animated sprites: 22
Starting integrated minecraft server version 1.12.2
Preparing spawn area: 0% ... 31% ... 75%
NidzarMX logged in with entity id 243 at (1656.42, 67.0, 836.73)
NidzarMX joined the game
Grab: true
```

Ukuran tekstur maksimum sekarang terbaca 8192, atlas terjahit, dunia dimuat,
dan pemain masuk ke permainan. Tidak ada lagi baris `GL ERROR` di seluruh log -
perbaikan `GL_RGBA_MODE` dan `GL_POLYGON_MODE` menghapus `1280: Invalid enum`
yang sebelumnya muncul di "Pre startup".

`Grab: true` lalu `false` lalu `true` lagi menunjukkan pemain benar-benar
berinteraksi: masuk dunia, membuka menu jeda, kembali bermain.

### Catatan performa

```
[Server thread/WARN]: Can't keep up! Running 5900ms behind, skipping 118 tick(s)
Changing view distance to 2, from 10
```

Ini thread server, bukan render, dan terjadi tepat saat pembuatan dunia di
perangkat kelas menengah. Tetap jadi alasan untuk memangkas biaya jalur gambar,
dan pemeriksaan pertama menemukan satu pemborosan besar:

`ffp_bind()` mengunggah **setiap** uniform pada **setiap** draw call - warna,
uji alpha, empat uniform kabut, sampai 24 uniform cahaya, dan 16 uniform texgen.
Untuk kunci state terberat itu lebih dari 40 panggilan `glUniform` per gambar,
dan Minecraft mengeluarkan ribuan gambar per frame.

Sekarang uniform dipecah dua blok, masing-masing dijaga penanda serial
per-program: blok matriks naik saat matriks berubah, blok sisanya naik saat
warna, uji alpha, kabut, cahaya, atau bidang texgen berubah. Dalam keadaan
tunak, biayanya turun jadi nol panggilan.

## Uji ketiga: tangan, entitas, dan item hilang

Terrain tampil, tetapi tangan pemain, entitas, dan item tidak. Ketiganya punya
satu kesamaan yang terrain tidak punya: digambar lewat `ModelRenderer`.

```java
private void compileDisplayList(float scale) {
    this.displayList = GLAllocation.generateDisplayLists(1);
    GlStateManager.glNewList(this.displayList, GL11.GL_COMPILE);
    for (ModelBox box : this.cubeList) {
        box.render(bufferbuilder, scale);   // begin -> vertex -> draw, per KOTAK
    }
    GlStateManager.glEndList();
}
```

Satu display list berisi **satu gambar per kotak model**, dan model biped punya
enam. Perekam display list Oryon hanya menyimpan satu `RecDraw` - dialokasikan
di `glNewList`, ditambahkan ke list di `glEndList`. Akibatnya `mode` dan `count`
tertimpa oleh gambar terakhir, sementara daftar atributnya justru menumpuk dari
semua gambar. Yang tersisa satu gambar rusak; entitas praktis tidak muncul.

Sekarang tiap `submit()` menyimpan gambarnya sendiri lalu mengosongkan slot
rekaman untuk gambar berikutnya.

`tests/entity_test.cpp` meniru jalur itu apa adanya - nilai cahaya persis dari
`RenderHelper.enableStandardItemLighting()`, tata letak vertex persis
`OLDMODEL_POSITION_TEX_NORMAL`, tiga kotak dalam satu list, lalu satu list berisi
dua belas kotak. Tes yang sama juga memeriksa pencahayaannya benar-benar
dihitung: tiga kotak dengan normal berbeda harus menghasilkan tiga tingkat
kecerahan yang berbeda, dan nilai harapannya dihitung dari rumus yang sama
dengan shader - bukan angka yang ditanam.

Menariknya, jalur langsung (tanpa display list) sudah benar sejak awal. Itu
menjelaskan mengapa terrain baik-baik saja: terrain tidak pernah lewat display
list.

## Uji keempat: bentuk benar, tekstur hitam

Setelah perbaikan display list, entitas dan item muncul dengan bentuk yang benar
- tetapi hitam pekat. Terrain tetap normal.

Perbedaannya ada di format vertex:

| yang digambar | format | koordinat tekstur |
|---|---|---|
| terrain | `BLOCK` | **dua**: tekstur blok + lightmap |
| entitas | `OLDMODEL_POSITION_TEX_NORMAL` | **satu**: tekstur entitas saja |
| item | `ITEM` | **satu** |

Padahal saat menggambar entitas, **dua** unit tekstur menyala: unit 0 untuk kulit
entitas, unit 1 untuk lightmap. Lalu dari mana koordinat lightmap datang kalau
tidak ada array untuk unit 1? Dari **koordinat berjalan**:

```java
OpenGlHelper.setLightmapTextureCoords(lightmapTexUnit, (float) j, (float) k);
// -> glMultiTexCoord2f(GL_TEXTURE1, j, k)
```

Aturan GL memang begitu: unit tekstur yang menyala tanpa array koordinat memakai
nilai berjalan yang terakhir disetel. Generator shader Oryon justru menuliskan
konstanta:

```glsl
vec4 t = vec4(0.0, 0.0, 0.0, 1.0);   // salah
```

Koordinat (0,0) pada tekstur lightmap adalah sudut paling gelap - hitam pekat.
Setiap entitas dan item lalu dikalikan hitam. Terrain lolos karena format
vertexnya memang membawa koordinat lightmap sendiri.

Sekarang shader memakai uniform `u_curTex<i>` yang berisi nilai berjalan, persis
sesuai aturan GL. Hal yang sama berlaku untuk normal: tanpa array normal,
`u_curNormal` yang dipakai, bukan `vec3(0,0,1)` yang ditanam.

`tests/entity_test.cpp` bagian D mereproduksi susunan itu apa adanya - unit 0
ber-array, unit 1 tanpa array, lightmap 2x2 yang hanya terang di satu texel - dan
memeriksa bahwa memindahkan koordinat berjalan benar-benar mengubah hasilnya.

## Catatan lingkup: OptiFine

Uji ini memakai OptiFine. Crash yang diperbaiki di atas adalah jalur **vanilla**
(`Minecraft.getGLMaximumTextureSize`), jadi tetap terjadi tanpa OptiFine. Tetapi
seluruh ground-truth Oryon diturunkan dari `1.12.2.jar` vanilla; OptiFine
memanggil permukaan GL yang lebih luas. Setelah perbaikan ini, kemungkinan besar
yang muncul berikutnya adalah simbol yang belum diekspor - dan gejalanya akan
berupa `NullPointerException` dari `Checks.check()` di LWJGL, bukan crash GL.

Kalau OptiFine mau didukung penuh, langkahnya sama seperti untuk vanilla:
jalankan `tools/extract_mc_callsites.py` dan `tools/audit_enums.py` terhadap
`OptiFine-1.12.2_HD_U_G5.jar`, lalu perluas daftar ekspor dari hasilnya.
