/*
HUD_PC_ICONS.C

The Xbox maps' weapon pickup icons (the campaign's too) drawn from Halo PC's
own, four times their size: the hud_msg_icons sheet of a Halo PC map of the
multi_maps folder (version 7), its pixels in that folder's bitmaps.map.

The Xbox's sheet (ui\hud\bitmaps\combined\hud_msg_icons: four 128x128 A8Y8
bitmaps) and Halo PC's (five 512x512 ones, laid out otherwise) have the same
sequences in the same order: a weapon's pickup icon is the sequence its
weapon HUD interface names (messaging_icon), in both. With HIGH-RES HUD on,
the Xbox's sheet is drawn from a texture four times its size
(hud_hires_tags.c's upscale_bitmaps, port/linux/src/hud_hires.c); here each
weapon's sprite of that texture is painted with Halo PC's sprite of its
sequence, scaled to fill it, so that the game draws the icon at the same
place and size, from Halo PC's art. The rest of the sheet (the messages'
other icons, with their English text) stays the Xbox's.

Halo PC's sheet is read once, the first time an Xbox map needs it, and kept
(2.6 MB). With no Halo PC map in multi_maps, or none whose sheet can be
read, the icons are the Xbox's, as before.
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

#define PC_ICONS_FOLDER "d:\\multi_maps\\"
#define PC_ICONS_TAG "ui\\hud\\bitmaps\\combined\\hud_msg_icons"
/* Halo PC's maps' tag data is loaded here */
#define PC_TAG_BASE 0x40440000UL
#define PC_CACHE_VERSION 7

enum
{
	PC_MAXIMUM_BITMAPS = 8,
	PC_MAXIMUM_SEQUENCES = 64,
	PC_MAXIMUM_BITMAP_SIDE = 1024,
	/* the most tag data read of a map (Halo PC's are about 5 MB) */
	PC_MAXIMUM_TAG_DATA = 64 << 20,
	XBOX_MAXIMUM_BITMAPS = 8,
	/* the Xbox's sheet's texture over its bitmaps (hud_hires.c) */
	UPSCALE = 4,
};

/* bitmap_data's flags: the pixels in bitmaps.map */
#define PC_BITMAP_EXTERNAL_FLAG 0x100
/* the formats read: A8Y8 (the sheet's) and A8R8G8B8 */
#define PC_FORMAT_A8Y8 3
#define PC_FORMAT_A8R8G8B8 11

/* ---------- globals */

static struct
{
	boolean attempted;
	boolean loaded;
	short bitmap_count;
	short sequence_count;
	struct
	{
		long width, height;
		/* alpha and luminance, two bytes a pixel */
		unsigned char *pixels;
	} bitmaps[PC_MAXIMUM_BITMAPS];
	/* each sequence's first sprite: its bitmap (NONE: none) and its bounds,
	in pixels */
	struct
	{
		short bitmap;
		short x0, y0, x1, y1;
	} sprites[PC_MAXIMUM_SEQUENCES];
} pc_icons;

/* the loaded Xbox map's: its sheet's bitmaps, and the sequences of its
weapons' icons */
static struct
{
	long group_index;
	struct bitmap_data *bitmaps[XBOX_MAXIMUM_BITMAPS];
	short bitmap_count;
	boolean weapon_sequences[PC_MAXIMUM_SEQUENCES];
} xbox_icons;

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

static void pc_icons_forget(void)
{
	short index;

	for (index = 0; index < PC_MAXIMUM_BITMAPS; index++)
		free(pc_icons.bitmaps[index].pixels);
	csmemset(pc_icons.bitmaps, 0, sizeof(pc_icons.bitmaps));
	pc_icons.bitmap_count = 0;
	pc_icons.sequence_count = 0;
	pc_icons.loaded = FALSE;
}

