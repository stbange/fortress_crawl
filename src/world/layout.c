#include "world/layout.h"

#include "core/noise.h"

const int DIR_X[4] = { 1, 0, -1, 0 };
const int DIR_Z[4] = { 0, 1, 0, -1 };
const char *PORTAL_NAMES[P_TYPE_COUNT] = { "corridor", "narrow", "crawl", "grand", "gate", "shaft" };

/* Opening sizes (width, height) per portal type. */
static const u8 PORTAL_SIZE[P_TYPE_COUNT][2] = { { 3, 4 }, { 1, 2 }, { 1, 1 }, { 7, 10 }, { 4, 14 }, { 5, 0 } };

#define SUPER 6            /* megastructure placement grid, in chunks */
#define STRUCT_CHANCE 0.7f

void world_init(World *w, u64 seed) { w->seed = seed; }

bool world_is_exterior(int cx, int cy, int cz) {
    return cy >= 0 && cx >= -3 && cx <= -1 && cz >= -2 && cz <= 1;
}

static bool in_vestibule(int cx, int cy, int cz) {
    return cx >= 0 && cx <= 1 && cy >= 0 && cy <= 1 && cz >= -1 && cz <= 0;
}

static float warp(const World *w, u32 salt, int cx, int cy, int cz, float freq) {
    return noise3(w->seed + salt * 7717ULL, cx * freq + 0.31f, cy * freq + 0.17f, cz * freq + 0.73f);
}

int world_stratum(const World *w, int cx, int cy, int cz) {
    if (abs(cx) <= 3 && abs(cz) <= 3 && cy >= -1 && cy <= 2) return ST_DUNGEON;
    float v = cy + warp(w, 1, cx, cy, cz, 0.12f) * 1.8f;
    int vi = (int)floorf(v + 0.5f);
    for (int s = 0; s < ST_COUNT; s++)
        if (vi >= STRATA[s].y_lo && vi <= STRATA[s].y_hi) return s;
    return vi < 0 ? ST_CORE : ST_ASTRAL;
}

int world_flavor(const World *w, int cx, int cy, int cz) {
    float heat = cz + warp(w, 2, cx, cy, cz, 0.09f) * 3.5f; /* + south / hot, - north / cold */
    float cult = cx + warp(w, 3, cx, cy, cz, 0.09f) * 3.5f; /* + east, - west */
    float ah = fabsf(heat), ac = fabsf(cult);
    float mag = ah > ac ? ah : ac;
    if (mag < 4.0f) return FL_OLD;
    bool use_heat;
    if (ah > ac * 1.25f) use_heat = true;
    else if (ac > ah * 1.25f) use_heat = false;
    else use_heat = hash3(w->seed, floordiv(cx, 3), floordiv(cy, 3), floordiv(cz, 3), 0x77) & 1;
    float m = use_heat ? ah : ac;
    int tier = m >= 18.0f ? 3 : (m >= 10.0f ? 2 : 1);
    if (use_heat) return (heat < 0 ? FL_FROST : FL_SANDSTONE) + tier - 1;
    return (cult > 0 ? FL_BOYAR : FL_GOTHIC) + tier - 1;
}

float world_difficulty(int cx, int cy, int cz) {
    float dx = (float)cx, dz = (float)cz, dy = (float)cy;
    return sqrtf(dx * dx + dz * dz) + 1.5f * fabsf(dy);
}

static void make_vestibule(const World *w, Structure *s) {
    memset(s, 0, sizeof *s);
    s->type = STRUCT_VESTIBULE;
    s->cx0 = 0; s->cy0 = 0; s->cz0 = -1;
    s->sx = 2; s->sy = 2; s->sz = 2;
    s->seed = hash3(w->seed, 0, 0, 0, 0xFE57);
    s->floor_y = 6;
    s->levels[0] = 6; s->levels[1] = 22; s->levels[2] = 38;
    s->nlevels = 3;
    s->levels_have_floor = true;
}

static bool boxes_overlap(int ax0, int ay0, int az0, int ax1, int ay1, int az1,
                          int bx0, int by0, int bz0, int bx1, int by1, int bz1) {
    return ax0 <= bx1 && bx0 <= ax1 && ay0 <= by1 && by0 <= ay1 && az0 <= bz1 && bz0 <= az1;
}

