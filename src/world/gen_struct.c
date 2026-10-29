#include "world/gen_common.h"

#include "core/noise.h"

/* Megastructures span several chunks. Each chunk generates only its own slice,
 * but every decision is a pure function of world coordinates and the structure
 * seed, so the slices fit together. */

typedef struct Rect { int ix0, iz0, ix1, iz1; } Rect; /* interior, inclusive */

static int wall_dist(const Rect *r, int x, int z) {
    return MINI(MINI(x - r->ix0, r->ix1 - x), MINI(z - r->iz0, r->iz1 - z));
}

static bool in_rect(const Rect *r, int x, int z) {
    return x >= r->ix0 && x <= r->ix1 && z >= r->iz0 && z <= r->iz1;
}

/* Ledges, 3 cells deep, running around all walls at a walkable level. */
static void build_ledge(GenCtx *g, const Rect *r, int level) {
    int y = level - 1;
    if (y < g->oy || y >= g->oy + CHUNK) return;
    for (int lz = 0; lz < CHUNK; lz++)
        for (int lx = 0; lx < CHUNK; lx++) {
            int wx = g->ox + lx, wz = g->oz + lz;
            if (!in_rect(r, wx, wz)) continue;
            int d = wall_dist(r, wx, wz);
            if (d < 3) g_set_w(g, wx, y, wz, d == 2 ? B_ACCENT : B_FLOOR);
        }
}

/* A straight flight of stairs alongside the ledges (cells 3..4 from the wall),
 * from level hi down to level lo. */
static void build_stairs(GenCtx *g, const Rect *r, int hi, int lo, int wall) {
    int dl = hi - lo;
    for (int i = 0; i < dl; i++) {
        int y = hi - 2 - i;
        for (int k = 0; k < 2; k++) {
            int x, z;
            switch (wall) {
            case 0: x = r->ix0 + 5 + i; z = r->iz0 + 3 + k; break;
            case 1: x = r->ix1 - 5 - i; z = r->iz1 - 3 - k; break;
            case 2: z = r->iz0 + 5 + i; x = r->ix0 + 3 + k; break;
            default: z = r->iz1 - 5 - i; x = r->ix1 - 3 - k; break;
            }
            g_set_w(g, x, y, z, B_FLOOR);
            for (int hy = 1; hy <= 3; hy++) g_set_w(g, x, y + hy, z, B_AIR);
        }
    }
}

/* Box in "long axis / short axis" coordinates relative to (al, as). */
static void sbox(GenCtx *g, bool along_x, int al, int as, int a0, int a1, int c0, int c1, int y0, int y1, Block v) {
    if (along_x) g_fill_w(g, al + a0, y0, as + c0, al + a1, y1, as + c1, v);
    else g_fill_w(g, as + c0, y0, al + a0, as + c1, y1, al + a1, v);
}

static void build_colossus(GenCtx *g, bool along_x, int al, int as, int floor, int face) {
    /* face = -1: the statue looks toward the low end of the long axis */
    sbox(g, along_x, al, as, -6, 6, -6, 6, floor, floor + 2, B_ACCENT);
    for (int s = -1; s <= 1; s += 2) {
        sbox(g, along_x, al, as, -1, 1, s * 3 - 1, s * 3 + 1, floor + 3, floor + 12, B_PILLAR);
        sbox(g, along_x, al, as, -1, 1, s * 7 - 1, s * 7 + 1, floor + 12, floor + 23, B_PILLAR);
    }
    sbox(g, along_x, al, as, -2, 2, -5, 5, floor + 13, floor + 24, B_PILLAR);
    sbox(g, along_x, al, as, -2, 2, -2, 2, floor + 25, floor + 29, B_PILLAR);
    sbox(g, along_x, al, as, -2, 2, -3, 3, floor + 30, floor + 30, B_ACCENT);
    sbox(g, along_x, al, as, face * 3, face * 3, -1, -1, floor + 27, floor + 27, B_CRYSTAL);
    sbox(g, along_x, al, as, face * 3, face * 3, 1, 1, floor + 27, floor + 27, B_CRYSTAL);
    /* a great sword planted in front */
    sbox(g, along_x, al, as, face * 4, face * 4, 0, 0, floor + 3, floor + 28, B_METAL);
    sbox(g, along_x, al, as, face * 4, face * 4, -2, 2, floor + 22, floor + 22, B_METAL);
    sbox(g, along_x, al, as, face * 4, face * 4, 0, 0, floor + 29, floor + 30, B_ACCENT);
}

