/*
HUD_PC_ICONS.C

The Xbox maps' HUD for the stock weapons (the campaign's too) drawn from
Halo PC's own art: the sheets of a Halo PC map of the multi_maps folder
(version 7), their pixels in that map or in that folder's bitmaps.map. The
weapon pickup icons (hud_msg_icons), the ammo meters, outlines, alphas and
type icons, the weapon backgrounds, the crosshairs and the scopes.

Each sheet's bitmaps are drawn, with HIGH-RES HUD on, from a texture four
times their size (hud_hires_tags.c's upscale_bitmaps, port/linux/src/
hud_hires.c), in place of the redraws of port/assets/hud. Halo PC's sheets
are the Xbox's at four times their size, sequence for sequence, though some
laid out otherwise (a sequence's whole bitmap on one is a sprite of a sheet
on the other: the ammo meters, the crosshairs). Each sequence's regions (its
sprites, or else its whole bitmaps) are painted with Halo PC's regions of
the same sequence, over the Xbox's region's place in the texture: the game
draws each where and as large as it drew the Xbox's, its registration point
and its draw order (an outline over its meter, a crosshair over another) as
they were. A sequence is painted only where Halo PC's has as many regions,
each of the same shape (within 4%), so that no layer moves; the pickup icons
(only the weapons', their first sprite: the messages' other icons, with
their text, stay the Xbox's), each drawn alone, are fitted in their place,
their shape kept. A sequence of another layout keeps its redraw or the
Xbox's art, and debug.txt says how many of each sheet's bitmaps are Halo
PC's.

The Custom Edition's and Halo PC's own maps draw their own sheets (Halo PC's
art), not the redraws (hud_pc_weapon_sheet, hud_hires_tags.c).

Halo PC's sheets' layouts are read once, the first time an Xbox map needs
them; their pixels are read when a texture is painted (one bitmap kept), not
kept. With no Halo PC map in multi_maps, or none whose sheets can be read,
the Xbox's are drawn, as before.
*/

#include "cseries.h"
#include "errors.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "bitmaps/bitmaps.h"
#include "cache/cache_files.h"
#include "interface/weapon_hud_interface_definition.h"
#include "tag_files/files.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "custom_edition_cache.h"

#include <stdlib.h>
#include <string.h>
#include <xtl.h>

/* ---------- constants */

#define PC_FOLDER "d:\\multi_maps\\"
/* Halo PC's maps' tag data is loaded here */
#define PC_TAG_BASE 0x40440000UL
#define PC_CACHE_VERSION 7

/* the stock weapons' HUD sheets; the first is the pickup icons' */
static char const *const sheet_tags[] =
{
	"ui\\hud\\bitmaps\\combined\\hud_msg_icons",
	"ui\\hud\\bitmaps\\combined\\hud_ammo_alphas",
	"ui\\hud\\bitmaps\\combined\\hud_ammo_meters",
	"ui\\hud\\bitmaps\\combined\\hud_ammo_outlines",
	"ui\\hud\\bitmaps\\combined\\hud_ammo_type_icons",
	"ui\\hud\\bitmaps\\combined\\hud_weapon_backgrounds",
	"ui\\hud\\bitmaps\\combined\\hud_reticles",
	"ui\\hud\\bitmaps\\sniper\\hud_reticles_scope",
	"ui\\hud\\bitmaps\\sniper\\sniper_scope_mask2",
	"ui\\hud\\bitmaps\\sniper\\sniper_scope_mask_sm",
	"ui\\hud\\bitmaps\\sniper\\sniper_scope_crosshairs2",
	"ui\\hud\\bitmaps\\sniper\\sniper_scope_crosshairs_sm",
	"ui\\hud\\bitmaps\\pistol\\pistol_scope_mask",
	"ui\\hud\\bitmaps\\pistol\\pistol_scope_mask split",
	"weapons\\sniper rifle\\bitmaps\\angle_ticks",
	"weapons\\sniper rifle\\bitmaps\\angle_ticks2",
	"weapons\\sniper rifle\\bitmaps\\angle_ticks_mask",
};

enum
{
	SHEET_COUNT = sizeof(sheet_tags) / sizeof(sheet_tags[0]),
	MSG_ICONS_SHEET = 0,
	PC_MAXIMUM_BITMAPS = 16,
	PC_MAXIMUM_SEQUENCES = 64,
	PC_MAXIMUM_REGIONS = 512,
	PC_MAXIMUM_BITMAP_SIDE = 2048,
	/* the most tag data read of a map (Halo PC's are about 5 MB) */
	PC_MAXIMUM_TAG_DATA = 64 << 20,
	XBOX_MAXIMUM_BITMAPS = 16,
	/* the Xbox's sheets' textures over their bitmaps (hud_hires.c) */
	UPSCALE = 4,
};

/* bitmap_data's flags: the pixels in bitmaps.map */
#define PC_BITMAP_EXTERNAL_FLAG 0x100
/* Halo PC's bitmap formats read */
enum
{
	_pc_format_a8 = 0,
	_pc_format_y8 = 1,
	_pc_format_ay8 = 2,
	_pc_format_a8y8 = 3,
	_pc_format_r5g6b5 = 6,
	_pc_format_a1r5g5b5 = 8,
	_pc_format_a4r4g4b4 = 9,
	_pc_format_x8r8g8b8 = 10,
	_pc_format_a8r8g8b8 = 11,
	_pc_format_dxt1 = 14,
	_pc_format_dxt3 = 15,
	_pc_format_dxt5 = 16,
};

