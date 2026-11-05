#include "world/gen_common.h"
#include "world/mech.h"

/* Vaults and traps (M8). Runs after an archetype chunk is carved: a vault is a small
 * room cut into solid rock beside a floor, its gate facing the floor, with the parts of
 * one puzzle in the reachable space in front of it. Traps go on the main floor. The
 * game builds the working devices from what is recorded in the chunk info. */

static bool walk(const GenCtx *g, int x, int y, int z) {
    if (x < 1 || x > CHUNK - 2 || z < 1 || z > CHUNK - 2 || y < 1 || y > CHUNK - 3) return false;
    Block below = g_get(g, x, y - 1, z), at = g_get(g, x, y, z);
    return block_solid(below) && below != B_LAVA && !block_solid(at) && at != B_WATER && !block_solid(g_get(g, x, y + 1, z));
}

static bool col_prot(const GenCtx *g, int x, int y, int z) {
    for (int k = -1; k <= 2; k++)
        if (g_in(x, y + k, z) && g_prot(g, x, y + k, z)) return true;
    return false;
}

/* A solid thing here would not cut the floor around it in two: the walkable cells of
 * the ring around it form one run. */
static bool noblock(const GenCtx *g, int x, int y, int z) {
    static const int RX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 }, RZ[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    if (!walk(g, x, y, z) || col_prot(g, x, y, z)) return false;
    int runs = 0, count = 0;
    for (int i = 0; i < 8; i++) {
        bool a = walk(g, x + RX[i], y, z + RZ[i]), b = walk(g, x + RX[(i + 7) & 7], y, z + RZ[(i + 7) & 7]);
        count += a;
        if (a && !b) runs++;
    }
    return runs <= 1 && count >= 3;
}

static bool open4(const GenCtx *g, int x, int y, int z) {
    if (!walk(g, x, y, z)) return false;
    for (int d = 0; d < 4; d++)
        if (!walk(g, x + DIR_X[d], y, z + DIR_Z[d])) return false;
    return true;
}

static bool solid_box(const GenCtx *g, int x0, int y0, int z0, int x1, int y1, int z1) {
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) {
                if (!g_in(x, y, z) || g_prot(g, x, y, z)) return false;
                Block b = g_get(g, x, y, z);
                if (!block_solid(b) || b == B_SHRINE || b == B_LAVA) return false;
            }
    return true;
}

static int cheb(int ax, int az, int bx, int bz) { return MAXI(abs(ax - bx), abs(az - bz)); }

/* ---------- the vault room ---------- */

static bool find_vault(GenCtx *g, Vault *v) {
    int y = g->info.nlevels ? g->info.levels[0] : LAYER_FLOOR;
    for (int t = 0; t < 400; t++) {
        int cx = rng_range(&g->rng, 3, CHUNK - 4), cz = rng_range(&g->rng, 3, CHUNK - 4);
        if (!solid_box(g, cx - 2, y - 1, cz - 2, cx + 2, y + 3, cz + 2)) continue;
        int d0 = rng_range(&g->rng, 0, 3);
        for (int k = 0; k < 4; k++) {
            int d = (d0 + k) & 3;
            int fx = cx + 3 * DIR_X[d], fz = cz + 3 * DIR_Z[d];
            if (!walk(g, fx, y, fz) || col_prot(g, fx, y, fz)) continue;
            /* room to stand back from the gate */
            int free = 0;
            for (int j = 0; j < 4; j++) free += walk(g, fx + DIR_X[j], y, fz + DIR_Z[j]);
            if (free < 2) continue;
            v->x = (u8)cx; v->y = (u8)y; v->z = (u8)cz; v->door = (u8)d;
            return true;
        }
    }
    return false;
}

static void carve_vault(GenCtx *g, const Vault *v) {
    int cx = v->x, cy = v->y, cz = v->z;
    g_fill(g, cx - 2, cy - 1, cz - 2, cx + 2, cy + 3, cz + 2, B_WALL);
    g_fill(g, cx - 1, cy - 1, cz - 1, cx + 1, cy - 1, cz + 1, B_FLOOR);
    g_fill(g, cx - 1, cy + 3, cz - 1, cx + 1, cy + 3, cz + 1, B_CEIL);
    g_fill(g, cx - 1, cy, cz - 1, cx + 1, cy + 2, cz + 1, B_AIR);
    int gx, gz;
    mech_gate_cell(v, &gx, &gz);
    int sx = DIR_Z[v->door], sz = DIR_X[v->door]; /* along the wall */
    for (int k = -1; k <= 1; k += 2) g_fill(g, gx + sx * k, cy, gz + sz * k, gx + sx * k, cy + 2, gz + sz * k, B_ACCENT);
    g_set(g, gx, cy + 2, gz, B_ACCENT);
    g_set(g, gx, cy, gz, B_AIR);
    g_set(g, gx, cy + 1, gz, B_AIR);
    g_set(g, gx, cy - 1, gz, B_ACCENT);
    g_set(g, cx, cy + 2, cz, B_LANTERN);
    for (int y = cy - 1; y <= cy + 3; y++)
        for (int z = cz - 2; z <= cz + 2; z++)
            for (int x = cx - 2; x <= cx + 2; x++) g_protect(g, x, y, z);
}

