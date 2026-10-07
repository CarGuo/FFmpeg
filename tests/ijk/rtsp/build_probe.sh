#!/usr/bin/env bash
# Build a demux-only regression probe from a supplied (baseline or patched) source.
set -euo pipefail
SOURCE=$(cd "${1:?FFmpeg source path required}" && pwd)
mkdir -p "${2:?isolated output path required}"
OUT=$(cd "$2" && pwd)
TESTS=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT/build"
cd "$OUT/build"
"$SOURCE/configure" --disable-everything --disable-programs --disable-doc \
 --disable-x86asm --disable-autodetect --disable-avdevice --disable-postproc \
 --disable-avfilter --enable-network --enable-protocol=tcp,udp,rtp,file \
 --enable-demuxer=rtsp,sdp,mov,wav --enable-decoder=pcm_mulaw,pcm_s16le,h264,png \
 --enable-encoder=png --enable-parser=h264 --enable-swscale --enable-swresample \
 --enable-zlib > configure.log 2>&1
make -j"${JOBS:-4}" > build.log 2>&1
cc -O2 -g -I. -I"$SOURCE" "$TESTS/open_probe.c" \
 -Wl,--start-group libavformat/libavformat.a libavcodec/libavcodec.a \
 libavutil/libavutil.a -Wl,--end-group -lm -pthread -lz -o "$OUT/probe"
cc -shared -fPIC -O2 -Wall -Wextra "$TESTS/fixture_resolver.c" \
 -o "$TESTS/fixture_resolver.so" -ldl -pthread
printf 'Probe: %s\n' "$OUT/probe"