/* Halo PC's sheet from a map of multi_maps (its file name), and its pixels
from the map or bitmaps.map: TRUE */
static boolean pc_icons_read_map(char const *name)
{
	char path[MAXIMUM_FILENAME_LENGTH + 1];
	unsigned char header[0x800];
	unsigned char *tag_data = NULL;
	unsigned long tag_data_offset, tag_data_size, tag_count, tag_array;
	unsigned long tag_index;
	unsigned char const *group = NULL;
	boolean success = FALSE;

	_snprintf(path, sizeof(path) - 1, "%s%s", PC_ICONS_FOLDER, name);
	path[sizeof(path) - 1] = 0;
	if (!pc_file_read(path, 0, sizeof(header), header) || memcmp(header, "daeh", 4) ||
		pc_long(header + 4) != PC_CACHE_VERSION)
	{
		return FALSE;
	}
	tag_data_offset = pc_long(header + 0x10);
	tag_data_size = pc_long(header + 0x14);
	if (tag_data_size < 0x28 || tag_data_size > PC_MAXIMUM_TAG_DATA || !(tag_data = malloc(tag_data_size)))
		return FALSE;
	if (!pc_file_read(path, tag_data_offset, tag_data_size, tag_data))
		goto done;
	tag_array = pc_long(tag_data);
	tag_count = pc_long(tag_data + 0xC);
	for (tag_index = 0; tag_index < tag_count && !group; tag_index++)
	{
		unsigned char const *entry = pc_at(tag_data, tag_data_size, tag_array + tag_index * 0x20, 0x20);
		unsigned char const *tag_name;

		if (!entry)
			goto done;
		if (memcmp(entry, "mtib", 4) || pc_long(entry + 0x18))
			continue;
		tag_name = pc_at(tag_data, tag_data_size, pc_long(entry + 0x10), sizeof(PC_ICONS_TAG));
		if (tag_name && !memcmp(tag_name, PC_ICONS_TAG, sizeof(PC_ICONS_TAG)))
			group = pc_at(tag_data, tag_data_size, pc_long(entry + 0x14), 0x6C);
	}
	if (!group)
		goto done;
	{
		long sequence_count = (long)pc_long(group + 0x54);
		long bitmap_count = (long)pc_long(group + 0x60);
		unsigned char const *sequences = pc_at(tag_data, tag_data_size, pc_long(group + 0x58),
			(unsigned long)MAX(sequence_count, 0) * 0x40);
		unsigned char const *bitmaps = pc_at(tag_data, tag_data_size, pc_long(group + 0x64),
			(unsigned long)MAX(bitmap_count, 0) * 0x30);
		long index;

		if (!sequences || !bitmaps || sequence_count <= 0 || bitmap_count <= 0 ||
			bitmap_count > PC_MAXIMUM_BITMAPS)
		{
			goto done;
		}
		pc_icons_forget();
		for (index = 0; index < bitmap_count; index++)
		{
			unsigned char const *bitmap = bitmaps + index * 0x30;
			long width = pc_short(bitmap + 4), height = pc_short(bitmap + 6);
			short format = (short)pc_short(bitmap + 0xC);
			unsigned short flags = pc_short(bitmap + 0xE);
			unsigned long offset = pc_long(bitmap + 0x18);
			unsigned long size = (unsigned long)width * height * (format == PC_FORMAT_A8Y8 ? 2 : 4);
			unsigned char *read;
			char source[MAXIMUM_FILENAME_LENGTH + 1];
			long pixel;

			if ((format != PC_FORMAT_A8Y8 && format != PC_FORMAT_A8R8G8B8) || width <= 0 || height <= 0 ||
				width > PC_MAXIMUM_BITMAP_SIDE || height > PC_MAXIMUM_BITMAP_SIDE || !(read = malloc(size)))
			{
				goto done;
			}
			if (flags & PC_BITMAP_EXTERNAL_FLAG)
				_snprintf(source, sizeof(source) - 1, "%sbitmaps.map", PC_ICONS_FOLDER);
			else
				_snprintf(source, sizeof(source) - 1, "%s", path);
			source[sizeof(source) - 1] = 0;
			if (!pc_file_read(source, offset, size, read))
			{
				free(read);
				goto done;
			}
			/* (A8R8G8B8: its alpha and its green, as luminance) */
			if (format == PC_FORMAT_A8R8G8B8)
			{
				for (pixel = 0; pixel < width * height; pixel++)
				{
					read[pixel * 2] = read[pixel * 4 + 1];
					read[pixel * 2 + 1] = read[pixel * 4 + 3];
				}
			}
			pc_icons.bitmaps[index].width = width;
			pc_icons.bitmaps[index].height = height;
			pc_icons.bitmaps[index].pixels = read;
			pc_icons.bitmap_count = (short)(index + 1);
		}
		for (index = 0; index < sequence_count && index < PC_MAXIMUM_SEQUENCES; index++)
		{
			unsigned char const *sequence = sequences + index * 0x40;
			unsigned char const *sprite = pc_long(sequence + 0x34) > 0 ?
				pc_at(tag_data, tag_data_size, pc_long(sequence + 0x38), 0x20) : NULL;
			short bitmap_index = sprite ? (short)pc_short(sprite) : NONE;

			pc_icons.sprites[index].bitmap = NONE;
			if (sprite && bitmap_index >= 0 && bitmap_index < pc_icons.bitmap_count)
			{
				long width = pc_icons.bitmaps[bitmap_index].width;
				long height = pc_icons.bitmaps[bitmap_index].height;
				long x0 = (long)(pc_real(sprite + 8) * width + 0.5f);
				long x1 = (long)(pc_real(sprite + 12) * width + 0.5f);
				long y0 = (long)(pc_real(sprite + 16) * height + 0.5f);
				long y1 = (long)(pc_real(sprite + 20) * height + 0.5f);

				if (x0 >= 0 && y0 >= 0 && x1 > x0 && y1 > y0 && x1 <= width && y1 <= height)
				{
					pc_icons.sprites[index].bitmap = bitmap_index;
					pc_icons.sprites[index].x0 = (short)x0;
					pc_icons.sprites[index].y0 = (short)y0;
					pc_icons.sprites[index].x1 = (short)x1;
					pc_icons.sprites[index].y1 = (short)y1;
				}
			}
		}
		pc_icons.sequence_count = (short)MIN(sequence_count, PC_MAXIMUM_SEQUENCES);
		pc_icons.loaded = TRUE;
		success = TRUE;
	}

done:
	if (!success)
		pc_icons_forget();
	free(tag_data);
	return success;
}

