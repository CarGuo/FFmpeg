# FFmpeg 5 Android integration verification — 2026-10-04

## Source and build identity

The branch name remains `ffmpeg-5.0`; the upstream version is **FFmpeg 5.1.10**.
This publication includes the exact MOV/HEVC seek-replay source and regression
files from the recovered build commit `0cb0a06ef3d2b8e2814cb0e60be99d7986e1bde1`
(tree `6621f9352eef1ec363cd0899e24a79d7cfb0f521`). Public documentation is updated
here; compiled library hashes below identify the already-built binaries. A new
publication commit is not a claim that embedded build-version strings changed.

Android builds use the companion [IJK FFmpeg 5 build guide](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/doc/FFMPEG5.md),
`init-android-ffmpeg5.sh`, `android/contrib/compile-ffmpeg5.sh`, NDK r22b,
`config/module-lite-more.sh`, and OpenSSL 3.5.9. The companion initializer pins
an immutable published FFmpeg commit and verifies all five expected patches;
`ffmpeg-5.0` is the discovery branch, not permission to silently use a moving
source revision. Existing unexpected checkouts are preserved and rejected.

## Current bounded Android evidence

The exact ARM64 FFmpeg library below was carried unchanged through the IJK
`platformrate-r3` integration and tested on a physical Pixel 8, Android API 35,
with a 4 KiB system page size:

- The official Mux HTTPS-HLS AVC/AAC sample prepared, rendered and advanced
  normally. This establishes ordinary playback, not comprehensive TLS security.
- All five public GSY/IJK speeds, 0.25×, 0.5×, 1×, 2× and 3×, passed actual
  media-position and completion-time assertions. These rate/clock fixes are in
  IJK/AudioTrack, not new FFmpeg codec changes.
- Public pause/resume, seek, replay, rate transitions and the audio-drained video
  tail passed two focused cases. Seven unchanged 1× tail/clock cases and three
  actual GSY widget cases passed independently.

The original Apple sample hostname did not resolve in the test environment.
Its failure remains in the historical seven-case report; it was not converted
into a pass. That unavailable test address is excluded from release assessment
and is not a library playback defect. No further Apple investigation is required.

ARMv7 and x86_64 have build/link/ABI evidence, not physical runtime qualification.
64-bit ELF LOAD alignment is 16 KiB; this is distinct from a 16 KiB device runtime
test. Hardware-decoder and long-duration coverage remain bounded by the companion
reports. The public rate path is qualified on the stated device; it does not
promise every device/rate/pitch combination, and pre-23 AudioTrack non-1× support
is not added by this work. No new comprehensive memory-safety certification is
claimed by these functional playback results.

## Carried FFmpeg binary identities

The complete native set consists of `libijkffmpeg.so`, `libijkplayer.so` and
`libijksdl.so` for each ABI. Keep each ABI's matched triple together; do not mix
these FFmpeg hashes with unrelated IJK/SDL builds. The final integration/checksum
manifest is maintained in [GSYVideoPlayer](https://github.com/CarGuo/GSYVideoPlayer/blob/ffmpeg-5.0/doc/ffmpeg-5.0.md).

| ABI | libijkffmpeg.so bytes | SHA-256 |
| --- | ---: | --- |
| arm64-v8a | 11815360 | `913ad7b65ccdd133f0ae5890d7d1c7b44338dafdc87cfc0634fc771b68cd0488` |
| armeabi-v7a | 10932464 | `6c069c7306830f4e6e0af4eaedcde1b25a12ff1e890b8b689737dd570aa7066e` |
| x86_64 | 16231176 | `7cb7407ba2e632c965e343ecfb99f0192977d230d05e7e000fc8c3cc652f8abb` |

## Historical recovery evidence

The following 2026-10-02 note is retained as the historical source-recovery
checkpoint. Its then-outstanding Android release gates are superseded only by
the explicit 2026-10-04 scope above; its host-only checks are not silently
relabelled as device or security coverage.

# FFmpeg 5 recovery candidate — 2026-10-02

The source baseline is published commit `eeda3f84b84b95cbecc5604b343a9100ad656311`
and remains FFmpeg **5.1.10**. This recovery reconstructs the missing MOV/HEVC
seek change; it does not claim to recover the exact unpublished historical file.

## Changed behavior

After a successful seek, eligible HEVC tracks with multiple sample descriptions
replay the selected description through `AV_PKT_DATA_NEW_EXTRADATA`. This is
needed when the Annex-B filter/decoder is flushed but the active description
has not changed. The interleaved seek path reselects `stsc` state. Fragment-only,
missing-table and exhausted tracks are excluded. The current description index
is committed only after side-data allocation/copy succeeds.

## Fresh evidence

Run `tests/mov-hevc-seek/run.sh` as described in that directory. Newly generated
fixtures use matching encoder hvcC records and IDR pictures with conforming hvc1
parameter-set placement. The checked suite passes 72 exact decoded-YUV seek
comparisons and 12 focused guard/allocation cases. Full-library AddressSanitizer
and UndefinedBehaviorSanitizer variants passed; LeakSanitizer was unavailable in
the execution environment. The original baseline fails the expected extradata
replay assertion. An independent extended check compared 462 decoded frames
through subsequent description transitions after seeks.

These are host demux/filter/software-decoder checks. They do not establish
Android Surface output, physical vendor MediaCodec behavior, sustained playback,
or final release acceptance. Three Android ABI FFmpeg libraries were rebuilt
with the companion module-lite-more.sh profile and OpenSSL 3.5.9; exact binary
identities belong to the companion build manifests, not these host results.

## Outstanding scope

The published HEVC parameter-update and TLS peer-identity changes remain present.
The later missing HTTPS-HLS child-option propagation and playback-speed changes
are not included or represented as passed. Android runtime and complete GSY
packaging validation remain required before publication. Keep historical test
records separate from this newly reconstructed candidate.
