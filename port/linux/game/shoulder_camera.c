/*
SHOULDER_CAMERA.C

A local over-the-shoulder view while the player holds a weapon whose tag has
no first person model (many of Digsite's and spasm's weapons), which would
otherwise show nothing in the player's hands. It only moves the display
camera: shots, aim assist and the network still use the first person eye
(director_camera_deterministic), and the reticle is drawn where the shot
would land (passenger_camera_reticle_begin). Zooming returns to first person
for the scope.
*/

#include "cseries.h"
#include "shoulder_camera.h"
#include "camera/director.h"
#include "cutscene/cinematics.h"
#include "game/players.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "units/units.h"

/* world units (one is about three metres): behind the eye, to the right of
it, and above it */
#define SHOULDER_DISTANCE 0.5f
#define SHOULDER_SIDE 0.2f
#define SHOULDER_HEIGHT 0.05f
/* kept between the camera's pivot and a wall beside the player */
#define SHOULDER_WALL_MARGIN 0.05f

boolean shoulder_camera_enabled(short local_player_index)
{
	long player_index, unit_index, weapon_index;
	struct unit_datum *unit;
	struct weapon_datum *weapon;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS ||
		cinematic_in_progress() || (director_camera_scripted && *director_camera_scripted))
		return FALSE;
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE)
		return FALSE;
	unit_index = player_control_get_unit_index(local_player_index);
	if (unit_index == NONE || player_get(player_index)->unit_index != unit_index)
		return FALSE;
	unit = unit_try_and_get(unit_index);
	/* (on foot: a seat has its own camera) */
	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
		unit->object.parent_object_index != NONE)
		return FALSE;
	/* a scope is looked through in first person */
	if ((signed char)unit->unit.current_zoom_level >= 0 ||
		(signed char)unit->unit.desired_zoom_level >= 0)
		return FALSE;
	if (unit->unit.current_weapon_index < 0 || unit->unit.current_weapon_index >= MAXIMUM_WEAPONS_PER_UNIT)
		return FALSE;
	weapon_index = unit_inventory_get_weapon(unit_index, unit->unit.current_weapon_index);
	if (weapon_index == NONE)
		return FALSE;
	weapon = weapon_try_and_get(weapon_index);
	if (!weapon)
		return FALSE;
	return weapon_definition_get(weapon->definition_index)->weapon.interface_definition.first_person_model.index == NONE;
}

void shoulder_camera_offset(short local_player_index, real_point3d const *eye,
	real_euler_angles2d const *facing, real_vector3d *offset, real *depth)
{
	real side = SHOULDER_SIDE;
	real_vector3d ray;
	struct collision_result collision;

	/* The observer keeps the camera out of what is behind this pivot; keep
	the pivot itself out of a wall at the player's right shoulder. */
	ray.i = sine(facing->yaw) * (SHOULDER_SIDE + SHOULDER_WALL_MARGIN);
	ray.j = -cosine(facing->yaw) * (SHOULDER_SIDE + SHOULDER_WALL_MARGIN);
	ray.k = 0.0f;
	if (collision_test_vector(
			FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_structure_bit) |
			FLAG(_collision_test_objects_bit) |
			FLAG(_collision_test_objects_scenery_bit),
			eye, &ray, player_control_get_unit_index(local_player_index), &collision))
	{
		side = collision.t * (SHOULDER_SIDE + SHOULDER_WALL_MARGIN) - SHOULDER_WALL_MARGIN;
		if (side < 0.0f)
			side = 0.0f;
	}
	offset->i = 0.0f;
	offset->j = side;
	offset->k = SHOULDER_HEIGHT;
	*depth = SHOULDER_DISTANCE;
}
