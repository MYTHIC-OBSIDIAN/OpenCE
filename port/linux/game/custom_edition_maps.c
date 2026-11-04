/*
CUSTOM_EDITION_MAPS.C

Lists Custom Edition maps for the menus (custom_edition_maps.h).

Whenever a map list opens, the custom_maps folder and then the Halo Custom
Edition install's maps folder are scanned for CE caches (.map; OpenSauce
caches are refused by the loader, so not listed). Multiplayer maps join the
level list after the Xbox levels, and the map lists' CUSTOM EDITION;
campaign maps (solo scenarios) are the map lists' CUSTOM SINGLEPLAYER,
played alone or as network co-op. The stock campaign levels get display
indices here too, so the menus can show them the same way.

A map's level name is custom_maps\<name> (CUSTOM_EDITION_LEVEL_NAME_PREFIX),
which the cache loader reads from the custom maps folders alone: a map named
as one of the game's own levels (a30.map, bloodgulch.map) is listed and
played as itself. The game engine keeps 63 characters of a level name, so a
map whose name is longer than 51 is skipped. A map keeps its name: a network
game names it so, and says which version it is on by its hash (map_share.h).
A downloaded map of a name another version has is named <name>_<hash>.map;
the menus show it without the hash, with (2), (3) and so on after the first
of the name. A map a build of before named for its hash has its name given
back (map_share_map_found).

Each map can have, beside it in its folder:
- <name>.bmp, its picture (bmp_files.c; for a map named for its hash, a
  picture named without the hash also does). The middle is cropped to the
  menus' picture shape. It is read the first time it is drawn, and freed
  when the maps are rescanned. Without one, the unknown level's picture
  is shown.
- <name>.txt, its description, line by line (the Xbox descriptions use
  lines of about 20 characters).

This LAN build lists the multiplayer maps of the multi_maps folder (Halo PC's:
its own six, Gearbox's Danger Canyon, Death Island, Gephyrophobia, Ice
Fields, Infinity and Timberland, and its versions of the Xbox's, with Halo
PC's bitmaps.map and sounds.map beside them) first in the map lists'
MULTIPLAYER, by their names (the Xbox's own multiplayer maps are not listed), and a fixed list of the Digsite maps
(digsite_maps) in their DIGSITE; CUSTOM EDITION leaves them out. A map of
multi_maps plays from there whatever other folder has one of its name; the
Digsite maps are looked for first in digsite_maps, then in the folders of
every other CE map. One of Halo PC's maps of multi_maps is named as Halo PC's
list names it, its description, without a <name>.txt, is the PC menus' own
(their map_data strings), and its picture, without a <name>.bmp, the Xbox
level's for one of the Xbox's, else the unknown level's:
tools/pc_map_pictures.py writes Gearbox's six's .bmp pictures from a Halo
PC or Custom Edition install's ui.map. A Digsite map not found is listed all
the same, as not installed.
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "tag_files/files.h"
#include "tag_files/tag_files.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "main/main.h"
#include "saved games/player_profile.h"
#include "text/text_group.h"
#include "text/unicode.h"
#include "bmp_files.h"
#include "custom_edition_cache.h"
#include "custom_edition_maps.h"
#include "map_share.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtl.h>

/* ---------- constants */

/* Custom Edition campaign maps, for co-op */
#define MAXIMUM_CUSTOM_EDITION_CAMPAIGNS 1024
#define FIRST_CUSTOM_CAMPAIGN_DISPLAY_INDEX 0x6000
/* room for the level list's Xbox levels (ui_widget_event_handler_functions.c has 13) */
#define MAXIMUM_XBOX_LEVELS 16
/* the Xbox's multiplayer maps (xbox_maps) */
#define NUMBER_OF_ORIGINAL_MAPS 13

/* the longest map file name whose level name
(CUSTOM_EDITION_LEVEL_NAME_PREFIX and it) the game engine keeps whole */
#define MAXIMUM_MAP_NAME_LENGTH \
	(CUSTOM_EDITION_MAXIMUM_LEVEL_NAME_LENGTH - (NUMBEROF(CUSTOM_EDITION_LEVEL_NAME_PREFIX) - 1))

/* Display indices for CE multiplayer maps, clear of every string and frame
index in the menus' tags (15 level names, 14 level pictures), and below the
custom campaigns' and ui_widget.c's spinner descriptions' (0x7000). */
#define FIRST_DISPLAY_INDEX 0x4000

/* The stock campaign levels, for co-op: their display indices, names, and
pictures (the campaign menu's, one frame per level in order). */
#define FIRST_CAMPAIGN_DISPLAY_INDEX 0x3000
#define CAMPAIGN_LEVEL_PICTURES_TAG_NAME "ui\\shell\\bitmaps\\sp_levels"
static wchar_t campaign_level_names[NUMBER_OF_SINGLE_PLAYER_LEVELS][32] =
{
	L"The Pillar of Autumn", L"Halo", L"The Truth and Reconciliation", L"The Silent Cartographer",
	L"Assault on the Control Room", L"343 Guilty Spark", L"The Library", L"Two Betrayals", L"Keyes", L"The Maw",
};
static wchar_t campaign_level_description[] = L"A campaign level, played co-op";

/* The level pictures tag, the picture shape (140x114 in the menus' 640x480;
the lobby's is 139x113), and the unknown level's frame
(ui_widget_game_data_input_functions.c). */
#define LEVEL_PICTURES_TAG_NAME "ui\\shell\\bitmaps\\mp_map_grafix"
#define LEVEL_PICTURE_SHAPE_WIDTH 140
#define LEVEL_PICTURE_SHAPE_HEIGHT 114
#define UNKNOWN_LEVEL_FRAME 13

#define PICTURE_EXTENSION ".bmp"
/* a 4K screenshot is 25 MB as a bmp file */
#define MAXIMUM_PICTURE_FILE_BYTES 0x04000000L
/* picture textures are square, about twice the menus' picture size */
#define PICTURE_TEXTURE_SIZE 256

#define DESCRIPTION_EXTENSION ".txt"

/* the Digsite maps' display indices, after the CE campaigns' and below
ui_widget.c's spinner descriptions' (0x7000) */
#define FIRST_DIGSITE_DISPLAY_INDEX 0x6800
#define NUMBER_OF_DIGSITE_MAPS 16

