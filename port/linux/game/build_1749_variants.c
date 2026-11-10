/*
BUILD_1749_VARIANTS.C

Build 1749's own game types. Its executable built twenty (in this order,
which its menus' 19 names in ui\default_multiplayer_game_setting_names
follow: Iron CTF has no name there), not this build's 26: Dark Stalker,
Killer, Hunter, Magic, Crazy Ball and Assault are 1749's alone, and the
rest are set otherwise than this build's of the same name. halo1749.exe
had made the menus' default game types with this build's builders under
1749's names, so its "Slayer" was this build's fifth, Endurance, with five
lives, odd man out and a warthog (playlist_profile.c).

The values are the ones 1749's builders write (1749betaP.xbe, the table
of builders at 0x2E34A0), into a game variant laid out as this build's:
each game type's description (its name cut to 11 characters, as 1749's
ustrncpy left it), engine, universal options and game type options.
*/

#include "cseries.h"
#include "game/game_engine.h"
#include "math/real_math.h"
#include "text/unicode.h"
#include "build_1749_variants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (port/linux/src/platform.h's: d:\ is the data folder, 1749\) */
void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);

struct build_1749_variant
{
	wchar_t const *description;
	long game_engine_index;
	boolean teams;
	unsigned long flags;
	long goal_radar;
	boolean odd_man_out;
	long respawn_time_growth;
	long respawn_time;
	long suicide_penalty;
	long lives;
	real health;
	long score_to_win;
	long weapon_set;
	long vehicle_set;
	/* the game type's own options (union game_engine_variant), as bytes */
	byte game_engine_variant[sizeof(union game_engine_variant)];
	word variant_flags;
};

static struct build_1749_variant const build_1749_variants[NUMBER_OF_BUILD_1749_VARIANTS] =
{
	{ L"Dark Stalke", game_engine_slayer, 0, 0x12, 1, 0, 0, 150, 150, 0, 1.0f, 10, 0, 0,
		{ 0x01, 0x01, 0x01 }, 1 },
	{ L"Race", game_engine_race, 0, 0x03, 1, 0, 0, 30, 270, 0, 1.0f, 3, 0, 0,
		{ 0 }, 1 },
	{ L"Team Race", game_engine_race, 1, 0x03, 1, 0, 0, 30, 270, 0, 1.0f, 3, 0, 0,
		{ 0 }, 1 },
	{ L"Rally", game_engine_race, 0, 0x03, 1, 0, 0, 30, 270, 0, 1.0f, 15, 0, 0,
		{ 0x02 }, 1 },
	{ L"Slayer", game_engine_slayer, 0, 0x03, 0, 0, 0, 30, 270, 0, 1.0f, 25, 0, 0,
		{ 0 }, 1 },
	{ L"Team Slayer", game_engine_slayer, 1, 0x03, 0, 0, 0, 30, 270, 0, 1.0f, 50, 0, 0,
		{ 0 }, 1 },
	{ L"Killer", game_engine_slayer, 0, 0x03, 0, 0, 0, 30, 270, 0, 1.0f, 10, 0, 0,
		{ 0x00, 0x00, 0x01 }, 1 },
	{ L"Elimination", game_engine_slayer, 0, 0x03, 0, 0, 0, 30, 270, 1, 1.0f, 10, 0, 0,
		{ 0 }, 1 },
	{ L"Hunter", game_engine_oddball, 0, 0x03, 2, 0, 0, 150, 150, 0, 1.0f, 10, 0, 0,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01 }, 1 },
	{ L"Stalker", game_engine_oddball, 0, 0x03, 2, 0, 0, 150, 150, 0, 1.0f, 10, 0, 0,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01 }, 1 },
	{ L"Team Oddbal", game_engine_oddball, 1, 0x03, 1, 0, 0, 150, 150, 0, 1.0f, 2, 0, 0,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }, 1 },
	{ L"Accumulatio", game_engine_oddball, 0, 0x03, 2, 0, 0, 150, 150, 0, 1.0f, 5, 0, 0,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10 }, 1 },
	{ L"Magic", game_engine_oddball, 0, 0x03, 0, 0, 0, 150, 150, 0, 1.0f, 2, 0, 0,
		{ 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01 }, 1 },
	{ L"Oddball", game_engine_oddball, 0, 0x03, 0, 0, 0, 150, 150, 0, 1.0f, 2, 0, 0,
		{ 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }, 1 },
	{ L"Crazyball", game_engine_oddball, 0, 0x07, 1, 0, 0, 150, 150, 0, 1.0f, 2, 0, 0,
		{ 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }, 1 },
	{ L"Assault", game_engine_ctf, 1, 0x03, 1, 0, 150, 150, 150, 0, 1.0f, 3, 0, 0,
		{ 0x01 }, 1 },
	{ L"Capture the", game_engine_ctf, 1, 0x03, 1, 0, 0, 300, 150, 0, 1.0f, 3, 0, 0,
		{ 0 }, 1 },
	{ L"King of the", game_engine_king, 0, 0x03, 1, 0, 0, 150, 150, 0, 1.0f, 2, 0, 0,
		{ 0x01 }, 1 },
	{ L"Team King o", game_engine_king, 1, 0x03, 1, 0, 0, 150, 150, 0, 1.0f, 2, 0, 0,
		{ 0 }, 1 },
	{ L"Iron CTF", game_engine_ctf, 1, 0x03, 1, 0, 0, 300, 150, 0, 3.0f, 3, 0, 0,
		{ 0 }, 1 },
};