/* ---------- puzzle parts ---------- */

typedef struct Cand { u8 x, z, dir; } Cand;

/* Floor cells reachable from the gate at the vault's height, 3..18 steps away. */
static int reachable(GenCtx *g, const Vault *v, Cand *out, int max) {
    static const int CAP = CHUNK * CHUNK;
    short dist[CHUNK * CHUNK];
    int queue[CHUNK * CHUNK], qh = 0, qt = 0, n = 0;
    for (int i = 0; i < CAP; i++) dist[i] = -1;
    for (int z = v->z - 2; z <= v->z + 2; z++)
        for (int x = v->x - 2; x <= v->x + 2; x++) dist[z * CHUNK + x] = 999; /* the vault */
    int fx = v->x + 3 * DIR_X[v->door], fz = v->z + 3 * DIR_Z[v->door];
    dist[fz * CHUNK + fx] = 0;
    queue[qt++] = fz * CHUNK + fx;
    while (qh < qt) {
        int c = queue[qh++], x = c % CHUNK, z = c / CHUNK;
        if (dist[c] >= 3 && dist[c] <= 18 && n < max && !col_prot(g, x, v->y, z)) out[n++] = (Cand){ (u8)x, (u8)z, 0 };
        for (int d = 0; d < 4; d++) {
            int nx = x + DIR_X[d], nz = z + DIR_Z[d];
            if (nx < 0 || nz < 0 || nx >= CHUNK || nz >= CHUNK || dist[nz * CHUNK + nx] != -1) continue;
            if (!walk(g, nx, v->y, nz)) continue;
            dist[nz * CHUNK + nx] = (short)(dist[c] + 1);
            queue[qt++] = nz * CHUNK + nx;
        }
    }
    /* shuffle so the parts spread out */
    for (int i = n - 1; i > 0; i--) {
        int j = rng_range(&g->rng, 0, i);
        Cand t = out[i]; out[i] = out[j]; out[j] = t;
    }
    return n;
}

static bool far_from(const Vault *v, int x, int z, int d) {
    for (int i = 0; i < v->nparts; i++)
        if (cheb(x, z, v->parts[i].x, v->parts[i].z) < d) return false;
    return true;
}

static bool pick_levers(GenCtx *g, Vault *v, Cand *c, int nc) {
    int want = rng_chance(&g->rng, 0.4f) ? 4 : 3;
    v->nparts = 0;
    for (int i = 0; i < nc && v->nparts < want; i++) {
        int x = c[i].x, z = c[i].z, y = v->y;
        for (int d = 0; d < 4; d++) {
            int wx = x + DIR_X[d], wz = z + DIR_Z[d];
            if (!block_solid(g_get(g, wx, y, wz)) || !block_solid(g_get(g, wx, y + 1, wz)) || g_prot(g, wx, y + 1, wz)) continue;
            if (!far_from(v, x, z, 3)) break;
            v->parts[v->nparts++] = (MechPart){ (u8)x, (u8)y, (u8)z, (u8)d, 0 };
            break;
        }
    }
    if (v->nparts < want) return false;
    int n = v->nparts;
    u8 all = (u8)((1u << n) - 1), lamps = all;
    while (lamps == all) {
        int pulls = rng_range(&g->rng, 1, 2);
        for (int k = 0; k < pulls; k++) lamps = mech_lever_pull(lamps, rng_range(&g->rng, 0, n - 1), n);
    }
    v->lamps = lamps;
    return true;
}

static bool pick_plates(GenCtx *g, Vault *v, Cand *c, int nc) {
    int want = rng_chance(&g->rng, 0.5f) ? 4 : 3;
    v->nparts = 0;
    for (int i = 0; i < nc && v->nparts < want; i++)
        if (open4(g, c[i].x, v->y, c[i].z) && far_from(v, c[i].x, c[i].z, 3))
            v->parts[v->nparts++] = (MechPart){ c[i].x, v->y, c[i].z, 0, 0 };
    return v->nparts == want;
}

