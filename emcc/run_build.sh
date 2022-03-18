#!/bin/bash

#flags="-O2 --memory-init-file 0"              # asm.js version
flags="-O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1" # wasm version
flags="$flags -gsource-map"

srcs="acodec.cpp jarray.cpp"
srcs="$srcs astream.cpp"
srcs="$srcs base/common.cpp"
srcs="$srcs base/signal_processing.cpp"
srcs="$srcs base/rtputils.cpp"

srcs="$srcs spl/resampler.cpp"
srcs="$srcs spl/resample.cpp spl/resample_48khz.cpp spl/resample_by_2_internal.cpp spl/resample_fractional.cpp"
srcs="$srcs spl/resample_by_2.cpp"
srcs="$srcs g711/g711.cpp g711/g711_interface.cpp"

incs="-I../include -I."
libs="-L../.libs -lopus"

em++ $srcs $incs $libs $flags \
    --pre-js preapi.js --post-js api.js \
    -s EXPORTED_FUNCTIONS='["_free"]' -o libac.js
