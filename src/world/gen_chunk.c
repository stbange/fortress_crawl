#include "world/gen_common.h"

#include "core/noise.h"

/* Chunk archetypes: every chunk not owned by a megastructure is one of these. */

typedef struct Room { int x0, y0, z0, x1, y1, z1; } Room; /* inclusive air box, y0 = feet */

static bool rooms_overlap(const Room *a, const Room *b, int margin) {
    return a->x0 - margin <= b->x1 && b->x0 - margin <= a->x1 && a->y0 - margin <= b->y1 &&
           b->y0 - margin <= a->y1 && a->z0 - margin <= b->z1 && b->z0 - margin <= a->z1;
}

static int place_rooms(GenCtx *g, Room *rooms, int count, int wmin, int wmax, int hmin, int hmax) {
    int n = 0;
    for (int attempt = 0; attempt < 60 && n < count; attempt++) {
        int w = rng_range(&g->rng, wmin, wmax), d = rng_range(&g->rng, wmin, wmax), h = rng_range(&g->rng, hmin, hmax);
        Room r;
        r.x0 = rng_range(&g->rng, 2, 29 - w);
        r.z0 = rng_range(&g->rng, 2, 29 - d);
        r.y0 = g->info.nlevels ? g->info.levels[rng_range(&g->rng, 0, g->info.nlevels - 1)] : rng_range(&g->rng, 2, MAXI(2, 29 - h));
        r.x1 = r.x0 + w - 1; r.z1 = r.z0 + d - 1; r.y1 = MINI(r.y0 + h - 1, 29);
        bool ok = true;
        for (int i = 0; i < n && ok; i++) ok = !rooms_overlap(&r, &rooms[i], 2);
        if (ok) rooms[n++] = r;
    }
    return n;
}

static void carve_room_box(GenCtx *g, const Room *r) {
    g_fill(g, r->x0, r->y0, r->z0, r->x1, r->y1, r->z1, B_AIR);
    for (int z = r->z0; z <= r->z1; z++)
        for (int x = r->x0; x <= r->x1; x++) {
            if (!block_solid(g_get(g, x, r->y0 - 1, z))) g_set(g, x, r->y0 - 1, z, B_FLOOR);
        }
}

static void carve_room_blob(GenCtx *g, const Room *r) {
    float cx = (r->x0 + r->x1) * 0.5f, cy = (r->y0 + r->y1) * 0.5f, cz = (r->z0 + r->z1) * 0.5f;
    float rx = (r->x1 - r->x0) * 0.5f + 1.0f, ry = (r->y1 - r->y0) * 0.5f + 1.5f, rz = (r->z1 - r->z0) * 0.5f + 1.0f;
    u64 s = g->info.seed ^ 0xB10B;
    for (int y = r->y0; y <= MINI(r->y1 + 2, CHUNK - 2); y++)
        for (int z = MAXI(r->z0 - 2, 1); z <= MINI(r->z1 + 2, CHUNK - 2); z++)
            for (int x = MAXI(r->x0 - 2, 1); x <= MINI(r->x1 + 2, CHUNK - 2); x++) {
                float qx = (x - cx) / rx, qy = (y - cy) / ry, qz = (z - cz) / rz;
                float d = sqrtf(qx * qx + qy * qy + qz * qz);
                float n = noise3(s, (g->ox + x) * 0.2f, (g->oy + y) * 0.2f, (g->oz + z) * 0.2f) * 0.45f;
                /* the floor stays flat so caves remain walkable */
                bool core = x >= r->x0 && x <= r->x1 && z >= r->z0 && z <= r->z1 && y <= r->y0 + 2;
                if (d + n < 1.0f || (core && d < 1.25f)) g_set(g, x, y, z, B_AIR);
            }
}

static void room_pillars(GenCtx *g, const Room *r) {
    int w = r->x1 - r->x0 + 1, d = r->z1 - r->z0 + 1;
    if (w < 9 || d < 9) return;
    int px[2] = { r->x0 + 2, r->x1 - 2 }, pz[2] = { r->z0 + 2, r->z1 - 2 };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) g_fill(g, px[i], r->y0, pz[j], px[i], r->y1, pz[j], B_PILLAR);
}

static void add_room_nodes(GenCtx *g, const Room *rooms, int n) {
    for (int i = 0; i < n; i++)
        g_add_node(g, (rooms[i].x0 + rooms[i].x1) / 2, rooms[i].y0, (rooms[i].z0 + rooms[i].z1) / 2, 99, 99);
}

/* ---------- archetypes ---------- */

