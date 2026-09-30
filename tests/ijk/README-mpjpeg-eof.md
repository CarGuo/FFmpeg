# MPJPEG exact final-boundary regression

The upstream n5.1.10 multipart demuxer treats an exact final MIME delimiter as a
new part, then reports INVALIDDATA because no Content-Type follows. The fix
retains the actual first delimiter, recognizes only that delimiter plus `--`,
and returns stable EOF tied to the current AVIO position. A generic byte seek
away from that position resets terminal state so the same demuxer can replay.
Ordinary permissive boundary matching is unchanged.
A boundary token ending in hyphens is not mistaken for a final delimiter.

The test compiles the **actual selected `libavformat/mpjpegdec.c`** and links the
existing native FFmpeg 5 static archives. It uses real custom AVIO, MIME options,
format initialization and the demuxer's packet entry point. It does not model or
reimplement the parser. Four-byte JPEG-marker payloads isolate MIME packet
integrity; this is not JPEG decoding, HTTP transport, Android or player evidence.

```sh
# A configured/built native FFmpeg 5 tree is required; no Android rebuild occurs.
bash tests/ijk/run-mpjpeg-eof.sh /path/to/ffmpeg-host-build

# Reproduce red using pristine n5.1.10 source, without changing the checkout.
git show 19feb712f5c1821d8a3fa1ad63c5bd2e3b9672eb:libavformat/mpjpegdec.c > /tmp/mpjpeg-upstream.c
MPJPEG_SOURCE=/tmp/mpjpeg-upstream.c \
  bash tests/ijk/run-mpjpeg-eof.sh /path/to/ffmpeg-host-build

# Instrument actual changed demuxer + test; existing static archives are not
# instrumented. LeakSanitizer is unavailable under this environment's ptrace.
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 \
  bash tests/ijk/run-mpjpeg-eof.sh /path/to/ffmpeg-host-build
```

Observed results: pristine upstream 9/22 (thirteen expected final-delimiter/edge-case failures);
patched 22/22; patched ASan+UBSan 22/22.

Coverage includes strict/permissive final delimiters, MIME epilogues and stable
repeated EOF; actual `av_seek_frame(..., AVSEEK_FLAG_BYTE)` replay after EOF;
maximum70-byte boundary tokens including initial/final padding; hyphen-ending boundary tokens; transport padding; permissive
ordinary boundary changes; strict wrong-boundary rejection; missing/non-JPEG
Content-Type; wrong final delimiter; truncated headers; EOF between parts;
short declared payload marked corrupt; and injected EIO during payload transfer.
Malformed ordinary headers still fail. Partial payloads remain corrupt and EIO
is still propagated. No malformed input is relabeled successful finite playback.

The patch does not promise automatic reconnect, timestamps for live multipart,
or successful finite playback in IJK. Those require the separate Android suite.