/* ---------- globals */

/* Halo PC's sheets: where each bitmap's pixels are, and each sequence's
regions (a sprite, or a whole bitmap: its bitmap and bounds, in pixels) */
struct pc_region
{
	short bitmap;
	short x0, y0, x1, y1;
};

struct pc_sheet
{
	boolean loaded;
	char path[MAXIMUM_FILENAME_LENGTH + 1];
	short bitmap_count;
	struct
	{
		long width, height;
		short format;
		boolean external;
		unsigned long offset;
	} bitmaps[PC_MAXIMUM_BITMAPS];
	short sequence_count;
	struct
	{
		short first_region;
		short region_count;
	} sequences[PC_MAXIMUM_SEQUENCES];
	short region_count;
	struct pc_region *regions;
};

static struct
{
	boolean attempted;
	struct pc_sheet sheets[SHEET_COUNT];
	/* the last bitmap read (red, green, blue and alpha) */
	struct
	{
		short sheet, bitmap;
		unsigned char *pixels;
	} read;
} pc;

/* the loaded Xbox map's sheets: their bitmaps, which of them are painted,
and the pickup icons' sequences of its weapons */
static struct
{
	long group_index;
	short bitmap_count;
	struct bitmap_data *bitmaps[XBOX_MAXIMUM_BITMAPS];
	boolean painted[XBOX_MAXIMUM_BITMAPS];
} xbox_sheets[SHEET_COUNT];
static boolean weapon_sequences[PC_MAXIMUM_SEQUENCES];

/* ---------- private code */

static unsigned long pc_long(unsigned char const *bytes)
{
	return (unsigned long)bytes[0] | (unsigned long)bytes[1] << 8 |
		(unsigned long)bytes[2] << 16 | (unsigned long)bytes[3] << 24;
}

static unsigned short pc_short(unsigned char const *bytes)
{
	return (unsigned short)(bytes[0] | bytes[1] << 8);
}

static real pc_real(unsigned char const *bytes)
{
	unsigned long value = pc_long(bytes);
	real result;

	csmemcpy(&result, &value, sizeof(result));
	return result;
}

static boolean pc_file_read(char const *path, unsigned long offset, unsigned long size, void *buffer)
{
	struct file_reference file;
	boolean read = FALSE;

	if (!file_reference_create_from_path(&file, path, FALSE) || !file_exists(&file))
		return FALSE;
	if (file_open(&file, FLAG(_permission_read_bit)))
	{
		read = offset + size <= file_get_eof(&file) && file_set_position(&file, offset) &&
			file_read(&file, size, buffer);
		file_close(&file);
	}
	return read;
}

/* the tag data's bytes at a pointer (Halo PC's address), size of them, or NULL */
static unsigned char const *pc_at(unsigned char const *tag_data, unsigned long tag_data_size,
	unsigned long address, unsigned long size)
{
	if (address < PC_TAG_BASE || address - PC_TAG_BASE > tag_data_size ||
		size > tag_data_size - (address - PC_TAG_BASE))
	{
		return NULL;
	}
	return tag_data + (address - PC_TAG_BASE);
}

static void pc_sheet_forget(struct pc_sheet *sheet)
{
	/* (the game's free takes no NULL: debug_memory.c) */
	if (sheet->regions)
		free(sheet->regions);
	csmemset(sheet, 0, sizeof(*sheet));
}

/* a 16-bit colour's channel of bits at shift, as 8 bits */
static unsigned char pc_channel(unsigned long value, short shift, short bits)
{
	unsigned long maximum = (1UL << bits) - 1;

	return (unsigned char)((((value >> shift) & maximum) * 255 + maximum / 2) / maximum);
}

/* a DXT block's colours: the two given and the two between (or one, and
clear black, for DXT1's three colour blocks) */
static void pc_dxt_colors(unsigned char const *block, boolean dxt1, unsigned char colors[4][4])
{
	unsigned short c0 = pc_short(block), c1 = pc_short(block + 2);
	short index;

	for (index = 0; index < 2; index++)
	{
		unsigned short c = index ? c1 : c0;

		colors[index][0] = pc_channel(c, 11, 5);
		colors[index][1] = pc_channel(c, 5, 6);
		colors[index][2] = pc_channel(c, 0, 5);
		colors[index][3] = 255;
	}
	for (index = 0; index < 3; index++)
	{
		if (!dxt1 || c0 > c1)
		{
			colors[2][index] = (unsigned char)((2 * colors[0][index] + colors[1][index] + 1) / 3);
			colors[3][index] = (unsigned char)((colors[0][index] + 2 * colors[1][index] + 1) / 3);
		}
		else
		{
			colors[2][index] = (unsigned char)((colors[0][index] + colors[1][index]) / 2);
			colors[3][index] = 0;
		}
	}
	colors[2][3] = 255;
	colors[3][3] = (unsigned char)(dxt1 && c0 <= c1 ? 0 : 255);
}

