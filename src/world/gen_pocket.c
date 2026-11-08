#include "world/gen_common.h"
#include "world/pocket.h"

#include "core/noise.h"

/* The Sanctum: a home on a floating island under a night sky, built up plot by plot.
 * The world's sanctum stages (World.sanctum, SP_*) say how far each plot has come; every
 * chunk of the realm runs the same plan and the fills clip themselves to the chunk.
 *
 *   island   1: the four middle plots, bare rock with a rim     2: twice as wide, cliffs
 *            3: towers at the corners, an outer wall, waterfalls off the edge
 *   hall     a ruined room with a bed, a fire, the drafting table and the stash; then
 *            whole walls, a roof and banners; then a throne, pillars and chandeliers
 *   court    where you arrive and the door home; a fountain; statues and hedges
 *   bath, dining hall, library, garden, armory: from ruins to their finest
 * Rooms keep a 2-cell lane between them and doors in the middle of their walls. */

#define G POCKET_G
#define F (POCKET_G - 1) /* the floor cells */

static const u8 *ST; /* the stages being built */
static int st(int plot) { return ST[plot]; }

static void box(GenCtx *g, int x0, int y0, int z0, int x1, int y1, int z1, Block b) { g_fill_w(g, x0, y0, z0, x1, y1, z1, b); }
static float hh(int x, int z, int k) { return hash3f(0x5A9C7, x, k, z, 3); }

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
                if (d > r - 0.6f && ((x * 3 + z) % 7 == 0)) box(g, x, G + 10, z, x, G + 12, z, B_GLASS);
            } else {
                box(g, x, F, z, x, F, z, B_FLOOR);
                box(g, x, top - 1, z, x, top, z, B_CEIL);
            }
        }
    box(g, cx, top + 1, cz, cx, top + 1, cz, B_BRAZIER);
    g_set_w(g, cx, top + 2, cz, B_FIRE);
}

static void brazier(GenCtx *g, int x, int z) {
    g_set_w(g, x, G, z, B_BRAZIER);
    g_set_w(g, x, G + 1, z, B_FIRE);
}

static void lamp_post(GenCtx *g, int x, int z) {
    box(g, x, G, z, x, G + 2, z, B_PILLAR);
    g_set_w(g, x, G + 3, z, B_LANTERN);
}

/* ---------- the island ---------- */

/* Signed distance to the island's rounded edge (negative inside). */
static float island_d(int x, int z, int stage) {
    float half = stage >= 2 ? 62.0f : 33.5f, r = stage >= 2 ? 20.0f : 8.0f;
    float qx = fabsf(x + 0.5f - 64.0f) - half + r, qz = fabsf(z + 0.5f - 64.0f) - half + r;
    float ox = fmaxf(qx, 0.0f), oz = fmaxf(qz, 0.0f);
    return sqrtf(ox * ox + oz * oz) + fminf(fmaxf(qx, qz), 0.0f) - r;
}

static void island(GenCtx *g) {
    int s = st(SP_ISLAND) < 1 ? 1 : st(SP_ISLAND);
    const int R = POCKET_Y0;
    for (int z = 0; z <= 127; z++)
        for (int x = 0; x <= 127; x++) {
            float d = island_d(x, z, s);
            if (d > 0.0f) continue;
            int depth = (int)fminf(40.0f, 4.0f - d * 0.9f + 3.0f * noise3(0x15A, x * 0.15f, 0, z * 0.15f));
            box(g, x, R - depth, z, x, R + 4, z, B_ROCK);
            box(g, x, R + 5, z, x, F, z, B_DIRT);
            if (d > -1.2f) box(g, x, G, z, x, G + 1, z, B_ROCK); /* a rim of rock: nobody walks off */
            else if (hh(x, z, 1) < 0.12f) g_set_w(g, x, G, z, B_GRASS);
            /* the grown island: ivy on the cliffs; the finished one: waterfalls off the edge */
            if (s >= 2 && d > -1.2f && hh(x, z, 2) < 0.3f) g_set_w(g, x, G + 2, z, B_GRASS);
            if (s >= 3 && d > -1.2f && (x == 63 || x == 64 || z == 63 || z == 64))
                box(g, x, R - depth - 30, z, x, G + 1, z, B_WATER);
        }
    if (s >= 3) {
        /* an outer wall just inside the rim, and towers at the corners */
        for (int z = 0; z <= 127; z++)
            for (int x = 0; x <= 127; x++) {
                float d = island_d(x, z, s);
                if (d > -1.2f || d < -2.2f) continue;
                if ((x >= 46 && x <= 49 && (z < 20 || z > 108)) || (z >= 46 && z <= 49 && (x < 20 || x > 108))) continue; /* gates */
                box(g, x, G, z, x, G + 5, z, B_WALL);
                if ((x + z) & 1) g_set_w(g, x, G + 6, z, B_WALL);
            }
        tower(g, 18, 18, 6, G + 24);
        tower(g, 109, 18, 6, G + 24);
        tower(g, 18, 109, 6, G + 24);
        tower(g, 109, 109, 6, G + 24);
    }
}

