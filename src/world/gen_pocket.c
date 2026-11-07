#include "world/gen_common.h"
#include "world/pocket.h"

#include "core/noise.h"

/* The pocket realm: a castle on a floating island under an open sky, built by hand.
 * Every chunk of the realm runs the same plan; the fills clip themselves to the chunk.
 *
 *   garden (south, z 8..40): lawn, trees, a fountain, the door back
 *   keep (x 16..111, z 44..119), with towers at its corners:
 *     great hall (middle)   bath hall (west)   dining hall (east)
 *     library (north-west, with a shrine)      bedchamber (north-east)
 *   a curtain wall round the island's edge */

#define G POCKET_G
#define F (POCKET_G - 1) /* the floor cells */

static void box(GenCtx *g, int x0, int y0, int z0, int x1, int y1, int z1, Block b) { g_fill_w(g, x0, y0, z0, x1, y1, z1, b); }

static void tree(GenCtx *g, int x, int z, int h) {
    box(g, x, G, z, x, G + h, z, B_WOOD);
    for (int k = 0; k < 3; k++) {
        int r = 2 - (k == 2);
        box(g, x - r, G + h - 1 + k, z - r, x + r, G + h - 1 + k, z + r, B_DIRT); /* leaves (the realm's dirt is green) */
    }
    g_set_w(g, x, G + h + 2, z, B_DIRT);
}

static void tower(GenCtx *g, int cx, int cz, int r, int top) {
    for (int z = cz - r; z <= cz + r; z++)
        for (int x = cx - r; x <= cx + r; x++) {
            float d = sqrtf((float)((x - cx) * (x - cx) + (z - cz) * (z - cz)));
            if (d > r + 0.4f) continue;
            if (d > r - 1.6f) {
                box(g, x, G, z, x, top, z, B_WALL);
                if ((x + z) & 1) g_set_w(g, x, top + 1, z, B_WALL); /* crenellations */
                if (d > r - 0.6f && ((x * 3 + z) % 7 == 0)) box(g, x, G + 12, z, x, G + 14, z, B_GLASS);
            } else {
                box(g, x, top - 1, z, x, top, z, B_CEIL);
            }
        }
    box(g, cx, top + 1, cz, cx, top + 1, cz, B_BRAZIER);
    g_set_w(g, cx, top + 2, cz, B_FIRE);
}

/* a 3-wide doorway through a partition */
static void door_x(GenCtx *g, int x, int z0, int z1) { box(g, x, G, z0, x, G + 4, z1, B_AIR); }
static void door_z(GenCtx *g, int z, int x0, int x1) { box(g, x0, G, z, x1, G + 4, z, B_AIR); }

static void brazier(GenCtx *g, int x, int z) {
    g_set_w(g, x, G, z, B_BRAZIER);
    g_set_w(g, x, G + 1, z, B_FIRE);
}