static void carve_gate(GenCtx *g) {
    /* the castle gate: an arch through the vestibule's west wall */
    for (int z = -4; z <= 3; z++) {
        float dz = z + 0.5f;
        int top = 6 + 10 + (int)sqrtf(fmaxf(0.0f, 16.0f - dz * dz));
        for (int x = 0; x <= 3; x++) {
            g_set_w(g, x, 5, z, B_FLOOR);
            g_protect(g, x - g->ox, 5 - g->oy, z - g->oz);
            for (int y = 6; y < top; y++) {
                g_set_w(g, x, y, z, B_AIR);
                g_protect(g, x - g->ox, y - g->oy, z - g->oz);
            }
        }
    }
}

static void gen_hall(GenCtx *g) {
    const Structure *s = &g->info.st;
    bool vest = s->type == STRUCT_VESTIBULE;
    int X0 = s->cx0 * CHUNK, Z0 = s->cz0 * CHUNK, Y0 = s->cy0 * CHUNK;
    int X1 = X0 + s->sx * CHUNK - 1, Z1 = Z0 + s->sz * CHUNK - 1, Y1 = Y0 + s->sy * CHUNK - 1;
    Rect r = { X0 + 3, Z0 + 3, X1 - 3, Z1 - 3 };
    int floor = s->floor_y, top = Y1 - 3;
    bool cistern = !vest && s->variant == 2;
    bool colossus = !vest && s->variant == 1;
    bool along_x = vest || (r.ix1 - r.ix0) >= (r.iz1 - r.iz0);
    int lmin = along_x ? r.ix0 : r.iz0, lmax = along_x ? r.ix1 : r.iz1;
    int smin = along_x ? r.iz0 : r.ix0, smax = along_x ? r.iz1 : r.ix1;
    int smid = (smin + smax) / 2;
    int statue_zone = colossus ? lmax - 24 : lmax + 100;

    /* shell and vaulted ceiling */
    for (int ly = 0; ly < CHUNK; ly++)
        for (int lz = 0; lz < CHUNK; lz++)
            for (int lx = 0; lx < CHUNK; lx++) {
                int wx = g->ox + lx, wy = g->oy + ly, wz = g->oz + lz;
                if (!in_rect(&r, wx, wz)) continue;
                int sc = along_x ? wz : wx;
                int ds = MINI(sc - smin, smax - sc);
                int cut = MAXI(0, 7 - ds);
                if (wy >= floor && wy <= top - cut) g_put(g, lx, ly, lz, B_AIR);
                else if (wy == floor - 1) g_put(g, lx, ly, lz, abs(sc - smid) <= 1 && !cistern ? B_ACCENT : B_FLOOR);
                if (cistern) {
                    int lc = along_x ? wx : wz;
                    bool walkway = wall_dist(&r, wx, wz) < 3 || abs(sc - (smin + 8)) <= 1 || abs(sc - (smax - 8)) <= 1 ||
                                   (lc - lmin) % 20 == 10;
                    if (wy == floor - 1) g_put(g, lx, ly, lz, walkway ? B_FLOOR : B_WATER);
                    if (wy == floor - 2) g_put(g, lx, ly, lz, walkway ? B_ROCK : B_WATER);
                }
            }

    /* galleries and the stairs between them */
    for (int k = 1; k < s->nlevels; k++) build_ledge(g, &r, s->levels[k]);
    int wall0 = (int)(s->seed % 4);
    for (int k = 1; k < s->nlevels; k++) build_stairs(g, &r, s->levels[k], s->levels[k - 1], (wall0 + k - 1) % 4);

    /* pillar rows with braziers */
    int spacing = 10;
    int rows[3] = { smin + 8, smax - 8, smid };
    int nrows = (smax - smin) > 70 ? 3 : 2;
    int pi = 0;
    for (int a = lmin + 8; a <= lmax - 8; a += spacing, pi++) {
        if (a > statue_zone) break;
        for (int ri = 0; ri < nrows; ri++) {
            if (ri == 2 && !cistern && abs(rows[ri] - smid) <= 2 && colossus) continue;
            int c = rows[ri];
            int pb = cistern ? floor - 2 : floor;
            sbox(g, along_x, a, c, -1, 1, -1, 1, pb, top, B_PILLAR);
            sbox(g, along_x, a, c, -1, 1, -1, 1, pb, pb, B_ACCENT);
            sbox(g, along_x, a, c, -2, 2, -2, 2, top - 1, top, B_ACCENT);
            if (ri < 2) {
                int off = c < smid ? 3 : -3;
                if (!cistern) {
                    sbox(g, along_x, a, c + off, 0, 0, 0, 0, floor, floor, B_BRAZIER);
                    sbox(g, along_x, a, c + off, 0, 0, 0, 0, floor + 1, floor + 1, B_FIRE);
                } else {
                    sbox(g, along_x, a, c, 2, 2, 0, 0, floor, floor, B_BRAZIER);
                    sbox(g, along_x, a, c, 2, 2, 0, 0, floor + 1, floor + 1, B_FIRE);
                }
            }
        }
        /* chandelier between pillar pairs */
        int ca = a + spacing / 2;
        if (ca <= lmax - 8 && ca < statue_zone) {
            int yb = floor + 14 + (int)(hash3f(s->seed, ca, 0, 0, 0xC4A) * 8);
            sbox(g, along_x, ca, smid, 0, 0, 0, 0, yb + 2, top, B_CHAIN);
            sbox(g, along_x, ca, smid, 0, 0, 0, 0, yb + 1, yb + 1, B_FIRE);
            sbox(g, along_x, ca, smid, -1, 1, -1, 1, yb, yb, B_METAL);
        }
    }

    /* tall windows in the long walls, one band per level */
    for (int a = lmin + 4; a <= lmax - 4; a += 8)
        for (int k = 0; k < s->nlevels; k++) {
            int y0 = s->levels[k] + 3, y1 = MINI(s->levels[k] + 9, top - 8);
            if (y1 < y0) continue;
            for (int side = 0; side < 2; side++) {
                int c = side == 0 ? smin - 1 : smax + 1;
                sbox(g, along_x, a, c, 0, 1, 0, 0, y0, y1, B_GLASS);
            }
        }

    if (colossus) {
        build_colossus(g, along_x, lmax - 12, smid, floor, -1);
        /* fires at the corners of the pedestal light the statue from below */
        for (int a = -1; a <= 1; a += 2)
            for (int c = -1; c <= 1; c += 2) {
                sbox(g, along_x, lmax - 12 + a * 8, smid + c * 8, 0, 0, 0, 0, floor, floor, B_BRAZIER);
                sbox(g, along_x, lmax - 12 + a * 8, smid + c * 8, 0, 0, 0, 0, floor + 1, floor + 1, B_FIRE);
            }
    }

    if (vest) {
        carve_gate(g);
        Portal p;
        if (world_face_portals(g->w, 1, 1, -1, -1, &p, 1)) carve_helix_w(g, p.x, p.z, 0, p.y_up + 3, p.phase, true);
        if (world_face_portals(g->w, 1, 1, 1, 0, &p, 1)) carve_helix_w(g, p.x, p.z, p.y_low, Y1, p.phase, true);
        /* a shrine opposite the gate */
        g_fill_w(g, lmax - 3, floor, smid - 1, lmax - 1, floor + 1, smid + 1, B_ACCENT);
        g_set_w(g, lmax - 2, floor + 2, smid, B_CRYSTAL);
    }

    carve_portal_mouths(g, 4, false);
    skin_pass(g, true);
    g->light_rate *= 0.6f;
    decorate(g, false);
}

