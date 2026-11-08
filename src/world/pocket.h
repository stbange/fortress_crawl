#ifndef FC_POCKET_H
#define FC_POCKET_H

/* The pocket realm: the Sanctum behind the Sanctum Door, a home on a floating island far
 * above the world (300 layers up). It starts as a ruin and grows: the island is a grid of
 * 4 x 4 plots of 32 x 32 m, and each plot is built up in stages (gen_pocket.c builds what
 * the stages say; the game side, realm.c and shop.c, does the comforts and the building).
 * World coordinates: the island spans x, z 0..127; its ground is walked on at POCKET_G.
 *
 *   pz 0:  tower   garden  garden  tower        (garden, library, armory: island stage 2;
 *   pz 1:  library hall    bath    armory        towers and the outer wall: stage 3)
 *   pz 2:  lawn    court   dining  lawn
 *   pz 3:  tower   lawn    lawn    tower                                                  */

#include "core/common.h"

#define POCKET_CY 300                 /* the chunk layer of the island */
#define POCKET_Y0 (POCKET_CY * 32)    /* CHUNK = 32 */
#define POCKET_G (POCKET_Y0 + 8)      /* feet height on the ground floor */

/* Every chunk of the realm and the empty sky around it, far enough that no ordinary
 * chunk is ever in view from the island (the island is chunks 0..3; views reach 10 out). */
static inline bool world_is_pocket(int cx, int cy, int cz) {
    return cx >= -10 && cx <= 13 && cz >= -10 && cz <= 13 && cy >= POCKET_CY - 6 && cy <= POCKET_CY + 6;
}

/* What can be built, and the most stages of each. Stage 0 is not built (a ruin or bare
 * ground); a new world starts with the island, the hall and the court at stage 1. */
enum { SP_ISLAND, SP_HALL, SP_BATH, SP_COURT, SP_DINING, SP_LIBRARY, SP_GARDEN, SP_ARMORY, SP_COUNT };
#define SANCTUM_STAGES 3

/* Decorations for gold (shop.c sells them, gen_pocket.c and realm.c build and draw them):
 * nothing but beauty. Each needs its plot built. */
enum { SD_STATUE, SD_BANNERS, SD_CARPET, SD_BRAZIERS, SD_FLOWERS, SD_KOI, SD_LANTERNS, SD_THRONE, SD_CHANDELIER, SD_SUNDIAL,
       SD_ORRERY, SD_COUNT };

/* Each plot's corner (world x, z) on the island grid. */
static inline int plot_x(int plot) {
    static const int X[SP_COUNT] = { 0, 32, 64, 32, 64, 0, 32, 96 };
    return X[plot];
}
static inline int plot_z(int plot) {
    static const int Z[SP_COUNT] = { 0, 32, 32, 64, 64, 32, 0, 32 };
    return Z[plot];
}

/* Where you arrive (in the court, looking at the hall) and the door back. */
#define POCKET_ARRIVE_X 47.5f
#define POCKET_ARRIVE_Z 80.5f
#define POCKET_EXIT_X 47.5f
#define POCKET_EXIT_Z 89.5f

/* The rooms' furniture, as world x/z rectangles (inclusive). */
#define POCKET_BATH_X0 70   /* the pool (bath) */
#define POCKET_BATH_X1 89
#define POCKET_BATH_Z0 39
#define POCKET_BATH_Z1 56
#define POCKET_TABLE_X0 77  /* the table (dining); short at stage 1 */
#define POCKET_TABLE_X1 82
#define POCKET_TABLE_Z0 71
#define POCKET_TABLE_Z1 88
#define POCKET_TABLE_Z0_SHORT 76
#define POCKET_TABLE_Z1_SHORT 82
#define POCKET_BED_X0 54    /* the bed (hall) */
#define POCKET_BED_X1 58
#define POCKET_BED_Z0 35
#define POCKET_BED_Z1 40
#define POCKET_DESK_X 37    /* the drafting table (hall) */
#define POCKET_DESK_Z 37
#define POCKET_STASH_X 36   /* the stash chest (hall) */
#define POCKET_STASH_Z 52
#define POCKET_SHRINE_X 15  /* the library's shrine */
#define POCKET_SHRINE_Z 47

#endif
