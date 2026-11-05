/* The local over-the-shoulder view for weapons without a first person model
(shoulder_camera.c). */
#ifndef SHOULDER_CAMERA_H
#define SHOULDER_CAMERA_H

#include "math/real_math.h"

boolean shoulder_camera_enabled(short local_player_index);
/* The following camera's focus offset and pull-back distance for this view,
from the unit's eye along `facing`. */
void shoulder_camera_offset(short local_player_index, real_point3d const *eye,
	real_euler_angles2d const *facing, real_vector3d *offset, real *depth);

#endif
