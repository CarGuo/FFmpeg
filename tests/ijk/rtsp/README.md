# RTSP initial-open host regression

These host tests use valid loopback RTSP/SDP/PCMU and a process-local resolver
shim. They do not change system DNS or contact public cameras. Requirements:
Python 3, a C compiler, Make and zlib development headers.

Build into an isolated output directory from the repository root:

```sh
bash tests/ijk/rtsp/build_probe.sh "$PWD" /tmp/ffmpeg-rtsp-probe
python3 tests/ijk/rtsp/redirect_matrix.py --probe /tmp/ffmpeg-rtsp-probe/probe --expect patched --out /tmp/redirect.json
python3 tests/ijk/rtsp/network_matrix.py --probe /tmp/ffmpeg-rtsp-probe/probe --expect patched --out /tmp/network.json
python3 tests/ijk/rtsp/advanced_matrix.py --probe /tmp/ffmpeg-rtsp-probe/probe --out /tmp/advanced.json
python3 tests/ijk/rtsp/limitations_probe.py --probe /tmp/ffmpeg-rtsp-probe/probe --out /tmp/limitations.json
```

The build writes test-only `fixture_resolver.so` beside these scripts; never
package it with production libraries. Build the parent in a separate source/output
directory for baseline comparison, using `--expect baseline` where supported.
Matrices assert open/read results, packet identities, stream counts and request
journals. The probe's process exit code alone is not playback success.

`redirect_state_test.c` exercises the production redirect helper with synthetic,
non-networked authentication/session state. Link it to the same patched static
FFmpeg archives and generated/source headers as `open_probe.c`.

The limitation test expects the remaining cross-port Content-Base failure, its
valid gateway control, and delayed synchronous resolution. Its pass means those
limits remain accurately characterized, not that they were fixed.

Android runtime and physical output are separate checks. See the
[behavior and scope](../../../doc/ijk-rtsp-network.md).
