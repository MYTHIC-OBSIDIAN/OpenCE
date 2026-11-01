/*
BINK_DECODER.C

Bink 1 movies (bink_decoder.h): the demuxer, the video decoder of revisions
f to k and the audio decoder (its RDFT and DCT kinds), adapted from FFmpeg
6.0's libavformat/bink.c, libavcodec/bink.c, binkdsp.c and binkaudio.c:

 * Bink demuxer
 * Copyright (c) 2008-2010 Peter Ross (pross@xvid.org)
 * Copyright (c) 2009 Daniel Verkamp (daniel@drv.nu)
 *
 * Bink video decoder
 * Copyright (c) 2009 Konstantin Shishkov
 * Copyright (C) 2011 Peter Ross <pross@xvid.org>
 *
 * Bink DSP routines
 * Copyright (c) 2009 Konstantin Shishkov
 *
 * Bink Audio decoder
 * Copyright (c) 2007-2011 Peter Ross (pross@xvid.org)
 * Copyright (c) 2009 Daniel Verkamp (daniel@drv.nu)
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version (port/third_party/ffmpeg_bink/COPYING.LGPLv2.1).
 * It is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.

Changed from FFmpeg's (2026, for OpenCE): libavcodec's frames, packets,
bit reader, VLC tables and transforms replaced by this file's own; the old
revision b's decoder left out; frames read from a FILE by the index the
header gives; the first audio track alone decoded, to floats; a frame's
YUV converted to x8r8g8b8. The decoding itself is FFmpeg's, step for step.
*/

#include "bink_decoder.h"
#include "../../third_party/ffmpeg_bink/binkdata.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

#define BINK_MAX_WIDTH 7680
#define BINK_MAX_HEIGHT 4800
#define BINK_MAX_FRAMES 1000000
#define BINK_MAX_AUDIO_TRACKS 256
#define BINK_FLAG_ALPHA 0x00100000

#define BINK_AUDIO_STEREO 0x2000
#define BINK_AUDIO_USE_DCT 0x1000

/* the planes' margins: blocks reach to the 16 pixels past an odd width or
height, and a 16x16 block's right half one block further */
#define PLANE_MARGIN 32

#define AUDIO_MAX_FRAME_LENGTH 4096

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum
{
	BINK_SRC_BLOCK_TYPES = 0,
	BINK_SRC_SUB_BLOCK_TYPES,
	BINK_SRC_COLORS,
	BINK_SRC_PATTERN,
	BINK_SRC_X_OFF,
	BINK_SRC_Y_OFF,
	BINK_SRC_INTRA_DC,
	BINK_SRC_INTER_DC,
	BINK_SRC_RUN,
	BINK_NB_SRC
};

enum
{
	SKIP_BLOCK = 0,
	SCALED_BLOCK,
	MOTION_BLOCK,
	RUN_BLOCK,
	RESIDUE_BLOCK,
	INTRA_BLOCK,
	FILL_BLOCK,
	INTER_BLOCK,
	PATTERN_BLOCK,
	RAW_BLOCK,
};

static const uint8_t bink_rlelens[4] = { 4, 8, 12, 32 };
static const uint8_t rle_length_tab[16] = { 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64 };
static const uint16_t wma_critical_freqs[25] =
{
	100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400,
	7700, 9500, 12000, 15500, 24500,
};

/* ---------- structures */

/* a little-endian bit reader over a buffer (FFmpeg's BITSTREAM_READER_LE);
reads past the end give zeros */
struct bits
{
	uint8_t const *data;
	uint32_t size_in_bits;
	uint32_t index;
};

struct vlc
{
	int bits;
	/* for each value of the next `bits` bits: the symbol and its length */
	uint8_t symbol[1 << 8];
	uint8_t length[1 << 8];
};

struct tree
{
	int vlc_num;
	uint8_t syms[16];
};

struct bundle
{
	int len;
	struct tree tree;
	uint8_t *data;
	uint8_t *data_end;
	uint8_t *cur_dec;
	uint8_t *cur_ptr;
};

struct plane
{
	uint8_t *base;
	uint8_t *data;
	int pitch;
};

struct frame
{
	struct plane planes[4];
};

struct bink_decoder
{
	FILE *file;
	uint32_t file_size;
	uint32_t frame_count;
	uint32_t fps_numerator;
	uint32_t fps_denominator;
	int width;
	int height;
	int version;
	int has_alpha;
	int swap_planes;
	uint32_t *frame_offsets;
	uint32_t audio_tracks;
	uint32_t next_frame;

	/* video */
	struct frame frames[2];
	int current;
	int have_last;
	struct bundle bundle[BINK_NB_SRC];
	uint8_t *bundle_memory;
	struct tree col_high[16];
	int col_lastval;
	uint8_t *packet;
	uint32_t packet_size;

	/* audio (the first track) */
	int audio_rate;
	int audio_channels;
	int audio_use_dct;
	int audio_frame_len;
	int audio_overlap_len;
	int audio_block_size;
	int audio_num_bands;
	int audio_first;
	int audio_coded_channels;
	float audio_root;
	unsigned int audio_bands[26];
	float audio_previous[2][AUDIO_MAX_FRAME_LENGTH / 16];
	float audio_quant_table[96];
	float *audio_out;
	float *audio_coeffs;
	float *audio_work;
	float *audio_twiddles;
};

/* ---------- globals */

static struct vlc bink_trees[16];
static int bink_trees_made = 0;

/* ---------- the bit reader */

static void bits_init(struct bits *bits, uint8_t const *data, uint32_t size)
{
	bits->data = data;
	bits->size_in_bits = size * 8;
	bits->index = 0;
}

static int bits_left(struct bits const *bits)
{
	return (int)bits->size_in_bits - (int)bits->index;
}

static uint32_t bits_peek(struct bits const *bits, int count)
{
	uint32_t value = 0;
	int shift = 0;
	uint32_t index = bits->index;

	while (shift < count)
	{
		uint32_t byte_index = index >> 3;
		int bit = index & 7;
		int take = 8 - bit < count - shift ? 8 - bit : count - shift;
		uint32_t byte = byte_index < (bits->size_in_bits >> 3) ? bits->data[byte_index] : 0;

		value |= ((byte >> bit) & ((1u << take) - 1)) << shift;
		shift += take;
		index += (uint32_t)take;
	}
	return value;
}

static void bits_skip(struct bits *bits, int count)
{
	bits->index += (uint32_t)count;
}

static uint32_t get_bits(struct bits *bits, int count)
{
	uint32_t value;

	if (count <= 0)
		return 0;
	value = bits_peek(bits, count);
	bits->index += (uint32_t)count;
	return value;
}

static int get_bits1(struct bits *bits)
{
	return (int)get_bits(bits, 1);
}

static int get_vlc(struct bits *bits, struct vlc const *vlc)
{
	uint32_t peek = bits_peek(bits, vlc->bits);

	bits_skip(bits, vlc->length[peek]);
	return vlc->symbol[peek];
}

#define GET_HUFF(bits, tree) ((tree).syms[get_vlc((bits), &bink_trees[(tree).vlc_num])])

static int log2_floor(unsigned value)
{
	int result = 0;

	while (value >>= 1)
		result++;
	return result;
}

/* ---------- the video decoder (libavcodec/bink.c) */

static void bink_trees_make(void)
{
	int tree;

	if (bink_trees_made)
		return;
	for (tree = 0; tree < 16; tree++)
	{
		struct vlc *vlc = &bink_trees[tree];
		int maxbits = bink_tree_lens[tree][15];
		int symbol;

		vlc->bits = maxbits;
		for (symbol = 0; symbol < 16; symbol++)
		{
			int length = bink_tree_lens[tree][symbol];
			unsigned code = bink_tree_bits[tree][symbol];
			unsigned index;

			/* (little-endian codes: those whose low `length` bits are it) */
			for (index = 0; index < (1u << maxbits); index++)
			{
				if ((index & ((1u << length) - 1)) == code)
				{
					vlc->symbol[index] = (uint8_t)symbol;
					vlc->length[index] = (uint8_t)length;
				}
			}
		}
	}
	bink_trees_made = 1;
}

