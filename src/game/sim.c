/* The world simulation, part 2: water that flows (docs/SIMULATION.md).
 *
 * Water keeps a level of 1..8 eighths per cell (Chunk.fluid; generated water is full). Only
 * cells that something changed are simulated: every lasting change to the world
 * (chunkmap_edit) wakes the cells around it, and a cell that moves water wakes its neighbours
 * again. Water is conserved: it falls into the cell below, then evens out sideways until
 * neighbouring levels differ by one at most, and a thin film runs off edges. Where it meets
 * lava, the lava cools to rock and the water boils off; it puts out fires and torches.
 *
 * Part 3: fire and gases. A fire the simulation lit keeps its fuel in the same level byte; it
 * burns what it can reach (wood, webs, grass, roots), melts ice and smokes, and goes out when
 * its fuel is spent (the fires the world was generated with burn for ever and never spread).
 * Gases are drifting clouds rather than cells: steam, smoke, poison, swamp gas (which explodes
 * when fire reaches it) and spores. Spells and bombs act on all of it by their element. */
#include "game/game.h"
#include "world/pocket.h"

#define SIM_HZ 8.0f
#define SIM_BUDGET 600   /* cells per tick */
#define SIM_CAP 16384    /* cells waiting */

typedef struct { int x, y, z; } Cell;
typedef struct Gas { vec3 pos; float amount, r, t; u8 kind; bool alive; } Gas;
#define MAX_GAS 96
static Gas gas[MAX_GAS];

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
    memset(gas, 0, sizeof gas);
    n_now = n_next = 0;
    memset(seen, 0, sizeof seen);
    acc = 0;
}

static void sim_on_edit(int x, int y, int z) {
    heat_mark(x, y, z);
    push(x, y, z);
    push(x + 1, y, z); push(x - 1, y, z);
    push(x, y + 1, z); push(x, y - 1, z);
    push(x, y, z + 1); push(x, y, z - 1);
}

/* A chunk comes back with changes: what was moving in it (water, lava, fire) moves on. */
static void sim_on_loaded(int cx, int cy, int cz) {
    if (!g_sim) return;
    int n;
    const BlockEdit *e = chunkmap_chunk_edits(&g_sim->map, cx, cy, cz, &n);
    for (int i = 0; i < n; i++) {
        if (!(e[i].raw == B_WATER || e[i].raw == B_LAVA || (e[i].raw == B_FIRE && e[i].level))) continue;
        int lx = e[i].idx % CHUNK, lz = (e[i].idx / CHUNK) % CHUNK, ly = e[i].idx / (CHUNK * CHUNK);
        push(cx * CHUNK + lx, cy * CHUNK + ly, cz * CHUNK + lz);
    }
}

void sim_init(Game *g) {
    g_sim = g;
    n_now = n_next = 0;
    memset(seen, 0, sizeof seen);
    g->map.on_edit = sim_on_edit;
    g->map.on_loaded = sim_on_loaded;
}

/* Saving the clouds (the cells are saved as edits). */
int sim_gas_list(float *out, int max) {
    int n = 0;
    for (int i = 0; i < MAX_GAS && n < max; i++) {
        if (!gas[i].alive) continue;
        float *o = out + n++ * 6;
        o[0] = gas[i].pos.x; o[1] = gas[i].pos.y; o[2] = gas[i].pos.z;
        o[3] = gas[i].amount; o[4] = gas[i].r; o[5] = gas[i].kind;
    }
    return n;
}