/* ---------- rooms ---------- */

enum { DOOR_N = 1, DOOR_S = 2, DOOR_W = 4, DOOR_E = 8 };

/* A room on a plot: walls one cell inside its edges, doors in their middles. Stage 0 is a
 * ruin (broken walls, grass); 1 walls without a roof; 2 taller with a roof and windows; 3
 * taller still, battlements, stained glass. Returns the inside height. */
static int shell(GenCtx *g, int x0, int z0, int w, int d, int stage, int doors) {
    int x1 = x0 + w - 1, z1 = z0 + d - 1;
    int ax = x0 + 1, bx = x1 - 1, az = z0 + 1, bz = z1 - 1; /* the walls */
    int mx = (ax + bx) / 2, mz = (az + bz) / 2;
    if (stage <= 0) {
        for (int z = az; z <= bz; z++)
            for (int x = ax; x <= bx; x++) {
                bool wall = x == ax || x == bx || z == az || z == bz;
                if (!wall) { if (hh(x, z, 5) < 0.25f) g_set_w(g, x, G, z, B_GRASS); continue; }
                int h = (int)(hh(x, z, 6) * 3.2f);
                if (h > 0) box(g, x, G, z, x, G + h - 1, z, hh(x, z, 7) < 0.3f ? B_ROCK : B_WALL);
            }
        return 0;
    }
    int h = stage == 1 ? 5 : stage == 2 ? 8 : 11;
    box(g, ax, F, az, bx, F, bz, B_FLOOR);
    box(g, ax, G, az, bx, G + h + 3, bz, B_AIR);
    for (int z = az; z <= bz; z++)
        for (int x = ax; x <= bx; x++) {
            if (!(x == ax || x == bx || z == az || z == bz)) continue;
            int top = h;
            if (stage == 1) top = h - (int)(hh(x, z, 8) * 2.5f); /* still broken along the top */
            box(g, x, G, z, x, G + top, z, B_WALL);
            if (stage >= 3 && ((x + z) & 1)) g_set_w(g, x, G + h + 2, z, B_WALL);
        }
    if (stage >= 2) { /* a roof, beams below it */
        box(g, ax, G + h + 1, az, bx, G + h + 1, bz, B_ROOF);
        box(g, ax + 1, G + h, az + 1, bx - 1, G + h, bz - 1, B_CEIL);
        box(g, ax + 2, G + h, az + 2, bx - 2, G + h, bz - 2, B_AIR);
        /* windows */
        for (int k = ax + 4; k < bx - 2; k += 6) { box(g, k, G + 3, az, k + 1, G + h - 2, az, B_GLASS); box(g, k, G + 3, bz, k + 1, G + h - 2, bz, B_GLASS); }
        for (int k = az + 4; k < bz - 2; k += 6) { box(g, ax, G + 3, k, ax, G + h - 2, k + 1, B_GLASS); box(g, bx, G + 3, k, bx, G + h - 2, k + 1, B_GLASS); }
    }
    if (stage >= 3) for (int k = 0; k < 4; k++) { /* pillars in the corners */
        int px = k & 1 ? bx - 1 : ax + 1, pz = k & 2 ? bz - 1 : az + 1;
        box(g, px, G, pz, px, G + h, pz, B_PILLAR);
    }
    int dh = stage >= 2 ? 4 : 3;
    if (doors & DOOR_N) box(g, mx - 1, G, az, mx + 2, G + dh, az, B_AIR);
    if (doors & DOOR_S) box(g, mx - 1, G, bz, mx + 2, G + dh, bz, B_AIR);
    if (doors & DOOR_W) box(g, ax, G, mz - 1, ax, G + dh, mz + 2, B_AIR);
    if (doors & DOOR_E) box(g, bx, G, mz - 1, bx, G + dh, mz + 2, B_AIR);
    return h;
}

