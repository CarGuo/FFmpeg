# RTSP initial-open redirects and network errors

This update is based on published commit
`ff87490262107e004f6a16765484e36e1973d705` (FFmpeg 5.1.10 integration).
Only `libavformat/rtsp.c`, `rtsp.h` and `rtspdec.c` change in production.

## Behavior

- Follow absolute 301/302 Location during OPTIONS, DESCRIBE, SETUP and the first
  PLAY inside input opening, with at most eight followed redirects
- Recreate streams/transport and clear source-derived SDP timing and session
  metadata; a changed scheme, host or effective port clears origin authentication
- Preserve caller interruption and finite network errors instead of replacing
  them with generic invalid-data errors
- Preserve direct A-port to B-port to A-port reopen behavior

Runtime server REDIRECT, seek/resume PLAY redirects, delayed PLAY after
`initial_pause` and automatic reconnect are not implemented. Relative Location
remains rejected. RTSP `timeout` remains in microseconds; no default change or
automatic unit conversion is introduced.

## Known boundaries

Content-Base or an SDP control URL with another authority is not a Location
redirect. This implementation still uses one control connection. An origin that
points SETUP at another server without proxying it can fail; a gateway accepting
the same absolute URI on its original socket is valid. Blindly switching sockets
would break that gateway case. General multi-control-server session, authentication
and transport routing remains unsupported. Use an explicit server redirect or
an independently verified final playback URL.

Reopening resolves DNS again; established TCP connections do not migrate.
Synchronous `getaddrinfo` cannot be interrupted by the normal FFmpeg callback
while resolution is running.

## Evidence and companion integration

Exact parent/patched host checks cover 13 redirect/endpoint cases, nine network
cases, nine advanced scenarios and four documented limitation controls. They
assert packet identity, stream count, timing and request journals. The matching
IJK update retains RTSP timeout, drains a fatal RTSP I/O error once, and adds an
opt-in local compressed-queue policy. Its initializer pins the resulting FFmpeg
publication commit and reverse-checks this sixth patch plus the five previous
integration patches.

The separately versioned API29 x86_64 software-emulator RTSP suite passed 12
executed cases with one MediaCodec case not applicable. Separate 3x playback-rate
verification failed in both candidate and previous-branch control; this is not
overall compatibility or release qualification. No physical-device or continuous
end-to-end latency guarantee is implied. Exact integration/runtime identities
and retained earlier failed runs are documented in the GSY companion branch.

Run instructions: [host regression tests](../tests/ijk/rtsp/README.md).
Protocol references: [RFC 2326 control URLs and multiple servers](https://www.rfc-editor.org/rfc/rfc2326.html#appendix-C),
[Location](https://www.rfc-editor.org/rfc/rfc2068.html#section-14.30).
