/*
BINK_CHECK.C

Decodes a Bink movie with port/linux/src/bink_decoder.c, to compare with
FFmpeg's decoding of it (port/third_party/ffmpeg_bink/README.md):

	bink_check MOVIE.bik FRAMES.yuv AUDIO.f32

writes every frame as YUV 4:2:0 and the first audio track as interleaved
32-bit floats. Build it with the decoder:

	cc -Iport/linux/src port/tools/bink_check.c port/linux/src/bink_decoder.c -lm
*/

#include "bink_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static FILE *audio_out;
static void audio(void *ctx, float const *s, int n) { (void)ctx; fwrite(s, sizeof(float), (size_t)n, audio_out); }
int main(int argc, char **argv)
{
	char error[256];
	FILE *f, *yuv;
	struct bink_decoder *b;
	uint32_t i;

	if (argc != 4)
	{
		fprintf(stderr, "usage: bink_check MOVIE.bik FRAMES.yuv AUDIO.f32\n");
		return 2;
	}
	f = fopen(argv[1], "rb");
	yuv = fopen(argv[2], "wb");
	audio_out = fopen(argv[3], "wb");
	if (!f || !yuv || !audio_out) { fprintf(stderr, "bink_check: cannot open the files\n"); return 2; }
	b = bink_decoder_open(f, error, sizeof(error));
	if (!b) { printf("open: %s\n", error); return 1; }
	printf("%dx%d frames %u audio %d x%d\n", bink_decoder_width(b), bink_decoder_height(b), bink_decoder_frame_count(b), bink_decoder_audio_rate(b), bink_decoder_audio_channels(b));
	for (i = 0; i < bink_decoder_frame_count(b); i++)
	{
		uint8_t const *p[3]; int pitch[3], plane, y;
		if (bink_decoder_decode(b, i, audio, NULL) < 0) { printf("frame %u failed\n", i); return 1; }
		bink_decoder_planes(b, p, pitch);
		for (plane = 0; plane < 3; plane++)
			for (y = 0; y < (plane ? bink_decoder_height(b) / 2 : bink_decoder_height(b)); y++)
				fwrite(p[plane] + y * pitch[plane], 1, (size_t)(plane ? bink_decoder_width(b) / 2 : bink_decoder_width(b)), yuv);
	}
	printf("ok\n");
	return 0;
}
