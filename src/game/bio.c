/* The world simulation, part 4: living things (docs/SIMULATION.md).
 *
 * Plants grow by what they need, looked at in random cells around the players a few hundred
 * times a second: grass and flowers need light and earth, moss needs stone, shade and water
 * near, fungus needs the dark and something to feed on (bones, ash, other fungus), vines hang
 * from wet ceilings and grow down, and bees build hives where many flowers bloom. Fire leaves
 * ash, which grass takes over first. The dead leave bones that flies find and fungus eats.
 * Grazing animals crop grass. Insects are drawn as swarms near what draws them: flies on
 * bones and ash, bees at flowers and hives, moths at lights and glowworms on dark moss.
 * Growth goes through chunkmap_edit, so it is saved and regrows what was cut. */
#include "game/game.h"
#include "game/food.h"

#define BIO_SAMPLES 48     /* cells looked at per player per tick (8 ticks a second) */
#define BIO_RANGE 16       /* metres around each player */

static float bio_acc;
static u32 bio_tick;

/* ---------- what a cell offers ---------- */

static Block blk(Game *g, int x, int y, int z) {
    bool known;
    Block b = chunkmap_block_raw(&g->map, x, y, z, &known);
    return known ? b : B_ROCK;
}

static int light_at(Game *g, int x, int y, int z) {
    u8 l[4];
    if (!chunkmap_light(&g->map, x, y, z, l)) return 0;
    return MAXI(l[0], MAXI(l[1], l[2]));
}

static bool earth(Block b) { return b == B_DIRT || b == B_MOSS; }
static bool stone(Block b) { return b == B_ROCK || b == B_MOSS || b == B_DIRT || b == B_WOOD; }

/* Water within r cells straight out from (x, y, z) in the four directions and below. */
static bool water_near(Game *g, int x, int y, int z, int r) {
    static const int D[5][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 } };
    for (int d = 0; d < 5; d++)
        for (int k = 1; k <= r; k++) {
            Block b = blk(g, x + D[d][0] * k, y + D[d][1] * k, z + D[d][2] * k);
            if (b == B_WATER) return true;
            if (block_solid(b) && b != B_MOSS) break;
        }
    return false;
}

/* How many cells of a kind in the box of radius r around (x, y, z) (r small). */
static int count_near(Game *g, int x, int y, int z, int r, int ry, Block what) {
    int n = 0;
    for (int dy = -ry; dy <= ry; dy++)
        for (int dz = -r; dz <= r; dz++)
            for (int dx = -r; dx <= r; dx++) n += blk(g, x + dx, y + dy, z + dz) == what;
    return n;
}

