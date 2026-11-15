/* The world simulation: temperature (docs/SIMULATION.md, docs/SIMULATION_PLAN.md step 2).
 *
 * A coarse field: one temperature per 2 m cube (16 x 16 x 16 per chunk), kept only for the
 * chunks around the players and rebuilt from what is there, never saved. Each region has a
 * base temperature (the frozen north is cold, the burning south and the deep strata hot); lava,
 * fires, braziers, torches and ice pull the cells they are in toward their own temperature,
 * and heat spreads to the neighbours. Spells add bursts of heat and cold. The rules that read it
 * are in bio.c (melting, freezing, boiling, igniting, growth), sim.c (steam condensing),
 * game.c (cold and warmth on players) and monsters.c (they keep out of the heat). */
#include "game/game.h"
#include "world/pocket.h"
#include "world/theme.h"

#define HC 16                 /* cells per chunk side */
#define HCV (HC * HC * HC)
#define MAX_HEAT 96
#define NO_SRC (-1000.0f)

typedef struct HeatChunk {
    bool used;
    int cx, cy, cz;
    float base;
    float t[HCV], src[HCV];
    float seen;               /* game time it was last within reach */
    bool dirty;               /* something in it changed: look at its sources again */
} HeatChunk;

static HeatChunk *heat;       /* MAX_HEAT, allocated on first use */
static int scan_next;
static float heat_acc;

static inline int hidx(int x, int y, int z) { return (y * HC + z) * HC + x; }

/* The base temperature of a region, by its flavour (north cold, south hot, deeper tiers more
 * so) and its stratum (the deep strata are hot, the high and airy ones cool). */
float heat_base_theme(int theme) {
    int st = theme_stratum(theme), fl = theme_flavor(theme);
    float t = 12.0f;
    if (fl >= FL_FROST && fl <= FL_LASTCOLD) t = -6.0f - 1.6f * (float)(fl - FL_FROST);
    else if (fl >= FL_SANDSTONE && fl <= FL_SUNBLAST) t = 26.0f + 2.0f * (float)(fl - FL_SANDSTONE);
    else if (fl == FL_SANCTUM) return 18.0f;
    if (st == ST_CORE || st == ST_MAW || st == ST_IRONHELL || st == ST_INFERNO) t += 18.0f;
    else if (st == ST_CLOUDS || st == ST_HEAVEN || st == ST_ASTRAL || st == ST_STORM) t -= 6.0f;
    return t;
}

static HeatChunk *hc_find(int cx, int cy, int cz) {
    if (!heat) return NULL;
    for (int i = 0; i < MAX_HEAT; i++)
        if (heat[i].used && heat[i].cx == cx && heat[i].cy == cy && heat[i].cz == cz) return &heat[i];
    return NULL;
}

/* What a block does to the temperature of its cell: NO_SRC if nothing. */
static float source_of(Block b, int level) {
    switch (b) {
    case B_LAVA: return 700.0f;
    case B_FIRE: return level ? 320.0f : 160.0f; /* a fire that spreads burns hotter than a hearth */
    case B_BRAZIER: return 130.0f;
    case B_TORCH: return 40.0f;
    case B_LANTERN: return 30.0f;
    case B_ICE: return 0.0f; /* ice keeps its cell at freezing; the cold comes from the region */
    default: return NO_SRC;
    }
}

/* Finds what heats or cools each cell of a chunk (the strongest source among its 8 blocks). */
static void scan(Game *g, HeatChunk *h) {
    Chunk *c = chunkmap_get(&g->map, h->cx, h->cy, h->cz);
    for (int i = 0; i < HCV; i++) h->src[i] = NO_SRC;
    if (!c || c->state != CH_READY) return;
    h->base = heat_base_theme(c->info.theme);
    for (int i = 0; i < CHUNK_VOL; i++) {
        Block b = c->blocks[i] & BLOCK_MAT;
        float s = source_of(b, c->fluid ? c->fluid[i] : 0);
        if (s == NO_SRC) continue;
        int x = i % CHUNK, z = (i / CHUNK) % CHUNK, y = i / (CHUNK * CHUNK);
        int k = hidx(x / 2, y / 2, z / 2);
        if (h->src[k] == NO_SRC || fabsf(s - h->base) > fabsf(h->src[k] - h->base)) h->src[k] = s;
    }
}

