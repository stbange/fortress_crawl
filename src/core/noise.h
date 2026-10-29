#ifndef FC_NOISE_H
#define FC_NOISE_H

#include "core/common.h"

/* Seeded gradient (Perlin-style) noise. Output roughly in [-1, 1]. */
float noise3(u64 seed, float x, float y, float z);
float noise2(u64 seed, float x, float y);

/* Fractal sums, normalised to roughly [-1, 1]. */
float fbm3(u64 seed, float x, float y, float z, int octaves);
float fbm2(u64 seed, float x, float y, int octaves);

#endif
