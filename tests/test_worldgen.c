/* Headless checks for the world generator:
 *  1. determinism: the same chunk generates identically twice
 *  2. portal alignment: every opening is open on both sides of its face
 *  3. reachability: a walking (crawl-capable) player starting outside the gate
 *     can reach most portal openings in a region around the start
 *  4. meshing smoke test with lighting
 *  5b. layers: every chunk has its up and down staircase (structures one each), the
 *     walkable floor of a layer is flat, and the walk from the gate reaches every layer
 *  6. stories: deterministic, and every chapter target and domain lord near the gate
 *     has a floor with room for its size in its home chunk
 *  7. mechanisms: vault gates open onto a floor reached from the gate, every puzzle can
 *     be solved and does not start solved, traps stand on the floor */
#include "world/layout.h"
#include "world/realms.h"
#include "world/towns.h"
#include "world/dragons.h"
#include "world/king.h"
#include "world/story.h"
#include "world/history.h"
#include "world/carve.h"
#include "game/physics.h"
#include "world/support.h"
#include "world/eco.h"
#include "world/mech.h"
#include "world/mesher.h"
#include "world/texgen.h"

#include <time.h>

#define MAX_TEST_PORTALS 24

#define R_X0 (-3)
#define R_X1 3
#define R_Y0 (-2)
#define R_Y1 2
#define R_Z0 (-3)
#define R_Z1 3
#define RX (R_X1 - R_X0 + 1)
#define RY (R_Y1 - R_Y0 + 1)
#define RZ (R_Z1 - R_Z0 + 1)

static Block *g_chunks[RX * RY * RZ];
static ChunkInfo g_infos[RX * RY * RZ];
static World g_world;
static int g_fail;

#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: " __VA_ARGS__); printf("\n"); g_fail++; } } while (0)

static int ci_index(int cx, int cy, int cz) {
    if (cx < R_X0 || cx > R_X1 || cy < R_Y0 || cy > R_Y1 || cz < R_Z0 || cz > R_Z1) return -1;
    return (cx - R_X0) + (cy - R_Y0) * RX + (cz - R_Z0) * RX * RY;
}

static Block get_block(int wx, int wy, int wz) {
    int i = ci_index(floordiv(wx, CHUNK), floordiv(wy, CHUNK), floordiv(wz, CHUNK));
    if (i < 0) return B_ROCK;
    return g_chunks[i][cidx(floormod(wx, CHUNK), floormod(wy, CHUNK), floormod(wz, CHUNK))];
}

static bool solid(int x, int y, int z) { return block_solid(get_block(x, y, z)); }
static bool standable(int x, int y, int z) { return !solid(x, y, z) && solid(x, y - 1, z); }
static int test_solid(void *u, int x, int y, int z) { (void)u; return solid(x, y, z); }

/* visited bitmap over the region */
#define WX (RX * CHUNK)
#define WY (RY * CHUNK)
#define WZ (RZ * CHUNK)
static u8 *g_seen;
static int *g_queue;

static int widx(int x, int y, int z) {
    return ((y - R_Y0 * CHUNK) * WZ + (z - R_Z0 * CHUNK)) * WX + (x - R_X0 * CHUNK);
}
static bool in_region(int x, int y, int z) {
    return x >= R_X0 * CHUNK && x < (R_X1 + 1) * CHUNK && y >= R_Y0 * CHUNK + 1 && y < (R_Y1 + 1) * CHUNK - 1 &&
           z >= R_Z0 * CHUNK && z < (R_Z1 + 1) * CHUNK;
}

static int g_bx0 = -(1 << 30), g_bx1 = 1 << 30, g_by0 = -(1 << 30), g_by1 = 1 << 30, g_bz0 = -(1 << 30), g_bz1 = 1 << 30;
static bool in_bounds(int x, int y, int z) {
    return x >= g_bx0 && x <= g_bx1 && y >= g_by0 && y <= g_by1 && z >= g_bz0 && z <= g_bz1;
}

static long flood(int sx, int sy, int sz) {
    long head = 0, tail = 0, n = 0;
    memset(g_seen, 0, (size_t)WX * WY * WZ);
    g_queue[tail++] = widx(sx, sy, sz);
    g_seen[widx(sx, sy, sz)] = 1;
    while (head < tail) {
        int c = g_queue[head++];
        n++;
        int x = c % WX + R_X0 * CHUNK, z = (c / WX) % WZ + R_Z0 * CHUNK, y = c / (WX * WZ) + R_Y0 * CHUNK;
        static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], nz = z + DZ[d];
            int targets[5], nt = 0;
            if (standable(nx, y, nz)) targets[nt++] = y;
            if (!solid(x, y + 1, z) && standable(nx, y + 1, nz)) targets[nt++] = y + 1;
            for (int k = 1; k <= 3 && !solid(nx, y - k + 1, nz); k++)
                if (standable(nx, y - k, nz)) { targets[nt++] = y - k; break; }
            for (int t = 0; t < nt; t++) {
                int ty = targets[t];
                if (!in_region(nx, ty, nz) || !in_bounds(nx, ty, nz)) continue;
                int i = widx(nx, ty, nz);
                if (g_seen[i]) continue;
                g_seen[i] = 1;
                g_queue[tail++] = i;
            }
        }
    }
    return n;
}

/* A tiny world for the physics of broken blocks: a floor at y = 0, one broken block at (5, 1, 5). */
static u64 g_test_mask;
static int broken_solid(void *u, int x, int y, int z) {
    (void)u;
    if (y <= 0) return 1;
    if (x == 5 && y == 1 && z == 5) return 2;
    return 0;
}
static u64 test_pieces(void *u, int x, int y, int z) { (void)u; (void)x; (void)y; (void)z; return g_test_mask; }

/* a world for the collapse tests: [y][z][x], 40 cells a side */
static int test_grid_block(void *user, int x, int y, int z) {
    const u8 (*grid)[40][40] = user;
    if (y < 0) return B_ROCK; /* the ground goes on down */
    if (x < 0 || z < 0 || x >= 40 || y >= 40 || z >= 40) return B_AIR;
    return grid[y][z][x];
}


/* ---------- realms: a cache of generated chunks ---------- */
typedef struct { int cx, cy, cz; Block *b; } RChunk;
static RChunk *g_rc;
static int g_nrc, g_caprc;
static Block realm_block(int x, int y, int z) {
    int cx = floordiv(x, CHUNK), cy = floordiv(y, CHUNK), cz = floordiv(z, CHUNK);
    for (int i = g_nrc - 1; i >= 0; i--)
        if (g_rc[i].cx == cx && g_rc[i].cy == cy && g_rc[i].cz == cz) return g_rc[i].b[cidx(x - cx * CHUNK, y - cy * CHUNK, z - cz * CHUNK)];
    if (g_nrc == g_caprc) { g_caprc = g_caprc ? g_caprc * 2 : 256; g_rc = realloc(g_rc, sizeof(RChunk) * (size_t)g_caprc); }
    RChunk *c = &g_rc[g_nrc++];
    c->cx = cx; c->cy = cy; c->cz = cz;
    c->b = malloc(CHUNK_VOL);
    world_generate_chunk(&g_world, cx, cy, cz, c->b, NULL);
    return c->b[cidx(x - cx * CHUNK, y - cy * CHUNK, z - cz * CHUNK)];
}
static void realm_cache_clear(void) {
    for (int i = 0; i < g_nrc; i++) free(g_rc[i].b);
    g_nrc = 0;
}
static bool r_open(int x, int y, int z) { Block b = realm_block(x, y, z); return !block_solid(b) && b != B_LAVA; }
static bool r_stand(int x, int y, int z) {
    if (!r_open(x, y, z) || !r_open(x, y + 1, z)) return false;
    Block below = realm_block(x, y - 1, z);
    return block_solid(below) || realm_block(x, y, z) == B_WATER;
}