/* the size of a bitmap's first level in a format (0: not one read) */
static unsigned long pc_level_size(short format, long width, long height)
{
	unsigned long blocks = (unsigned long)((width + 3) / 4) * (unsigned long)((height + 3) / 4);

	switch (format)
	{
	case _pc_format_a8: case _pc_format_y8: case _pc_format_ay8:
		return (unsigned long)width * height;
	case _pc_format_a8y8: case _pc_format_r5g6b5: case _pc_format_a1r5g5b5: case _pc_format_a4r4g4b4:
		return (unsigned long)width * height * 2;
	case _pc_format_x8r8g8b8: case _pc_format_a8r8g8b8:
		return (unsigned long)width * height * 4;
	case _pc_format_dxt1:
		return blocks * 8;
	case _pc_format_dxt3: case _pc_format_dxt5:
		return blocks * 16;
	}
	return 0;
}

/* a bitmap's first level decoded to red, green, blue and alpha */
static void pc_decode(short format, long width, long height, unsigned char const *data, unsigned char *rgba)
{
	long x, y;

	if (format == _pc_format_dxt1 || format == _pc_format_dxt3 || format == _pc_format_dxt5)
	{
		long block_size = format == _pc_format_dxt1 ? 8 : 16;
		long blocks_wide = (width + 3) / 4;
		long bx, by;

		for (by = 0; by < (height + 3) / 4; by++)
		{
			for (bx = 0; bx < blocks_wide; bx++)
			{
				unsigned char const *block = data + (by * blocks_wide + bx) * block_size;
				unsigned char const *color_block = format == _pc_format_dxt1 ? block : block + 8;
				unsigned char colors[4][4];
				unsigned char alphas[8];
				unsigned long color_bits = pc_long(color_block + 4);
				short px, py;

				pc_dxt_colors(color_block, format == _pc_format_dxt1, colors);
				if (format == _pc_format_dxt5)
				{
					short index;

					alphas[0] = block[0];
					alphas[1] = block[1];
					for (index = 2; index < 8; index++)
					{
						alphas[index] = alphas[0] > alphas[1] ?
							(unsigned char)(((8 - index) * alphas[0] + (index - 1) * alphas[1] + 3) / 7) :
							index < 6 ? (unsigned char)(((6 - index) * alphas[0] + (index - 1) * alphas[1] + 2) / 5) :
							(unsigned char)(index == 6 ? 0 : 255);
					}
				}
				for (py = 0; py < 4; py++)
				{
					for (px = 0; px < 4; px++)
					{
						long ix = bx * 4 + px, iy = by * 4 + py;
						short texel = (short)(py * 4 + px);
						unsigned char *out;
						unsigned char alpha;

						if (ix >= width || iy >= height)
							continue;
						out = rgba + (iy * width + ix) * 4;
						csmemcpy(out, colors[(color_bits >> (texel * 2)) & 3], 4);
						if (format == _pc_format_dxt3)
						{
							alpha = (unsigned char)((block[texel / 2] >> ((texel & 1) * 4)) & 0xF);
							out[3] = (unsigned char)(alpha * 17);
						}
						else if (format == _pc_format_dxt5)
						{
							/* (48 bits of 3-bit indices after the two alphas) */
							long bit = 16 + texel * 3;
							unsigned long bits = (unsigned long)block[bit / 8] |
								(unsigned long)block[bit / 8 + 1] << 8;

							out[3] = alphas[(bits >> (bit % 8)) & 7];
						}
					}
				}
			}
		}
		return;
	}
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			long pixel = y * width + x;
			unsigned char *out = rgba + pixel * 4;
			unsigned long value;

			switch (format)
			{
			case _pc_format_a8:
				out[0] = out[1] = out[2] = 255;
				out[3] = data[pixel];
				break;
			case _pc_format_y8:
				out[0] = out[1] = out[2] = data[pixel];
				out[3] = 255;
				break;
			case _pc_format_ay8:
				out[0] = out[1] = out[2] = out[3] = data[pixel];
				break;
			case _pc_format_a8y8:
				out[0] = out[1] = out[2] = data[pixel * 2];
				out[3] = data[pixel * 2 + 1];
				break;
			case _pc_format_r5g6b5:
				value = pc_short(data + pixel * 2);
				out[0] = pc_channel(value, 11, 5);
				out[1] = pc_channel(value, 5, 6);
				out[2] = pc_channel(value, 0, 5);
				out[3] = 255;
				break;
			case _pc_format_a1r5g5b5:
				value = pc_short(data + pixel * 2);
				out[0] = pc_channel(value, 10, 5);
				out[1] = pc_channel(value, 5, 5);
				out[2] = pc_channel(value, 0, 5);
				out[3] = (unsigned char)(value & 0x8000 ? 255 : 0);
				break;
			case _pc_format_a4r4g4b4:
				value = pc_short(data + pixel * 2);
				out[0] = pc_channel(value, 8, 4);
				out[1] = pc_channel(value, 4, 4);
				out[2] = pc_channel(value, 0, 4);
				out[3] = pc_channel(value, 12, 4);
				break;
			default:
				/* (X8R8G8B8 and A8R8G8B8: blue, green, red, alpha in memory) */
				out[0] = data[pixel * 4 + 2];
				out[1] = data[pixel * 4 + 1];
				out[2] = data[pixel * 4];
				out[3] = format == _pc_format_x8r8g8b8 ? 255 : data[pixel * 4 + 3];
				break;
			}
		}
	}
}