/* lanterns hanging along the walls */
static void lanterns(GenCtx *g, int x0, int z0, int w, int d, int y) {
    for (int k = x0 + 6; k < x0 + w - 4; k += 8) { g_set_w(g, k, y, z0 + 2, B_LANTERN); g_set_w(g, k, y, z0 + d - 3, B_LANTERN); }
}

static void hall(GenCtx *g) {
    int s = st(SP_HALL) < 1 ? 1 : st(SP_HALL), wide = st(SP_ISLAND) >= 2;
    int x0 = plot_x(SP_HALL), z0 = plot_z(SP_HALL);
    int h = shell(g, x0, z0, 32, 32, s, DOOR_S | DOOR_E | (wide ? DOOR_N | DOOR_W : 0));
    /* the fire in the middle: a ring of stones, then braziers round it */
    box(g, 46, F, 45, 49, F, 48, B_ROCK);
    g_set_w(g, 47, G, 46, B_FIRE);
    g_set_w(g, 48, G, 47, B_FIRE);
    if (s >= 2) { brazier(g, 40, 58); brazier(g, 55, 58); box(g, 46, F, 50, 49, F, 61, B_ACCENT); /* a carpet to the door */ }
    /* the bed */
    box(g, POCKET_BED_X0, G, POCKET_BED_Z0, POCKET_BED_X1, G, POCKET_BED_Z1, B_WOOD);
    box(g, POCKET_BED_X0, G + 1, POCKET_BED_Z0, POCKET_BED_X1, G + 1, POCKET_BED_Z0 + 1, B_FLOOR); /* pillows */
    if (s >= 2) {
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) {
                int x = a ? POCKET_BED_X1 : POCKET_BED_X0, z = b ? POCKET_BED_Z1 : POCKET_BED_Z0;
                box(g, x, G + 1, z, x, G + 4, z, B_PILLAR);
            }
        box(g, POCKET_BED_X0, G + 5, POCKET_BED_Z0, POCKET_BED_X1, G + 5, POCKET_BED_Z1, B_ACCENT);
    }
    /* the drafting table, the stash, the shop counter */
    box(g, POCKET_DESK_X, G, POCKET_DESK_Z, POCKET_DESK_X + 1, G, POCKET_DESK_Z + 1, B_WOOD);
    box(g, POCKET_STASH_X, G, POCKET_STASH_Z, POCKET_STASH_X + 1, G, POCKET_STASH_Z + 2, B_WOOD);
    g_set_w(g, POCKET_STASH_X, G + 1, POCKET_STASH_Z + 1, B_ACCENT);
    box(g, 56, G, 51, 59, G, 51, B_WOOD);
    if (s >= 2) { /* banners, lanterns */
        for (int z = 38; z <= 56; z += 9) { box(g, 34, G + 3, z, 34, G + 6, z + 1, B_ACCENT); box(g, 61, G + 3, z, 61, G + 6, z + 1, B_ACCENT); }
        lanterns(g, x0, z0, 32, 32, G + h - 2);
    }
    if (s >= 3) { /* a throne on a dais, pillars, chandeliers */
        box(g, 45, G, 34, 50, G, 36, B_PILLAR);
        box(g, 47, G + 1, 35, 48, G + 1, 35, B_ACCENT);
        box(g, 46, G + 1, 34, 49, G + 4, 34, B_ACCENT);
        for (int k = 0; k < 2; k++) { box(g, 41, G, 41 + k * 12, 41, G + h, 41 + k * 12, B_PILLAR); box(g, 53, G, 43 + k * 12, 53, G + h, 43 + k * 12, B_PILLAR); }
        for (int k = 0; k < 3; k++) g_set_w(g, 47 + (k - 1) * 6, G + h - 1, 47, B_LANTERN);
    }
    if (s == 1) { brazier(g, 40, 40); brazier(g, 56, 56); }
}