void sim_gas_restore(const float *in) {
    int k = (int)in[5];
    if (k < 0 || k >= GAS_COUNT || !(in[3] > 0.0f)) return;
    for (int i = 0; i < MAX_GAS; i++)
        if (!gas[i].alive) {
            gas[i] = (Gas){ v3(in[0], in[1], in[2]), fminf(in[3], 1.0f), clampf(in[4], 0.3f, 3.0f), 0, (u8)k, true };
            return;
        }
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

/* What water can flow into: 1 empty (or a flame it puts out, or ash it washes away), 2 water, 3 lava, 0 nothing. */
static int kind_of(Game *g, int x, int y, int z, Block *out) {
    bool known;
    Block raw = chunkmap_block_raw(&g->map, x, y, z, &known);
    *out = raw;
    if (!known || (raw & BLOCK_CARVED)) return 0;
    switch (raw) {
    case B_AIR: case B_FIRE: case B_TORCH: case B_ASH: return 1;
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
        chunkmap_edit(&g->map, tx, ty, tz, B_OBSIDIAN, 0, 0);
        *l = MAXI(0, *l - 3);
        set_water(g, x, y, z, *l);
        hiss(g, tx, ty, tz, true);
        sim_gas(g, GAS_STEAM, v3(tx + 0.5f, ty + 1.2f, tz + 0.5f), 0.6f);
        return true;
    }
    if (tk == 1 && tb != B_AIR) hiss(g, tx, ty, tz, false); /* a fire or torch goes out */
    *l -= n;
    set_water(g, x, y, z, *l);
    set_water(g, tx, ty, tz, tl + n);
    return true;
}

static const int SIDE[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
static const int N6[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

/* ---------- gases ---------- */


static const struct { const char *name; vec3 color; float rise, decay, rmax; } GAS_DEF[GAS_COUNT] = {
    [GAS_STEAM] = { "steam", { 0.9f, 0.92f, 0.95f }, 1.2f, 0.35f, 2.0f },
    [GAS_SMOKE] = { "smoke", { 0.22f, 0.21f, 0.2f }, 0.6f, 0.08f, 2.6f },
    [GAS_POISON] = { "poison", { 0.45f, 0.85f, 0.25f }, -0.1f, 0.06f, 2.4f },
    [GAS_SWAMP] = { "swamp gas", { 0.7f, 0.75f, 0.35f }, 0.15f, 0.03f, 2.0f },
    [GAS_SPORES] = { "spores", { 0.85f, 0.75f, 0.55f }, 0.05f, 0.07f, 2.2f },
};

void sim_gas(Game *g, int kind, vec3 p, float amount) {
    (void)g;
    int slot = -1;
    for (int i = 0; i < MAX_GAS; i++) {
        Gas *q = &gas[i];
        if (!q->alive) { if (slot < 0) slot = i; continue; }
        /* the same gas close by: it grows */
        if (q->kind == kind && v3_len(v3_sub(q->pos, p)) < q->r) { q->amount = fminf(1.0f, q->amount + amount * 0.5f); return; }
    }
    if (slot < 0) {
        slot = 0;
        for (int i = 1; i < MAX_GAS; i++) if (gas[i].amount < gas[slot].amount) slot = i;
    }
    gas[slot] = (Gas){ p, fminf(1.0f, amount), 0.6f, 0, (u8)kind, true };
}

/* How much of a gas is around p (0..1). */
float sim_gas_at(vec3 p, int kind) {
    float a = 0;
    for (int i = 0; i < MAX_GAS; i++) {
        const Gas *q = &gas[i];
        if (!q->alive || q->kind != kind) continue;
        float d = v3_len(v3_sub(q->pos, p));
        if (d < q->r) a += q->amount * (1.0f - d / q->r * 0.5f);
    }
    return fminf(a, 1.0f);
}

static bool open_at(Game *g, vec3 p) { return game_solid(g, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z)) == 0; }

/* A cloud of swamp gas meets fire: it goes up in a blast. */
static void gas_ignite(Game *g, Gas *q) {
    q->alive = false;
    SDL_Log("swamp gas explodes at %.1f %.1f %.1f", q->pos.x, q->pos.y, q->pos.z);
    world_explode(g, q->pos, 1.2f + q->r * 0.8f, HARD_SOFT, 20.0f + 40.0f * q->amount, -1);
    sim_gas(g, GAS_SMOKE, q->pos, 0.6f);
}

void sim_gas_fire(Game *g, vec3 p, float r) {
    for (int i = 0; i < MAX_GAS; i++) {
        Gas *q = &gas[i];
        if (q->alive && q->kind == GAS_SWAMP && q->amount > 0.15f && v3_len(v3_sub(q->pos, p)) < q->r + r) gas_ignite(g, q);
    }
}

static void gas_update(Game *g, float dt) {
    for (int i = 0; i < MAX_GAS; i++) {
        Gas *q = &gas[i];
        if (!q->alive) continue;
        q->t += dt;
        q->amount -= GAS_DEF[q->kind].decay * dt;
        if (q->amount <= 0.02f) { q->alive = false; continue; }
        q->r = fminf(GAS_DEF[q->kind].rmax, q->r + 0.6f * dt);
        /* steam that meets cool air condenses and drips back down as water */
        if (q->kind == GAS_STEAM && heat_at(g, q->pos) < 30.0f) {
            q->amount -= 0.4f * dt;
            if (hash_unit(mix64((u64)(q->t * 1000) + (u64)i * 31)) < 0.6f * dt) {
                int x = (int)floorf(q->pos.x), y = (int)floorf(q->pos.y), z = (int)floorf(q->pos.z);
                for (int k = 0; k < 8; k++, y--) {
                    if (game_solid(g, x, y - 1, z) == 0 && chunkmap_fluid(&g->map, x, y - 1, z) < 8) continue;
                    int l = chunkmap_fluid(&g->map, x, y, z);
                    bool known;
                    if ((l || chunkmap_block(&g->map, x, y, z, &known) == B_AIR) && l < 8) set_water(g, x, y, z, l + 1);
                    break;
                }
            }
        }
        /* drift: rise (or sink) and wander, sliding along what is solid */
        float w = q->t * 0.7f + i;
        vec3 v = v3(sinf(w) * 0.25f, GAS_DEF[q->kind].rise, cosf(w * 1.3f) * 0.25f);
        for (int ax = 0; ax < 3; ax++) {
            vec3 n = q->pos;
            if (ax == 0) n.x += v.x * dt; else if (ax == 1) n.y += v.y * dt; else n.z += v.z * dt;
            if (open_at(g, n)) q->pos = n;
        }
        /* fire anywhere in the cloud lights swamp gas (a few cells looked at each frame) */
        if (q->kind == GAS_SWAMP && q->amount > 0.15f) {
            bool lit = false;
            for (int k = 0; k < 4 && !lit; k++) {
                u64 h = mix64((u64)(q->t * 1e4) + (u64)k * 31 + (u64)i);
                vec3 o = v3_scale(v3(hash_unit(h) - 0.5f, hash_unit(h >> 16) - 0.5f, hash_unit(h >> 32) - 0.5f), 2.0f * q->r);
                if (k == 0) o = v3(0, 0, 0);
                bool known;
                Block b = chunkmap_block(&g->map, (int)floorf(q->pos.x + o.x), (int)floorf(q->pos.y + o.y), (int)floorf(q->pos.z + o.z), &known);
                lit = b == B_FIRE || b == B_LAVA || b == B_TORCH || b == B_BRAZIER;
            }
            if (lit) { gas_ignite(g, q); continue; }
        }
        /* what it does to those inside */
        if (!game_nearest_player(g, q->pos, 60.0f)) continue;
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *ps = &g->players[k];
            if (!ps->active || ps->st.dead) continue;
            vec3 head = player_eye(&ps->pl);
            float d = v3_len(v3_sub(q->pos, head));
            if (d > q->r) continue;
            float a = q->amount * (1.0f - d / q->r * 0.5f);
            switch (q->kind) {
            case GAS_STEAM: if (a > 0.3f) player_element_hit(g, ps, EL_FIRE, 4.0f * a * dt); break;
            case GAS_SMOKE: ps->breath = fmaxf(0.0f, ps->breath - 1.6f * a * dt); break; /* (breath comes back at 4/s) */
            case GAS_POISON: player_element_hit(g, ps, EL_POISON, 6.0f * a * dt); break;
            case GAS_SPORES: if (a > 0.4f) player_element_hit(g, ps, EL_POISON, 2.0f * a * dt); break;
            default: break;
            }
        }
        if (q->kind == GAS_POISON || q->kind == GAS_STEAM)
            for (int k = 0; k < MAX_MONSTERS; k++) {
                Monster *m = &g->mons[k];
                if (!m->alive || v3_len(v3_sub(m->pos, q->pos)) > q->r) continue;
                if (q->kind == GAS_POISON) { m->poison_t = fmaxf(m->poison_t, 1.0f); m->poison_dps = fmaxf(m->poison_dps, 6.0f * q->amount); }
                else { m->burn_t = fmaxf(m->burn_t, 0.5f); m->burn_dps = fmaxf(m->burn_dps, 3.0f * q->amount); }
            }
    }
}

/* Clouds are drawn as slow motes around their centre. */
static void gas_draw_particles(Game *g, float dt) {
    for (int i = 0; i < MAX_GAS; i++) {
        Gas *q = &gas[i];
        if (!q->alive || !game_nearest_player(g, q->pos, 40.0f)) continue;
        float rate = 10.0f + 30.0f * q->amount * q->r;
        int n = (int)(rate * dt + hash_unit(mix64((u64)(g->time * 1000) + (u64)i)));
        for (int k = 0; k < n && g->nparts < MAX_PARTICLES - 200; k++) {
            u64 h = mix64((u64)(g->time * 1e5) + (u64)i * 977 + (u64)k);
            Particle *pt = &g->parts[g->nparts++];
            vec3 o = v3(hash_unit(h) - 0.5f, hash_unit(h >> 11) - 0.5f, hash_unit(h >> 22) - 0.5f);
            pt->pos = v3_add(q->pos, v3_scale(o, q->r * 1.4f));
            pt->vel = v3(0, GAS_DEF[q->kind].rise * 0.3f, 0);
            pt->color = v3_scale(GAS_DEF[q->kind].color, 0.8f + 0.3f * hash_unit(h >> 33));
            pt->max_life = pt->life = 1.2f + hash_unit(h >> 40);
            pt->size = 0.1f + 0.12f * q->amount;
            pt->emissive = false;
            pt->gravity = false;
        }
    }
}

/* ---------- fire ---------- */

static bool flammable(Block b) { return b == B_WOOD || b == B_WEB || b == B_GRASS || b == B_ROOTS || b == B_VINE || b == B_FLOWER || b == B_HIVE; }

/* Sets (x, y, z) alight if it burns: what is there becomes fire with fuel by what it was. */
static bool ignite(Game *g, int x, int y, int z) {
    bool known;
    Block raw = chunkmap_block_raw(&g->map, x, y, z, &known);
    if (!known || (raw & BLOCK_CARVED) || dig_protected(g, x, y, z)) return false;
    int fuel = 0;
    if (raw == B_WOOD) fuel = 8;
    else if (raw == B_WEB || raw == B_GRASS || raw == B_ROOTS || raw == B_VINE || raw == B_FLOWER || raw == B_HIVE) fuel = 2;
    else if (raw == B_AIR) {
        /* air beside something that burns catches from it */
        for (int k = 0; k < 6 && !fuel; k++) {
            Block nb = chunkmap_block_raw(&g->map, x + N6[k][0], y + N6[k][1], z + N6[k][2], &known);
            if (known && flammable(nb)) fuel = 1;
        }
    }
    if (!fuel) return false;
    if (!chunkmap_edit(&g->map, x, y, z, B_FIRE, 0, fuel)) return false;
    particles_burst(g, v3(x + 0.5f, y + 0.4f, z + 0.5f), v3(1.0f, 0.55f, 0.15f), 6, 1.5f, 0.05f, true);
    return true;
}

static void fire_step(Game *g, Cell c, int fuel) {
    u64 h = mix64(cell_hash(c.x, c.y, c.z) * 0x9E3779B97F4A7C15ull + tick_no);
    /* it spreads to what burns around it, and warms what melts */
    for (int k = 0; k < 6; k++) {
        int x = c.x + N6[k][0], y = c.y + N6[k][1], z = c.z + N6[k][2];
        bool known;
        Block nb = chunkmap_block_raw(&g->map, x, y, z, &known);
        if (!known) continue;
        if (nb == B_ICE && (h >> (k * 5) & 15) == 0 && !dig_protected(g, x, y, z)) {
            chunkmap_edit(&g->map, x, y, z, B_WATER, 0, 8);
            sim_gas(g, GAS_STEAM, v3(x + 0.5f, y + 0.8f, z + 0.5f), 0.3f);
        }
        /* fire climbs faster than it creeps */
        int chance = k == 2 ? 5 : 10;
        if (flammable(nb) && (int)((h >> (k * 7 + 3)) % (u64)chance) == 0) ignite(g, x, y, z);
    }
    if ((h >> 50 & 15) == 0) sim_gas(g, GAS_SMOKE, v3(c.x + 0.5f, c.y + 1.2f, c.z + 0.5f), 0.25f);
    if ((h >> 56 & 7) == 0) {
        fuel--;
        /* burnt out: ash where it lay on something, nothing where it hung */
        Block out = fuel > 0 ? B_FIRE : B_AIR;
        if (fuel <= 0 && (h >> 60 & 1) == 0) {
            bool known;
            if (block_solid(chunkmap_block(&g->map, c.x, c.y - 1, c.z, &known))) out = B_ASH;
        }
        if (!chunkmap_edit(&g->map, c.x, c.y, c.z, out, 0, fuel)) { push(c.x, c.y, c.z); return; }
        if (fuel <= 0) return;
    }
    push(c.x, c.y, c.z); /* still burning */
}

/* Those standing in fire burn. */
static void fire_touch(Game *g) {
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead || ps->pl.fly) continue;
        bool known;
        Block b = chunkmap_block(&g->map, (int)floorf(ps->pl.pos.x), (int)floorf(ps->pl.pos.y + 0.1f), (int)floorf(ps->pl.pos.z), &known);
        if (b == B_FIRE && chunkmap_level(&g->map, (int)floorf(ps->pl.pos.x), (int)floorf(ps->pl.pos.y + 0.1f), (int)floorf(ps->pl.pos.z)))
            player_element_hit(g, ps, EL_FIRE, 3.0f);
    }
    for (int k = 0; k < MAX_MONSTERS; k++) {
        Monster *m = &g->mons[k];
        if (!m->alive) continue;
        bool known;
        if (chunkmap_block(&g->map, (int)floorf(m->pos.x), (int)floorf(m->pos.y + 0.1f), (int)floorf(m->pos.z), &known) == B_FIRE) {
            m->burn_t = fmaxf(m->burn_t, 2.0f);
            m->burn_dps = fmaxf(m->burn_dps, 6.0f);
        }
    }
}