/* Halo PC's sheet, from the first map of multi_maps that has it (once) */
static void pc_icons_load(void)
{
	WIN32_FIND_DATAA data;
	HANDLE find;
	char used[sizeof(data.cFileName) + 1] = "";

	if (pc_icons.attempted)
		return;
	pc_icons.attempted = TRUE;
	find = FindFirstFileA(PC_ICONS_FOLDER "*.map", &data);
	if (find == INVALID_HANDLE_VALUE)
	{
		error(_error_silent, "pc pickup icons: no map in %s: the Xbox's are drawn", PC_ICONS_FOLDER);
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
		if (pc_icons_read_map(name))
		{
			csmemcpy(used, name, sizeof(used));
			break;
		}
	} while (FindNextFileA(find, &data));
	CloseHandle(find);
	if (pc_icons.loaded)
	{
		error(_error_silent, "pc pickup icons: Halo PC's (%d sequences, %d bitmaps) from %s%s",
			pc_icons.sequence_count, pc_icons.bitmap_count, PC_ICONS_FOLDER, used);
	}
	else
	{
		error(_error_silent, "pc pickup icons: no Halo PC map in %s has its pickup icons (with bitmaps.map): "
			"the Xbox's are drawn", PC_ICONS_FOLDER);
	}
}

/* Halo PC's pixel (alpha, luminance) at a point of a bitmap, bilinear */
static void pc_icons_sample(short bitmap_index, real x, real y, real *alpha, real *luminance)
{
	long width = pc_icons.bitmaps[bitmap_index].width;
	long height = pc_icons.bitmaps[bitmap_index].height;
	unsigned char const *pixels = pc_icons.bitmaps[bitmap_index].pixels;
	long x0, y0, x1, y1;
	real fx, fy;
	real weights[4];
	long points[4];
	short index;

	x -= 0.5f;
	y -= 0.5f;
	x0 = (long)PIN(x, 0.f, (real)(width - 1));
	y0 = (long)PIN(y, 0.f, (real)(height - 1));
	x1 = MIN(x0 + 1, width - 1);
	y1 = MIN(y0 + 1, height - 1);
	fx = PIN(x - x0, 0.f, 1.f);
	fy = PIN(y - y0, 0.f, 1.f);
	points[0] = y0 * width + x0; weights[0] = (1.f - fx) * (1.f - fy);
	points[1] = y0 * width + x1; weights[1] = fx * (1.f - fy);
	points[2] = y1 * width + x0; weights[2] = (1.f - fx) * fy;
	points[3] = y1 * width + x1; weights[3] = fx * fy;
	*alpha = 0.f;
	*luminance = 0.f;
	for (index = 0; index < 4; index++)
	{
		real a = pixels[points[index] * 2 + 1] * weights[index];

		*alpha += a;
		/* (alpha weighted, so that clear pixels' colour does not bleed in) */
		*luminance += pixels[points[index] * 2] * a;
	}
	*luminance = *alpha > 0.f ? *luminance / *alpha : 0.f;
}

/* ---------- public code */