static char const *const build_1749_variant_names[NUMBER_OF_BUILD_1749_VARIANTS] =
{
	"dark_stalker", "race", "team_race", "rally", "slayer", "team_slayer", "killer", "elimination",
	"hunter", "stalker", "team_oddball", "accumulation", "magic", "oddball", "crazyball", "assault",
	"ctf", "king", "team_king", "ironctf",
};

struct game_variant *build_1749_variant(
	struct game_variant *variant,
	long index)
{
	struct build_1749_variant const *source = &build_1749_variants[PIN(index, 0, NUMBER_OF_BUILD_1749_VARIANTS - 1)];

	csmemset(variant, 0, sizeof(*variant));
	ustrncpy(variant->human_readable_game_description, source->description,
		NUMBEROF(variant->human_readable_game_description) - 1);
	variant->game_engine_index = source->game_engine_index;
	variant->universal_variant.teams = source->teams;
	variant->universal_variant.flags = source->flags;
	variant->universal_variant.goal_radar = source->goal_radar;
	variant->universal_variant.odd_man_out = source->odd_man_out;
	variant->universal_variant.respawn_time_growth = source->respawn_time_growth;
	variant->universal_variant.respawn_time = source->respawn_time;
	variant->universal_variant.suicide_penalty = source->suicide_penalty;
	variant->universal_variant.lives = source->lives;
	variant->universal_variant.health = source->health;
	variant->universal_variant.score_to_win = source->score_to_win;
	variant->universal_variant.weapon_set = source->weapon_set;
	variant->universal_variant.vehicle_set = source->vehicle_set;
	csmemcpy(&variant->game_engine_variant, source->game_engine_variant, sizeof(variant->game_engine_variant));
	variant->flags = source->variant_flags;

	return variant;
}

long build_1749_variant_index(
	char const *name)
{
	long index;

	for (index = 0; index < NUMBER_OF_BUILD_1749_VARIANTS; index++)
	{
		if (!strcmp(name, build_1749_variant_names[index]))
			return index;
	}
	return NONE;
}

/* ---------- the playlist

1749's executable (1749betaP.xbe's game_engine.c) read d:\playlist.txt
once: a line "version 1", then for each entry a line each of its map, its
game type's name (build_1749_variant_index's), and five numbers: the
fewest and most players it is for, the range of playlist types it is for,
and its weight (1 to 255). Without the file it had one entry, Slayer on
Derelict, for 0 to 10 players and types 1 to 16, of weight 100. Its next
stage was the entry, of those for the players and the type, with the most
of a random 512 to 1023 times its weight (a sixteenth of that where the
players are fewer or more than it is for). */

enum
{
	MAXIMUM_BUILD_1749_PLAYLIST_ENTRIES = 256,
	BUILD_1749_PLAYLIST_LINE_SIZE = 1024,
};

struct build_1749_playlist_entry
{
	char map_name[64];
	char variant_name[32];
	long minimum_players;
	long maximum_players;
	long minimum_type;
	long maximum_type;
	long weight;
};