static void init_lengths(struct bink_decoder *c, int width, int bw)
{
	width = (width + 7) & ~7;
	c->bundle[BINK_SRC_BLOCK_TYPES].len = log2_floor((unsigned)(width >> 3) + 511) + 1;
	c->bundle[BINK_SRC_SUB_BLOCK_TYPES].len = log2_floor((unsigned)(width >> 4) + 511) + 1;
	c->bundle[BINK_SRC_COLORS].len = log2_floor((unsigned)bw * 64 + 511) + 1;
	c->bundle[BINK_SRC_INTRA_DC].len =
	c->bundle[BINK_SRC_INTER_DC].len =
	c->bundle[BINK_SRC_X_OFF].len =
	c->bundle[BINK_SRC_Y_OFF].len = log2_floor((unsigned)(width >> 3) + 511) + 1;
	c->bundle[BINK_SRC_PATTERN].len = log2_floor((unsigned)(bw << 3) + 511) + 1;
	c->bundle[BINK_SRC_RUN].len = log2_floor((unsigned)bw * 48 + 511) + 1;
}

static int init_bundles(struct bink_decoder *c)
{
	int bw = (c->width + 7) >> 3;
	int bh = (c->height + 7) >> 3;
	size_t blocks = (size_t)bw * bh;
	uint8_t *memory = calloc(blocks, 64 * BINK_NB_SRC);
	int index;

	if (!memory)
		return -1;
	c->bundle_memory = memory;
	for (index = 0; index < BINK_NB_SRC; index++)
	{
		c->bundle[index].data = memory;
		memory += blocks * 64;
		c->bundle[index].data_end = memory;
	}
	return 0;
}

static void merge(struct bits *gb, uint8_t *dst, uint8_t *src, int size)
{
	uint8_t *src2 = src + size;
	int size2 = size;

	do
	{
		if (!get_bits1(gb))
		{
			*dst++ = *src++;
			size--;
		}
		else
		{
			*dst++ = *src2++;
			size2--;
		}
	} while (size && size2);
	while (size--)
		*dst++ = *src++;
	while (size2--)
		*dst++ = *src2++;
}

static int read_tree(struct bits *gb, struct tree *tree)
{
	uint8_t tmp1[16] = { 0 }, tmp2[16], *in = tmp1, *out = tmp2, *swap;
	int i, t, len;

	if (bits_left(gb) < 4)
		return -1;
	tree->vlc_num = (int)get_bits(gb, 4);
	if (!tree->vlc_num)
	{
		for (i = 0; i < 16; i++)
			tree->syms[i] = (uint8_t)i;
		return 0;
	}
	if (get_bits1(gb))
	{
		len = (int)get_bits(gb, 3);
		for (i = 0; i <= len; i++)
		{
			tree->syms[i] = (uint8_t)get_bits(gb, 4);
			tmp1[tree->syms[i]] = 1;
		}
		for (i = 0; i < 16 && len < 16 - 1; i++)
			if (!tmp1[i])
				tree->syms[++len] = (uint8_t)i;
	}
	else
	{
		len = (int)get_bits(gb, 2);
		for (i = 0; i < 16; i++)
			in[i] = (uint8_t)i;
		for (i = 0; i <= len; i++)
		{
			int size = 1 << i;

			for (t = 0; t < 16; t += size << 1)
				merge(gb, out + t, in + t, size);
			swap = in;
			in = out;
			out = swap;
		}
		memcpy(tree->syms, in, 16);
	}
	return 0;
}

static int read_bundle(struct bits *gb, struct bink_decoder *c, int bundle_num)
{
	int i;

	if (bundle_num == BINK_SRC_COLORS)
	{
		for (i = 0; i < 16; i++)
		{
			if (read_tree(gb, &c->col_high[i]) < 0)
				return -1;
		}
		c->col_lastval = 0;
	}
	if (bundle_num != BINK_SRC_INTRA_DC && bundle_num != BINK_SRC_INTER_DC)
	{
		if (read_tree(gb, &c->bundle[bundle_num].tree) < 0)
			return -1;
	}
	c->bundle[bundle_num].cur_dec = c->bundle[bundle_num].cur_ptr = c->bundle[bundle_num].data;
	return 0;
}

#define CHECK_READ_VAL(gb, b, t) \
	if (!(b)->cur_dec || ((b)->cur_dec > (b)->cur_ptr)) \
		return 0; \
	t = (int)get_bits(gb, (b)->len); \
	if (!t) \
	{ \
		(b)->cur_dec = NULL; \
		return 0; \
	}

static int read_runs(struct bits *gb, struct bundle *b)
{
	int t, v;
	uint8_t const *dec_end;

	CHECK_READ_VAL(gb, b, t);
	dec_end = b->cur_dec + t;
	if (dec_end > b->data_end || bits_left(gb) < 1)
		return -1;
	if (get_bits1(gb))
	{
		v = (int)get_bits(gb, 4);
		memset(b->cur_dec, v, (size_t)t);
		b->cur_dec += t;
	}
	else
	{
		while (b->cur_dec < dec_end)
			*b->cur_dec++ = GET_HUFF(gb, b->tree);
	}
	return 0;
}

static int read_motion_values(struct bits *gb, struct bundle *b)
{
	int t, sign, v;
	uint8_t const *dec_end;

	CHECK_READ_VAL(gb, b, t);
	dec_end = b->cur_dec + t;
	if (dec_end > b->data_end || bits_left(gb) < 1)
		return -1;
	if (get_bits1(gb))
	{
		v = (int)get_bits(gb, 4);
		if (v)
		{
			sign = -get_bits1(gb);
			v = (v ^ sign) - sign;
		}
		memset(b->cur_dec, v, (size_t)t);
		b->cur_dec += t;
	}
	else
	{
		while (b->cur_dec < dec_end)
		{
			v = GET_HUFF(gb, b->tree);
			if (v)
			{
				sign = -get_bits1(gb);
				v = (v ^ sign) - sign;
			}
			*b->cur_dec++ = (uint8_t)v;
		}
	}
	return 0;
}

static int read_block_types(struct bink_decoder *c, struct bits *gb, struct bundle *b)
{
	int t, v;
	int last = 0;
	uint8_t const *dec_end;

	CHECK_READ_VAL(gb, b, t);
	if (c->version == 'k')
	{
		t ^= 0xBB;
		if (t == 0)
		{
			b->cur_dec = NULL;
			return 0;
		}
	}
	dec_end = b->cur_dec + t;
	if (dec_end > b->data_end || bits_left(gb) < 1)
		return -1;
	if (get_bits1(gb))
	{
		v = (int)get_bits(gb, 4);
		memset(b->cur_dec, v, (size_t)t);
		b->cur_dec += t;
	}
	else
	{
		while (b->cur_dec < dec_end)
		{
			v = GET_HUFF(gb, b->tree);
			if (v < 12)
			{
				last = v;
				*b->cur_dec++ = (uint8_t)v;
			}
			else
			{
				int run = bink_rlelens[v - 12];

				if (dec_end - b->cur_dec < run)
					return -1;
				memset(b->cur_dec, last, (size_t)run);
				b->cur_dec += run;
			}
		}
	}
	return 0;
}

static int read_patterns(struct bits *gb, struct bundle *b)
{
	int t, v;
	uint8_t const *dec_end;

	CHECK_READ_VAL(gb, b, t);
	dec_end = b->cur_dec + t;
	if (dec_end > b->data_end)
		return -1;
	while (b->cur_dec < dec_end)
	{
		if (bits_left(gb) < 2)
			return -1;
		v = GET_HUFF(gb, b->tree);
		v |= GET_HUFF(gb, b->tree) << 4;
		*b->cur_dec++ = (uint8_t)v;
	}
	return 0;
}

