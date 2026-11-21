/*
ACHIEVEMENTS.C

The Xbox-style achievements of Halo: Combat Evolved's campaign, earned in
the game and written to achievements.log in the save root's u folder, which
a companion program (tools/achievements) shows as they come.

Each record of the log is 16 bytes: the achievement's number (a 32-bit
little-endian integer), 0, and when it was earned (a Windows FILETIME, 64
bits). Numbers 1 to 37 are the achievements, in the list's order
(ACHIEVEMENT_*); 100 + level * 4 + difficulty records a level finished on a
difficulty, from which the campaign's three are decided. A number is written
once.

What a level's achievements look for is kept in the game state, so a revert
to a checkpoint undoes what was done since (a mistake, a kill counted). It
is decided where the game runs: alone, or on the host of network co-op,
whose clients' players' achievements it sends them (network_coop.c).
*/

/* ---------- headers */

#include "cseries.h"
#include "achievements.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/equipment.h"
#include "items/equipment_definitions.h"
#include "items/weapons.h"
#include "main/main.h"
#include "memory/data.h"
#include "network_coop.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "saved games/game_state.h"
#include "tag_files/tag_files.h"
#include "units/units.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* port/linux/src/port_config.c's */
int config_boolean(char const *name);

/* ---------- constants */

/* the achievements' numbers in the log, in the list's order */
enum
{
	ACHIEVEMENT_PILLAR_OF_AUTUMN = 1,
	ACHIEVEMENT_HALO,
	ACHIEVEMENT_TRUTH_AND_RECONCILIATION,
	ACHIEVEMENT_SILENT_CARTOGRAPHER,
	ACHIEVEMENT_ASSAULT_ON_THE_CONTROL_ROOM,
	ACHIEVEMENT_GUILTY_SPARK,
	ACHIEVEMENT_LIBRARY,
	ACHIEVEMENT_TWO_BETRAYALS,
	ACHIEVEMENT_KEYES,
	ACHIEVEMENT_MAW,
	ACHIEVEMENT_BIRTH_OF_A_SPARTAN,
	ACHIEVEMENT_BELIEVE_IN_A_HERO,
	ACHIEVEMENT_LIVING_LEGEND,
	ACHIEVEMENT_STANDARD_OPERATING_BROCEDURE,
	ACHIEVEMENT_BROVERSHIELD,
	ACHIEVEMENT_BRO_HAMMER,
	ACHIEVEMENT_HES_UNSTOPPABLE,
	ACHIEVEMENT_OVERSHIELDS_ARE_FOR_SISSIES,
	ACHIEVEMENT_WALK_IT_OFF,
	ACHIEVEMENT_NO_FLY_ZONE,
	ACHIEVEMENT_HOW_PEDESTRIAN,
	ACHIEVEMENT_ALL_ACCORDING_TO_PLAN,
	ACHIEVEMENT_CLOSE_QUARTERS_COMBAT,
	ACHIEVEMENT_BEACHHEAD,
	ACHIEVEMENT_GRENADIER,
	ACHIEVEMENT_WRAITH_HUNTER,
	ACHIEVEMENT_ILL_BE_TAKING_THAT,
	ACHIEVEMENT_THIS_ONES_FOR_JENKINS,
	ACHIEVEMENT_BREAKING_QUARANTINE,
	ACHIEVEMENT_THAT_JUST_HAPPENED,
	ACHIEVEMENT_SPEED_READER,
	ACHIEVEMENT_LOOK_OUT_FOR_THE_LITTLE_GUYS,
	ACHIEVEMENT_LEAVE_IT_WHERE_IT_LAY,
	ACHIEVEMENT_TYING_UP_LOOSE_ENDS,
	ACHIEVEMENT_POPCORN,
	ACHIEVEMENT_THIS_SIDE_UP,
	ACHIEVEMENT_NEVER_TELL_ME_THE_ODDS,

	/* 100 + level * 4 + difficulty: a level finished on a difficulty */
	ACHIEVEMENT_LEVEL_FINISHED = 100,
	ACHIEVEMENT_NUMBERS = ACHIEVEMENT_LEVEL_FINISHED + 10 * NUMBER_OF_GAME_DIFFICULTY_LEVELS,
};

