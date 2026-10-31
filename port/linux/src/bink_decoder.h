/*
BINK_DECODER.H

A Bink 1 movie (.bik, revisions f to k) read and decoded: its frames as
YUV 4:2:0 planes, its first audio track as interleaved float samples
(bink_decoder.c, adapted from FFmpeg's Bink demuxer and decoders: LGPL 2.1
or later). Standalone C with fixed-width types, so that a host tool can
check it against FFmpeg's own output.
*/

#ifndef __BINK_DECODER_H
#define __BINK_DECODER_H

#include <stdint.h>
#include <stdio.h>

struct bink_decoder;

/* the movie's file opened and its header read; NULL (with why in error)
when it is not a Bink 1 movie this decodes */
struct bink_decoder *bink_decoder_open(FILE *file, char *error, int error_size);
void bink_decoder_close(struct bink_decoder *bink);

int bink_decoder_width(struct bink_decoder const *bink);
int bink_decoder_height(struct bink_decoder const *bink);
uint32_t bink_decoder_frame_count(struct bink_decoder const *bink);
/* frames per second, as a fraction */
uint32_t bink_decoder_fps_numerator(struct bink_decoder const *bink);
uint32_t bink_decoder_fps_denominator(struct bink_decoder const *bink);
/* the first audio track's: 0 when it has none */
int bink_decoder_audio_rate(struct bink_decoder const *bink);
int bink_decoder_audio_channels(struct bink_decoder const *bink);

/* Decodes frame `index` (0 is the first; the frames after the last one
decoded, in order, else from the first again): 0 on success. Its audio,
interleaved floats in [-1, 1], goes to `audio` as it is decoded. */
typedef void (*bink_audio_proc)(void *context, float const *samples, int sample_count);
int bink_decoder_decode(struct bink_decoder *bink, uint32_t index, bink_audio_proc audio, void *context);

/* the last frame decoded's planes: Y, then U and V at half the size, each
with its pitch */
void bink_decoder_planes(struct bink_decoder const *bink, uint8_t const *planes[3], int pitches[3]);

/* the last frame decoded as 32-bit x8r8g8b8 (BT.601, the range the
revision gives), `pitch` bytes apart */
void bink_decoder_to_xrgb(struct bink_decoder const *bink, uint32_t *destination, long pitch, int height);

#endif