/* Catching up a chunk that was out of reach for dt seconds (away.c): the fires that were
 * burning there spread once to what burns beside them and have burnt out since. False if the
 * chunk is busy: try again. */
bool sim_catch_up_fires(Game *g, int cx, int cy, int cz, float dt) {
    if (dt < 20.0f) return true;
    int n;
    const BlockEdit *e = chunkmap_chunk_edits(&g->map, cx, cy, cz, &n);
    Cell fires[256];
    int nf = 0;
    for (int i = 0; i < n && nf < 256; i++) {
        if (e[i].raw != B_FIRE || !e[i].level) continue;
        int lx = e[i].idx % CHUNK, lz = (e[i].idx / CHUNK) % CHUNK, ly = e[i].idx / (CHUNK * CHUNK);
        fires[nf++] = (Cell){ cx * CHUNK + lx, cy * CHUNK + ly, cz * CHUNK + lz };
    }
    if (!nf) return true;
    if (!chunkmap_can_edit(&g->map, cx * CHUNK + CHUNK / 2, cy * CHUNK + CHUNK / 2, cz * CHUNK + CHUNK / 2)) return false;
    /* one more round of spreading, then everything that burned is ash (where it lay) or gone */
    int first = nf;
    for (int i = 0; i < first && nf < 256; i++)
        for (int k = 0; k < 6 && nf < 256; k++) {
            int x = fires[i].x + N6[k][0], y = fires[i].y + N6[k][1], z = fires[i].z + N6[k][2];
            bool known;
            Block b = chunkmap_block_raw(&g->map, x, y, z, &known);
            if (known && flammable(b) && (mix64(cell_hash(x, y, z)) & 1) && ignite(g, x, y, z)) fires[nf++] = (Cell){ x, y, z };
        }
    for (int i = 0; i < nf; i++) {
        bool known;
        Block out = block_solid(chunkmap_block(&g->map, fires[i].x, fires[i].y - 1, fires[i].z, &known)) ? B_ASH : B_AIR;
        chunkmap_edit(&g->map, fires[i].x, fires[i].y, fires[i].z, out, 0, 0);
    }
    return true;
}

