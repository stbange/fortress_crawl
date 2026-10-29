#ifndef FC_PHYSICS_H
#define FC_PHYSICS_H

#include "core/fmath.h"

/* Returns 1 for solid, 0 for free, -1 for not loaded yet (treated as solid). */
typedef int (*SolidFn)(void *user, int x, int y, int z);

/* Axis-aligned box standing on pos (feet centre), half width hw, height h. */
bool phys_box_free(vec3 pos, float hw, float h, SolidFn solid, void *user);
/* Moves along one axis (0 x, 1 y, 2 z) in small steps; stops flush at the first obstacle.
 * Returns false if blocked. */
bool phys_move_axis(vec3 *pos, int axis, float d, float hw, float h, SolidFn solid, void *user);
/* Voxel ray cast. Returns the distance to the first solid cell (or max). */
float phys_raycast(vec3 o, vec3 dir, float max, SolidFn solid, void *user);
/* True if nothing solid lies between a and b. */
bool phys_line_of_sight(vec3 a, vec3 b, SolidFn solid, void *user);

#endif
