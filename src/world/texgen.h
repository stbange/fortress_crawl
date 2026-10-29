#ifndef FC_TEXGEN_H
#define FC_TEXGEN_H

#include "core/common.h"
#include "world/theme.h"

#define TEX_SIZE 16
#define TEX_VARIANTS 4
#define TILES_PER_THEME (BLOCK_TEX_SLOTS * TEX_VARIANTS)

/* Generates every block texture of a theme: TILES_PER_THEME tiles of TEX_SIZE^2 RGBA
 * pixels, stored tile after tile. Tile index = block * TEX_VARIANTS + variant. */
void texgen_theme(int theme, u8 *out);

#endif
