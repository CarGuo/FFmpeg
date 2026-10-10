#!/bin/sh
# Build and run the source-level HLS TLS regression. No network I/O is performed.
set -eu
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 ABSOLUTE_BUILD_DIR [http,https|https]" >&2
    exit 2
fi
src=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$1" in /*) build=$1 ;; *) echo "Build directory must be absolute" >&2; exit 2 ;; esac
case "${2:-http,https}" in http,https|https) protocols=${2:-http,https} ;; *) exit 2 ;; esac
mkdir -p "$build"
cd "$build"
"$src/configure" --cc="${CC:-cc}" --disable-everything --disable-x86asm \
    --disable-autodetect --disable-programs --disable-doc --enable-pic \
    --enable-protocol="$protocols,tls,tcp,crypto,file" --enable-openssl \
    --enable-demuxer=hls > configure.log 2>&1
make -j"${JOBS:-4}" libavformat/libavformat.a libavcodec/libavcodec.a \
    libavutil/libavutil.a > build.log 2>&1
# Including http.c and hls.c gives the test access to their private retained
# state. The actual aviobuf object is linked from the production library.
"${CC:-cc}" -I. -I"$src" -DHAVE_AV_CONFIG_H -Wno-deprecated-declarations \
    -o hls_tls_options "$src/libavformat/tests/hls_tls_options.c" \
    libavformat/libavformat.a libavcodec/libavcodec.a libavutil/libavutil.a \
    -lssl -lcrypto -lm -lpthread -ldl > test-build.log 2>&1
./hls_tls_options
