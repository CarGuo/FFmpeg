FFmpeg README
=============

## IJK / GSY Android FFmpeg 5 migration

The `ffmpeg-5.0` branch contains **FFmpeg 5.1.10** with IJK compatibility
hooks and a multipart-JPEG final-boundary EOF fix. The branch name identifies
the migration series, not upstream FFmpeg 5.0.

Start with the [migration guide](doc/ijk-ffmpeg5.md) for the pinned source,
Android build entry points, host tests, and the remaining 16 KB/device/TLS
validation limits. Android builds use the companion
[CarGuo/ijkplayer `ffmpeg-5.0` instructions](https://github.com/CarGuo/ijkplayer/blob/ffmpeg-5.0/doc/FFMPEG5.md).
The [GSY integration guide](https://github.com/CarGuo/GSYVideoPlayer/blob/ffmpeg-5.0/doc/ffmpeg-5.0.md)
identifies the tested three-ABI library set and its checksums.

## Upstream FFmpeg

FFmpeg is a collection of libraries and tools to process multimedia content
such as audio, video, subtitles and related metadata.

## Libraries

* `libavcodec` provides implementation of a wider range of codecs.
* `libavformat` implements streaming protocols, container formats and basic I/O access.
* `libavutil` includes hashers, decompressors and miscellaneous utility functions.
* `libavfilter` provides means to alter decoded audio and video through a directed graph of connected filters.
* `libavdevice` provides an abstraction to access capture and playback devices.
* `libswresample` implements audio mixing and resampling routines.
* `libswscale` implements color conversion and scaling routines.

## Tools

* [ffmpeg](https://ffmpeg.org/ffmpeg.html) is a command line toolbox to
  manipulate, convert and stream multimedia content.
* [ffplay](https://ffmpeg.org/ffplay.html) is a minimalistic multimedia player.
* [ffprobe](https://ffmpeg.org/ffprobe.html) is a simple analysis tool to inspect
  multimedia content.
* Additional small tools such as `aviocat`, `ismindex` and `qt-faststart`.

## Documentation

The offline documentation is available in the **doc/** directory.

The online documentation is available in the main [website](https://ffmpeg.org)
and in the [wiki](https://trac.ffmpeg.org).

### Examples

Coding examples are available in the **doc/examples** directory.

## License

FFmpeg codebase is mainly LGPL-licensed with optional components licensed under
GPL. Please refer to the LICENSE file for detailed information.

## Contributing

Patches should be submitted to the ffmpeg-devel mailing list using
`git format-patch` or `git send-email`. Github pull requests should be
avoided because they are not part of our review process and will be ignored.
