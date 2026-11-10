/*
BUILD_1749_VARIANTS.H

Build 1749's own game types (build_1749_variants.c), which halo1749.exe
plays in place of this build's.
*/

#ifndef __BUILD_1749_VARIANTS_H
#define __BUILD_1749_VARIANTS_H

struct game_variant;

enum
{
	_build_1749_variant_dark_stalker = 0,
	_build_1749_variant_race,
	_build_1749_variant_team_race,
	_build_1749_variant_rally,
	_build_1749_variant_slayer,
	_build_1749_variant_team_slayer,
	_build_1749_variant_killer,
	_build_1749_variant_elimination,
	_build_1749_variant_hunter,
	_build_1749_variant_stalker,
	_build_1749_variant_team_oddball,
	_build_1749_variant_accumulation,
	_build_1749_variant_magic,
	_build_1749_variant_oddball,
	_build_1749_variant_crazyball,
	_build_1749_variant_assault,
	_build_1749_variant_ctf,
	_build_1749_variant_king,
	_build_1749_variant_team_king,
	_build_1749_variant_iron_ctf,
	NUMBER_OF_BUILD_1749_VARIANTS
};

/* 1749's game type index (its executable's order, which its menus' names
follow), as 1749 built it */
struct game_variant *build_1749_variant(struct game_variant *variant, long index);
/* a game type by the name game_engine_get_variant_by_name takes ("slayer",
"ctf", ...), 1749's own among them ("dark_stalker", "killer", "hunter",
"magic", "crazyball", "assault"); NONE for none */
long build_1749_variant_index(char const *name);

#endif
