#include "world/gen_common.h"
#include "world/realms.h"

#include "core/noise.h"

/* The realms behind the Gate (realms.h, docs/REALMS.md). Every chunk of a realm runs the same
 * plan in world coordinates and the fills clip themselves to the chunk:
 *   1. the land: ground (B_DIRT on top, rock below) by realm_height, or islands in the sky,
 *      or a cavern under a roof of rock, and the realm's liquid up to its sea level;
 *   2. its features (domes, spires, giant mushrooms, rivers of light, ribs of giants...),
 *      on a grid of cells, each from the hash of its cell;
 *   3. the gate home (an arch) and the keep of the ruler, on flat ground;
 *   4. its three plants, in patches.
 * The Mirror Fortress and the Dream are made of fortress chunks instead: mirrored, or taken
 * from all over and stitched together with passages. */

static int R;          /* the realm */
static u64 SEED;
static const RealmDef *D;

static int height(int wx, int wz) { return realm_height(SEED, R, wx, wz); }
static float hf(int x, int z, u32 k) { return hash3f(SEED ^ (u64)R * 0x51ED, x, 0, z, k); }
static void put(GenCtx *g, int x, int y, int z, Block b) { g_set_w(g, x, y, z, b); }
static Block get(GenCtx *g, int x, int y, int z) { return g_get_w(g, x, y, z); }
static bool in_chunk_xz(GenCtx *g, int x, int z, int pad) {
    return x >= g->ox - pad && x < g->ox + CHUNK + pad && z >= g->oz - pad && z < g->oz + CHUNK + pad;
}

/* ---------- islands in the sky ---------- */

#define ISLES 4
static const int ISLE_AT[ISLES] = { REALM_ARRIVE_X, 106, REALM_KEEP_X, 226 };

static void isle_centre(int i, int j, float *x, float *z, float *rad) {
    *x = (float)ISLE_AT[i];
    *z = (float)ISLE_AT[j];
    *rad = 12.0f + 8.0f * hf(i, j, 0x151);
    bool arrive = i == 0 && j == 0, keep = i == 2 && j == 2;
    if (!arrive && !keep) { *x += (hf(i, j, 0x152) - 0.5f) * 14.0f; *z += (hf(i, j, 0x153) - 0.5f) * 14.0f; }
    if (arrive) *rad = 15.0f;
    if (keep) *rad = 27.0f;
    if (i == 3 || j == 3) *rad = fminf(*rad, 16.0f);
}

/* Bridges along the grid: always the way from the arrival to the keep (row 0, then column 2),
 * most of the others. */
static bool bridge_on(int i0, int j0, int i1, int j1) {
    if (j0 == 0 && j1 == 0 && i1 <= 2) return true;
    if (i0 == 2 && i1 == 2 && j1 <= 2) return true;
    return hf(i0 * 7 + i1, j0 * 7 + j1, 0x154) < 0.7f;
}

/* Is (x, z) on an island: its top (first air above it) and how deep it reaches. */
static bool isle_at(int wx, int wz, int *top, int *bottom) {
    int g0 = realm_ground_y(R);
    for (int j = 0; j < ISLES; j++)
        for (int i = 0; i < ISLES; i++) {
            float cx, cz, rad;
            isle_centre(i, j, &cx, &cz, &rad);
            float dx = wx + 0.5f - cx, dz = wz + 0.5f - cz;
            float a = atan2f(dz, dx);
            float re = rad * (0.85f + 0.3f * (noise2(SEED + (u64)(i * 4 + j), cosf(a) * 1.3f, sinf(a) * 1.3f) * 0.5f + 0.5f));
            float d = sqrtf(dx * dx + dz * dz) / re;
            if (d >= 1.0f) continue;
            bool flat = (i == 0 && j == 0) || (i == 2 && j == 2);
            float bump = flat ? 0.0f : fmaxf(0.0f, fbm2(SEED + 3, wx * 0.05f, wz * 0.05f, 3)) * 4.0f * (1.0f - d);
            *top = g0 + (int)bump;
            *bottom = g0 - 2 - (int)((1.0f - d * d) * re * 0.9f + fbm2(SEED + 9, wx * 0.1f, wz * 0.1f, 2) * 3.0f);
            return true;
        }
    return false;
}

static void bridges(GenCtx *g) {
    int g0 = realm_ground_y(R);
    for (int j = 0; j < ISLES; j++)
        for (int i = 0; i < ISLES; i++)
            for (int k = 0; k < 2; k++) {
                int i1 = i + (k == 0), j1 = j + (k == 1);
                if (i1 >= ISLES || j1 >= ISLES || !bridge_on(i, j, i1, j1)) continue;
                float ax, az, ar, bx, bz, br;
                isle_centre(i, j, &ax, &az, &ar);
                isle_centre(i1, j1, &bx, &bz, &br);
                float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
                for (float t = 0; t <= len; t += 0.5f) {
                    float px = ax + (bx - ax) * t / len, pz = az + (bz - az) * t / len;
                    for (int dz = -2; dz <= 2; dz++)
                        for (int dx = -2; dx <= 2; dx++) {
                            int x = (int)floorf(px) + dx, z = (int)floorf(pz) + dz;
                            if (!in_chunk_xz(g, x, z, 0)) continue;
                            float ex = x + 0.5f - px, ez = z + 0.5f - pz;
                            if (ex * ex + ez * ez > 2.6f) continue;
                            if (get(g, x, g0 - 1, z) == B_AIR) put(g, x, g0 - 1, z, B_WOOD);
                        }
                    /* posts every few metres */
                    if (fmodf(t, 4.0f) < 0.5f) {
                        float nx = -(bz - az) / len, nz = (bx - ax) / len;
                        for (int s = -1; s <= 1; s += 2) {
                            int x = (int)floorf(px + nx * 2.2f * s), z = (int)floorf(pz + nz * 2.2f * s);
                            if (in_chunk_xz(g, x, z, 0) && get(g, x, g0, z) == B_AIR) { put(g, x, g0, z, B_WOOD); put(g, x, g0 + 1, z, B_WOOD); }
                        }
                    }
                }
            }
}