/* the PC menus' map descriptions (menu_tags.c's copy of ui.map's), where
Halo PC's own maps follow the Xbox's 13 in stock_maps' order */
#define PC_LEVEL_DESCRIPTIONS_TAG_NAME "pc\\main_menu\\multiplayer_type_select\\mp_map_select\\map_data"
#define FIRST_PC_STOCK_MAP_STRING 13
/* descriptions are a few short lines */
#define MAXIMUM_DESCRIPTION_FILE_BYTES 1024
#define MAXIMUM_DESCRIPTION_LENGTH 127

/* bitmaps.c's format of 32-bit color with alpha */
enum
{
	_bitmap_format_a8r8g8b8 = 11,
};

/* (the multiplayer maps', custom campaigns' and spinner descriptions'
display indices do not meet) */
typedef char verify_multiplayer_display_indices[
	FIRST_DISPLAY_INDEX + CUSTOM_EDITION_MAPS_MAXIMUM <= FIRST_CUSTOM_CAMPAIGN_DISPLAY_INDEX ? 1 : -1];
typedef char verify_campaign_display_indices[
	FIRST_CUSTOM_CAMPAIGN_DISPLAY_INDEX + MAXIMUM_CUSTOM_EDITION_CAMPAIGNS <= FIRST_DIGSITE_DISPLAY_INDEX ? 1 : -1];
typedef char verify_digsite_display_indices[
	FIRST_DIGSITE_DISPLAY_INDEX + NUMBER_OF_DIGSITE_MAPS <= 0x7000 ? 1 : -1];

/* ---------- structures */

struct custom_edition_map
{
	/* file name without extension, and the folder it was found in */
	char name[MAXIMUM_MAP_NAME_LENGTH + 1];
	char folder[32];
	/* saved_game_file_remember_last_used_multiplayer_map writes
	MAXIMUM_FILENAME_LENGTH+1 characters of a level name */
	char level_name[MAXIMUM_FILENAME_LENGTH + 1];
	/* (and room for a version's number, " (2)") */
	wchar_t display_name[MAXIMUM_MAP_NAME_LENGTH + 8];
	wchar_t description[MAXIMUM_DESCRIPTION_LENGTH + 1];
	/* (whether description is its <name>.txt's, not the default) */
	boolean description_read;
	/* (found in multi_maps: MULTIPLAYER's) */
	boolean pc;
	boolean picture_read;
	struct bitmap_data *picture;
};

struct custom_edition_maps_globals
{
	boolean looked_for;
	short map_count;
	struct custom_edition_map maps[CUSTOM_EDITION_MAPS_MAXIMUM];
	/* campaign maps, kept out of the level list */
	short campaign_count;
	struct custom_edition_map campaigns[MAXIMUM_CUSTOM_EDITION_CAMPAIGNS];
	/* the latest level list: the Xbox levels, then the CE maps */
	short xbox_level_count;
	char *levels[MAXIMUM_XBOX_LEVELS + CUSTOM_EDITION_MAPS_MAXIMUM];
	/* the multiplayer maps the CUSTOM EDITION list shows (neither multi_maps'
	nor Digsite's), and multi_maps' (by their menu names), by their index in
	maps */
	short listed_count;
	short listed[CUSTOM_EDITION_MAPS_MAXIMUM];
	short pc_count;
	short pc[CUSTOM_EDITION_MAPS_MAXIMUM];
	/* each Digsite map's: the map found (in maps or campaigns), else
	NULL and a stand-in of its name, saying it is not installed */
	struct custom_edition_map *digsite[NUMBER_OF_DIGSITE_MAPS];
	struct custom_edition_map digsite_missing[NUMBER_OF_DIGSITE_MAPS];
	/* the Xbox's multiplayer maps put in multi_maps as <name>_xbox.map, by
	their index in the Xbox's level list (xbox_maps'), and their menu names
	(Halo PC's for the map, and " (Classic)") */
	short original_count;
	short original[NUMBER_OF_ORIGINAL_MAPS];
	wchar_t original_names[NUMBER_OF_ORIGINAL_MAPS][MAXIMUM_MAP_NAME_LENGTH + 16];
	/* MULTIPLAYER's entries: multi_maps' Halo PC maps (an index in pc), each
	followed by the Xbox's own of it if it is there (-1 - its index in the
	Xbox's level list), and those of the Xbox's without one of Halo PC's in
	their places by name (made with the first ask after a scan) */
	boolean multiplayer_made;
	short multiplayer_count;
	short multiplayer[CUSTOM_EDITION_MAPS_MAXIMUM + NUMBER_OF_ORIGINAL_MAPS];
};

/* ---------- globals */

static struct custom_edition_maps_globals custom_edition_maps_globals;

/* Halo PC's own multiplayer maps (not on the Xbox), with their menu names
from ui.map's mp_map_list, whose map_data strings follow the Xbox maps'
(FIRST_PC_STOCK_MAP_STRING) */
static struct
{
	char const *file;
	wchar_t const *name;
} const stock_maps[] =
{
	{ "icefields", L"Ice Fields" }, { "deathisland", L"Death Island" }, { "dangercanyon", L"Danger Canyon" },
	{ "infinity", L"Infinity" }, { "timberland", L"Timberland" }, { "gephyrophobia", L"Gephyrophobia" },
};

/* Halo PC's versions of the Xbox's multiplayer maps, in the Xbox's level
list's order (ui_widget_event_handler_functions.c), which is their frames'
in the level pictures and their strings' in the PC menus' map_data, with
Halo PC's names for them */
static struct
{
	char const *file;
	wchar_t const *name;
} const xbox_maps[] =
{
	{ "beavercreek", L"Battle Creek" }, { "sidewinder", L"Sidewinder" }, { "damnation", L"Damnation" },
	{ "ratrace", L"Rat Race" }, { "prisoner", L"Prisoner" }, { "hangemhigh", L"Hang 'Em High" },
	{ "chillout", L"Chill Out" }, { "carousel", L"Derelict" }, { "boardingaction", L"Boarding Action" },
	{ "bloodgulch", L"Blood Gulch" }, { "wizard", L"Wizard" }, { "putput", L"Chiron TL-34" },
	{ "longest", L"Longest" },
};
typedef char verify_number_of_original_maps[NUMBEROF(xbox_maps) == NUMBER_OF_ORIGINAL_MAPS ? 1 : -1];