/* a sheet of a Halo PC map (path; its tag data, read): its bitmaps' places
and its sequences' regions, TRUE */
static boolean pc_sheet_read(struct pc_sheet *sheet, char const *path, unsigned char const *tag_data,
	unsigned long tag_data_size, unsigned char const *group)
{
	long sequence_count = (long)pc_long(group + 0x54);
	long bitmap_count = (long)pc_long(group + 0x60);
	unsigned char const *sequences = pc_at(tag_data, tag_data_size, pc_long(group + 0x58),
		(unsigned long)MAX(sequence_count, 0) * 0x40);
	unsigned char const *bitmaps = pc_at(tag_data, tag_data_size, pc_long(group + 0x64),
		(unsigned long)MAX(bitmap_count, 0) * 0x30);
	long index;

	if (!bitmaps || bitmap_count <= 0 || bitmap_count > PC_MAXIMUM_BITMAPS || sequence_count <= 0 || !sequences)
		return FALSE;
	_snprintf(sheet->path, sizeof(sheet->path) - 1, "%s", path);
	sheet->path[sizeof(sheet->path) - 1] = 0;
	for (index = 0; index < bitmap_count; index++)
	{
		unsigned char const *bitmap = bitmaps + index * 0x30;
		long width = pc_short(bitmap + 4), height = pc_short(bitmap + 6);
		short type = (short)pc_short(bitmap + 0xA);
		short format = (short)pc_short(bitmap + 0xC);

		/* (2D, of a format read) */
		if (type != 0 || !pc_level_size(format, width, height) || width <= 0 || height <= 0 ||
			width > PC_MAXIMUM_BITMAP_SIDE || height > PC_MAXIMUM_BITMAP_SIDE)
		{
			return FALSE;
		}
		sheet->bitmaps[index].width = width;
		sheet->bitmaps[index].height = height;
		sheet->bitmaps[index].format = format;
		sheet->bitmaps[index].external = (pc_short(bitmap + 0xE) & PC_BITMAP_EXTERNAL_FLAG) != 0;
		sheet->bitmaps[index].offset = pc_long(bitmap + 0x18);
	}
	sheet->bitmap_count = (short)bitmap_count;
	if (!(sheet->regions = malloc(PC_MAXIMUM_REGIONS * sizeof(*sheet->regions))))
		return FALSE;
	for (index = 0; index < sequence_count && index < PC_MAXIMUM_SEQUENCES; index++)
	{
		unsigned char const *sequence = sequences + index * 0x40;
		short first_bitmap = (short)pc_short(sequence + 0x20);
		short sequence_bitmaps = (short)pc_short(sequence + 0x22);
		long sprite_count = (long)pc_long(sequence + 0x34);
		unsigned char const *sprites = sprite_count > 0 ?
			pc_at(tag_data, tag_data_size, pc_long(sequence + 0x38), (unsigned long)sprite_count * 0x20) : NULL;
		long region_count = sprite_count > 0 ? sprite_count : MAX(sequence_bitmaps, 0);
		long region_index;

		sheet->sequences[index].first_region = sheet->region_count;
		sheet->sequences[index].region_count = 0;
		if ((sprite_count > 0 && !sprites) || sheet->region_count + region_count > PC_MAXIMUM_REGIONS)
			continue;
		for (region_index = 0; region_index < region_count; region_index++)
		{
			struct pc_region *out = &sheet->regions[sheet->region_count + region_index];
			short bitmap_index = sprites ? (short)pc_short(sprites + region_index * 0x20) :
				(short)(first_bitmap + region_index);

			out->bitmap = NONE;
			if (bitmap_index < 0 || bitmap_index >= sheet->bitmap_count)
				continue;
			if (sprites)
			{
				unsigned char const *sprite = sprites + region_index * 0x20;
				long width = sheet->bitmaps[bitmap_index].width;
				long height = sheet->bitmaps[bitmap_index].height;
				long x0 = (long)(pc_real(sprite + 8) * width + 0.5f);
				long x1 = (long)(pc_real(sprite + 12) * width + 0.5f);
				long y0 = (long)(pc_real(sprite + 16) * height + 0.5f);
				long y1 = (long)(pc_real(sprite + 20) * height + 0.5f);

				if (x0 < 0 || y0 < 0 || x1 <= x0 || y1 <= y0 || x1 > width || y1 > height)
					continue;
				out->x0 = (short)x0;
				out->y0 = (short)y0;
				out->x1 = (short)x1;
				out->y1 = (short)y1;
			}
			else
			{
				out->x0 = 0;
				out->y0 = 0;
				out->x1 = (short)sheet->bitmaps[bitmap_index].width;
				out->y1 = (short)sheet->bitmaps[bitmap_index].height;
			}
			out->bitmap = bitmap_index;
		}
		sheet->sequences[index].region_count = (short)region_count;
		sheet->region_count = (short)(sheet->region_count + region_count);
	}
	sheet->sequence_count = (short)MIN(sequence_count, PC_MAXIMUM_SEQUENCES);
	sheet->loaded = TRUE;
	return TRUE;
}

