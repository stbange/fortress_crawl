#include "world/gen_common.h"

#include "core/noise.h"

/* The valley in front of the castle (world x -96..-1, z -64..63) with the castle
 * front built into the cliff at x = 0. Hand-designed, but placed through the
 * same clipped world-coordinate helpers as everything else. */

#define VAL_X0 (-96)
#define VAL_Z0 (-64)
#define VAL_Z1 63

static int ground_height(u64 seed, int x, int z) {
    float h = 6.0f + fbm2(seed, x * 0.05f, z * 0.05f, 3) * 2.5f;
    /* flat approach path and courtyard */
    float path = clampf(1.0f - (fabsf((float)z) - 4.0f) / 6.0f, 0.0f, 1.0f);
    float court = (x > -26 && abs(z) < 34) ? 1.0f : 0.0f;
    h = lerpf(h, 6.0f, fmaxf(path, court));
    /* mountain walls on the west, north and south */
    float e = fminf((float)(x - VAL_X0), fminf((float)(z - VAL_Z0), (float)(VAL_Z1 - z)));
    if (e < 26.0f) {
        float t = (26.0f - e) / 26.0f;
        h += powf(t, 1.8f) * 130.0f + fbm2(seed + 5, x * 0.08f, z * 0.08f, 3) * 10.0f * t;
    }
    /* the cliff the castle is built into rises behind the wall ends */
    if (x > -18 && abs(z) > 40) h += (float)(abs(z) - 40) * 3.0f;
    return (int)floorf(h);
}

static void tower(GenCtx *g, int cx, int cz, int r, int height) {
    for (int z = cz - r - 1; z <= cz + r + 1; z++)
        for (int x = cx - r - 1; x <= cx + r + 1; x++) {
            float d = sqrtf((float)((x - cx) * (x - cx) + (z - cz) * (z - cz)));
            if (d > r + 0.5f) continue;
            g_fill_w(g, x, 0, z, x, height, z, B_WALL);
            /* crenellations */
            if (d > r - 0.6f && ((x + z) & 1)) g_set_w(g, x, height + 1, z, B_WALL);
            /* slit windows facing the valley */
            if (d > r - 0.6f && x < cx - r + 2 && abs(z - cz) <= 1)
                for (int wy = 20; wy < height - 4; wy += 18) g_fill_w(g, x, wy, z, x, wy + 4, z, B_GLASS);
        }
    /* conical roof */
    for (int k = 0; k <= r + 2; k++) {
        int rr = r + 1 - k;
        for (int z = cz - rr; z <= cz + rr; z++)
            for (int x = cx - rr; x <= cx + rr; x++)
                if ((x - cx) * (x - cx) + (z - cz) * (z - cz) <= rr * rr) g_set_w(g, x, height + 3 + k * 2, z, B_ROOF),
                                                                          g_set_w(g, x, height + 4 + k * 2, z, B_ROOF);
    }
}

static void brazier(GenCtx *g, int x, int y, int z) {
    g_set_w(g, x, y, z, B_BRAZIER);
    g_set_w(g, x, y + 1, z, B_FIRE);
}

