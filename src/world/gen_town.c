#include "world/gen_common.h"
#include "world/towns.h"

#include "core/noise.h"

/* A town of a friendly people (towns.h, docs/TOWNS.md): a great hall of three by three chunks
 * under a vaulted roof. In the middle a plaza with a well; round it six buildings (an inn, a
 * temple with a shrine, the elder's hall, a workshop and two houses), a market with stalls and
 * a garden in the corners where the stairs come up and go down; a road round the walls joins
 * the passages in. Each people builds in its own way: dwarves in stone, goblins in scrap,
 * gnomes in brass, elves in living wood, humans in timber, halflings in turf, orcs in iron and
 * bone. Every chunk runs the same plan; the fills clip themselves to the chunk. */

static const Town *T;
static const PeopleDef *P;

static void put(GenCtx *g, int x, int y, int z, Block b) { g_set_w(g, x, y, z, b); }
static void fill(GenCtx *g, int x0, int y0, int z0, int x1, int y1, int z1, Block b) { g_fill_w(g, x0, y0, z0, x1, y1, z1, b); }
static float hh(int x, int z, u32 k) { return hash3f(T->key, x, 0, z, k); }

static void lamp(GenCtx *g, int x, int z) {
    int y = T->floor;
    fill(g, x, y, z, x, y + 2, z, P->build == 3 ? B_WOOD : B_PILLAR);
    put(g, x, y + 3, z, B_LANTERN);
}

static void tree(GenCtx *g, int x, int z, int h, int r) {
    int y = T->floor;
    fill(g, x, y, z, x, y + h, z, B_WOOD);
    for (int dy = -1; dy <= 2; dy++) {
        int rr = r - (dy == 2) - (dy == -1);
        for (int dz = -rr; dz <= rr; dz++)
            for (int dx = -rr; dx <= rr; dx++)
                if (dx * dx + dz * dz <= rr * rr + 1 && hh(x + dx, z + dz + dy * 31, 0x7E) < 0.9f) put(g, x + dx, y + h + dy, z + dz, B_MOSS);
    }
}

/* ---------- buildings ---------- */

static Block wall_block(int x, int y, int z, const TownLot *L) {
    int base = T->floor;
    bool corner = (x == L->x0 || x == L->x1) && (z == L->z0 || z == L->z1);
    switch (P->build) {
    case 0: return y == base ? B_PILLAR : B_WALL;                                         /* dwarves: dressed stone */
    case 1: return hh(x * 7 + y, z, 0x5A) < 0.25f ? B_RUBBLE : hh(x, z * 5 + y, 0x5B) < 0.2f ? B_METAL : B_WOOD; /* goblins: scrap */
    case 2: return y - base >= 3 ? B_ACCENT : B_METAL;                                    /* gnomes: brass on iron */
    case 3: return corner ? B_WOOD : (y - base) % 3 == 2 ? B_MOSS : B_WOOD;               /* elves: living wood */
    case 4: return corner || (x + z) % 4 == 0 || y == base ? B_WOOD : B_WALL;             /* humans: timber framing */
    case 5: return B_DIRT;                                                                /* halflings: turf */
    default: return (y - base) % 3 == 0 ? B_METAL : B_WALL;                               /* orcs: iron bands */
    }
}