static bool pick_statues(GenCtx *g, Vault *v, Cand *c, int nc) {
    v->nparts = 0;
    for (int i = 0; i < nc && v->nparts < 3; i++)
        if (noblock(g, c[i].x, v->y, c[i].z) && far_from(v, c[i].x, c[i].z, 3)) {
            int k = v->nparts++;
            v->parts[k] = (MechPart){ c[i].x, v->y, c[i].z, 0, 0 };
            v->parts[k].start = (u8)((mech_statue_goal(v, k) + rng_range(&g->rng, 1, 3)) & 3);
        }
    return v->nparts == 3;
}

static int gen_solid(void *user, int x, int y, int z) { return block_solid(g_get((const GenCtx *)user, x, y, z)); }

/* Light runs along row y+1: the cells between two points must be free there. */
static bool line_clear(GenCtx *g, int y, int x0, int z0, int d, int k) {
    for (int s = 1; s < k; s++) {
        int x = x0 + DIR8_X[d] * s, z = z0 + DIR8_Z[d] * s;
        if (!g_in(x, y + 1, z) || block_solid(g_get(g, x, y + 1, z))) return false;
    }
    return true;
}

/* The point k steps from (x, z) in direction d holds a solid thing (a mirror or the
 * crystal): it must stand on the floor and leave room around it. */
static bool stand_ok(GenCtx *g, const Vault *v, int x, int z) {
    int gx, gz;
    mech_gate_cell(v, &gx, &gz);
    if (cheb(x, z, v->x, v->z) <= 3 || cheb(x, z, gx, gz) < 2) return false;
    return x >= 2 && z >= 2 && x <= CHUNK - 3 && z <= CHUNK - 3 && noblock(g, x, v->y, z) && far_from(v, x, z, 3);
}

static bool pick_beam(GenCtx *g, Vault *v) {
    int y = v->y, gx, gz;
    mech_gate_cell(v, &gx, &gz);
    int out = 2 * v->door; /* light must arrive at the gate travelling inward */
    int mirrors = rng_chance(&g->rng, 0.5f) ? 2 : 1;
    for (int t = 0; t < 60; t++) {
        v->nparts = 1; /* [0] is the emitter, placed last */
        v->parts[0] = (MechPart){ 0 };
        int px = gx, pz = gz, arrive = (out + 4) & 7; /* direction the light travels into (px,pz) */
        bool ok = true;
        u8 sol[VAULT_PARTS] = { 0 };
        for (int m = 0; m <= mirrors && ok; m++) {
            /* walk back from the point along -arrive to place the previous element */
            int back = (arrive + 4) & 7;
            int k = rng_range(&g->rng, 3, 8);
            int x = px + DIR8_X[back] * k, z = pz + DIR8_Z[back] * k;
            if (!stand_ok(g, v, x, z) || !line_clear(g, y, x, z, arrive, k)) { ok = false; break; }
            if (m < mirrors) {
                int i = v->nparts++;
                v->parts[i] = (MechPart){ (u8)x, (u8)y, (u8)z, 0, 0 };
                sol[i] = (u8)arrive;
                /* the light comes to this mirror from a new direction */
                int turn = rng_range(&g->rng, 1, 3) * (rng_chance(&g->rng, 0.5f) ? 1 : -1);
                arrive = (arrive + turn + 8) & 7;
                px = x; pz = z;
            } else {
                v->parts[0] = (MechPart){ (u8)x, (u8)y, (u8)z, (u8)arrive, 0 };
            }
        }
        if (!ok) continue;
        /* the solution must work and the start must not */
        int pts[MECH_BEAM_PTS * 2], np;
        if (!mech_beam(v, sol, 0, 0, 0, gen_solid, g, pts, &np)) continue;
        u8 start[VAULT_PARTS] = { 0 };
        for (int i = 1; i < v->nparts; i++) {
            start[i] = (u8)((sol[i] + rng_range(&g->rng, 1, 7)) & 7);
            v->parts[i].start = start[i];
        }
        if (mech_beam(v, start, 0, 0, 0, gen_solid, g, pts, &np)) continue;
        return true;
    }
    return false;
}