/* ---------- the land ---------- */

static int ceiling_at(int wx, int wz) {
    int g0 = realm_ground_y(R);
    int hgt = R == RM_HOLLOW_EARTH ? 56 : 30;
    float edge = fminf(fminf((float)wx, (float)wz), fminf((float)(REALM_W - 1 - wx), (float)(REALM_W - 1 - wz)));
    int c = g0 + hgt + (int)(fbm2(SEED + 11, wx * 0.02f, wz * 0.02f, 3) * 8.0f);
    /* stalactites */
    if (hf(wx, wz, 0x5A1) < 0.04f) c -= 2 + (int)(hf(wx, wz, 0x5A2) * 8.0f);
    if (edge < 6.0f) c = g0 - 10;
    return c;
}

static void land(GenCtx *g) {
    int g0 = realm_ground_y(R), sea = g0 + D->sea;
    for (int lz = 0; lz < CHUNK; lz++)
        for (int lx = 0; lx < CHUNK; lx++) {
            int wx = g->ox + lx, wz = g->oz + lz;
            if (D->terrain == TR_ISLES) {
                int top, bottom;
                bool on = isle_at(wx, wz, &top, &bottom);
                for (int ly = 0; ly < CHUNK; ly++) {
                    int wy = g->oy + ly;
                    Block b = B_AIR;
                    if (on && wy < top && wy >= bottom) b = wy >= top - 1 ? B_DIRT : wy >= top - 3 ? B_DIRT : B_ROCK;
                    g_put(g, lx, ly, lz, b);
                }
                continue;
            }
            int h = height(wx, wz);
            int ceil = D->terrain == TR_CAVERN ? ceiling_at(wx, wz) : 1 << 30;
            bool outside = wx < 0 || wz < 0 || wx >= REALM_W || wz >= REALM_W;
            for (int ly = 0; ly < CHUNK; ly++) {
                int wy = g->oy + ly;
                Block b;
                if (outside && D->terrain == TR_CAVERN) b = B_ROCK;
                else if (wy < h) {
                    b = wy >= h - 4 ? B_DIRT : B_ROCK;
                    if (b == B_ROCK && hf(wx * 3 + wy, wz, 0x0E5) < (R == RM_FORGE || R == RM_FALLEN_STAR || R == RM_RED_MOON ? 0.03f : 0.006f)) b = B_ORE;
                } else if (wy >= ceil) b = B_ROCK;
                else if (D->liquid && wy < sea) b = D->liquid;
                else b = B_AIR;
                g_put(g, lx, ly, lz, b);
            }
            /* lava cools to obsidian where it meets the shore */
            if (D->liquid == B_LAVA && h == sea) g_set_w(g, wx, h - 1, wz, B_OBSIDIAN);
        }
}

/* ---------- features ---------- */

typedef void (*FeatFn)(GenCtx *g, int cx, int cz, int x, int z, float rnd);

/* Every cell of `size` metres near the chunk: its feature at a jittered point, with chance p. */
static void scatter(GenCtx *g, int size, int reach, float p, u32 salt, FeatFn fn) {
    int x0 = floordiv(g->ox - reach, size), x1 = floordiv(g->ox + CHUNK + reach, size);
    int z0 = floordiv(g->oz - reach, size), z1 = floordiv(g->oz + CHUNK + reach, size);
    for (int cz = z0; cz <= z1; cz++)
        for (int cx = x0; cx <= x1; cx++) {
            if (hf(cx, cz, salt) >= p) continue;
            int x = cx * size + 2 + (int)(hf(cx, cz, salt + 1) * (size - 4)), z = cz * size + 2 + (int)(hf(cx, cz, salt + 2) * (size - 4));
            if (x < 8 || z < 8 || x >= REALM_W - 8 || z >= REALM_W - 8) continue;
            /* never on the arrival's plaza or in the keep */
            if (abs(x - REALM_ARRIVE_X) < 12 && abs(z - REALM_ARRIVE_Z) < 12) continue;
            if (abs(x - REALM_KEEP_X) < REALM_KEEP_R + 8 && abs(z - REALM_KEEP_Z) < REALM_KEEP_R + 8) continue;
            fn(g, cx, cz, x, z, hf(cx, cz, salt + 3));
        }
}

static int ground_at(GenCtx *g, int x, int z) {
    if (D->terrain == TR_ISLES) {
        int top, bottom;
        return isle_at(x, z, &top, &bottom) ? top : -1;
    }
    (void)g;
    return height(x, z);
}

static void sphere(GenCtx *g, float cx, float cy, float cz, float r, Block shell, Block inside, bool upper_only) {
    for (int y = (int)floorf(cy - r - 1); y <= cy + r + 1; y++)
        for (int z = (int)floorf(cz - r - 1); z <= cz + r + 1; z++)
            for (int x = (int)floorf(cx - r - 1); x <= cx + r + 1; x++) {
                if (upper_only && y < (int)floorf(cy)) continue;
                if (!in_chunk_xz(g, x, z, 0) || y < g->oy || y >= g->oy + CHUNK) continue;
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy, dz = z + 0.5f - cz;
                float d = sqrtf(dx * dx + dy * dy + dz * dz);
                if (d < r - 1.0f) { if (inside != 255) put(g, x, y, z, inside); }
                else if (d < r && shell != 255) put(g, x, y, z, shell);
            }
}

static void column(GenCtx *g, int x, int z, int y0, int y1, Block b) {
    if (!in_chunk_xz(g, x, z, 0)) return;
    for (int y = y0; y <= y1; y++) put(g, x, y, z, b);
}