static void furnish(GenCtx *g, const TownLot *L, int h) {
    int y = T->floor;
    int ix0 = L->x0 + 1, ix1 = L->x1 - 1, iz0 = L->z0 + 1, iz1 = L->z1 - 1;
    /* the back wall: the one away from the door */
    bool door_x = L->door_x == L->x0 || L->door_x == L->x1;
    int bx = door_x ? (L->door_x == L->x0 ? ix1 : ix0) : (ix0 + ix1) / 2;
    int bz = door_x ? (iz0 + iz1) / 2 : (L->door_z == L->z0 ? iz1 : iz0);
    switch (L->role) {
    case TB_INN:
        /* the counter beside the keeper, tables with lamps, beds along the walls, the hearth */
        if (door_x) fill(g, L->spot_x + (L->door_x == L->x0 ? 1 : -1), y, L->spot_z - 2, L->spot_x + (L->door_x == L->x0 ? 1 : -1), y, L->spot_z + 2, B_WOOD);
        else fill(g, L->spot_x - 2, y, L->spot_z + (L->door_z == L->z0 ? 1 : -1), L->spot_x + 2, y, L->spot_z + (L->door_z == L->z0 ? 1 : -1), B_WOOD);
        for (int k = 0; k < 3; k++) {
            int tx = ix0 + 2 + k * 4, tz = door_x ? iz0 + 2 : iz1 - 2;
            if (tx < ix1 - 1) { put(g, tx, y, tz, B_WOOD); put(g, tx, y + 1, tz, B_LANTERN); }
        }
        for (int k = 0; k < 4; k++) {
            int ex = door_x ? bx : ix0 + 1 + k * 3, ez = door_x ? iz0 + 1 + k * 3 : bz;
            if (ex <= ix1 && ez <= iz1) put(g, ex, y, ez, B_WOOD);
        }
        put(g, ix0, y, iz0, B_BRAZIER);
        put(g, ix0, y + 1, iz0, B_FIRE);
        break;
    case TB_TEMPLE:
        fill(g, bx - (door_x ? 0 : 1), y, bz - (door_x ? 1 : 0), bx + (door_x ? 0 : 1), y, bz + (door_x ? 1 : 0), B_ACCENT);
        put(g, bx, y + 1, bz, B_SHRINE);
        put(g, ix0, y, iz0, B_BRAZIER); put(g, ix0, y + 1, iz0, B_FIRE);
        put(g, ix1, y, iz1, B_BRAZIER); put(g, ix1, y + 1, iz1, B_FIRE);
        break;
    case TB_ELDER:
        fill(g, bx, y, bz, bx, y + 2, bz, B_ACCENT); /* the high seat */
        put(g, ix0 + 1, y, iz0 + 1, B_BRAZIER); put(g, ix0 + 1, y + 1, iz0 + 1, B_FIRE);
        put(g, ix1 - 1, y, iz1 - 1, B_BRAZIER); put(g, ix1 - 1, y + 1, iz1 - 1, B_FIRE);
        put(g, (ix0 + ix1) / 2, y + h - 2, (iz0 + iz1) / 2, B_LANTERN);
        break;
    case TB_WORKSHOP:
        put(g, bx, y, bz, B_BRAZIER); put(g, bx, y + 1, bz, B_FIRE); /* the forge */
        put(g, (bx + L->spot_x) / 2, y, (bz + L->spot_z) / 2, B_METAL); /* the anvil */
        put(g, ix0, y, iz1, B_WOOD); put(g, ix1, y, iz0, B_WOOD);
        break;
    default: /* a home: a bed, a table, a lamp */
        put(g, bx, y, bz, B_WOOD);
        put(g, (ix0 + ix1) / 2, y, (iz0 + iz1) / 2, B_WOOD);
        put(g, (ix0 + ix1) / 2, y + 1, (iz0 + iz1) / 2, B_LANTERN);
        break;
    }
}