static bool neighbour_is(Game *g, int x, int y, int z, Block what) {
    static const int D[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    for (int d = 0; d < 6; d++)
        if (blk(g, x + D[d][0], y + D[d][1], z + D[d][2]) == what) return true;
    return false;
}

static bool exposed(Game *g, int x, int y, int z) {
    static const int D[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    for (int d = 0; d < 6; d++)
        if (blk(g, x + D[d][0], y + D[d][1], z + D[d][2]) == B_AIR) return true;
    return false;
}

static int g_grown[B_COUNT]; /* growth since start, for the demo and the tests */

static void grow(Game *g, int x, int y, int z, Block b) {
    if (chunkmap_edit(&g->map, x, y, z, b, 0, 0)) g_grown[b]++;
}

int bio_grown(int b) { return b >= 0 && b < B_COUNT ? g_grown[b] : 0; }

/* ---------- the rules ---------- */

static void bio_cell(Game *g, int x, int y, int z, u64 h) {
    Block b = blk(g, x, y, z);
    float r = hash_unit(h);
    if (b == B_AIR || b == B_ASH) {
        Block below = blk(g, x, y - 1, z), above = blk(g, x, y + 1, z);
        int light = light_at(g, x, y, z);
        /* grass: on earth (or ash), in light, beside other grass; ash is taken over first */
        if ((earth(below) || (b == B_ASH && stone(below))) && light >= (b == B_ASH ? 4 : 6)) {
            if (count_near(g, x, y, z, 1, 1, B_GRASS) > 0 || (b == B_ASH && r < 0.3f)) {
                if (!dig_protected(g, x, y, z)) grow(g, x, y, z, B_GRASS);
                return;
            }
        }
        /* fungus: in the dark, on stone, earth or ash, beside fungus or bones */
        if (light <= 4 && (stone(below) || b == B_ASH) && r < 0.5f &&
            (neighbour_is(g, x, y, z, B_FUNGUS) || neighbour_is(g, x, y, z, B_BONES) || count_near(g, x, y, z, 1, 0, B_FUNGUS) > 0)) {
            if (!dig_protected(g, x, y, z)) grow(g, x, y, z, B_FUNGUS);
            return;
        }
        if (b != B_AIR) return;
        /* vines: under a wet or mossy ceiling with some light, and down from vines above */
        if (above == B_VINE && r < 0.6f) {
            int len = 1;
            while (len < 7 && blk(g, x, y + 1 + len, z) == B_VINE) len++;
            if (len < 6 && !block_solid(below) && !dig_protected(g, x, y, z)) grow(g, x, y, z, B_VINE);
            return;
        }
        if ((above == B_MOSS || (stone(above) && water_near(g, x, y, z, 3))) && light >= 3 && r < 0.08f && !block_solid(below)) {
            if (!dig_protected(g, x, y, z)) grow(g, x, y, z, B_VINE);
            return;
        }
        /* a hive: under a ceiling, where many flowers bloom and no hive is near */
        if (stone(above) && r < 0.01f && count_near(g, x, y - 2, z, 3, 2, B_FLOWER) >= 3 && count_near(g, x, y, z, 5, 3, B_HIVE) == 0) {
            if (!dig_protected(g, x, y, z)) grow(g, x, y, z, B_HIVE);
        }
        return;
    }
    if (b == B_GRASS) {
        /* flowers open where it is bright */
        if (light_at(g, x, y, z) >= 9 && r < 0.12f && count_near(g, x, y, z, 1, 0, B_FLOWER) < 2 && !dig_protected(g, x, y, z))
            grow(g, x, y, z, B_FLOWER);
        return;
    }
    if (b == B_ROCK) {
        /* moss: on stone open to the air, in the shade, with water near or moss beside */
        /* (damp: water in the box of 2 around, straight below or out to 3) */
        if (r < 0.3f && exposed(g, x, y, z) &&
            ((neighbour_is(g, x, y, z, B_MOSS) && r < 0.15f) || water_near(g, x, y, z, 3) || count_near(g, x, y, z, 2, 2, B_WATER) > 0)) {
            int light = MAXI(light_at(g, x, y + 1, z), light_at(g, x, y - 1, z));
            if (light <= 8 && !dig_protected(g, x, y, z)) grow(g, x, y, z, B_MOSS);
        }
        return;
    }
    if (b == B_BONES) {
        /* the dead feed the fungus */
        if (r < 0.04f && !dig_protected(g, x, y, z)) grow(g, x, y, z, B_FUNGUS);
        return;
    }
}

/* ---------- insects ---------- */

enum { SW_FLIES, SW_BEES, SW_MOTHS, SW_GLOW, SW_ANGRY };
typedef struct Swarm { vec3 pos; float t, life; u8 kind; bool alive; } Swarm;
#define MAX_SWARMS 24
static Swarm swarms[MAX_SWARMS];

static void swarm_add(int kind, vec3 p, float life) {
    int slot = -1;
    for (int i = 0; i < MAX_SWARMS; i++) {
        if (!swarms[i].alive) { if (slot < 0) slot = i; continue; }
        if (swarms[i].kind == kind && v3_len(v3_sub(swarms[i].pos, p)) < 2.5f) { swarms[i].life = fmaxf(swarms[i].life, life); return; }
    }
    if (slot < 0) return;
    swarms[slot] = (Swarm){ p, 0, life, (u8)kind, true };
}

/* What draws insects, found among the cells looked at. */
static void insect_source(Game *g, int x, int y, int z, Block b) {
    vec3 c = v3(x + 0.5f, y + 0.5f, z + 0.5f);
    switch (b) {
    case B_BONES: case B_ASH: swarm_add(SW_FLIES, v3_add(c, v3(0, 0.1f, 0)), 10.0f); break;
    case B_FLOWER: swarm_add(SW_BEES, v3_add(c, v3(0, 0.3f, 0)), 8.0f); break;
    case B_HIVE: swarm_add(SW_BEES, c, 12.0f); break;
    case B_TORCH: case B_LANTERN: swarm_add(SW_MOTHS, v3_add(c, v3(0, 0.4f, 0)), 10.0f); break;
    case B_MOSS:
        if (blk(g, x, y - 1, z) == B_AIR && light_at(g, x, y - 1, z) <= 3) swarm_add(SW_GLOW, v3(c.x, c.y - 0.55f, c.z), 15.0f);
        break;
    default: break;
    }
}

static void swarms_update(Game *g, float dt) {
    static const vec3 COL[5] = { { 0.08f, 0.08f, 0.07f }, { 0.95f, 0.8f, 0.2f }, { 0.85f, 0.82f, 0.7f }, { 0.5f, 1.0f, 0.55f }, { 1.0f, 0.75f, 0.1f } };
    for (int i = 0; i < MAX_SWARMS; i++) {
        Swarm *s = &swarms[i];
        if (!s->alive) continue;
        s->t += dt;
        s->life -= dt;
        PlayerSlot *near = game_nearest_player(g, s->pos, 30.0f);
        if (s->life <= 0 || !near) { s->alive = false; continue; }
        if (s->kind == SW_ANGRY) {
            /* stinging bees follow whoever broke into their hive */
            vec3 to = v3_sub(v3_add(near->pl.pos, v3(0, 1.2f, 0)), s->pos);
            float d = v3_len(to);
            if (d > 0.1f) s->pos = v3_add(s->pos, v3_scale(to, fminf(1.0f, 4.5f * dt / d)));
            if (d < 1.6f && !near->st.dead) game_dot_player(g, near, near->st.maxhp * 0.025f * dt);
            if (fmodf(s->t, 1.5f) < dt) sfx_at(g, SFX_BEES, s->pos, 0.6f);
            /* water or smoke drives them off */
            if (near->pl.water > 1.2f || sim_gas_at(s->pos, GAS_SMOKE) > 0.2f) s->life = fminf(s->life, 0.5f);
        }
        int n = s->kind == SW_ANGRY ? 6 : s->kind == SW_GLOW ? 1 : 2;
        float spread = s->kind == SW_GLOW ? 0.9f : s->kind == SW_ANGRY ? 0.9f : 0.5f;
        for (int k = 0; k < n && g->nparts < MAX_PARTICLES - 300; k++) {
            if (s->kind == SW_GLOW && hash_unit(mix64((u64)(g->time * 977) + (u64)i)) > 0.3f) break;
            u64 h = mix64((u64)(g->time * 1e5) + (u64)i * 7717 + (u64)k);
            Particle *p = &g->parts[g->nparts++];
            float a = hash_unit(h) * 2.0f * PI_F, rr = spread * hash_unit(h >> 12);
            p->pos = v3_add(s->pos, v3(cosf(a) * rr, (hash_unit(h >> 24) - 0.5f) * spread, sinf(a) * rr));
            p->vel = v3((hash_unit(h >> 32) - 0.5f) * 3.0f, (hash_unit(h >> 40) - 0.5f) * 1.5f, (hash_unit(h >> 48) - 0.5f) * 3.0f);
            if (s->kind == SW_GLOW) p->vel = v3(0, 0, 0);
            p->color = COL[s->kind];
            p->max_life = p->life = s->kind == SW_GLOW ? 1.5f : 0.18f;
            p->size = s->kind == SW_MOTHS ? 0.035f : 0.022f;
            p->emissive = s->kind == SW_GLOW || s->kind == SW_BEES || s->kind == SW_ANGRY;
            p->gravity = false;
        }
    }
}

/* A hive broken open: honey, and the bees. */
void bio_hive_broken(Game *g, vec3 p) {
    swarm_add(SW_ANGRY, p, 14.0f);
    sfx_at(g, SFX_BEES, p, 1.0f);
}

/* ---------- the dead ---------- */

/* What a death leaves: bones where it fell, for flies and later fungus. */
void bio_on_death(Game *g, vec3 p, u64 h) {
    if (hash_unit(h) > 0.35f) return;
    int x = (int)floorf(p.x), y = (int)floorf(p.y + 0.1f), z = (int)floorf(p.z);
    if (blk(g, x, y, z) != B_AIR || !block_solid(blk(g, x, y - 1, z)) || dig_protected(g, x, y, z)) return;
    chunkmap_edit(&g->map, x, y, z, B_BONES, 0, 0);
}

/* ---------- the tick ---------- */

void bio_update(Game *g, float dt) {
    swarms_update(g, dt);
    bio_acc += dt;
    if (bio_acc < 0.125f) return;
    bio_acc = 0;
    bio_tick++;
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        int px = (int)floorf(ps->pl.pos.x), py = (int)floorf(ps->pl.pos.y), pz = (int)floorf(ps->pl.pos.z);
        for (int s = 0; s < BIO_SAMPLES; s++) {
            u64 h = mix64((u64)bio_tick * 0x9E3779B97F4A7C15ull + (u64)s * 0xBF58476D1CE4E5B9ull + (u64)k);
            int x = px + (int)(h % (2 * BIO_RANGE + 1)) - BIO_RANGE, z = pz + (int)((h >> 10) % (2 * BIO_RANGE + 1)) - BIO_RANGE;
            int y = py + (int)((h >> 20) % 13) - 6;
            bio_cell(g, x, y, z, h >> 28);
            /* insects are looked for closer by */
            if (s < 16) {
                int ix = px + (int)((h >> 34) % 21) - 10, iz = pz + (int)((h >> 42) % 21) - 10, iy = py + (int)((h >> 50) % 9) - 3;
                insect_source(g, ix, iy, iz, blk(g, ix, iy, iz));
            }
        }
    }
}

void bio_reset(void) {
    memset(swarms, 0, sizeof swarms);
    memset(g_grown, 0, sizeof g_grown);
}
