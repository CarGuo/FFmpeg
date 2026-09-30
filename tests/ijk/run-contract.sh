#!/usr/bin/env bash
set -euo pipefail
SOURCE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=$(cd "${1:?Usage: [SANITIZE=1] run-contract.sh FFmpeg-build-directory}" && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
EXTRA_FLAGS=()
EXTRA_SOURCES=()
if [[ "${SANITIZE:-0}" == 1 ]]; then
  EXTRA_FLAGS=(-D_GNU_SOURCE -DHAVE_AV_CONFIG_H -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  # Instrument the ported code while keeping the existing FFmpeg archive build.
  EXTRA_SOURCES=("$SOURCE_DIR/libavformat/ijkutils.c" "$SOURCE_DIR/libavutil/application.c" "$SOURCE_DIR/libavutil/dict.c")
fi
"${CC:-cc}" -std=c11 -Wall -Wextra -Wno-unused-parameter -Werror=implicit-function-declaration \
  "${EXTRA_FLAGS[@]}" -I"$BUILD_DIR" -I"$SOURCE_DIR" "$SOURCE_DIR/tests/ijk/contract.c" \
  "${EXTRA_SOURCES[@]}" \
  -L"$BUILD_DIR/libavformat" -L"$BUILD_DIR/libavcodec" -L"$BUILD_DIR/libavutil" \
  -lavformat -lavcodec -lavutil -lm -lz -lpthread -o "$TEST_DIR/contract"
"$TEST_DIR/contract" "$TEST_DIR"