/* the campaign's levels (main_get_solo_level_from_name) */
enum
{
	_level_pillar_of_autumn,
	_level_halo,
	_level_truth_and_reconciliation,
	_level_silent_cartographer,
	_level_assault_on_the_control_room,
	_level_guilty_spark,
	_level_library,
	_level_two_betrayals,
	_level_keyes,
	_level_maw,
	NUMBER_OF_LEVELS
};

/* the players a level's record keeps (a campaign has at most four) */
#define ACHIEVEMENT_PLAYERS 16
/* the kinds of weapon a player may start a level with */
#define STARTING_WEAPONS 8

/* achievement_player.flags: what a player did in the level */
enum
{
	_achievement_health_damaged_bit,
	_achievement_died_bit,
	_achievement_took_overshield_bit,
	_achievement_took_health_pack_bit,
	_achievement_entered_vehicle_bit,
	_achievement_killed_grunt_bit,
	_achievement_took_other_weapon_bit,
	/* the player has picked up a weapon: what it started with is known */
	_achievement_picked_up_weapon_bit,
	/* the ones earned in the level as they happen, once */
	_achievement_no_fly_zone_bit,
	_achievement_wraith_hunter_bit,
	_achievement_taking_that_bit,
	_achievement_jenkins_bit,
	_achievement_popcorn_bit,
};

/* No-Fly Zone, Wraith Hunter, This One's for Jenkins, Popcorn.gif */
#define BANSHEES_DESTROYED 3
#define WRAITHS_DESTROYED 4
#define COMBAT_FORMS_KILLED 50
#define INFECTION_FORMS_KILLED 100
/* Close Quarters Combat: rounds left in the Sniper Rifle */
#define SNIPER_ROUNDS_LEFT 4
/* Speed Reader: the Library within 30 minutes */
#define SPEED_READER_TICKS (30 * 60 * TICKS_PER_SECOND)

/* the log in the save root's u folder (platform_translate_path) */
#define ACHIEVEMENTS_LOG "u:\\achievements.log"

/* a Windows FILETIME's 100 ns from 1601 to 1970 */
#define FILETIME_UNIX_EPOCH 116444736000000000ULL

/* ---------- structures */

struct achievement_player
{
	unsigned long flags;
	short banshees_destroyed;
	short wraiths_destroyed;
	short combat_forms_killed;
	short infection_forms_killed;
	/* the weapons the player started the level with (Leave It Where It
	Lay): every kind it held before it first picked one up (a level's
	scripts may hand out its loadout after the player appears) */
	long starting_weapons[STARTING_WEAPONS];
};

/* game state: saved and reverted with the checkpoints */
struct achievement_level
{
	/* the campaign level, NONE for any other map */
	short level;
	short difficulty;
	/* when it began (Speed Reader) */
	long start_time;
	/* Keyes: an Elite left the level alive (Tying Up Loose Ends) */
	boolean elite_left_alive;
	byte pad[3];
	struct achievement_player players[ACHIEVEMENT_PLAYERS];
};

/* ---------- globals */

static struct achievement_level *achievement_level;

/* which numbers the log has (read once, then kept up to date) */
static byte achievements_logged[(ACHIEVEMENT_NUMBERS + 7) / 8];
static boolean achievements_log_read = FALSE;

/* ---------- private code */

static boolean achievements_on(
	void)
{
	return config_boolean("game.achievements") != 0;
}

/* this machine decides a campaign level's achievements: alone, or the host
of network co-op (a client's come from its host), not the editor */
static struct achievement_level *achievements_deciding(
	void)
{
	if (!achievement_level || achievement_level->level == NONE || game_in_editor() ||
		game_connection() == _game_connection_network_client)
	{
		return NULL;
	}
	return achievement_level;
}

static struct achievement_player *achievements_player(
	long player_index)
{
	struct achievement_level *level = achievements_deciding();
	long slot;

	if (!level || player_index == NONE || !player_try_and_get(player_index))
		return NULL;
	slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	return slot >= 0 && slot < ACHIEVEMENT_PLAYERS ? &level->players[slot] : NULL;
}