bool sim_ignite(Game *g, int x, int y, int z) { return ignite(g, x, y, z); }

/* ---------- elements ---------- */

/* Water cells joined to (x, y, z), up to max: lightning runs through all of them. */
static int water_body(Game *g, int x, int y, int z, Cell *out, int max) {
    int n = 0;
    if (!chunkmap_fluid(&g->map, x, y, z)) return 0;
    out[n++] = (Cell){ x, y, z };
    for (int i = 0; i < n && n < max; i++)
        for (int k = 0; k < 6 && n < max; k++) {
            Cell c = { out[i].x + N6[k][0], out[i].y + N6[k][1], out[i].z + N6[k][2] };
            if (abs(c.x - x) > 10 || abs(c.z - z) > 10 || abs(c.y - y) > 4 || !chunkmap_fluid(&g->map, c.x, c.y, c.z)) continue;
            bool dup = false;
            for (int j = 0; j < n && !dup; j++) dup = out[j].x == c.x && out[j].y == c.y && out[j].z == c.z;
            if (!dup) out[n++] = c;
        }
    return n;
}

static bool in_body(const Cell *body, int n, vec3 p) {
    int x = (int)floorf(p.x), y = (int)floorf(p.y + 0.1f), z = (int)floorf(p.z);
    for (int i = 0; i < n; i++)
        if (body[i].x == x && body[i].z == z && (body[i].y == y || body[i].y == y - 1)) return true;
    return false;
}

