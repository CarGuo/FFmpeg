# HLS TLS option inheritance regression

HTTP(S)-backed HLS inputs must retain explicit transport policy when they open
media playlists, live-playlist reloads, keys, media segments and initialization
sections. `ffio_copy_url_options` copies the explicit TLS allowlist from HTTP's
saved lower-protocol dictionary, including when no TLS child remains after a
redirect. It does not enable verification by default.

The allowlist is `tls_verify`, `ca_file`, `cafile`, `verifyhost`, `cert_file`,
`key_file`. Explicit peer-name overrides and client-authentication files apply to
nested resources just as they already apply to HTTP redirects. Do not pass
client-authentication settings to an untrusted playlist. The `listen` setting and
unrelated demuxer options are not inherited. Explicit empty `http_proxy` means
"do not use an environment proxy"; an unset proxy remains unset.

CA aliases retain their input order through HTTP replay, including initial opens,
redirects and reconnects. Saved HTTP lower-protocol settings retain precedence
over pending settings. Allocation failures abort rather than silently discard
verification or proxy policy. HLS keepalive fallback rebuilds a fresh options
dictionary after the failed request consumed its HTTP options.

## Host regression

Requirements: POSIX host, C compiler, GNU make, development headers/libraries for
OpenSSL, and the normal FFmpeg build prerequisites. The test does not connect to
any server, create sockets, require certificates, run target binaries or download
anything. Use a build directory outside the source tree:

    tests/hls-tls-options.sh /absolute/path/to/hls-tls-host
    tests/hls-tls-options.sh /absolute/path/to/hls-tls-https-only https

The test compiles the real HTTP and HLS implementations and links the actual
AVIO implementation. It covers:

- HTTP and HTTPS sources; no invented default verification/proxy policy
- Every allowlisted TLS option; explicit verification off; unrelated-option exclusion
- Both CA alias orders, actual `TLS_COMMON_OPTIONS` application, and effective CA
  consistency between source and nested HTTP replay
- Saved-option precedence and preservation of other pending options
- Unset, empty and nonempty proxy overrides; existing custom-header propagation
- Child/reloaded playlist, plain segment, AES-128 key/encrypted segment,
  SAMPLE-AES key/segment and EXT-X-MAP initialization open paths
- Protocol whitelist/blacklist preservation at the HLS I/O callback
- Forced keepalive failure after HTTP options are consumed, with fallback policy
  checked before any network operation (HTTP-enabled build)
- Allocation failure on AVOption retrieval, explicit TLS export and atomic HTTP replay

The source-level callbacks intentionally stop before protocol/network opens.
These tests are not a TLS-handshake, Android, media-decoding or real-server test.
Physical-device testing must additionally use a trusted initial playlist with
independently untrusted media-playlist/key/segment/init endpoints, test redirects
and live reload/reconnect, and verify that no HTTP request occurs after a rejected
TLS handshake. A custom/local initial AVIO does not expose HTTP's saved options;
its I/O provider remains responsible for applying TLS policy to nested opens.

The shared URL-option helper is also used by DASH and IMF demuxers when enabled;
they gain the same explicit HTTP(S) TLS/proxy inheritance semantics.