void gen_exterior(GenCtx *g) {
    u64 seed = g->w->seed ^ 0xE47;
    memset(g->b, B_AIR, CHUNK_VOL);

    /* terrain */
    for (int lz = 0; lz < CHUNK; lz++)
        for (int lx = 0; lx < CHUNK; lx++) {
            int wx = g->ox + lx, wz = g->oz + lz;
            int h = ground_height(seed, wx, wz);
            bool on_path = abs(wz) <= 2 && wx > -84;
            for (int ly = 0; ly < CHUNK; ly++) {
                int wy = g->oy + ly;
                if (wy >= h) break;
                Block v = B_ROCK;
                if (wy == h - 1) v = on_path ? B_FLOOR : (h > 14 ? B_ROCK : B_DIRT);
                else if (wy >= h - 3 && h <= 14) v = B_DIRT;
                g->b[cidx(lx, ly, lz)] = v;
            }
            if (!on_path && h <= 14 && h - g->oy >= 0 && h - g->oy < CHUNK && hash3f(seed, wx, 1, wz, 0x6A) < 0.10f)
                g->b[cidx(lx, h - g->oy, lz)] = B_GRASS;
        }

    /* curtain wall with crenellations, built into the cliff */
    g_fill_w(g, -6, 0, -44, -1, 44, 43, B_WALL);
    for (int z = -44; z <= 43; z += 2) g_fill_w(g, -6, 45, z, -6, 46, z, B_WALL);
    /* gatehouse over the gate with a great lit window */
    g_fill_w(g, -9, 0, -8, -1, 58, 7, B_WALL);
    for (int z = -8; z <= 7; z += 2) g_fill_w(g, -9, 59, z, -9, 60, z, B_WALL);
    g_fill_w(g, -9, 26, -2, -9, 36, 1, B_GLASS);
    g_fill_w(g, -9, 25, -3, -9, 25, 2, B_ACCENT);
    /* the gate arch, same shape as on the inside */
    for (int z = -4; z <= 3; z++) {
        float dz = z + 0.5f;
        int top = 6 + 10 + (int)sqrtf(fmaxf(0.0f, 16.0f - dz * dz));
        for (int x = -9; x <= -1; x++) {
            g_set_w(g, x, 5, z, B_FLOOR);
            for (int y = 6; y < top; y++) g_set_w(g, x, y, z, B_AIR);
        }
        /* raised portcullis */
        g_fill_w(g, -5, top - 3, z, -5, top - 1, z, B_BARS);
    }
    /* towers flanking the gate */
    tower(g, -12, -17, 6, 70);
    tower(g, -12, 16, 6, 70);
    /* braziers at the gate and along the path */
    brazier(g, -12, 6, -6);
    brazier(g, -12, 6, 5);
    for (int x = -24; x >= -80; x -= 14) {
        brazier(g, x, ground_height(seed, x, -4), -4);
        brazier(g, x, ground_height(seed, x, 4), 4);
    }
    /* a small shrine where the journey begins */
    {
        int y = ground_height(seed, -76, -7);
        g_fill_w(g, -77, y, -8, -76, y + 1, -7, B_ACCENT);
        g_fill_w(g, -77, y + 2, -8, -76, y + 2, -7, B_SHRINE);
    }

    /* dead trees and a graveyard, placed on a jittered grid */
    for (int gz = floordiv(g->oz - 8, 9); gz <= floordiv(g->oz + CHUNK + 8, 9); gz++)
        for (int gx = floordiv(g->ox - 8, 9); gx <= floordiv(g->ox + CHUNK + 8, 9); gx++) {
            u64 h = hash3(seed, gx, 0, gz, 0x7EE);
            int tx = gx * 9 + (int)(h % 7), tz = gz * 9 + (int)((h >> 8) % 7);
            if (abs(tz) < 9 || tx > -22 || tx < VAL_X0 + 4) continue;
            int gy = ground_height(seed, tx, tz);
            if (gy > 12) continue;
            if (((h >> 16) & 7) < 3) {
                int th = 5 + (int)((h >> 20) % 5);
                g_fill_w(g, tx, gy, tz, tx, gy + th, tz, B_WOOD);
                for (int b = 0; b < 3; b++) {
                    int d = (int)((h >> (24 + b * 3)) & 3);
                    int by = gy + th - 1 - b;
                    for (int k = 1; k <= 2 + b % 2; k++) g_set_w(g, tx + DIR_X[d] * k, by + k / 2, tz + DIR_Z[d] * k, B_WOOD);
                }
            } else if (tz > 12 && tz < 44 && tx > -64 && tx < -30) {
                g_fill_w(g, tx, gy, tz, tx, gy + 1, tz, B_ROCK);
                g_set_w(g, tx, gy, tz + 1, B_DIRT);
            }
        }
}
