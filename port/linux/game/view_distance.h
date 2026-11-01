/*
VIEW_DISTANCE.H

How far a multiplayer game's view reaches (display.multiplayer_draw_distance,
view_distance.c).
*/

#ifndef VIEW_DISTANCE_H
#define VIEW_DISTANCE_H

#include "math/real_math.h"

struct render_fog;

/* the size, in the pixels render_model measures, an object's model is
drawn at: in a multiplayer game a unit's at least its model's smallest
level's cutoff, so it is drawn at any distance */
real view_distance_object_pixels(long object_index, long model_index, real level_of_detail_pixels);
/* the atmospheric fog of a multiplayer game's view pushed out */
void view_distance_fog(struct render_fog *fog);

#endif