/* a Halo PC sheet's bitmap's pixels (red, green, blue and alpha), read from
its map or bitmaps.map (the last one read is kept), or NULL */
static unsigned char const *pc_bitmap_pixels(short sheet_index, short bitmap_index)
{
	struct pc_sheet const *sheet = &pc.sheets[sheet_index];
	long width, height;
	unsigned long size;
	unsigned char *data, *pixels;
	char source[MAXIMUM_FILENAME_LENGTH + 1];

	if (pc.read.pixels && pc.read.sheet == sheet_index && pc.read.bitmap == bitmap_index)
		return pc.read.pixels;
	if (pc.read.pixels)
		free(pc.read.pixels);
	pc.read.pixels = NULL;
	if (!sheet->loaded || bitmap_index < 0 || bitmap_index >= sheet->bitmap_count)
		return NULL;
	width = sheet->bitmaps[bitmap_index].width;
	height = sheet->bitmaps[bitmap_index].height;
	size = pc_level_size(sheet->bitmaps[bitmap_index].format, width, height);
	if (sheet->bitmaps[bitmap_index].external)
		_snprintf(source, sizeof(source) - 1, "%sbitmaps.map", PC_FOLDER);
	else
		_snprintf(source, sizeof(source) - 1, "%s", sheet->path);
	source[sizeof(source) - 1] = 0;
	data = malloc(size);
	pixels = malloc((size_t)width * height * 4);
	if (!data || !pixels || !pc_file_read(source, sheet->bitmaps[bitmap_index].offset, size, data))
	{
		if (data)
			free(data);
		if (pixels)
			free(pixels);
		return NULL;
	}
	pc_decode(sheet->bitmaps[bitmap_index].format, width, height, data, pixels);
	free(data);
	pc.read.sheet = sheet_index;
	pc.read.bitmap = bitmap_index;
	pc.read.pixels = pixels;
	return pixels;
}

/* the sheets of a Halo PC map of multi_maps (its file name) that it has:
how many */
static short pc_read_map(char const *name)
{
	char path[MAXIMUM_FILENAME_LENGTH + 1];
	unsigned char header[0x800];
	unsigned char *tag_data = NULL;
	unsigned long tag_data_offset, tag_data_size, tag_count, tag_array;
	unsigned long tag_index;
	short read = 0;

	_snprintf(path, sizeof(path) - 1, "%s%s", PC_FOLDER, name);
	path[sizeof(path) - 1] = 0;
	if (!pc_file_read(path, 0, sizeof(header), header) || memcmp(header, "daeh", 4) ||
		pc_long(header + 4) != PC_CACHE_VERSION)
	{
		return 0;
	}
	tag_data_offset = pc_long(header + 0x10);
	tag_data_size = pc_long(header + 0x14);
	if (tag_data_size < 0x28 || tag_data_size > PC_MAXIMUM_TAG_DATA || !(tag_data = malloc(tag_data_size)))
		return 0;
	if (!pc_file_read(path, tag_data_offset, tag_data_size, tag_data))
	{
		free(tag_data);
		return 0;
	}
	tag_array = pc_long(tag_data);
	tag_count = pc_long(tag_data + 0xC);
	for (tag_index = 0; tag_index < tag_count; tag_index++)
	{
		unsigned char const *entry = pc_at(tag_data, tag_data_size, tag_array + tag_index * 0x20, 0x20);
		unsigned char const *group;
		short sheet;

		if (!entry)
			break;
		if (memcmp(entry, "mtib", 4) || pc_long(entry + 0x18))
			continue;
		for (sheet = 0; sheet < SHEET_COUNT; sheet++)
		{
			size_t length = strlen(sheet_tags[sheet]) + 1;
			unsigned char const *tag_name;

			if (pc.sheets[sheet].loaded)
				continue;
			tag_name = pc_at(tag_data, tag_data_size, pc_long(entry + 0x10), (unsigned long)length);
			if (!tag_name || _stricmp((char const *)tag_name, sheet_tags[sheet]))
				continue;
			group = pc_at(tag_data, tag_data_size, pc_long(entry + 0x14), 0x6C);
			if (group && pc_sheet_read(&pc.sheets[sheet], path, tag_data, tag_data_size, group))
				read++;
			else
				pc_sheet_forget(&pc.sheets[sheet]);
			break;
		}
	}
	free(tag_data);
	return read;
}

/* Halo PC's sheets, from the maps of multi_maps that have them (once) */
static void pc_load(void)
{
	WIN32_FIND_DATAA data;
	HANDLE find;
	short sheet;
	short loaded = 0;

	if (pc.attempted)
		return;
	pc.attempted = TRUE;
	find = FindFirstFileA(PC_FOLDER "*.map", &data);
	if (find == INVALID_HANDLE_VALUE)
	{
		error(_error_silent, "pc hud: no map in %s: the Xbox's weapon HUD is drawn", PC_FOLDER);
		return;
	}
	do
	{
		char name[sizeof(data.cFileName) + 1];

		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		csmemcpy(name, data.cFileName, sizeof(data.cFileName));
		name[sizeof(data.cFileName)] = 0;
		/* (the resource maps are not caches) */
		if (!_stricmp(name, "bitmaps.map") || !_stricmp(name, "sounds.map") || !_stricmp(name, "loc.map") ||
			!_stricmp(name, "ui.map"))
		{
			continue;
		}
		loaded = (short)(loaded + pc_read_map(name));
	} while (loaded < SHEET_COUNT && FindNextFileA(find, &data));
	CloseHandle(find);
	for (sheet = 0; sheet < SHEET_COUNT; sheet++)
	{
		if (pc.sheets[sheet].loaded)
		{
			error(_error_silent, "pc hud: Halo PC's %s (%d bitmaps, %d sequences)", sheet_tags[sheet],
				pc.sheets[sheet].bitmap_count, pc.sheets[sheet].sequence_count);
		}
	}
	if (!loaded)
	{
		error(_error_silent, "pc hud: no Halo PC map in %s has its weapon HUD (with bitmaps.map): "
			"the Xbox's is drawn", PC_FOLDER);
	}
}

