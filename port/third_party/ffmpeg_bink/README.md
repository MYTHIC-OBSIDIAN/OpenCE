# FFmpeg's Bink tables

`binkdata.h` is FFmpeg's `libavcodec/binkdata.h`, copied unchanged:
the Bink video decoder's scan order, Huffman trees, block patterns and
quantisation tables.

| | |
| --- | --- |
| Upstream | https://github.com/FFmpeg/FFmpeg |
| Version | n6.0 (commit ea3d24bbe3c58b171e55fe2151fc7ffaca3ab3d2) |
| Licence | GNU LGPL 2.1 or later (`COPYING.LGPLv2.1`) |
| SHA-256 of `binkdata.h` | e549d4f3df3debd0de047593db32ce9d671de210475c76aefd1a741b815b4080 |
| Used by | `port/linux/src/bink_decoder.c`, the Bink 1 demuxer and video and audio decoders adapted from FFmpeg's (LGPL 2.1 or later, its notices kept), which `port/linux/src/bink_play.c` plays the Xbox's movies with |

`port/tools/bink_check.c` decodes a movie to raw YUV 4:2:0 and float audio,
to compare with FFmpeg's (`ffmpeg -i movie.bik -f rawvideo -pix_fmt yuv420p
ref.yuv -f f32le ref.f32`). On the Xbox's `intro.bik` (640x480, 481 frames,
44.1 kHz stereo) every frame is identical to FFmpeg's, and the audio is
within 7e-6 of it.