int main(int argc, char **argv) {
    u64 seed = argc > 1 ? strtoull(argv[1], NULL, 10) : 1337;
    world_init(&g_world, seed);
    printf("seed %llu\n", (unsigned long long)seed);

    clock_t t0 = clock();
    for (int cz = R_Z0; cz <= R_Z1; cz++)
        for (int cy = R_Y0; cy <= R_Y1; cy++)
            for (int cx = R_X0; cx <= R_X1; cx++) {
                int i = ci_index(cx, cy, cz);
                g_chunks[i] = malloc(CHUNK_VOL);
                world_generate_chunk(&g_world, cx, cy, cz, g_chunks[i], &g_infos[i]);
            }
    double gen_ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC / (RX * RY * RZ);
    printf("generated %d chunks, %.2f ms per chunk\n", RX * RY * RZ, gen_ms);

    /* 1. determinism */
    {
        Block *b = malloc(CHUNK_VOL);
        int mism = 0;
        for (int k = 0; k < 12; k++) {
            int cx = R_X0 + k % RX, cy = R_Y0 + k % RY, cz = R_Z0 + (k * 3) % RZ;
            world_generate_chunk(&g_world, cx, cy, cz, b, NULL);
            if (memcmp(b, g_chunks[ci_index(cx, cy, cz)], CHUNK_VOL) != 0) mism++;
        }
        CHECK(mism == 0, "%d chunks generated differently on the second run", mism);
        free(b);
    }

    /* 2. portal alignment + collect portal opening cells for reachability */
    int portal_cells[4096][3];
    int nportal_cells = 0, nportals = 0, bad_portals = 0;
    for (int cz = R_Z0; cz < R_Z1; cz++)
        for (int cy = R_Y0; cy <= R_Y1; cy++)
            for (int cx = R_X0; cx < R_X1; cx++)
                for (int axis = 0; axis < 3; axis += 2) {
                    Portal ps[8];
                    int n = world_face_portals(&g_world, axis, cx, cy, cz, ps, 8);
                    for (int i = 0; i < n; i++) {
                        const Portal *p = &ps[i];
                        nportals++;
                        int open = 0, total = 0;
                        for (int k = 0; k < p->w; k++)
                            for (int y = p->y; y < p->y + p->h; y++)
                                for (int side = -1; side <= 0; side++) {
                                    int x = axis == 0 ? p->x + side : p->x + k;
                                    int z = axis == 0 ? p->z + k : p->z + side;
                                    total++;
                                    if (!solid(x, y, z)) open++;
                                }
                        if (open < total) {
                            bad_portals++;
                            if (bad_portals <= 5)
                                printf("  blocked %s portal at %d %d %d axis %d (%d/%d open)\n", PORTAL_NAMES[p->type], p->x,
                                       p->y, p->z, axis, open, total);
                        }
                        if (nportal_cells < 4096) {
                            int x = axis == 0 ? p->x : p->x + p->w / 2, z = axis == 0 ? p->z + p->w / 2 : p->z;
                            portal_cells[nportal_cells][0] = x;
                            portal_cells[nportal_cells][1] = p->y;
                            portal_cells[nportal_cells][2] = z;
                            nportal_cells++;
                        }
                    }
                }
    printf("portals: %d horizontal, %d blocked\n", nportals, bad_portals);
    CHECK(bad_portals * 20 <= nportals, "too many blocked portals (%d of %d)", bad_portals, nportals);

    /* 3. reachability from the start position in front of the gate */
    g_seen = malloc((size_t)WX * WY * WZ);
    g_queue = malloc(sizeof(int) * (size_t)WX * WY * WZ);
    int sx = -60, sz = 0, sy = 0;
    for (int y = 60; y > 0; y--)
        if (standable(sx, y, sz)) { sy = y; break; }
    long reached = flood(sx, sy, sz);
    int ok = 0;
    for (int i = 0; i < nportal_cells; i++) {
        int x = portal_cells[i][0], y = portal_cells[i][1], z = portal_cells[i][2];
        if (in_region(x, y, z) && g_seen[widx(x, y, z)]) ok++;
        else if (getenv("FC_VERBOSE")) {
            ChunkInfo a, b;
            world_chunk_info(&g_world, floordiv(x - 1, CHUNK), floordiv(y, CHUNK), floordiv(z - 1, CHUNK), &a);
            world_chunk_info(&g_world, floordiv(x, CHUNK), floordiv(y, CHUNK), floordiv(z, CHUNK), &b);
            printf("  unreachable opening at %d %d %d (chunks %s / %s)\n", x, y, z,
                   a.arch < ARCH_COUNT ? ARCH_NAMES[a.arch] : "struct", b.arch < ARCH_COUNT ? ARCH_NAMES[b.arch] : "struct");
        }
    }
    printf("reachability: start at y=%d, %ld walkable cells reached, %d/%d portal openings reachable (%.0f%%)\n", sy,
           reached, ok, nportal_cells, 100.0 * ok / (nportal_cells ? nportal_cells : 1));
    CHECK(ok * 100 >= nportal_cells * 95, "less than 95%% of portal openings reachable");

    /* 5b. layers */
    {
        /* reached cells per layer (by the layer's floor band) */
        long per_layer[R_Y1 - R_Y0 + 1] = { 0 };
        for (int y = R_Y0 * CHUNK; y < (R_Y1 + 1) * CHUNK; y++)
            for (int z = R_Z0 * CHUNK; z < (R_Z1 + 1) * CHUNK; z++)
                for (int x = R_X0 * CHUNK; x < (R_X1 + 1) * CHUNK; x++)
                    if (in_region(x, y, z) && g_seen[widx(x, y, z)] && floormod(y, CHUNK) == LAYER_FLOOR)
                        per_layer[floordiv(y, CHUNK) - R_Y0]++;
        printf("layers: floor cells reached from the gate per layer:");
        for (int l = 0; l <= R_Y1 - R_Y0; l++) {
            printf(" %d:%ld", R_Y0 + l, per_layer[l]);
            CHECK(per_layer[l] > 500, "a layer is not reachable from the gate");
        }
        printf("\n");
        /* staircases: every inner chunk has one up and one down unless a structure or the
         * outside rules it out */
        int chunks = 0, both = 0, exempt = 0;
        for (int cy = R_Y0 + 1; cy < R_Y1; cy++)
            for (int cz = R_Z0; cz <= R_Z1; cz++)
                for (int cx = R_X0; cx <= R_X1; cx++) {
                    ChunkInfo ci;
                    world_chunk_info(&g_world, cx, cy, cz, &ci);
                    if (ci.arch == ARCH_EXTERIOR) continue;
                    Portal up[4], dn[4];
                    int nu = world_face_portals(&g_world, 1, cx, cy, cz, up, 4);
                    int nd = world_face_portals(&g_world, 1, cx, cy - 1, cz, dn, 4);
                    bool near_outside = world_is_exterior(cx, cy + 1, cz) || world_is_exterior(cx, cy - 1, cz);
                    ChunkInfo above, below;
                    world_chunk_info(&g_world, cx, cy + 1, cz, &above);
                    world_chunk_info(&g_world, cx, cy - 1, cz, &below);
                    if (ci.has_struct || above.has_struct || below.has_struct || near_outside) { exempt++; continue; }
                    chunks++;
                    both += nu == 1 && nd == 1;
                }
        printf("layers: %d/%d plain chunks have both an up and a down staircase (%d next to structures or outside)\n", both,
               chunks, exempt);
        CHECK(both == chunks, "a plain chunk misses a staircase");
        /* flatness: where can you stand in plain chunks (outside the stair wells)? */
        long on_floor = 0, near_floor = 0, total = 0;
        for (int cy = R_Y0; cy <= R_Y1; cy++)
            for (int cz = R_Z0; cz <= R_Z1; cz++)
                for (int cx = R_X0; cx <= R_X1; cx++) {
                    ChunkInfo ci;
                    world_chunk_info(&g_world, cx, cy, cz, &ci);
                    if (ci.arch == ARCH_EXTERIOR || ci.has_struct) continue;
                    Portal ps[MAX_TEST_PORTALS];
                    int np = world_chunk_portals(&g_world, cx, cy, cz, ps, MAX_TEST_PORTALS);
                    for (int ly = 1; ly < CHUNK - 1; ly++)
                        for (int lz = 1; lz < CHUNK - 1; lz++)
                            for (int lx = 1; lx < CHUNK - 1; lx++) {
                                int x = cx * CHUNK + lx, y = cy * CHUNK + ly, z = cz * CHUNK + lz;
                                if (!standable(x, y, z)) continue;
                                bool well = false;
                                for (int i = 0; i < np && !well; i++)
                                    if (ps[i].axis == 1 && abs(x - ps[i].x) <= 2 && abs(z - ps[i].z) <= 2) well = true;
                                if (well) continue;
                                total++;
                                on_floor += ly == LAYER_FLOOR;
                                near_floor += abs(ly - LAYER_FLOOR) <= 2;
                            }
                }
        printf("layers: %.1f%% of the standing cells outside stair wells are on the layer floor, %.1f%% within two steps\n",
               100.0 * on_floor / (total ? total : 1), 100.0 * near_floor / (total ? total : 1));
        CHECK(near_floor * 100 >= total * 90, "plain chunks are not flat enough");
    }

    /* per-chunk: all openings of a chunk connect to each other inside it (+1 cell margin) */
    {
        int bad = 0, tested = 0;
        for (int cz = R_Z0 + 1; cz < R_Z1; cz++)
            for (int cy = R_Y0 + 1; cy < R_Y1; cy++)
                for (int cx = R_X0 + 1; cx < R_X1; cx++) {
                    ChunkInfo *ci = &g_infos[ci_index(cx, cy, cz)];
                    if (ci->arch == ARCH_EXTERIOR || ci->has_struct) continue;
                    Portal ps[MAX_TEST_PORTALS];
                    int n = world_chunk_portals(&g_world, cx, cy, cz, ps, MAX_TEST_PORTALS);
                    int pts[MAX_TEST_PORTALS][3], np = 0;
                    for (int i = 0; i < n; i++) {
                        const Portal *p = &ps[i];
                        if (p->axis == 1) {
                            bool lower = p->y == (cy + 1) * CHUNK;
                            int d = lower ? p->low_dir : p->exit_dir;
                            pts[np][0] = p->x + DIR_X[d] * 3;
                            pts[np][1] = lower ? p->y_low : p->y_up;
                            pts[np][2] = p->z + DIR_Z[d] * 3;
                        } else {
                            bool low_side = (p->axis == 0 ? p->x : p->z) == (p->axis == 0 ? cx : cz) * CHUNK;
                            int a = low_side ? 0 : -1;
                            pts[np][0] = p->axis == 0 ? p->x + a : p->x + p->w / 2;
                            pts[np][1] = p->y;
                            pts[np][2] = p->axis == 0 ? p->z + p->w / 2 : p->z + a;
                        }
                        np++;
                    }
                    if (np < 2) continue;
                    tested++;
                    g_bx0 = cx * CHUNK - 1; g_bx1 = cx * CHUNK + CHUNK; g_by0 = cy * CHUNK - 1; g_by1 = cy * CHUNK + CHUNK;
                    g_bz0 = cz * CHUNK - 1; g_bz1 = cz * CHUNK + CHUNK;
                    flood(pts[0][0], pts[0][1], pts[0][2]);
                    int miss = 0;
                    for (int i = 1; i < np; i++)
                        if (!g_seen[widx(pts[i][0], pts[i][1], pts[i][2])]) miss++;
                    if (miss) {
                        bad++;
                        const char *dump = getenv("FC_DUMP");
                        int dx, dy, dz;
                        if (dump && sscanf(dump, "%d %d %d", &dx, &dy, &dz) == 3 && dx == cx && dy == cy && dz == cz) {
                            for (int i = 0; i < np; i++)
                                printf("opening %d at %d %d %d reached=%d\n", i, pts[i][0], pts[i][1], pts[i][2],
                                       g_seen[widx(pts[i][0], pts[i][1], pts[i][2])]);
                            for (int y = cy * CHUNK; y < cy * CHUNK + CHUNK; y++) {
                                printf("y=%d\n", y);
                                for (int z = cz * CHUNK; z < cz * CHUNK + CHUNK; z++) {
                                    for (int x = cx * CHUNK; x < cx * CHUNK + CHUNK; x++) {
                                        Block b = get_block(x, y, z);
                                        char ch = block_solid(b) ? '#' : (standable(x, y, z) ? '.' : ' ');
                                        if (standable(x, y, z) && g_seen[widx(x, y, z)]) ch = 'o';
                                        for (int i = 0; i < np; i++)
                                            if (pts[i][0] == x && pts[i][1] == y && pts[i][2] == z) ch = (char)('A' + i);
                                        putchar(ch);
                                    }
                                    putchar('\n');
                                }
                            }
                        }
                        if (getenv("FC_VERBOSE"))
                            printf("  chunk %d %d %d (%s): %d of %d openings not connected to the first\n", cx, cy, cz,
                                   ci->arch < ARCH_COUNT ? ARCH_NAMES[ci->arch] : STRUCT_NAMES[ci->st.type], miss, np);
                    }
                }
        g_bx0 = g_by0 = g_bz0 = -(1 << 30);
        g_bx1 = g_by1 = g_bz1 = 1 << 30;
        printf("chunk-internal connectivity: %d of %d chunks fully connected\n", tested - bad, tested);
        CHECK(bad * 10 <= tested, "more than 10%% of chunks have openings that do not connect inside the chunk");
    }

    /* chunk mix */
    int arch_count[ARCH_COUNT] = { 0 }, structs = 0;
    for (int i = 0; i < RX * RY * RZ; i++) {
        if (g_infos[i].arch < ARCH_COUNT) arch_count[g_infos[i].arch]++;
        else if (g_infos[i].has_struct) structs++;
    }
    printf("chunk archetypes:");
    for (int a = 0; a < ARCH_COUNT; a++) printf(" %s=%d", ARCH_NAMES[a], arch_count[a]);
    printf(" structure=%d\n", structs);

    /* 4. meshing smoke test on the vestibule chunk */
    {
        MeshInput in;
        memset(&in, 0, sizeof in);
        for (int dz = -1; dz <= 1; dz++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int i = ci_index(0 + dx, 0 + dy, 0 + dz);
                    in.blocks[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 9] = i >= 0 ? g_chunks[i] : NULL;
                    in.info[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 9] = i >= 0 ? &g_infos[i] : NULL;
                }
        MeshScratch *s = mesh_scratch_new();
        MeshOutput out;
        t0 = clock();
        mesh_build(&in, s, &out);
        printf("mesh of chunk 0,0,0: %d quads + %d sprites in %.1f ms\n", out.nsolid, out.nsprite,
               (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC);
        CHECK(out.nsolid > 100, "vestibule mesh suspiciously small");
        free(out.verts);
        free(out.light);
        mesh_scratch_free(s);
    }

    /* 5. themes along each axis, for a sense of the directional layout */
    printf("places along the axes:\n");
    static const int probes[][3] = { { 0, 0, 0 }, { 0, -6, 0 }, { 0, -12, 0 }, { 0, -20, 0 }, { 0, -30, 0 }, { 0, -45, 0 },
                                     { 0, 5, 0 }, { 0, 10, 0 }, { 0, 16, 0 }, { 0, 24, 0 }, { 0, 34, 0 },
                                     { 0, 0, -6 }, { 0, 0, -14 }, { 0, 0, -24 }, { 0, 0, 6 }, { 0, 0, 14 }, { 0, 0, 24 },
                                     { 6, 0, 0 }, { 14, 0, 0 }, { 24, 0, 0 }, { -6, 0, 0 }, { -14, 0, 0 }, { -24, 0, 0 },
                                     { 0, -30, -24 } };
    for (int i = 0; i < ARRAY_LEN(probes); i++) {
        ChunkInfo ci;
        char name[128];
        world_chunk_info(&g_world, probes[i][0], probes[i][1], probes[i][2], &ci);
        world_place_name(&ci, name, sizeof name);
        printf("  chunk %4d %4d %4d  level %3d  %s\n", probes[i][0], probes[i][1], probes[i][2], ci.level, name);
    }

    /* 6. stories */
    {
        Block *b = malloc(CHUNK_VOL);
        ChunkInfo ci;
        int room_ok = 0, room_total = 0;
        for (int c = 0; c < 9; c++) {
            Named t, t2;
            story_chapter_target(&g_world, c, &t);
            story_chapter_target(&g_world, c, &t2);
            CHECK(t.key == t2.key && t.cx == t2.cx && t.cy == t2.cy && t.cz == t2.cz && !strcmp(t.name, t2.name),
                  "story chapter targets are not deterministic");
            world_generate_chunk(&g_world, t.cx, t.cy, t.cz, b, &ci);
            CHECK(ci.arch != ARCH_EXTERIOR, "a chapter target lives outside");
            /* a floor cell with headroom for the scaled monster (brute 2 m tall at most) */
            int need = (int)ceilf(2.0f * t.scale) + 1, found = 0;
            for (int y = 1; y + need < CHUNK && !found; y++)
                for (int z = 1; z < CHUNK - 1 && !found; z++)
                    for (int x = 1; x < CHUNK - 1 && !found; x++) {
                        if (!block_solid(b[cidx(x, y - 1, z)])) continue;
                        bool free_ = true;
                        for (int k = 0; k < need && free_; k++) free_ = !block_solid(b[cidx(x, y + k, z)]);
                        found = free_;
                    }
            room_total++;
            room_ok += found;
            if (!found) printf("  chapter %d target %s at %d %d %d has no room for scale %.1f\n", c + 1, t.name, t.cx, t.cy, t.cz, t.scale);
        }
        printf("story: %d/%d chapter targets have room for their full size\n", room_ok, room_total);
        CHECK(room_ok >= room_total - 2, "too many chapter targets without room (the game shrinks them to fit)");
        int lords = 0, lords_ok = 0;
        for (int cz = -6; cz <= 6; cz += 2)
            for (int cx = -6; cx <= 6; cx += 2) {
                Domain d;
                Named l;
                story_domain(&g_world, cx, 0, cz, &d);
                story_lord(&g_world, &d, &l);
                Named here;
                bool is_here = story_lord_here(&g_world, l.cx, l.cy, l.cz, &here);
                if (d.level < 2) continue;
                lords++;
                lords_ok += is_here && here.key == l.key;
            }
        printf("story: %d/%d domain lords live in a chunk of their own domain\n", lords_ok, lords);
        CHECK(lords_ok == lords, "a domain lord's home chunk belongs to another domain");
        char t1[STORY_TEXT], t2[STORY_TEXT];
        Trace tr;
        int traces = 0;
        for (int cx = -8; cx <= 8; cx++)
            if (story_trace(&g_world, cx, 0, 3, &tr)) {
                traces++;
                story_trace_text(&g_world, cx, 0, 3, t1, sizeof t1);
                story_trace_text(&g_world, cx, 0, 3, t2, sizeof t2);
                CHECK(!strcmp(t1, t2) && t1[0], "trace texts are not deterministic");
            }
        printf("story: %d traces on a 17-chunk row\n", traces);
        free(b);
        /* generated histories (docs/STORY_GEN.md): sound, varied, fast, and the same every time */
        static History hs, hs2;
        int bad = 0, shapes[SH_COUNT] = { 0 }, nshapes = 0;
        clock_t t0 = clock();
        for (int i = 0; i < 200; i++) {
            history_generate((u64)i * 104729 + 3, &hs);
            if (!history_validate(&hs)) {
                if (bad < 5) printf("  history of seed %llu: %s\n", (unsigned long long)hs.seed, hs.problems);
                bad++;
            }
            if (!shapes[hs.shape]++) nshapes++;
        }
        double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC / 200.0;
        printf("story: 200 histories, %d unsound, %d plot shapes, %.2f ms each (with validation)\n", bad, nshapes, ms);
        CHECK(bad == 0, "%d generated histories failed validation", bad);
        CHECK(nshapes >= 7, "only %d plot shapes in 200 worlds", nshapes);
        CHECK(ms < 10.0, "a history takes %.1f ms", ms);
        history_generate(g_world.seed, &hs);
        history_generate(g_world.seed, &hs2);
        CHECK(!memcmp(&hs, &hs2, sizeof hs), "a world's history is not deterministic");
        /* the story functions of a generated world speak of its history */
        Personal pp;
        story_personal(g_world.seed, 0, &pp);
        int legends[UNIQUE_SLOTS] = { 0 };
        for (int c = 0; c < H_CAST; c++) {
            Named t;
            story_chapter_target(&g_world, c, &t);
            CHECK(!strcmp(t.name, hs.actor[hs.cast[c]].name), "chapter %d target is not the history's", c + 1);
            int id = story_legend_id(&g_world, t.key);
            CHECK(id == hs.cast_legend[c], "chapter %d target carries the wrong legendary", c + 1);
            CHECK(!legends[id]++, "two chapter targets carry the same legendary");
            for (int m = 0; m < MOTIVE_COUNT; m++) {
                story_chapter_hook(&g_world, m, c, &pp, t1, sizeof t1);
                story_chapter_done(&g_world, m, c, &pp, t2, sizeof t2);
                CHECK(t1[0] && t2[0] && strlen(t1) < STORY_TEXT - 1 && strlen(t2) < STORY_TEXT - 1, "chapter %d motive %d text empty or cut", c + 1, m);
            }
            story_revelation(&g_world, c, &pp, t1, sizeof t1);
            CHECK(t1[0] != 0, "chapter %d has no revelation", c + 1);
            char lore[STORY_TEXT];
            CHECK(story_legend_lore(&g_world, hs.cast_legend[c], lore, sizeof lore) && lore[0], "chapter %d's legendary has no lore", c + 1);
        }
        for (int k = 0; k < 40; k++) { /* a lord never carries a chapter target's legendary */
            Domain d;
            Named l;
            story_domain(&g_world, k * 4 - 80, 0, (k % 7) * 4, &d);
            story_lord(&g_world, &d, &l);
            int id = story_legend_id(&g_world, l.key);
            for (int c = 0; c < H_CAST; c++) CHECK(id != hs.cast_legend[c], "a domain lord carries chapter %d's legendary", c + 1);
        }
        int clues = 0;
        for (int cz = -20; cz <= 20; cz++)
            for (int cx = -20; cx <= 20; cx++) clues += story_trace_clue(&g_world, cx, 0, cz) >= 0;
        printf("story: %d traces within 20 chunks carry a clue of the history\n", clues);
        CHECK(clues >= 50, "too few traces carry clues (%d)", clues);
        /* choices (docs/STORY_CHOICES.md): every ending, every text, and the world answering */
        World cw = g_world;
        Named before[H_CAST];
        for (int c = 0; c < H_CAST; c++) story_chapter_target(&cw, c, &before[c]);
        int texts = 0;
        for (int policy = 0; policy < CH_COUNT; policy++)
            for (int fin = 0; fin < CH_COUNT; fin++) {
                cw.ndeeds = 0;
                for (int c = 0; c < H_CAST; c++)
                    story_deed(&cw, DK_CHAPTER, c, c == 8 ? fin : (policy + c) % CH_COUNT, 0, before[c].key, 0, 0, 0);
                CHECK(story_chapter_of_key(&cw, before[4].key) == 4, "a chapter target is not found by its key");
                char names[H_SEQUEL][STORY_NAME];
                for (int c = 0; c < H_CAST + H_SEQUEL; c++) {
                    Named t;
                    story_chapter_target(&cw, c, &t);
                    if (c < H_CAST) CHECK(t.key == before[c].key && !strcmp(t.name, before[c].name), "a deed changed chapter %d's target", c + 1);
                    else {
                        snprintf(names[c - H_CAST], STORY_NAME, "%s", t.name);
                        CHECK(t.name[0] && t.title[0], "chapter %d of the sequel has no target", c + 1);
                        for (int k = 0; k < c - H_CAST; k++) CHECK(strcmp(names[k], t.name), "two sequel targets share a name (%s)", t.name);
                        CHECK(story_chapter_of_key(&cw, t.key) == c, "sequel chapter %d not found by its key", c + 1);
                    }
                    for (int m = 0; m < MOTIVE_COUNT; m++) {
                        story_chapter_hook(&cw, m, c, &pp, t1, sizeof t1);
                        CHECK(t1[0] && strlen(t1) < STORY_TEXT - 1, "chapter %d hook empty or cut", c + 1);
                    }
                    story_revelation(&cw, c, &pp, t2, sizeof t2);
                    CHECK(t2[0] && strlen(t2) < STORY_TEXT - 1, "chapter %d revelation empty or cut", c + 1);
                    story_orders(&cw, c, t2, sizeof t2);
                    texts += 2;
                }
                char lines[12][STORY_TEXT];
                int n = story_deed_lines(&cw, &pp, lines, 12);
                CHECK(n == H_CAST, "the deeds list has %d lines, not %d", n, H_CAST);
                for (int i = 0; i < n; i++) CHECK(lines[i][0] && strlen(lines[i]) < STORY_TEXT - 1, "a deed line is empty or cut");
            }
        HPersonal hp2 = { pp.village, pp.kin };
        for (int c = 0; c < H_CAST; c++) {
            char plea[STORY_TEXT], desc[CH_COUNT][STORY_TEXT_H];
            history_parley(&hs, c, &hp2, plea, sizeof plea, desc);
            CHECK(plea[0] && strlen(plea) < STORY_TEXT - 1, "chapter %d plea empty or cut", c + 1);
            for (int k = 0; k < CH_COUNT; k++) CHECK(strstr(desc[k], " - ") && strlen(desc[k]) < STORY_TEXT - 1, "chapter %d choice %d text", c + 1, k);
        }
        /* a killed lord changes its domain: emptied, or taken by the rival under a new lord */
        int changed = 0, emptied = 0, lords_tested = 0;
        for (int k = 0; k < 24; k++) {
            cw.ndeeds = 0;
            Domain d, d2;
            story_domain(&cw, k * 4 - 48, 0, (k % 5) * 4 - 8, &d);
            if (d.level < 2) continue;
            Named l, l2;
            story_lord(&cw, &d, &l);
            story_deed(&cw, DK_LORD, 0, CH_KILL, 0, l.key, d.gx, d.gy, d.gz);
            fill_check:
            story_domain(&cw, k * 4 - 48, 0, (k % 5) * 4 - 8, &d2);
            lords_tested++;
            CHECK(d2.changed, "killing a lord did not change its domain");
            changed += d2.changed != 0;
            if (d2.changed == 2) {
                story_lord(&cw, &d2, &l2);
                CHECK(l2.key != l.key && strcmp(d2.faction, d.faction), "a taken domain kept its lord or faction");
                if (!cw.ndeeds || cw.deeds[cw.ndeeds - 1].key != l2.key) { story_deed(&cw, DK_LORD, 0, CH_KILL, 0, l2.key, d.gx, d.gy, d.gz); goto fill_check; }
            } else emptied++;
            Named here;
            CHECK(!story_lord_here(&cw, l.cx, l.cy, l.cz, &here) || here.key != l.key, "a killed lord came back");
        }
        printf("story: choices give %d texts that fit; %d killed lords changed their domains (%d emptied)\n", texts, changed, emptied);
        CHECK(lords_tested > 4, "too few lords tested");
    }

    /* 6b. the physics of broken blocks (docs/DESTRUCTION.md) */
    {
        phys_pieces = test_pieces;
        /* only the lower half of the block is left: a step of 50 cm */
        g_test_mask = 0;
        for (int sy = 0; sy < 2; sy++)
            for (int sz = 0; sz < 4; sz++)
                for (int sx = 0; sx < 4; sx++) g_test_mask |= 1ULL << carve_bit(sx, sy, sz);
        CHECK(!phys_box_free(v3(5.5f, 1.2f, 5.5f), 0.3f, 1.8f, broken_solid, NULL), "a box inside the pieces is free");
        CHECK(phys_box_free(v3(5.5f, 1.5f, 5.5f), 0.3f, 1.8f, broken_solid, NULL), "a box standing on the half block is blocked");
        vec3 p = v3(5.5f, 3.0f, 5.5f);
        phys_move_axis(&p, 1, -2.5f, 0.3f, 1.8f, broken_solid, NULL);
        CHECK(fabsf(p.y - 1.5f) < 0.01f, "falling onto the half block lands at %.3f, not 1.5", p.y);
        vec3 q = v3(3.5f, 1.0f, 5.5f);
        phys_move_axis(&q, 0, 3.0f, 0.3f, 0.4f, broken_solid, NULL);
        CHECK(fabsf(q.x - (5.0f - 0.3f)) < 0.01f, "walking into the half block stops at %.3f", q.x);
        vec3 r = v3(3.5f, 1.8f, 5.5f);
        phys_move_axis(&r, 0, 3.0f, 0.3f, 0.1f, broken_solid, NULL);
        CHECK(r.x > 6.0f, "above the half block the way is free (%.2f)", r.x);
        float d = phys_raycast(v3(3.5f, 1.25f, 5.5f), v3(1, 0, 0), 5.0f, broken_solid, NULL);
        CHECK(fabsf(d - 1.5f) < 0.06f, "a ray hits the pieces at %.2f, not 1.5", d);
        d = phys_raycast(v3(3.5f, 1.75f, 5.5f), v3(1, 0, 0), 5.0f, broken_solid, NULL);
        CHECK(d >= 5.0f, "a ray above the pieces hits at %.2f", d);
        g_test_mask = 0;
        CHECK(phys_box_free(v3(5.5f, 1.0f, 5.5f), 0.3f, 1.8f, broken_solid, NULL), "a block with no pieces left blocks");
        phys_pieces = NULL;
        printf("physics: broken blocks collide and cast rays by their 25 cm pieces\n");
    }

    /* 6d. weight and collapse (support.c): beams and a pillared slab */
    {
        static u8 grid[40][40][40]; /* [y][z][x] */
        #define GSET(x, y, z, b) (grid[y][z][x] = (u8)(b))
        int ok = 0;
        for (int pass = 0; pass < 2; pass++) {
            /* a beam between two pillars on a floor; the pillar at x = 24 is cut */
            Block beam = pass ? B_WOOD : B_WALL;
            memset(grid, 0, sizeof grid);
            for (int z = 0; z < 40; z++) for (int x = 0; x < 40; x++) for (int y = 0; y <= 2; y++) GSET(x, y, z, B_ROCK);
            for (int y = 3; y <= 6; y++) { GSET(16, y, 20, B_WALL); GSET(24, y, 20, B_WALL); }
            for (int x = 16; x <= 24; x++) GSET(x, 7, 20, beam);
            SupportCell removed[4];
            u8 mat[4];
            for (int y = 3; y <= 6; y++) { removed[y - 3] = (SupportCell){ 24, y, 20 }; mat[y - 3] = B_WALL; GSET(24, y, 20, B_AIR); }
            SupportCell out[64];
            int n = support_falls(test_grid_block, NULL, grid, 20, 7, 20, 8, removed, mat, 4, out, 64);
            int expect = pass ? 4 : 3; /* wood reaches 4 from its pillar, masonry 5: the rest of the 9 falls */
            CHECK(n == expect, "a %s beam cut at one end: %d of it falls (expected %d)", BLOCKS[beam].name, n, expect);
            for (int i = 0; i < n; i++) CHECK(out[i].y == 7 && out[i].x > 16 + SUBSTANCES[beam].span, "only the far end of the beam falls");
            ok += n == expect;
        }
        /* a 5 x 5 slab of masonry on four corner pillars */
        int falls[3];
        for (int cut = 0; cut < 3; cut++) {
            memset(grid, 0, sizeof grid);
            for (int z = 0; z < 40; z++) for (int x = 0; x < 40; x++) for (int y = 0; y <= 2; y++) GSET(x, y, z, B_ROCK);
            int px[4] = { 16, 20, 16, 20 }, pz[4] = { 16, 16, 20, 20 };
            for (int k = 0; k < 4; k++) for (int y = 3; y <= 6; y++) GSET(px[k], y, pz[k], B_PILLAR);
            for (int z = 16; z <= 20; z++) for (int x = 16; x <= 20; x++) GSET(x, 7, z, B_WALL);
            int ncut = cut == 0 ? 0 : cut == 1 ? 2 : 4; /* none, the two at x = 20, all four */
            SupportCell removed[16];
            u8 mat[16];
            int nr = 0;
            for (int k = 0; k < 4; k++) {
                if (!(ncut == 4 || (ncut == 2 && px[k] == 20))) continue;
                for (int y = 3; y <= 6; y++) { removed[nr] = (SupportCell){ px[k], y, pz[k] }; mat[nr++] = B_PILLAR; GSET(px[k], y, pz[k], B_AIR); }
            }
            SupportCell out[64];
            falls[cut] = support_falls(test_grid_block, NULL, grid, 18, 7, 18, 8, removed, mat, nr, out, 64);
        }
        CHECK(falls[0] == 0, "a slab on its pillars stands (%d fall)", falls[0]);
        CHECK(falls[1] > 0 && falls[1] < 25, "half the pillars cut: part of the slab falls (%d of 25)", falls[1]);
        CHECK(falls[2] == 25, "every pillar cut: the whole slab falls (%d of 25)", falls[2]);
        printf("collapse: beams fall beyond their span (masonry 5, wood 4); a slab with half its pillars cut loses %d of 25 cells, with all of them all 25\n",
               falls[1]);
        #undef GSET
    }

    /* 6e. populations (eco.c): the numbers alone */
    {
        Eco a = { 0 }, b = { 0 }, c = { 0 };
        eco_start(&a, 100, 10, false);
        a.predators = 0;
        eco_step(&a, 200.0f);
        float ka = eco_capacity(&a);
        CHECK(a.grazers > 0.8f * ka && a.grazers < 1.2f * ka, "without predators grazers settle at what the plants feed (%.2f of %.2f)", a.grazers, ka);
        eco_start(&b, 300, 30, true);
        b.predators = 1.0f;
        float gmin = 1e9f, gmax = 0, pmin = 1e9f, pmax = 0;
        for (int t = 0; t < 1000; t++) {
            eco_step(&b, 0.5f);
            if (t > 400) {
                gmin = fminf(gmin, b.grazers); gmax = fmaxf(gmax, b.grazers);
                pmin = fminf(pmin, b.predators); pmax = fmaxf(pmax, b.predators);
            }
        }
        CHECK(gmin > 0.2f && pmin > 0.05f, "with predators neither dies out (grazers %.2f, predators %.2f at least)", gmin, pmin);
        CHECK(gmax < 3.0f * eco_capacity(&b) && pmax < 20.0f, "and neither grows without bound (%.2f, %.2f at most)", gmax, pmax);
        eco_start(&c, 100, 0, false);
        c.grazers = 12.0f;
        c.predators = 0;
        float eaten = eco_step(&c, 10.0f);
        CHECK(eaten > 5.0f, "too many grazers eat the plants down (%.1f cells)", eaten);
        CHECK(c.grazers < 12.0f, "and fall back toward what is left (%.2f)", c.grazers);
        printf("populations: alone the grazers settle at %.1f (plants feed %.1f); with predators they live on (grazers %.1f-%.1f, predators %.1f-%.1f); overgrazing eats %.0f cells\n",
               a.grazers, ka, gmin, gmax, pmin, pmax, eaten);
    }

    /* 6c. the table of substances (docs/SIMULATION.md): sane values for every block */
    {
        int burns = 0;
        for (int b = 1; b < B_COUNT; b++) {
            const Substance *m = &SUBSTANCES[b];
            CHECK(m->density > 0, "%s has a density", BLOCKS[b].name);
            if (m->ignite) { burns++; CHECK(m->fuel > 0, "%s burns and has fuel", BLOCKS[b].name); }
            if (m->melt) CHECK(m->melt_to > B_AIR && m->melt_to < B_COUNT && m->melt_to != b, "%s melts into something else", BLOCKS[b].name);
            if (BLOCKS[b].render == RENDER_LIQUID) CHECK(!block_solid(b), "the liquid %s is not solid", BLOCKS[b].name);
            if (m->lightning) CHECK(m->density > 10, "%s carries lightning and sinks", BLOCKS[b].name);
        }
        CHECK(SUBSTANCES[B_OIL].density < SUBSTANCES[B_WATER].density, "oil floats on water");
        CHECK(SUBSTANCES[B_ICE].melt_to == B_WATER, "ice melts into water");
        printf("substances: %d blocks, %d of them burn; oil floats, ice melts, metal carries lightning\n", B_COUNT - 1, burns);
    }

    /* 7. mechanisms */
    {
        g_bx0 = g_by0 = g_bz0 = -(1 << 30);
        g_bx1 = g_by1 = g_bz1 = 1 << 30;
        flood(sx, sy, sz);
        int vaults = 0, by_kind[PZ_COUNT] = { 0 }, front_reached = 0, bad = 0, traps[TRAP_COUNT] = { 0 }, chunks = 0;
        for (int cy = R_Y0; cy <= R_Y1; cy++)
            for (int cz = R_Z0; cz <= R_Z1; cz++)
                for (int cx = R_X0; cx <= R_X1; cx++) {
                    const ChunkInfo *ci = &g_infos[ci_index(cx, cy, cz)];
                    int ox = cx * CHUNK, oy = cy * CHUNK, oz = cz * CHUNK;
                    chunks++;
                    for (int i = 0; i < ci->ntraps; i++) {
                        const Trap *t = &ci->traps[i];
                        traps[t->kind]++;
                        if (!standable(ox + t->x, oy + t->y, oz + t->z)) { bad++; printf("  trap not on a floor at chunk %d %d %d\n", cx, cy, cz); }
                    }
                    const Vault *v = &ci->vault;
                    if (!v->y) continue;
                    vaults++;
                    by_kind[v->puzzle]++;
                    int gx, gz;
                    mech_gate_cell(v, &gx, &gz);
                    int fx = ox + v->x + 3 * DIR_X[v->door], fy = oy + v->y, fz = oz + v->z + 3 * DIR_Z[v->door];
                    bool gate_open = !solid(ox + gx, fy, oz + gz) && !solid(ox + gx, fy + 1, oz + gz);
                    bool inside = !solid(ox + v->x, fy, oz + v->z) && solid(ox + v->x, fy - 1, oz + v->z);
                    if (!gate_open || !inside || !standable(fx, fy, fz)) { bad++; printf("  vault without a gate at chunk %d %d %d\n", cx, cy, cz); }
                    if (in_region(fx, fy, fz) && g_seen[widx(fx, fy, fz)]) front_reached++;
                    bool ok = true;
                    for (int i = 0; i < v->nparts; i++) {
                        const MechPart *p = &v->parts[i];
                        int px = ox + p->x, pz = oz + p->z;
                        if (!standable(px, fy, pz)) ok = false;
                        if (v->puzzle == PZ_LEVERS && !solid(px + DIR_X[p->dir], fy + 1, pz + DIR_Z[p->dir])) ok = false;
                    }
                    if (v->puzzle == PZ_LEVERS) {
                        /* the lamps can all be lit from the start */
                        int n = v->nparts;
                        u8 seen[16] = { 0 }, q[16], qh = 0, qt = 0, all = (u8)((1 << n) - 1);
                        q[qt++] = v->lamps; seen[v->lamps] = 1;
                        while (qh < qt) {
                            u8 st = q[qh++];
                            for (int i = 0; i < n; i++) {
                                u8 nx = mech_lever_pull(st, i, n);
                                if (!seen[nx]) { seen[nx] = 1; q[qt++] = nx; }
                            }
                        }
                        if (!seen[all] || v->lamps == all) ok = false;
                    } else if (v->puzzle == PZ_STATUES) {
                        int wrong = 0;
                        for (int i = 0; i < v->nparts; i++) wrong += v->parts[i].start != mech_statue_goal(v, i);
                        if (!wrong) ok = false;
                    } else if (v->puzzle == PZ_BEAM) {
                        int pts[MECH_BEAM_PTS * 2], np, nm = v->nparts - 1, solved = 0;
                        u8 f[VAULT_PARTS] = { 0 };
                        for (int i = 1; i < v->nparts; i++) f[i] = v->parts[i].start;
                        if (mech_beam(v, f, ox, oy, oz, test_solid, NULL, pts, &np)) ok = false;
                        for (int combo = 0; combo < (1 << (3 * nm)); combo++) {
                            for (int i = 0; i < nm; i++) f[i + 1] = (u8)((combo >> (3 * i)) & 7);
                            solved += mech_beam(v, f, ox, oy, oz, test_solid, NULL, pts, &np);
                        }
                        if (!solved) ok = false;
                    }
                    if (!ok) { bad++; printf("  puzzle %d cannot be solved or starts solved at chunk %d %d %d\n", v->puzzle, cx, cy, cz); }
                }
        printf("mechanisms: %d vaults in %d chunks (levers %d, plates %d, statues %d, beam %d, open %d), %d gates reached from the "
               "gate; traps: darts %d, spikes %d, flames %d\n",
               vaults, chunks, by_kind[PZ_LEVERS], by_kind[PZ_PLATES], by_kind[PZ_STATUES], by_kind[PZ_BEAM], by_kind[PZ_NONE],
               front_reached, traps[TRAP_DARTS], traps[TRAP_SPIKES], traps[TRAP_FLAME]);
        CHECK(bad == 0, "%d broken vaults or traps", bad);
        CHECK(vaults >= 8, "too few vaults");
        CHECK(front_reached * 10 >= vaults * 7, "too many vault gates cannot be reached from the gate");
    }


    /* towns of the friendly peoples (towns.h): the first one east of the gate, more farther out;
     * each one's plaza can be walked to from its passages in, and from the gate */
    {
        int ntowns = 0, per[PEOPLE_COUNT] = { 0 };
        u64 seen[64];
        bool first = false;
        Town ft;
        for (int cy = -6; cy <= 6; cy++)
            for (int cz = -18; cz <= 18; cz++)
                for (int cx = -18; cx <= 18; cx++) {
                    Town t;
                    if (!world_town_at(&g_world, cx, cy, cz, &t)) continue;
                    bool dup = false;
                    for (int k = 0; k < ntowns && k < 64; k++) dup |= seen[k] == t.key;
                    if (dup) continue;
                    if (ntowns < 64) seen[ntowns] = t.key;
                    ntowns++;
                    per[t.people]++;
                    if (t.cy == 0 && t.cx0 >= 6 && t.cx0 <= 11 && !first) { first = true; ft = t; }
                }
        if (getenv("TOWN_LIST"))
            for (int k = 0; k < ntowns && k < 64; k++)
                for (int cy = -6; cy <= 6; cy++)
                    for (int cz = -18; cz <= 18; cz++)
                        for (int cx = -18; cx <= 18; cx++) {
                            Town t;
                            if (world_town_at(&g_world, cx, cy, cz, &t) && t.key == seen[k] && cx == t.cx0 && cz == t.cz0) printf("  %s (%s): plaza %d %d %d\n", t.name, PEOPLES[t.people].plural, t.px, t.floor, t.pz);
                        }
        printf("towns: %d within 18 chunks and 6 layers:", ntowns);
        for (int i = 0; i < PEOPLE_COUNT; i++) printf(" %s %d", PEOPLES[i].plural, per[i]);
        printf("\n");
        CHECK(first, "no town east of the gate");
        if (first) {
            printf("towns: the first is %s, plaza at %d %d %d\n", ft.name, ft.px, ft.floor, ft.pz);
            int pb = ft.floor - 1;
            CHECK(r_stand(ft.px + 4, ft.floor, ft.pz) || r_stand(ft.px + 5, ft.floor, ft.pz + 5), "nowhere to stand in the first town's plaza");
            CHECK(block_solid(realm_block(ft.px + 3, pb, ft.pz + 3)), "the first town's plaza has no floor");
            /* every building's door is open, and its keeper has room */
            int doors = 0;
            for (int i = 0; i < TOWN_LOTS; i++) {
                const TownLot *L = &ft.lot[i];
                if (L->role < TB_INN || L->role > TB_HOUSE) continue;
                bool ok = r_open(L->door_x, ft.floor, L->door_z) && r_open(L->door_x, ft.floor + 1, L->door_z) && r_stand(L->spot_x, ft.floor, L->spot_z);
                doors += ok;
            }
            printf("towns: %d of 6 buildings have an open door and room for their keeper\n", doors);
            CHECK(doors == 6, "a building of the first town is closed or full");
        }
        realm_cache_clear();
    }

    /* the Old King (king.h): every seed has a way of ten steps that can be made, with a throne */
    {
        int seeds = getenv("KING_SEEDS") ? atoi(getenv("KING_SEEDS")) : 60, ok = 0, kinds[KING_KINDS] = { 0 }, types[KS_COUNT] = { 0 };
        World keep = g_world;
        for (int k = 0; k < seeds; k++) {
            World w;
            world_init(&w, k == 0 ? keep.seed : mix64((u64)k * 0x9E37));
            const KingWay *K = king_way(&w, realm_pick(w.seed));
            if (!K->valid) { printf("  seed %llu: no way to the king\n", (unsigned long long)w.seed); continue; }
            ok++;
            kinds[K->kind]++;
            for (int i = 0; i < KING_STEPS; i++) types[K->step[i].type]++;
            /* the throne hall is where the way says */
            Structure st;
            bool hall = world_structure_at(&w, K->step[9].cx, K->step[9].cy, K->step[9].cz, &st) && st.variant == HALL_THRONE;
            CHECK(hall, "the throne is not in its hall");
            bool dragon = false, town = false, sanctum = false;
            for (int i = 0; i < KING_STEPS; i++) {
                int t = K->step[i].type;
                dragon |= t == KS_DRAGON || t == KS_HOARD || t == KS_WYRM;
                town |= t == KS_HERALD || t == KS_ALLEGIANCE || t == KS_BREW;
                sanctum |= t == KS_VISION || t == KS_FORGE;
            }
            CHECK(dragon && town && sanctum, "a way without a dragon, a town or the Sanctum");
            if (k == 0) {
                printf("king: %s, %s; throne at chunk %d %d %d\n", K->name, KING_KINDS_DEF[K->kind].name, K->step[9].cx, K->step[9].cy, K->step[9].cz);
                for (int i = 0; i < KING_STEPS; i++)
                    printf("  step %2d (group %d): %-15s at %4d %4d %4d\n", i + 1, K->step[i].group, KING_STEP_NAMES[K->step[i].type], K->step[i].cx, K->step[i].cy, K->step[i].cz);
            }
        }
        printf("king: %d of %d seeds have a way; kinds:", ok, seeds);
        for (int i = 0; i < KING_KINDS; i++) printf(" %d", kinds[i]);
        printf("; steps:");
        for (int i = 0; i < KS_COUNT; i++) printf(" %s %d,", KING_STEP_NAMES[i], types[i]);
        printf("\n");
        CHECK(ok == seeds, "a seed without a way to the Old King");
    }

    /* dragons' lairs (dragons.h): some within reach, each a cave with room to fly over its hoard */
    {
        int nlairs = 0, ancient = 0, roomy = 0;
        u64 seen[64];
        for (int cy = -8; cy <= 8; cy++)
            for (int cz = -24; cz <= 24; cz++)
                for (int cx = -24; cx <= 24; cx++) {
                    Structure st;
                    Lair L;
                    if (!world_structure_at(&g_world, cx, cy, cz, &st) || !lair_from_structure(&st, &L)) continue;
                    bool dup = false;
                    for (int k = 0; k < nlairs && k < 64; k++) dup |= seen[k] == L.key;
                    if (dup) continue;
                    if (nlairs < 64) seen[nlairs] = L.key;
                    nlairs++;
                    ancient += L.size == DS_ANCIENT;
                    /* air above the hoard for a dragon to rise: 12 cells clear */
                    int clear = 0;
                    for (int y = L.floor + 3; y < L.floor + 15; y++) clear += r_open(L.cx + 12, y, L.cz);
                    roomy += clear >= 12;
                    if (getenv("LAIR_LIST")) printf("  the Lair of %s (%s %s) at %d %d %d\n", L.name, DRAGON_KINDS[L.kind].name, DRAGON_SIZES[L.size].name, L.cx, L.floor, L.cz);
                    realm_cache_clear();
                }
        printf("lairs: %d within 24 chunks and 8 layers (%d of ancient wyrms), %d with room to fly\n", nlairs, ancient, roomy);
        CHECK(nlairs >= 3, "too few dragons' lairs");
        CHECK(roomy == nlairs, "a lair without room to fly");
    }

    /* realms behind the Gate: every one has ground to arrive on, a keep, its plants, and a
     * way on foot from the gate to the keep */
    {
        int reached = 0;
        u32 picked = realm_pick(g_world.seed);
        int npick = 0;
        for (int r = 0; r < REALM_COUNT; r++) npick += (picked >> r) & 1;
        CHECK(npick >= 6 && npick <= 8, "a world must be joined to 6 to 8 realms");
        for (int r = 0; r < REALM_COUNT; r++) {
            int ax, ay, az, kx, ky, kz;
            realm_arrival(r, &ax, &ay, &az);
            realm_keep(r, &kx, &ky, &kz);
            ChunkInfo ci;
            world_chunk_info(&g_world, ax / CHUNK, ay / CHUNK, az / CHUNK, &ci);
            CHECK(ci.arch == ARCH_REALM && realm_of_flavor(ci.flavor) == r && ci.level == REALMS[r].level, "realm chunk info");
            bool arrive_ok = r_stand(ax, ay, az + 3);
            CHECK(arrive_ok, "nowhere to stand where you arrive in a realm");
            /* walk: from the arrival, a breadth-first search over standable cells */
            int span = REALM_W, y0 = ay - 48, ylen = 112;
            u8 *seen = calloc((size_t)span * span * ylen, 1);
            int *q = malloc(sizeof(int) * 4 * 400000);
            int qh = 0, qt = 0;
            #define RIDX(x, y, z) (((size_t)((y) - y0) * span + (z)) * span + (x))
            q[qt * 3] = ax; q[qt * 3 + 1] = ay; q[qt * 3 + 2] = az + 3; qt++;
            seen[RIDX(ax, ay, az + 3)] = 1;
            bool found = false;
            int flora = 0, visited = 0;
            while (qh < qt && !found && qt < 390000) {
                int x = q[qh * 3], y = q[qh * 3 + 1], z = q[qh * 3 + 2];
                qh++;
                visited++;
                if (abs(x - kx) <= 6 && abs(z - kz) <= 6 && abs(y - ky) <= 3) found = true;
                Block here = realm_block(x, y, z);
                if (here >= B_RFLORA_LOW && here <= B_RFLORA_GLOW) flora++;
                static const int D4[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                for (int d = 0; d < 4; d++)
                    for (int dy = 1; dy >= -4; dy--) {
                        int nx = x + D4[d][0], nz = z + D4[d][1], ny = y + dy;
                        if (nx < 0 || nz < 0 || nx >= span || nz >= span || ny < y0 || ny >= y0 + ylen) continue;
                        if (dy > 0 && !r_open(x, y + 2, z)) continue;
                        if (!r_stand(nx, ny, nz)) continue;
                        bool clear = true; /* a step down: the way down the neighbour's column is open */
                        for (int k = ny + 2; k <= y + 1 && clear; k++) clear = r_open(nx, k, nz);
                        if (!clear) break;
                        if (seen[RIDX(nx, ny, nz)]) break;
                        seen[RIDX(nx, ny, nz)] = 1;
                        q[qt * 3] = nx; q[qt * 3 + 1] = ny; q[qt * 3 + 2] = nz; qt++;
                        break;
                    }
                /* swimming up and down */
                if (here == B_WATER)
                    for (int dy = -1; dy <= 1; dy += 2)
                        if (y + dy >= y0 && y + dy < y0 + ylen && r_stand(x, y + dy, z) && !seen[RIDX(x, y + dy, z)]) {
                            seen[RIDX(x, y + dy, z)] = 1;
                            q[qt * 3] = x; q[qt * 3 + 1] = y + dy; q[qt * 3 + 2] = z; qt++;
                        }
            }
            #undef RIDX
            printf("realm %2d %-38s level %2d  arrive %s  keep %s on foot (%d cells walked, %d plants met)\n", r, REALMS[r].name, REALMS[r].level,
                   arrive_ok ? "ok" : "BAD", found ? "reached" : "NOT reached", visited, flora);
            reached += found;
            free(seen);
            free(q);
            realm_cache_clear();
        }
        printf("realms: the keep reached on foot in %d of %d\n", reached, REALM_COUNT);
        CHECK(reached >= REALM_COUNT - 2, "too many realms where the keep cannot be reached from the gate");
    }

    if (g_fail) printf("%d check(s) failed\n", g_fail);
    else printf("all checks passed\n");
    return g_fail ? 1 : 0;
}