/* a cone (spire, iceberg, shard) from (x, y0, z), radius r at the foot, h tall, leaning */
static void cone(GenCtx *g, int x, int y0, int z, float r, int h, float lean_x, float lean_z, Block b) {
    for (int k = 0; k < h; k++) {
        float t = (float)k / h, rr = r * (1.0f - t);
        float px = x + lean_x * k, pz = z + lean_z * k;
        for (int dz = -(int)r - 1; dz <= (int)r + 1; dz++)
            for (int dx = -(int)r - 1; dx <= (int)r + 1; dx++) {
                int cx = (int)floorf(px) + dx, cz = (int)floorf(pz) + dz;
                float ex = cx + 0.5f - px, ez = cz + 0.5f - pz;
                if (ex * ex + ez * ez <= rr * rr + 0.3f && in_chunk_xz(g, cx, cz, 0)) put(g, cx, y0 + k, cz, b);
            }
    }
}

static void f_dome(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    (void)cx; (void)cz;
    int y = ground_at(g, x, z);
    float r = 6.0f + rnd * 5.0f;
    sphere(g, x + 0.5f, (float)y, z + 0.5f, r, B_GLASS, B_AIR, true);
    for (int dz = -(int)r; dz <= (int)r; dz++) /* a paved floor */
        for (int dx = -(int)r; dx <= (int)r; dx++)
            if (dx * dx + dz * dz < (r - 1) * (r - 1) && in_chunk_xz(g, x + dx, z + dz, 0)) put(g, x + dx, y - 1, z + dz, B_FLOOR);
    if (in_chunk_xz(g, x, z, 0)) { put(g, x, y, z, B_BRAZIER); put(g, x, y + 1, z, B_FIRE); }
}

static void f_spire(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    int y = ground_at(g, x, z);
    cone(g, x, y - 2, z, 1.5f + rnd * 1.5f, 10 + (int)(rnd * 18.0f), (hf(cx, cz, 0x77) - 0.5f) * 0.15f, (hf(cx, cz, 0x78) - 0.5f) * 0.15f, rnd < 0.5f ? B_METAL : B_ORE);
}

static void f_mushroom(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    (void)cx; (void)cz;
    int y = ground_at(g, x, z);
    int h = 8 + (int)(rnd * 10.0f);
    float r = 3.0f + rnd * 3.5f;
    for (int dz = 0; dz <= 1; dz++)
        for (int dx = 0; dx <= 1; dx++) column(g, x + dx, z + dz, y, y + h, B_WOOD);
    for (int dy = 0; dy < 3; dy++) {
        float rr = r - dy * 1.2f;
        for (int dz = -(int)rr; dz <= (int)rr + 1; dz++)
            for (int dx = -(int)rr; dx <= (int)rr + 1; dx++)
                if ((dx - 0.5f) * (dx - 0.5f) + (dz - 0.5f) * (dz - 0.5f) <= rr * rr && in_chunk_xz(g, x + dx, z + dz, 0)) put(g, x + dx, y + h + dy, z + dz, dy == 0 ? B_CRYSTAL : B_WALL);
    }
}

static void f_shard(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    int y = ground_at(g, x, z);
    cone(g, x, y - 1, z, 1.0f + rnd, 4 + (int)(rnd * 7.0f), (hf(cx, cz, 0x81) - 0.5f) * 0.6f, (hf(cx, cz, 0x82) - 0.5f) * 0.6f, R == RM_FALLEN_STAR ? B_CRYSTAL : B_GLASS);
}

static void f_iceberg(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    int y = ground_at(g, x, z);
    cone(g, x, y - 3, z, 3.0f + rnd * 3.0f, 8 + (int)(rnd * 16.0f), (hf(cx, cz, 0x91) - 0.5f) * 0.2f, 0, B_ICE);
    /* the frozen dead, in the ice at its foot */
    if (in_chunk_xz(g, x + 2, z, 0) && rnd < 0.5f) put(g, x + 2, y, z, B_BONES);
}

static void f_kelp(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    (void)cx; (void)cz;
    int y = ground_at(g, x, z);
    for (int k = 0; k < 6; k++) {
        int px = x + (int)((hf(x, k, 0xA1) - 0.5f) * 8), pz = z + (int)((hf(z, k, 0xA2) - 0.5f) * 8);
        if (!in_chunk_xz(g, px, pz, 0)) continue;
        int py = height(px, pz), n = 4 + (int)(rnd * 6.0f + hf(px, pz, 0xA3) * 8.0f);
        for (int t = 0; t < n; t++)
            if (get(g, px, py + t, pz) == B_WATER) put(g, px, py + t, pz, B_RFLORA_TALL);
    }
    (void)y;
    /* coral rocks */
    if (rnd < 0.4f) sphere(g, x + 0.5f, (float)height(x, z), z + 0.5f, 2.0f + rnd * 2.0f, B_ACCENT, B_ACCENT, false);
}

static void f_conductor(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    (void)cx; (void)cz;
    int y = ground_at(g, x, z);
    if (y < 0) return;
    column(g, x, z, y, y + 8 + (int)(rnd * 8.0f), B_METAL);
}

static void f_cloud(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    (void)cx; (void)cz;
    int g0 = realm_ground_y(R);
    int top, bottom;
    if (isle_at(x, z, &top, &bottom)) return;
    float y = g0 - 12.0f + rnd * 30.0f;
    for (int k = 0; k < 4; k++)
        sphere(g, x + (hf(x, k, 0xB1) - 0.5f) * 8.0f, y + hf(z, k, 0xB2) * 2.0f, z + (hf(x + 1, k, 0xB3) - 0.5f) * 8.0f, 2.0f + rnd * 2.5f, B_DIRT, B_DIRT, false);
}

static void f_starrock(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    (void)cx; (void)cz;
    int top, bottom;
    if (isle_at(x, z, &top, &bottom)) return;
    float y = realm_ground_y(R) - 20.0f + rnd * 50.0f;
    sphere(g, x + 0.5f, y, z + 0.5f, 1.5f + rnd * 3.0f, B_ROCK, B_ROCK, false);
    if (in_chunk_xz(g, x, z, 0)) put(g, x, (int)(y + 1.5f + rnd * 3.0f), z, B_CRYSTAL);
}

