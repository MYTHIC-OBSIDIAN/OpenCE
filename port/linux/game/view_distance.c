/*
VIEW_DISTANCE.C

display.multiplayer_draw_distance: in a multiplayer game, players are seen
from base to base.

A model smaller than its smallest level's cutoff is not drawn (render_model,
source/models/models.c). The cyborg's is 5 pixels in the Xbox's 480 lines,
which a player is about 100 world units off: Blood Gulch's bases are 97
apart, so a player at the other base was drawn or not as he moved. A unit is
now drawn at any distance, at its smallest level as before. The campaign's
are drawn as the Xbox drew them.
*/

#include "cseries.h"
#include "view_distance.h"
#include "game/game_engine.h"
#include "models/model_definitions.h"
#include "models/models.h"
#include "objects/objects.h"
#include "objects/object_types.h"

/* port/linux/src/port_config.c */
int config_boolean(const char *name);
unsigned long config_changes(void);
/* port/linux/src/d3d8_gl.c */
float halo_model_detail_scale(int level);

/* ---------- private code */

static boolean view_distance_enabled(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled = TRUE;

	if (changes != config_changes())
	{
		changes = config_changes();
		enabled = config_boolean("display.multiplayer_draw_distance") != 0;
	}
	return enabled && game_engine_running();
}

/* ---------- public code */

real view_distance_object_pixels(
	long object_index,
	long model_index,
	real level_of_detail_pixels)
{
	struct model *model;
	real smallest;

	if (model_index == NONE || !view_distance_enabled() ||
		!object_try_and_get_and_verify_type(object_index, _object_mask_unit))
	{
		return level_of_detail_pixels;
	}
	model = model_definition_get(model_index);
	/* (render_model measures it scaled as halo_model_detail_scale says) */
	smallest = model->detail_cutoff_pixels[0] / halo_model_detail_scale(FALSE);
	return level_of_detail_pixels < smallest ? smallest : level_of_detail_pixels;
}