/* ---------- cavern ---------- */

typedef struct CavernShape {
    const Structure *s;
    float cx, cy, cz, rx, ry, rz;
    int X0, Y0, Z0, X1, Y1, Z1;
    int ncol;
    float col[6][3]; /* x, z, radius */
} CavernShape;

static void cavern_shape(const Structure *s, CavernShape *c) {
    c->s = s;
    c->X0 = s->cx0 * CHUNK; c->Y0 = s->cy0 * CHUNK; c->Z0 = s->cz0 * CHUNK;
    c->X1 = c->X0 + s->sx * CHUNK - 1; c->Y1 = c->Y0 + s->sy * CHUNK - 1; c->Z1 = c->Z0 + s->sz * CHUNK - 1;
    c->cx = c->X0 + s->sx * 16.0f; c->cy = c->Y0 + s->sy * 16.0f; c->cz = c->Z0 + s->sz * 16.0f;
    c->rx = s->sx * 16.0f - 5; c->ry = s->sy * 16.0f - 5; c->rz = s->sz * 16.0f - 5;
    Rng r = rng_make(s->seed ^ 0xC01);
    c->ncol = rng_range(&r, 2, 6);
    for (int i = 0; i < c->ncol; i++) {
        c->col[i][0] = c->cx + (rng_float(&r) - 0.5f) * c->rx * 1.2f;
        c->col[i][1] = c->cz + (rng_float(&r) - 0.5f) * c->rz * 1.2f;
        c->col[i][2] = 2.0f + rng_float(&r) * 3.0f;
    }
}

