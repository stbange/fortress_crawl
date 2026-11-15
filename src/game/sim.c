/* The world simulation, part 2: water that flows (docs/SIMULATION.md).
 *
 * Water keeps a level of 1..8 eighths per cell (Chunk.fluid; generated water is full). Only
 * cells that something changed are simulated: every lasting change to the world
 * (chunkmap_edit) wakes the cells around it, and a cell that moves water wakes its neighbours
 * again. Water is conserved: it falls into the cell below, then evens out sideways until
 * neighbouring levels differ by one at most, and a thin film runs off edges. Where it meets
 * lava, the lava cools to rock and the water boils off; it puts out fires and torches. */
#include "game/game.h"
#include "world/pocket.h"

#define SIM_HZ 8.0f
#define SIM_BUDGET 600   /* cells per tick */
#define SIM_CAP 16384    /* cells waiting */

typedef struct { int x, y, z; } Cell;

static Cell q_now[SIM_CAP], q_next[SIM_CAP];
static int n_now, n_next;
static u32 seen[SIM_CAP * 2]; /* a hash of the cells already in q_next (0: empty) */
static float acc;
static u32 tick_no;
static Game *g_sim;

static u32 cell_hash(int x, int y, int z) {
    u32 h = (u32)x * 73856093u ^ (u32)y * 19349663u ^ (u32)z * 83492791u;
    return h ? h : 1;
}

static void push(int x, int y, int z) {
    if (n_next >= SIM_CAP) return;
    u32 h = cell_hash(x, y, z);
    u32 i = h & (SIM_CAP * 2 - 1);
    for (int k = 0; k < 8; k++, i = (i + 1) & (SIM_CAP * 2 - 1)) {
        if (seen[i] == h) return; /* (a rare collision only delays a cell by a tick) */
        if (!seen[i]) { seen[i] = h; q_next[n_next++] = (Cell){ x, y, z }; return; }
    }
    q_next[n_next++] = (Cell){ x, y, z };
}

void sim_reset(void) {
    n_now = n_next = 0;
    memset(seen, 0, sizeof seen);
    acc = 0;
}

static void sim_on_edit(int x, int y, int z) {
    push(x, y, z);
    push(x + 1, y, z); push(x - 1, y, z);
    push(x, y + 1, z); push(x, y - 1, z);
    push(x, y, z + 1); push(x, y, z - 1);
}

void sim_init(Game *g) {
    g_sim = g;
    sim_reset();
    g->map.on_edit = sim_on_edit;
}

void sim_wake(int x, int y, int z) { sim_on_edit(x, y, z); }

int sim_fluid(Game *g, int x, int y, int z) { return chunkmap_fluid(&g->map, x, y, z); }

/* How deep the water reaches above p (feet), in metres; 0 if dry. */
float sim_water_depth(Game *g, vec3 p) {
    int x = (int)floorf(p.x), z = (int)floorf(p.z), y0 = (int)floorf(p.y);
    float top = -1e9f;
    for (int y = y0; y <= y0 + 3; y++) {
        int l = chunkmap_fluid(&g->map, x, y, z);
        if (!l) break;
        top = (float)y + l / 8.0f;
    }
    return top > p.y ? top - p.y : 0.0f;
}

/* What water can flow into: 1 empty (or a flame it puts out), 2 water, 3 lava, 0 nothing. */
static int kind_of(Game *g, int x, int y, int z, Block *out) {
    bool known;
    Block raw = chunkmap_block_raw(&g->map, x, y, z, &known);
    *out = raw;
    if (!known || (raw & BLOCK_CARVED)) return 0;
    switch (raw) {
    case B_AIR: case B_FIRE: case B_TORCH: return 1;
    case B_WATER: return 2;
    case B_LAVA: return 3;
    default: return 0;
    }
}

static bool set_water(Game *g, int x, int y, int z, int level) {
    if (level <= 0) return chunkmap_edit(&g->map, x, y, z, B_AIR, 0, 0);
    return chunkmap_edit(&g->map, x, y, z, B_WATER, 0, MINI(level, 8));
}

static void hiss(Game *g, int x, int y, int z, bool steam) {
    vec3 p = v3(x + 0.5f, y + 0.6f, z + 0.5f);
    particles_burst(g, p, steam ? v3(0.85f, 0.85f, 0.9f) : v3(0.35f, 0.35f, 0.35f), steam ? 14 : 6, 2.0f, 0.07f, false);
    sfx_at(g, SFX_SALAMANDER_HISS, p, steam ? 1.0f : 0.6f);
}