static int read_colors(struct bits *gb, struct bundle *b, struct bink_decoder *c)
{
	int t, sign, v;
	uint8_t const *dec_end;

	CHECK_READ_VAL(gb, b, t);
	dec_end = b->cur_dec + t;
	if (dec_end > b->data_end || bits_left(gb) < 1)
		return -1;
	if (get_bits1(gb))
	{
		c->col_lastval = GET_HUFF(gb, c->col_high[c->col_lastval]);
		v = GET_HUFF(gb, b->tree);
		v = (c->col_lastval << 4) | v;
		if (c->version < 'i')
		{
			sign = ((int8_t)v) >> 7;
			v = ((v & 0x7F) ^ sign) - sign;
			v += 0x80;
		}
		memset(b->cur_dec, v, (size_t)t);
		b->cur_dec += t;
	}
	else
	{
		while (b->cur_dec < dec_end)
		{
			if (bits_left(gb) < 2)
				return -1;
			c->col_lastval = GET_HUFF(gb, c->col_high[c->col_lastval]);
			v = GET_HUFF(gb, b->tree);
			v = (c->col_lastval << 4) | v;
			if (c->version < 'i')
			{
				sign = ((int8_t)v) >> 7;
				v = ((v & 0x7F) ^ sign) - sign;
				v += 0x80;
			}
			*b->cur_dec++ = (uint8_t)v;
		}
	}
	return 0;
}

#define DC_START_BITS 11

static int read_dcs(struct bits *gb, struct bundle *b, int start_bits, int has_sign)
{
	int i, j, len, len2, bsize, sign, v, v2;
	int16_t *dst = (int16_t *)b->cur_dec;
	int16_t *dst_end = (int16_t *)b->data_end;

	CHECK_READ_VAL(gb, b, len);
	if (bits_left(gb) < start_bits - has_sign)
		return -1;
	v = (int)get_bits(gb, start_bits - has_sign);
	if (v && has_sign)
	{
		sign = -get_bits1(gb);
		v = (v ^ sign) - sign;
	}
	if (dst_end - dst < 1)
		return -1;
	*dst++ = (int16_t)v;
	len--;
	for (i = 0; i < len; i += 8)
	{
		len2 = len - i < 8 ? len - i : 8;
		if (dst_end - dst < len2)
			return -1;
		bsize = (int)get_bits(gb, 4);
		if (bsize)
		{
			for (j = 0; j < len2; j++)
			{
				v2 = (int)get_bits(gb, bsize);
				if (v2)
				{
					sign = -get_bits1(gb);
					v2 = (v2 ^ sign) - sign;
				}
				v += v2;
				*dst++ = (int16_t)v;
				if (v < -32768 || v > 32767)
					return -1;
			}
		}
		else
		{
			for (j = 0; j < len2; j++)
				*dst++ = (int16_t)v;
		}
	}
	b->cur_dec = (uint8_t *)dst;
	return 0;
}

static int get_value(struct bink_decoder *c, int bundle)
{
	int16_t value;

	if (bundle < BINK_SRC_X_OFF || bundle == BINK_SRC_RUN)
		return *c->bundle[bundle].cur_ptr++;
	if (bundle == BINK_SRC_X_OFF || bundle == BINK_SRC_Y_OFF)
		return (int8_t)*c->bundle[bundle].cur_ptr++;
	memcpy(&value, c->bundle[bundle].cur_ptr, sizeof(value));
	c->bundle[bundle].cur_ptr += 2;
	return value;
}

static int read_dct_coeffs(struct bits *gb, int32_t block[64], uint8_t const *scan, int *coef_count_,
	int coef_idx[64], int q)
{
	int coef_list[128];
	int mode_list[128];
	int i, t, bits, ccoef, mode, sign;
	int list_start = 64, list_end = 64, list_pos;
	int coef_count = 0;
	int quant_idx;

	if (bits_left(gb) < 4)
		return -1;
	coef_list[list_end] = 4; mode_list[list_end++] = 0;
	coef_list[list_end] = 24; mode_list[list_end++] = 0;
	coef_list[list_end] = 44; mode_list[list_end++] = 0;
	coef_list[list_end] = 1; mode_list[list_end++] = 3;
	coef_list[list_end] = 2; mode_list[list_end++] = 3;
	coef_list[list_end] = 3; mode_list[list_end++] = 3;

	for (bits = (int)get_bits(gb, 4) - 1; bits >= 0; bits--)
	{
		list_pos = list_start;
		while (list_pos < list_end)
		{
			if (!(mode_list[list_pos] | coef_list[list_pos]) || !get_bits1(gb))
			{
				list_pos++;
				continue;
			}
			ccoef = coef_list[list_pos];
			mode = mode_list[list_pos];
			switch (mode)
			{
			case 0:
				coef_list[list_pos] = ccoef + 4;
				mode_list[list_pos] = 1;
				/* fall through */
			case 2:
				if (mode == 2)
				{
					coef_list[list_pos] = 0;
					mode_list[list_pos++] = 0;
				}
				for (i = 0; i < 4; i++, ccoef++)
				{
					if (get_bits1(gb))
					{
						if (list_start <= 0)
							return -1;
						coef_list[--list_start] = ccoef;
						mode_list[list_start] = 3;
					}
					else
					{
						if (!bits)
						{
							t = 1 - (get_bits1(gb) << 1);
						}
						else
						{
							t = (int)get_bits(gb, bits) | 1 << bits;
							sign = -get_bits1(gb);
							t = (t ^ sign) - sign;
						}
						if (ccoef > 63 || coef_count >= 64)
							return -1;
						block[scan[ccoef]] = t;
						coef_idx[coef_count++] = ccoef;
					}
				}
				break;
			case 1:
				mode_list[list_pos] = 2;
				for (i = 0; i < 3; i++)
				{
					ccoef += 4;
					if (list_end >= 128)
						return -1;
					coef_list[list_end] = ccoef;
					mode_list[list_end++] = 2;
				}
				break;
			case 3:
				if (!bits)
				{
					t = 1 - (get_bits1(gb) << 1);
				}
				else
				{
					t = (int)get_bits(gb, bits) | 1 << bits;
					sign = -get_bits1(gb);
					t = (t ^ sign) - sign;
				}
				if (ccoef > 63 || coef_count >= 64)
					return -1;
				block[scan[ccoef]] = t;
				coef_idx[coef_count++] = ccoef;
				coef_list[list_pos] = 0;
				mode_list[list_pos++] = 0;
				break;
			}
		}
	}

	if (q == -1)
	{
		quant_idx = (int)get_bits(gb, 4);
	}
	else
	{
		quant_idx = q;
		if ((unsigned)quant_idx > 15u)
			return -1;
	}
	*coef_count_ = coef_count;
	return quant_idx;
}

static void unquantize_dct_coeffs(int32_t block[64], int32_t const quant[64], int coef_count, int coef_idx[64],
	uint8_t const *scan)
{
	int i;

	block[0] = (int)(block[0] * (uint32_t)quant[0]) >> 11;
	for (i = 0; i < coef_count; i++)
	{
		int idx = coef_idx[i];

		block[scan[idx]] = (int)(block[scan[idx]] * (uint32_t)quant[idx]) >> 11;
	}
}