/* Halo PC's pixel at a point of a sheet's bitmap, bilinear (red, green, blue
and alpha), within a rectangle of it (a sprite's: its neighbours' pixels do
not bleed in), of pixels width wide */
static void pc_sample(unsigned char const *pixels, long width, long left, long top, long right, long bottom,
	real x, real y, real *rgba)
{
	long x0, y0, x1, y1;
	real fx, fy;
	real weights[4];
	long points[4];
	short index, channel;

	x -= 0.5f;
	y -= 0.5f;
	x0 = (long)PIN(x, (real)left, (real)(right - 1));
	y0 = (long)PIN(y, (real)top, (real)(bottom - 1));
	x1 = MIN(x0 + 1, right - 1);
	y1 = MIN(y0 + 1, bottom - 1);
	fx = PIN(x - x0, 0.f, 1.f);
	fy = PIN(y - y0, 0.f, 1.f);
	points[0] = y0 * width + x0; weights[0] = (1.f - fx) * (1.f - fy);
	points[1] = y0 * width + x1; weights[1] = fx * (1.f - fy);
	points[2] = y1 * width + x0; weights[2] = (1.f - fx) * fy;
	points[3] = y1 * width + x1; weights[3] = fx * fy;
	for (channel = 0; channel < 4; channel++)
		rgba[channel] = 0.f;
	for (index = 0; index < 4; index++)
	{
		unsigned char const *pixel = pixels + points[index] * 4;
		real a = pixel[3] * weights[index];

		rgba[3] += a;
		/* (alpha weighted, so that clear pixels' colour does not bleed in) */
		for (channel = 0; channel < 3; channel++)
			rgba[channel] += pixel[channel] * a;
	}
	for (channel = 0; channel < 3; channel++)
		rgba[channel] = rgba[3] > 0.f ? rgba[channel] / rgba[3] : 0.f;
}

/* Halo PC's rectangle (of source's pixels, source_stride wide) painted over a rectangle of an upscaled
texture (texture_width wide), which keeps the Xbox's place and size: the
game draws it where and as large as it drew the Xbox's, in the same order.
Of the same shape (within 4%) it fills it; of another it is fitted in it,
its shape kept, centred, and the rest of the rectangle cleared (none of the
Xbox's art is left beside it) */
static void pc_paint(unsigned char const *source, long source_stride, long sx0, long sy0, long sx1, long sy1,
	unsigned char *pixels, long texture_width, long x0, long y0, long x1, long y1)
{
	long source_width = sx1 - sx0, source_height = sy1 - sy0;
	long width = x1 - x0, height = y1 - y0;
	long fx0 = x0, fy0 = y0, fx1 = x1, fy1 = y1;
	real scale_x, scale_y;
	long x, y;

	if (source_width <= 0 || source_height <= 0 || width <= 0 || height <= 0)
		return;
	/* (shapes compared: source_width / source_height against width / height) */
	if (labs(source_width * height - source_height * width) * 25 > source_width * height)
	{
		if (source_width * height > source_height * width)
		{
			long fitted = (long)((real)width * source_height / source_width + 0.5f);

			fy0 = y0 + (height - fitted) / 2;
			fy1 = fy0 + MAX(fitted, 1);
		}
		else
		{
			long fitted = (long)((real)height * source_width / source_height + 0.5f);

			fx0 = x0 + (width - fitted) / 2;
			fx1 = fx0 + MAX(fitted, 1);
		}
	}
	scale_x = (real)source_width / (fx1 - fx0);
	scale_y = (real)source_height / (fy1 - fy0);
	for (y = y0; y < y1; y++)
	{
		for (x = x0; x < x1; x++)
		{
			unsigned char *texel = pixels + (y * texture_width + x) * 4;
			real rgba[4];
			short channel;

			if (x < fx0 || x >= fx1 || y < fy0 || y >= fy1)
			{
				texel[0] = texel[1] = texel[2] = texel[3] = 0;
				continue;
			}
			pc_sample(source, source_stride, sx0, sy0, sx1, sy1, sx0 + (x - fx0 + 0.5f) * scale_x,
				sy0 + (y - fy0 + 0.5f) * scale_y, rgba);
			for (channel = 0; channel < 4; channel++)
				texel[channel] = (unsigned char)PIN(rgba[channel] + 0.5f, 0.f, 255.f);
		}
	}
}

/* an Xbox sheet's sequence's regions: its sprites, or else its whole
bitmaps (how many; the one numbered region, if region is given) */
static short xbox_regions(short sheet, struct bitmap_group const *group, short sequence_index, short region,
	short *bitmap_index, real_rectangle2d *bounds)
{
	struct bitmap_group_sequence const *sequence;

	if (sequence_index < 0 || sequence_index >= group->sequences.count)
		return 0;
	sequence = TAG_BLOCK_GET_ELEMENT(&group->sequences, sequence_index, struct bitmap_group_sequence);
	if (sequence->sprites.count > 0)
	{
		if (region >= 0 && region < sequence->sprites.count)
		{
			struct bitmap_group_sprite const *sprite = TAG_BLOCK_GET_ELEMENT(&sequence->sprites, region,
				struct bitmap_group_sprite);

			*bitmap_index = sprite->bitmap_index;
			*bounds = sprite->bounds;
		}
		return (short)sequence->sprites.count;
	}
	if (region >= 0 && region < sequence->bitmap_count)
	{
		*bitmap_index = (short)(sequence->first_bitmap_index + region);
		bounds->x0 = 0.f;
		bounds->x1 = 1.f;
		bounds->y0 = 0.f;
		bounds->y1 = 1.f;
	}
	return (short)MAX(sequence->bitmap_count, 0);
}

