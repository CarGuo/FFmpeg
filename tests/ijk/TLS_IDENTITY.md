# OpenSSL TLS identity regression tests

`tls_openssl.c` verifies the certificate chain and the expected server DNS name or
IP address when `tls_verify=1`. `verifyhost` overrides the reference identity; it
can differ from the TCP destination and can also change its DNS/IP type. IP
addresses match iPAddress SANs, never a DNS SAN containing a textual address.
DNS verification gives SANs precedence over CN, permits full-label wildcards,
and rejects partial-label or multiple-label wildcard matches.

The default remains `tls_verify=0` for compatibility. It does **not** authenticate
the server. Applications that require authenticated HTTPS must explicitly set
`tls_verify=1` and provide a trusted `ca_file` or deliberately configure OpenSSL's
CA store. This Android OpenSSL build does **not** automatically use the Java/
Android platform trust store. The configure-time OpenSSL install prefix is not a
runtime Android CA bundle. A bad or missing explicit CA file fails closed when
verification is enabled; verification-disabled behavior is retained.

The TLS listen/server branch still verifies client certificate chains when
requested. It does not impose the server URL's hostname on a client certificate.

## Run

Build FFmpeg out of tree with the intended OpenSSL prefix. A minimal host profile:

```sh
FFSOURCE=/absolute/path/to/ffmpeg5
OPENSSL_PREFIX=/absolute/path/to/openssl-static-prefix
mkdir -p /absolute/path/to/tls-build
cd /absolute/path/to/tls-build
PKG_CONFIG_LIBDIR="$OPENSSL_PREFIX/lib/pkgconfig" "$FFSOURCE/configure" \
  --disable-doc --disable-x86asm --disable-autodetect --disable-programs \
  --disable-avdevice --disable-avfilter --disable-postproc \
  --disable-swscale --disable-swresample --disable-everything \
  --enable-network --enable-openssl --enable-protocol=http,https,tls,tcp,file \
  --disable-debug --enable-pic
make -j4
"$FFSOURCE/tests/ijk/run_tls_identity.sh" "$PWD" "$PWD/tls-result.json" "$OPENSSL_PREFIX"
```

Without a prefix argument the runner links the host OpenSSL. Requirements are a
Linux C compiler/linker with `--wrap`, Python 3, an `openssl` command supporting
`req -addext`, and IPv4/IPv6 loopback. There are no external network requests.
All certificates and private keys are generated in a temporary directory and
removed after the run. `CFLAGS`, `LDFLAGS`, and `CC` can instrument the probe when
FFmpeg is built with the same sanitizer flags.

The test probe links the actual FFmpeg libavformat TLS/HTTPS backend. It is not a
reimplementation of the verification logic. The matrix exercises correct and
wrong names, trusted and untrusted CAs, IPv4/IPv6 SANs, explicit identity overrides,
wildcards, default and explicit verification-disabled behavior, default trust
stores, invalid CA files, and TLS listen/client-certificate behavior. Link-time
wrappers force hostname/IP/SNI/trust-store configuration failures and BIO
allocation failures and require an error without an application request or crash.
The JSON report records the actual client OpenSSL version, return codes, SNI,
application request observation, and diagnostics for each case.

## Verification scope

The release-remediation run tested this backend against OpenSSL 1.1.1w, 3.5.7,
and the official 3.5.9 source. A separately compiled unmodified backend was the
negative control: it accepted the wrong-host/right-CA fixture, so the regression
failed as intended. The patch's API calls are available in OpenSSL 1.0.2, but no
1.0.2 runtime was tested. Older versions fail closed for verified client use
instead of silently doing chain-only verification.

Passing host tests does not replace Android ABI builds, Android CA integration,
real-device tests, or a complete supported-dependency/security review. It also
does not make verification-disabled playback secure.

## Resolver fallback regression

A separate host build forces `HAVE_GETADDRINFO=0` in its generated `config.h`
after the normal minimal configure and before rebuilding. This compiles the
real IPv4-only `ff_getaddrinfo` replacement in `libavformat/os_support.c`, rather
than a simulated resolver. Run the same script with the OpenSSL prefix and a
fourth `--ipv4-only` argument. Only the two IPv6 transport cases are excluded.
The runner also builds `test_getaddrinfo_fallback.c`, which requires fallback
configuration and verifies DNS/non-numeric → `EAI_NONAME`, numeric IPv4 → success,
and forced allocation failures → `EAI_FAIL` with no returned address.

The normalization of the fallback's `AI_NUMERICHOST`/non-numeric error is needed
because verified TLS intentionally treats `EAI_FAIL` as a real failure. Broadly
accepting `EAI_FAIL` would also conceal allocation failures. The separate
negative control restores only the original `os_support.c`; its valid-DNS TLS
case fails as expected, while the corrected fallback passes all 33 applicable
TLS cases and all six resolver checks. Native `getaddrinfo` behavior is unchanged.