static int read_residue(struct bits *gb, int16_t block[64], int masks_count)
{
	int coef_list[128];
	int mode_list[128];
	int i, sign, mask, ccoef, mode;
	int list_start = 64, list_end = 64, list_pos;
	int nz_coeff[64];
	int nz_coeff_count = 0;

	coef_list[list_end] = 4; mode_list[list_end++] = 0;
	coef_list[list_end] = 24; mode_list[list_end++] = 0;
	coef_list[list_end] = 44; mode_list[list_end++] = 0;
	coef_list[list_end] = 0; mode_list[list_end++] = 2;

	for (mask = 1 << get_bits(gb, 3); mask; mask >>= 1)
	{
		for (i = 0; i < nz_coeff_count; i++)
		{
			if (!get_bits1(gb))
				continue;
			if (block[nz_coeff[i]] < 0)
				block[nz_coeff[i]] -= (int16_t)mask;
			else
				block[nz_coeff[i]] += (int16_t)mask;
			masks_count--;
			if (masks_count < 0)
				return 0;
		}
		list_pos = list_start;
		while (list_pos < list_end)
		{
			if (!(coef_list[list_pos] | mode_list[list_pos]) || !get_bits1(gb))
			{
				list_pos++;
				continue;
			}
			ccoef = coef_list[list_pos];
			mode = mode_list[list_pos];
			switch (mode)
			{
			case 0:
				coef_list[list_pos] = ccoef + 4;
				mode_list[list_pos] = 1;
				/* fall through */
			case 2:
				if (mode == 2)
				{
					coef_list[list_pos] = 0;
					mode_list[list_pos++] = 0;
				}
				for (i = 0; i < 4; i++, ccoef++)
				{
					if (get_bits1(gb))
					{
						if (list_start <= 0)
							return -1;
						coef_list[--list_start] = ccoef;
						mode_list[list_start] = 3;
					}
					else
					{
						if (ccoef > 63 || nz_coeff_count >= 64)
							return -1;
						nz_coeff[nz_coeff_count++] = bink_scan[ccoef];
						sign = -get_bits1(gb);
						block[bink_scan[ccoef]] = (int16_t)((mask ^ sign) - sign);
						masks_count--;
						if (masks_count < 0)
							return 0;
					}
				}
				break;
			case 1:
				mode_list[list_pos] = 2;
				for (i = 0; i < 3; i++)
				{
					ccoef += 4;
					if (list_end >= 128)
						return -1;
					coef_list[list_end] = ccoef;
					mode_list[list_end++] = 2;
				}
				break;
			case 3:
				if (ccoef > 63 || nz_coeff_count >= 64)
					return -1;
				nz_coeff[nz_coeff_count++] = bink_scan[ccoef];
				sign = -get_bits1(gb);
				block[bink_scan[ccoef]] = (int16_t)((mask ^ sign) - sign);
				coef_list[list_pos] = 0;
				mode_list[list_pos++] = 0;
				masks_count--;
				if (masks_count < 0)
					return 0;
				break;
			}
		}
	}
	return 0;
}

/* ---------- the DSP (libavcodec/binkdsp.c, blockdsp, hpeldsp) */

#define A1 2896
#define A2 2217
#define A3 3784
#define A4 -5352

#define MUL(X, Y) ((int)((unsigned)(X) * (Y)) >> 11)

#define IDCT_TRANSFORM(dest, s0, s1, s2, s3, s4, s5, s6, s7, d0, d1, d2, d3, d4, d5, d6, d7, munge, src) \
	{ \
		const int a0 = (src)[s0] + (src)[s4]; \
		const int a1 = (src)[s0] - (src)[s4]; \
		const int a2 = (src)[s2] + (src)[s6]; \
		const int a3 = MUL(A1, (src)[s2] - (src)[s6]); \
		const int a4 = (src)[s5] + (src)[s3]; \
		const int a5 = (src)[s5] - (src)[s3]; \
		const int a6 = (src)[s1] + (src)[s7]; \
		const int a7 = (src)[s1] - (src)[s7]; \
		const int b0 = a4 + a6; \
		const int b1 = MUL(A3, a5 + a7); \
		const int b2 = MUL(A4, a5) - b0 + b1; \
		const int b3 = MUL(A1, a6 - a4) - b2; \
		const int b4 = MUL(A2, a7) + b3 - b1; \
		(dest)[d0] = munge(a0 + a2 + b0); \
		(dest)[d1] = munge(a1 + a3 - a2 + b2); \
		(dest)[d2] = munge(a1 - a3 + a2 + b3); \
		(dest)[d3] = munge(a0 - a2 - b4); \
		(dest)[d4] = munge(a0 - a2 + b4); \
		(dest)[d5] = munge(a1 - a3 + a2 - b3); \
		(dest)[d6] = munge(a1 + a3 - a2 - b2); \
		(dest)[d7] = munge(a0 + a2 - b0); \
	}

#define MUNGE_NONE(x) (x)
#define IDCT_COL(dest, src) IDCT_TRANSFORM(dest, 0, 8, 16, 24, 32, 40, 48, 56, 0, 8, 16, 24, 32, 40, 48, 56, MUNGE_NONE, src)
#define MUNGE_ROW(x) (((x) + 0x7F) >> 8)
#define IDCT_ROW(dest, src) IDCT_TRANSFORM(dest, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, MUNGE_ROW, src)

static void bink_idct_col(int *dest, int32_t const *src)
{
	if ((src[8] | src[16] | src[24] | src[32] | src[40] | src[48] | src[56]) == 0)
	{
		dest[0] = dest[8] = dest[16] = dest[24] = dest[32] = dest[40] = dest[48] = dest[56] = src[0];
	}
	else
	{
		IDCT_COL(dest, src);
	}
}

static void bink_idct(int32_t *block)
{
	int i;
	int temp[64];

	for (i = 0; i < 8; i++)
		bink_idct_col(&temp[i], &block[i]);
	for (i = 0; i < 8; i++)
	{
		IDCT_ROW((&block[8 * i]), (&temp[8 * i]));
	}
}

static void bink_idct_add(uint8_t *dest, int linesize, int32_t *block)
{
	int i, j;

	bink_idct(block);
	for (i = 0; i < 8; i++, dest += linesize, block += 8)
		for (j = 0; j < 8; j++)
			dest[j] = (uint8_t)(dest[j] + block[j]);
}

static void bink_idct_put(uint8_t *dest, int linesize, int32_t *block)
{
	int i;
	int temp[64];

	for (i = 0; i < 8; i++)
		bink_idct_col(&temp[i], &block[i]);
	for (i = 0; i < 8; i++)
	{
		uint8_t *row = &dest[i * linesize];
		int out[8];
		int j;

		IDCT_ROW(out, (&temp[8 * i]));
		/* (FFmpeg stores to uint8_t: the low 8 bits) */
		for (j = 0; j < 8; j++)
			row[j] = (uint8_t)out[j];
	}
}

static void scale_block(uint8_t const src[64], uint8_t *dst, int linesize)
{
	int i, j;

	for (j = 0; j < 8; j++)
	{
		uint8_t *row1 = dst + 2 * j * linesize;
		uint8_t *row2 = row1 + linesize;

		for (i = 0; i < 8; i++)
			row1[2 * i] = row1[2 * i + 1] = row2[2 * i] = row2[2 * i + 1] = src[i];
		src += 8;
	}
}

static void add_pixels8(uint8_t *pixels, int16_t *block, int line_size)
{
	int i, j;

	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 8; j++)
			pixels[j] = (uint8_t)(pixels[j] + block[j]);
		pixels += line_size;
		block += 8;
	}
}

static void put_pixels8(uint8_t *dst, uint8_t const *src, int stride)
{
	int i;

	for (i = 0; i < 8; i++)
		memmove(dst + i * stride, src + i * stride, 8);
}

static void fill_block(uint8_t *dst, int value, int stride, int size)
{
	int i;

	for (i = 0; i < size; i++)
		memset(dst + i * stride, value, (size_t)size);
}

/* ---------- a plane */

static int bink_put_pixels(struct bink_decoder *c, uint8_t *dst, uint8_t *prev, int stride, uint8_t *ref_start,
	uint8_t *ref_end)
{
	int xoff = get_value(c, BINK_SRC_X_OFF);
	int yoff = get_value(c, BINK_SRC_Y_OFF);
	uint8_t *ref = prev + xoff + yoff * stride;

	if (ref < ref_start || ref > ref_end)
		return -1;
	put_pixels8(dst, ref, stride);
	return 0;
}