static bool cavern_air(const CavernShape *c, int wx, int wy, int wz) {
    if (wx < c->X0 + 2 || wx > c->X1 - 2 || wz < c->Z0 + 2 || wz > c->Z1 - 2 || wy < c->Y0 + 2 || wy > c->Y1 - 2)
        return false;
    if (wy < cavern_floor(c->s, wx, wz)) return false;
    float qx = (wx - c->cx) / c->rx, qy = (wy - c->cy) / c->ry, qz = (wz - c->cz) / c->rz;
    float d = sqrtf(qx * qx + qy * qy + qz * qz);
    float n = fbm3(c->s->seed, wx * 0.035f, wy * 0.035f, wz * 0.035f, 3) * 0.30f +
              noise3(c->s->seed + 1, wx * 0.11f, wy * 0.11f, wz * 0.11f) * 0.08f;
    if (d + n >= 1.0f) return false;
    for (int i = 0; i < c->ncol; i++) {
        float dx = wx - c->col[i][0], dz = wz - c->col[i][1];
        float cn = noise3(c->s->seed + 2, wx * 0.2f, wy * 0.1f, wz * 0.2f) * 1.5f;
        if (sqrtf(dx * dx + dz * dz) < c->col[i][2] + cn) return false;
    }
    return true;
}

/* Tunnels from every portal of the structure into the cave, following the floor. */
static void cavern_tunnels(GenCtx *g, const CavernShape *c) {
    const Structure *s = c->s;
    for (int cz = s->cz0; cz < s->cz0 + s->sz; cz++)
        for (int cx = s->cx0; cx < s->cx0 + s->sx; cx++)
            for (int d = 0; d < 4; d++) {
                int nx = cx + DIR_X[d], nz = cz + DIR_Z[d];
                if (nx >= s->cx0 && nx < s->cx0 + s->sx && nz >= s->cz0 && nz < s->cz0 + s->sz) continue;
                int axis = DIR_X[d] != 0 ? 0 : 2;
                bool positive = DIR_X[d] + DIR_Z[d] > 0;
                Portal ps[4];
                int np = positive ? world_face_portals(g->w, axis, cx, s->cy0, cz, ps, 4)
                                  : world_face_portals(g->w, axis, nx, s->cy0, nz, ps, 4);
                for (int i = 0; i < np; i++) {
                    const Portal *p = &ps[i];
                    if (p->axis == 1) continue;
                    int plane = axis == 0 ? p->x : p->z;
                    int start = positive ? plane - 1 : plane;
                    int dir = positive ? -1 : 1;
                    int along = axis == 0 ? p->z : p->x;
                    int f = p->y;
                    for (int step = 0; step < 48; step++) {
                        int a = start + dir * step;
                        int mid_x = axis == 0 ? a : along + p->w / 2, mid_z = axis == 0 ? along + p->w / 2 : a;
                        if (step >= 3) {
                            if (cavern_air(c, mid_x, f, mid_z) && cavern_air(c, mid_x, f + 1, mid_z)) break;
                            int target = cavern_floor(s, mid_x, mid_z);
                            f = CLAMPI(target, f - 1, f + 1);
                        }
                        for (int k = 0; k < p->w; k++) {
                            int x = axis == 0 ? a : along + k, z = axis == 0 ? along + k : a;
                            for (int y = f; y <= f + p->h; y++) g_set_w(g, x, y, z, B_AIR);
                            if (!block_solid(g_get_w(g, x, f - 1, z))) g_set_w(g, x, f - 1, z, B_ROCK);
                        }
                    }
                }
            }
}