static HeatChunk *hc_get(Game *g, int cx, int cy, int cz) {
    HeatChunk *h = hc_find(cx, cy, cz);
    if (h) return h;
    Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
    if (!c || c->state != CH_READY) return NULL;
    if (!heat) heat = calloc(MAX_HEAT, sizeof(HeatChunk));
    int slot = -1;
    for (int i = 0; i < MAX_HEAT && slot < 0; i++)
        if (!heat[i].used) slot = i;
    if (slot < 0) {
        slot = 0;
        for (int i = 1; i < MAX_HEAT; i++)
            if (heat[i].seen < heat[slot].seen) slot = i;
    }
    h = &heat[slot];
    h->used = true;
    h->cx = cx; h->cy = cy; h->cz = cz;
    h->seen = g->time;
    scan(g, h);
    /* start settled: base, with the sources already at their own temperature */
    for (int i = 0; i < HCV; i++) h->t[i] = h->src[i] != NO_SRC ? (h->src[i] + h->base) * 0.5f : h->base;
    return h;
}

void heat_reset(void) {
    if (heat) memset(heat, 0, sizeof(HeatChunk) * MAX_HEAT);
    scan_next = 0;
    heat_acc = 0;
}

/* The temperature (degrees) at a world cell. Outside the field: the region's base. */
float heat_cell(Game *g, int x, int y, int z) {
    int cx = floordiv(x, CHUNK), cy = floordiv(y, CHUNK), cz = floordiv(z, CHUNK);
    HeatChunk *h = hc_find(cx, cy, cz);
    if (h) return h->t[hidx((x - cx * CHUNK) / 2, (y - cy * CHUNK) / 2, (z - cz * CHUNK) / 2)];
    Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
    return c && c->state == CH_READY ? heat_base_theme(c->info.theme) : 12.0f;
}

float heat_at(Game *g, vec3 p) { return heat_cell(g, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z)); }

/* A burst of heat (or cold, negative) at p: a fire spell, a frost one, a blast. */
void heat_add(Game *g, vec3 p, float degrees) {
    int x = (int)floorf(p.x), y = (int)floorf(p.y), z = (int)floorf(p.z);
    for (int d = 0; d < 7; d++) {
        int dx = d == 1 ? 2 : d == 2 ? -2 : 0, dy = d == 3 ? 2 : d == 4 ? -2 : 0, dz = d == 5 ? 2 : d == 6 ? -2 : 0;
        int wx = x + dx, wy = y + dy, wz = z + dz;
        int cx = floordiv(wx, CHUNK), cy = floordiv(wy, CHUNK), cz = floordiv(wz, CHUNK);
        HeatChunk *h = hc_find(cx, cy, cz);
        if (!h) continue;
        float *t = &h->t[hidx((wx - cx * CHUNK) / 2, (wy - cy * CHUNK) / 2, (wz - cz * CHUNK) / 2)];
        *t = clampf(*t + (d ? 0.4f : 1.0f) * degrees, -60.0f, 900.0f);
    }
}

/* One step of spreading for one chunk: each cell moves toward its neighbours, its base and its
 * source. Neighbours across a chunk border are read from that chunk if it has a field. */