void sim_element_hit(Game *g, vec3 p, int el, int owner, float dmg) {
    int x0 = (int)floorf(p.x), y0 = (int)floorf(p.y), z0 = (int)floorf(p.z);
    /* fire and frost are heat and cold too */
    if (el == EL_FIRE) heat_add(g, p, 60.0f + dmg * 2.0f);
    else if (el == EL_FROST) heat_add(g, p, -(25.0f + dmg));
    switch (el) {
    case EL_FIRE:
        for (int dy = -1; dy <= 1; dy++)
            for (int dz = -1; dz <= 1; dz++)
                for (int dx = -1; dx <= 1; dx++) {
                    int x = x0 + dx, y = y0 + dy, z = z0 + dz;
                    bool known;
                    Block b = chunkmap_block_raw(&g->map, x, y, z, &known);
                    if (!known) continue;
                    if (b == B_ICE && !dig_protected(g, x, y, z)) {
                        chunkmap_edit(&g->map, x, y, z, B_WATER, 0, 8);
                        sim_gas(g, GAS_STEAM, v3(x + 0.5f, y + 1.0f, z + 0.5f), 0.4f);
                    } else if (b == B_WATER && dy >= 0) sim_gas(g, GAS_STEAM, v3(x + 0.5f, y + 1.0f, z + 0.5f), 0.3f);
                    else if (flammable(b) || (b == B_AIR && (dx || dy || dz) == 0)) ignite(g, x, y, z);
                }
        sim_gas_fire(g, p, 1.0f);
        break;
    case EL_FROST:
        /* water freezes: its surface (and a little below) turns to ice you can walk on; fire dies */
        for (int dy = -2; dy <= 1; dy++)
            for (int dz = -2; dz <= 2; dz++)
                for (int dx = -2; dx <= 2; dx++) {
                    if (dx * dx + dz * dz + dy * dy > 6) continue;
                    int x = x0 + dx, y = y0 + dy, z = z0 + dz;
                    bool known;
                    Block b = chunkmap_block_raw(&g->map, x, y, z, &known);
                    if (!known || dig_protected(g, x, y, z)) continue;
                    if (b == B_WATER && chunkmap_block(&g->map, x, y + 1, z, &known) != B_WATER && !cell_occupied(g, x, y, z)) {
                        if (chunkmap_edit(&g->map, x, y, z, B_ICE, 0, 0))
                            particles_burst(g, v3(x + 0.5f, y + 1.0f, z + 0.5f), v3(0.7f, 0.9f, 1.0f), 3, 1.0f, 0.04f, true);
                    } else if (b == B_FIRE && chunkmap_level(&g->map, x, y, z)) chunkmap_edit(&g->map, x, y, z, B_AIR, 0, 0);
                }
        break;
    case EL_LIGHTNING: {
        /* lightning in water runs through all of it */
        Cell body[400];
        int n = 0;
        for (int k = 0; k < 7 && !n; k++) {
            int dx = k == 0 ? 0 : N6[k - 1][0], dy = k == 0 ? 0 : N6[k - 1][1], dz = k == 0 ? 0 : N6[k - 1][2];
            n = water_body(g, x0 + dx, y0 + dy, z0 + dz, body, 400);
        }
        if (n < 2) break;
        for (int i = 0; i < n; i += 3)
            if (chunkmap_block(&g->map, body[i].x, body[i].y + 1, body[i].z, &(bool){ 0 }) != B_WATER)
                particles_burst(g, v3(body[i].x + 0.5f, body[i].y + 0.95f, body[i].z + 0.5f), v3(0.6f, 0.8f, 1.0f), 2, 2.5f, 0.04f, true);
        flash_add(g, p, v3(0.6f, 0.8f, 1.2f), 10.0f, 0.3f);
        for (int k = 0; k < MAX_MONSTERS; k++) {
            Monster *m = &g->mons[k];
            if (m->alive && in_body(body, n, m->pos)) monster_damage(g, m, dmg * 0.8f, m->pos, 0.0f, owner);
        }
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *ps = &g->players[k];
            if (ps->active && !ps->st.dead && in_body(body, n, ps->pl.pos)) game_hurt_player(g, ps, dmg * (k == owner ? 0.3f : 0.5f), p);
        }
        break;
    }
    case EL_POISON: sim_gas(g, GAS_POISON, p, 0.5f); break;
    default: break;
    }
}