static void f_stalk(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    f_mushroom(g, cx, cz, x, z, rnd);
    if (R == RM_FUNGAL_DEEP) /* spore-shelves on the stalk */
        for (int k = 3; k < 8; k += 2)
            if (in_chunk_xz(g, x + 2, z, 0)) put(g, x + 2, ground_at(g, x, z) + k, z, B_FUNGUS);
}

static void f_ribs(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    int y = ground_at(g, x, z);
    float a = hf(cx, cz, 0xC1) * PI_F;
    float ux = cosf(a), uz = sinf(a);
    int len = 14 + (int)(rnd * 16.0f);
    for (int k = 0; k < len; k++) { /* the spine */
        int px = x + (int)(ux * k), pz = z + (int)(uz * k);
        if (in_chunk_xz(g, px, pz, 0)) put(g, px, y + 6 - (k > len - 4 ? (k - len + 4) * 2 : 0), pz, B_PILLAR);
    }
    for (int k = 2; k < len - 3; k += 3) /* the ribs: arcs over the ground */
        for (int s = -1; s <= 1; s += 2)
            for (int t = 0; t <= 8; t++) {
                float an = t / 8.0f * PI_F * 0.5f;
                float off = sinf(an) * 6.0f, up = cosf(an) * 6.0f;
                int px = x + (int)(ux * k - uz * off * s), pz = z + (int)(uz * k + ux * off * s);
                if (in_chunk_xz(g, px, pz, 0)) put(g, px, y + (int)up, pz, B_PILLAR);
            }
    /* the skull */
    if (rnd < 0.6f) sphere(g, x - ux * 3.0f, y + 3.0f, z - uz * 3.0f, 3.5f, B_PILLAR, B_AIR, false);
}

static void f_shelf(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    int y = ground_at(g, x, z);
    bool along_x = hf(cx, cz, 0xD1) < 0.5f;
    int len = 5 + (int)(rnd * 8.0f), h = 3 + (int)(hf(cx, cz, 0xD2) * 4.0f);
    for (int k = 0; k < len; k++) {
        int px = x + (along_x ? k : 0), pz = z + (along_x ? 0 : k);
        int hh = h - (hf(px, pz, 0xD3) < 0.3f ? (int)(hf(px, pz, 0xD4) * h) : 0); /* burned down in places */
        for (int t = 0; t < hh; t++)
            if (in_chunk_xz(g, px, pz, 0)) put(g, px, y + t, pz, t % 2 ? B_ACCENT : B_WOOD);
        int ax = px + (along_x ? 0 : 1), az = pz + (along_x ? 1 : 0);
        if (in_chunk_xz(g, ax, az, 0) && hf(ax, az, 0xD5) < 0.5f && get(g, ax, y, az) == B_AIR) put(g, ax, y, az, B_ASH);
    }
}

static void tree(GenCtx *g, int x, int y, int z, int h) {
    column(g, x, z, y, y + h, B_WOOD);
    for (int k = 0; k < 3; k++) {
        int r = 2 - (k == 2);
        for (int dz = -r; dz <= r; dz++)
            for (int dx = -r; dx <= r; dx++)
                if (in_chunk_xz(g, x + dx, z + dz, 0) && get(g, x + dx, y + h - 1 + k, z + dz) == B_AIR) put(g, x + dx, y + h - 1 + k, z + dz, B_MOSS);
    }
}

static void f_grove(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    (void)cx; (void)cz;
    for (int k = 0; k < 4; k++) {
        int px = x + (int)((hf(x, k, 0xE1) - 0.5f) * 12), pz = z + (int)((hf(z, k, 0xE2) - 0.5f) * 12);
        int py = ground_at(g, px, pz);
        if (D->liquid && py <= realm_ground_y(R) + D->sea) continue;
        tree(g, px, py, pz, 4 + (int)(rnd * 3.0f + hf(px, pz, 0xE3) * 3.0f));
    }
}

static void f_crystal(GenCtx *g, int cx, int cz, int x, int z, float rnd) {
    for (int k = 0; k < 3; k++) f_shard(g, cx * 3 + k, cz, x + k * 2 - 2, z + (k & 1) * 2, rnd * (0.6f + 0.2f * k));
}

/* Rivers of light: where the noise crosses zero, a channel two deep, full of the liquid. */
static void rivers(GenCtx *g) {
    for (int lz = 0; lz < CHUNK; lz++)
        for (int lx = 0; lx < CHUNK; lx++) {
            int wx = g->ox + lx, wz = g->oz + lz;
            if (wx < 8 || wz < 8 || wx >= REALM_W - 8 || wz >= REALM_W - 8) continue;
            float n = fbm2(SEED + 21, wx * 0.008f, wz * 0.008f, 3);
            if (fabsf(n) > 0.035f) continue;
            if (abs(wx - REALM_KEEP_X) < REALM_KEEP_R + 6 && abs(wz - REALM_KEEP_Z) < REALM_KEEP_R + 6) continue;
            if (abs(wx - REALM_ARRIVE_X) < 10 && abs(wz - REALM_ARRIVE_Z) < 10) continue;
            int h = height(wx, wz);
            put(g, wx, h - 1, wz, D->liquid ? D->liquid : B_LAVA);
            put(g, wx, h - 2, wz, D->liquid ? D->liquid : B_LAVA);
            put(g, wx, h - 3, wz, B_OBSIDIAN);
        }
}

/* The causeway over the Sea of Night: from the arrival to the keep's isle, a stone road at the
 * water's top (the tide covers it), climbing to the plazas at both ends. */