/* the Digsite maps, in the DIGSITE list's order: their files' names (without
.map) and their menu names */
static struct
{
	char const *file;
	wchar_t const *name;
} const digsite_maps[NUMBER_OF_DIGSITE_MAPS] =
{
	{ "abyss", L"Abyss" }, { "b20", L"B20" }, { "city", L"City" }, { "destructo", L"Destructo" },
	{ "dusk", L"Dusk" }, { "indoor", L"Indoor" }, { "m1_assault", L"M1 Assault" },
	{ "m1_defense", L"M1 Defense" }, { "m1_exploration_moonbase", L"M1 Exploration Moonbase" },
	{ "ruinedpain", L"Ruined Pain" }, { "spacestation", L"Space Station" }, { "spasm", L"Spasm" },
	{ "swamp", L"Swamp" }, { "swampthing", L"Swamp Thing" }, { "tutorial", L"Tutorial" },
	{ "underground", L"Underground" },
};

/* for a map without a description file */
static wchar_t const default_description[] = L"Halo Custom\r\nEdition map";
/* a Halo PC map's, without the PC menus' (the Xbox menus') */
static wchar_t stock_description[] = L"Halo PC map\r\n(Gearbox)";
/* a Digsite map's, by its kind */
static wchar_t const digsite_campaign_description[] = L"Digsite map\r\n\r\nCampaign:\r\nalone or co-op";
static wchar_t const digsite_multiplayer_description[] = L"Digsite map\r\n\r\nMultiplayer";
static wchar_t const digsite_missing_description[] =
	L"Not installed\r\n\r\nPut the .map file\r\nin custom_maps";

/* ---------- private code */

static void custom_edition_maps_forget(
	void)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	short map_index;

	for (map_index = 0; map_index < globals->map_count; map_index++)
	{
		if (globals->maps[map_index].picture)
			bitmap_delete(globals->maps[map_index].picture);
	}
	for (map_index = 0; map_index < globals->campaign_count; map_index++)
	{
		if (globals->campaigns[map_index].picture)
			bitmap_delete(globals->campaigns[map_index].picture);
	}
	for (map_index = 0; map_index < NUMBER_OF_DIGSITE_MAPS; map_index++)
	{
		if (globals->digsite_missing[map_index].picture)
			bitmap_delete(globals->digsite_missing[map_index].picture);
		globals->digsite_missing[map_index].picture = NULL;
		globals->digsite_missing[map_index].picture_read = FALSE;
		globals->digsite[map_index] = NULL;
	}
	globals->map_count = 0;
	globals->campaign_count = 0;
	globals->listed_count = 0;
	globals->pc_count = 0;

	return;
}

/* the map's index in stock_maps, or NONE (by its name without its hash) */
static short stock_map_index(
	char const *name)
{
	short length = map_share_name_base_length(name);
	short index;

	for (index = 0; index < NUMBEROF(stock_maps); index++)
	{
		if ((short)csstrlen(stock_maps[index].file) == length && !_strnicmp(name, stock_maps[index].file, length))
			return index;
	}
	return NONE;
}

/* the map's index in xbox_maps, or NONE (by its name without its hash) */
static short xbox_map_index(
	char const *name)
{
	short length = map_share_name_base_length(name);
	short index;

	for (index = 0; index < NUMBEROF(xbox_maps); index++)
	{
		if ((short)csstrlen(xbox_maps[index].file) == length && !_strnicmp(name, xbox_maps[index].file, length))
			return index;
	}
	return NONE;
}

/* the map's index in digsite_maps, or NONE (by its name without its hash) */
static short digsite_map_index(
	char const *name);

short custom_edition_maps_home(
	char const *name)
{
	return digsite_map_index(name) != NONE ? _custom_edition_maps_home_digsite : _custom_edition_maps_home_none;
}

/* the map's index in digsite_maps, or NONE (by its name without its hash) */
static short digsite_map_index(
	char const *name)
{
	short length = map_share_name_base_length(name);
	short index;

	for (index = 0; index < NUMBER_OF_DIGSITE_MAPS; index++)
	{
		if ((short)csstrlen(digsite_maps[index].file) == length && !_strnicmp(name, digsite_maps[index].file, length))
			return index;
	}
	return NONE;
}

/* A map's menu name: Halo PC's name for its own maps (and for its versions
of the Xbox's, in multi_maps), otherwise the file
name (without a hash it ends in) with underscores as spaces and words
capitalized ("the_bay_of_pigs" becomes "The Bay Of Pigs"). */
static void display_name_make(
	char const *name,
	boolean pc,
	wchar_t *display_name)
{
	boolean word_start = TRUE;
	short index = stock_map_index(name);
	short length = map_share_name_base_length(name);

	if (index != NONE)
	{
		ustrcpy(display_name, stock_maps[index].name);
		return;
	}
	/* (Halo PC's version of an Xbox map, in multi_maps) */
	if (pc && (index = xbox_map_index(name)) != NONE)
	{
		ustrcpy(display_name, xbox_maps[index].name);
		return;
	}
	if ((index = digsite_map_index(name)) != NONE)
	{
		ustrcpy(display_name, digsite_maps[index].name);
		return;
	}

	for (index = 0; index < length; index++)
	{
		unsigned char character = (unsigned char)name[index];

		if (character == '_')
		{
			character = ' ';
		}
		else if (word_start && character >= 'a' && character <= 'z')
		{
			character = (unsigned char)(character - 'a' + 'A');
		}
		word_start = character == ' ';
		display_name[index] = (wchar_t)character;
	}
	display_name[index] = 0;

	return;
}

/* Reads <name>.txt into map->description: line ends become \r\n (as in the
menus' strings), tabs become spaces, and non-ASCII characters become '?'.
A missing or empty file gives the default description. */
static void custom_edition_map_description_read(
	struct custom_edition_map *map)
{
	char path[MAXIMUM_FILENAME_LENGTH + 1];
	byte text[MAXIMUM_DESCRIPTION_FILE_BYTES];
	FILE *stream;
	long text_size = 0;
	long text_index = 0;
	short length = 0;

