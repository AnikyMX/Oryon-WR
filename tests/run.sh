#!/bin/sh
# Tes kontrak bootstrap LWJGL di host, memakai Mesa EGL + GLES sebagai driver
# sungguhan. Bukan simulasi: context GLES 3.2 asli, perintah GL asli.
set -e
cd "$(dirname "$0")/.."

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DORYON_BUILD_TESTS=ON \
    -DORYON_DEBUG=ON
cmake --build build --parallel

echo
echo "########## Tahap 3: kontrak bootstrap LWJGL ##########"
LIBGL_ALWAYS_SOFTWARE=1 ./build/lwjgl_bootstrap ./build/liboryon.so

echo
echo "########## Tahap 4: state fixed-function + generator shader ##########"
LIBGL_ALWAYS_SOFTWARE=1 ./build/shadergen_test

echo
echo "########## Tahap 5: menggambar sungguhan ##########"
LIBGL_ALWAYS_SOFTWARE=1 ./build/render_test

echo
echo "########## Tahap 6: tekstur, display list, shader Minecraft ##########"
LIBGL_ALWAYS_SOFTWARE=1 ./build/stage6_test

echo
echo "########## Jalur entitas: display list + pencahayaan ##########"
LIBGL_ALWAYS_SOFTWARE=1 ./build/entity_test