static void gen_vault(GenCtx *g) {
    Vault v;
    memset(&v, 0, sizeof v);
    if (!find_vault(g, &v)) return;
    carve_vault(g, &v);
    Cand c[CHUNK * CHUNK];
    int nc = reachable(g, &v, c, CHUNK * CHUNK);
    float r = rng_float(&g->rng);
    int first = r < 0.3f ? PZ_LEVERS : r < 0.55f ? PZ_PLATES : r < 0.78f ? PZ_STATUES : PZ_BEAM;
    for (int k = 0; k < PZ_COUNT - 1; k++) {
        int pz = 1 + (first - 1 + k) % (PZ_COUNT - 1);
        bool ok = pz == PZ_LEVERS ? pick_levers(g, &v, c, nc)
                : pz == PZ_PLATES ? pick_plates(g, &v, c, nc)
                : pz == PZ_STATUES ? pick_statues(g, &v, c, nc)
                                   : pick_beam(g, &v);
        if (ok) { v.puzzle = (u8)pz; break; }
    }
    if (!v.puzzle) {
        /* nothing fits in front of it: an old vault whose lock has rusted away (open) */
        v.nparts = 0;
    }
    g->info.vault = v;
}

/* ---------- traps ---------- */

static bool trap_spot_free(GenCtx *g, int x, int z, int size) {
    const Vault *v = &g->info.vault;
    if (v->y) {
        if (cheb(x, z, v->x, v->z) < 5 + size) return false;
        for (int i = 0; i < v->nparts; i++)
            if (cheb(x, z, v->parts[i].x, v->parts[i].z) < 2 + size) return false;
    }
    for (int i = 0; i < g->info.ntraps; i++)
        if (cheb(x, z, g->info.traps[i].x, g->info.traps[i].z) < 4 + size) return false;
    return true;
}

static void gen_traps(GenCtx *g) {
    const ChunkInfo *ci = &g->info;
    if (ci->level < 2) return;
    if (hash3f(ci->seed, 1, 2, 3, 0x7A9) > 0.3f + fminf(0.3f, ci->level * 0.01f)) return;
    int want = rng_range(&g->rng, 1, ci->level < 10 ? 2 : 3);
    int y = ci->nlevels ? ci->levels[0] : LAYER_FLOOR;
    for (int t = 0; t < 300 && g->info.ntraps < want; t++) {
        int x = rng_range(&g->rng, 2, CHUNK - 3), z = rng_range(&g->rng, 2, CHUNK - 3);
        if (!walk(g, x, y, z) || col_prot(g, x, y, z)) continue;
        float r = rng_float(&g->rng);
        Trap tr = { 0 };
        tr.x = (u8)x; tr.y = (u8)y; tr.z = (u8)z;
        if (r < 0.4f) {
            /* a plate and a dart launcher in the nearest wall beside it */
            int best = -1, best_len = 99;
            for (int d = 0; d < 4; d++)
                for (int k = 1; k <= 6; k++) {
                    int wx = x + DIR_X[d] * k, wz = z + DIR_Z[d] * k;
                    if (!block_solid(g_get(g, wx, y + 1, wz))) continue;
                    if (block_solid(g_get(g, wx, y, wz)) && k - 1 < best_len) { best = d; best_len = k - 1; }
                    break;
                }
            if (best < 0) continue;
            tr.kind = TRAP_DARTS; tr.dir = (u8)best; tr.len = (u8)best_len;
            if (!trap_spot_free(g, x, z, 0)) continue;
        } else if (r < 0.75f) {
            int size = rng_chance(&g->rng, 0.5f) ? 3 : 2;
            bool ok = true;
            for (int dz = 0; dz < size && ok; dz++)
                for (int dx = 0; dx < size && ok; dx++) ok = walk(g, x + dx, y, z + dz) && !col_prot(g, x + dx, y, z + dz);
            if (!ok || !trap_spot_free(g, x, z, size)) continue;
            tr.kind = TRAP_SPIKES; tr.len = (u8)size;
        } else {
            if (!trap_spot_free(g, x, z, 0)) continue;
            tr.kind = TRAP_FLAME;
        }
        g->info.traps[g->info.ntraps++] = tr;
    }
}

void gen_mechanisms(GenCtx *g) {
    const ChunkInfo *ci = &g->info;
    if (ci->has_struct || ci->arch == ARCH_EXTERIOR || ci->arch == ARCH_VOID) return;
    Rng keep = g->rng;
    g->rng = rng_make(ci->seed ^ 0x3EC4ULL);
    if (hash3f(ci->seed, 3, 1, 4, 0x7A01) < 0.14f) gen_vault(g);
    gen_traps(g);
    g->rng = keep;
}