static void causeway(GenCtx *g) {
    int g0 = realm_ground_y(R), sea = g0 + D->sea;
    float ax = REALM_ARRIVE_X + 0.5f, az = REALM_ARRIVE_Z + 0.5f, bx = REALM_KEEP_X + 0.5f, bz = REALM_KEEP_Z + 0.5f;
    float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
    for (int lz = 0; lz < CHUNK; lz++)
        for (int lx = 0; lx < CHUNK; lx++) {
            float px = g->ox + lx + 0.5f, pz = g->oz + lz + 0.5f;
            float t = ((px - ax) * (bx - ax) + (pz - az) * (bz - az)) / (len * len);
            if (t < 0 || t > 1) continue;
            float qx = ax + (bx - ax) * t, qz = az + (bz - az) * t;
            float d = sqrtf((px - qx) * (px - qx) + (pz - qz) * (pz - qz));
            if (d > 2.5f) continue;
            int wx = g->ox + lx, wz = g->oz + lz;
            int top = sea; /* walk at the water's top */
            if (height(wx, wz) > top) continue; /* on land already */
            for (int y = height(wx, wz) - 1; y < top; y++) put(g, wx, y, wz, d > 2.0f ? B_WALL : B_FLOOR);
        }
}

/* The inner sun of the Hollow Earth: a ball of light under the roof. */
static void inner_sun(GenCtx *g) {
    int g0 = realm_ground_y(R);
    sphere(g, REALM_W * 0.5f, g0 + 40.0f, REALM_W * 0.5f, 7.0f, B_CRYSTAL, B_CRYSTAL, false);
    for (int k = 0; k < 6; k++) /* lesser lights */
        sphere(g, 30.0f + hf(k, 0, 0xF1) * 196.0f, g0 + 34.0f + hf(k, 1, 0xF1) * 10.0f, 30.0f + hf(k, 2, 0xF1) * 196.0f, 2.5f, B_CRYSTAL, B_CRYSTAL, false);
}

/* The orrery's rings and moons round the keep's isle. */
static void rings(GenCtx *g) {
    float cx = REALM_KEEP_X + 0.5f, cz = REALM_KEEP_Z + 0.5f, cy = realm_ground_y(R) + 14.0f;
    for (int k = 0; k < 3; k++) {
        float rad = 40.0f + k * 24.0f, tilt = 0.15f + 0.2f * k;
        for (float a = 0; a < 2 * PI_F; a += 0.5f / rad) {
            int x = (int)floorf(cx + cosf(a) * rad), z = (int)floorf(cz + sinf(a) * rad), y = (int)floorf(cy + sinf(a) * rad * tilt);
            if (in_chunk_xz(g, x, z, 0)) { put(g, x, y, z, B_ACCENT); put(g, x, y + 1, z, B_METAL); }
        }
        float ma = hf(k, 0, 0x0F1) * 2 * PI_F;
        sphere(g, cx + cosf(ma) * rad, cy + sinf(ma) * rad * tilt + 2.0f, cz + sinf(ma) * rad, 4.0f + k * 1.5f, k == 0 ? B_ROCK : k == 1 ? B_CRYSTAL : B_ORE,
               k == 0 ? B_ROCK : k == 1 ? B_CRYSTAL : B_ORE, false);
    }
}

static void features(GenCtx *g) {
    switch (D->feature) {
    case FE_DOMES: scatter(g, 40, 14, 0.45f, 0x100, f_dome); break;
    case FE_SPIRES: scatter(g, 20, 6, 0.5f, 0x110, f_spire); break;
    case FE_MUSHROOMS: scatter(g, 18, 8, 0.6f, 0x120, f_mushroom); break;
    case FE_LIGHTRIVERS: rivers(g); scatter(g, 30, 4, 0.4f, 0x130, f_shard); break;
    case FE_MAGMA: scatter(g, 24, 6, 0.5f, 0x140, f_spire); break;
    case FE_SHARDS: scatter(g, 14, 6, 0.4f, 0x150, f_shard); break;
    case FE_KELP: scatter(g, 12, 6, 0.8f, 0x160, f_kelp); break;
    case FE_ICEBERGS: scatter(g, 26, 8, 0.5f, 0x170, f_iceberg); break;
    case FE_CAUSEWAY: causeway(g); break;
    case FE_CONDUCTORS: scatter(g, 14, 2, 0.5f, 0x180, f_conductor); break;
    case FE_CLOUDS: scatter(g, 22, 12, 0.5f, 0x190, f_cloud); break;
    case FE_STARROCKS: scatter(g, 18, 8, 0.6f, 0x1A0, f_starrock); break;
    case FE_INNERSUN: inner_sun(g); scatter(g, 16, 6, 0.4f, 0x1B0, f_grove); break;
    case FE_STALKS: scatter(g, 16, 8, 0.6f, 0x1C0, f_stalk); break;
    case FE_RIBS: scatter(g, 48, 34, 0.5f, 0x1D0, f_ribs); break;
    case FE_RINGS: rings(g); break;
    case FE_SHELVES: scatter(g, 12, 14, 0.6f, 0x1E0, f_shelf); break;
    case FE_GROVES: scatter(g, 20, 8, 0.6f, 0x1F0, f_grove); break;
    case FE_CRYSTALS: scatter(g, 16, 6, 0.6f, 0x200, f_crystal); break;
    default: break;
    }
}

/* ---------- the road ---------- */

/* An old road from the gate to the keep: straight, paved, never steeper than a step a metre,
 * cut through the hills and built up over the low ground, above any sea of lava. */
#define ROAD_MAX 400
static int road_n;
static int road_y[ROAD_MAX];
static u64 road_for;

static void road_profile(void) {
    u64 key = SEED * 31 + (u64)R + 1;
    if (road_for == key) return;
    road_for = key;
    float ax = REALM_ARRIVE_X + 0.5f, az = REALM_ARRIVE_Z + 0.5f, bx = REALM_KEEP_X + 0.5f, bz = REALM_KEEP_Z + 0.5f;
    float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
    road_n = MINI(ROAD_MAX, (int)len + 1);
    int low = D->liquid ? realm_ground_y(R) + D->sea + 1 : -(1 << 30);
    for (int i = 0; i < road_n; i++) {
        float t = (float)i / (road_n - 1);
        int h = height((int)floorf(ax + (bx - ax) * t), (int)floorf(az + (bz - az) * t));
        road_y[i] = MAXI(h, low);
    }
    road_y[0] = height(REALM_ARRIVE_X, REALM_ARRIVE_Z);
    road_y[road_n - 1] = height(REALM_KEEP_X, REALM_KEEP_Z);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 1; i < road_n; i++) road_y[i] = CLAMPI(road_y[i], road_y[i - 1] - 1, road_y[i - 1] + 1);
        for (int i = road_n - 2; i >= 0; i--) road_y[i] = CLAMPI(road_y[i], road_y[i + 1] - 1, road_y[i + 1] + 1);
    }
}

