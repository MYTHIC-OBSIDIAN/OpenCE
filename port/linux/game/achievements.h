/*
ACHIEVEMENTS.H

The calls the game's sources make into achievements.c: what the players do
in a campaign level (the damage they take, what they pick up, kill and get
into, their deaths, the level won), which the achievements are earned from.
Only a machine that runs the game decides (alone, the host of network
co-op); a co-op client's players' achievements come from its host.
*/

#ifndef __ACHIEVEMENTS_H
#define __ACHIEVEMENTS_H

struct damage_data;

/* ---------- prototypes/ACHIEVEMENTS.C */

/* the game state's record of the level so far (game_initialize): reverted
with the checkpoints, so a revert undoes what was done since */
void achievements_initialize(void);
/* a new map: its level and difficulty, nothing done yet */
void achievements_initialize_for_new_map(void);
/* each tick: the players' starting weapons, once */
void achievements_update(void);
/* the level won (main_won_map_private), before the game leaves it */
void achievements_won_map(void);

/* object_damage_body: an object's body was hurt (a player's: health damage) */
void achievements_body_damaged(long object_index);
/* object_damage_body: an object died of this damage (by its owner's hand) */
void achievements_object_killed(long object_index, struct damage_data const *damage);
/* unit_died: a player's unit died */
void achievements_player_died(long player_index);
/* players.c: a player picked up a weapon (not ammunition) */
void achievements_player_picked_up_weapon(long player_index, long definition_index);
/* players.c: a player took a power-up (an overshield, a health pack) */
void achievements_player_picked_up_powerup(long player_index, long definition_index);
/* players.c: a player got into a vehicle's seat */
void achievements_player_entered_seat(long player_index, long vehicle_index);
/* biped_update: a unit thrown from its flipped vehicle */
void achievements_player_ejected(long unit_index);
/* unit_died: any unit died */
void achievements_unit_died(long unit_index);
/* weapon_trigger_fire: a weapon fired a shot (its owner: a unit, or the
vehicle whose weapon it is) */
void achievements_weapon_fired(long owner_object_index);
/* object_deleted_procs_call: an object about to be deleted */
void achievements_object_deleted(long object_index);

/* network co-op client: its host says a player earned an achievement
(port/linux/game/network_coop.c) */
void achievements_network_earned(long player_index, short achievement);
/* a local player's profile changed (player_ui.c's active profile): the
companion program's file of player 1's name and the local players' logs */
void achievements_profiles_changed(void);
/* a chapter title card shown (cinematics.c) */
void achievements_title_shown(char const *name);

#endif // __ACHIEVEMENTS_H
