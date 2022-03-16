#!/bin/bash
# usage: ./build.sh
# asm.js version: -O2 --memory-init-file 0
# wasm version: -O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1
flags="-O2 --memory-init-file 0"
flags="-O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1"

srcs="api.cpp g711/g711.cpp g711/g711_interface.cpp"
incs="-I../include -I."
libs="-L../.libs -lopus"

em++ $srcs $incs $libs $flags \
    --pre-js preapi.js --post-js api.js \
    -s EXPORTED_FUNCTIONS='["_free"]' -o libac.js