/* whether a tag's name starts with prefix (its folder), any case */
static boolean tag_in_folder(
	long definition_index,
	char const *prefix)
{
	char const *name;
	unsigned long length = strlen(prefix);

	if (definition_index == NONE)
		return FALSE;
	name = tag_get_name(definition_index);
	return name && strlen(name) >= length && _strnicmp(name, prefix, length) == 0;
}

static boolean object_in_folder(
	long object_index,
	char const *prefix)
{
	struct object_datum *object = object_index != NONE ? object_try_and_get(object_index) : NULL;

	return object && tag_in_folder(object->definition_index, prefix);
}

/* an Elite's body, not the Flood's of one (its folder is its own) */
static boolean object_is_elite(
	long object_index)
{
	struct object_datum *object = object_try_and_get(object_index);

	return object && object->object.type == _object_type_biped &&
		tag_in_folder(object->definition_index, "characters\\elite\\");
}

static void achievements_log_read_once(
	void)
{
	FILE *file;
	unsigned char record[16];

	if (achievements_log_read)
		return;
	achievements_log_read = TRUE;
	csmemset(achievements_logged, 0, sizeof(achievements_logged));
	file = fopen(ACHIEVEMENTS_LOG, "rb");
	if (!file)
		return;
	while (fread(record, sizeof(record), 1, file) == 1)
	{
		unsigned long number = record[0] | (record[1] << 8) | (record[2] << 16) | ((unsigned long)record[3] << 24);

		if (number < ACHIEVEMENT_NUMBERS)
			achievements_logged[number / 8] |= (byte)(1 << (number % 8));
	}
	fclose(file);
}

static boolean achievements_logged_has(
	long number)
{
	achievements_log_read_once();
	return number >= 0 && number < ACHIEVEMENT_NUMBERS &&
		(achievements_logged[number / 8] & (1 << (number % 8))) != 0;
}

static void achievements_record(long number);

/* the campaign's three: every level finished on the difficulty or a harder
one (Legendary earns Heroic's and Normal's) */
static void achievements_check_campaign(
	void)
{
	static short const earned[] =
	{
		ACHIEVEMENT_BIRTH_OF_A_SPARTAN,
		ACHIEVEMENT_BELIEVE_IN_A_HERO,
		ACHIEVEMENT_LIVING_LEGEND,
	};
	short index;

	for (index = 0; index < NUMBEROF(earned); index++)
	{
		short difficulty = (short)(_game_difficulty_level_normal + index);
		short level;
		boolean all = TRUE;

		for (level = 0; level < NUMBER_OF_LEVELS && all; level++)
		{
			short harder;
			boolean finished = FALSE;

			for (harder = difficulty; harder < NUMBER_OF_GAME_DIFFICULTY_LEVELS && !finished; harder++)
				finished = achievements_logged_has(ACHIEVEMENT_LEVEL_FINISHED + level * NUMBER_OF_GAME_DIFFICULTY_LEVELS + harder);
			all = finished;
		}
		if (all)
			achievements_record(earned[index]);
	}
}

/* this machine's log gets the number, once */
static void achievements_record(
	long number)
{
	FILE *file;
	unsigned char record[16];
	unsigned long long when;
	short index;

	if (!achievements_on() || number <= 0 || number >= ACHIEVEMENT_NUMBERS || achievements_logged_has(number))
		return;
	when = FILETIME_UNIX_EPOCH + (unsigned long long)time(NULL) * 10000000ULL;
	csmemset(record, 0, sizeof(record));
	for (index = 0; index < 4; index++)
		record[index] = (unsigned char)(number >> (8 * index));
	for (index = 0; index < 8; index++)
		record[8 + index] = (unsigned char)(when >> (8 * index));
	file = fopen(ACHIEVEMENTS_LOG, "ab");
	if (!file)
	{
		error(_error_silent, "achievements: could not write %s", ACHIEVEMENTS_LOG);
		return;
	}
	fwrite(record, sizeof(record), 1, file);
	fclose(file);
	achievements_logged[number / 8] |= (byte)(1 << (number % 8));
	error(_error_silent, "achievements: %ld earned", number);
	if (number >= ACHIEVEMENT_LEVEL_FINISHED)
		achievements_check_campaign();
}

