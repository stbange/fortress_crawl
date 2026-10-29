/* Headless checks for the world generator:
 *  1. determinism: the same chunk generates identically twice
 *  2. portal alignment: every opening is open on both sides of its face
 *  3. reachability: a walking (crawl-capable) player starting outside the gate
 *     can reach most portal openings in a region around the start
 *  4. meshing smoke test with lighting */
#include "world/layout.h"
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

    if (g_fail) printf("%d check(s) failed\n", g_fail);
    else printf("all checks passed\n");
    return g_fail ? 1 : 0;
}