static void court(GenCtx *g) {
    int s = st(SP_COURT) < 1 ? 1 : st(SP_COURT);
    /* a paved way from the door home to the hall */
    box(g, 45, F, 64, 50, F, 93, B_FLOOR);
    brazier(g, 43, 86);
    brazier(g, 52, 86);
    if (s >= 2) { /* a raised fountain (one step out of it from anywhere) */
        for (int z = 68; z <= 80; z++)
            for (int x = 33; x <= 43; x++) {
                float d = sqrtf((x - 38.0f) * (x - 38.0f) + (z - 74.0f) * (z - 74.0f));
                if (d <= 4.4f && d > 3.4f) g_set_w(g, x, G, z, B_PILLAR);
                else if (d <= 3.4f) g_set_w(g, x, G, z, B_WATER);
            }
        box(g, 38, G, 74, 38, G + 2, 74, B_PILLAR);
        g_set_w(g, 38, G + 3, 74, B_LANTERN);
        lamp_post(g, 44, 70);
        lamp_post(g, 51, 70);
    }
    if (s >= 3) { /* statues and hedges */
        for (int k = 0; k < 2; k++) {
            int x = k ? 57 : 38;
            box(g, x, G, 88, x + 1, G, 89, B_PILLAR);
            box(g, x, G + 1, 88, x + 1, G + 3, 89, B_ACCENT);
            g_set_w(g, x, G + 4, 88, B_ACCENT);
        }
        for (int x = 34; x <= 61; x++)
            if (x < 44 || x > 51) box(g, x, G, 93, x, G + 1, 93, B_DIRT);
    }
}

static void bath(GenCtx *g) {
    int s = st(SP_BATH);
    int x0 = plot_x(SP_BATH), z0 = plot_z(SP_BATH);
    int doors = DOOR_W | DOOR_S | (st(SP_ISLAND) >= 2 ? DOOR_E : 0);
    int h = shell(g, x0, z0, 32, 32, s, doors);
    if (s <= 0) { box(g, POCKET_BATH_X0, F, POCKET_BATH_Z0, POCKET_BATH_X1, F, POCKET_BATH_Z1, B_ROCK); return; } /* a dry pit */
    box(g, POCKET_BATH_X0 - 1, F, POCKET_BATH_Z0 - 1, POCKET_BATH_X1 + 1, F, POCKET_BATH_Z1 + 1, s >= 3 ? B_PILLAR : B_ROCK);
    box(g, POCKET_BATH_X0, F - 1, POCKET_BATH_Z0, POCKET_BATH_X1, F - 1, POCKET_BATH_Z1, B_PILLAR);
    box(g, POCKET_BATH_X0, F, POCKET_BATH_Z0, POCKET_BATH_X1, F, POCKET_BATH_Z1, B_WATER);
    if (s >= 2)
        for (int z = POCKET_BATH_Z0; z <= POCKET_BATH_Z1; z += 6) {
            box(g, 67, G, z, 67, G + h - 1, z, B_PILLAR);
            box(g, 92, G, z, 92, G + h - 1, z, B_PILLAR);
            g_set_w(g, 68, G + h - 2, z, B_LANTERN);
            g_set_w(g, 91, G + h - 2, z, B_LANTERN);
        }
    else { brazier(g, 68, 36); brazier(g, 91, 59); }
    if (s >= 3) { /* marble round the pool, statues at its corners */
        box(g, 66, F, 34, 93, F, 37, B_PILLAR);
        box(g, 66, F, 58, 93, F, 61, B_PILLAR);
        for (int k = 0; k < 4; k++) {
            int x = k & 1 ? POCKET_BATH_X1 + 1 : POCKET_BATH_X0 - 1, z = k & 2 ? POCKET_BATH_Z1 + 1 : POCKET_BATH_Z0 - 1;
            box(g, x, G, z, x, G + 2, z, B_ACCENT);
        }
    }
}

static void dining(GenCtx *g) {
    int s = st(SP_DINING);
    int x0 = plot_x(SP_DINING), z0 = plot_z(SP_DINING);
    int h = shell(g, x0, z0, 32, 32, s, DOOR_W | DOOR_N);
    if (s <= 0) return;
    if (s == 1) {
        box(g, POCKET_TABLE_X0, G, POCKET_TABLE_Z0_SHORT, POCKET_TABLE_X1, G, POCKET_TABLE_Z1_SHORT, B_WOOD);
        brazier(g, 88, 79);
        return;
    }
    box(g, POCKET_TABLE_X0, G, POCKET_TABLE_Z0, POCKET_TABLE_X1, G, POCKET_TABLE_Z1, B_WOOD);
    /* a fireplace in the east wall */
    box(g, 90, G, 74, 93, G + 5, 85, B_WALL);
    box(g, 90, G, 77, 91, G + 2, 82, B_AIR);
    box(g, 91, G, 78, 91, G, 81, B_FIRE);
    box(g, 89, G + 3, 76, 89, G + 3, 83, B_ACCENT);
    lanterns(g, x0, z0, 32, 32, G + h - 2);
    if (s >= 3) { /* the kitchen in the south-west corner: a hearth, a counter, hanging lanterns */
        box(g, 67, G, 90, 73, G, 92, B_WOOD);
        box(g, 67, G, 86, 68, G + 3, 88, B_WALL);
        g_set_w(g, 68, G, 87, B_FIRE);
        box(g, 66, G + 1, 89, 66, G + 1, 92, B_ACCENT);
        for (int z = 72; z <= 88; z += 8) g_set_w(g, 79, G + h - 2, z, B_LANTERN);
    }
}

