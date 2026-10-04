> Current Android integration status and exact binary identities are in the
> [2026-10-04 verification note](ijk-ffmpeg5-recovery.md). The dated checks below
> remain historical; they do not override that explicitly bounded current scope.

# IJK / GSY Android migration to FFmpeg 5.1.10

The `ffmpeg-5.0` branch is the Android migration series. Its actual FFmpeg base
is **n5.1.10**, upstream commit
`19feb712f5c1821d8a3fa1ad63c5bd2e3b9672eb`.
The prior **2026-10-01** four-patch source pin was
[`5d01026835e0fc18be967461df180432e85db0c7`](https://github.com/CarGuo/FFmpeg/commit/5d01026835e0fc18be967461df180432e85db0c7).
That dated checkpoint preceded the fifth MOV/HEVC seek-replay repair. For the
current immutable published pin, use the companion
[IJK initializer](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/init-android-ffmpeg5.sh)
and [current verification note](ijk-ffmpeg5-recovery.md). The discovery branch
may advance for documentation; the initializer's immutable commit is the source
identity. The remaining 2026-10-01 status below is retained as historical evidence.

## Release-remediation status (2026-10-01)

Current source repairs HEVC in-band/new-extradata parameter updates and verified
TLS server identity; the companion build selects OpenSSL 3.5.9. Host regressions
passed. The new three-ABI, nine-library Android build is complete, and its
independent static audit passed **73/73** checks.

On the cloud x86_64 Android 11/API 30 **4 KB** emulator, core **21/21**, TLS
**7/7**, and network semantics **4/4** passed. H.264 and ordinary HEVC
MediaCodec synchronous/asynchronous runs passed with the appropriate codec
options. RTSP/RTMP/MJPEG protocol suites passed **8/8** in ordinary and strict-
interruption modes; interruption recovery uses explicit new-player reprepare,
not automatic reconnect. See the companion
[current Android evidence](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/doc/FFMPEG5.md#current-rebuilt-library-android-evidence)
for the exact cases and scope.

The three-frame HEVC parameter-update fixture passed the synchronous gate with
its rendering callback arriving after completion; the asynchronous gate failed
because no initial rendering callback was observed. Correct Android pixels after
parameter changes are not established. An extended-codec run forced MediaCodec
instead of the software-default matrix contract and failed six fallback
assertions; it does not establish format incompatibility or a complete matrix
pass. The correctly configured rerun is pending. New **16 KB** results establish
loading/registration only;
core playback was blocked by a Surface prerequisite timeout. No completed new
endurance/soak, physical ARM device, or final GSY AAR/APK acceptance is claimed.
Binary publication and full release acceptance remain unfinished. The original
nine-library hashes and Android records below remain historical evidence for
the OpenSSL 1.1.1w candidate, not these repaired sources.

## What changed here

- IJK custom protocol and demuxer registration, application callbacks, pointer
  dictionary helpers, concat segment indices, and matching private headers.
  Registration checks reject null or mismatched ABI-size objects. Built-in
  `async` stays upstream's implementation; its legacy registration is a
  compatibility no-op.
- Multipart-JPEG recognizes the exact final MIME delimiter, returns stable EOF,
  and permits replay after a byte seek. Ordinary malformed headers, corrupt
  partial payloads, and I/O errors retain their failure behavior.
- Persistent HEVC VPS/SPS/PPS caching by type/layer/ID. In-band and hvcC
  `AV_PKT_DATA_NEW_EXTRADATA` updates affect later IRAPs and NAL-length changes;
  outgoing side data is Annex B. Malformed packets roll back staged updates.
  Flush restores the original CSD and length, so demuxers must resend applicable
  updates when seeking into changed configurations. The repaired filter SHA-256
  is `168ef23bfeb4f0651a3c7d2713f280c01436f1137004ff5ea507e67a59e7acb4`.
- With `tls_verify=1`, OpenSSL verifies both the chain and DNS/IP identity,
  including explicit `verifyhost`. Configuration failures fail closed. The real
  IPv4-only resolver fallback distinguishes nonnumeric input from allocation
  failure, preserving verified DNS connections without concealing real errors.
- Native host regressions for IJK contracts, MPJPEG and TLS; the HEVC
  packet/decoder regression suite lives in the companion IJK tree.

The four production patches are preserved in the companion
[IJK patch directory](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/patches/ffmpeg5/README.md).
The historical pin contains the first two; the published repaired pin includes
all four. Do not reapply patches to source that already contains them.
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
export OPENSSL_SOURCE="$PWD/extra/openssl-3.5.9"
(
  set -e
  for abi in arm64 armv7a x86_64; do
    android/contrib/compile-ffmpeg5.sh "$abi"
    (cd android && ./compile-ijk.sh "$abi")
  done
)
```

The companion initializer update selects the published immutable repaired
source and verifies all four production patches. Fresh end-to-end initialization
is a separate check, not established by the reverse-patch and syntax checks.
It preserves unexpected existing checkouts rather than resetting them. The legacy `init-android.sh` still selects the older FFmpeg path.

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
player-version override for reproducing the historical candidate with its
original sources and dependency. The current repaired sources cannot reproduce
those bytes solely by overriding a version label.

The current dependency is OpenSSL **3.5.9**, commit
`45e844fa2a14ec92d146bd8f5778ac130b6625fb`, in `extra/openssl-3.5.9`.
Content-addressed TLS caches include actual source/toolchain bytes, ABI/API,
configuration and host tools, and verify installed headers/archive hashes.
Old OpenSSL 1.1.1w caches are not reused. Set `OPENSSL_ONLY=1` for the dependency-
only path; it uses the same cache and writes its own build manifest without
reading `FFMPEG5_SOURCE` or building the player.

See the [GSY integration and validation guide](https://github.com/CarGuo/GSYVideoPlayer/blob/ffmpeg-5.0/doc/ffmpeg-5.0.md)
and [nine-library checksum file](https://github.com/CarGuo/GSYVideoPlayer/blob/ffmpeg-5.0/doc/ffmpeg-5.0.SHA256SUMS)
for the packaged candidate. This FFmpeg repository contains source and host
tests, not those Android library triples.

## TLS compatibility and notices

Default and explicit `tls_verify=0` remain unauthenticated compatibility modes.
For authenticated Android HTTPS, set `tls_verify=1` and supply an explicit
trusted `ca_file` readable by the process. This native build does not
implicitly use Android's Java trust store; a host trust-store test does not
establish Android CA integration. `verifyhost` overrides the expected identity,
not the transport destination. IP references require IP SANs; DNS SANs take
precedence over CN. Server/listen mode retains client chain verification without
applying the server URL's name to the client certificate.

OpenSSL 3.5 defaults reject TLS 1.0/1.1 and weak keys/signatures, including in
verification-disabled mode. Legacy-endpoint compatibility requirements remain
pending; no security-level downgrade is made or implied.

The inspected FFmpeg flags remain `CONFIG_GPL=0`, `CONFIG_VERSION3=0` and
`CONFIG_NONFREE=0`. OpenSSL 3.5.9 carries Apache-2.0 terms; its official unmodified
[LICENSE.txt is retained in IJK](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/doc/third-party/openssl-3.5.9-LICENSE.txt).
Keep the relevant dependency and source notices with redistribution. These
configuration facts are not a licensing certification.

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

The current remediation adds these host results:

- The companion [HEVC packet/decoder suite](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/ijkmedia/ijkplayer/android/pipeline/tests/README.md)
  passed normally and under ASan+UBSan, covering real pixel output from in-band
  and side-data updates, later IRAPs, rollback, repeated flush and a real
  three-frame `hev1` MP4. Original and injection-order-only negative controls
  expose the old failure. This does not certify vendor decoder reconfiguration.
- [TLS identity tests](../tests/ijk/TLS_IDENTITY.md) passed 35/35 on OpenSSL
  1.1.1w, 3.5.7 and 3.5.9, with 35/35 on the 3.5.9 ASan+UBSan build. The real
  IPv4-only resolver-fallback build passed 33/33 applicable TLS cases and six
  resolver checks. Unmodified-backend/fallback controls failed as intended.
  The TLS sanitizer build does not instrument the OpenSSL archive, and leak
  detection was unavailable. Follow that guide for the actual linked-backend
  runner; its host-only results do not close Android trust-store or device work.

## Recorded historical Android results and remaining limits

Results below apply to
[the immutable pre-remediation candidate hashes](https://github.com/CarGuo/GSYVideoPlayer/blob/ece13ca87fd1682f4aa3ba97230bf832f23190b6/doc/ffmpeg-5.0.SHA256SUMS),
not to the new libraries or a later replacement of the branch checksum file.
Rebuilds and other devices need their own checks. All three ABIs compiled and linked;
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
- **Historical TLS:** the frozen OpenSSL 1.1.1w candidate has chain-only
  verification and no complete hostname check. Its trusted/wrong-CA passes
  cannot establish identity validation. The current repair and OpenSSL 3.5.9
  have separate bounded Android results above; the remaining gates are still
  open, and verification remains disabled by default.

GSY APK/AAR builds and matching embedded hashes establish packaging, not full
demo-page or permission-flow coverage. Read the companion guides and retain
the checksum-indexed validation records with any deployment evaluation.

### Follow-up runtime status (2026-10-01 13:56 UTC)

The attempted 1,200-second live-HLS soak did not finish: the emulator disappeared and the runner exited unsuccessfully at09:24 UTC. The retained report is empty, so server requests or elapsed wall time are not a sustained-playback pass. Both simultaneously running emulator processes reported a hanging QEMU main loop; the root cause has not been established. The new API36 16KB environment also has no completed playback result. A single-emulator retry is in progress. The separately rebuilt GSY verification APK packages all nine new libraries byte-identically, uncompressed at16KiB ZIP offsets; packaging verification is not a16KB runtime pass.
