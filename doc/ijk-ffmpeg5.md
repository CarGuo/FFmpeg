# IJK / GSY Android migration to FFmpeg 5.1.10

The `ffmpeg-5.0` branch is the Android migration series. Its actual FFmpeg base
is **n5.1.10**, upstream commit
`19feb712f5c1821d8a3fa1ad63c5bd2e3b9672eb`.
The companion IJK initializer pins the patched FFmpeg source commit
[`4a62cc600696982f189fbad6c6ea1593e909b814`](https://github.com/CarGuo/FFmpeg/commit/4a62cc600696982f189fbad6c6ea1593e909b814).
Documentation commits can advance this branch without changing that source pin.

## What changed here

- IJK custom protocol and demuxer registration, application callbacks, pointer
  dictionary helpers, concat segment indices, and matching private headers.
  Registration checks reject null or mismatched ABI-size objects. Built-in
  `async` stays upstream's implementation; its legacy registration is a
  compatibility no-op.
- Multipart-JPEG recognizes the exact final MIME delimiter, returns stable EOF,
  and permits replay after a byte seek. Ordinary malformed headers, corrupt
  partial payloads, and I/O errors retain their failure behavior.
- Native host regression tests for the IJK contracts and MPJPEG parser.

These are the two patches preserved in the companion
[IJK patch directory](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/patches/ffmpeg5/README.md).
They are already applied to the pinned fork commit; do not apply them again.
Player-side API, MediaCodec, seek/EOF and buffering migration changes live in
the companion IJK branch, so this source alone does not replace the player.

## Android build entry point

Use [CarGuo/ijkplayer `ffmpeg-5.0`](https://github.com/CarGuo/ijkplayer/tree/ffmpeg-5.0)
and its [full build guide](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/doc/FFMPEG5.md).
The tested toolchain was Linux x86_64 with Android NDK r22b (`22.1.7171670`).
Bash, Git, Perl, GNU Make, `sha256sum`, and NASM or YASM for x86_64 are required.
Start from a fresh companion checkout:

```sh
git clone --branch ffmpeg-5.0 https://github.com/CarGuo/ijkplayer.git ijkplayer-ffmpeg5
cd ijkplayer-ffmpeg5
./init-android-ffmpeg5.sh
export ANDROID_NDK=/absolute/path/to/android-ndk-r22b
export FFMPEG5_SOURCE="$PWD/extra/ffmpeg5"
export OPENSSL_SOURCE="$PWD/extra/openssl5"
for abi in arm64 armv7a x86_64; do
  android/contrib/compile-ffmpeg5.sh "$abi"
  (cd android && ./compile-ijk.sh "$abi")
done
```

The new initializer selects the immutable patched source and verifies both
patches. It preserves unexpected existing checkouts rather than resetting
them. The legacy `init-android.sh` still selects the older FFmpeg path.

The build explicitly uses `config/module-lite-more.sh`, including MJPEG parser,
MPJPEG demuxer, IJK demuxers, HTTPS, and H.264/HEVC Annex-B filters. Targets are
`arm64` (arm64-v8a, API 21), `armv7a` (armeabi-v7a, API 16), and `x86_64`
(API 21). Use separate source/build trees from FFmpeg 4: output directories are
shared with the legacy scripts. Keep each ABI's `libijkffmpeg.so`,
`libijkplayer.so`, and `libijksdl.so` together.

The script exports `revision=n5.1.10` to retain the tested FFmpeg runtime version
label, and records actual source identity, configuration, toolchain and hashes
in its per-ABI `output/build-manifest.txt`. This is not a guarantee of identical
binaries across environments. The IJK guide documents the separate, opt-in
player-version override for reproducing the frozen candidate. OpenSSL archives
are cached; use fresh build directories if source, toolchain or flags change.

See the [GSY integration and validation guide](https://github.com/CarGuo/GSYVideoPlayer/blob/ffmpeg-5.0/doc/ffmpeg-5.0.md)
and [nine-library checksum file](https://github.com/CarGuo/GSYVideoPlayer/blob/ffmpeg-5.0/doc/ffmpeg-5.0.SHA256SUMS)
for the packaged candidate. This FFmpeg repository contains source and host
tests, not those Android library triples.

## Host tests

Follow [the native build instructions](../tests/ijk/README.md), then run from
this FFmpeg repository root with the configured host build directory:

```sh
bash tests/ijk/run-contract.sh /path/to/ffmpeg-host-build
bash tests/ijk/run-mpjpeg-eof.sh /path/to/ffmpeg-host-build
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 \
  bash tests/ijk/run-contract.sh /path/to/ffmpeg-host-build
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 \
  bash tests/ijk/run-mpjpeg-eof.sh /path/to/ffmpeg-host-build
```

All six IJK contract groups and MPJPEG 22/22 passed, including these ASan+UBSan
variants. The MPJPEG suite compiles the actual selected demuxer source; see its
[coverage and pristine-upstream comparison](../tests/ijk/README-mpjpeg-eof.md).
Sanitizers instrument the changed code and harness, not the existing FFmpeg
archives. `detect_leaks=0` accommodates the tested ptrace environment and is not
a leak-check pass. Host tests do not establish Android playback.

## Recorded Android results and remaining limits

Results below apply to the exact candidate hashes linked above. Rebuilds and
other devices need their own checks. All three ABIs compiled and linked;
64-bit ELF load alignment is 16 KB and ARMv7 alignment is 4 KB. Runtime evidence
is primarily cloud x86_64 Android.

- **Official Android 15/API 35 16 KB emulation:** loading and registrations,
  21/21 core cases plus 52 callback/state assertions, 9/9 extended codec cases,
  and 8/8 scoped RTSP/RTMP/MJPEG cases passed. Actual GSY rotation frames passed
  6/8: all software angles and MediaCodec 90/180 degrees. MediaCodec 0/270
  remained black. A fresh image also crashed `system_server` before app
  installation; remaining 16 KB checks were not closed. This is official
  emulation evidence, not physical ARM64 device certification.
- **Android 11/API 30 4 KB emulation:** trusted/wrong-CA HTTPS 2/2, forced RTSP
  UDP, synchronous/asynchronous MediaCodec 2/2, and actual GSY rotation 8/8
  passed. Sustained playback reached 1202.4 seconds and create/release passed
  50/50. Small memory growth was recorded; these are bounded observations.
  Separate fault-injection variants passed sync/async software fallback 2/2;
  they are not the production library set.
- **Open validation:** 4 KB results do not close 16 KB sustained-run, HTTPS,
  forced-UDP, sync/async/fault-fallback, or the two hardware-rotation gaps.
  Real ARM64/ARMv7 playback, vendor MediaCodec on phones/TVs, long-running
  multi-camera/NVR use and real audiovisual synchronization remain unverified.
  Explicit player recreation after RTSP/RTMP interruption does not establish
  automatic reconnect or bounded terminal callbacks. Fallback can wait for the
  next keyframe. iOS, legacy ABI/slim distributions and the selected profile's
  unavailable embedded `mov_text` callbacks are outside completed scope.
- **TLS:** OpenSSL 1.1.1w is retained and outdated. Certificate verification
  remains disabled by default; even when enabled, this OpenSSL backend does
  not perform complete hostname validation. Passing test-CA HTTPS cases does
  not establish a complete TLS security policy.

GSY APK/AAR builds and matching embedded hashes establish packaging, not full
demo-page or permission-flow coverage. Read the companion guides and retain
the checksum-indexed validation records with any deployment evaluation.