	csprintf(path, "%s%s%s", map->folder, map->name, DESCRIPTION_EXTENSION);
	stream = fopen(path, "rb");
	/* (a map named for its hash: its description named without it too) */
	if (!stream && map_share_name_hashed(map->name))
	{
		csprintf(path, "%s%.*s%s", map->folder, (int)map_share_name_base_length(map->name), map->name,
			DESCRIPTION_EXTENSION);
		stream = fopen(path, "rb");
	}
	if (stream)
	{
		text_size = (long)fread(text, 1, sizeof(text), stream);
		fclose(stream);
	}
	/* the byte order mark of UTF-8 text */
	if (text_size >= 3 && text[0] == 0xEF && text[1] == 0xBB && text[2] == 0xBF)
	{
		text_index = 3;
	}
	for (; text_index < text_size && length < MAXIMUM_DESCRIPTION_LENGTH; text_index++)
	{
		byte character = text[text_index];

		if (character == '\n')
		{
			if (length + 2 > MAXIMUM_DESCRIPTION_LENGTH)
			{
				break;
			}
			map->description[length++] = '\r';
			map->description[length++] = '\n';
		}
		else if (character == '\t')
		{
			map->description[length++] = ' ';
		}
		else if (character >= ' ' && character < 0x7F)
		{
			map->description[length++] = character;
		}
		else if (character >= 0xC0)
		{
			/* the lead byte of a multi-byte UTF-8 character */
			map->description[length++] = '?';
		}
	}
	while (length > 0 &&
		(map->description[length - 1] == ' ' ||
			map->description[length - 1] == '\r' ||
			map->description[length - 1] == '\n'))
	{
		length--;
	}
	map->description[length] = 0;

	if (!length)
	{
		csmemcpy(map->description, default_description, sizeof(default_description));
	}
	map->description_read = length > 0;

	return;
}

/* Adds the file `name`.`extension` from `folder` if it is a CE map that
isn't listed yet. */
static void custom_edition_map_add(
	char const *folder,
	char const *name,
	char const *extension)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	struct custom_edition_map *map;
	boolean campaign;
	short map_index;

	if (csstrcasecmp(extension, "map"))
	{
		return;
	}
	for (map_index = 0; map_index < globals->map_count; map_index++)
	{
		if (!csstrcasecmp(globals->maps[map_index].name, name))
		{
			return;
		}
	}
	for (map_index = 0; map_index < globals->campaign_count; map_index++)
	{
		if (!csstrcasecmp(globals->campaigns[map_index].name, name))
		{
			return;
		}
	}
	/* campaign maps go to co-op; anything that isn't multiplayer either is skipped */
	campaign = custom_edition_cache_campaign(name);
	if (!campaign && !custom_edition_cache_multiplayer(name))
	{
		return;
	}
	if (campaign)
	{
		if (globals->campaign_count == MAXIMUM_CUSTOM_EDITION_CAMPAIGNS || csstrlen(name) > MAXIMUM_MAP_NAME_LENGTH)
		{
			error(_error_silent, "custom edition: the campaign map '%s' is not listed (too many, or too long a name)",
				name);
			return;
		}
		map = &globals->campaigns[globals->campaign_count++];
	}
	else if (csstrlen(name) > MAXIMUM_MAP_NAME_LENGTH)
	{
		error(
			_error_silent,
			"custom edition: the map '%s' is not in the level list: its name is longer than %d characters",
			name,
			MAXIMUM_MAP_NAME_LENGTH);
		return;
	}
	else if (globals->map_count == CUSTOM_EDITION_MAPS_MAXIMUM)
	{
		error(
			_error_silent,
			"custom edition: the map '%s' is not in the level list, which holds %d of them",
			name,
			CUSTOM_EDITION_MAPS_MAXIMUM);
		return;
	}
	else
	{
		map = &globals->maps[globals->map_count++];
	}
	csmemset(map, 0, sizeof(*map));
	csstrcpy(map->name, name);
	csstrncpy(map->folder, folder, sizeof(map->folder) - 1);
	map->pc = !campaign && !csstrcasecmp(folder, CUSTOM_EDITION_PC_MAP_DIRECTORY);
	csprintf(map->level_name, "%s%s", CUSTOM_EDITION_LEVEL_NAME_PREFIX, name);
	display_name_make(name, map->pc, map->display_name);
	custom_edition_map_description_read(map);
	/* (named for its hash by a build of before: its name given back,
	map_share_files.c) */
	map_share_map_found(folder, name);

	return;
}

static int custom_edition_map_compare(
	void const *first,
	void const *second)
{
	return (int)csstrcasecmp(
		((struct custom_edition_map const *)first)->name,
		((struct custom_edition_map const *)second)->name);
}

/* Versions of a map listed (its name the same but for the hash a version's
file name ends in, a download's beside another of its name): the first in
the order the maps are sorted by name (the one of the name alone, which
sorts before them) shown by its name, the others with (2), (3) and so on. */
static void custom_edition_maps_tell_versions_apart(
	struct custom_edition_map *maps,
	short count)
{
	short index;

	for (index = 0; index < count; index++)
	{
		struct custom_edition_map *map = &maps[index];
		short length = map_share_name_base_length(map->name);
		short number = 1;
		short other;

		if (!map_share_name_hashed(map->name))
			continue;
		for (other = 0; other < count; other++)
		{
			struct custom_edition_map const *version = &maps[other];

			if (other == index || map_share_name_base_length(version->name) != length ||
				_strnicmp(version->name, map->name, length))
			{
				continue;
			}
			if (other < index)
				number++;
		}
		if (number > 1)
		{
			char suffix[8];
			short end = 0;
			short character;

			while (map->display_name[end])
				end++;
			csprintf(suffix, " (%d)", (int)MIN(number, 999));
			for (character = 0; suffix[character] && end + 1 < NUMBEROF(map->display_name); character++)
				map->display_name[end++] = (wchar_t)suffix[character];
			map->display_name[end] = 0;
		}
	}
}

/* whether the first menu name sorts after the second (case aside) */
static boolean display_name_after(
	wchar_t const *first,
	wchar_t const *second)
{
	for (;; first++, second++)
	{
		wchar_t a = *first >= 'a' && *first <= 'z' ? (wchar_t)(*first - 'a' + 'A') : *first;
		wchar_t b = *second >= 'a' && *second <= 'z' ? (wchar_t)(*second - 'a' + 'A') : *second;

		if (a != b || !a)
			return a > b;
	}
}