static bool cell_structure(const World *w, int gx, int gy, int gz, Structure *s) {
    u64 h = hash3(w->seed, gx, gy, gz, 0x5757);
    if (hash_unit(h) > STRUCT_CHANCE) return false;
    Rng r = rng_make(h);
    int ccx = gx * SUPER + SUPER / 2, ccy = gy * SUPER + SUPER / 2, ccz = gz * SUPER + SUPER / 2;
    int type = pick_weighted(rng_float(&r), STRATA[world_stratum(w, ccx, ccy, ccz)].struct_w, STRUCT_TYPE_COUNT);
    if (type <= STRUCT_VESTIBULE) return false;
    memset(s, 0, sizeof *s);
    s->type = (u8)type;
    s->seed = h;
    switch (type) {
    case STRUCT_HALL:
        s->sx = rng_range(&r, 2, 3); s->sz = rng_range(&r, 2, 3); s->sy = 2;
        s->variant = (u8)pick_weighted(rng_float(&r), (const float[]){ 5, 3, 2 }, 3);
        break;
    case STRUCT_CAVERN:
        s->sx = rng_range(&r, 2, 3); s->sz = rng_range(&r, 2, 3); s->sy = rng_range(&r, 2, 3);
        break;
    case STRUCT_CHASM:
        s->sx = rng_range(&r, 1, 2); s->sz = rng_range(&r, 1, 2); s->sy = rng_range(&r, 3, 5);
        break;
    }
    s->cx0 = gx * SUPER + rng_range(&r, 0, SUPER - s->sx);
    s->cy0 = gy * SUPER + rng_range(&r, 0, SUPER - s->sy);
    s->cz0 = gz * SUPER + rng_range(&r, 0, SUPER - s->sz);
    int x1 = s->cx0 + s->sx - 1, y1 = s->cy0 + s->sy - 1, z1 = s->cz0 + s->sz - 1;
    /* keep the start area and the outside clear */
    if (boxes_overlap(s->cx0, s->cy0, s->cz0, x1, y1, z1, -1, -2, -3, 3, 3, 2)) return false;
    if (boxes_overlap(s->cx0, s->cy0, s->cz0, x1, y1, z1, -3, 0, -2, -1, 1 << 20, 1)) return false;

    int wy0 = s->cy0 * CHUNK, top = (s->cy0 + s->sy) * CHUNK - 4;
    switch (type) {
    case STRUCT_HALL:
        s->floor_y = wy0 + 4;
        s->levels_have_floor = true;
        for (int L = s->floor_y; L <= top - 12 && s->nlevels < 8; L += 16) s->levels[s->nlevels++] = L;
        break;
    case STRUCT_CHASM:
        s->floor_y = wy0 + 2;
        s->levels_have_floor = false;
        for (int L = wy0 + 10; L <= top - 10 && s->nlevels < 8; L += 14) s->levels[s->nlevels++] = L;
        break;
    case STRUCT_CAVERN: {
        int cy = wy0 + s->sy * 16;
        int ry = s->sy * 16 - 5;
        s->floor_y = cy - ry / 2;
        s->levels[0] = s->floor_y;
        s->nlevels = 1;
        s->levels_have_floor = true;
        break;
    }
    }
    return true;
}

bool world_structure_at(const World *w, int cx, int cy, int cz, Structure *out) {
    if (in_vestibule(cx, cy, cz)) {
        make_vestibule(w, out);
        return true;
    }
    if (world_is_exterior(cx, cy, cz)) return false;
    Structure s;
    if (!cell_structure(w, floordiv(cx, SUPER), floordiv(cy, SUPER), floordiv(cz, SUPER), &s)) return false;
    if (cx < s.cx0 || cx >= s.cx0 + s.sx || cy < s.cy0 || cy >= s.cy0 + s.sy || cz < s.cz0 || cz >= s.cz0 + s.sz)
        return false;
    *out = s;
    return true;
}

void world_chunk_info(const World *w, int cx, int cy, int cz, ChunkInfo *ci) {
    memset(ci, 0, sizeof *ci);
    ci->cx = cx; ci->cy = cy; ci->cz = cz;
    ci->seed = hash3(w->seed, cx, cy, cz, 0xC4);
    ci->difficulty = world_difficulty(cx, cy, cz);
    ci->level = (int)ci->difficulty;
    if (world_is_exterior(cx, cy, cz)) {
        ci->stratum = ST_DUNGEON;
        ci->flavor = FL_OLD;
        ci->arch = ARCH_EXTERIOR;
    } else if (world_structure_at(w, cx, cy, cz, &ci->st)) {
        ci->has_struct = true;
        ci->arch = ARCH_STRUCTURE;
        /* one look for the whole structure */
        ci->stratum = (u8)world_stratum(w, ci->st.cx0, ci->st.cy0, ci->st.cz0);
        ci->flavor = (u8)world_flavor(w, ci->st.cx0, ci->st.cy0, ci->st.cz0);
    } else {
        ci->stratum = (u8)world_stratum(w, cx, cy, cz);
        ci->flavor = (u8)world_flavor(w, cx, cy, cz);
        float wts[ARCH_COUNT];
        memcpy(wts, STRATA[ci->stratum].arch_w, sizeof wts);
        wts[ARCH_CRAWL] *= 1.0f + ci->difficulty * 0.03f;
        if (ci->difficulty < 3.0f) wts[ARCH_VOID] = 0;
        int a = pick_weighted(hash_unit(hash3(w->seed, cx, cy, cz, 0xA2C4)), wts, ARCH_COUNT);
        ci->arch = (u8)(a < 0 ? ARCH_ROOMS : a);
        u64 lh = hash3(w->seed, cx, cy, cz, 0x1E7E1);
        ci->levels[0] = 3 + (int)(lh % 6);
        ci->nlevels = 1;
        bool two = ci->arch == ARCH_VOID || (ci->arch != ARCH_ARENA && (lh >> 8) % 10 < 6);
        if (two) ci->levels[ci->nlevels++] = ci->levels[0] + 12 + (int)((lh >> 16) % 5);
    }
    ci->theme = (u16)theme_id(ci->stratum, ci->flavor);
}

