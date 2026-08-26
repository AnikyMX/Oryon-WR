# Membangun Oryon

## Android (arm64-v8a) — yang dipakai CI

```sh
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DANDROID_STL=none \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Hasil: `build/liboryon.so`.

`android-24` adalah level API terendah yang menjamin OpenGL ES 3.2.
`ANDROID_STL=none` disengaja: tidak ada satu pun berkas sumber yang menyentuh
libc++, jadi `.so` tidak menyeret runtime C++ sama sekali.

## Tes host (sandbox / desktop Linux)

```sh
./tests/run.sh
```

Butuh Mesa: `libgles-dev` dan `libegl-dev`. Tes membuat context GLES 3.2 asli
lewat EGL surfaceless, memuat `liboryon.so` persis seperti LWJGL, lalu
menjalankan urutan `GL.createCapabilities()` yang sebenarnya.

## Membangkitkan ulang berkas turunan

```sh
python3 tools/regen.py                                  # dari data/*.json
python3 tools/regen.py --jars lwjgl.jar 1.12.2.jar      # ekstraksi ulang dari jar
```

`data/*.json` ikut di-commit, sehingga repositori bisa dibangun tanpa jar
Minecraft maupun LWJGL. Jar hanya diperlukan bila ingin memperluas lingkup.

## Opsi CMake

| opsi | bawaan | arti |
|---|---|---|
| `ORYON_DEBUG` | OFF | Log verbose. Di rilis, seluruh pemanggilan log hilang saat kompilasi. |
| `ORYON_BUILD_TESTS` | OFF | Bangun `lwjgl_bootstrap` (host saja). |

## Variabel lingkungan saat berjalan

| variabel | arti |
|---|---|
| `ORYON_GLES_LIB` | Menimpa nama pustaka GLES yang di-`dlopen`. Berguna untuk pengujian. |

## Memakainya di Pojav

```
-Dorg.lwjgl.opengl.libname=/path/ke/liboryon.so
```

Tidak ada yang lain. Tanpa JNI, tanpa berkas pendamping, tanpa perubahan di
sisi Java.