static int bink_decode_plane(struct bink_decoder *c, struct frame *frame, struct frame *last, struct bits *gb,
	int plane_idx, int is_chroma)
{
	int blk, ret;
	int i, j, bx, by;
	uint8_t *dst, *prev, *ref_start, *ref_end, *prev_base;
	int v, col[2];
	uint8_t const *scan;
	int16_t block[64];
	uint8_t ublock[64];
	int32_t dctblock[64];
	int coordmap[64], quant_idx, coef_count, coef_idx[64];
	int const stride = frame->planes[plane_idx].pitch;
	int bw = is_chroma ? (c->width + 15) >> 4 : (c->width + 7) >> 3;
	int bh = is_chroma ? (c->height + 15) >> 4 : (c->height + 7) >> 3;
	int width = c->width >> is_chroma;
	int height = c->height >> is_chroma;

	if (c->version == 'k' && get_bits1(gb))
	{
		int fill = (int)get_bits(gb, 8);

		dst = frame->planes[plane_idx].data;
		for (i = 0; i < height; i++)
			memset(dst + i * stride, fill, (size_t)width);
		goto end;
	}

	init_lengths(c, width > 8 ? width : 8, bw);
	for (i = 0; i < BINK_NB_SRC; i++)
	{
		if (read_bundle(gb, c, i) < 0)
			return -1;
	}

	/* (the frame before's planes, else this one's: FFmpeg's c->last) */
	prev_base = c->have_last ? last->planes[plane_idx].data : frame->planes[plane_idx].data;
	ref_start = prev_base;
	ref_end = ref_start + (bw - 1 + stride * (bh - 1)) * 8;

	for (i = 0; i < 64; i++)
		coordmap[i] = (i & 7) + (i >> 3) * stride;

	for (by = 0; by < bh; by++)
	{
		if (read_block_types(c, gb, &c->bundle[BINK_SRC_BLOCK_TYPES]) < 0 ||
			read_block_types(c, gb, &c->bundle[BINK_SRC_SUB_BLOCK_TYPES]) < 0 ||
			read_colors(gb, &c->bundle[BINK_SRC_COLORS], c) < 0 ||
			read_patterns(gb, &c->bundle[BINK_SRC_PATTERN]) < 0 ||
			read_motion_values(gb, &c->bundle[BINK_SRC_X_OFF]) < 0 ||
			read_motion_values(gb, &c->bundle[BINK_SRC_Y_OFF]) < 0 ||
			read_dcs(gb, &c->bundle[BINK_SRC_INTRA_DC], DC_START_BITS, 0) < 0 ||
			read_dcs(gb, &c->bundle[BINK_SRC_INTER_DC], DC_START_BITS, 1) < 0 ||
			read_runs(gb, &c->bundle[BINK_SRC_RUN]) < 0)
		{
			return -1;
		}

		dst = frame->planes[plane_idx].data + 8 * by * stride;
		prev = prev_base + 8 * by * stride;
		for (bx = 0; bx < bw; bx++, dst += 8, prev += 8)
		{
			blk = get_value(c, BINK_SRC_BLOCK_TYPES);
			/* (a 16x16 block type on an odd line or column: part of the
			block decoded already) */
			if (((by & 1) || (bx & 1)) && blk == SCALED_BLOCK)
			{
				bx++;
				dst += 8;
				prev += 8;
				continue;
			}
			switch (blk)
			{
			case SKIP_BLOCK:
				put_pixels8(dst, prev, stride);
				break;
			case SCALED_BLOCK:
				blk = get_value(c, BINK_SRC_SUB_BLOCK_TYPES);
				switch (blk)
				{
				case RUN_BLOCK:
					if (bits_left(gb) < 4)
						return -1;
					scan = bink_patterns[get_bits(gb, 4)];
					i = 0;
					do
					{
						int run = get_value(c, BINK_SRC_RUN) + 1;

						i += run;
						if (i > 64)
							return -1;
						if (get_bits1(gb))
						{
							v = get_value(c, BINK_SRC_COLORS);
							for (j = 0; j < run; j++)
								ublock[*scan++] = (uint8_t)v;
						}
						else
						{
							for (j = 0; j < run; j++)
								ublock[*scan++] = (uint8_t)get_value(c, BINK_SRC_COLORS);
						}
					} while (i < 63);
					if (i == 63)
						ublock[*scan++] = (uint8_t)get_value(c, BINK_SRC_COLORS);
					break;
				case INTRA_BLOCK:
					memset(dctblock, 0, sizeof(dctblock));
					dctblock[0] = get_value(c, BINK_SRC_INTRA_DC);
					quant_idx = read_dct_coeffs(gb, dctblock, bink_scan, &coef_count, coef_idx, -1);
					if (quant_idx < 0)
						return -1;
					unquantize_dct_coeffs(dctblock, bink_intra_quant[quant_idx], coef_count, coef_idx, bink_scan);
					bink_idct_put(ublock, 8, dctblock);
					break;
				case FILL_BLOCK:
					v = get_value(c, BINK_SRC_COLORS);
					fill_block(dst, v, stride, 16);
					break;
				case PATTERN_BLOCK:
					for (i = 0; i < 2; i++)
						col[i] = get_value(c, BINK_SRC_COLORS);
					for (j = 0; j < 8; j++)
					{
						v = get_value(c, BINK_SRC_PATTERN);
						for (i = 0; i < 8; i++, v >>= 1)
							ublock[i + j * 8] = (uint8_t)col[v & 1];
					}
					break;
				case RAW_BLOCK:
					for (j = 0; j < 8; j++)
						for (i = 0; i < 8; i++)
							ublock[i + j * 8] = (uint8_t)get_value(c, BINK_SRC_COLORS);
					break;
				default:
					return -1;
				}
				if (blk != FILL_BLOCK)
					scale_block(ublock, dst, stride);
				bx++;
				dst += 8;
				prev += 8;
				break;
			case MOTION_BLOCK:
				if (bink_put_pixels(c, dst, prev, stride, ref_start, ref_end) < 0)
					return -1;
				break;
			case RUN_BLOCK:
				scan = bink_patterns[get_bits(gb, 4)];
				i = 0;
				do
				{
					int run = get_value(c, BINK_SRC_RUN) + 1;

					i += run;
					if (i > 64)
						return -1;
					if (get_bits1(gb))
					{
						v = get_value(c, BINK_SRC_COLORS);
						for (j = 0; j < run; j++)
							dst[coordmap[*scan++]] = (uint8_t)v;
					}
					else
					{
						for (j = 0; j < run; j++)
							dst[coordmap[*scan++]] = (uint8_t)get_value(c, BINK_SRC_COLORS);
					}
				} while (i < 63);
				if (i == 63)
					dst[coordmap[*scan++]] = (uint8_t)get_value(c, BINK_SRC_COLORS);
				break;
			case RESIDUE_BLOCK:
				if (bink_put_pixels(c, dst, prev, stride, ref_start, ref_end) < 0)
					return -1;
				memset(block, 0, sizeof(block));
				v = (int)get_bits(gb, 7);
				read_residue(gb, block, v);
				add_pixels8(dst, block, stride);
				break;
			case INTRA_BLOCK:
				memset(dctblock, 0, sizeof(dctblock));
				dctblock[0] = get_value(c, BINK_SRC_INTRA_DC);
				quant_idx = read_dct_coeffs(gb, dctblock, bink_scan, &coef_count, coef_idx, -1);
				if (quant_idx < 0)
					return -1;
				unquantize_dct_coeffs(dctblock, bink_intra_quant[quant_idx], coef_count, coef_idx, bink_scan);
				bink_idct_put(dst, stride, dctblock);
				break;
			case FILL_BLOCK:
				v = get_value(c, BINK_SRC_COLORS);
				fill_block(dst, v, stride, 8);
				break;
			case INTER_BLOCK:
				if (bink_put_pixels(c, dst, prev, stride, ref_start, ref_end) < 0)
					return -1;
				memset(dctblock, 0, sizeof(dctblock));
				dctblock[0] = get_value(c, BINK_SRC_INTER_DC);
				quant_idx = read_dct_coeffs(gb, dctblock, bink_scan, &coef_count, coef_idx, -1);
				if (quant_idx < 0)
					return -1;
				unquantize_dct_coeffs(dctblock, bink_inter_quant[quant_idx], coef_count, coef_idx, bink_scan);
				bink_idct_add(dst, stride, dctblock);
				break;
			case PATTERN_BLOCK:
				for (i = 0; i < 2; i++)
					col[i] = get_value(c, BINK_SRC_COLORS);
				for (i = 0; i < 8; i++)
				{
					v = get_value(c, BINK_SRC_PATTERN);
					for (j = 0; j < 8; j++, v >>= 1)
						dst[i * stride + j] = (uint8_t)col[v & 1];
				}
				break;
			case RAW_BLOCK:
				if (c->bundle[BINK_SRC_COLORS].data_end - c->bundle[BINK_SRC_COLORS].cur_ptr < 64)
					return -1;
				for (i = 0; i < 8; i++)
					memcpy(dst + i * stride, c->bundle[BINK_SRC_COLORS].cur_ptr + i * 8, 8);
				c->bundle[BINK_SRC_COLORS].cur_ptr += 64;
				break;
			default:
				return -1;
			}
		}
	}

end:
	/* (the next plane starts at a 32-bit boundary) */
	if (gb->index & 0x1F)
		bits_skip(gb, 32 - (int)(gb->index & 0x1F));
	(void)ret;
	return 0;
}