/* (hud_hires_tags.c, as an Xbox map's tags load) */
void hud_pc_icons_map_loaded(void)
{
	struct tag_iterator iterator;
	long tag_index;
	short index;

	csmemset(&xbox_icons, 0, sizeof(xbox_icons));
	xbox_icons.group_index = NONE;
	if (custom_edition_cache_tags_loaded())
		return;
	xbox_icons.group_index = tag_loaded(BITMAP_GROUP_TAG, PC_ICONS_TAG);
	if (xbox_icons.group_index == NONE)
		return;
	{
		struct bitmap_group *group = bitmap_group_get(xbox_icons.group_index);

		for (index = 0; index < group->bitmaps.count && index < XBOX_MAXIMUM_BITMAPS; index++)
			xbox_icons.bitmaps[index] = bitmap_group_try_and_get_bitmap(xbox_icons.group_index, index);
		xbox_icons.bitmap_count = index;
	}
	/* (each weapon's icon: the weapons' HUD interfaces', not the vehicles') */
	tag_iterator_new(&iterator, 'wphi');
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		struct weapon_hud_interface_definition const *definition =
			(struct weapon_hud_interface_definition const *)tag_get('wphi', tag_index);
		char const *name = tag_get_name(tag_index);
		short sequence = definition ? definition->messaging_icon.sequence_index : NONE;

		if (name && !_strnicmp(name, "weapons\\", 8) && sequence >= 0 && sequence < PC_MAXIMUM_SEQUENCES)
			xbox_icons.weapon_sequences[sequence] = TRUE;
	}
	pc_icons_load();
}

/* (port/linux/src/hud_hires.c) The Xbox sheet's bitmap at address (width by
height) drawn as upscaled pixels (UPSCALE times its size, four bytes a
pixel): its weapons' icons painted with Halo PC's */
void hud_pc_icons_paint(unsigned long address, long width, long height, unsigned char *pixels)
{
	struct bitmap_group *group;
	short bitmap_index;
	short sequence_index;

	if (!pc_icons.loaded || xbox_icons.group_index == NONE || !address || !pixels)
		return;
	for (bitmap_index = 0; bitmap_index < xbox_icons.bitmap_count; bitmap_index++)
	{
		struct bitmap_data const *bitmap = xbox_icons.bitmaps[bitmap_index];

		if (bitmap && bitmap->cache_block_index != NONE && (unsigned long)bitmap->base_address == address &&
			bitmap->width == width && bitmap->height == height)
		{
			break;
		}
	}
	if (bitmap_index >= xbox_icons.bitmap_count)
		return;
	group = bitmap_group_get(xbox_icons.group_index);
	for (sequence_index = 0; sequence_index < group->sequences.count && sequence_index < pc_icons.sequence_count;
		sequence_index++)
	{
		struct bitmap_group_sequence const *sequence;
		struct bitmap_group_sprite const *sprite;
		long x0, y0, x1, y1, x, y;
		short source;
		real scale_x, scale_y;

		if (!xbox_icons.weapon_sequences[sequence_index] || pc_icons.sprites[sequence_index].bitmap == NONE)
			continue;
		sequence = TAG_BLOCK_GET_ELEMENT(&group->sequences, sequence_index, struct bitmap_group_sequence);
		if (sequence->sprites.count <= 0)
			continue;
		sprite = TAG_BLOCK_GET_ELEMENT(&sequence->sprites, 0, struct bitmap_group_sprite);
		if (sprite->bitmap_index != bitmap_index)
			continue;
		x0 = (long)(sprite->bounds.x0 * width * UPSCALE + 0.5f);
		x1 = (long)(sprite->bounds.x1 * width * UPSCALE + 0.5f);
		y0 = (long)(sprite->bounds.y0 * height * UPSCALE + 0.5f);
		y1 = (long)(sprite->bounds.y1 * height * UPSCALE + 0.5f);
		x0 = PIN(x0, 0, width * UPSCALE);
		x1 = PIN(x1, 0, width * UPSCALE);
		y0 = PIN(y0, 0, height * UPSCALE);
		y1 = PIN(y1, 0, height * UPSCALE);
		if (x1 <= x0 || y1 <= y0)
			continue;
		source = pc_icons.sprites[sequence_index].bitmap;
		scale_x = (real)(pc_icons.sprites[sequence_index].x1 - pc_icons.sprites[sequence_index].x0) / (x1 - x0);
		scale_y = (real)(pc_icons.sprites[sequence_index].y1 - pc_icons.sprites[sequence_index].y0) / (y1 - y0);
		for (y = y0; y < y1; y++)
		{
			for (x = x0; x < x1; x++)
			{
				unsigned char *texel = pixels + (y * width * UPSCALE + x) * 4;
				real alpha, luminance;

				pc_icons_sample(source,
					pc_icons.sprites[sequence_index].x0 + (x - x0 + 0.5f) * scale_x,
					pc_icons.sprites[sequence_index].y0 + (y - y0 + 0.5f) * scale_y,
					&alpha, &luminance);
				texel[0] = texel[1] = texel[2] = (unsigned char)PIN(luminance + 0.5f, 0.f, 255.f);
				texel[3] = (unsigned char)PIN(alpha + 0.5f, 0.f, 255.f);
			}
		}
	}
}