/* how many of an Xbox sheet's sequence's regions are painted with Halo PC's
(0: none, it keeps its redraw or the Xbox's art) */
static short sequence_painted(short sheet, struct bitmap_group const *group, short sequence_index)
{
	struct pc_sheet const *source = &pc.sheets[sheet];
	short count = xbox_regions(sheet, group, sequence_index, NONE, NULL, NULL);
	short region;

	if (!source->loaded || count <= 0 || sequence_index >= source->sequence_count ||
		source->sequences[sequence_index].region_count <= 0)
	{
		return 0;
	}
	/* (the pickup icons: the weapons' first sprites, each drawn alone, fitted
	in its place whatever its shape; the others, with their text, stay the
	Xbox's) */
	if (sheet == MSG_ICONS_SHEET)
		count = (short)(weapon_sequences[sequence_index] ? 1 : 0);
	else if (count != source->sequences[sequence_index].region_count)
		return 0;
	for (region = 0; region < count; region++)
	{
		struct pc_region const *from = &source->regions[source->sequences[sequence_index].first_region + region];
		struct bitmap_data const *bitmap;
		short bitmap_index = NONE;
		real_rectangle2d bounds;
		real width, height, difference;
		long from_width, from_height;

		xbox_regions(sheet, group, sequence_index, region, &bitmap_index, &bounds);
		bitmap = bitmap_index >= 0 && bitmap_index < xbox_sheets[sheet].bitmap_count ?
			xbox_sheets[sheet].bitmaps[bitmap_index] : NULL;
		if (!bitmap || from->bitmap == NONE)
			return 0;
		if (sheet == MSG_ICONS_SHEET)
			continue;
		/* (the others are drawn over and under each other, an outline over
		its meter, a crosshair on its registration point: each region only of
		the same shape, within 4%, so that every layer keeps its place) */
		width = (bounds.x1 - bounds.x0) * bitmap->width;
		height = (bounds.y1 - bounds.y0) * bitmap->height;
		from_width = from->x1 - from->x0;
		from_height = from->y1 - from->y0;
		difference = from_width * height - from_height * width;
		if (width <= 0.f || height <= 0.f || (difference < 0.f ? -difference : difference) * 25.f >
			from_width * height)
		{
			return 0;
		}
	}
	return count;
}

/* ---------- public code */

/* whether a bitmap group's tag is one of the stock weapons' HUD sheets
(the Custom Edition's and Halo PC's maps draw theirs as they are, not the
redraws: hud_hires_tags.c) */
boolean hud_pc_weapon_sheet(char const *tag)
{
	short sheet;

	for (sheet = 0; sheet < SHEET_COUNT; sheet++)
	{
		if (tag && !_stricmp(tag, sheet_tags[sheet]))
			return TRUE;
	}
	return FALSE;
}

/* whether a bitmap of an Xbox map's sheet (tag, its index) is painted with
Halo PC's art (drawn upscaled, not from a redraw: hud_hires_tags.c) */
boolean hud_pc_sheet_painted(char const *tag, long bitmap_index)
{
	short sheet;

	for (sheet = 0; sheet < SHEET_COUNT; sheet++)
	{
		if (tag && !_stricmp(tag, sheet_tags[sheet]))
		{
			return xbox_sheets[sheet].group_index != NONE && bitmap_index >= 0 &&
				bitmap_index < xbox_sheets[sheet].bitmap_count && xbox_sheets[sheet].painted[bitmap_index];
		}
	}
	return FALSE;
}

/* the Xbox map's bitmaps painted with Halo PC's art, to be drawn upscaled:
how many (at most maximum) */
long hud_pc_painted_bitmaps(struct bitmap_data **bitmaps, long maximum)
{
	long count = 0;
	short sheet, index;

	for (sheet = 0; sheet < SHEET_COUNT; sheet++)
	{
		for (index = 0; index < xbox_sheets[sheet].bitmap_count && count < maximum; index++)
		{
			if (xbox_sheets[sheet].painted[index] && xbox_sheets[sheet].bitmaps[index])
				bitmaps[count++] = xbox_sheets[sheet].bitmaps[index];
		}
	}
	return count;
}

