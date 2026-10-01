#!/usr/bin/env bash
# Usage: run_tls_identity.sh FFMPEG_BUILD RESULT_JSON [OPENSSL_PREFIX] [--ipv4-only]
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
SOURCE=$(cd "$HERE/../.." && pwd)
BUILD=$(cd "$1" && pwd)
RESULT=$2
read -r -a EXTRA_CFLAGS <<< "${CFLAGS:-}"
read -r -a EXTRA_LDFLAGS <<< "${LDFLAGS:-}"
OPENSSL_FLAGS=(-lssl -lcrypto)
if [[ $# -ge 3 ]]; then
    PREFIX=$(cd "$3" && pwd)
    OPENSSL_FLAGS=(-I"$PREFIX/include" "$PREFIX/lib/libssl.a" "$PREFIX/lib/libcrypto.a")
fi
WRAP=(-Wl,--wrap=X509_VERIFY_PARAM_set1_host,--wrap=X509_VERIFY_PARAM_set1_ip,--wrap=SSL_CTX_set_default_verify_paths,--wrap=BIO_meth_new,--wrap=BIO_new,--wrap=SSL_ctrl)
"${CC:-cc}" "${EXTRA_CFLAGS[@]}" -DTLS_FAULT_INJECTION -I"$SOURCE" -I"$BUILD" \
    "$HERE/tls_identity_client.c" "$BUILD/libavformat/libavformat.a" \
    "$BUILD/libavcodec/libavcodec.a" "$BUILD/libavutil/libavutil.a" \
    "${WRAP[@]}" "${OPENSSL_FLAGS[@]}" "${EXTRA_LDFLAGS[@]}" \
    -lm -lpthread -ldl -o "$BUILD/tls_identity_client"
TEST_FLAGS=()
if [[ ${4:-} == --ipv4-only ]]; then
    TEST_FLAGS+=(--ipv4-only)
    "${CC:-cc}" "${EXTRA_CFLAGS[@]}" -D_POSIX_C_SOURCE=200112 \
        -I"$SOURCE" -I"$BUILD" "$HERE/test_getaddrinfo_fallback.c" \
        "$BUILD/libavformat/libavformat.a" "$BUILD/libavutil/libavutil.a" \
        "${EXTRA_LDFLAGS[@]}" -lm -lpthread -ldl -o "$BUILD/test_getaddrinfo_fallback"
    "$BUILD/test_getaddrinfo_fallback"
fi
python3 "$HERE/test_tls_identity.py" --binary "$BUILD/tls_identity_client" --output "$RESULT" "${TEST_FLAGS[@]}"