static void road(GenCtx *g) {
    road_profile();
    float ax = REALM_ARRIVE_X + 0.5f, az = REALM_ARRIVE_Z + 0.5f, bx = REALM_KEEP_X + 0.5f, bz = REALM_KEEP_Z + 0.5f;
    float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
    for (int lz = 0; lz < CHUNK; lz++)
        for (int lx = 0; lx < CHUNK; lx++) {
            float px = g->ox + lx + 0.5f, pz = g->oz + lz + 0.5f;
            float t = ((px - ax) * (bx - ax) + (pz - az) * (bz - az)) / (len * len);
            if (t < 0 || t > 1) continue;
            float qx = ax + (bx - ax) * t, qz = az + (bz - az) * t;
            float d = sqrtf((px - qx) * (px - qx) + (pz - qz) * (pz - qz));
            if (d > 2.6f) continue;
            int i = CLAMPI((int)lroundf(t * (road_n - 1)), 0, road_n - 1);
            int y = road_y[i], wx = g->ox + lx, wz = g->oz + lz;
            for (int k = 0; k < 6; k++) put(g, wx, y + k, wz, B_AIR); /* cut through */
            if (d <= 2.0f) {
                put(g, wx, y - 1, wz, B_FLOOR);
                for (int k = 2; k < 14 && block_solid(get(g, wx, y - k, wz)) == false; k++) put(g, wx, y - k, wz, B_ROCK); /* built up */
            } else if (get(g, wx, y - 1, wz) == B_AIR || get(g, wx, y - 1, wz) == D->liquid) put(g, wx, y - 1, wz, B_WALL); /* a kerb */
        }
}

/* A dome's floor: solid, so no water (or anything) comes up from under its rim. */
static void dome_floor(GenCtx *g, int cx, int cy, int cz, float r) {
    for (int z = cz - (int)r; z <= cz + (int)r; z++)
        for (int x = cx - (int)r; x <= cx + (int)r; x++) {
            if ((x - cx) * (x - cx) + (z - cz) * (z - cz) > r * r || !in_chunk_xz(g, x, z, 0)) continue;
            put(g, x, cy - 1, z, B_FLOOR);
            for (int k = 2; k < 16 && !block_solid(get(g, x, cy - k, z)); k++) put(g, x, cy - k, z, B_ROCK);
        }
}

/* ---------- the gate home and the keep ---------- */

static void gate_arch(GenCtx *g, int y) {
    int x = REALM_ARRIVE_X, z = REALM_ARRIVE_Z;
    for (int dz = -5; dz <= 5; dz++) /* a paved ring */
        for (int dx = -5; dx <= 5; dx++)
            if (dx * dx + dz * dz <= 26 && in_chunk_xz(g, x + dx, z + dz, 0)) {
                put(g, x + dx, y - 1, z + dz, B_FLOOR);
                for (int k = 0; k < 6; k++)
                    if (get(g, x + dx, y + k, z + dz) != B_GLASS) put(g, x + dx, y + k, z + dz, B_AIR);
            }
    for (int s = -1; s <= 1; s += 2) column(g, x + 3 * s, z, y, y + 5, B_PILLAR);
    for (int dx = -3; dx <= 3; dx++)
        if (in_chunk_xz(g, x + dx, z, 0)) put(g, x + dx, y + 6, z, abs(dx) == 3 ? B_PILLAR : B_ACCENT);
    if (in_chunk_xz(g, x, z, 0)) put(g, x, y + 7, z, B_CRYSTAL);
}

static void keep(GenCtx *g, int y) {
    int x0 = REALM_KEEP_X - REALM_KEEP_R, x1 = REALM_KEEP_X + REALM_KEEP_R, z0 = REALM_KEEP_Z - REALM_KEEP_R, z1 = REALM_KEEP_Z + REALM_KEEP_R;
    int top = y + 10;
    if (!(g->ox <= x1 + 6 && g->ox + CHUNK > x0 - 6 && g->oz <= z1 + 6 && g->oz + CHUNK > z0 - 6)) return;
    g_fill_w(g, x0 - 4, y - 3, z0 - 4, x1 + 4, y - 1, z1 + 4, B_FLOOR); /* the footing */
    g_fill_w(g, x0, y, z0, x1, top + 6, z1, B_AIR);
    g_fill_w(g, x0, y, z0, x1, top, z1, B_WALL);
    g_fill_w(g, x0 + 1, y, z0 + 1, x1 - 1, top - 1, z1 - 1, B_AIR);
    g_fill_w(g, x0 + 1, y - 1, z0 + 1, x1 - 1, y - 1, z1 - 1, B_FLOOR);
    g_fill_w(g, x0 + 1, top, z0 + 1, x1 - 1, top, z1 - 1, B_CEIL);
    /* doors toward the arrival: west and north, and windows */
    g_fill_w(g, x0, y, REALM_KEEP_Z - 2, x0, y + 4, REALM_KEEP_Z + 2, B_AIR);
    g_fill_w(g, REALM_KEEP_X - 2, y, z0, REALM_KEEP_X + 2, y + 4, z0, B_AIR);
    for (int k = x0 + 3; k < x1 - 2; k += 5) {
        g_fill_w(g, k, y + 6, z1, k + 1, y + 8, z1, B_GLASS);
        g_fill_w(g, x1, y + 6, k, x1, y + 8, k + 1, B_GLASS);
    }
    /* towers at the corners */
    for (int c = 0; c < 4; c++) {
        int tx = c & 1 ? x1 - 2 : x0 - 2, tz = c & 2 ? z1 - 2 : z0 - 2;
        g_fill_w(g, tx, y, tz, tx + 4, top + 6, tz + 4, B_WALL);
        g_fill_w(g, tx + 1, top + 7, tz + 1, tx + 3, top + 7, tz + 3, B_ROOF);
        put(g, tx + 2, top + 8, tz + 2, B_BRAZIER);
        put(g, tx + 2, top + 9, tz + 2, B_FIRE);
    }
    /* inside: pillars, braziers, the throne */
    for (int k = -1; k <= 1; k += 2)
        for (int j = -2; j <= 2; j++) {
            int px = REALM_KEEP_X + j * 4, pz = REALM_KEEP_Z + k * 6;
            g_fill_w(g, px, y, pz, px, top - 1, pz, B_PILLAR);
            if (j != 0) { put(g, px, y, pz + k * -2, B_BRAZIER); put(g, px, y + 1, pz + k * -2, B_FIRE); }
        }
    g_fill_w(g, x1 - 4, y, REALM_KEEP_Z - 2, x1 - 2, y, REALM_KEEP_Z + 2, B_ACCENT); /* the dais */
    g_fill_w(g, x1 - 2, y + 1, REALM_KEEP_Z - 1, x1 - 2, y + 3, REALM_KEEP_Z + 1, B_ACCENT); /* the throne */
    put(g, x1 - 3, y + 1, REALM_KEEP_Z, B_SHRINE);
}