int helix_sector(int dx, int dz) {
    float a = atan2f((float)dz, (float)dx);
    return floormod((int)floorf(a / (2.0f * PI_F) * 16.0f + 0.5f), 16);
}

int cavern_floor(const Structure *s, int wx, int wz) {
    return s->floor_y + (int)floorf(fbm2(s->seed, wx * 0.03f, wz * 0.03f, 3) * 7.0f);
}

static void finish_shaft(Portal *p) {
    int ex = DIR_X[p->exit_dir] * 2, ez = DIR_Z[p->exit_dir] * 2;
    p->phase = floormod(p->y_up - 1 - helix_sector(ex, ez), 16);
}

static int vestibule_shafts(const World *w, int lx, int ly, int lz, Portal *out) {
    Portal p;
    memset(&p, 0, sizeof p);
    p.axis = 1; p.type = P_SHAFT; p.w = 5;
    if (lx == 1 && ly == -1 && lz == -1) { /* down from the vestibule floor */
        p.x = 50; p.z = -20; p.y = 0;
        p.y_up = 6; p.y_low = -32 + 6;
        p.low_dir = 2; p.exit_dir = 2;
    } else if (lx == 1 && ly == 1 && lz == 0) { /* up from the vestibule floor */
        p.x = 50; p.z = 20; p.y = 64;
        p.y_low = 6; p.y_up = 64 + 8;
        p.low_dir = 2; p.exit_dir = 2;
    } else {
        return 0;
    }
    finish_shaft(&p);
    *out = p;
    return 1;
}