/* The map lists' parts: multi_maps' (by their menu names, as Halo PC's list
has them), the Digsite maps (the first of each name, multiplayer or
campaign), and the rest. */
static void custom_edition_maps_sort_into_lists(
	void)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	short map_index;
	short index;

	for (index = 0; index < NUMBER_OF_DIGSITE_MAPS; index++)
	{
		struct custom_edition_map *missing = &globals->digsite_missing[index];

		globals->digsite[index] = NULL;
		missing->name[0] = 0;
		csstrncpy(missing->name, digsite_maps[index].file, sizeof(missing->name) - 1);
		missing->level_name[0] = 0;
		ustrcpy(missing->display_name, digsite_maps[index].name);
		csmemcpy(missing->description, digsite_missing_description, sizeof(digsite_missing_description));
	}
	globals->listed_count = 0;
	globals->pc_count = 0;
	for (map_index = 0; map_index < globals->map_count; map_index++)
	{
		struct custom_edition_map const *map = &globals->maps[map_index];
		short digsite = digsite_map_index(map->name);

		if (map->pc)
		{
			/* (put in its place by its menu name) */
			for (index = globals->pc_count++;
				index > 0 && display_name_after(globals->maps[globals->pc[index - 1]].display_name, map->display_name);
				index--)
			{
				globals->pc[index] = globals->pc[index - 1];
			}
			globals->pc[index] = map_index;
			continue;
		}
		if (digsite != NONE && !globals->digsite[digsite])
		{
			globals->digsite[digsite] = &globals->maps[map_index];
			continue;
		}
		globals->listed[globals->listed_count++] = map_index;
	}
	for (map_index = 0; map_index < globals->campaign_count; map_index++)
	{
		short digsite = digsite_map_index(globals->campaigns[map_index].name);

		if (digsite != NONE && !globals->digsite[digsite])
			globals->digsite[digsite] = &globals->campaigns[map_index];
	}

	return;
}

static void custom_edition_maps_look_for(
	void)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	/* (each map's own folder first, so its copy there wins: multi_maps' Halo
	PC maps (all of them), digsite_maps' Digsite maps (those alone)) */
	char const *folders[] = { CUSTOM_EDITION_PC_MAP_DIRECTORY, CUSTOM_EDITION_DIGSITE_MAP_DIRECTORY,
		CUSTOM_EDITION_MAP_DIRECTORY, CUSTOM_EDITION_INSTALL_MAP_DIRECTORY };
	short const folder_homes[] = { NONE, _custom_edition_maps_home_digsite, NONE, NONE };
	short folder_index;

	custom_edition_maps_forget();
	globals->looked_for = TRUE;
	globals->multiplayer_made = FALSE;
	/* (the Xbox's in multi_maps as <name>_xbox.map,
	custom_edition_original_map_file; Custom Edition maps or none) */
	globals->original_count = 0;
	{
		short xbox_index;

		for (xbox_index = 0; xbox_index < NUMBER_OF_ORIGINAL_MAPS; xbox_index++)
		{
			char path[256];

			if (custom_edition_original_map_file(xbox_maps[xbox_index].file, path, sizeof(path)))
			{
				wchar_t *name = globals->original_names[xbox_index];

				ustrncpy(name, xbox_maps[xbox_index].name, MAXIMUM_MAP_NAME_LENGTH);
				name[MAXIMUM_MAP_NAME_LENGTH] = 0;
				ustrcat(name, L" (Classic)");
				globals->original[globals->original_count++] = xbox_index;
			}
		}
	}
	if (!halo_custom_edition_tag_cache())
	{
		return;
	}

	/* the game's custom_maps first, so its copy wins */
	for (folder_index = 0; folder_index < NUMBEROF(folders); folder_index++)
	{
		char pattern[MAXIMUM_FILENAME_LENGTH + 1];
		WIN32_FIND_DATAA data;
		HANDLE find;

		/* (the install's only when there is one) */
		if (folders[folder_index] == CUSTOM_EDITION_INSTALL_MAP_DIRECTORY && !custom_edition_install_present())
			continue;
		/* A folder is listed through a handle of its own, never with
		find_files_start and find_files_next: those keep their place in one
		global, and as the game starts a thread of the menus' deletes the
		files of folders with them (perform_filesystem_initialization in
		ui_widget.c, directory_create_or_delete_contents). A listing here at
		the same time takes that one's place, and the thread then deletes
		the files of this folder: the maps. */
		csprintf(pattern, "%s*.*", folders[folder_index]);
		find = FindFirstFileA(pattern, &data);
		if (find == INVALID_HANDLE_VALUE)
			continue;
		do
		{
			char name[sizeof(data.cFileName) + 1];
			char *extension;

			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			csmemcpy(name, data.cFileName, sizeof(data.cFileName));
			name[sizeof(data.cFileName)] = 0;
			extension = strrchr(name, '.');
			if (extension)
			{
				*extension++ = 0;
				/* (digsite_maps' Digsite maps alone) */
				if (folder_homes[folder_index] != NONE && custom_edition_maps_home(name) != folder_homes[folder_index])
					continue;
				custom_edition_map_add(folders[folder_index], name, extension);
			}
		} while (FindNextFileA(find, &data));
		CloseHandle(find);
	}
	qsort(globals->maps, globals->map_count, sizeof(globals->maps[0]), custom_edition_map_compare);
	qsort(globals->campaigns, globals->campaign_count, sizeof(globals->campaigns[0]), custom_edition_map_compare);
	custom_edition_maps_tell_versions_apart(globals->maps, globals->map_count);
	custom_edition_maps_tell_versions_apart(globals->campaigns, globals->campaign_count);
	custom_edition_maps_sort_into_lists();
	error(_error_silent, "custom edition: %d multiplayer maps for the level list, %d campaign maps", globals->map_count,
		globals->campaign_count);

	return;
}

