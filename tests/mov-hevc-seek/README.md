# MOV HEVC seek replay regression (new reconstruction, 2026-10-02)

Run `tests/mov-hevc-seek/run.sh /absolute/scratch/output` from any directory.
Dependencies: POSIX shell, Python 3, C compiler, make, zlib development files,
and a system `ffmpeg` with `libx265` encoding. The system encoder generates
fixtures only; all tested demux, BSF, and decoder code comes from this checkout.

The generator preserves separately encoded x265 hvc1 descriptions and their
matching slice packets. In-band VPS/SPS/PPS emitted by x265 are verified
against hvcC and removed for hvc1 conformance; VCL bytes are unchanged. All frames are IDR, with 64x64 and 96x64 coded pictures;
12 samples use descriptions 1, 2, then 1. It builds normal indexed MOV sample
tables and a second fixture with two tracks sharing those media extents.
It does not mutate parameter-set syntax or pair slices with unrelated hvcC.

The integration test first sequentially decodes the fixture with this checkout's
HEVC BSF and decoder, then seeks repeatedly, flushes both, requires fresh
NEW_EXTRADATA on every track, and compares dimensions and MD5 of every decoded
YUV plane to that sequential reference. Both seek modes are tested, including
non-target-track description reselection. This gives 72 exact-YUV seek checks,
not 72 distinct media assets or an Android certification.

A production-helper extraction test exercises eight exclusion guards (including
fragment-only and exhausted tracks), both reselection modes, failed side-data
allocation leaving the last description unchanged, and successful retry.
Those 12 checks use minimal test types, not the complete demuxer environment.
Both layers run ASan/UBSan. Leak detection defaults off because the recovery
runner's ptrace environment rejects LeakSanitizer; no leak-pass claim is made.

The fresh baseline negative control, built from eeda3f84's mov.c with the same
libraries and test, fails at the NEW_EXTRADATA assertion. This is newly created
evidence and does not restore or inherit the historical 120-check report.

No Android vendor MediaCodec behavior or complete player integration is covered.