/* ---------- the audio decoder (libavcodec/binkaudio.c) */

/* a complex FFT of n points (a power of two), in place: X[k] = sum of
x[m] e^(-2 pi i k m / n) */
static void fft(float *re, float *im, int n)
{
	int i, j, length;

	for (i = 1, j = 0; i < n; i++)
	{
		int bit = n >> 1;
		float t;

		for (; j & bit; bit >>= 1)
			j ^= bit;
		j ^= bit;
		if (i < j)
		{
			t = re[i]; re[i] = re[j]; re[j] = t;
			t = im[i]; im[i] = im[j]; im[j] = t;
		}
	}
	for (length = 2; length <= n; length <<= 1)
	{
		double angle = -2.0 * M_PI / length;
		float wr = (float)cos(angle), wi = (float)sin(angle);

		for (i = 0; i < n; i += length)
		{
			double cr = 1.0, ci = 0.0;

			for (j = 0; j < length / 2; j++)
			{
				float ur = re[i + j], ui = im[i + j];
				float vr = (float)(re[i + j + length / 2] * cr - im[i + j + length / 2] * ci);
				float vi = (float)(re[i + j + length / 2] * ci + im[i + j + length / 2] * cr);
				double next;

				re[i + j] = ur + vr; im[i + j] = ui + vi;
				re[i + j + length / 2] = ur - vr; im[i + j + length / 2] = ui - vi;
				next = cr * wr - ci * wi;
				ci = cr * wi + ci * wr;
				cr = next;
			}
		}
	}
}

/* The inverse real DFT FFmpeg's av_tx RDFT gives binkaudio: the spectrum
`spectrum` (length / 2 + 1 bins, real and imaginary interleaved) to
`length` real samples, x[n] = scale * the sum over the whole Hermitian
spectrum of X[k] e^(+2 pi i k n / length). By one complex FFT of half the
length: z[m] = x[2m] + i x[2m+1] is the inverse transform of
Z[k] = E[k] + i e^(2 pi i k / length) O[k], where E[k] = X[k] + conj(X[N/2-k])
and O[k] = X[k] - conj(X[N/2-k]); the inverse is the conjugate of the
forward transform of the conjugate. */
static void inverse_rdft(struct bink_decoder *c, float *output, float const *spectrum, int length, float scale)
{
	int half = length / 2;
	float *re = c->audio_work;
	float *im = c->audio_work + half;
	int k, n;

	for (k = 0; k < half; k++)
	{
		float xr = spectrum[2 * k], xi = spectrum[2 * k + 1];
		float yr = spectrum[2 * (half - k)], yi = -spectrum[2 * (half - k) + 1];
		float er = xr + yr, ei = xi + yi;
		float orr = xr - yr, oi = xi - yi;
		float tr = c->audio_twiddles[2 * k], ti = c->audio_twiddles[2 * k + 1];
		/* i e^(2 pi i k / N) O[k] */
		float pr = -(orr * ti + oi * tr), pi = orr * tr - oi * ti;

		re[k] = er + pr;
		im[k] = -(ei + pi);
	}
	fft(re, im, half);
	for (n = 0; n < half; n++)
	{
		output[2 * n] = re[n] * scale;
		output[2 * n + 1] = -im[n] * scale;
	}
}

static float get_float(struct bits *gb)
{
	int power = (int)get_bits(gb, 5);
	float f = ldexpf((float)get_bits(gb, 23), power - 23);

	if (get_bits1(gb))
		f = -f;
	return f;
}

static int audio_init(struct bink_decoder *c)
{
	int sample_rate = c->audio_rate;
	int sample_rate_half;
	int frame_len_bits;
	int i;

	if (sample_rate < 22050)
		frame_len_bits = 9;
	else if (sample_rate < 44100)
		frame_len_bits = 10;
	else
		frame_len_bits = 11;
	if (!c->audio_use_dct)
	{
		/* (the RDFT kind's samples are interleaved already) */
		sample_rate *= c->audio_channels;
		c->audio_coded_channels = 1;
		frame_len_bits += log2_floor((unsigned)c->audio_channels);
	}
	else
	{
		c->audio_coded_channels = c->audio_channels;
	}
	c->audio_frame_len = 1 << frame_len_bits;
	if (c->audio_frame_len > AUDIO_MAX_FRAME_LENGTH)
		return -1;
	c->audio_overlap_len = c->audio_frame_len / 16;
	c->audio_block_size = (c->audio_frame_len - c->audio_overlap_len) * c->audio_coded_channels;
	sample_rate_half = (sample_rate + 1) / 2;
	if (!c->audio_use_dct)
		c->audio_root = (float)(2.0 / (sqrt((double)c->audio_frame_len) * 32768.0));
	else
		c->audio_root = (float)(c->audio_frame_len / (sqrt((double)c->audio_frame_len) * 32768.0));
	for (i = 0; i < 96; i++)
		c->audio_quant_table[i] = expf(i * 0.15289164787221953823f) * c->audio_root;
	for (c->audio_num_bands = 1; c->audio_num_bands < 25; c->audio_num_bands++)
		if (sample_rate_half <= wma_critical_freqs[c->audio_num_bands - 1])
			break;
	c->audio_bands[0] = 2;
	for (i = 1; i < c->audio_num_bands; i++)
		c->audio_bands[i] = ((unsigned)wma_critical_freqs[i - 1] * (unsigned)c->audio_frame_len / (unsigned)sample_rate_half) & ~1u;
	c->audio_bands[c->audio_num_bands] = (unsigned)c->audio_frame_len;
	c->audio_first = 1;
	c->audio_out = calloc((size_t)c->audio_frame_len * 2, sizeof(float));
	c->audio_coeffs = calloc((size_t)c->audio_frame_len + 4, sizeof(float));
	/* (the RDFT's half-length FFT, or the DCT's double-length one) */
	c->audio_work = calloc((size_t)c->audio_frame_len * 4, sizeof(float));
	c->audio_twiddles = calloc((size_t)c->audio_frame_len, sizeof(float));
	if (!c->audio_out || !c->audio_coeffs || !c->audio_work || !c->audio_twiddles)
		return -1;
	for (i = 0; i < c->audio_frame_len / 2; i++)
	{
		double angle = 2.0 * M_PI * i / c->audio_frame_len;

		c->audio_twiddles[2 * i] = (float)cos(angle);
		c->audio_twiddles[2 * i + 1] = (float)sin(angle);
	}
	return 0;
}

/* The DCT kind's transform, a DCT-III of the frame's length n:
output[m] = scale * (input[0] / 2 + the sum over k of input[k] cos(pi k (2m + 1) / 2n)),
the real part of a 2n-point inverse DFT of input[k] e^(i pi k / 2n) (by one
FFT: the inverse transform's real part is the forward transform's of the
conjugate) */
static void inverse_dct(struct bink_decoder *c, float *output, float const *input, int length, float scale)
{
	float *re = c->audio_work;
	float *im = c->audio_work + 2 * length;
	int k;

	for (k = 0; k < length; k++)
	{
		double angle = M_PI * k / (2.0 * length);
		double weight = k ? input[k] : input[0] * 0.5;

		re[k] = (float)(weight * cos(angle));
		im[k] = (float)(-weight * sin(angle));
		re[k + length] = 0.0f;
		im[k + length] = 0.0f;
	}
	fft(re, im, 2 * length);
	for (k = 0; k < length; k++)
		output[k] = re[k] * scale;
}