static void building(GenCtx *g, const TownLot *L) {
    int y = T->floor;
    int h = L->role == TB_ELDER || L->role == TB_INN || L->role == TB_TEMPLE ? 8 : 6;
    if (P->build == 5) h -= 2; /* burrows are low */
    for (int z = L->z0; z <= L->z1; z++)
        for (int x = L->x0; x <= L->x1; x++) {
            if (!(x >= g->ox && x < g->ox + CHUNK && z >= g->oz && z < g->oz + CHUNK)) continue;
            bool edge = x == L->x0 || x == L->x1 || z == L->z0 || z == L->z1;
            put(g, x, y - 1, z, B_FLOOR);
            for (int k = 0; k < h; k++) put(g, x, y + k, z, edge ? wall_block(x, y + k, z, L) : B_AIR);
            /* windows */
            if (edge && !((x == L->x0 || x == L->x1) && (z == L->z0 || z == L->z1)) && ((x + z) % 4 == 1)) put(g, x, y + 2, z, B_GLASS);
            /* the roof */
            switch (P->build) {
            case 0: case 6: /* flat, with a parapet */
                put(g, x, y + h, z, B_CEIL);
                if (edge && (x + z) % 2 == 0) put(g, x, y + h + 1, z, P->build == 6 ? B_METAL : B_WALL);
                if (P->build == 6 && edge && (x + z) % 6 == 0) put(g, x, y + h + 2, z, B_BONES);
                break;
            case 5: { /* a turf mound with grass on it */
                int dx = MINI(x - L->x0, L->x1 - x), dz = MINI(z - L->z0, L->z1 - z), d = MINI(dx, dz);
                for (int k = 0; k <= MINI(d, 3); k++) put(g, x, y + h + k, z, B_DIRT);
                put(g, x, y + h + MINI(d, 3) + 1, z, B_GRASS);
                break;
            }
            case 3: /* a roof of leaves */
                put(g, x, y + h, z, B_MOSS);
                if (!edge && hh(x, z, 0x3F) < 0.4f) put(g, x, y + h + 1, z, B_MOSS);
                break;
            case 2: { /* a brass dome */
                float cx = (L->x0 + L->x1) * 0.5f, cz = (L->z0 + L->z1) * 0.5f;
                float rx = (L->x1 - L->x0) * 0.5f + 0.5f, rz = (L->z1 - L->z0) * 0.5f + 0.5f;
                float d = sqrtf((x - cx) * (x - cx) / (rx * rx) + (z - cz) * (z - cz) / (rz * rz));
                int top = (int)((1.0f - fminf(d, 1.0f)) * 4.0f);
                put(g, x, y + h, z, B_ROOF);
                for (int k = 1; k <= top; k++) put(g, x, y + h + k, z, k == top ? B_ACCENT : B_ROOF);
                break;
            }
            default: { /* pitched, along the long side */
                bool long_x = (L->x1 - L->x0) >= (L->z1 - L->z0);
                int a = long_x ? z - L->z0 : x - L->x0, w = long_x ? L->z1 - L->z0 : L->x1 - L->x0;
                int up = MINI(a, w - a);
                for (int k = 0; k <= up; k++) put(g, x, y + h + k, z, k == up ? B_ROOF : (edge ? wall_block(x, y + h + k, z, L) : B_AIR));
                put(g, x, y + h + up, z, B_ROOF);
                break;
            }
            }
        }
    /* the door: two wide, three high */
    bool door_x = L->door_x == L->x0 || L->door_x == L->x1;
    for (int k = 0; k < 3; k++)
        for (int w = 0; w < 2; w++) put(g, L->door_x + (door_x ? 0 : w), y + k, L->door_z + (door_x ? w : 0), B_AIR);
    /* a lamp over the door, outside */
    int ox = door_x ? (L->door_x == L->x0 ? -1 : 1) : 0, oz = door_x ? 0 : (L->door_z == L->z0 ? -1 : 1);
    put(g, L->door_x + ox + (door_x ? 0 : 2), y + 3, L->door_z + oz + (door_x ? 2 : 0), B_LANTERN);
    furnish(g, L, h);
}

/* ---------- the market, the garden, the plaza ---------- */

static void stall(GenCtx *g, int x, int z, int k) {
    int y = T->floor;
    fill(g, x - 1, y, z, x + 1, y, z, B_WOOD);                     /* the counter */
    fill(g, x - 1, y, z - 1, x - 1, y + 2, z - 1, B_WOOD);         /* posts */
    fill(g, x + 1, y, z - 1, x + 1, y + 2, z - 1, B_WOOD);
    fill(g, x - 2, y + 3, z - 2, x + 2, y + 3, z + 1, (k & 1) ? B_ROOF : B_ACCENT); /* the awning */
    put(g, x, y + 1, z, B_LANTERN);
}

static void plaza(GenCtx *g) {
    int y = T->floor, cx = T->px, cz = T->pz;
    /* paving round the well */
    for (int dz = -12; dz <= 12; dz++)
        for (int dx = -12; dx <= 12; dx++)
            if (dx * dx + dz * dz <= 150) put(g, cx + dx, y - 1, cz + dz, (abs(dx) + abs(dz)) % 6 == 0 ? B_ACCENT : B_FLOOR);
    if (P->build == 3) { /* the elves: a great tree, a pool at its roots */
        tree(g, cx + 6, cz + 6, 12, 5);
    } else if (P->build == 2) { /* the gnomes: a clock tower */
        fill(g, cx + 6, y, cz + 6, cx + 7, y + 12, cz + 7, B_METAL);
        fill(g, cx + 5, y + 10, cz + 5, cx + 8, y + 11, cz + 8, B_ACCENT);
        put(g, cx + 6, y + 13, cz + 6, B_CRYSTAL);
    }
    /* the well */
    for (int dz = -2; dz <= 2; dz++)
        for (int dx = -2; dx <= 2; dx++) {
            int d = dx * dx + dz * dz;
            if (d <= 1) { put(g, cx + dx, y - 1, cz + dz, B_WATER); put(g, cx + dx, y - 2, cz + dz, B_WATER); }
            else if (d <= 5) put(g, cx + dx, y, cz + dz, B_PILLAR);
        }
    fill(g, cx - 2, y + 1, cz - 2, cx - 2, y + 2, cz - 2, B_WOOD);
    fill(g, cx + 2, y + 1, cz + 2, cx + 2, y + 2, cz + 2, B_WOOD);
    put(g, cx, y + 3, cz, B_LANTERN);
    for (int k = 0; k < 4; k++) lamp(g, cx + (k & 1 ? 9 : -9), cz + (k & 2 ? 9 : -9));
}