/* ---------- plants ---------- */

static float density(void) {
    switch (R) {
    case RM_GARDEN: return 2.2f;
    case RM_HOLLOW_EARTH: case RM_FUNGAL_DEEP: return 1.8f;
    case RM_PALE_MOON: case RM_GLASS_DESERT: case RM_FALLEN_STAR: case RM_ASTRAL_SHORE: return 0.5f;
    case RM_SUN: case RM_FORGE: return 0.6f;
    default: return 1.0f;
    }
}

static void plants(GenCtx *g) {
    float dn = density();
    for (int lz = 0; lz < CHUNK; lz++)
        for (int lx = 0; lx < CHUNK; lx++) {
            int wx = g->ox + lx, wz = g->oz + lz;
            float patch = fbm2(SEED + 31, wx * 0.05f, wz * 0.05f, 2) * 0.5f + 0.5f;
            for (int ly = CHUNK - 1; ly >= 1; ly--) {
                Block b = g->b[cidx(lx, ly, lz)], below = g->b[cidx(lx, ly - 1, lz)];
                bool wet = b == B_WATER;
                if (b != B_AIR && !(wet && R == RM_OCEAN_KEEP)) continue;
                if (below != B_DIRT && !(below == B_FLOOR && (D->terrain == TR_MIRROR || D->terrain == TR_DREAM))) continue;
                float r = hf(wx * 31 + ly, wz, 0x300);
                int kind = -1;
                if (r < 0.10f * dn * patch * 2.0f) kind = 0;
                else if (r < 0.10f * dn * patch * 2.0f + 0.035f * dn) kind = 1;
                else if (r < 0.10f * dn * patch * 2.0f + 0.035f * dn + 0.015f * dn) kind = 2;
                if (below == B_FLOOR && r > 0.03f) kind = -1; /* in the fortress chunks: sparse */
                if (kind >= 0) {
                    g->b[cidx(lx, ly, lz)] = (Block)(B_RFLORA_LOW + kind);
                    /* tall ones in the Hollow Earth and reeds by the water stand taller */
                    if (kind == 1 && (R == RM_HOLLOW_EARTH || R == RM_SEA_OF_NIGHT) && ly + 1 < CHUNK && g->b[cidx(lx, ly + 1, lz)] == B_AIR && (wx ^ wz) & 1)
                        g->b[cidx(lx, ly + 1, lz)] = B_RFLORA_TALL;
                }
                break;
            }
        }
}

/* ---------- the Mirror and the Dream: made of fortress chunks ---------- */

static void fortress_chunk(GenCtx *g, int sx, int sy, int sz, bool mirror) {
    Block *tmp = malloc(CHUNK_VOL);
    world_generate_chunk(g->w, sx, sy, sz, tmp, NULL);
    for (int y = 0; y < CHUNK; y++)
        for (int z = 0; z < CHUNK; z++)
            for (int x = 0; x < CHUNK; x++) {
                Block b = tmp[cidx(mirror ? CHUNK - 1 - x : x, y, z)];
                if (b == B_SHRINE) b = B_ACCENT; /* (no shrines of the fortress in here) */
                g_put(g, x, y, z, b);
            }
    free(tmp);
}

/* An open hall the size of the chunk on the layer's floor: the arrival and the keep in a
 * realm of fortress chunks. Its sides are whatever the neighbours have. */
static void hall(GenCtx *g) {
    g_fill(g, 0, LAYER_FLOOR, 0, CHUNK - 1, LAYER_FLOOR + 8, CHUNK - 1, B_AIR);
    g_fill(g, 0, LAYER_FLOOR - 2, 0, CHUNK - 1, LAYER_FLOOR - 1, CHUNK - 1, B_FLOOR);
    g_fill(g, 0, LAYER_FLOOR + 9, 0, CHUNK - 1, LAYER_FLOOR + 9, CHUNK - 1, B_CEIL);
    for (int k = 4; k < CHUNK; k += 8)
        for (int j = 4; j < CHUNK; j += 8) {
            g_fill(g, k, LAYER_FLOOR, j, k, LAYER_FLOOR + 8, j, B_PILLAR);
            g_put(g, k + 1, LAYER_FLOOR, j, B_BRAZIER);
            g_put(g, k + 1, LAYER_FLOOR + 1, j, B_FIRE);
        }
}

