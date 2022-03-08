#!/bin/bash

# opus repo
#git clone https://github.com/xiph/opus.git
#git checkout v1.3.1

# build lib
(
cd ..
./autogen.sh
emconfigure ./configure --disable-rtcd --disable-intrinsics --disable-shared \
    --enable-static --disable-stack-protector --disable-hardening \
    --enable-fixed-point --disable-float-api --disable-doc
emmake make -j4
)

# should ouput libopus.a in .libs/ that is linkable with emcc
