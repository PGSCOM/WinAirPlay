#!/bin/bash
#https://www.programmersought.com/article/55484044480/
make clean
./configure --target-os=win64 --disable-d3d11va --disable-dxva2 --disable-asm --enable-avdevice --disable-programs --disable-shared --enable-static --enable-cross-compile --prefix=./vs2019_build --toolchain=msvc --arch=x86_64 --enable-mediafoundation --enable-ffmpeg
make -j8