void gen_pocket(GenCtx *g) {
    memset(g->b, B_AIR, CHUNK_VOL);
    const int R = POCKET_Y0;
    /* the island: ground, and a rocky underside tapering into the sky */
    for (int z = 2; z <= 125; z++)
        for (int x = 2; x <= 125; x++) {
            float dx = (x - 63.5f) / 62.0f, dz = (z - 63.5f) / 62.0f, e = sqrtf(dx * dx + dz * dz);
            if (e > 1.0f) continue;
            int depth = (int)((1.0f - e) * 34.0f + 4.0f + 3.0f * noise3(0x15A, x * 0.15f, 0, z * 0.15f));
            box(g, x, R - depth, z, x, R + 4, z, B_ROCK);
            box(g, x, R + 5, z, x, F, z, B_DIRT);
        }
    /* the curtain wall round the edge, with a walk on top */
    for (int z = 2; z <= 125; z++)
        for (int x = 2; x <= 125; x++) {
            float dx = (x - 63.5f) / 62.0f, dz = (z - 63.5f) / 62.0f, e = sqrtf(dx * dx + dz * dz);
            if (e > 1.0f || e < 0.94f) continue;
            box(g, x, G, z, x, G + 14, z, B_WALL);
            if ((x + z) & 1) g_set_w(g, x, G + 15, z, B_WALL);
        }

    /* ---------- the garden ---------- */
    box(g, 61, F, 6, 67, F, 44, B_FLOOR); /* the path from the door to the keep */
    box(g, 40, F, 22, 88, F, 26, B_FLOOR);
    for (int z = 8; z <= 42; z++)
        for (int x = 10; x <= 118; x++)
            if (hash3f(0x6A2D, x, 0, z, 1) < 0.18f && g_get_w(g, x, F, z) == B_DIRT) g_set_w(g, x, G, z, B_GRASS);
    static const int TREES[][3] = { { 24, 16, 5 }, { 34, 30, 6 }, { 46, 12, 5 }, { 84, 14, 6 }, { 96, 30, 5 }, { 106, 18, 6 },
                                    { 18, 34, 4 }, { 110, 36, 4 }, { 52, 36, 5 }, { 78, 34, 5 } };
    for (int i = 0; i < (int)(sizeof TREES / sizeof TREES[0]); i++) tree(g, TREES[i][0], TREES[i][1], TREES[i][2]);
    /* the fountain */
    for (int z = 19; z <= 29; z++)
        for (int x = 59; x <= 69; x++) {
            float d = sqrtf((x - 64.0f) * (x - 64.0f) + (z - 24.0f) * (z - 24.0f));
            if (d <= 5.4f && d > 4.4f) g_set_w(g, x, G, z, B_PILLAR);
            else if (d <= 4.4f) g_set_w(g, x, G, z, B_WATER); /* a raised basin: one step out from anywhere in it */
        }
    box(g, 64, G, 24, 64, G + 2, 24, B_PILLAR);
    g_set_w(g, 64, G + 3, 24, B_LANTERN);
    /* lamps along the path */
    for (int z = 10; z <= 40; z += 10) {
        box(g, 59, G, z, 59, G + 2, z, B_PILLAR);
        g_set_w(g, 59, G + 3, z, B_LANTERN);
        box(g, 69, G, z, 69, G + 2, z, B_PILLAR);
        g_set_w(g, 69, G + 3, z, B_LANTERN);
    }

    /* ---------- the keep ---------- */
    const int X0 = 16, X1 = 111, Z0 = 44, Z1 = 119, TOP = G + 20;
    box(g, X0, F, Z0, X1, F, Z1, B_FLOOR);
    box(g, X0, G, Z0, X1, TOP, Z0 + 1, B_WALL);   /* south */
    box(g, X0, G, Z1 - 1, X1, TOP, Z1, B_WALL);   /* north */
    box(g, X0, G, Z0, X0 + 1, TOP, Z1, B_WALL);   /* west */
    box(g, X1 - 1, G, Z0, X1, TOP, Z1, B_WALL);   /* east */
    box(g, X0, TOP + 1, Z0, X1, TOP + 1, Z1, B_CEIL);
    box(g, X0, TOP + 2, Z0, X1, TOP + 2, Z1, B_ROOF);
    for (int x = X0; x <= X1; x += 2) { g_set_w(g, x, TOP + 3, Z0, B_WALL); g_set_w(g, x, TOP + 3, Z1, B_WALL); }
    for (int z = Z0; z <= Z1; z += 2) { g_set_w(g, X0, TOP + 3, z, B_WALL); g_set_w(g, X1, TOP + 3, z, B_WALL); }
    /* tall windows of coloured glass */
    for (int x = X0 + 6; x < X1 - 4; x += 8) {
        box(g, x, G + 8, Z0, x + 1, G + 14, Z0 + 1, B_GLASS);
        box(g, x, G + 8, Z1 - 1, x + 1, G + 14, Z1, B_GLASS);
    }
    for (int z = Z0 + 8; z < Z1 - 4; z += 8) {
        box(g, X0, G + 8, z, X0 + 1, G + 14, z + 1, B_GLASS);
        box(g, X1 - 1, G + 8, z, X1, G + 14, z + 1, B_GLASS);
    }
    /* the great gate */
    box(g, 60, G, Z0, 67, G + 9, Z0 + 1, B_AIR);
    box(g, 59, G, Z0 - 1, 59, G + 10, Z0 - 1, B_PILLAR);
    box(g, 68, G, Z0 - 1, 68, G + 10, Z0 - 1, B_PILLAR);
    box(g, 59, G + 10, Z0 - 1, 68, G + 10, Z0 - 1, B_ACCENT);
    /* partitions */
    box(g, 39, G, Z0, 39, TOP, 86, B_WALL);  /* hall | bath */
    box(g, 88, G, Z0, 88, TOP, 86, B_WALL);  /* hall | dining */
    box(g, X0, G, 86, X1, TOP, 86, B_WALL);  /* south rooms | north rooms */
    box(g, 63, G, 86, 63, TOP, Z1, B_WALL);  /* library | bedchamber */
    door_x(g, 39, 64, 66);
    door_x(g, 88, 64, 66);
    door_z(g, 86, 50, 52);
    door_z(g, 86, 75, 77);
    door_x(g, 63, 100, 102);

    /* great hall: two rows of pillars, braziers, banners, a throne at the far end */
    for (int z = 50; z <= 82; z += 8) {
        box(g, 47, G, z, 48, TOP, z + 1, B_PILLAR);
        box(g, 79, G, z, 80, TOP, z + 1, B_PILLAR);
        brazier(g, 52, z);
        brazier(g, 75, z);
    }
    for (int z = 52; z <= 80; z += 14) {
        box(g, 40, G + 6, z, 40, G + 14, z + 2, B_ACCENT);
        box(g, 87, G + 6, z, 87, G + 14, z + 2, B_ACCENT);
    }
    box(g, 56, F, 46, 71, F, 84, B_ACCENT); /* the long carpet */
    box(g, 61, G, 82, 66, G, 85, B_PILLAR);  /* the dais */
    box(g, 63, G + 1, 83, 64, G + 1, 84, B_ACCENT);
    box(g, 62, G + 1, 85, 65, G + 4, 85, B_ACCENT); /* the throne's back */

    /* bath hall: a warm pool, columns round it, lanterns */
    box(g, POCKET_BATH_X0 - 1, F, POCKET_BATH_Z0 - 1, POCKET_BATH_X1 + 1, F, POCKET_BATH_Z1 + 1, B_PILLAR);
    box(g, POCKET_BATH_X0, F - 1, POCKET_BATH_Z0, POCKET_BATH_X1, F - 1, POCKET_BATH_Z1, B_PILLAR);
    box(g, POCKET_BATH_X0, F, POCKET_BATH_Z0, POCKET_BATH_X1, F, POCKET_BATH_Z1, B_WATER);
    for (int z = POCKET_BATH_Z0 - 2; z <= POCKET_BATH_Z1 + 2; z += 6) {
        box(g, 19, G, z, 19, G + 6, z, B_PILLAR);
        box(g, 37, G, z, 37, G + 6, z, B_PILLAR);
        g_set_w(g, 19, G + 7, z, B_LANTERN);
        g_set_w(g, 37, G + 7, z, B_LANTERN);
    }

    /* dining hall: the long table, a great fireplace, lamps */
    box(g, POCKET_TABLE_X0, G, POCKET_TABLE_Z0, POCKET_TABLE_X1, G, POCKET_TABLE_Z1, B_WOOD);
    box(g, 106, G, 60, 109, G + 6, 70, B_WALL); /* the chimney breast */
    box(g, 106, G, 63, 107, G + 2, 67, B_AIR);  /* the fire place */
    box(g, 107, G, 64, 107, G, 66, B_FIRE);
    box(g, 105, G + 3, 62, 105, G + 3, 68, B_ACCENT); /* mantel */
    for (int z = 56; z <= 76; z += 10) g_set_w(g, 98, G + 6, z, B_LANTERN);
    for (int z = 50; z <= 82; z += 8) { brazier(g, 90, z); }

    /* library: shelves along the walls, a reading table, the shrine */
    for (int x = 20; x <= 60; x += 1)
        for (int k = 0; k < 2; k++) {
            int z = k ? 117 : 88;
            if (x % 6 == 2) box(g, x, G, z, x, G + 5, z, B_WOOD);
            else for (int y = G; y <= G + 5; y++) g_set_w(g, x, y, z, ((y - G) & 1) ? B_ROOF : B_ACCENT); /* rows of books */
        }
    box(g, 30, G, 100, 34, G, 103, B_WOOD);
    g_set_w(g, 32, G + 4, 101, B_LANTERN);
    g_set_w(g, 50, G, 102, B_SHRINE);
    g_set_w(g, 50, G + 4, 102, B_LANTERN);

    /* bedchamber: a canopy bed, a rug, a wardrobe, lamps */
    box(g, 80, F, 96, 105, F, 112, B_ACCENT);
    box(g, POCKET_BED_X0, G, POCKET_BED_Z0, POCKET_BED_X1, G, POCKET_BED_Z1, B_WOOD);
    box(g, POCKET_BED_X0, G + 1, POCKET_BED_Z0 + 4, POCKET_BED_X1, G + 1, POCKET_BED_Z1, B_FLOOR); /* pillows */
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++) box(g, a ? POCKET_BED_X1 : POCKET_BED_X0, G + 1, b ? POCKET_BED_Z1 : POCKET_BED_Z0, a ? POCKET_BED_X1 : POCKET_BED_X0, G + 4, b ? POCKET_BED_Z1 : POCKET_BED_Z0, B_PILLAR);
    box(g, POCKET_BED_X0, G + 5, POCKET_BED_Z0, POCKET_BED_X1, G + 5, POCKET_BED_Z1, B_ACCENT);
    box(g, 70, G, 113, 74, G + 4, 117, B_WOOD); /* wardrobe */
    g_set_w(g, 90, G + 6, 104, B_LANTERN);
    g_set_w(g, 75, G + 6, 95, B_LANTERN);
    brazier(g, 66, 90);
    brazier(g, 108, 90);

    /* towers at the corners */
    tower(g, X0, Z0, 6, G + 30);
    tower(g, X1, Z0, 6, G + 30);
    tower(g, X0, Z1, 6, G + 30);
    tower(g, X1, Z1, 6, G + 30);
    /* the gate stays open through the tower walls */
    box(g, 60, G, Z0, 67, G + 9, Z0 + 1, B_AIR);
}