static void gen_rooms(GenCtx *g) {
    Room rooms[8];
    int n = place_rooms(g, rooms, rng_range(&g->rng, 3, 6), 5, 13, 4, 8);
    for (int i = 0; i < n; i++) {
        carve_room_box(g, &rooms[i]);
        if (rng_chance(&g->rng, 0.6f)) room_pillars(g, &rooms[i]);
    }
    carve_shafts(g);
    link_levels(g);
    carve_portal_mouths(g, 3, false);
    add_room_nodes(g, rooms, n);
    connect_nodes(g, rng_chance(&g->rng, 0.7f) ? 3 : 2, rng_chance(&g->rng, 0.6f) ? 4 : 3, 0.15f, false, false);
    skin_pass(g, true);
    decorate(g, false);
}

static void gen_crawl(GenCtx *g) {
    Room rooms[8];
    int n = place_rooms(g, rooms, rng_range(&g->rng, 2, 4), 3, 6, 2, 3);
    for (int i = 0; i < n; i++) carve_room_blob(g, &rooms[i]);
    carve_shafts(g);
    link_levels(g);
    carve_portal_mouths(g, 3, false);
    add_room_nodes(g, rooms, n);
    connect_nodes(g, 1, 2, 0.25f, false, true);
    skin_pass(g, false);
    g->light_rate *= 0.5f;
    decorate(g, true);
}

static void gen_cave(GenCtx *g) {
    Room rooms[8];
    int n = place_rooms(g, rooms, rng_range(&g->rng, 3, 5), 8, 15, 6, 12);
    for (int i = 0; i < n; i++) carve_room_blob(g, &rooms[i]);
    carve_shafts(g);
    link_levels(g);
    carve_portal_mouths(g, 3, false);
    add_room_nodes(g, rooms, n);
    connect_nodes(g, 3, 3, 0.2f, false, false);
    skin_pass(g, false);
    decorate(g, true);
}

static void gen_mine(GenCtx *g) {
    Room rooms[8];
    int n = place_rooms(g, rooms, rng_range(&g->rng, 1, 3), 5, 9, 4, 6);
    for (int i = 0; i < n; i++) carve_room_box(g, &rooms[i]);
    carve_shafts(g);
    link_levels(g);
    carve_portal_mouths(g, 3, false);
    add_room_nodes(g, rooms, n);
    connect_nodes(g, 3, 3, 0.3f, true, false);
    skin_pass(g, false);
    decorate(g, true);
}

static void gen_cells(GenCtx *g) {
    bool along_x = rng_next(&g->rng) & 1;
    int len = rng_range(&g->rng, 20, 26), y0 = g->info.levels[rng_range(&g->rng, 0, g->info.nlevels - 1)];
    int a0 = (CHUNK - len) / 2, c0 = rng_range(&g->rng, 10, 16);
    Room hall;
    if (along_x) hall = (Room){ a0, y0, c0, a0 + len - 1, y0 + 4, c0 + 4 };
    else hall = (Room){ c0, y0, a0, c0 + 4, y0 + 4, a0 + len - 1 };
    carve_room_box(g, &hall);
    /* cells on both sides of the hall, behind bars */
    for (int s = 0; s < 2; s++)
        for (int k = a0 + 1; k + 3 <= a0 + len - 1; k += 4) {
            int c_out = s == 0 ? c0 - 1 : c0 + 5;    /* bars line */
            int c_far = s == 0 ? c0 - 4 : c0 + 8;
            int cmin = MINI(c_out, c_far), cmax = MAXI(c_out, c_far);
            for (int a = k; a < k + 3; a++)
                for (int c = cmin; c <= cmax; c++)
                    for (int y = y0; y < y0 + 3; y++) {
                        int x = along_x ? a : c, z = along_x ? c : a;
                        g_set(g, x, y, z, c == c_out ? B_BARS : B_AIR);
                    }
            int fx = along_x ? k + 1 : (s == 0 ? c0 - 3 : c0 + 7);
            int fz = along_x ? (s == 0 ? c0 - 3 : c0 + 7) : k + 1;
            if (rng_chance(&g->rng, 0.6f)) g_set(g, fx, y0, fz, B_BONES);
            if (rng_chance(&g->rng, 0.4f)) g_set(g, fx, y0 + 2, fz, B_CHAIN);
        }
    carve_shafts(g);
    link_levels(g);
    carve_portal_mouths(g, 3, false);
    add_room_nodes(g, &hall, 1);
    Room extra[3];
    int n = place_rooms(g, extra, rng_range(&g->rng, 0, 2), 4, 7, 3, 5);
    for (int i = 0; i < n; i++) carve_room_box(g, &extra[i]);
    add_room_nodes(g, extra, n);
    connect_nodes(g, 3, 4, 0.1f, false, false);
    skin_pass(g, true);
    decorate(g, false);
}

