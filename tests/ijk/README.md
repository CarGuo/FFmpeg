# ijkplayer fork contract tests

These tests exercise the FFmpeg-facing hooks carried forward from
CarGuo/FFmpeg `ijk-n4.3-20260301-007` onto upstream FFmpeg `n5.1.10`.
They are native host tests, not Android MediaCodec or end-to-end player tests.

## Build and run

Use an out-of-tree native FFmpeg build (no x86 assembler required):

```sh
mkdir -p ../ffmpeg-host && cd ../ffmpeg-host
../ffmpeg5/configure --disable-doc --disable-x86asm --disable-autodetect \
  --disable-programs --disable-avdevice --disable-avfilter --disable-postproc \
  --disable-everything --enable-pic \
  --enable-protocol=file,async,ijkio,ijkhttphook,ijklongurl,ijkmediadatasource,ijksegment,ijktcphook \
  --enable-demuxer=ijklivehook,ijklas,concat,wav,mov \
  --enable-decoder=pcm_s16le,png,h264,aac --enable-encoder=png \
  --enable-parser=h264,aac --enable-zlib
make -j4
../ffmpeg5/tests/ijk/run-contract.sh .
SANITIZE=1 ../ffmpeg5/tests/ijk/run-contract.sh .
```

The sanitizer variant instruments the ported protocol/application/dictionary
source and test harness. Existing upstream FFmpeg static archives are not
instrumented. On an execution environment using ptrace, LeakSanitizer reports
that it cannot run; `ASAN_OPTIONS=detect_leaks=0` allows ASan and UBSan to run,
but is not a leak-check pass.

## Coverage

- Six configurable custom URL protocols, registration/open dispatch and rejection
  of null or mismatched ABI-size objects
- Two custom demuxers, registry lookup and input opening
- Built-in async protocol remains unchanged and can read a file after the
  compatibility no-op registration
- Concat segment indices reach custom segment URL callbacks in order (0, 1)
- Pointer dictionary helpers preserve zero, high-bit, maximum and real addresses
- Callback control return values, HTTP filesize payloads larger than 32 bits,
  async buffer statistics, traffic payloads and close/reset behavior

Expected ABI-mismatch diagnostics are from negative tests.
