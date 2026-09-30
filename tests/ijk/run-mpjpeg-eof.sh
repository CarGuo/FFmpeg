#!/usr/bin/env bash
set -euo pipefail
SOURCE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=$(cd "${1:?Usage: [MPJPEG_SOURCE=file] [SANITIZE=1] run-mpjpeg-eof.sh FFmpeg-host-build-directory}" && pwd)
MPJPEG_SOURCE="${MPJPEG_SOURCE:-$SOURCE_DIR/libavformat/mpjpegdec.c}"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
EXTRA_FLAGS=()
if [[ "${SANITIZE:-0}" == 1 ]]; then
  EXTRA_FLAGS=(-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"${CC:-cc}" -std=c11 -D_GNU_SOURCE -DHAVE_AV_CONFIG_H -Wall -Wextra -Wno-unused-parameter \
  -Wno-pointer-sign -Wno-missing-field-initializers -Werror=implicit-function-declaration "${EXTRA_FLAGS[@]}" \
  -DMPJPEG_SOURCE="\"$MPJPEG_SOURCE\"" -I"$BUILD_DIR" -I"$SOURCE_DIR" -I"$SOURCE_DIR/libavformat" \
  "$SOURCE_DIR/tests/ijk/mpjpeg-eof.c" \
  -L"$BUILD_DIR/libavformat" -L"$BUILD_DIR/libavcodec" -L"$BUILD_DIR/libavutil" \
  -lavformat -lavcodec -lavutil -lm -lz -lpthread -o "$TEST_DIR/mpjpeg-eof"
"$TEST_DIR/mpjpeg-eof"
