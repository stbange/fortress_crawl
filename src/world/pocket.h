#ifndef FC_POCKET_H
#define FC_POCKET_H

/* The pocket realm: the player's castle behind the Sanctum Door. It is a fixed place far
 * above the world (300 layers up), on a floating island, built by gen_pocket.c; the game
 * side (realm.c) handles the doors and the comforts: the bath, the feast and the bed.
 * World coordinates: the island spans x, z 0..127; its ground is walked on at POCKET_G. */

#include "core/common.h"

#define POCKET_CY 300                 /* the chunk layer of the island */
#define POCKET_Y0 (POCKET_CY * 32)    /* CHUNK = 32 */
#define POCKET_G (POCKET_Y0 + 8)      /* feet height on the ground floor */

/* Every chunk of the realm and the empty sky around it, far enough that no ordinary
 * chunk is ever in view from the island (the island is chunks 0..3; views reach 10 out). */
static inline bool world_is_pocket(int cx, int cy, int cz) {
    return cx >= -10 && cx <= 13 && cz >= -10 && cz <= 13 && cy >= POCKET_CY - 6 && cy <= POCKET_CY + 6;
}

/* Where you arrive (in the garden, looking at the keep) and the door back. */
#define POCKET_ARRIVE_X 64.5f
#define POCKET_ARRIVE_Z 34.5f /* between the fountain (z 19..29) and the keep */
#define POCKET_EXIT_X 64.5f
#define POCKET_EXIT_Z 11.5f

/* The rooms' furniture, as world x/z rectangles (inclusive). */
#define POCKET_BATH_X0 22
#define POCKET_BATH_X1 34
#define POCKET_BATH_Z0 52
#define POCKET_BATH_Z1 78
#define POCKET_TABLE_X0 96
#define POCKET_TABLE_X1 101
#define POCKET_TABLE_Z0 54
#define POCKET_TABLE_Z1 76
#define POCKET_BED_X0 97
#define POCKET_BED_X1 101
#define POCKET_BED_Z0 104
#define POCKET_BED_Z1 109

#endif