static void garden(GenCtx *g, const TownLot *L) {
    int y = T->floor;
    for (int z = L->z0 + 3; z <= L->z1 - 3; z++)
        for (int x = L->x0 + 3; x <= L->x1 - 3; x++) {
            bool rim = x < L->x0 + 8 || x > L->x1 - 8 || z < L->z0 + 8 || z > L->z1 - 8; /* (the stair is in the middle) */
            if (!rim) continue;
            put(g, x, y - 1, z, B_DIRT);
            float r = hh(x, z, 0x6A);
            if (r < 0.25f) put(g, x, y, z, B_FLOWER);
            else if (r < 0.6f) put(g, x, y, z, B_GRASS);
        }
    for (int k = 0; k < 4; k++) tree(g, L->x0 + (k & 1 ? 26 : 5), L->z0 + (k & 2 ? 26 : 5), 5 + (int)(hh(k, 0, 0x6B) * 3), 2);
}

void gen_town(GenCtx *g) {
    Town t;
    if (!town_from_structure(&g->info.st, &t)) return;
    T = &t;
    P = &PEOPLES[t.people];
    const Structure *s = &g->info.st;
    int Y1 = s->cy0 * CHUNK + CHUNK - 1, top = Y1 - 3, floor = t.floor;
    bool earthy = P->build == 1 || P->build == 3 || P->build == 5 || P->build == 6;
    /* the hall: a vault over the whole town, its floor paved or earth by the people */
    for (int ly = 0; ly < CHUNK; ly++)
        for (int lz = 0; lz < CHUNK; lz++)
            for (int lx = 0; lx < CHUNK; lx++) {
                int wx = g->ox + lx, wy = g->oy + ly, wz = g->oz + lz;
                if (wx < t.x0 || wx > t.x1 || wz < t.z0 || wz > t.z1) continue;
                int ds = MINI(MINI(wx - t.x0, t.x1 - wx), MINI(wz - t.z0, t.z1 - wz));
                int cut = MAXI(0, 8 - ds);
                bool road = ds < 6 || abs(wx - t.px) <= 2 || abs(wz - t.pz) <= 2;
                if (wy >= floor && wy <= top - cut) g_put(g, lx, ly, lz, B_AIR);
                else if (wy == floor - 1) g_put(g, lx, ly, lz, road || !earthy ? B_FLOOR : B_DIRT);
            }
    /* grass where the people are earthy */
    if (earthy)
        for (int lz = 0; lz < CHUNK; lz++)
            for (int lx = 0; lx < CHUNK; lx++) {
                int wx = g->ox + lx, wz = g->oz + lz;
                if (wx < t.x0 || wx > t.x1 || wz < t.z0 || wz > t.z1) continue;
                if (g_get_w(g, wx, floor - 1, wz) == B_DIRT && hh(wx, wz, 0x61) < (P->build == 1 ? 0.06f : 0.3f)) put(g, wx, floor, wz, P->build == 1 ? B_FUNGUS : B_GRASS);
            }
    for (int i = 0; i < TOWN_LOTS; i++) {
        const TownLot *L = &t.lot[i];
        if (L->role == TB_MARKET) for (int k = 0; k < t.nstalls; k++) stall(g, t.stall_x[k], t.stall_z[k], k);
        else if (L->role == TB_GARDEN) garden(g, L);
        else if (L->role != TB_PLAZA) building(g, L);
    }
    plaza(g);
    /* lamps along the road round the walls and the ways in from the plaza */
    for (int a = t.x0 + 4; a <= t.x1 - 4; a += 10) { lamp(g, a, t.z0 + 4); lamp(g, a, t.z1 - 4); }
    for (int a = t.z0 + 14; a <= t.z1 - 14; a += 10) { lamp(g, t.x0 + 4, a); lamp(g, t.x1 - 4, a); }
    /* the stairs up and down, the passages in, and stone where rock shows */
    carve_shafts(g);
    carve_portal_mouths(g, 4, false);
    skin_pass(g, true);
}