static struct build_1749_playlist_entry const build_1749_playlist_default =
{
	"levels\\test\\carousel\\carousel", "slayer", 0, 10, 1, 16, 100
};

static struct build_1749_playlist_entry *build_1749_playlist = NULL;
static long build_1749_playlist_count = 0;
static boolean build_1749_playlist_read = FALSE;

static boolean build_1749_playlist_line(FILE *file, char *line)
{
	return fgets(line, BUILD_1749_PLAYLIST_LINE_SIZE, file) != NULL;
}

static boolean build_1749_playlist_number(FILE *file, char *line, long *value)
{
	int number;

	if (!build_1749_playlist_line(file, line) || sscanf(line, "%d", &number) != 1)
		return FALSE;
	*value = number;
	return TRUE;
}

static void build_1749_playlist_load(void)
{
	char path[1024];
	char line[BUILD_1749_PLAYLIST_LINE_SIZE];
	FILE *file;

	build_1749_playlist_read = TRUE;
	platform_translate_path("d:\\playlist.txt", path, sizeof(path));
	file = fopen(path, "r");
	if (!file)
		return;
	if (build_1749_playlist_line(file, line) && !strncmp(line, "version 1", 9))
	{
		build_1749_playlist = calloc(MAXIMUM_BUILD_1749_PLAYLIST_ENTRIES, sizeof(*build_1749_playlist));
		while (build_1749_playlist && build_1749_playlist_count < MAXIMUM_BUILD_1749_PLAYLIST_ENTRIES)
		{
			struct build_1749_playlist_entry entry;
			char map_name[BUILD_1749_PLAYLIST_LINE_SIZE];
			char variant_name[BUILD_1749_PLAYLIST_LINE_SIZE];

			csmemset(&entry, 0, sizeof(entry));
			if (!build_1749_playlist_line(file, line) || sscanf(line, "%1023s", map_name) != 1 ||
				!build_1749_playlist_line(file, line) || sscanf(line, "%1023s", variant_name) != 1 ||
				!build_1749_playlist_number(file, line, &entry.minimum_players) ||
				!build_1749_playlist_number(file, line, &entry.maximum_players) ||
				!build_1749_playlist_number(file, line, &entry.minimum_type) ||
				!build_1749_playlist_number(file, line, &entry.maximum_type) ||
				!build_1749_playlist_number(file, line, &entry.weight))
			{
				break;
			}
			strncpy(entry.map_name, map_name, sizeof(entry.map_name) - 1);
			strncpy(entry.variant_name, variant_name, sizeof(entry.variant_name) - 1);
			/* (1749 asserted a weight of 1 to 255) */
			entry.weight = PIN(entry.weight, 1, 255);
			build_1749_playlist[build_1749_playlist_count++] = entry;
		}
	}
	fclose(file);
}

void build_1749_playlist_next(
	long minimum,
	long maximum,
	long type,
	char *map_name,
	long map_name_size,
	struct game_variant *variant)
{
	struct build_1749_playlist_entry const *chosen = NULL;
	long best = NONE;
	long index;
	long variant_index;

	if (!build_1749_playlist_read)
		build_1749_playlist_load();
	for (index = 0; index < build_1749_playlist_count; index++)
	{
		struct build_1749_playlist_entry const *entry = &build_1749_playlist[index];
		long value;

		if (type < entry->minimum_type || type > entry->maximum_type ||
			maximum < entry->minimum_players || minimum > entry->maximum_players)
		{
			continue;
		}
		value = (long)local_random_range(0x200, 0x400) * entry->weight;
		if (minimum < entry->minimum_players)
			value /= 16;
		if (maximum > entry->maximum_players)
			value /= 16;
		if (value > best)
		{
			best = value;
			chosen = entry;
		}
	}
	if (!chosen)
		chosen = build_1749_playlist_count > 0 ? &build_1749_playlist[0] : &build_1749_playlist_default;

	strncpy(map_name, chosen->map_name, (size_t)map_name_size - 1);
	map_name[map_name_size - 1] = 0;
	variant_index = build_1749_variant_index(chosen->variant_name);
	build_1749_variant(variant, variant_index != NONE ? variant_index : _build_1749_variant_slayer);
}
