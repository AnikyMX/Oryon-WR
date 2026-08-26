# Uji Perangkat Pertama

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