static int audio_decode_block(struct bink_decoder *c, struct bits *gb, float *out)
{
	int ch, i, j, k;
	float q, quant[25];
	int width, coeff;
	float *coeffs = c->audio_coeffs;
	int channels = c->audio_coded_channels < 2 ? c->audio_coded_channels : 2;

	if (c->audio_use_dct)
		bits_skip(gb, 2);
	for (ch = 0; ch < channels; ch++)
	{
		float *channel_out = out + ch * c->audio_frame_len;

		if (bits_left(gb) < 58)
			return -1;
		coeffs[0] = get_float(gb) * c->audio_root;
		coeffs[1] = get_float(gb) * c->audio_root;
		if (bits_left(gb) < c->audio_num_bands * 8)
			return -1;
		for (i = 0; i < c->audio_num_bands; i++)
		{
			int value = (int)get_bits(gb, 8);

			quant[i] = c->audio_quant_table[value < 95 ? value : 95];
		}
		k = 0;
		q = quant[0];
		i = 2;
		while (i < c->audio_frame_len)
		{
			int v = get_bits1(gb);

			if (v)
			{
				v = (int)get_bits(gb, 4);
				j = i + rle_length_tab[v] * 8;
			}
			else
			{
				j = i + 8;
			}
			j = j < c->audio_frame_len ? j : c->audio_frame_len;
			width = (int)get_bits(gb, 4);
			if (width == 0)
			{
				memset(coeffs + i, 0, (size_t)(j - i) * sizeof(*coeffs));
				i = j;
				while (c->audio_bands[k] < (unsigned)i)
					q = quant[k++];
			}
			else
			{
				while (i < j)
				{
					if (c->audio_bands[k] == (unsigned)i)
						q = quant[k++];
					coeff = (int)get_bits(gb, width);
					if (coeff)
						coeffs[i] = get_bits1(gb) ? -q * coeff : q * coeff;
					else
						coeffs[i] = 0.0f;
					i++;
				}
			}
			if (bits_left(gb) < 0)
				return -1;
		}
		if (c->audio_use_dct)
		{
			coeffs[0] /= 0.5f;
			/* (FFmpeg's av_tx DCT-III of half the length, scaled by one
			over the length: matched sample for sample) */
			inverse_dct(c, channel_out, coeffs, c->audio_frame_len, 2.0f / c->audio_frame_len);
		}
		else
		{
			for (i = 2; i < c->audio_frame_len; i += 2)
				coeffs[i + 1] *= -1;
			coeffs[c->audio_frame_len + 0] = coeffs[1];
			coeffs[c->audio_frame_len + 1] = coeffs[1] = 0;
			inverse_rdft(c, channel_out, coeffs, c->audio_frame_len, 0.5f);
		}
	}
	for (ch = 0; ch < channels; ch++)
	{
		float *channel_out = out + ch * c->audio_frame_len;
		int count = c->audio_overlap_len * channels;

		if (!c->audio_first)
		{
			j = ch;
			for (i = 0; i < c->audio_overlap_len; i++, j += channels)
				channel_out[i] = (c->audio_previous[ch][i] * (count - j) + channel_out[i] * j) / count;
		}
		memcpy(c->audio_previous[ch], &channel_out[c->audio_frame_len - c->audio_overlap_len],
			(size_t)c->audio_overlap_len * sizeof(float));
	}
	c->audio_first = 0;
	return 0;
}

/* an audio packet's blocks, to `audio` as interleaved floats */
static void audio_decode_packet(struct bink_decoder *c, uint8_t const *data, uint32_t size, bink_audio_proc audio,
	void *context)
{
	struct bits gb;

	if (size < 4 || !c->audio_rate)
		return;
	bits_init(&gb, data, size);
	bits_skip(&gb, 32);
	while (bits_left(&gb) > 0)
	{
		int samples_per_channel = c->audio_frame_len - c->audio_overlap_len;

		if (audio_decode_block(c, &gb, c->audio_out) < 0)
			return;
		if (gb.index & 31)
			bits_skip(&gb, 32 - (int)(gb.index & 31));
		if (!audio)
			continue;
		if (!c->audio_use_dct)
		{
			/* (interleaved already) */
			audio(context, c->audio_out, samples_per_channel);
		}
		else
		{
			float interleaved[AUDIO_MAX_FRAME_LENGTH * 2];
			int i, ch;

			for (i = 0; i < samples_per_channel && i < AUDIO_MAX_FRAME_LENGTH; i++)
				for (ch = 0; ch < c->audio_channels && ch < 2; ch++)
					interleaved[i * c->audio_channels + ch] = c->audio_out[ch * c->audio_frame_len + i];
			audio(context, interleaved, samples_per_channel * c->audio_channels);
		}
	}
}

/* ---------- the file (libavformat/bink.c) */