/* passages through every chunk of the Dream, so its patches join up */
static void seams(GenCtx *g) {
    g_fill(g, 14, LAYER_FLOOR, 0, 17, LAYER_FLOOR + 3, CHUNK - 1, B_AIR);
    g_fill(g, 0, LAYER_FLOOR, 14, CHUNK - 1, LAYER_FLOOR + 3, 17, B_AIR);
    g_fill(g, 14, LAYER_FLOOR - 1, 0, 17, LAYER_FLOOR - 1, CHUNK - 1, B_FLOOR);
    g_fill(g, 0, LAYER_FLOOR - 1, 14, CHUNK - 1, LAYER_FLOOR - 1, 17, B_FLOOR);
}

static void gen_patchwork(GenCtx *g) {
    int cx = g->info.cx, cy = g->info.cy, cz = g->info.cz, base = REALM_CY(R) + 1;
    bool land = cx >= 0 && cx < REALM_SPAN && cz >= 0 && cz < REALM_SPAN;
    int layer = cy - base;
    bool dream = D->terrain == TR_DREAM;
    if (!land || layer < (dream ? 0 : -1) || layer > (dream ? 0 : 1)) { memset(g->b, B_ROCK, CHUNK_VOL); return; }
    if (dream) {
        /* a piece of somewhere: anywhere in the fortress, any height */
        u64 h = hash3(SEED, cx, cy, cz, 0xD4EA);
        int sx = (int)(h % 61) - 30, sy = (int)((h >> 8) % 31) - 15, sz = (int)((h >> 16) % 61) - 30;
        if (world_is_exterior(sx, sy, sz) || (abs(sx) <= 3 && abs(sz) <= 3)) sx += 8;
        fortress_chunk(g, sx, sy, sz, false);
        seams(g);
    } else
        fortress_chunk(g, 8 - cx, layer, cz - 4, true); /* the fortress round the gate, the other way round */
    bool arrive = cx == REALM_ARRIVE_X / CHUNK && cz == REALM_ARRIVE_Z / CHUNK && layer == 0;
    bool kp = cx == REALM_KEEP_X / CHUNK && cz == REALM_KEEP_Z / CHUNK && layer == 0;
    if (arrive || kp) hall(g);
    if (kp) { /* the throne */
        g_fill(g, 24, LAYER_FLOOR, 13, 26, LAYER_FLOOR, 18, B_ACCENT);
        g_fill(g, 26, LAYER_FLOOR + 1, 14, 26, LAYER_FLOOR + 3, 17, B_ACCENT);
    }
    /* the outermost faces: closed */
    for (int y = 0; y < CHUNK; y++)
        for (int a = 0; a < CHUNK; a++) {
            if (cx == 0) g_put(g, 0, y, a, B_ROCK);
            if (cx == REALM_SPAN - 1) g_put(g, CHUNK - 1, y, a, B_ROCK);
            if (cz == 0) g_put(g, a, y, 0, B_ROCK);
            if (cz == REALM_SPAN - 1) g_put(g, a, y, CHUNK - 1, B_ROCK);
        }
}

void gen_realm(GenCtx *g) {
    R = realm_of_flavor(g->info.flavor);
    if (R < 0) return;
    D = &REALMS[R];
    SEED = g->w->seed;
    if (D->terrain == TR_MIRROR || D->terrain == TR_DREAM) {
        gen_patchwork(g);
        if (g->info.cy == REALM_CY(R) + 1) plants(g);
        if (g->info.cx == REALM_ARRIVE_X / CHUNK && g->info.cz == REALM_ARRIVE_Z / CHUNK && g->info.cy == REALM_CY(R) + 1)
            gate_arch(g, (REALM_CY(R) + 1) * CHUNK + LAYER_FLOOR);
        return;
    }
    land(g);
    if (D->terrain == TR_ISLES) bridges(g);
    features(g);
    int ay, ky, ax, az, kx, kz;
    realm_arrival(R, &ax, &ay, &az);
    realm_keep(R, &kx, &ky, &kz);
    if (D->terrain == TR_DEEP) {
        /* air under glass round the arrival and the keep */
        sphere(g, ax + 0.5f, (float)ay, az + 0.5f, 11.0f, B_GLASS, B_AIR, true);
        float kr = REALM_KEEP_R * 1.9f;
        sphere(g, kx + 0.5f, (float)ky, kz + 0.5f, kr, B_GLASS, B_AIR, true);
        dome_floor(g, ax, ay, az, 11.0f);
        dome_floor(g, kx, ky, kz, kr);
        /* a glass tunnel from the arrival's dome to the keep's */
        float len = sqrtf((float)((kx - ax) * (kx - ax) + (kz - az) * (kz - az)));
        for (int lz = 0; lz < CHUNK; lz++)
            for (int lx = 0; lx < CHUNK; lx++) {
                float px = g->ox + lx + 0.5f, pz = g->oz + lz + 0.5f;
                float t = ((px - ax) * (kx - ax) + (pz - az) * (kz - az)) / (len * len);
                if (t < 0 || t > 1) continue;
                float qx = ax + (kx - ax) * t, qz = az + (kz - az) * t;
                float d = sqrtf((px - qx) * (px - qx) + (pz - qz) * (pz - qz));
                if (d > 3.0f) continue;
                int wx = g->ox + lx, wz = g->oz + lz, gy = ay; /* level all the way, through the hills */
                for (int y = gy; y < gy + 5; y++) {
                    bool shell = d > 2.0f || y == gy + 4;
                    Block cur = get(g, wx, y, wz);
                    if (shell) { if (cur == B_WATER || (cur >= B_RFLORA_LOW && cur <= B_RFLORA_GLOW)) put(g, wx, y, wz, B_GLASS); }
                    else put(g, wx, y, wz, B_AIR);
                }
                put(g, wx, gy - 1, wz, B_FLOOR);
            }
    }
    if (D->terrain != TR_ISLES && D->terrain != TR_DEEP && D->terrain != TR_SEA) road(g);
    gate_arch(g, ay);
    keep(g, ky);
    plants(g);
}