static void library(GenCtx *g) {
    if (st(SP_ISLAND) < 2) return;
    int s = st(SP_LIBRARY);
    int x0 = plot_x(SP_LIBRARY), z0 = plot_z(SP_LIBRARY);
    int h = shell(g, x0, z0, 32, 32, s, DOOR_E);
    if (s <= 0) return;
    /* shelves along the north and south walls: empty frames, then rows of books */
    for (int x = x0 + 3; x <= x0 + 28; x++)
        for (int k = 0; k < 2; k++) {
            int z = k ? z0 + 29 : z0 + 2;
            int top = G + (s >= 3 ? 7 : 4);
            if (x % 5 == 0) box(g, x, G, z, x, top, z, B_WOOD);
            else if (s >= 2) for (int y = G; y <= top; y++) g_set_w(g, x, y, z, ((y - G) & 1) ? B_ROOF : B_ACCENT);
            else for (int y = G + 1; y <= top; y += 2) g_set_w(g, x, y, z, B_WOOD);
        }
    if (s >= 2) {
        box(g, 6, G, 45, 10, G, 49, B_WOOD); /* a reading table */
        g_set_w(g, 8, G + 3, 47, B_LANTERN);
        g_set_w(g, POCKET_SHRINE_X, G, POCKET_SHRINE_Z, B_SHRINE);
        g_set_w(g, POCKET_SHRINE_X, G + 4, POCKET_SHRINE_Z, B_LANTERN);
    }
    if (s >= 3) { /* a dome of glass over the middle */
        box(g, x0 + 9, G + h + 1, z0 + 9, x0 + 22, G + h + 1, z0 + 22, B_GLASS);
        box(g, x0 + 11, G + h + 2, z0 + 11, x0 + 20, G + h + 2, z0 + 20, B_GLASS);
        box(g, x0 + 9, G + h, z0 + 9, x0 + 22, G + h, z0 + 22, B_AIR);
        for (int k = 0; k < 4; k++) g_set_w(g, x0 + 6 + k * 6, G + h - 2, z0 + 16, B_LANTERN);
    }
}

static void garden(GenCtx *g) {
    if (st(SP_ISLAND) < 2) return;
    int s = st(SP_GARDEN);
    int x0 = plot_x(SP_GARDEN), z0 = plot_z(SP_GARDEN);
    if (s <= 0) { /* wild ground */
        for (int z = z0 + 2; z < z0 + 30; z++)
            for (int x = x0; x < x0 + 64; x++)
                if (hh(x, z, 11) < 0.35f && g_get_w(g, x, F, z) == B_DIRT && g_get_w(g, x, G, z) == B_AIR) g_set_w(g, x, G, z, B_GRASS);
        return;
    }
    box(g, 46, F, 8, 49, F, 31, B_FLOOR);  /* paths */
    box(g, 36, F, 16, 91, F, 18, B_FLOOR);
    for (int z = z0 + 3; z < z0 + 30; z++)
        for (int x = x0 + 2; x < x0 + 62; x++)
            if (hh(x, z, 12) < 0.2f && g_get_w(g, x, F, z) == B_DIRT && g_get_w(g, x, G, z) == B_AIR) g_set_w(g, x, G, z, B_GRASS);
    lamp_post(g, 45, 22);
    lamp_post(g, 50, 22);
    if (s >= 2) { /* trees and a pond */
        static const int T[][3] = { { 38, 8, 5 }, { 58, 10, 6 }, { 70, 24, 5 }, { 86, 8, 6 }, { 40, 26, 4 }, { 88, 26, 5 } };
        for (int i = 0; i < 6; i++) tree(g, T[i][0], T[i][1], T[i][2]);
        for (int z = 4; z <= 14; z++)
            for (int x = 66; x <= 80; x++) {
                float d = sqrtf((x - 73.0f) * (x - 73.0f) * 0.6f + (z - 9.0f) * (z - 9.0f));
                if (d <= 4.5f) g_set_w(g, x, F, z, B_WATER);
            }
        /* a henhouse: three hens live here (wildlife.c) */
        box(g, 77, G, 27, 81, G + 1, 30, B_WOOD);
        box(g, 78, G, 28, 80, G + 1, 29, B_AIR);
        box(g, 78, G, 27, 78, G, 27, B_AIR); /* the little door */
        box(g, 76, G + 2, 26, 82, G + 2, 31, B_ROOF);
        box(g, 79, G, 28, 80, G, 29, B_GRASS); /* straw */
    }
    if (s >= 3) { /* walls of hedge, flower beds, benches, more light */
        for (int x = x0 + 2; x < x0 + 62; x++)
            if (x < 45 || x > 50) box(g, x, G, z0 + 2, x, G + 1, z0 + 2, B_DIRT);
        for (int k = 0; k < 6; k++) {
            int x = 37 + k * 9, z = 21;
            if (x >= 44 && x <= 51) continue;
            box(g, x, F, z, x + 3, F, z + 1, B_ACCENT); /* flowers */
        }
        box(g, 54, G, 13, 57, G, 13, B_WOOD);
        box(g, 82, G, 19, 85, G, 19, B_WOOD);
        lamp_post(g, 62, 4);
        lamp_post(g, 92, 14);
    }
}

