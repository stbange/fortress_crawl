#ifndef FC_BATCH_H
#define FC_BATCH_H

#include "core/fmath.h"

/* Immediate-mode batches for things that move: monsters, projectiles and particles
 * (3D, lit on the CPU, fogged in the shader), the weapon in view, and flat UI quads. */

typedef struct BatchFog {
    vec3 cam, color;
    float density;
} BatchFog;

void batch_init(void);

/* 3D: colours are final (already lit); emissive parts fog less. */
void batch3d_begin(void);
/* Box centred at c with half extents he, rotated by yaw around Y. shade: per-face lighting. */
void batch3d_box(vec3 c, vec3 he, float yaw, vec3 col, bool emissive);
/* Box with an arbitrary orientation given by three axes (each scaled by the half extent). */
void batch3d_obox(vec3 c, vec3 ax, vec3 ay, vec3 az, vec3 col, bool emissive);
/* Double pyramid (octahedron) with radius r and half height h. */
void batch3d_octa(vec3 c, float r, float h, float yaw, vec3 col, bool emissive);
/* Pyramid / cone stand-in: square base of half size r at c, apex `h` along `dir`. */
void batch3d_spike(vec3 c, vec3 dir, float r, float h, vec3 col, bool emissive);
void batch3d_flush(const mat4 *vp, const BatchFog *fog);

/* 2D in target pixels, origin top-left, with alpha blending. */
void batch2d_begin(int target_w, int target_h);
void batch2d_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void batch2d_flush(void);

#endif