static void diffuse(HeatChunk *h) {
    static float nt[HCV];
    HeatChunk *nb[6];
    static const int D[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    for (int k = 0; k < 6; k++) nb[k] = hc_find(h->cx + D[k][0], h->cy + D[k][1], h->cz + D[k][2]);
    for (int y = 0; y < HC; y++)
        for (int z = 0; z < HC; z++)
            for (int x = 0; x < HC; x++) {
                int i = hidx(x, y, z);
                float sum = 0;
                for (int k = 0; k < 6; k++) {
                    int ax = x + D[k][0], ay = y + D[k][1], az = z + D[k][2];
                    if (ax >= 0 && ay >= 0 && az >= 0 && ax < HC && ay < HC && az < HC) sum += h->t[hidx(ax, ay, az)];
                    else if (nb[k]) sum += nb[k]->t[hidx((ax + HC) % HC, (ay + HC) % HC, (az + HC) % HC)];
                    else sum += h->base;
                }
                float t = h->t[i];
                t += 0.22f * (sum / 6.0f - t) + 0.015f * (h->base - t);
                if (h->src[i] != NO_SRC) t += 0.35f * (h->src[i] - t);
                nt[i] = t;
            }
    memcpy(h->t, nt, sizeof nt);
}

/* A block changed at (x, y, z): its chunk's sources are looked at again soon. */
void heat_mark(int x, int y, int z) {
    HeatChunk *h = hc_find(floordiv(x, CHUNK), floordiv(y, CHUNK), floordiv(z, CHUNK));
    if (h) h->dirty = true;
}

int heat_chunks(void) {
    int n = 0;
    for (int i = 0; heat && i < MAX_HEAT; i++) n += heat[i].used;
    return n;
}

void heat_update(Game *g, float dt) {
    heat_acc += dt;
    if (heat_acc < 0.25f) return;
    heat_acc = 0;
    /* the chunks round each player have a field */
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active) continue;
        int pcx = floordiv((int)floorf(ps->pl.pos.x), CHUNK), pcy = floordiv((int)floorf(ps->pl.pos.y), CHUNK),
            pcz = floordiv((int)floorf(ps->pl.pos.z), CHUNK);
        for (int dy = -1; dy <= 1; dy++)
            for (int dz = -1; dz <= 1; dz++)
                for (int dx = -1; dx <= 1; dx++) {
                    HeatChunk *h = hc_get(g, pcx + dx, pcy + dy, pcz + dz);
                    if (h) h->seen = g->time;
                }
    }
    if (!heat) return;
    /* the air shimmers over what is very hot */
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active) continue;
        for (int s = 0; s < 10 && g->nparts < MAX_PARTICLES - 100; s++) {
            u64 h = mix64((u64)(g->time * 1000) + (u64)s * 7919 + (u64)k);
            int x = (int)floorf(ps->pl.pos.x) + (int)(h % 13) - 6, z = (int)floorf(ps->pl.pos.z) + (int)((h >> 8) % 13) - 6;
            int y = (int)floorf(ps->pl.pos.y) + (int)((h >> 16) % 6) - 2;
            bool known;
            if (heat_cell(g, x, y, z) < 150.0f || chunkmap_block(&g->map, x, y, z, &known) != B_AIR) continue;
            Particle *q = &g->parts[g->nparts++];
            *q = (Particle){ v3(x + hash_unit(h >> 24), y + hash_unit(h >> 32), z + hash_unit(h >> 40)), v3(0, 1.2f, 0), v3(0.9f, 0.75f, 0.55f), 0.7f, 0.7f, 0.025f, false, false };
        }
    }
    /* changed chunks are looked at again first (a few a tick) */
    for (int i = 0, n = 0; i < MAX_HEAT && n < 3; i++)
        if (heat[i].used && heat[i].dirty) { heat[i].dirty = false; scan(g, &heat[i]); n++; }
    /* what heats them is looked at again a chunk at a time; those left behind are let go */
    for (int n = 0; n < 2; n++) {
        HeatChunk *h = &heat[scan_next];
        scan_next = (scan_next + 1) % MAX_HEAT;
        if (!h->used) continue;
        if (g->time - h->seen > 10.0f) { h->used = false; continue; }
        scan(g, h);
    }
    for (int i = 0; i < MAX_HEAT; i++)
        if (heat[i].used) diffuse(&heat[i]);
}