static void gen_cavern(GenCtx *g) {
    CavernShape c;
    cavern_shape(&g->info.st, &c);
    const Structure *s = &g->info.st;
    int water = s->floor_y - 4;
    for (int ly = 0; ly < CHUNK; ly++)
        for (int lz = 0; lz < CHUNK; lz++)
            for (int lx = 0; lx < CHUNK; lx++) {
                int wx = g->ox + lx, wy = g->oy + ly, wz = g->oz + lz;
                if (!cavern_air(&c, wx, wy, wz)) continue;
                Block v = B_AIR;
                if (wy <= water) v = B_WATER;
                /* stalactites hanging from the roof, stalagmites from the floor */
                float hc = hash3f(s->seed, wx, 0, wz, 0x57A);
                if (hc < 0.015f) {
                    int len = 2 + (int)(hc / 0.015f * 8);
                    for (int k = 1; k <= len; k++)
                        if (!cavern_air(&c, wx, wy + k, wz)) { v = B_ROCK; break; }
                } else if (hc > 0.99f && wy < cavern_floor(s, wx, wz) + 2) {
                    v = B_ROCK;
                }
                g_put(g, lx, ly, lz, v);
            }
    cavern_tunnels(g, &c);
    skin_pass(g, false);
    g->light_rate *= 0.8f;
    decorate(g, true);
}

/* ---------- chasm ---------- */

static void gen_chasm(GenCtx *g) {
    const Structure *s = &g->info.st;
    int X0 = s->cx0 * CHUNK, Z0 = s->cz0 * CHUNK, Y0 = s->cy0 * CHUNK;
    int X1 = X0 + s->sx * CHUNK - 1, Z1 = Z0 + s->sz * CHUNK - 1, Y1 = Y0 + s->sy * CHUNK - 1;
    Rect r = { X0 + 3, Z0 + 3, X1 - 3, Z1 - 3 };
    int bottom = s->floor_y, top = Y1 - 3;
    bool fiery = g->info.stratum == ST_INFERNO || g->info.stratum == ST_CORE || g->info.flavor == FL_EMBER;

    for (int ly = 0; ly < CHUNK; ly++)
        for (int lz = 0; lz < CHUNK; lz++)
            for (int lx = 0; lx < CHUNK; lx++) {
                int wx = g->ox + lx, wy = g->oy + ly, wz = g->oz + lz;
                if (!in_rect(&r, wx, wz) || wy < bottom || wy > top) continue;
                int d = wall_dist(&r, wx, wz);
                if (d < 2 && noise3(s->seed, wx * 0.15f, wy * 0.08f, wz * 0.15f) > 0.25f) continue;
                Block v = B_AIR;
                if (wy == bottom) v = fiery ? B_LAVA : B_ROCK;
                else if (wy == bottom + 1 && !fiery) v = B_WATER;
                g_put(g, lx, ly, lz, v);
            }

    for (int k = 0; k < s->nlevels; k++) build_ledge(g, &r, s->levels[k]);
    for (int k = 1; k < s->nlevels; k++) build_stairs(g, &r, s->levels[k], s->levels[k - 1], (k % 2) ? 0 : 2);
    /* bridges across, near the far corner so they never cross the stairs */
    for (int k = 0; k < s->nlevels; k++) {
        u64 h = hash3(s->seed, k, 0, 0, 0xB21D);
        if ((h & 3) == 0) continue;
        int y = s->levels[k] - 1;
        if (h & 4) g_fill_w(g, r.ix0, y, r.iz1 - 4, r.ix1, y, r.iz1 - 3, B_WOOD);
        else g_fill_w(g, r.ix1 - 4, y, r.iz0, r.ix1 - 3, y, r.iz1, B_WOOD);
    }
    /* braziers on the ledges */
    for (int k = 0; k < s->nlevels; k++) {
        int y = s->levels[k];
        for (int wz = r.iz0; wz <= r.iz1; wz++)
            for (int wx = r.ix0; wx <= r.ix1; wx++)
                if (wall_dist(&r, wx, wz) == 1 && floormod(wx * 7 + wz * 13 + k * 5, 11) == 0 &&
                    g_get_w(g, wx, y, wz) == B_AIR) {
                    g_set_w(g, wx, y, wz, B_BRAZIER);
                    g_set_w(g, wx, y + 1, wz, B_FIRE);
                }
    }
    carve_portal_mouths(g, 4, false);
    skin_pass(g, false);
    g->light_rate *= 0.5f;
    decorate(g, true);
}

void gen_structure(GenCtx *g) {
    switch (g->info.st.type) {
    case STRUCT_VESTIBULE:
    case STRUCT_HALL: gen_hall(g); break;
    case STRUCT_CAVERN: gen_cavern(g); break;
    case STRUCT_CHASM: gen_chasm(g); break;
    default: break;
    }
}