/* Loads the map's picture as a texture. Returns NULL if it has none, or if
it can't be shown (logged). */
static struct bitmap_data *custom_edition_map_picture_read(
	struct custom_edition_map const *map)
{
	char path[MAXIMUM_FILENAME_LENGTH + 1];
	struct bmp_file_picture picture;
	enum bmp_file_status status;
	struct bitmap_data *bitmap = NULL;
	uint8_t *file = NULL;
	FILE *stream;
	long size = 0;

	csprintf(path, "%s%s%s", map->folder, map->name, PICTURE_EXTENSION);
	stream = fopen(path, "rb");
	/* (a map named for its hash: its picture named without it too) */
	if (!stream && map_share_name_hashed(map->name))
	{
		csprintf(path, "%s%.*s%s", map->folder, (int)map_share_name_base_length(map->name), map->name,
			PICTURE_EXTENSION);
		stream = fopen(path, "rb");
	}
	if (!stream)
	{
		return NULL;
	}
	if (fseek(stream, 0, SEEK_END) == 0 &&
		(size = ftell(stream)) >= 0 &&
		size <= MAXIMUM_PICTURE_FILE_BYTES &&
		fseek(stream, 0, SEEK_SET) == 0)
	{
		/* +1 so an empty file doesn't look like a failed allocation */
		file = malloc((size_t)size + 1);
		if (file && fread(file, 1, (size_t)size, stream) != (size_t)size)
		{
			free(file);
			file = NULL;
		}
	}
	fclose(stream);
	if (!file)
	{
		error(_error_silent, "custom edition: the picture '%s' could not be read", path);
		return NULL;
	}

	status = bmp_file_open(file, (uint32_t)size, &picture);
	if (status == _bmp_file_status_ok)
	{
		bitmap = bitmap_2d_new(PICTURE_TEXTURE_SIZE, PICTURE_TEXTURE_SIZE, 0, _bitmap_format_a8r8g8b8);
		if (bitmap && bitmap->base_address)
		{
			bmp_file_fit(
				file,
				&picture,
				LEVEL_PICTURE_SHAPE_WIDTH,
				LEVEL_PICTURE_SHAPE_HEIGHT,
				PICTURE_TEXTURE_SIZE,
				PICTURE_TEXTURE_SIZE,
				XBOX_POINTER(uint32_t, bitmap->base_address));
			bitmap_rebuild(bitmap);
		}
		if (bitmap && !bitmap->hardware_format)
		{
			bitmap_delete(bitmap);
			bitmap = NULL;
		}
	}
	else
	{
		error(_error_silent, "custom edition: the picture '%s' cannot be shown: %s", path, bmp_file_status_describe(status));
	}
	free(file);

	return bitmap;
}

/* the CE map with this display index, or NULL */
static struct custom_edition_map *custom_edition_map_get(
	short display_index)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	short map_index = display_index - FIRST_DISPLAY_INDEX;
	short campaign_index = display_index - FIRST_CUSTOM_CAMPAIGN_DISPLAY_INDEX;
	short digsite_index = display_index - FIRST_DIGSITE_DISPLAY_INDEX;

	if (digsite_index >= 0 && digsite_index < NUMBER_OF_DIGSITE_MAPS)
	{
		if (!globals->looked_for)
			custom_edition_maps_look_for();
		return globals->digsite[digsite_index] ? globals->digsite[digsite_index] :
			&globals->digsite_missing[digsite_index];
	}
	if (campaign_index >= 0 && campaign_index < globals->campaign_count)
		return &globals->campaigns[campaign_index];
	return map_index >= 0 && map_index < globals->map_count ? &globals->maps[map_index] : NULL;
}

/* the stock campaign level with this display index, or NONE */
static short campaign_level_get(
	short display_index)
{
	short level = display_index - FIRST_CAMPAIGN_DISPLAY_INDEX;

	return level >= 0 && level < NUMBER_OF_SINGLE_PLAYER_LEVELS ? level : NONE;
}

/* the stock campaign level a level name refers to (levels\a10\a10 is the
first), or NONE */
static short campaign_level_from_name(
	char const *level_name)
{
	char const *name = tag_name_strip_path(level_name);
	short level;

	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
	{
		if (!csstrcasecmp(tag_name_strip_path(main_get_solo_level_name(level)), name))
			return level;
	}

	return NONE;
}

/* ---------- public code */

char **custom_edition_maps_level_list(
	char **xbox_levels,
	short xbox_level_count,
	short *level_count)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	short level_index;
	short map_index;

	globals->xbox_level_count = MIN(xbox_level_count, MAXIMUM_XBOX_LEVELS);
	for (level_index = 0; level_index < globals->xbox_level_count; level_index++)
	{
		globals->levels[level_index] = xbox_levels[level_index];
	}
	/* (the menus ask for the list every tick, so the folders are scanned
	once, and again when a map list opens: custom_edition_maps_look_again) */
	if (!globals->looked_for)
	{
		custom_edition_maps_look_for();
	}
	for (map_index = 0; map_index < globals->map_count; map_index++)
	{
		globals->levels[globals->xbox_level_count + map_index] = globals->maps[map_index].level_name;
	}
	*level_count = globals->xbox_level_count + globals->map_count;

	return globals->levels;
}

void custom_edition_maps_look_again(
	void)
{
	custom_edition_maps_globals.looked_for = FALSE;
	/* (a network game's map's file, found again) */
	custom_edition_map_paths_forget();

	return;
}

short custom_edition_maps_level_display_index(
	short level_index)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	short map_index = level_index - globals->xbox_level_count;

	return map_index >= 0 && map_index < globals->map_count ? FIRST_DISPLAY_INDEX + map_index : level_index;
}

