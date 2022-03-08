#!/bin/bash
# usage: ./build.sh
# asm.js version: -O2 --memory-init-file 0
# wasm version: -O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1
flags="-O2 --memory-init-file 0"
flags="-O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1"

em++ api.cpp -I../include -L../.libs -lopus $flags \
    --pre-js preapi.js --post-js api.js \
    -s EXPORTED_FUNCTIONS='["_free"]' -o libopus.js