/* a player earned it: this machine's (logged here) or, on a co-op host, a
client's (sent to it) */
static void achievement_earned(
	long player_index,
	short achievement)
{
	struct player_datum *player = player_try_and_get(player_index);

	if (!player)
		return;
	if (player->local_player_index != NONE)
		achievements_record(achievement);
	else if (game_connection() == _game_connection_network_server)
		network_coop_host_send_achievement(player_index, achievement);
}

static void achievement_earned_by_all(
	short achievement)
{
	struct data_iterator iterator;

	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator))
		achievement_earned(iterator.datum_index, achievement);
}

static short achievements_player_count(
	void)
{
	struct data_iterator iterator;
	short count = 0;

	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator))
		count++;
	return count;
}

/* the level being played: the co-op host's game's, else the solo one */
static short achievements_current_level(
	void)
{
	if (network_coop_active())
	{
		struct network_game *game = network_game_get_game();

		return game ? main_get_solo_level_from_name(game->map.name) : NONE;
	}
	if (game_connection() != _game_connection_local || game_engine_running())
		return NONE;
	return main_get_current_solo_level();
}

/* Leave It Where It Lay: a weapon the player started with */
static boolean achievement_starting_weapon(
	struct achievement_player *record,
	long definition_index)
{
	short index;

	for (index = 0; index < STARTING_WEAPONS; index++)
	{
		if (record->starting_weapons[index] == definition_index)
			return TRUE;
	}
	return FALSE;
}

static void achievement_starting_weapon_add(
	struct achievement_player *record,
	long definition_index)
{
	short index;

	if (definition_index == NONE || achievement_starting_weapon(record, definition_index))
		return;
	for (index = 0; index < STARTING_WEAPONS; index++)
	{
		if (record->starting_weapons[index] == NONE)
		{
			record->starting_weapons[index] = definition_index;
			return;
		}
	}
}

/* Close Quarters Combat: the rounds (loaded and not) of the Sniper Rifle
the player's unit carries, 0 without one */
static short achievement_sniper_rounds(
	long unit_index)
{
	struct unit_datum *unit = unit_index != NONE ? (struct unit_datum *)object_try_and_get(unit_index) : NULL;
	short best = 0;
	short index;

	if (!unit || !TEST_FLAG(_object_mask_unit, unit->object.type))
		return 0;
	for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
	{
		long weapon_index = unit->unit.weapon_object_indices[index];
		struct weapon_datum *weapon;

		if (!object_in_folder(weapon_index, "weapons\\sniper"))
			continue;
		weapon = weapon_get(weapon_index);
		best = (short)MAX(best, weapon->weapon.magazines[0].rounds_loaded + weapon->weapon.magazines[0].rounds_total);
	}
	return best;
}

/* Tying Up Loose Ends: no Elite left alive in the level */
static boolean achievements_elites_all_dead(
	void)
{
	struct object_iterator iterator;
	struct object_datum *object;

	object_iterator_new(&iterator, _object_mask_biped, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		if (tag_in_folder(object->definition_index, "characters\\elite\\") &&
			!TEST_FLAG(object->object.damage_flags, _object_dead_bit))
		{
			return FALSE;
		}
	}
	return TRUE;
}

/* ---------- public code */

void achievements_initialize(
	void)
{
	achievement_level = (struct achievement_level *)game_state_malloc("achievements", NULL,
		sizeof(struct achievement_level));
}

void achievements_initialize_for_new_map(
	void)
{
	if (!achievement_level)
		return;
	csmemset(achievement_level, 0, sizeof(*achievement_level));
	csmemset(achievement_level->players, NONE, sizeof(achievement_level->players));
	{
		short index;

		for (index = 0; index < ACHIEVEMENT_PLAYERS; index++)
		{
			struct achievement_player *record = &achievement_level->players[index];

			record->flags = 0;
			record->banshees_destroyed = 0;
			record->wraiths_destroyed = 0;
			record->combat_forms_killed = 0;
			record->infection_forms_killed = 0;
		}
	}
	achievement_level->level = achievements_current_level();
	achievement_level->difficulty = game_difficulty_level_get();
	achievement_level->start_time = game_time_get();
}

