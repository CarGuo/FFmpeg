#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE=$(CDPATH= cd -- "$HERE/../.." && pwd)
WORK=${1:-$(mktemp -d)}
mkdir -p "$WORK"
WORK=$(CDPATH= cd -- "$WORK" && pwd)
# LeakSanitizer is unavailable in some ptrace-based runners. ASan/UBSan remain on.
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
mkdir -p "$WORK/build"
cd "$WORK/build"
"$SOURCE/configure" --disable-everything --disable-programs --disable-doc \
 --disable-x86asm --disable-avdevice --disable-avfilter --disable-swscale \
 --disable-swresample --enable-demuxer=mov --enable-protocol=file \
 --enable-decoder=hevc --enable-parser=hevc --enable-bsf=hevc_mp4toannexb \
 --extra-cflags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
 --extra-ldflags='-fsanitize=address,undefined' > configure.log 2>&1
make -j"${JOBS:-4}" > build.log 2>&1
python3 "$HERE/generate_fixture.py" "$WORK"
cc -fsanitize=address,undefined -I"$SOURCE" -I. "$HERE/test_seek.c" \
 libavformat/libavformat.a libavcodec/libavcodec.a libavutil/libavutil.a \
 -lm -lpthread -lz -o "$WORK/test_seek"
for fixture in two-descriptions two-tracks; do
 for mode in 1 0; do "$WORK/test_seek" "$WORK/$fixture.mp4" "$mode"; done
done
python3 "$HERE/test_guards.py" "$SOURCE" "$WORK"
printf 'Evidence directory: %s\n' "$WORK"