short custom_edition_maps_display_index(
	char const *level_name)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	char const *name = tag_name_strip_path(level_name);
	char path[MAP_SHARE_PATH_SIZE];
	short map_index;

	/* (only a custom_maps\ name is a CE map's; any other is the game's own,
	a stock campaign level or none of these) */
	if (!custom_edition_level_name(level_name))
	{
		short level = campaign_level_from_name(level_name);

		return level != NONE ? FIRST_CAMPAIGN_DISPLAY_INDEX + level : NONE;
	}
	if (!globals->looked_for)
	{
		custom_edition_maps_look_for();
	}
	/* (the map by its file: a network game's of its version, which may be
	one of another name, <name>_<hash>.map, shown with its number) */
	if (custom_edition_map_file_path(level_name, path))
	{
		char const *file_name = tag_name_strip_path(path);
		short length = (short)csstrlen(file_name) - (short)(sizeof(".map") - 1);

		for (map_index = 0; length > 0 && map_index < globals->map_count; map_index++)
		{
			if ((short)csstrlen(globals->maps[map_index].name) == length &&
				!_strnicmp(globals->maps[map_index].name, file_name, length))
			{
				return FIRST_DISPLAY_INDEX + map_index;
			}
		}
		for (map_index = 0; length > 0 && map_index < globals->campaign_count; map_index++)
		{
			if ((short)csstrlen(globals->campaigns[map_index].name) == length &&
				!_strnicmp(globals->campaigns[map_index].name, file_name, length))
			{
				return FIRST_CUSTOM_CAMPAIGN_DISPLAY_INDEX + map_index;
			}
		}
	}
	for (map_index = 0; map_index < globals->map_count; map_index++)
	{
		if (!csstrcasecmp(globals->maps[map_index].name, name))
		{
			return FIRST_DISPLAY_INDEX + map_index;
		}
	}
	for (map_index = 0; map_index < globals->campaign_count; map_index++)
	{
		if (!csstrcasecmp(globals->campaigns[map_index].name, name))
		{
			return FIRST_CUSTOM_CAMPAIGN_DISPLAY_INDEX + map_index;
		}
	}

	return NONE;
}

/* whether the map is one of the CE campaigns (a Digsite map's too) */
static boolean custom_edition_map_campaign(
	struct custom_edition_map const *map)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;

	return map && map >= globals->campaigns && map < globals->campaigns + globals->campaign_count;
}

boolean custom_edition_maps_campaign(
	short display_index)
{
	return campaign_level_get(display_index) != NONE || custom_edition_map_campaign(custom_edition_map_get(display_index));
}

short custom_edition_maps_campaign_level(
	short display_index)
{
	return campaign_level_get(display_index);
}

boolean custom_edition_maps_level_campaign(
	char const *level_name)
{
	return level_name && custom_edition_maps_campaign(custom_edition_maps_display_index(level_name));
}

short custom_edition_maps_count(
	boolean campaign)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;

	if (!globals->looked_for)
	{
		custom_edition_maps_look_for();
	}

	return campaign ? globals->campaign_count : globals->listed_count;
}

short custom_edition_maps_display_index_of(
	boolean campaign,
	short index)
{
	if (index < 0 || index >= custom_edition_maps_count(campaign))
		return NONE;
	return campaign ? FIRST_CUSTOM_CAMPAIGN_DISPLAY_INDEX + index :
		FIRST_DISPLAY_INDEX + custom_edition_maps_globals.listed[index];
}

short custom_edition_maps_pc_count(
	void)
{
	if (!custom_edition_maps_globals.looked_for)
		custom_edition_maps_look_for();
	return custom_edition_maps_globals.pc_count;
}

short custom_edition_maps_pc_display_index(
	short index)
{
	return index < 0 || index >= custom_edition_maps_pc_count() ? NONE :
		FIRST_DISPLAY_INDEX + custom_edition_maps_globals.pc[index];
}

/* MULTIPLAYER's entries (custom_edition_maps_multiplayer_count): multi_maps'
Halo PC maps in their order, each followed by the Xbox's own of it if it is
in multi_maps, and the Xbox's without one of Halo PC's put in by name */
static void custom_edition_maps_multiplayer_make(
	void)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	boolean paired[NUMBER_OF_ORIGINAL_MAPS];
	short alone[NUMBER_OF_ORIGINAL_MAPS];
	short alone_count = 0, alone_next = 0;
	short index, original;

	globals->multiplayer_made = TRUE;
	globals->multiplayer_count = 0;
	csmemset(paired, 0, sizeof(paired));
	for (index = 0; index < globals->pc_count; index++)
	{
		short xbox_index = xbox_map_index(globals->maps[globals->pc[index]].name);

		if (xbox_index != NONE)
			paired[xbox_index] = TRUE;
	}
	/* (the Xbox's alone, by name) */
	for (original = 0; original < globals->original_count; original++)
	{
		short xbox_index = globals->original[original];

		if (paired[xbox_index])
			continue;
		for (index = alone_count++;
			index > 0 && display_name_after(globals->original_names[alone[index - 1]], globals->original_names[xbox_index]);
			index--)
		{
			alone[index] = alone[index - 1];
		}
		alone[index] = xbox_index;
	}
	for (index = 0; index < globals->pc_count; index++)
	{
		struct custom_edition_map const *map = &globals->maps[globals->pc[index]];
		short xbox_index = xbox_map_index(map->name);

		while (alone_next < alone_count &&
			display_name_after(map->display_name, globals->original_names[alone[alone_next]]))
		{
			globals->multiplayer[globals->multiplayer_count++] = (short)(-1 - alone[alone_next++]);
		}
		globals->multiplayer[globals->multiplayer_count++] = index;
		for (original = 0; xbox_index != NONE && original < globals->original_count; original++)
		{
			if (globals->original[original] == xbox_index)
				globals->multiplayer[globals->multiplayer_count++] = (short)(-1 - xbox_index);
		}
	}
	while (alone_next < alone_count)
		globals->multiplayer[globals->multiplayer_count++] = (short)(-1 - alone[alone_next++]);

	return;
}

short custom_edition_maps_multiplayer_count(
	void)
{
	if (!custom_edition_maps_globals.looked_for)
		custom_edition_maps_look_for();
	if (!custom_edition_maps_globals.multiplayer_made)
		custom_edition_maps_multiplayer_make();
	return custom_edition_maps_globals.multiplayer_count;
}

short custom_edition_maps_multiplayer_entry(
	short entry,
	short *pc_index)
{
	short value;

	*pc_index = NONE;
	if (entry < 0 || entry >= custom_edition_maps_multiplayer_count())
		return NONE;
	value = custom_edition_maps_globals.multiplayer[entry];
	if (value < 0)
		return (short)(-1 - value);
	*pc_index = value;
	return NONE;
}

short custom_edition_maps_multiplayer_entry_of(
	short pc_index,
	short xbox_level)
{
	short entry;
	short count = custom_edition_maps_multiplayer_count();

	for (entry = 0; entry < count; entry++)
	{
		short value = custom_edition_maps_globals.multiplayer[entry];

		if (xbox_level != NONE ? value == -1 - xbox_level : value == pc_index)
			return entry;
	}
	return NONE;
}