void achievements_update(
	void)
{
	struct achievement_level *level = achievements_deciding();
	struct data_iterator iterator;
	struct player_datum *player;

	if (!level)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct achievement_player *record = achievements_player(iterator.datum_index);
		struct unit_datum *unit;
		short index;

		if (!record || TEST_FLAG(record->flags, _achievement_picked_up_weapon_bit) || player->unit_index == NONE)
			continue;
		unit = unit_get(player->unit_index);
		for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
		{
			struct object_datum *weapon = unit->unit.weapon_object_indices[index] != NONE ?
				object_try_and_get(unit->unit.weapon_object_indices[index]) : NULL;

			if (weapon)
				achievement_starting_weapon_add(record, weapon->definition_index);
		}
	}
}

void achievements_won_map(
	void)
{
	struct achievement_level *level = achievements_deciding();
	struct data_iterator iterator;
	struct player_datum *player;
	short difficulty;
	boolean heroic, legendary;

	if (!level || level->level < 0 || level->level >= NUMBER_OF_LEVELS)
		return;
	difficulty = (short)PIN(level->difficulty, 0, NUMBER_OF_GAME_DIFFICULTY_LEVELS - 1);
	heroic = difficulty >= _game_difficulty_level_hard;
	legendary = difficulty >= _game_difficulty_level_impossible;
	error(_error_silent, "achievements: level %d won on difficulty %d", level->level, difficulty);

	/* the level, and the level on the difficulty (the campaign's three) */
	achievement_earned_by_all((short)(ACHIEVEMENT_PILLAR_OF_AUTUMN + level->level));
	achievement_earned_by_all((short)(ACHIEVEMENT_LEVEL_FINISHED + level->level * NUMBER_OF_GAME_DIFFICULTY_LEVELS + difficulty));
	/* cooperatively: two players or more, on Normal or harder */
	if (achievements_player_count() >= 2 && difficulty >= _game_difficulty_level_normal)
	{
		achievement_earned_by_all(ACHIEVEMENT_STANDARD_OPERATING_BROCEDURE);
		if (heroic)
			achievement_earned_by_all(ACHIEVEMENT_BROVERSHIELD);
		if (legendary)
			achievement_earned_by_all(ACHIEVEMENT_BRO_HAMMER);
	}

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct achievement_player *record = achievements_player(iterator.datum_index);
		long player_index = iterator.datum_index;

		if (!record)
			continue;
		if (heroic && !TEST_FLAG(record->flags, _achievement_health_damaged_bit))
			achievement_earned(player_index, ACHIEVEMENT_HES_UNSTOPPABLE);
		switch (level->level)
		{
		case _level_pillar_of_autumn:
			if (legendary && !TEST_FLAG(record->flags, _achievement_took_overshield_bit))
				achievement_earned(player_index, ACHIEVEMENT_OVERSHIELDS_ARE_FOR_SISSIES);
			if (legendary && !TEST_FLAG(record->flags, _achievement_took_health_pack_bit))
				achievement_earned(player_index, ACHIEVEMENT_WALK_IT_OFF);
			break;
		case _level_halo:
			if (!TEST_FLAG(record->flags, _achievement_entered_vehicle_bit))
				achievement_earned(player_index, ACHIEVEMENT_HOW_PEDESTRIAN);
			break;
		case _level_truth_and_reconciliation:
			if (achievement_sniper_rounds(player->unit_index) >= SNIPER_ROUNDS_LEFT)
				achievement_earned(player_index, ACHIEVEMENT_CLOSE_QUARTERS_COMBAT);
			break;
		case _level_library:
			if (heroic && !TEST_FLAG(record->flags, _achievement_died_bit))
				achievement_earned(player_index, ACHIEVEMENT_THAT_JUST_HAPPENED);
			if (legendary && game_time_get() - level->start_time <= SPEED_READER_TICKS)
				achievement_earned(player_index, ACHIEVEMENT_SPEED_READER);
			break;
		case _level_two_betrayals:
			if (heroic && !TEST_FLAG(record->flags, _achievement_killed_grunt_bit))
				achievement_earned(player_index, ACHIEVEMENT_LOOK_OUT_FOR_THE_LITTLE_GUYS);
			if (legendary && !TEST_FLAG(record->flags, _achievement_took_other_weapon_bit))
				achievement_earned(player_index, ACHIEVEMENT_LEAVE_IT_WHERE_IT_LAY);
			break;
		case _level_keyes:
			if (heroic && !level->elite_left_alive && achievements_elites_all_dead())
				achievement_earned(player_index, ACHIEVEMENT_TYING_UP_LOOSE_ENDS);
			break;
		}
	}
}