/* ---------- seeping ---------- */

/* Free pieces of a broken block on its side facing -d (where water comes in) and on its far
 * side: water gets through where both are open and the block is at least an eighth hollow. */
static bool porous(u64 mask, int dx, int dy, int dz) {
    if (__builtin_popcountll(~mask) < 8) return false;
    bool near = false, far = false;
    for (int a = 0; a < 4; a++)
        for (int b = 0; b < 4; b++) {
            int sx, sy, sz, fx, fy, fz;
            if (dx) { sx = dx > 0 ? 0 : 3; fx = 3 - sx; sy = fy = a; sz = fz = b; }
            else if (dy) { sy = dy > 0 ? 0 : 3; fy = 3 - sy; sx = fx = a; sz = fz = b; }
            else { sz = dz > 0 ? 0 : 3; fz = 3 - sz; sx = fx = a; sy = fy = b; }
            near |= !(mask >> carve_bit(sx, sy, sz) & 1);
            far |= !(mask >> carve_bit(fx, fy, fz) & 1);
        }
    return near && far;
}

/* Water slowly seeps through a broken block into the cell beyond it (one eighth at a time). */
static void seep(Game *g, Cell c, int l) {
    static const int D[5][3] = { { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    for (int d = 0; d < 5 && l >= 2; d++) {
        int bx = c.x + D[d][0], by = c.y + D[d][1], bz = c.z + D[d][2];
        bool known;
        Block raw = chunkmap_block_raw(&g->map, bx, by, bz, &known);
        if (!known || !(raw & BLOCK_CARVED) || !porous(chunkmap_mask(&g->map, bx, by, bz), D[d][0], D[d][1], D[d][2])) continue;
        int tx = bx + D[d][0], ty = by + D[d][1], tz = bz + D[d][2];
        Block tb;
        int tk = kind_of(g, tx, ty, tz, &tb);
        if (tk != 1 && tk != 2) continue;
        int tl = tk == 2 ? chunkmap_fluid(&g->map, tx, ty, tz) : 0;
        if (tl >= 8 || (d > 0 && l - tl < 2)) continue;
        if ((cell_hash(c.x, c.y, c.z) ^ tick_no) & 1) { push(c.x, c.y, c.z); return; } /* slowly */
        if (!flow(g, c.x, c.y, c.z, &l, tx, ty, tz, tk, tl, 1, tb)) { push(c.x, c.y, c.z); return; }
        particles_burst(g, v3(bx + 0.5f, by + 0.5f, bz + 0.5f), v3(0.4f, 0.55f, 0.7f), 2, 0.5f, 0.03f, false);
    }
}

/* ---------- lava ---------- */

/* The level of a lava cell, 1..8 (generated lava is full), 0 if it is not lava. */
int sim_lava(Game *g, int x, int y, int z) {
    bool known;
    if (chunkmap_block_raw(&g->map, x, y, z, &known) != B_LAVA) return 0;
    int l = chunkmap_level(&g->map, x, y, z);
    return l ? l : 8;
}

static bool set_lava(Game *g, int x, int y, int z, int level) {
    if (level <= 0) return chunkmap_edit(&g->map, x, y, z, B_AIR, 0, 0);
    return chunkmap_edit(&g->map, x, y, z, B_LAVA, 0, MINI(level, 8));
}

/* What lava can run into: 1 empty or something it burns away, 2 lava, 0 nothing. */
static int lava_kind(Game *g, int x, int y, int z) {
    Block b;
    int k = kind_of(g, x, y, z, &b);
    if (k == 1) return 1;
    if (k == 3) return 2;
    switch (b) {
    case B_GRASS: case B_FLOWER: case B_VINE: case B_WEB: case B_ROOTS: case B_BONES: case B_FUNGUS: return 1;
    default: return 0;
    }
}

/* Lava is slow and thick: it moves on every third tick, runs sideways only where it stands
 * three eighths above its neighbour, and a thin tongue of it crusts over into obsidian. It
 * sets what burns alight, melts ice and is quenched by water. */
static void lava_step(Game *g, Cell c, int l) {
    u64 h = mix64(cell_hash(c.x, c.y, c.z) + (u64)tick_no * 0x9E3779B97F4A7C15ull);
    if ((tick_no + (cell_hash(c.x, c.y, c.z) & 3)) % 3) { push(c.x, c.y, c.z); return; }
    if (!chunkmap_can_edit(&g->map, c.x, c.y, c.z)) { push(c.x, c.y, c.z); return; }
    /* heat and water around it */
    for (int k = 0; k < 6; k++) {
        int x = c.x + N6[k][0], y = c.y + N6[k][1], z = c.z + N6[k][2];
        bool known;
        Block nb = chunkmap_block_raw(&g->map, x, y, z, &known);
        if (!known) continue;
        if (nb == B_WATER && k != 3) {
            /* quenched: the lava turns to obsidian, the water to steam */
            int wl = chunkmap_fluid(&g->map, x, y, z);
            if (!chunkmap_can_edit(&g->map, x, y, z)) { push(c.x, c.y, c.z); return; }
            chunkmap_edit(&g->map, c.x, c.y, c.z, B_OBSIDIAN, 0, 0);
            set_water(g, x, y, z, wl - 3);
            hiss(g, c.x, c.y, c.z, true);
            sim_gas(g, GAS_STEAM, v3(c.x + 0.5f, c.y + 1.2f, c.z + 0.5f), 0.6f);
            return;
        }
        if (flammable(nb) && (h >> (k * 4) & 3) == 0) ignite(g, x, y, z);
        if (nb == B_ICE && !dig_protected(g, x, y, z)) {
            chunkmap_edit(&g->map, x, y, z, B_WATER, 0, 8);
            sim_gas(g, GAS_STEAM, v3(x + 0.5f, y + 1.0f, z + 0.5f), 0.4f);
        }
    }
    /* down */
    int k = lava_kind(g, c.x, c.y - 1, c.z);
    if (k) {
        int tl = k == 2 ? sim_lava(g, c.x, c.y - 1, c.z) : 0;
        int n = MINI(l, 8 - tl);
        if (n > 0 && chunkmap_can_edit(&g->map, c.x, c.y - 1, c.z)) {
            l -= n;
            set_lava(g, c.x, c.y, c.z, l);
            set_lava(g, c.x, c.y - 1, c.z, tl + n);
            if (l <= 0) return;
        }
    }
    /* sideways, thickly */
    if (l >= 3) {
        int best = -1, best_l = 99;
        for (int s = 0; s < 4; s++) {
            int d = (s + (int)(h & 3)) & 3;
            int nx = c.x + SIDE[d][0], nz = c.z + SIDE[d][1];
            int nk = lava_kind(g, nx, c.y, nz);
            if (!nk) continue;
            int nl = nk == 2 ? sim_lava(g, nx, c.y, nz) : 0;
            if (nl < best_l) { best_l = nl; best = d; }
        }
        if (best >= 0 && l - best_l >= 3) {
            int nx = c.x + SIDE[best][0], nz = c.z + SIDE[best][1];
            if (chunkmap_can_edit(&g->map, nx, c.y, nz)) {
                l--;
                set_lava(g, c.x, c.y, c.z, l);
                set_lava(g, nx, c.y, nz, best_l + 1);
            } else push(c.x, c.y, c.z);
        }
    }
    /* a thin tongue cools and crusts over */
    if (l <= 2 && (h >> 40) % 24 == 0) {
        chunkmap_edit(&g->map, c.x, c.y, c.z, B_OBSIDIAN, 0, 0);
        particles_burst(g, v3(c.x + 0.5f, c.y + 0.4f, c.z + 0.5f), v3(0.4f, 0.4f, 0.4f), 4, 1.0f, 0.05f, false);
        return;
    }
    if (l <= 2) push(c.x, c.y, c.z); /* keeps cooling */
}

/* ---------- the tick ---------- */

static void step_cell(Game *g, Cell c) {
    if (world_is_pocket(floordiv(c.x, CHUNK), floordiv(c.y, CHUNK), floordiv(c.z, CHUNK))) return;
    int l = chunkmap_fluid(&g->map, c.x, c.y, c.z);
    if (!l) {
        bool known;
        Block b = chunkmap_block_raw(&g->map, c.x, c.y, c.z, &known);
        if (b == B_FIRE) {
            int fuel = chunkmap_level(&g->map, c.x, c.y, c.z);
            if (fuel) fire_step(g, c, fuel);
        } else if (b == B_LAVA && !dig_protected(g, c.x, c.y, c.z)) lava_step(g, c, sim_lava(g, c.x, c.y, c.z));
        return;
    }
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
    if (l >= 2) seep(g, c, l);
}

/* Still water with something growing at its edge rots, and sends up bubbles of swamp gas. */
static void swamp_bubbles(Game *g) {
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        for (int s = 0; s < 24; s++) {
            u64 h = mix64((u64)tick_no * 131 + (u64)s * 7919 + (u64)k);
            int x = (int)floorf(ps->pl.pos.x) + (int)(h % 25) - 12, z = (int)floorf(ps->pl.pos.z) + (int)((h >> 8) % 25) - 12;
            int y = (int)floorf(ps->pl.pos.y) + (int)((h >> 16) % 7) - 4;
            if (chunkmap_fluid(&g->map, x, y, z) != 8) continue;
            bool known;
            if (chunkmap_block(&g->map, x, y + 1, z, &known) != B_AIR) continue;
            bool rot = false;
            for (int d = 0; d < 4 && !rot; d++) {
                Block b = chunkmap_block(&g->map, x + SIDE[d][0], y + 1, z + SIDE[d][1], &known);
                rot = b == B_ROOTS || b == B_FUNGUS || b == B_GRASS;
            }
            if (!rot) continue;
            sim_gas(g, GAS_SWAMP, v3(x + 0.5f, y + 1.3f, z + 0.5f), 0.35f);
            particles_burst(g, v3(x + 0.5f, y + 1.0f, z + 0.5f), v3(0.6f, 0.7f, 0.4f), 3, 0.6f, 0.04f, false);
            break;
        }
    }
}

void sim_update(Game *g, float dt) {
    gas_update(g, dt);
    gas_draw_particles(g, dt);
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
    fire_touch(g);
    if (tick_no % 24 == 0) swamp_bubbles(g); /* every 3 s */
    /* what did not fit waits for the next tick */
    for (int i = n; i < n_now; i++) push(q_now[i].x, q_now[i].y, q_now[i].z);
}

int sim_pending(void) { return n_next; }