/* (hud_hires_tags.c, as a map's tags load) which of an Xbox map's weapon HUD
sheets' bitmaps are painted with Halo PC's */
void hud_pc_icons_map_loaded(void)
{
	struct tag_iterator iterator;
	long tag_index;
	short sheet, index;
	boolean any = FALSE;

	csmemset(xbox_sheets, 0, sizeof(xbox_sheets));
	csmemset(weapon_sequences, 0, sizeof(weapon_sequences));
	for (sheet = 0; sheet < SHEET_COUNT; sheet++)
		xbox_sheets[sheet].group_index = NONE;
	if (custom_edition_cache_tags_loaded())
		return;
	for (sheet = 0; sheet < SHEET_COUNT; sheet++)
	{
		xbox_sheets[sheet].group_index = tag_loaded(BITMAP_GROUP_TAG, sheet_tags[sheet]);
		any = any || xbox_sheets[sheet].group_index != NONE;
	}
	if (!any)
		return;
	/* (each weapon's pickup icon: the weapons' HUD interfaces', not the
	vehicles') */
	tag_iterator_new(&iterator, 'wphi');
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		struct weapon_hud_interface_definition const *definition =
			(struct weapon_hud_interface_definition const *)tag_get('wphi', tag_index);
		char const *name = tag_get_name(tag_index);
		short sequence = definition ? definition->messaging_icon.sequence_index : NONE;

		if (name && !_strnicmp(name, "weapons\\", 8) && sequence >= 0 && sequence < PC_MAXIMUM_SEQUENCES)
			weapon_sequences[sequence] = TRUE;
	}
	pc_load();
	for (sheet = 0; sheet < SHEET_COUNT; sheet++)
	{
		struct bitmap_group *group;
		short sequence_index;
		short painted = 0;

		if (xbox_sheets[sheet].group_index == NONE)
			continue;
		group = bitmap_group_get(xbox_sheets[sheet].group_index);
		for (index = 0; index < group->bitmaps.count && index < XBOX_MAXIMUM_BITMAPS; index++)
			xbox_sheets[sheet].bitmaps[index] = bitmap_group_try_and_get_bitmap(xbox_sheets[sheet].group_index, index);
		xbox_sheets[sheet].bitmap_count = index;
		for (sequence_index = 0; sequence_index < group->sequences.count; sequence_index++)
		{
			short regions = sequence_painted(sheet, group, sequence_index);
			short region;

			for (region = 0; region < regions; region++)
			{
				short bitmap_index = NONE;
				real_rectangle2d bounds;

				xbox_regions(sheet, group, sequence_index, region, &bitmap_index, &bounds);
				if (bitmap_index >= 0 && bitmap_index < xbox_sheets[sheet].bitmap_count)
					xbox_sheets[sheet].painted[bitmap_index] = TRUE;
			}
		}
		for (index = 0; index < xbox_sheets[sheet].bitmap_count; index++)
			painted = (short)(painted + (xbox_sheets[sheet].painted[index] ? 1 : 0));
		error(_error_silent, "pc hud: %s: %d of %d bitmaps drawn with Halo PC's art%s", sheet_tags[sheet],
			painted, xbox_sheets[sheet].bitmap_count, !pc.sheets[sheet].loaded ? " (not in Halo PC's maps)" :
			painted ? "" : " (laid out otherwise: the Xbox's)");
	}
}

/* (port/linux/src/hud_hires.c) An Xbox sheet's bitmap at address (width by
height) drawn as upscaled pixels (UPSCALE times its size, red, green, blue
and alpha): the regions Halo PC's sheet has of it painted with Halo PC's
art */
void hud_pc_icons_paint(unsigned long address, long width, long height, unsigned char *pixels)
{
	struct bitmap_group *group;
	struct pc_sheet const *source;
	long texture_width = width * UPSCALE;
	short sheet, bitmap_index = NONE, sequence_index;

	if (!address || !pixels)
		return;
	for (sheet = 0; sheet < SHEET_COUNT; sheet++)
	{
		for (bitmap_index = 0; bitmap_index < xbox_sheets[sheet].bitmap_count; bitmap_index++)
		{
			struct bitmap_data const *bitmap = xbox_sheets[sheet].bitmaps[bitmap_index];

			if (xbox_sheets[sheet].painted[bitmap_index] && bitmap && bitmap->cache_block_index != NONE &&
				(unsigned long)bitmap->base_address == address && bitmap->width == width &&
				bitmap->height == height)
			{
				break;
			}
		}
		if (bitmap_index < xbox_sheets[sheet].bitmap_count)
			break;
	}
	if (sheet >= SHEET_COUNT || !pc.sheets[sheet].loaded)
		return;
	source = &pc.sheets[sheet];
	group = bitmap_group_get(xbox_sheets[sheet].group_index);
	for (sequence_index = 0; sequence_index < group->sequences.count; sequence_index++)
	{
		short regions = sequence_painted(sheet, group, sequence_index);
		short region;

		for (region = 0; region < regions; region++)
		{
			struct pc_region const *from = &source->regions[source->sequences[sequence_index].first_region + region];
			unsigned char const *from_pixels;
			short region_bitmap = NONE;
			real_rectangle2d bounds;
			long x0, y0, x1, y1;

			xbox_regions(sheet, group, sequence_index, region, &region_bitmap, &bounds);
			if (region_bitmap != bitmap_index || from->bitmap == NONE)
				continue;
			x0 = PIN((long)(bounds.x0 * texture_width + 0.5f), 0, texture_width);
			x1 = PIN((long)(bounds.x1 * texture_width + 0.5f), 0, texture_width);
			y0 = PIN((long)(bounds.y0 * height * UPSCALE + 0.5f), 0, height * UPSCALE);
			y1 = PIN((long)(bounds.y1 * height * UPSCALE + 0.5f), 0, height * UPSCALE);
			if (x1 <= x0 || y1 <= y0)
				continue;
			from_pixels = pc_bitmap_pixels(sheet, from->bitmap);
			if (!from_pixels)
				continue;
			pc_paint(from_pixels, source->bitmaps[from->bitmap].width, from->x0, from->y0, from->x1, from->y1,
				pixels, texture_width, x0, y0, x1, y1);
		}
	}
}