void achievements_body_damaged(
	long object_index)
{
	struct unit_datum *unit;
	struct achievement_player *record;

	if (!achievements_deciding())
		return;
	unit = (struct unit_datum *)object_try_and_get(object_index);
	if (!unit || unit->object.type != _object_type_biped)
		return;
	record = achievements_player(unit->unit.player_index);
	if (record)
		SET_FLAG(record->flags, _achievement_health_damaged_bit, TRUE);
}

void achievements_object_killed(
	long object_index,
	struct damage_data const *damage)
{
	struct achievement_level *level = achievements_deciding();
	struct achievement_player *record;
	long owner_player_index = damage ? damage->owner_player_index : NONE;
	long owner_object_index = damage ? damage->owner_object_index : NONE;
	struct object_datum *owner = owner_object_index != NONE ? object_try_and_get(owner_object_index) : NULL;
	boolean heroic;

	if (!level || !(record = achievements_player(owner_player_index)))
		return;
	heroic = level->difficulty >= _game_difficulty_level_hard;
	switch (level->level)
	{
	case _level_halo:
		/* No-Fly Zone: a Banshee itself destroyed */
		if (object_in_folder(object_index, "vehicles\\banshee\\") &&
			++record->banshees_destroyed >= BANSHEES_DESTROYED &&
			!TEST_FLAG(record->flags, _achievement_no_fly_zone_bit))
		{
			SET_FLAG(record->flags, _achievement_no_fly_zone_bit, TRUE);
			achievement_earned(owner_player_index, ACHIEVEMENT_NO_FLY_ZONE);
		}
		break;
	case _level_assault_on_the_control_room:
		/* Wraith Hunter: with the Scorpion's guns (its shell's or bullet's
		damage, or fired by it or from its seat) */
		if (object_in_folder(object_index, "vehicles\\wraith\\") &&
			(tag_in_folder(damage->definition_index, "vehicles\\scorpion\\") ||
				(owner && tag_in_folder(owner->definition_index, "vehicles\\scorpion\\")) ||
				(owner && object_in_folder(owner->object.parent_object_index, "vehicles\\scorpion\\"))) &&
			++record->wraiths_destroyed >= WRAITHS_DESTROYED &&
			!TEST_FLAG(record->flags, _achievement_wraith_hunter_bit))
		{
			SET_FLAG(record->flags, _achievement_wraith_hunter_bit, TRUE);
			achievement_earned(owner_player_index, ACHIEVEMENT_WRAITH_HUNTER);
		}
		break;
	case _level_guilty_spark:
		if (heroic && object_in_folder(object_index, "characters\\floodcombat") &&
			++record->combat_forms_killed >= COMBAT_FORMS_KILLED &&
			!TEST_FLAG(record->flags, _achievement_jenkins_bit))
		{
			SET_FLAG(record->flags, _achievement_jenkins_bit, TRUE);
			achievement_earned(owner_player_index, ACHIEVEMENT_THIS_ONES_FOR_JENKINS);
		}
		break;
	case _level_two_betrayals:
		if (object_in_folder(object_index, "characters\\grunt\\"))
			SET_FLAG(record->flags, _achievement_killed_grunt_bit, TRUE);
		break;
	case _level_keyes:
		if (heroic && (object_in_folder(object_index, "characters\\flood_infection\\") ||
				object_in_folder(object_index, "characters\\floodinfection\\")) &&
			++record->infection_forms_killed >= INFECTION_FORMS_KILLED &&
			!TEST_FLAG(record->flags, _achievement_popcorn_bit))
		{
			SET_FLAG(record->flags, _achievement_popcorn_bit, TRUE);
			achievement_earned(owner_player_index, ACHIEVEMENT_POPCORN);
		}
		break;
	}
}