int world_face_portals(const World *w, int axis, int lx, int ly, int lz, Portal *out, int max) {
    int ux = lx + (axis == 0), uy = ly + (axis == 1), uz = lz + (axis == 2);
    bool eL = world_is_exterior(lx, ly, lz), eU = world_is_exterior(ux, uy, uz);
    if (eL || eU) {
        if (axis == 0 && lx == -1 && ly == 0 && (lz == -1 || lz == 0) && eL && !eU && max > 0) {
            Portal p;
            memset(&p, 0, sizeof p);
            p.axis = 0; p.type = P_GATE; p.w = 4; p.h = 10; /* the rectangular part of the arch */
            p.x = 0; p.y = 6; p.z = lz == -1 ? -4 : 0;
            out[0] = p;
            return 1;
        }
        return 0;
    }
    ChunkInfo A, B;
    world_chunk_info(w, lx, ly, lz, &A);
    world_chunk_info(w, ux, uy, uz, &B);
    Rng r = rng_make(hash3(w->seed, lx, ly, lz, 0xFACE + (u32)axis));

    if (axis == 1) {
        int n = vestibule_shafts(w, lx, ly, lz, out);
        if (n || A.has_struct || B.has_struct || max < 1) return n;
        float p_shaft = fmaxf(STRATA[A.stratum].shaft_chance, STRATA[B.stratum].shaft_chance);
        if (rng_float(&r) >= p_shaft) return 0;
        Portal p;
        memset(&p, 0, sizeof p);
        p.axis = 1; p.type = P_SHAFT; p.w = 5;
        /* shafts through a chunk's floor and ceiling use disjoint zones, and both stay
         * clear of the portal mouths near the side faces */
        p.x = lx * CHUNK + (floormod(ly, 2) == 0 ? rng_range(&r, 10, 13) : rng_range(&r, 18, 21));
        p.z = lz * CHUNK + rng_range(&r, 10, 21);
        p.y = uy * CHUNK;
        p.y_low = ly * CHUNK + A.levels[rng_range(&r, 0, A.nlevels - 1)];
        p.y_up = uy * CHUNK + B.levels[rng_range(&r, 0, B.nlevels - 1)];
        if (p.y_up - p.y_low < 8) return 0;
        p.low_dir = (u8)rng_range(&r, 0, 3);
        p.exit_dir = (u8)rng_range(&r, 0, 3);
        finish_shaft(&p);
        out[0] = p;
        return 1;
    }

    if (A.has_struct && B.has_struct) return 0;
    const ChunkInfo *S = A.has_struct ? &A : (B.has_struct ? &B : NULL);

    int count;
    float cu = rng_float(&r);
    if (S) count = cu < 0.7f ? 1 : 2;
    else count = cu < 0.05f ? 0 : (cu < 0.72f ? 1 : (cu < 0.95f ? 2 : 3));

    float tw[4] = { 60, 15, 8, 17 }; /* corridor, narrow, crawl, grand */
    if (A.arch == ARCH_CRAWL || B.arch == ARCH_CRAWL) { tw[0] = 10; tw[1] = 50; tw[2] = 40; tw[3] = 0; }
    if (A.arch == ARCH_MINE || B.arch == ARCH_MINE) { tw[0] = 80; tw[1] = 10; tw[2] = 5; tw[3] = 5; }
    if (A.arch == ARCH_ARENA || B.arch == ARCH_ARENA) tw[3] += 20;
    if (A.arch == ARCH_VOID || B.arch == ARCH_VOID) { tw[1] = 5; tw[2] = 0; }
    if (S) { tw[0] = 40; tw[1] = 15; tw[2] = 5; tw[3] = 40; }

    int base_y = ly * CHUNK;
    int along_base = axis == 0 ? lz * CHUNK : lx * CHUNK;
    int plane = axis == 0 ? ux * CHUNK : uz * CHUNK;
    int seg = count > 0 ? 26 / count : 26;
    int n = 0;
    for (int i = 0; i < count && n < max; i++) {
        int type = pick_weighted(rng_float(&r), tw, 4);
        int pw = PORTAL_SIZE[type][0], ph = PORTAL_SIZE[type][1];
        int u = 3 + i * seg + rng_range(&r, 0, MAXI(0, seg - pw - 1));
        float vu = rng_float(&r);
        int v;
        if (S) {
            const Structure *st = &S->st;
            if (st->type == STRUCT_CAVERN) {
                if (S->cy != st->cy0) continue;
                int along = along_base + u + pw / 2;
                int wx = axis == 0 ? plane : along, wz = axis == 0 ? along : plane;
                v = cavern_floor(st, wx, wz) - base_y;
                if (v < 2 || v > 30 - ph) continue;
            } else {
                int cand[8], nc = 0;
                for (int k = 0; k < st->nlevels; k++) {
                    int lv = st->levels[k] - base_y;
                    if (lv >= 2 && lv <= 30 - ph) cand[nc++] = lv;
                }
                if (nc == 0) continue;
                v = cand[(int)(vu * nc) % nc];
            }
        } else {
            /* sit on one of the two chunks' floor levels */
            int cand[4], nc = 0;
            for (int k = 0; k < A.nlevels; k++) if (A.levels[k] <= 30 - ph) cand[nc++] = A.levels[k];
            for (int k = 0; k < B.nlevels; k++) if (B.levels[k] <= 30 - ph) cand[nc++] = B.levels[k];
            if (nc == 0) continue;
            v = cand[(int)(vu * nc) % nc];
        }
        Portal p;
        memset(&p, 0, sizeof p);
        p.axis = (u8)axis; p.type = (u8)type; p.w = (u8)pw; p.h = (u8)ph;
        p.y = base_y + v;
        if (axis == 0) { p.x = plane; p.z = along_base + u; }
        else { p.z = plane; p.x = along_base + u; }
        out[n++] = p;
    }
    return n;
}

int world_chunk_portals(const World *w, int cx, int cy, int cz, Portal *out, int max) {
    int n = 0;
    for (int axis = 0; axis < 3; axis++) {
        int ex = axis == 0, ey = axis == 1, ez = axis == 2;
        n += world_face_portals(w, axis, cx - ex, cy - ey, cz - ez, out + n, max - n);
        n += world_face_portals(w, axis, cx, cy, cz, out + n, max - n);
    }
    return n;
}

void world_place_name(const ChunkInfo *ci, char *buf, int n) {
    char tn[96];
    theme_name(ci->theme, tn, sizeof tn);
    if (ci->arch == ARCH_EXTERIOR) snprintf(buf, (size_t)n, "Before the Gates");
    else if (ci->has_struct && ci->st.type == STRUCT_VESTIBULE) snprintf(buf, (size_t)n, "The Grand Vestibule");
    else if (ci->has_struct) snprintf(buf, (size_t)n, "%s - %s", tn, STRUCT_NAMES[ci->st.type]);
    else snprintf(buf, (size_t)n, "%s (%s)", tn, ARCH_NAMES[ci->arch]);
}