static void gen_arena(GenCtx *g) {
    int w = rng_range(&g->rng, 18, 26), d = rng_range(&g->rng, 18, 26);
    int y0 = g->info.levels[0], h = MINI(rng_range(&g->rng, 9, 14), CHUNK - 3 - y0);
    Room r = { (CHUNK - w) / 2, y0, (CHUNK - d) / 2, 0, 0, 0 };
    r.x1 = r.x0 + w - 1; r.y1 = r.y0 + h - 1; r.z1 = r.z0 + d - 1;
    carve_room_box(g, &r);
    /* ring of pillars and a raised centre */
    float cx = (r.x0 + r.x1) * 0.5f, cz = (r.z0 + r.z1) * 0.5f, rad = MINI(w, d) * 0.5f - 3.0f;
    for (int i = 0; i < 8; i++) {
        float a = i * PI_F / 4.0f;
        int px = (int)(cx + cosf(a) * rad), pz = (int)(cz + sinf(a) * rad);
        g_fill(g, px, r.y0, pz, px + 1, r.y1, pz + 1, B_PILLAR);
    }
    g_fill(g, (int)cx - 2, r.y0, (int)cz - 2, (int)cx + 2, r.y0, (int)cz + 2, B_ACCENT);
    g_set(g, (int)cx, r.y0 + 1, (int)cz, B_BRAZIER);
    g_set(g, (int)cx, r.y0 + 2, (int)cz, B_FIRE);
    carve_shafts(g);
    link_levels(g);
    carve_portal_mouths(g, 3, false);
    add_room_nodes(g, &r, 1);
    connect_nodes(g, 3, 4, 0.1f, false, false);
    skin_pass(g, true);
    decorate(g, false);
}

/* Open air: floating islands and bridges. Heaven gets little temples, the void gets crystals. */
static void gen_void(GenCtx *g) {
    memset(g->b, B_AIR, CHUNK_VOL);
    Room isl[8];
    int n = place_rooms(g, isl, rng_range(&g->rng, 2, 5), 4, 9, 6, 10);
    for (int i = 0; i < n; i++) {
        Room *r = &isl[i];
        int depth = rng_range(&g->rng, 2, 5);
        for (int d = 0; d < depth; d++) {
            int inset = d;
            g_fill(g, r->x0 + inset, r->y0 - 1 - d, r->z0 + inset, r->x1 - inset, r->y0 - 1 - d, r->z1 - inset,
                   d == 0 ? B_FLOOR : B_ROCK);
        }
        if (g->info.stratum == ST_HEAVEN || g->info.stratum == ST_CLOUDS) {
            /* small open temple */
            int top = r->y0 + 5;
            int px[2] = { r->x0, r->x1 }, pz[2] = { r->z0, r->z1 };
            for (int a = 0; a < 2; a++)
                for (int b = 0; b < 2; b++) g_fill(g, px[a], r->y0, pz[b], px[a], top - 1, pz[b], B_PILLAR);
            g_fill(g, r->x0, top, r->z0, r->x1, top, r->z1, B_ROOF);
            g_set(g, (r->x0 + r->x1) / 2, top - 1, (r->z0 + r->z1) / 2, B_LANTERN);
        } else if (g->info.stratum == ST_ASTRAL) {
            g_fill(g, r->x0 + 1, r->y0, r->z0 + 1, r->x0 + 1, r->y0 + rng_range(&g->rng, 1, 4), r->z0 + 1, B_CRYSTAL);
        }
    }
    carve_shafts(g);
    link_levels(g);
    carve_portal_mouths(g, 4, true);
    add_room_nodes(g, isl, n);
    connect_nodes(g, 3, 4, 0.2f, false, false);
    skin_pass(g, true);
    decorate(g, false);
}

void gen_archetype(GenCtx *g) {
    switch (g->info.arch) {
    case ARCH_ROOMS: gen_rooms(g); break;
    case ARCH_CRAWL: gen_crawl(g); break;
    case ARCH_CAVE: gen_cave(g); break;
    case ARCH_MINE: gen_mine(g); break;
    case ARCH_CELLS: gen_cells(g); break;
    case ARCH_ARENA: gen_arena(g); break;
    case ARCH_VOID: gen_void(g); break;
    default: gen_rooms(g); break;
    }
}