void achievements_player_died(
	long player_index)
{
	struct achievement_player *record = achievements_player(player_index);

	if (record)
	{
		SET_FLAG(record->flags, _achievement_died_bit, TRUE);
		SET_FLAG(record->flags, _achievement_health_damaged_bit, TRUE);
	}
}

void achievements_player_picked_up_weapon(
	long player_index,
	long definition_index)
{
	struct achievement_player *record = achievements_player(player_index);

	/* (the weapons the player started with may be picked up again, for
	their ammunition) */
	if (!record)
		return;
	SET_FLAG(record->flags, _achievement_picked_up_weapon_bit, TRUE);
	if (!achievement_starting_weapon(record, definition_index))
		SET_FLAG(record->flags, _achievement_took_other_weapon_bit, TRUE);
}

void achievements_player_picked_up_powerup(
	long player_index,
	long definition_index)
{
	struct achievement_player *record = achievements_player(player_index);
	struct equipment_definition *definition;

	if (!record || definition_index == NONE)
		return;
	definition = equipment_definition_get(definition_index);
	if (definition->equipment.powerup_type == _equipment_powerup_overshield)
		SET_FLAG(record->flags, _achievement_took_overshield_bit, TRUE);
	else if (definition->equipment.powerup_type == _equipment_powerup_health)
		SET_FLAG(record->flags, _achievement_took_health_pack_bit, TRUE);
}

void achievements_player_entered_seat(
	long player_index,
	long vehicle_index)
{
	struct achievement_level *level = achievements_deciding();
	struct achievement_player *record = achievements_player(player_index);
	struct player_datum *player = player_try_and_get(player_index);
	struct unit_datum *vehicle;

	if (!level || !record || !player)
		return;
	vehicle = (struct unit_datum *)object_try_and_get(vehicle_index);
	if (!vehicle || vehicle->object.type != _object_type_vehicle)
		return;
	SET_FLAG(record->flags, _achievement_entered_vehicle_bit, TRUE);
	/* I'll Be Taking That!: a Banshee's pilot */
	if (level->level == _level_assault_on_the_control_room &&
		tag_in_folder(vehicle->definition_index, "vehicles\\banshee\\") &&
		player->unit_index != NONE && vehicle->unit.driver_object_index == player->unit_index &&
		!TEST_FLAG(record->flags, _achievement_taking_that_bit))
	{
		SET_FLAG(record->flags, _achievement_taking_that_bit, TRUE);
		achievement_earned(player_index, ACHIEVEMENT_ILL_BE_TAKING_THAT);
	}
}

void achievements_object_deleted(
	long object_index)
{
	struct achievement_level *level = achievements_deciding();
	struct object_datum *object;

	/* Keyes: an Elite taken out of the level alive voids Tying Up Loose Ends */
	if (!level || level->level != _level_keyes || level->elite_left_alive || !object_is_elite(object_index))
		return;
	object = object_get(object_index);
	if (!TEST_FLAG(object->object.damage_flags, _object_dead_bit))
	{
		level->elite_left_alive = TRUE;
		error(_error_silent, "achievements: an Elite (%ld) left Keyes alive", object_index);
	}
}

void achievements_network_earned(
	long player_index,
	short achievement)
{
	struct player_datum *player;

	if (achievement <= 0 || achievement >= ACHIEVEMENT_NUMBERS)
		return;
	player = player_try_and_get(player_index);
	if (player && player->local_player_index != NONE)
		achievements_record(achievement);
}