/* Moves n eighths from (x, y, z) at level l to the cell t of kind tk holding tl. False if a
 * chunk is busy (the cell stays awake and tries again). */
static bool flow(Game *g, int x, int y, int z, int *l, int tx, int ty, int tz, int tk, int tl, int n, Block tb) {
    if (!chunkmap_can_edit(&g->map, x, y, z) || !chunkmap_can_edit(&g->map, tx, ty, tz)) return false;
    if (tk == 3) {
        /* water on lava: the lava cools to rock, the water boils off */
        if (dig_protected(g, tx, ty, tz)) return false;
        chunkmap_edit(&g->map, tx, ty, tz, B_ROCK, 0, 0);
        *l = MAXI(0, *l - 3);
        set_water(g, x, y, z, *l);
        hiss(g, tx, ty, tz, true);
        return true;
    }
    if (tk == 1 && tb != B_AIR) hiss(g, tx, ty, tz, false); /* a fire or torch goes out */
    *l -= n;
    set_water(g, x, y, z, *l);
    set_water(g, tx, ty, tz, tl + n);
    return true;
}

static const int SIDE[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

static void step_cell(Game *g, Cell c) {
    int l = chunkmap_fluid(&g->map, c.x, c.y, c.z);
    if (!l) {
        /* lava next to standing water also reacts when woken */
        return;
    }
    if (world_is_pocket(floordiv(c.x, CHUNK), floordiv(c.y, CHUNK), floordiv(c.z, CHUNK))) return;
    Block tb;
    /* down first */
    int k = kind_of(g, c.x, c.y - 1, c.z, &tb);
    if (k) {
        int tl = k == 2 ? chunkmap_fluid(&g->map, c.x, c.y - 1, c.z) : 0;
        int n = k == 3 ? l : MINI(l, 8 - tl);
        if (n > 0) {
            if (!flow(g, c.x, c.y, c.z, &l, c.x, c.y - 1, c.z, k, tl, n, tb)) { push(c.x, c.y, c.z); return; }
            if (l <= 0) return;
        }
    }
    /* then sideways, toward the lowest neighbour, a unit at a time */
    int r = (int)(cell_hash(c.x, c.y, c.z) ^ tick_no) & 3;
    for (int iter = 0; iter < 4 && l > 0; iter++) {
        int best = -1, best_l = 99, best_k = 0;
        Block best_b = 0;
        for (int s = 0; s < 4; s++) {
            int d = (s + r) & 3;
            int nx = c.x + SIDE[d][0], nz = c.z + SIDE[d][1];
            Block b;
            int nk = kind_of(g, nx, c.y, nz, &b);
            if (!nk) continue;
            int nl = nk == 2 ? chunkmap_fluid(&g->map, nx, c.y, nz) : 0;
            if (nk == 3) nl = -1; /* lava: always */
            /* a thin film only runs toward a drop */
            if (nk == 1 && l == 1) {
                Block bb;
                int below = kind_of(g, nx, c.y - 1, nz, &bb);
                if (below != 1 && !(below == 2 && chunkmap_fluid(&g->map, nx, c.y - 1, nz) < 8)) continue;
                nl = -1;
            }
            if (nl < best_l) { best_l = nl; best = d; best_k = nk; best_b = b; }
        }
        if (best < 0 || (best_l >= 0 && l - best_l < 2)) break;
        int tl = best_l < 0 ? 0 : best_l;
        if (!flow(g, c.x, c.y, c.z, &l, c.x + SIDE[best][0], c.y, c.z + SIDE[best][1], best_k, tl, 1, best_b)) {
            push(c.x, c.y, c.z);
            return;
        }
    }
}

void sim_update(Game *g, float dt) {
    acc += dt;
    if (acc < 1.0f / SIM_HZ) return;
    acc = 0;
    tick_no++;
    /* this tick works through what was woken since the last one */
    memcpy(q_now, q_next, sizeof(Cell) * (size_t)n_next);
    n_now = n_next;
    n_next = 0;
    memset(seen, 0, sizeof seen);
    int n = MINI(n_now, SIM_BUDGET);
    for (int i = 0; i < n; i++) step_cell(g, q_now[i]);
    /* what did not fit waits for the next tick */
    for (int i = n; i < n_now; i++) push(q_now[i].x, q_now[i].y, q_now[i].z);
}

int sim_pending(void) { return n_next; }