static uint32_t read_u32(uint8_t const *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static int frame_allocate(struct bink_decoder *c, struct frame *frame)
{
	int plane;

	for (plane = 0; plane < 4; plane++)
	{
		int chroma = plane == 1 || plane == 2;
		int width = ((c->width >> chroma) + 15) & ~15;
		int height = ((c->height >> chroma) + 15) & ~15;
		int pitch = width + 2 * PLANE_MARGIN;
		size_t size = (size_t)pitch * (height + 2 * PLANE_MARGIN);

		if (plane == 3 && !c->has_alpha)
			continue;
		frame->planes[plane].base = calloc(size, 1);
		if (!frame->planes[plane].base)
			return -1;
		frame->planes[plane].pitch = pitch;
		frame->planes[plane].data = frame->planes[plane].base + PLANE_MARGIN * pitch + PLANE_MARGIN;
		/* (black: Y 16, U and V 128) */
		memset(frame->planes[plane].base, chroma ? 128 : plane == 3 ? 255 : 16, size);
	}
	return 0;
}

struct bink_decoder *bink_decoder_open(FILE *file, char *error, int error_size)
{
	struct bink_decoder *c;
	uint8_t header[44];
	uint8_t bytes[8];
	uint32_t index, flags, previous;

	if (fseek(file, 0, SEEK_SET) != 0 || fread(header, 1, sizeof(header), file) != sizeof(header))
	{
		snprintf(error, (size_t)error_size, "too short for a Bink header");
		return NULL;
	}
	if (header[0] != 'B' || header[1] != 'I' || header[2] != 'K' || header[3] < 'f' || header[3] > 'k')
	{
		snprintf(error, (size_t)error_size, "not a Bink 1 movie of revision f to k");
		return NULL;
	}
	c = calloc(1, sizeof(*c));
	if (!c)
	{
		snprintf(error, (size_t)error_size, "out of memory");
		return NULL;
	}
	c->file = file;
	c->version = header[3];
	c->file_size = read_u32(header + 4) + 8;
	c->frame_count = read_u32(header + 8);
	c->width = (int)read_u32(header + 20);
	c->height = (int)read_u32(header + 24);
	c->fps_numerator = read_u32(header + 28);
	c->fps_denominator = read_u32(header + 32);
	flags = read_u32(header + 36);
	c->audio_tracks = read_u32(header + 40);
	c->has_alpha = (flags & BINK_FLAG_ALPHA) != 0;
	c->swap_planes = c->version >= 'h';
	if (!c->frame_count || c->frame_count > BINK_MAX_FRAMES || c->width <= 0 || c->width > BINK_MAX_WIDTH ||
		c->height <= 0 || c->height > BINK_MAX_HEIGHT || !c->fps_numerator || !c->fps_denominator ||
		c->audio_tracks > BINK_MAX_AUDIO_TRACKS)
	{
		snprintf(error, (size_t)error_size, "a Bink header out of range");
		goto fail;
	}
	if (c->version == 'k')
		fseek(file, 4, SEEK_CUR);
	if (c->audio_tracks)
	{
		/* (the tracks' largest decoded sizes, then each one's rate and
		flags, then their ids: the first track's is played) */
		fseek(file, 4 * (long)c->audio_tracks, SEEK_CUR);
		for (index = 0; index < c->audio_tracks; index++)
		{
			if (fread(bytes, 1, 4, file) != 4)
				goto truncated;
			if (!index)
			{
				uint32_t track_flags = (uint32_t)bytes[2] | (uint32_t)bytes[3] << 8;

				c->audio_rate = bytes[0] | bytes[1] << 8;
				c->audio_channels = track_flags & BINK_AUDIO_STEREO ? 2 : 1;
				c->audio_use_dct = (track_flags & BINK_AUDIO_USE_DCT) != 0;
			}
		}
		fseek(file, 4 * (long)c->audio_tracks, SEEK_CUR);
	}
	c->frame_offsets = malloc(((size_t)c->frame_count + 1) * sizeof(uint32_t));
	if (!c->frame_offsets)
		goto out_of_memory;
	for (index = 0; index < c->frame_count; index++)
	{
		if (fread(bytes, 1, 4, file) != 4)
			goto truncated;
		c->frame_offsets[index] = read_u32(bytes) & ~1u;
	}
	c->frame_offsets[c->frame_count] = c->file_size;
	for (index = 0, previous = 0; index <= c->frame_count; index++)
	{
		if (c->frame_offsets[index] <= previous && index)
		{
			snprintf(error, (size_t)error_size, "an invalid frame index");
			goto fail;
		}
		previous = c->frame_offsets[index];
	}
	if (c->audio_rate && (c->audio_rate < 1000 || audio_init(c) < 0))
		c->audio_rate = 0;
	bink_trees_make();
	if (init_bundles(c) < 0 || frame_allocate(c, &c->frames[0]) < 0 || frame_allocate(c, &c->frames[1]) < 0)
		goto out_of_memory;
	return c;

truncated:
	snprintf(error, (size_t)error_size, "a Bink header cut short");
	goto fail;
out_of_memory:
	snprintf(error, (size_t)error_size, "out of memory");
fail:
	bink_decoder_close(c);
	return NULL;
}

void bink_decoder_close(struct bink_decoder *c)
{
	int frame, plane;

	if (!c)
		return;
	for (frame = 0; frame < 2; frame++)
		for (plane = 0; plane < 4; plane++)
			free(c->frames[frame].planes[plane].base);
	free(c->bundle_memory);
	free(c->frame_offsets);
	free(c->packet);
	free(c->audio_out);
	free(c->audio_coeffs);
	free(c->audio_work);
	free(c->audio_twiddles);
	free(c);
}

int bink_decoder_width(struct bink_decoder const *c) { return c->width; }
int bink_decoder_height(struct bink_decoder const *c) { return c->height; }
uint32_t bink_decoder_frame_count(struct bink_decoder const *c) { return c->frame_count; }
uint32_t bink_decoder_fps_numerator(struct bink_decoder const *c) { return c->fps_numerator; }
uint32_t bink_decoder_fps_denominator(struct bink_decoder const *c) { return c->fps_denominator; }
int bink_decoder_audio_rate(struct bink_decoder const *c) { return c->audio_rate; }
int bink_decoder_audio_channels(struct bink_decoder const *c) { return c->audio_rate ? c->audio_channels : 0; }

static int decode_video(struct bink_decoder *c, uint8_t const *data, uint32_t size)
{
	struct bits gb;
	struct frame *frame = &c->frames[c->current ^ 1];
	struct frame *last = &c->frames[c->current];
	int plane, plane_idx;
	uint32_t bits_count = size * 8;

	bits_init(&gb, data, size);
	if (c->has_alpha)
	{
		if (c->version >= 'i')
			bits_skip(&gb, 32);
		if (bink_decode_plane(c, frame, last, &gb, 3, 0) < 0)
			return -1;
	}
	if (c->version >= 'i')
		bits_skip(&gb, 32);
	for (plane = 0; plane < 3; plane++)
	{
		plane_idx = (!plane || !c->swap_planes) ? plane : (plane ^ 3);
		if (bink_decode_plane(c, frame, last, &gb, plane_idx, !!plane) < 0)
			return -1;
		if (gb.index >= bits_count)
			break;
	}
	c->current ^= 1;
	c->have_last = 1;
	return 0;
}

int bink_decoder_decode(struct bink_decoder *c, uint32_t index, bink_audio_proc audio, void *context)
{
	uint32_t size, track, offset, remaining;
	uint8_t *packet;

	if (index >= c->frame_count)
		return -1;
	/* (a frame out of order: from the first, the movie's only key frame
	that this decodes from) */
	if (index != c->next_frame)
	{
		uint32_t frame;

		c->have_last = 0;
		c->audio_first = 1;
		c->next_frame = 0;
		for (frame = 0; frame < index; frame++)
		{
			if (bink_decoder_decode(c, frame, NULL, NULL) < 0)
				return -1;
		}
	}
	offset = c->frame_offsets[index];
	size = c->frame_offsets[index + 1] - offset;
	if (size > c->packet_size)
	{
		packet = realloc(c->packet, size);
		if (!packet)
			return -1;
		c->packet = packet;
		c->packet_size = size;
	}
	if (fseek(c->file, (long)offset, SEEK_SET) != 0 || fread(c->packet, 1, size, c->file) != size)
		return -1;
	packet = c->packet;
	remaining = size;
	for (track = 0; track < c->audio_tracks; track++)
	{
		uint32_t audio_size;

		if (remaining < 4)
			return -1;
		audio_size = read_u32(packet);
		if (audio_size > remaining - 4)
			return -1;
		if (!track && audio_size >= 4)
			audio_decode_packet(c, packet + 4, audio_size, audio, context);
		packet += 4 + audio_size;
		remaining -= 4 + audio_size;
	}
	if (decode_video(c, packet, remaining) < 0)
		return -1;
	c->next_frame = index + 1;
	return 0;
}

void bink_decoder_planes(struct bink_decoder const *c, uint8_t const *planes[3], int pitches[3])
{
	int plane;

	for (plane = 0; plane < 3; plane++)
	{
		planes[plane] = c->frames[c->current].planes[plane].data;
		pitches[plane] = c->frames[c->current].planes[plane].pitch;
	}
}

static uint8_t clamp_byte(int value)
{
	return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
}

void bink_decoder_to_xrgb(struct bink_decoder const *c, uint32_t *destination, long pitch, int height)
{
	struct frame const *frame = &c->frames[c->current];
	/* (revision k's full range, the others' studio range: FFmpeg's
	color_range) */
	int full = c->version == 'k';
	int x, y;

	for (y = 0; y < c->height && y < height; y++)
	{
		uint8_t const *luma = frame->planes[0].data + y * frame->planes[0].pitch;
		uint8_t const *u = frame->planes[1].data + (y >> 1) * frame->planes[1].pitch;
		uint8_t const *v = frame->planes[2].data + (y >> 1) * frame->planes[2].pitch;
		uint32_t *row = (uint32_t *)((uint8_t *)destination + y * pitch);

		for (x = 0; x < c->width; x++)
		{
			int cb = u[x >> 1] - 128, cr = v[x >> 1] - 128;
			int l = full ? luma[x] * 256 : (luma[x] - 16) * 298;
			int r = full ? l + 359 * cr : l + 409 * cr;
			int g = full ? l - 88 * cb - 183 * cr : l - 100 * cb - 208 * cr;
			int b = full ? l + 454 * cb : l + 516 * cb;

			row[x] = 0xFF000000u | (uint32_t)clamp_byte((r + 128) >> 8) << 16 |
				(uint32_t)clamp_byte((g + 128) >> 8) << 8 | clamp_byte((b + 128) >> 8);
		}
	}
}