static void armory(GenCtx *g) {
    if (st(SP_ISLAND) < 2) return;
    int s = st(SP_ARMORY);
    int x0 = plot_x(SP_ARMORY), z0 = plot_z(SP_ARMORY);
    int h = shell(g, x0, z0, 32, 32, s, DOOR_W);
    if (s <= 0) return;
    /* racks along the north and south walls (the weapons on them are drawn by realm.c) */
    int racks = s >= 2 ? 10 : 4;
    for (int k = 0; k < racks; k++) {
        int x = x0 + 4 + k * 2 + (k >= 5 ? 4 : 0);
        if (k >= 10) break;
        box(g, x, G, z0 + 2, x, G + 3, z0 + 2, B_WOOD);
        box(g, x, G, z0 + 29, x, G + 3, z0 + 29, B_WOOD);
    }
    brazier(g, x0 + 16, z0 + 8);
    brazier(g, x0 + 16, z0 + 23);
    if (s >= 2) lanterns(g, x0, z0, 32, 32, G + h - 2);
    if (s >= 3) /* plinths for the statues of the chapters (drawn by realm.c) */
        for (int k = 0; k < 6; k++) box(g, x0 + 25, G, z0 + 6 + k * 4, x0 + 26, G, z0 + 7 + k * 4, B_PILLAR);
}

/* the other plots of the grown island: lawns with trees, an orchard once it is finished */
static void lawns(GenCtx *g) {
    if (st(SP_ISLAND) < 2) return;
    static const int P[4][2] = { { 0, 64 }, { 96, 64 }, { 32, 96 }, { 64, 96 } };
    for (int i = 0; i < 4; i++) {
        int x0 = P[i][0], z0 = P[i][1];
        for (int z = z0 + 2; z < z0 + 30; z++)
            for (int x = x0 + 2; x < x0 + 30; x++)
                if (hh(x, z, 20) < 0.2f && g_get_w(g, x, F, z) == B_DIRT && g_get_w(g, x, G, z) == B_AIR) g_set_w(g, x, G, z, B_GRASS);
        int n = st(SP_ISLAND) >= 3 ? 5 : 2;
        for (int k = 0; k < n; k++) {
            int x = x0 + 6 + (int)(hh(i, k, 21) * 20.0f), z = z0 + 6 + (int)(hh(k, i, 22) * 20.0f);
            if (island_d(x, z, st(SP_ISLAND)) < -4.0f) tree(g, x, z, 4 + (int)(hh(x, z, 23) * 3.0f));
        }
    }
    box(g, 45, F, 94, 50, F, 110, B_FLOOR); /* a path south from the court */
}

void gen_pocket(GenCtx *g) {
    memset(g->b, B_AIR, CHUNK_VOL);
    static const u8 START[16] = { 1, 1, 0, 1, 0, 0, 0, 0 };
    ST = g->w ? g->w->sanctum : START;
    if (!ST[SP_ISLAND]) ST = START; /* (a world before its Sanctum state is set) */
    island(g);
    lawns(g);
    garden(g);
    court(g);
    hall(g);
    bath(g);
    dining(g);
    library(g);
    armory(g);
}