wchar_t const *custom_edition_maps_original_name(
	short xbox_level)
{
	return xbox_level < 0 || xbox_level >= NUMBER_OF_ORIGINAL_MAPS ? L"" :
		custom_edition_maps_globals.original_names[xbox_level];
}

short custom_edition_maps_digsite_count(
	void)
{
	return NUMBER_OF_DIGSITE_MAPS;
}

short custom_edition_maps_digsite_display_index(
	short index)
{
	return index < 0 || index >= NUMBER_OF_DIGSITE_MAPS ? NONE : FIRST_DIGSITE_DISPLAY_INDEX + index;
}

boolean custom_edition_maps_installed(
	short display_index)
{
	struct custom_edition_map *map = custom_edition_map_get(display_index);

	return campaign_level_get(display_index) != NONE || (map && map->level_name[0]);
}

short custom_edition_maps_level_index(
	short display_index)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	struct custom_edition_map *map = custom_edition_map_get(display_index);

	if (!map || map < globals->maps || map >= globals->maps + globals->map_count)
		return NONE;
	return (short)(globals->xbox_level_count + (map - globals->maps));
}

short custom_edition_maps_entry(
	short display_index,
	short *kind)
{
	struct custom_edition_maps_globals *globals = &custom_edition_maps_globals;
	struct custom_edition_map *map = custom_edition_map_get(display_index);
	short index;

	*kind = NONE;
	if (!map)
		return NONE;
	for (index = 0; index < globals->pc_count; index++)
	{
		if (&globals->maps[globals->pc[index]] == map)
		{
			*kind = _custom_edition_maps_kind_pc;
			return index;
		}
	}
	for (index = 0; index < NUMBER_OF_DIGSITE_MAPS; index++)
	{
		if (globals->digsite[index] == map)
		{
			*kind = _custom_edition_maps_kind_digsite;
			return index;
		}
	}
	for (index = 0; index < globals->listed_count; index++)
	{
		if (&globals->maps[globals->listed[index]] == map)
		{
			*kind = _custom_edition_maps_kind_listed;
			return index;
		}
	}
	return NONE;
}

char const *custom_edition_maps_level_name(
	short display_index)
{
	struct custom_edition_map *map = custom_edition_map_get(display_index);
	short level = campaign_level_get(display_index);

	if (level != NONE)
	{
		return main_get_solo_level_name(level);
	}

	return map && map->level_name[0] ? map->level_name : NULL;
}

boolean custom_edition_maps_pc(
	short display_index)
{
	struct custom_edition_map *map = custom_edition_map_get(display_index);

	return map && map->pc;
}

wchar_t *custom_edition_maps_name(
	short display_index)
{
	struct custom_edition_map *map = custom_edition_map_get(display_index);
	short level = campaign_level_get(display_index);

	if (level != NONE)
	{
		return campaign_level_names[level];
	}

	return map ? map->display_name : NULL;
}

wchar_t *custom_edition_maps_description(
	short display_index)
{
	struct custom_edition_map *map = custom_edition_map_get(display_index);

	if (campaign_level_get(display_index) != NONE)
	{
		return campaign_level_description;
	}
	if (!map)
	{
		return NULL;
	}
	/* (a Digsite map without a <name>.txt: its kind, or that it is not
	installed) */
	if (digsite_map_index(map->name) != NONE && !map->description_read)
	{
		return (wchar_t *)(!map->level_name[0] ? digsite_missing_description :
			custom_edition_map_campaign(map) ? digsite_campaign_description : digsite_multiplayer_description);
	}
	/* (one of Halo PC's own without a <name>.txt, or of its versions of the
	Xbox's in multi_maps: the PC menus' words for it, its own following the
	Xbox maps' there; never past their end, which would ask this function
	again: text_group.c's fallback_string) */
	if (!map->description_read &&
		(stock_map_index(map->name) != NONE || (map->pc && xbox_map_index(map->name) != NONE)))
	{
		long tag_index = tag_loaded(UNICODE_STRING_LIST_TAG, PC_LEVEL_DESCRIPTIONS_TAG_NAME);
		short string_index = stock_map_index(map->name) != NONE ?
			(short)(FIRST_PC_STOCK_MAP_STRING + stock_map_index(map->name)) : xbox_map_index(map->name);

		if (tag_index != NONE && string_index < unicode_string_list_definition_get(tag_index)->strings.count)
			return unicode_string_list_get_string(tag_index, string_index);
		return stock_description;
	}

	return map->description;
}

struct bitmap_data *custom_edition_maps_picture(
	long bitmap_tag_index,
	short *frame_index)
{
	struct custom_edition_map *map;
	short level = campaign_level_get(*frame_index);

	if (level != NONE &&
		bitmap_tag_index != NONE &&
		!csstrcasecmp(tag_get_name(bitmap_tag_index), LEVEL_PICTURES_TAG_NAME))
	{
		long pictures = tag_loaded('bitm', CAMPAIGN_LEVEL_PICTURES_TAG_NAME);

		*frame_index = UNKNOWN_LEVEL_FRAME;
		return pictures != NONE ? bitmap_group_get_bitmap_from_sequence(pictures, 0, level) : NULL;
	}
	if (*frame_index < FIRST_DISPLAY_INDEX ||
		bitmap_tag_index == NONE ||
		csstrcasecmp(tag_get_name(bitmap_tag_index), LEVEL_PICTURES_TAG_NAME))
	{
		return NULL;
	}

	map = custom_edition_map_get(*frame_index);
	*frame_index = UNKNOWN_LEVEL_FRAME;
	/* (a Digsite map not installed: the unknown level's) */
	if (!map || !map->level_name[0])
	{
		return NULL;
	}
	if (!map->picture_read)
	{
		map->picture = custom_edition_map_picture_read(map);
		map->picture_read = TRUE;
	}
	/* (Halo PC's version of an Xbox map without a <name>.bmp: the Xbox
	level's frame) */
	if (!map->picture && map->pc && xbox_map_index(map->name) != NONE)
	{
		*frame_index = xbox_map_index(map->name);
	}

	return map->picture;
}
