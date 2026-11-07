#include "game/game.h"

#include <ctype.h>

#include "render/batch.h"

/* For now monsters are simple shapes in strong colours. The family mix comes from the
 * stratum, the tint from the flavor, and toughness from the danger level. */

const MonsterDef MONSTERS[MON_TYPE_COUNT] = {
    /*               hw     h     hp   dmg  speed reach atk   xp  flies  colour                      windup lvl boss title */
    { "Crawler",    0.32f, 0.6f, 18,  5,  5.2f, 0.9f, 0.9f, 8,  false, { 0.85f, 0.6f, 0.25f },  0.35f, 0, "Crawler Queen" },
    { "Brute",      0.55f, 2.0f, 130, 16, 2.8f, 1.5f, 1.6f, 30, false, { 0.85f, 0.2f, 0.15f },  0.6f,  2, "Brute Warlord" },
    { "Wisp",       0.35f, 0.9f, 30,  9,  3.4f, 0.0f, 2.2f, 18, true,  { 0.3f, 0.55f, 1.0f },   0.35f, 2, "Wisp Matriarch" },
    { "Horned",     0.45f, 1.2f, 60,  13, 3.6f, 1.2f, 1.4f, 22, false, { 0.5f, 0.85f, 0.3f },   0.35f, 3, "Horned King" },
    { "Skeleton",   0.3f,  1.7f, 32,  8,  3.2f, 0.0f, 1.6f, 16, false, { 0.85f, 0.82f, 0.7f },  0.35f, 2, "Bone Lord" },
    { "Slime",      0.45f, 0.8f, 40,  7,  2.6f, 0.9f, 1.1f, 12, false, { 0.35f, 0.8f, 0.55f },  0.35f, 1, "Slime Mother" },
    { "Bat",        0.22f, 0.4f, 10,  4,  6.5f, 0.8f, 0.8f, 5,  true,  { 0.45f, 0.3f, 0.4f },   0.35f, 1, "Bat Sovereign" },
    { "Spider",     0.4f,  0.5f, 26,  9,  4.6f, 0.9f, 1.0f, 14, false, { 0.35f, 0.3f, 0.32f },  0.3f,  2, "Broodmother" },
    { "Wraith",     0.35f, 1.6f, 34,  12, 3.0f, 1.1f, 1.4f, 22, true,  { 0.6f, 0.65f, 0.75f },  0.45f, 6, "Wraith Lord" },
    { "Golem",      0.6f,  2.2f, 140, 22, 1.9f, 1.6f, 2.4f, 45, false, { 0.52f, 0.5f, 0.46f },  1.0f,  8, "Elder Golem" },
    { "Shaman",     0.3f,  1.5f, 36,  7,  3.0f, 0.0f, 2.0f, 26, false, { 0.55f, 0.4f, 0.6f },   0.35f, 5, "High Shaman" },
    { "Bloater",    0.5f,  1.1f, 30,  30, 2.4f, 1.2f, 0.8f, 15, false, { 0.6f, 0.65f, 0.35f },  0.8f,  4, "Plague Mother" },
    { "Eye",        0.4f,  0.8f, 44,  14, 1.6f, 0.0f, 3.0f, 28, true,  { 0.82f, 0.76f, 0.7f },  1.1f,  7, "Elder Eye" },
    { "Knight",     0.35f, 1.8f, 70,  15, 3.0f, 1.4f, 1.3f, 32, false, { 0.56f, 0.58f, 0.64f }, 0.4f,  6, "Black Knight" },
    { "Worm",       0.4f,  1.6f, 55,  16, 3.8f, 1.3f, 2.5f, 30, false, { 0.62f, 0.46f, 0.36f }, 0.5f,  6, "Great Wurm" },
    { "Mimic",      0.45f, 0.8f, 60,  18, 4.2f, 1.0f, 0.9f, 35, false, { 0.45f, 0.28f, 0.14f }, 0.25f, 4, "King Mimic" },
    { "Necromancer",0.3f,  1.7f, 40,  8,  2.8f, 0.0f, 5.0f, 34, false, { 0.3f, 0.26f, 0.36f },  0.35f, 9, "Lich" },
    { "Harpy",      0.35f, 0.9f, 24,  12, 5.5f, 0.9f, 2.8f, 18, true,  { 0.62f, 0.5f, 0.4f },   0.35f, 5, "Harpy Queen" },
    { "Hound",      0.35f, 0.8f, 28,  8,  6.0f, 0.9f, 0.8f, 13, false, { 0.42f, 0.36f, 0.3f },  0.35f, 3, "Pack Alpha" },
    { "Troll",      0.5f,  2.1f, 160, 18, 2.4f, 1.5f, 3.0f, 40, false, { 0.42f, 0.56f, 0.4f },  0.6f,  7, "Troll King" },
};

/* family weights per stratum:
 *   crawler brute wisp horned skeleton slime bat | spider wraith golem shaman bloater eye knight worm mimic necro harpy hound troll
 * (mimics are placed on their own, not in packs) */
static const float FAMILY[ST_COUNT][MON_TYPE_COUNT] = {
    [ST_CORE] =     { 2, 2, 3, 2, 1, 2, 1,  1, 2, 3, 1, 1, 3, 1, 2, 0, 1, 0, 1, 2 },
    [ST_MAW] =      { 4, 3, 1, 2, 0, 4, 1,  2, 0, 1, 1, 4, 1, 0, 4, 0, 0, 0, 2, 2 },
    [ST_IRONHELL] = { 1, 5, 1, 4, 3, 0, 1,  0, 1, 4, 1, 1, 1, 5, 0, 0, 1, 0, 2, 2 },
    [ST_INFERNO] =  { 3, 2, 2, 3, 2, 1, 2,  1, 1, 2, 2, 3, 1, 1, 1, 0, 1, 1, 3, 2 },
    [ST_CRYSTAL] =  { 2, 1, 4, 1, 1, 3, 2,  2, 2, 4, 1, 0, 4, 0, 2, 0, 0, 1, 0, 1 },
    [ST_LIMBO] =    { 3, 1, 3, 1, 3, 1, 2,  1, 5, 0, 1, 1, 1, 1, 0, 0, 3, 0, 1, 0 },
    [ST_FUNGAL] =   { 4, 1, 1, 1, 0, 5, 2,  3, 0, 0, 3, 4, 1, 0, 3, 0, 0, 0, 1, 1 },
    [ST_OSSUARY] =  { 2, 1, 2, 1, 6, 1, 2,  2, 4, 1, 1, 1, 1, 2, 0, 0, 4, 0, 1, 0 },
    [ST_ROOTS] =    { 4, 1, 2, 1, 1, 3, 2,  4, 0, 1, 2, 1, 0, 0, 4, 0, 0, 0, 2, 3 },
    [ST_MINES] =    { 3, 2, 1, 3, 2, 2, 3,  3, 0, 2, 1, 2, 0, 1, 3, 0, 0, 0, 1, 3 },
    [ST_DUNGEON] =  { 4, 2, 2, 1, 3, 2, 2,  2, 1, 0, 1, 1, 1, 2, 1, 0, 1, 0, 2, 1 },
    [ST_CASTLE] =   { 2, 3, 2, 2, 3, 1, 2,  1, 1, 1, 2, 0, 1, 5, 0, 0, 1, 1, 3, 1 },
    [ST_SPIRES] =   { 1, 2, 4, 2, 2, 1, 3,  1, 2, 1, 2, 0, 2, 2, 0, 0, 1, 4, 1, 0 },
    [ST_AERIE] =    { 1, 1, 2, 2, 1, 0, 6,  0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 6, 1, 1 },
    [ST_CLOUDS] =   { 1, 1, 5, 1, 1, 1, 3,  0, 3, 0, 2, 0, 3, 1, 0, 0, 0, 4, 0, 0 },
    [ST_GARDENS] =  { 3, 1, 3, 1, 0, 3, 2,  3, 0, 1, 3, 2, 1, 0, 2, 0, 0, 2, 3, 2 },
    [ST_HEAVEN] =   { 1, 2, 4, 1, 1, 1, 2,  0, 2, 3, 3, 0, 3, 3, 0, 0, 0, 2, 1, 0 },
    [ST_SUNCOURT] = { 1, 3, 4, 3, 1, 0, 1,  0, 1, 4, 3, 0, 3, 4, 0, 0, 0, 2, 2, 0 },
    [ST_STORM] =    { 0, 2, 6, 1, 1, 0, 4,  0, 2, 2, 1, 0, 4, 1, 0, 0, 0, 5, 0, 0 },
    [ST_ORRERY] =   { 1, 2, 5, 2, 2, 1, 1,  0, 2, 4, 2, 0, 5, 2, 0, 0, 1, 1, 0, 0 },
    [ST_ASTRAL] =   { 1, 1, 5, 2, 1, 2, 2,  1, 3, 2, 1, 1, 5, 0, 0, 0, 2, 1, 0, 0 },
};

/* ---------- spawned-chunk set ---------- */

u64 chunk_spawn_key(int cx, int cy, int cz) {
    return (((u64)(u32)cx & 0x1FFFFF) << 42) | (((u64)(u32)cy & 0x1FFFFF) << 21) | ((u64)(u32)cz & 0x1FFFFF) | (1ULL << 63);
}

#define TOMBSTONE 1ULL

static bool spawned_has(Game *g, u64 k) {
    if (!g->spawned_cap) return false;
    for (int i = (int)(mix64(k) & (u64)(g->spawned_cap - 1));; i = (i + 1) & (g->spawned_cap - 1)) {
        if (g->spawned[i] == 0) return false;
        if (g->spawned[i] == k) return true;
    }
}

static void spawned_remove(Game *g, u64 k) {
    if (!g->spawned_cap) return;
    for (int i = (int)(mix64(k) & (u64)(g->spawned_cap - 1));; i = (i + 1) & (g->spawned_cap - 1)) {
        if (g->spawned[i] == 0) return;
        if (g->spawned[i] == k) { g->spawned[i] = TOMBSTONE; return; }
    }
}

static void spawned_add(Game *g, u64 k) {
    if ((g->spawned_count + 1) * 2 > g->spawned_cap) {
        int old_cap = g->spawned_cap;
        u64 *old = g->spawned;
        g->spawned_cap = old_cap ? old_cap * 2 : 1024;
        g->spawned = calloc((size_t)g->spawned_cap, sizeof(u64));
        g->spawned_count = 0;
        for (int i = 0; i < old_cap; i++)
            if (old[i] > TOMBSTONE) spawned_add(g, old[i]);
        free(old);
    }
    for (int i = (int)(mix64(k) & (u64)(g->spawned_cap - 1));; i = (i + 1) & (g->spawned_cap - 1)) {
        if (g->spawned[i] == k) return;
        if (g->spawned[i] <= TOMBSTONE) {
            g->spawned[i] = k;
            g->spawned_count++;
            return;
        }
    }
}

void chunk_forget(Game *g, u64 chunk_key) { spawned_remove(g, chunk_key); }

/* ---------- spawning ---------- */

int region_element(int region) {
    static const int E[] = { [REG_NONE] = EL_NONE, [REG_NORTH] = EL_FROST, [REG_SOUTH] = EL_FIRE, [REG_EAST] = EL_LIGHTNING,
                             [REG_WEST] = EL_POISON, [REG_UP] = EL_HOLY, [REG_DOWN] = EL_SHADOW };
    return region >= 0 && region <= REG_DOWN ? E[region] : EL_NONE;
}

static vec3 colv(Col c) { return v3(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f); }

/* Colours from the place it lives: its body takes some of the local rock, armour and bone
 * the local metal and accent, eyes and markings the region's element or the torch light. */
static void monster_palette(Game *g, Monster *m, vec3 pos, vec3 tint) {
    const MonsterDef *d = &MONSTERS[m->type];
    int cx = floordiv((int)floorf(pos.x), CHUNK), cy = floordiv((int)floorf(pos.y), CHUNK), cz = floordiv((int)floorf(pos.z), CHUNK);
    ChunkInfo ci;
    Chunk *ch = chunkmap_get(&g->map, cx, cy, cz);
    if (ch && ch->state == CH_READY) ci = ch->info;
    else world_chunk_info(&g->world, cx, cy, cz, &ci);
    ThemeInfo ti;
    theme_info(ci.theme, &ti);
    vec3 rock = colv(ti.mat[B_ROCK]), metal = colv(ti.mat[B_METAL]), accent = colv(ti.mat[B_ACCENT]), wood = colv(ti.mat[B_WOOD]);
    vec3 torch = v3(ti.light[LIGHT_TORCH][0] / 15.0f, ti.light[LIGHT_TORCH][1] / 15.0f, ti.light[LIGHT_TORCH][2] / 15.0f);
    m->color = v3_lerp(v3_lerp(d->color, v3_scale(rock, 1.4f), 0.3f), tint, 0.12f);
    m->color2 = v3_lerp(v3_scale(metal, 1.2f), accent, 0.35f);
    if (m->type == MON_MIMIC) { m->color = v3_scale(wood, 1.15f); m->color2 = v3_lerp(metal, accent, 0.6f); }
    if (m->type == MON_SKELETON || m->type == MON_NECRO) m->color2 = v3_lerp(v3(0.85f, 0.82f, 0.7f), accent, 0.2f); /* bone */
    m->glow = m->element != EL_NONE ? element_color(m->element) : v3_lerp(torch, v3(1.0f, 0.85f, 0.4f), 0.4f);
    if (m->element != EL_NONE) m->color = v3_lerp(m->color, element_color(m->element), fminf(0.6f, 0.12f + 0.05f * m->rank));
}

void monster_make_boss(Game *g, Monster *m) {
    const MonsterDef *d = &MONSTERS[m->type];
    bool was_elite = m->elite;
    m->grade = GRADE_BOSS;
    m->elite = true;
    m->maxhp = m->hp = m->maxhp / (was_elite ? 3.0f : 1.0f) * 10.0f;
    m->dmg = m->dmg / (was_elite ? 1.5f : 1.0f) * 1.8f;
    m->xp *= 3.0f;
    m->scale = 1.9f * (1.0f + 0.02f * m->rank);
    snprintf(m->name, sizeof m->name, "%s", d->boss);
    for (char *c = m->name; *c; c++) *c = (char)toupper((unsigned char)*c);
    /* bosses are darker and their markings burn brighter */
    m->color = v3_scale(m->color, 0.8f);
    m->glow = v3_scale(m->glow, 1.3f);
    (void)g;
}

Monster *monster_spawn(Game *g, int type, vec3 pos, int level, bool elite, vec3 tint, u64 key) {
    Monster *m = NULL;
    for (int i = 0; i < MAX_MONSTERS && !m; i++)
        if (!g->mons[i].alive) m = &g->mons[i];
    if (!m) return NULL;
    const MonsterDef *d = &MONSTERS[type];
    memset(m, 0, sizeof *m);
    m->alive = true;
    m->type = (u8)type;
    m->elite = elite;
    m->pos = pos;
    m->scale = elite ? 1.35f : 1.0f;
    /* more players, tougher monsters (but not proportionally: slaying should stay quick) */
    int np = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) np += g->players[i].active;
    float party = 1.0f + 0.5f * (float)MAXI(0, np - 1);
    m->maxhp = m->hp = d->hp * (1.0f + 0.18f * level) * (elite ? 3.0f : 1.0f) * party;
    m->dmg = d->dmg * (1.0f + 0.14f * level) * (elite ? 1.6f : 1.0f);
    m->xp = d->xp * (1.0f + 0.12f * level) * (elite ? 4.0f : 1.0f);
    m->grade = elite ? GRADE_ELITE : GRADE_NORMAL;
    m->state = MS_IDLE;
    m->chunk_key = key;
    m->target = 0;
    m->last_hitter = -1;
    m->level = (float)level;
    /* the region shapes it: every 10-chunk step out a little tougher and bigger; from the
     * second step it takes the region's element (colour, attacks, resistance) */
    int tier, region = world_region(&g->world, floordiv((int)floorf(pos.x), CHUNK), floordiv((int)floorf(pos.y), CHUNK),
                                    floordiv((int)floorf(pos.z), CHUNK), &tier);
    m->rank = (u8)tier;
    m->element = (u8)(tier >= 1 ? region_element(region) : EL_NONE);
    m->maxhp = m->hp = m->maxhp * (1.0f + 0.06f * tier);
    m->dmg *= 1.0f + 0.08f * tier;
    m->xp *= 1.0f + 0.05f * tier;
    m->scale *= 1.0f + 0.02f * tier;
    monster_palette(g, m, pos, tint);
    m->yaw = (float)((key + (u64)g->mons_anim_seed * 97) % 628) * 0.01f;
    m->anim = (float)((key + (u64)g->mons_anim_seed * 31) % 100) * 0.1f;
    return m;
}

static bool standable_cell(Game *g, int x, int y, int z, int headroom) {
    if (game_solid(g, x, y - 1, z) != 1) return false;
    for (int k = 0; k < headroom; k++)
        if (game_solid(g, x, y + k, z) != 0) return false;
    return true;
}

/* A free spot in the chunk, away from the players, with this much headroom. */
static bool monster_spot(Game *g, Chunk *c, Rng *r, int head, bool flies, vec3 *out) {
    for (int attempt = 0; attempt < 40; attempt++) {
        int wx = c->cx * CHUNK + rng_range(r, 1, 30), wz = c->cz * CHUNK + rng_range(r, 1, 30);
        for (int y = 30; y >= 1; y--)
            if (standable_cell(g, wx, c->cy * CHUNK + y, wz, head)) {
                vec3 p = v3(wx + 0.5f, (float)(c->cy * CHUNK + y) + (flies ? 1.0f : 0.0f), wz + 0.5f);
                bool near = false;
                for (int k = 0; k < MAX_PLAYERS; k++)
                    if (g->players[k].active && v3_len(v3_sub(p, player_eye(&g->players[k].pl))) < 12.0f) near = true;
                if (near) break;
                *out = p;
                return true;
            }
    }
    return false;
}

static void spawn_chunk(Game *g, Chunk *c) {
    const ChunkInfo *ci = &c->info;
    u64 key = chunk_spawn_key(c->cx, c->cy, c->cz);
    spawned_add(g, key);
    /* shrines */
    for (int i = 0; i < CHUNK_VOL && g->nshrines < MAX_SHRINES; i++)
        if (c->blocks[i] == B_SHRINE) {
            int x = i % CHUNK, z = (i / CHUNK) % CHUNK, y = i / (CHUNK * CHUNK);
            vec3 sp = v3(c->cx * CHUNK + x + 0.5f, c->cy * CHUNK + y + 0.5f, c->cz * CHUNK + z + 0.5f);
            bool known = false;
            for (int k = 0; k < g->nshrines && !known; k++) known = v3_len(v3_sub(g->shrines[k], sp)) < 0.5f;
            if (!known) g->shrines[g->nshrines++] = sp;
        }
    if (ci->arch == ARCH_EXTERIOR || ci->arch == ARCH_POCKET) return; /* nothing hostile outside or at home */

    loot_place_in_chunk(g, c);
    story_spawn_lord(g, c);
    Rng r = rng_make(ci->seed ^ 0x5EED);
    int level = ci->level;
    /* most chunks hold a pack, structures two; packs grow with danger */
    int n = 0;
    if (level <= 1) n = rng_range(&r, 2, 3); /* something to fight within a minute of the gate */
    else if (rng_chance(&r, ci->has_struct ? 1.0f : 0.8f)) n = MINI(10, 3 + (int)(level * 0.1f + rng_float(&r) * 3.0f));
    if (level > 1 && ci->has_struct && rng_chance(&r, 0.6f)) n += 3 + (int)(rng_float(&r) * 3.0f);
    /* the stratum's mix, without the kinds not met at this danger yet */
    float w[MON_TYPE_COUNT];
    float sum = 0;
    for (int t = 0; t < MON_TYPE_COUNT; t++) {
        w[t] = level >= MONSTERS[t].min_level ? FAMILY[ci->stratum][t] : 0.0f;
        sum += w[t];
    }
    if (sum <= 0) w[MON_CRAWLER] = 1;
    int main_type = level <= 1 ? MON_CRAWLER : pick_weighted(rng_float(&r), w, MON_TYPE_COUNT);
    int tier;
    world_region(&g->world, c->cx, c->cy, c->cz, &tier);
    if (main_type == MON_BAT) n += 3; /* bats come in swarms */
    ThemeInfo ti;
    theme_info(ci->theme, &ti);
    vec3 tint = v3(ti.mat[B_ACCENT].r / 255.0f, ti.mat[B_ACCENT].g / 255.0f, ti.mat[B_ACCENT].b / 255.0f);
    /* half the melee packs bring someone to shoot or heal from behind */
    int support = -1;
    bool melee_pack = main_type != MON_SKELETON && main_type != MON_WISP && main_type != MON_SHAMAN && main_type != MON_MIMIC;
    if (level >= 2 && n >= 2 && melee_pack && rng_chance(&r, 0.5f)) {
        static const int SUP[3] = { MON_SKELETON, MON_WISP, MON_SHAMAN };
        int k = rng_range(&r, 0, 2);
        if (level < MONSTERS[SUP[k]].min_level) k = rng_range(&r, 0, 1);
        if (level >= MONSTERS[SUP[k]].min_level) support = SUP[k];
    }
    for (int i = 0; i < n; i++) {
        int type = (level > 1 && rng_chance(&r, 0.3f)) ? pick_weighted(rng_float(&r), w, MON_TYPE_COUNT) : main_type;
        if (i == n - 1 && support >= 0) type = support;
        bool elite = level > 2 && rng_chance(&r, 0.04f + 0.004f * level + 0.015f * tier);
        int head = (int)ceilf(MONSTERS[type].h * (elite ? 1.35f : 1.0f)) + (MONSTERS[type].flies ? 1 : 0);
        for (int attempt = 0; attempt < 40; attempt++) {
            int x = rng_range(&r, 1, 30), z = rng_range(&r, 1, 30);
            int wx = c->cx * CHUNK + x, wz = c->cz * CHUNK + z;
            /* (world y is negative below the gate's layer: no -1 for "none") */
            int found = 0;
            bool any = false;
            for (int y = 30; y >= 1 && !any; y--)
                if (standable_cell(g, wx, c->cy * CHUNK + y, wz, head)) { found = c->cy * CHUNK + y; any = true; }
            if (!any) continue;
            vec3 p = v3(wx + 0.5f, (float)found + (MONSTERS[type].flies ? 1.0f : 0.0f), wz + 0.5f);
            bool near_player = false;
            for (int k = 0; k < MAX_PLAYERS; k++)
                if (g->players[k].active && v3_len(v3_sub(p, player_eye(&g->players[k].pl))) < 12.0f) near_player = true;
            if (near_player) continue;
            monster_spawn(g, type, p, level, elite, tint, key);
            g->mons_anim_seed++;
            break;
        }
    }
    /* a chest that is not one */
    if (level >= MONSTERS[MON_MIMIC].min_level && rng_chance(&r, 0.1f)) {
        vec3 p;
        if (monster_spot(g, c, &r, 2, false, &p)) {
            Monster *m = monster_spawn(g, MON_MIMIC, p, level, false, tint, key);
            if (m) m->yaw = (float)rng_range(&r, 0, 3) * PI_F * 0.5f;
        }
    }
    /* now and then a boss holds a great hall or cavern, more often farther out */
    if (level > 3 && ci->has_struct && n > 0 && main_type != MON_MIMIC && rng_chance(&r, 0.1f + 0.015f * tier)) {
        vec3 p;
        int head = (int)ceilf(MONSTERS[main_type].h * 1.9f * 1.2f) + (MONSTERS[main_type].flies ? 1 : 0);
        if (monster_spot(g, c, &r, head, MONSTERS[main_type].flies, &p)) {
            Monster *m = monster_spawn(g, main_type, p, level, true, tint, key);
            if (m) monster_make_boss(g, m);
        }
    }
}

#define SPAWN_RADIUS_H 2
#define SPAWN_RADIUS_V 1
#define DESPAWN_DIST 110.0f

/* Distance from p to the nearest active player's eye. */
static float player_dist(Game *g, vec3 p) {
    float best = 1e9f;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) best = fminf(best, v3_len(v3_sub(p, player_eye(&g->players[i].pl))));
    return best;
}

void monsters_spawn_for_chunks(Game *g) {
    ChunkMap *m = &g->map;
    /* monsters far from everyone go back to sleep in their chunk: it will be repopulated on return */
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *mo = &g->mons[i];
        if (!mo->alive || player_dist(g, mo->pos) < DESPAWN_DIST) continue;
        mo->alive = false;
        spawned_remove(g, mo->chunk_key);
    }
    for (int i = 0; i < m->pool_cap; i++) {
        Chunk *c = &m->pool[i];
        if (!c->used || c->state != CH_READY || !c->has_mesh) continue;
        vec3 centre = v3(c->cx * CHUNK + 16.0f, c->cy * CHUNK + 16.0f, c->cz * CHUNK + 16.0f);
        bool in_range = false, too_close = false;
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *ps = &g->players[k];
            if (!ps->active) continue;
            vec3 eye = player_eye(&ps->pl);
            int pcx = floordiv((int)floorf(eye.x), CHUNK), pcy = floordiv((int)floorf(eye.y), CHUNK), pcz = floordiv((int)floorf(eye.z), CHUNK);
            float d = v3_len(v3_sub(centre, eye));
            if (abs(c->cx - pcx) <= SPAWN_RADIUS_H && abs(c->cz - pcz) <= SPAWN_RADIUS_H && abs(c->cy - pcy) <= SPAWN_RADIUS_V && d <= 70.0f)
                in_range = true;
            /* never spawn into the chunk a player is standing in */
            if (d < 20.0f && c->info.arch != ARCH_EXTERIOR) too_close = true;
        }
        if (!in_range || too_close) continue;
        u64 key = chunk_spawn_key(c->cx, c->cy, c->cz);
        if (spawned_has(g, key)) continue;
        spawn_chunk(g, c);
    }
}

/* ---------- flow field ---------- */

static inline int fidx(int x, int y, int z) { return (y * FLOW_W + z) * FLOW_W + x; }
#define SY0 4 /* solid cache has 4 extra layers below and above */
static inline bool fsolid(const PlayerSlot *g, int x, int y, int z) {
    if ((unsigned)x >= FLOW_W || (unsigned)z >= FLOW_W || y < -SY0 || y >= FLOW_Y + SY0) return true;
    return g->flow_solid[((y + SY0) * FLOW_W + z) * FLOW_W + x];
}
static inline bool fstand(const PlayerSlot *g, int x, int y, int z) { return !fsolid(g, x, y, z) && fsolid(g, x, y - 1, z); }

void flow_update(Game *game, PlayerSlot *g, float dt) {
    if (!g->flow) {
        g->flow = malloc(sizeof(u16) * FLOW_W * FLOW_W * FLOW_Y);
        g->flow_solid = malloc((size_t)FLOW_W * FLOW_W * (FLOW_Y + 2 * SY0));
    }
    int px = (int)floorf(g->pl.pos.x), py = (int)floorf(g->pl.pos.y + 0.01f), pz = (int)floorf(g->pl.pos.z);
    g->flow_timer -= dt;
    if (g->flow_timer > 0 && px == g->flow_px && py == g->flow_py && pz == g->flow_pz) return;
    g->flow_timer = 0.35f;
    g->flow_px = px; g->flow_py = py; g->flow_pz = pz;
    g->flow_ox = px - FLOW_R;
    g->flow_oy = py - FLOW_H;
    g->flow_oz = pz - FLOW_R;
    for (int y = -SY0; y < FLOW_Y + SY0; y++)
        for (int z = 0; z < FLOW_W; z++)
            for (int x = 0; x < FLOW_W; x++)
                g->flow_solid[((y + SY0) * FLOW_W + z) * FLOW_W + x] =
                    game_solid(game, g->flow_ox + x, g->flow_oy + y, g->flow_oz + z) != 0;
    memset(g->flow, 0xFF, sizeof(u16) * FLOW_W * FLOW_W * FLOW_Y);
    int sx = FLOW_R, sy = FLOW_H, sz = FLOW_R;
    for (int k = 0; k < 4 && !fstand(g, sx, sy, sz); k++) sy--;
    if (!fstand(g, sx, sy, sz)) return;
    static int queue[FLOW_W * FLOW_W * FLOW_Y];
    int head = 0, tail = 0;
    g->flow[fidx(sx, sy, sz)] = 0;
    queue[tail++] = fidx(sx, sy, sz);
    static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
    while (head < tail) {
        int u = queue[head++];
        int ux = u % FLOW_W, uz = (u / FLOW_W) % FLOW_W, uy = u / (FLOW_W * FLOW_W);
        u16 du = g->flow[u];
        for (int d = 0; d < 4; d++) {
            int vx = ux + DX[d], vz = uz + DZ[d];
            if ((unsigned)vx >= FLOW_W || (unsigned)vz >= FLOW_W) continue;
            /* cells v from which a monster can move into u */
            int cand[5], nc = 0;
            if (fstand(g, vx, uy, vz)) cand[nc++] = uy;
            if (fstand(g, vx, uy - 1, vz) && !fsolid(g, vx, uy, vz)) cand[nc++] = uy - 1;
            for (int k = 1; k <= 3; k++) {
                if (fsolid(g, ux, uy + k, uz)) break;
                if (fstand(g, vx, uy + k, vz)) { cand[nc++] = uy + k; break; }
            }
            for (int c = 0; c < nc; c++) {
                int vy = cand[c];
                if (vy < 0 || vy >= FLOW_Y) continue;
                int v = fidx(vx, vy, vz);
                if (g->flow[v] != 0xFFFF) continue;
                g->flow[v] = du + 1;
                queue[tail++] = v;
            }
        }
    }
}

/* Next cell centre on the way to the player, or false if the field does not cover it. */
static bool flow_next(const PlayerSlot *g, vec3 pos, vec3 *target) {
    if (!g->flow || g->flow_timer <= -1.0f) return false;
    int x = (int)floorf(pos.x) - g->flow_ox, y = (int)floorf(pos.y + 0.01f) - g->flow_oy, z = (int)floorf(pos.z) - g->flow_oz;
    if ((unsigned)x >= FLOW_W || (unsigned)z >= FLOW_W || y < 0 || y >= FLOW_Y) return false;
    if (g->flow[fidx(x, y, z)] == 0xFFFF && y > 0 && g->flow[fidx(x, y - 1, z)] != 0xFFFF) y--;
    u16 here = g->flow[fidx(x, y, z)];
    if (here == 0xFFFF || here == 0) return false;
    static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
    u16 best = here;
    int bx = -1, by = 0, bz = 0;
    for (int d = 0; d < 4; d++) {
        int nx = x + DX[d], nz = z + DZ[d];
        if ((unsigned)nx >= FLOW_W || (unsigned)nz >= FLOW_W) continue;
        int cand[5], nc = 0;
        if (fstand(g, nx, y, nz)) cand[nc++] = y;
        if (fstand(g, nx, y + 1, nz) && !fsolid(g, x, y + 1, z)) cand[nc++] = y + 1;
        for (int k = 1; k <= 3; k++) {
            if (fsolid(g, nx, y - k + 1, nz)) break;
            if (fstand(g, nx, y - k, nz)) { cand[nc++] = y - k; break; }
        }
        for (int c = 0; c < nc; c++) {
            int ny = cand[c];
            if (ny < 0 || ny >= FLOW_Y) continue;
            u16 dn = g->flow[fidx(nx, ny, nz)];
            if (dn < best) { best = dn; bx = nx; by = ny; bz = nz; }
        }
    }
    if (bx < 0) return false;
    *target = v3(g->flow_ox + bx + 0.5f, (float)(g->flow_oy + by), g->flow_oz + bz + 0.5f);
    return true;
}

/* ---------- behaviour ---------- */

Monster *monster_nearest(Game *g, vec3 p, float max_dist, const Monster *exclude) {
    Monster *best = NULL;
    float bd = max_dist;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || m == exclude) continue;
        float d = v3_len(v3_sub(v3_add(m->pos, v3(0, MONSTERS[m->type].h * 0.5f, 0)), p));
        if (d < bd) { bd = d; best = m; }
    }
    return best;
}

static vec3 monster_centre(const Monster *m) {
    return v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
}

static void wake_neighbours(Game *g, Monster *m) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *o = &g->mons[i];
        if (o->alive && !o->awake && o->type != MON_MIMIC && v3_len(v3_sub(o->pos, m->pos)) < 10.0f) {
            sfx_monster(g, o, MSFX_ALERT);
            o->awake = true;
            o->state = MS_CHASE;
        }
    }
}

static void bloater_burst(Game *g, Monster *m, float mult);

void monster_damage(Game *g, Monster *m, float dmg, vec3 from, float knock, int attacker) {
    if (!m->alive || m->hidden) return; /* vanished or underground */
    if (m->rank >= 4) dmg *= 0.85f; /* armoured, far out */
    if (attacker >= 0) g_tele.dealt += fminf(dmg, m->hp);
    if (m->type == MON_KNIGHT && m->state != MS_STUN && !(g_hit_flags & WBF_SHIELDBREAK)) {
        /* the shield takes blows from the front: go round it */
        vec3 fw = v3(cosf(m->yaw), 0, sinf(m->yaw)), to = v3_sub(from, m->pos);
        to.y = 0;
        float facing = v3_len(to) > 0.05f ? v3_dot(fw, v3_norm(to)) : 0.0f;
        if (facing < -0.45f) dmg *= 2.0f; /* its back is bare: going round pays */
        if (facing > 0.45f) {
            dmg *= 0.25f;
            vec3 sh = v3_add(monster_centre(m), v3_scale(fw, MONSTERS[m->type].hw * m->scale + 0.1f));
            particles_burst(g, sh, v3(1.0f, 0.85f, 0.5f), 6, 3.0f, 0.03f, true);
            sfx_at(g, SFX_KNIGHT_BLOCK, sh, 1.0f);
        }
    }
    m->hp -= dmg;
    m->flash = 0.15f;
    if (attacker >= 0 && attacker < MAX_PLAYERS && g->players[attacker].active) {
        m->last_hitter = attacker;
        if (!g->players[attacker].st.dead) m->target = attacker; /* turn on whoever hurts you */
    }
    m->sulk_t = 0;
    if (!m->awake) { sfx_monster(g, m, MSFX_ALERT); m->awake = true; m->state = MS_CHASE; wake_neighbours(g, m); }
    vec3 dir = v3_sub(m->pos, from);
    dir.y = 0;
    dir = v3_norm(dir);
    float resist = m->type == MON_BRUTE ? 0.3f : 1.0f;
    m->vel = v3_add(m->vel, v3_scale(dir, knock * resist / m->scale));
    if (m->type != MON_WISP && knock > 0) m->vel.y += 2.0f * resist;
    vec3 c = monster_centre(m);
    particles_burst(g, c, v3_scale(m->color, 0.5f), 6, 3.0f, 0.06f, false);
    if (m->hp > 0) sfx_monster(g, m, MSFX_HURT);
    if (m->hp <= 0) {
        m->alive = false;
        sfx_monster(g, m, MSFX_DEATH);
        if (m->named_kind) sfx_at(g, SFX_NAMED_DEATH, c, 1.0f);
        particles_burst(g, c, m->color, 18, 5.0f, 0.12f, false);
        particles_burst(g, c, v3_scale(m->color, 0.4f), 10, 4.0f, 0.08f, false);
        if (m->elite) flash_add(g, c, v3(1.0f, 0.6f, 0.3f), 8.0f, 0.4f);
        loot_drop(g, m->pos, m->level, m->elite, m->named_kind ? 0.5f : 0.0f);
        if (m->story_key) story_on_kill(g, m);
        if (m->named_kind && m->story_key && m->level >= 6.0f) {
            /* a named enemy carries one of this world's legendaries (each is found once) */
            int id = (int)(m->story_key % UNIQUE_COUNT);
            u64 key = mix64(g->world.seed ^ 0x11E6ULL ^ (u64)id * 0x9E3779B9ULL) | 1;
            if (!looted_has(g, key)) {
                bool lying = false;
                for (int k = 0; k < MAX_PICKUPS; k++) lying |= g->pickups[k].alive && g->pickups[k].key == key;
                if (!lying) {
                    Item it = arms_unique(id, m->level + 3.0f);
                    pickup_spawn(g, PK_WEAPON, v3_add(m->pos, v3(0, 0.6f, 0)), 0, &it, key, 0);
                    char buf[128];
                    snprintf(buf, sizeof buf, "IT DROPS %s, A LEGENDARY OF THIS WORLD", arms_unique_name(id));
                    for (char *q = buf; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
                    game_banner_all(g, buf);
                    flash_add(g, c, v3(0.6f, 1.0f, 0.8f), 12.0f, 0.8f);
                }
            }
        }
        if (m->type == MON_BLOATER && m->aux != 2) { m->aux = 2; bloater_burst(g, m, 0.6f); }
        if (m->grade == GRADE_BOSS) {
            /* a boss always leaves a good weapon */
            int tier, region = world_region(&g->world, floordiv((int)floorf(m->pos.x), CHUNK), floordiv((int)floorf(m->pos.y), CHUNK),
                                            floordiv((int)floorf(m->pos.z), CHUNK), &tier);
            u64 seed = mix64((u64)(i64)(m->pos.x * 977.0f) ^ ((u64)(i64)(m->pos.z * 613.0f) << 20) ^ m->chunk_key ^ 0xB055ULL);
            Item it = item_roll_region(seed, m->level + 2.0f, RAR_RARE, tier, region);
            pickup_spawn(g, PK_WEAPON, v3_add(m->pos, v3(0, 0.8f, 0)), 0, &it, 0, 0);
            loot_drop(g, m->pos, m->level + 2.0f, true, 0.5f);
            flash_add(g, c, v3_scale(m->glow, 1.5f), 14.0f, 0.8f);
            particles_burst(g, c, m->glow, 30, 6.0f, 0.1f, true);
            if (!m->named_kind) sfx_at(g, SFX_NAMED_DEATH, c, 1.0f);
        }
        if (m->type == MON_SLIME && m->scale > 0.75f) {
            /* a big slime splits in two small ones */
            sfx_at(g, SFX_SLIME_SPLIT, c, 1.0f);
            for (int k = 0; k < 2; k++) {
                vec3 p = v3_add(m->pos, v3(k ? 0.4f : -0.4f, 0.1f, 0));
                monster_spawn(g, MON_SLIME, p, (int)m->level, false, m->color, m->chunk_key);
                for (int j = MAX_MONSTERS - 1; j >= 0; j--) {
                    Monster *o = &g->mons[j];
                    if (o->alive && o->type == MON_SLIME && v3_len(v3_sub(o->pos, p)) < 0.01f) {
                        o->scale = 0.6f;
                        o->maxhp = o->hp = m->maxhp * 0.35f;
                        o->xp = m->xp * 0.3f;
                        o->awake = true;
                        o->state = MS_CHASE;
                        o->vel = v3(k ? 3.0f : -3.0f, 4.0f, 0);
                        break;
                    }
                }
            }
        }
        /* the killer gets the full XP, everyone else nearby half of it */
        int killer = m->last_hitter;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            PlayerSlot *ps = &g->players[i];
            if (!ps->active) continue;
            /* one kill is never worth more than 30% of a level (no jumps of several levels
             * from a pack far above you) */
            float cap = ps->st.xp_next * 0.3f;
            if (i == killer) {
                ps->st.kills++;
                g_tele.kills[m->type & 31][m->grade % 3]++;
                game_add_xp(g, ps, fminf(m->xp, cap));
            } else if (v3_len(v3_sub(ps->pl.pos, m->pos)) < 30.0f) {
                game_add_xp(g, ps, fminf(m->xp * 0.5f, cap));
            }
        }
    }
}

/* A monster's blow lands: the damage, then its element (shadow ones feed on it). */
static void monster_strike(Game *g, Monster *m, PlayerSlot *ps, float dmg, vec3 from) {
    if (m->alt_kind && utility_monster_strike(g, m, dmg)) return; /* a decoy, a lure, or a charmed foe's enemy */
    bool was_dead = ps->st.dead;
    tele_src(HS_MELEE, m->type, m->grade, m->rank);
    game_hurt_player(g, ps, dmg, from);
    if (m->element == EL_NONE || was_dead) return;
    player_element_hit(g, ps, m->element, dmg * (m->rank < 3 ? 0.5f : 1.0f)); /* faint in the first regions */
    if (m->element == EL_SHADOW) m->hp = fminf(m->maxhp, m->hp + dmg * 0.5f);
}

static void enemy_fire(Game *g, Monster *m, vec3 target) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (p->alive) continue;
        memset(p, 0, sizeof *p);
        p->alive = true;
        bool arrow = m->type == MON_SKELETON;
        p->kind = arrow ? PJ_ENEMY_ARROW : PJ_ENEMY_FIRE;
        p->element = m->element;
        p->owner = -1;
        if (m->charm_t > 0 && m->charmer >= 0) { /* charmed: it shoots for its friend */
            p->kind = PJ_BOLT;
            p->owner = m->charmer;
        }
        p->pos = v3_add(monster_centre(m), v3(0, arrow ? 0.4f : 0.0f, 0));
        vec3 dir = v3_norm(v3_sub(target, p->pos));
        if (arrow) { /* about 2 degrees of spread: a target on the move is often missed */
            u64 h = mix64((u64)(g->time * 1000.0f) ^ ((u64)(m - g->mons) << 20) ^ (u64)i);
            float a = ((float)(h & 0xFFFF) / 65536.0f - 0.5f) * 0.07f, b = ((float)((h >> 16) & 0xFFFF) / 65536.0f - 0.5f) * 0.05f;
            dir = v3_norm(v3(dir.x * cosf(a) - dir.z * sinf(a), dir.y + b, dir.x * sinf(a) + dir.z * cosf(a)));
        }
        p->vel = v3_scale(dir, arrow ? 20.0f : 11.0f);
        p->dmg = m->dmg;
        p->life = 4.0f;
        p->last_hit = p->owner >= 0 ? (int)(m - g->mons) : -1;
        return;
    }
}

/* Monsters also stop at a Warden's Keep's gate (utility.c). */
static int monster_solid(void *user, int x, int y, int z) {
    Game *g = user;
    int s = game_solid(g, x, y, z);
    return s == 0 && g->nbarriers && utility_barrier(g, x, y, z) ? 1 : s;
}

static void monster_physics(Game *g, Monster *m, vec3 want, float dt) {
    const MonsterDef *d = &MONSTERS[m->type];
    float hw = d->hw * m->scale, h = d->h * m->scale;
    if (d->flies) {
        m->vel = v3_lerp(m->vel, want, fminf(1.0f, 4.0f * dt));
        for (int a = 0; a < 3; a++) {
            float v = a == 0 ? m->vel.x : (a == 1 ? m->vel.y : m->vel.z);
            if (!phys_move_axis(&m->pos, a, v * dt, hw, h, monster_solid, g)) {
                if (a == 0) m->vel.x = 0; else if (a == 1) m->vel.y = 0; else m->vel.z = 0;
            }
        }
        return;
    }
    float accel = m->on_ground ? 10.0f : 2.0f;
    m->vel.x += (want.x - m->vel.x) * fminf(1.0f, accel * dt);
    m->vel.z += (want.z - m->vel.z) * fminf(1.0f, accel * dt);
    m->vel.y -= 25.0f * dt;
    if (m->vel.y < -30.0f) m->vel.y = -30.0f;
    bool blocked = false;
    for (int a = 0; a < 3; a += 2) {
        float v = a == 0 ? m->vel.x : m->vel.z;
        vec3 before = m->pos;
        if (phys_move_axis(&m->pos, a, v * dt, hw, h, monster_solid, g)) continue;
        vec3 up = v3_add(before, v3(0, 1.01f, 0));
        if (m->on_ground && phys_box_free(up, hw, h, monster_solid, g)) {
            m->pos = up;
            phys_move_axis(&m->pos, a, v * dt, hw, h, monster_solid, g);
        } else {
            blocked = true;
            if (a == 0) m->vel.x = 0; else m->vel.z = 0;
        }
    }
    bool falling = m->vel.y < 0;
    m->on_ground = false;
    if (!phys_move_axis(&m->pos, 1, m->vel.y * dt, hw, h, monster_solid, g)) {
        if (falling) m->on_ground = true;
        m->vel.y = 0;
    }
    if (blocked && m->state == MS_CHARGE) {
        sfx_at(g, SFX_MONSTER_STUN, m->pos, 1.0f);
        m->state = MS_STUN;
        m->timer = 1.0f;
        particles_burst(g, v3_add(m->pos, v3(0, h, 0)), v3(1.0f, 0.9f, 0.5f), 8, 3.0f, 0.05f, true);
    }
}

/* Keep chasing the current target unless another living player is clearly closer. */
/* An invisible player is only found by bumping into them. */
static bool can_see_player(const PlayerSlot *ps, vec3 c) {
    if (!ps->active || ps->st.dead) return false;
    return ps->st.invis_t <= 0 || v3_len(v3_sub(player_eye(&ps->pl), c)) < 1.6f;
}

static PlayerSlot *monster_target(Game *g, Monster *m, vec3 c) {
    PlayerSlot *cur = NULL;
    if (m->target >= 0 && m->target < MAX_PLAYERS && can_see_player(&g->players[m->target], c)) cur = &g->players[m->target];
    PlayerSlot *near = NULL;
    float bd = 1e9f;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        float d = v3_len(v3_sub(player_eye(&ps->pl), c));
        if (can_see_player(ps, c) && d < bd) { bd = d; near = ps; }
    }
    if (!near) return NULL;
    if (!cur || v3_len(v3_sub(player_eye(&near->pl), c)) + 4.0f < v3_len(v3_sub(player_eye(&cur->pl), c)))
        cur = near;
    m->target = cur->index;
    return cur;
}

/* ---------- the ways of fighting ---------- */

typedef struct MCtx {
    PlayerSlot *tp;
    vec3 c, eye, feet, hdir;
    float dist, hdist, dy, speed, dt;
    bool los;
    int index;
} MCtx;

enum { BH_GENERIC, BH_MOVED, BH_DONE }; /* run the usual melee chase / moved itself / changed state */

/* One shot, or three spread out for a boss. */
static void monster_volley(Game *g, Monster *m, vec3 target) {
    int n = m->grade == GRADE_BOSS ? 3 : 1;
    vec3 c = monster_centre(m), to = v3_sub(target, c);
    vec3 side = v3_norm(v3(-to.z, 0, to.x));
    for (int k = 0; k < n; k++) enemy_fire(g, m, v3_add(target, v3_scale(side, (k - (n - 1) * 0.5f) * 1.4f)));
}

/* Keep between lo and hi from the target, sidestepping in between. */
static vec3 keep_range(const MCtx *x, float speed, float lo, float hi, float anim) {
    if (x->hdist < lo) return v3_scale(x->hdir, -speed);
    if (x->hdist > hi) return v3_scale(x->hdir, speed);
    vec3 side = v3(-x->hdir.z, 0, x->hdir.x);
    return v3_scale(side, speed * 0.6f * sinf(anim * 0.5f));
}

/* Hits every living player within r of the monster's feet: a stomp, a worm bursting up. */
static void ground_blast(Game *g, Monster *m, float r, vec3 col) {
    vec3 c = monster_centre(m);
    if (m->alt_kind) utility_monster_strike(g, m, m->dmg);
    else for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        float dx = ps->pl.pos.x - m->pos.x, dz = ps->pl.pos.z - m->pos.z;
        if (dx * dx + dz * dz < r * r && fabsf(ps->pl.pos.y - m->pos.y) < 1.6f) monster_strike(g, m, ps, m->dmg, m->pos);
    }
    for (int k = 0; k < 20; k++) {
        float a = k * 2.0f * PI_F / 20.0f;
        vec3 q = v3_add(m->pos, v3(cosf(a) * 0.6f, 0.15f, sinf(a) * 0.6f));
        particles_burst(g, q, col, 1, r * 2.0f, 0.08f, false);
    }
    flash_add(g, c, v3_scale(m->glow, 0.8f), r * 2.5f, 0.3f);
    if (m->type == MON_GOLEM) sfx_at(g, SFX_GOLEM_STOMP, m->pos, 1.0f);
    else sfx_at(g, SFX_HIT_BLUNT, m->pos, 1.0f);
}

/* The bloater bursts: poison for the players, and its neighbours get hurt too. */
static void bloater_burst(Game *g, Monster *m, float mult) {
    vec3 c = monster_centre(m);
    float r = m->grade == GRADE_BOSS ? 4.5f : 3.0f;
    int el = m->element != EL_NONE ? m->element : EL_POISON;
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        float d = v3_len(v3_sub(player_eye(&ps->pl), c));
        if (d > r) continue;
        tele_src(HS_BLAST, m->type, m->grade, m->rank);
        game_hurt_player(g, ps, m->dmg * mult * (1.0f - 0.5f * d / r), c);
        player_element_hit(g, ps, el, m->dmg * mult);
    }
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *o = &g->mons[i];
        if (o == m || !o->alive || v3_len(v3_sub(o->pos, m->pos)) > r) continue;
        monster_damage(g, o, m->dmg * mult * 0.5f, m->pos, 3.0f, -1);
    }
    particles_burst(g, c, element_color(el), 34, 7.0f, 0.1f, true);
    particles_burst(g, c, v3_scale(m->color, 0.6f), 20, 5.0f, 0.1f, false);
    flash_add(g, c, element_color(el), r * 3.0f, 0.4f);
    sfx_at(g, SFX_EXPLOSION, c, 1.0f);
}

/* A boulder thrown in an arc to land at target. */
static void troll_throw(Game *g, Monster *m, vec3 target) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (p->alive) continue;
        memset(p, 0, sizeof *p);
        p->alive = true;
        p->kind = PJ_BOULDER;
        p->owner = -1;
        p->element = m->element;
        p->pos = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.9f, 0));
        vec3 d = v3_sub(target, p->pos);
        float hd = sqrtf(d.x * d.x + d.z * d.z), t = clampf(hd / 12.0f, 0.5f, 1.6f);
        p->vel = v3(d.x / t, d.y / t + 0.5f * 14.0f * t, d.z / t);
        p->dmg = m->dmg * 1.2f;
        p->radius = m->grade == GRADE_BOSS ? 3.2f : 2.2f;
        p->life = 5.0f;
        p->last_hit = -1;
        return;
    }
}

/* The eye lines up its beam: sparks along the line it will fire down. */
static void eye_telegraph(Game *g, Monster *m) {
    static u64 st = 0xE7E;
    st = mix64(st + 1);
    vec3 c = monster_centre(m);
    float t = (float)(st >> 40) / (float)(1ULL << 24);
    vec3 q = v3_lerp(c, m->charge_dir, t);
    if (g->nparts < MAX_PARTICLES - 4) particles_burst(g, q, m->glow, 1, 0.2f, 0.035f, true);
}

static void eye_beam(Game *g, Monster *m) {
    vec3 c = monster_centre(m);
    vec3 dir = v3_norm(v3_sub(m->charge_dir, c));
    float len = phys_raycast(c, dir, 30.0f, game_solid, g);
    float r = m->grade == GRADE_BOSS ? 1.4f : 0.9f;
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        vec3 pc = v3_add(ps->pl.pos, v3(0, 0.9f, 0));
        float t = clampf(v3_dot(v3_sub(pc, c), dir), 0.0f, len);
        if (v3_len(v3_sub(pc, v3_add(c, v3_scale(dir, t)))) < r) monster_strike(g, m, ps, m->dmg * 1.6f, c);
    }
    for (float t = 0.4f; t < len; t += 0.5f)
        if (g->nparts < MAX_PARTICLES - 2) particles_burst(g, v3_add(c, v3_scale(dir, t)), m->glow, 1, 0.6f, 0.07f, true);
    flash_add(g, c, m->glow, 7.0f, 0.25f);
    flash_add(g, v3_add(c, v3_scale(dir, len)), m->glow, 6.0f, 0.25f);
    sfx_at(g, SFX_EYE_BEAM, c, 1.0f);
}

/* Raises the dead next to it (up to a few at a time around). */
static void necro_summon(Game *g, Monster *m, const MCtx *x) {
    int around = 0;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        const Monster *o = &g->mons[i];
        if (o->alive && o->aux == 1 && (o->type == MON_CRAWLER || o->type == MON_SKELETON) && v3_len(v3_sub(o->pos, m->pos)) < 25.0f)
            around++;
    }
    int cap = m->grade == GRADE_BOSS ? 8 : 4, n = m->grade == GRADE_BOSS ? 3 : 2;
    for (int k = 0; k < n && around < cap; k++) {
        float a = (float)(k * 2.1f) + m->anim;
        int type = k % 2 ? MON_SKELETON : MON_CRAWLER;
        vec3 at = v3_add(m->pos, v3(cosf(a) * 1.6f, 0.2f, sinf(a) * 1.6f));
        if (!phys_box_free(at, MONSTERS[type].hw, MONSTERS[type].h, game_solid, g)) continue;
        Monster *o = monster_spawn(g, type, at, (int)m->level, false, m->color, m->chunk_key);
        if (!o) break;
        o->aux = 1; /* raised: worth little */
        o->xp *= 0.2f;
        o->awake = true;
        o->state = MS_CHASE;
        o->target = x->tp->index;
        particles_burst(g, v3_add(at, v3(0, 0.5f, 0)), m->glow, 12, 2.5f, 0.06f, true);
        around++;
    }
    sfx_at(g, SFX_NECRO_SUMMON, m->pos, 1.0f);
}

/* A wide blow: every living player in front within r (half-angle in degrees), with a shove. */
static void sweep(Game *g, Monster *m, float r, float half_deg, float mult, float shove) {
    vec3 fw = v3(cosf(m->yaw), 0, sinf(m->yaw));
    if (m->alt_kind) { utility_monster_strike(g, m, m->dmg * mult); return; }
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        vec3 to = v3_sub(ps->pl.pos, m->pos);
        to.y = 0;
        float dd = v3_len(to);
        if (dd > r || fabsf(ps->pl.pos.y - m->pos.y) > 1.8f) continue;
        if (dd > 0.3f && v3_dot(v3_scale(to, 1.0f / dd), fw) < cosf(half_deg * PI_F / 180.0f)) continue;
        monster_strike(g, m, ps, m->dmg * mult, m->pos);
        if (dd > 0.01f) ps->pl.vel = v3_add(ps->pl.vel, v3_scale(to, shove / dd));
    }
    sfx_at(g, SFX_HIT_BLUNT, m->pos, 1.1f);
}

/* What happens when a raised attack comes down. */
static void windup_release(Game *g, Monster *m, const MCtx *x) {
    const MonsterDef *d = &MONSTERS[m->type];
    bool in_reach = x->hdist < d->reach + d->hw * m->scale + 0.6f && fabsf(x->dy) < 2.0f;
    /* a blow needs a clear line, as the player's does: no biting through a ledge's edge */
    if (in_reach && !m->alt_kind) {
        vec3 top = v3_add(m->pos, v3(0, d->h * m->scale * 0.9f, 0)), chest = v3_add(x->feet, v3(0, 1.0f, 0));
        if (!phys_line_of_sight(top, chest, game_solid, g) && !phys_line_of_sight(top, v3_add(x->feet, v3(0, 0.3f, 0)), game_solid, g))
            in_reach = false;
    }
    m->state = MS_CHASE;
    m->cooldown = d->attack_time;
    switch (m->type) {
    case MON_GOLEM: ground_blast(g, m, (m->grade == GRADE_BOSS ? 1.5f : 1.0f) * 3.2f, v3(0.55f, 0.5f, 0.42f)); break;
    case MON_WORM: ground_blast(g, m, 2.0f, v3(0.5f, 0.38f, 0.25f)); break;
    case MON_BLOATER:
        m->aux = 2;
        bloater_burst(g, m, 1.0f);
        monster_damage(g, m, m->hp + 1.0f, m->pos, 0.0f, m->last_hitter);
        break;
    case MON_EYE: eye_beam(g, m); break;
    case MON_KNIGHT:
        if (in_reach) monster_strike(g, m, x->tp, m->dmg, x->c);
        if (m->aux == 0) { /* and a second blow at once */
            m->aux = 1;
            m->state = MS_WINDUP;
            m->timer = 0.3f;
            m->cooldown = 0;
        } else m->aux = 0;
        break;
    case MON_TROLL:
        if (m->aux == 1) troll_throw(g, m, m->charge_dir);
        else sweep(g, m, d->reach + d->hw * m->scale + 1.0f, 80.0f, 1.2f, 6.0f);
        m->aux = 0;
        break;
    case MON_SKELETON:
        if (x->los) monster_volley(g, m, v3_add(x->eye, v3(0, -0.2f, 0)));
        m->cooldown = d->attack_time;
        break;
    case MON_BRUTE:
        if (m->aux == 1) { /* the slam: it has had enough of you standing there */
            float dmg = m->dmg;
            m->dmg *= 1.2f;
            ground_blast(g, m, 2.5f, v3(0.55f, 0.45f, 0.35f));
            m->dmg = dmg;
            m->aux = 0;
            m->close_t = 0;
        } else if (in_reach) monster_strike(g, m, x->tp, m->dmg, x->c);
        break;
    default:
        if (in_reach) monster_strike(g, m, x->tp, m->dmg, x->c);
        break;
    }
}

static int behave(Game *g, Monster *m, const MCtx *x, vec3 *want) {
    const MonsterDef *d = &MONSTERS[m->type];
    bool boss = m->grade == GRADE_BOSS;
    switch (m->type) {
    case MON_SPIDER:
        /* leaps from a distance */
        if (x->los && m->on_ground && x->hdist > 3.0f && x->hdist < 9.0f && fabsf(x->dy) < 2.5f && m->cooldown <= 0) {
            m->state = MS_CHARGE;
            m->charge_dir = x->hdir;
            m->timer = 0.7f;
            m->cooldown = boss ? 2.0f : 3.5f;
            m->vel.y = 6.5f;
            m->yaw = atan2f(x->hdir.z, x->hdir.x);
            sfx_monster(g, m, MSFX_ATTACK);
            return BH_DONE;
        }
        return BH_GENERIC;
    case MON_WRAITH:
        if (m->hidden) {
            m->t2 -= x->dt;
            if (m->t2 > 0) return BH_MOVED;
            /* back, behind its target */
            vec3 pf = v3(cosf(x->tp->pl.yaw), 0, sinf(x->tp->pl.yaw));
            vec3 at = v3_sub(x->feet, v3_scale(pf, 1.8f));
            at.y = x->feet.y + 0.2f;
            if (phys_box_free(at, d->hw * m->scale, d->h * m->scale, game_solid, g)) m->pos = at;
            m->hidden = 0;
            m->vel = v3(0, 0, 0);
            m->t2 = boss ? 3.5f : 6.0f;
            m->state = MS_WINDUP;
            m->timer = d->windup;
            particles_burst(g, monster_centre(m), m->glow, 16, 3.0f, 0.06f, true);
            sfx_at(g, SFX_WRAITH_VANISH, m->pos, 1.0f);
            return BH_DONE;
        }
        m->t2 -= x->dt;
        if (m->t2 <= 0 && x->los && x->dist < 14.0f && x->dist > 3.0f) {
            m->hidden = 1;
            m->t2 = 1.1f;
            particles_burst(g, monster_centre(m), m->glow, 16, 3.0f, 0.06f, true);
            sfx_at(g, SFX_WRAITH_VANISH, m->pos, 0.9f);
            return BH_MOVED;
        }
        return BH_GENERIC;
    case MON_SHAMAN:
        if (!x->los) return BH_GENERIC;
        if (m->cooldown <= 0 && x->dist < 22.0f) {
            /* mend the wounded around it, or else throw an orb */
            bool hurt = false;
            for (int i = 0; i < MAX_MONSTERS && !hurt; i++) {
                const Monster *o = &g->mons[i];
                hurt = o != m && o->alive && o->hp < o->maxhp * 0.75f && v3_len(v3_sub(o->pos, m->pos)) < 10.0f;
            }
            if (hurt) {
                for (int i = 0; i < MAX_MONSTERS; i++) {
                    Monster *o = &g->mons[i];
                    if (!o->alive || v3_len(v3_sub(o->pos, m->pos)) > 10.0f) continue;
                    o->hp = fminf(o->maxhp, o->hp + o->maxhp * (boss ? 0.3f : 0.15f));
                    particles_burst(g, monster_centre(o), v3(0.4f, 1.0f, 0.5f), 6, 1.5f, 0.05f, true);
                }
                sfx_at(g, SFX_SPELL_HEAL, m->pos, 1.0f);
            } else {
                sfx_monster(g, m, MSFX_ATTACK);
                monster_volley(g, m, x->eye);
            }
            m->cooldown = d->attack_time;
        }
        *want = keep_range(x, x->speed, 7.0f, 12.0f, m->anim);
        return BH_MOVED;
    case MON_EYE:
        if (x->los && x->dist < 22.0f && m->cooldown <= 0) {
            m->state = MS_WINDUP;
            m->timer = d->windup * (boss ? 0.7f : 1.0f);
            m->charge_dir = x->eye; /* where the beam will go: step aside */
            sfx_monster(g, m, MSFX_ATTACK);
            return BH_DONE;
        }
        if (!x->los) return BH_GENERIC;
        *want = keep_range(x, x->speed, 8.0f, 15.0f, m->anim);
        want->y = (x->eye.y + 0.8f - x->c.y) * 1.5f;
        return BH_MOVED;
    case MON_WORM:
        if (m->hidden) {
            /* a mound moving under the floor */
            vec3 target;
            if (x->los && x->dist < 5.0f) *want = v3_scale(x->hdir, x->speed * 1.1f);
            else if (flow_next(x->tp, m->pos, &target)) *want = v3_scale(v3_norm(v3(target.x - m->pos.x, 0, target.z - m->pos.z)), x->speed * 1.1f);
            else *want = v3_scale(x->hdir, x->speed);
            if (fmodf(m->anim, 0.4f) < x->dt * 4.0f) particles_burst(g, v3_add(m->pos, v3(0, 0.1f, 0)), v3(0.4f, 0.32f, 0.22f), 2, 1.5f, 0.06f, false);
            if (x->hdist < 1.6f && fabsf(x->dy) < 1.5f && m->cooldown <= 0) {
                m->hidden = 0;
                m->state = MS_WINDUP;
                m->timer = d->windup;
                m->t2 = 3.5f;
                particles_burst(g, v3_add(m->pos, v3(0, 0.3f, 0)), v3(0.45f, 0.35f, 0.25f), 20, 4.0f, 0.08f, false);
                sfx_monster(g, m, MSFX_ATTACK); /* bursting up */
                return BH_DONE;
            }
            return BH_MOVED;
        }
        m->t2 -= x->dt;
        if (m->t2 <= 0) {
            m->hidden = 1;
            m->cooldown = 1.2f;
            particles_burst(g, v3_add(m->pos, v3(0, 0.3f, 0)), v3(0.45f, 0.35f, 0.25f), 14, 3.0f, 0.07f, false);
            sfx_at(g, SFX_WORM_BURROW, m->pos, 1.0f);
            return BH_MOVED;
        }
        return BH_GENERIC;
    case MON_NECRO:
        if (!x->los) return BH_GENERIC;
        if (m->cooldown <= 0 && x->dist < 20.0f) {
            necro_summon(g, m, x);
            m->cooldown = d->attack_time * (boss ? 0.7f : 1.0f);
        }
        *want = keep_range(x, x->speed, 9.0f, 14.0f, m->anim);
        return BH_MOVED;
    case MON_HARPY:
        if (x->dist > 20.0f) return BH_GENERIC;
        if (m->cooldown <= 0 && x->los) {
            /* dives at you */
            m->state = MS_CHARGE;
            m->charge_dir = v3_norm(v3_sub(x->eye, x->c));
            m->timer = 0.9f;
            m->cooldown = d->attack_time * (boss ? 0.6f : 1.0f);
            sfx_monster(g, m, MSFX_ATTACK);
            return BH_DONE;
        } else {
            float a = g->time * 0.9f + x->index * 1.7f;
            vec3 tgt = v3_add(x->feet, v3(cosf(a) * 5.0f, 3.5f, sinf(a) * 5.0f));
            *want = v3_scale(v3_norm(v3_sub(tgt, x->c)), x->speed);
        }
        return BH_MOVED;
    case MON_HOUND:
        if (!x->los || x->hdist > 8.0f) return BH_GENERIC;
        if (m->cooldown <= 0 && x->hdist < 4.5f && fabsf(x->dy) < 1.5f) {
            m->state = MS_CHARGE;
            m->charge_dir = x->hdir;
            m->timer = 0.45f;
            m->cooldown = d->attack_time * 2.0f;
            sfx_monster(g, m, MSFX_ATTACK);
            return BH_DONE;
        } else {
            /* circle round, each hound its own way */
            float a = atan2f(m->pos.z - x->feet.z, m->pos.x - x->feet.x) + (x->index % 2 ? 0.8f : -0.8f);
            vec3 tgt = v3_add(x->feet, v3(cosf(a) * 3.2f, 0, sinf(a) * 3.2f));
            vec3 dd = v3_sub(tgt, m->pos);
            dd.y = 0;
            *want = v3_scale(v3_norm(dd), x->speed);
            m->yaw = atan2f(x->hdir.z, x->hdir.x);
        }
        return BH_MOVED;
    case MON_TROLL:
        if (x->los && x->dist > 5.0f && x->dist < 20.0f && m->cooldown <= 0) {
            m->state = MS_WINDUP;
            m->timer = 0.8f;
            m->aux = 1;
            m->charge_dir = v3_add(x->feet, v3(0, 0.5f, 0));
            sfx_monster(g, m, MSFX_ATTACK);
            return BH_DONE;
        }
        return BH_GENERIC;
    default: return BH_GENERIC; /* golem, bloater, knight, mimic: the usual chase and a raised blow */
    }
}

/* What else a monster may be after this tick (utility spells): a charmed one fights other
 * monsters or follows its charmer; others go for a lure or a mirror image near them. */
static void pick_alt(Game *g, Monster *m, int self, vec3 c, float player_dist, vec3 *eye, vec3 *feet) {
    m->alt_kind = 0;
    if (m->charm_t > 0) {
        float bd = 16.0f;
        for (int k = 0; k < MAX_MONSTERS; k++) {
            Monster *o = &g->mons[k];
            if (k == self || !o->alive || o->charm_t > 0 || o->hidden) continue;
            float d = v3_len(v3_sub(monster_centre(o), c));
            if (d < bd) { bd = d; m->alt_kind = 2; m->alt_idx = (i16)k; }
        }
        if (m->alt_kind == 2) {
            Monster *o = &g->mons[m->alt_idx];
            *eye = monster_centre(o);
            *feet = o->pos;
            return;
        }
        if (m->charmer >= 0 && m->charmer < MAX_PLAYERS && g->players[m->charmer].active) {
            m->alt_kind = 3;
            *eye = player_eye(&g->players[m->charmer].pl);
            *feet = g->players[m->charmer].pl.pos;
        }
        return;
    }
    float bd = 1e9f;
    for (int k = 0; k < MAX_EFFECTS; k++) {
        const Effect *e = &g->effects[k];
        if (!e->alive || (e->type != EF_LURE && e->type != EF_DECOY)) continue;
        float d = v3_len(v3_sub(e->pos, m->pos));
        float reach = e->type == EF_LURE ? e->radius : 14.0f;
        /* a decoy is taken for you when it is about as close as you are */
        if (d > reach || (e->type == EF_DECOY && d > player_dist + 3.0f)) continue;
        if (e->type == EF_LURE) d -= 100.0f; /* a lure wins */
        if (d < bd) { bd = d; m->alt_kind = 1; m->alt_idx = (i16)k; }
    }
    if (m->alt_kind == 1) {
        const Effect *e = &g->effects[m->alt_idx];
        *feet = e->pos;
        *eye = v3_add(e->pos, v3(0, 1.4f, 0));
    }
}

void monsters_update(Game *g, float dt) {
    if (g->freeze_monsters) return;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        const MonsterDef *d = &MONSTERS[m->type];
        vec3 c = monster_centre(m);
        PlayerSlot *tp = monster_target(g, m, c);
        bool all_dead = tp == NULL;
        if (!tp) tp = &g->players[0];
        vec3 eye = player_eye(&tp->pl);
        vec3 feet = tp->pl.pos;
        vec3 to = v3_sub(eye, c);
        float dist = v3_len(to);
        if (dist > 80.0f && m->charm_t <= 0) continue; /* frozen far away (their chunk may be unloaded) */
        /* utility spells: charm wears off, fear fades, and something else may draw it */
        if (m->charm_t > 0 && (m->charm_t -= dt) <= 0) {
            m->charm_t = 0;
            m->charmer = -1;
            if (m->aux == 99) { /* a summoned wisp: the bond ends, and so does it */
                particles_burst(g, c, v3(0.6f, 0.8f, 1.0f), 14, 2.0f, 0.05f, true);
                m->alive = false;
                continue;
            }
        }
        if (m->fear_t > 0) m->fear_t -= dt;
        float player_dist = all_dead ? 1e9f : dist;
        pick_alt(g, m, i, c, player_dist, &eye, &feet);
        if (m->alt_kind) {
            all_dead = false;
            to = v3_sub(eye, c);
            dist = v3_len(to);
        }
        /* stay put while the ground under them is not loaded */
        if (game_solid(g, (int)floorf(m->pos.x), (int)floorf(m->pos.y) - 1, (int)floorf(m->pos.z)) < 0) continue;
        m->flash = fmaxf(0.0f, m->flash - dt);
        /* a boss below half its health fights harder */
        bool enraged = m->grade == GRADE_BOSS && m->hp < m->maxhp * 0.5f;
        m->cooldown -= dt * (enraged ? 1.7f : 1.0f);
        if (enraged && dist < 30.0f && fmodf(m->anim * 1.3f, 1.0f) < dt * 3.0f) particles_burst(g, c, v3(1.0f, 0.25f, 0.15f), 1, 1.5f, 0.05f, true);
        /* burning and poison tick; frost slows */
        float dot = 0.0f;
        if (m->burn_t > 0) { m->burn_t -= dt; dot += m->burn_dps * dt; }
        if (m->poison_t > 0) { m->poison_t -= dt; dot += m->poison_dps * dt; }
        if (m->slow_t > 0) m->slow_t -= dt;
        /* far out they mend their wounds */
        if (m->rank >= 8 && m->burn_t <= 0 && m->poison_t <= 0 && m->hp < m->maxhp) m->hp = fminf(m->maxhp, m->hp + m->maxhp * 0.008f * dt);
        /* and give off their element */
        if (m->element != EL_NONE && m->rank >= 3 && dist < 30.0f && fmodf(m->anim * 0.7f + (float)i * 0.37f, 1.0f) < dt * 1.5f)
            particles_burst(g, c, element_color(m->element), 1, 0.7f, 0.035f, true);
        if (dot > 0) {
            m->hp -= dot;
            if (((int)(m->anim * 10.0f)) % 4 == 0)
                particles_burst(g, c, m->burn_t > 0 ? element_color(EL_FIRE) : element_color(EL_POISON), 1, 1.0f, 0.04f, true);
            if (m->hp <= 0) {
                m->hp = 0.01f;
                monster_damage(g, m, 1.0f, m->pos, 0.0f, m->last_hitter);
                continue;
            }
        }
        m->anim += dt * (1.0f + v3_len(m->vel));
        float hdist = sqrtf((feet.x - m->pos.x) * (feet.x - m->pos.x) + (feet.z - m->pos.z) * (feet.z - m->pos.z));
        float dy = feet.y - m->pos.y;
        bool los = dist < 40.0f && phys_line_of_sight(c, eye, game_solid, g);
        if (all_dead) { m->awake = false; m->state = MS_IDLE; }
        if (m->alt_kind && !m->awake) { m->awake = true; m->state = MS_CHASE; }
        if (m->sulk_t > 0) m->sulk_t -= dt;
        if (!m->awake && !all_dead && m->sulk_t <= 0) {
            /* mimics wait until you are right next to them */
            if (m->type == MON_MIMIC ? (hdist < 2.4f && fabsf(dy) < 1.5f) : (dist < 7.0f || (dist < 24.0f && los))) {
                sfx_monster(g, m, MSFX_ALERT);
                m->awake = true;
                m->state = MS_CHASE;
                wake_neighbours(g, m);
            }
        } else if (dist > 60.0f) {
            m->awake = false;
            m->state = MS_IDLE;
        }
        /* a melee monster that cannot get closer to its target for 6 s gives up for a while
         * (stuck in a pit, below a ledge): no endless pacing under the player */
        bool ranged = d->flies || m->type == MON_SKELETON || m->type == MON_WISP || m->type == MON_SHAMAN || m->type == MON_EYE ||
                      m->type == MON_NECRO || m->type == MON_WORM;
        if (m->awake && !ranged && !m->alt_kind && m->state == MS_CHASE && !all_dead) {
            if (hdist < d->reach + d->hw * m->scale + 0.6f && fabsf(dy) < 1.8f && los) m->giveup_t = 0, m->giveup_d = hdist;
            else if (hdist < m->giveup_d - 1.0f || m->giveup_t <= 0) { m->giveup_d = hdist; m->giveup_t = dt; }
            else if ((m->giveup_t += dt) > 6.0f) {
                m->awake = false;
                m->state = MS_IDLE;
                m->sulk_t = 10.0f;
                m->giveup_t = 0;
            }
        }
        vec3 want = v3(0, 0, 0);
        float speed = d->speed * (m->elite ? 1.1f : 1.0f) * (m->slow_t > 0 ? 0.45f : 1.0f) * (m->rank >= 6 ? 1.15f : 1.0f) *
                      (enraged ? 1.3f : 1.0f);
        vec3 hdir = v3_norm(v3(feet.x - m->pos.x, 0, feet.z - m->pos.z));
        MCtx x = { tp, c, eye, feet, hdir, dist, hdist, dy, speed, dt, los, i };
        bool lured = m->alt_kind == 1 && g->effects[m->alt_idx].type == EF_LURE;
        if (m->awake && m->fear_t > 0 && !all_dead && m->alt_kind != 3) {
            /* afraid: runs from the nearest player */
            vec3 away = v3_norm(v3(m->pos.x - tp->pl.pos.x, 0, m->pos.z - tp->pl.pos.z));
            want = v3_scale(away, speed * 1.2f);
            if (MONSTERS[m->type].flies) want.y = 0.5f;
            m->yaw = atan2f(away.z, away.x);
            if (m->state == MS_WINDUP || m->state == MS_CHARGE) m->state = MS_CHASE;
        } else if (m->awake && (m->alt_kind == 3 || lured)) {
            /* following its charmer, or standing spellbound by a lure */
            float keep = lured ? 1.2f : 3.0f;
            if (hdist > keep) want = v3_scale(hdir, speed * (lured ? 0.8f : 1.0f));
            if (d->flies) want.y = (feet.y + 1.5f - m->pos.y) * 2.0f;
            if (v3_len(want) > 0.1f) m->yaw = atan2f(want.z, want.x);
            else m->yaw = atan2f(hdir.z, hdir.x);
            if (m->state == MS_WINDUP || m->state == MS_CHARGE) m->state = MS_CHASE;
        } else if (m->awake) {
            switch (m->state) {
            case MS_STUN:
                m->timer -= dt;
                if (m->timer <= 0) m->state = MS_CHASE;
                break;
            case MS_WINDUP:
                m->timer -= dt;
                m->yaw = atan2f(hdir.z, hdir.x);
                if (m->type == MON_EYE) eye_telegraph(g, m);
                if (m->timer <= 0) windup_release(g, m, &x);
                break;
            case MS_CHARGE:
                m->timer -= dt;
                want = v3_scale(m->charge_dir, 12.0f);
                if (hdist < d->hw * m->scale + 0.7f && fabsf(dy) < 1.5f) {
                    monster_strike(g, m, tp, m->dmg * 1.3f, c);
                    if (!m->alt_kind) tp->st.chill_t = fmaxf(tp->st.chill_t, 0.8f); /* staggered */
                    m->state = MS_STUN;
                    m->timer = 0.6f;
                } else if (m->timer <= 0) {
                    m->state = MS_CHASE;
                }
                break;
            default: {
                m->state = MS_CHASE;
                bool moved = false;
                int bh = m->type >= MON_SPIDER && !m->alt_kind ? behave(g, m, &x, &want) : BH_GENERIC;
                if (bh == BH_DONE) break;
                if (bh == BH_MOVED) moved = true;
                else if (m->type == MON_SKELETON) {
                    /* archer: keeps its distance and shoots */
                    if (los && dist < 24.0f && m->cooldown <= 0) {
                        /* the bow creaks a moment before the arrow flies: time to step aside */
                        sfx_monster(g, m, MSFX_ATTACK);
                        m->state = MS_WINDUP;
                        m->timer = 0.35f;
                        m->cooldown = d->attack_time;
                        break;
                    }
                    if (los && dist < 16.0f) {
                        vec3 side = v3(-hdir.z, 0, hdir.x);
                        if (hdist < 6.0f) want = v3_scale(hdir, -speed);
                        else want = v3_scale(side, speed * 0.6f * sinf(m->anim * 0.4f));
                        moved = true;
                    }
                } else if (m->type == MON_BAT && los && dist < 14.0f) {
                    /* flutters straight at your face */
                    vec3 dir3 = v3_norm(to);
                    vec3 side = v3(-dir3.z, 0, dir3.x);
                    want = v3_add(v3_scale(dir3, speed), v3_scale(side, 2.5f * sinf(m->anim * 2.0f)));
                    want.y = (eye.y - 0.4f - c.y) * 3.0f + sinf(m->anim * 3.0f);
                    if (dist < 1.4f && m->cooldown <= 0) {
                        sfx_monster(g, m, MSFX_ATTACK);
                        monster_strike(g, m, tp, m->dmg, c);
                        m->cooldown = d->attack_time;
                        m->vel = v3_scale(dir3, -5.0f); /* bounce off */
                    }
                    moved = true;
                } else if (m->type == MON_WISP) {
                    if (los && dist < 18.0f && m->cooldown <= 0) {
                        sfx_monster(g, m, MSFX_ATTACK);
                        monster_volley(g, m, eye);
                        m->cooldown = d->attack_time;
                    }
                    if (los) {
                        vec3 dir3 = v3_norm(to);
                        vec3 side = v3(-dir3.z, 0, dir3.x);
                        if (dist < 7.0f) want = v3_scale(dir3, -speed);
                        else if (dist > 11.0f) want = v3_scale(dir3, speed);
                        else want = v3_scale(side, speed * sinf(m->anim * 0.5f));
                        want.y = (eye.y + 0.5f - c.y) * 1.5f;
                        moved = true;
                    }
                } else {
                    if (m->type == MON_HORNED && los && hdist > 4.0f && hdist < 14.0f && fabsf(dy) < 1.5f && m->cooldown <= 0) {
                        sfx_at(g, SFX_HORNED_CHARGE, c, 1.0f);
                        m->state = MS_CHARGE;
                        /* aimed where you will be */
                        vec3 lead = v3_add(feet, v3_scale(tp->pl.vel, 0.4f));
                        m->charge_dir = v3_norm(v3(lead.x - m->pos.x, 0, lead.z - m->pos.z));
                        m->timer = 1.3f;
                        m->cooldown = 3.0f;
                        m->yaw = atan2f(hdir.z, hdir.x);
                        break;
                    }
                    if (m->type == MON_BRUTE) {
                        m->close_t = hdist < 2.5f && fabsf(dy) < 1.5f ? m->close_t + dt : 0.0f;
                        if (m->close_t > 1.0f && m->cooldown <= 0) {
                            sfx_monster(g, m, MSFX_ATTACK);
                            m->state = MS_WINDUP;
                            m->timer = d->windup * 1.4f;
                            m->aux = 1;
                            break;
                        }
                    }
                    if (hdist < d->reach + d->hw * m->scale + 0.2f && fabsf(dy) < 1.8f && m->cooldown <= 0) {
                        sfx_monster(g, m, MSFX_ATTACK);
                        m->state = MS_WINDUP;
                        m->timer = d->windup;
                        break;
                    }
                    if (hdist < d->reach + d->hw * m->scale) { moved = true; } /* close enough: hold */
                }
                if (!moved) {
                    vec3 target;
                    if (los && dist < 5.0f) want = v3_scale(hdir, speed);
                    else if (!m->alt_kind && flow_next(tp, m->pos, &target)) {
                        vec3 dir = v3_norm(v3(target.x - m->pos.x, 0, target.z - m->pos.z));
                        want = v3_scale(dir, speed);
                        if (d->flies) want.y = (target.y + 1.2f - m->pos.y) * 2.0f;
                    } else if (los) want = v3_scale(hdir, speed);
                }
                if (m->type == MON_SKELETON && los) m->yaw = atan2f(hdir.z, hdir.x);
                else if (v3_len(want) > 0.1f) m->yaw = atan2f(want.z, want.x);
                /* slimes move in hops */
                if (m->type == MON_SLIME && m->on_ground && v3_len(want) > 0.5f && fmodf(m->anim, 1.3f) < 0.05f) {
                    m->vel.y = 4.5f;
                    sfx_at(g, SFX_SLIME_HOP, m->pos, m->scale);
                }
                break;
            }
            }
        }
        monster_physics(g, m, want, dt);
    }
    /* keep monsters from stacking inside each other */
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *a = &g->mons[i];
        if (!a->alive) continue;
        for (int j = i + 1; j < MAX_MONSTERS; j++) {
            Monster *b = &g->mons[j];
            if (!b->alive || fabsf(a->pos.y - b->pos.y) > 1.5f) continue;
            float dx = b->pos.x - a->pos.x, dz = b->pos.z - a->pos.z;
            float min = (MONSTERS[a->type].hw * a->scale + MONSTERS[b->type].hw * b->scale) * 0.9f;
            float d2 = dx * dx + dz * dz;
            if (d2 >= min * min || d2 < 1e-6f) continue;
            float dd = sqrtf(d2), push = (min - dd) * 0.5f;
            vec3 n = v3(dx / dd * push, 0, dz / dd * push);
            vec3 pa = v3_sub(a->pos, n), pb = v3_add(b->pos, n);
            if (phys_box_free(pa, MONSTERS[a->type].hw * a->scale, MONSTERS[a->type].h * a->scale, game_solid, g)) a->pos = pa;
            if (phys_box_free(pb, MONSTERS[b->type].hw * b->scale, MONSTERS[b->type].h * b->scale, game_solid, g)) b->pos = pb;
        }
    }
}

/* ---------- drawing ---------- */

void players_draw(Game *g, const PlayerSlot *viewer) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const PlayerSlot *ps = &g->players[i];
        if (!ps->active || (ps == viewer && ps->st.scry_t <= 0)) continue;
        const Player *pl = &ps->pl;
        vec3 p = pl->pos;
        float h = player_height(pl->stance);
        vec3 light = game_light_at(g, v3_add(p, v3(0, 1.0f, 0)));
        light = v3(fmaxf(light.x, 0.3f), fmaxf(light.y, 0.3f), fmaxf(light.z, 0.3f));
        vec3 base = ps->st.dead ? v3(0.3f, 0.3f, 0.3f) : ps->color;
        if (ps->st.hurt > 0.3f) base = v3_lerp(base, v3(1.0f, 0.2f, 0.2f), 0.5f);
        vec3 col = v3(base.x * light.x, base.y * light.y, base.z * light.z);
        vec3 dark = v3_scale(col, 0.55f);
        vec3 steel = v3(0.6f * light.x, 0.62f * light.y, 0.66f * light.z);
        float y = pl->yaw;
        vec3 f = v3(cosf(y), 0, sinf(y)), r = v3(-sinf(y), 0, cosf(y));
        if (ps->st.dead) {
            /* a fallen marker until they respawn */
            batch3d_box(v3_add(p, v3(0, 0.15f, 0)), v3(0.25f, 0.15f, 0.7f), y, dark, false);
            continue;
        }
        float walk = sqrtf(pl->vel.x * pl->vel.x + pl->vel.z * pl->vel.z);
        float sw = sinf(g->time * 9.0f) * fminf(1.0f, walk / 5.0f) * 0.3f;
        float s = h / 1.75f;
        for (int k = -1; k <= 1; k += 2) {
            vec3 leg = v3_add(v3_add(p, v3_scale(r, 0.12f * k)), v3_add(v3(0, 0.38f * s, 0), v3_scale(f, sw * k)));
            batch3d_box(leg, v3(0.09f, 0.38f * s, 0.09f), y, dark, false);
        }
        vec3 torso = v3_add(p, v3(0, 1.05f * s, 0));
        batch3d_box(torso, v3(0.16f, 0.32f * s, 0.26f), y, col, false);
        vec3 head = v3_add(torso, v3(0, 0.48f * s, 0));
        batch3d_box(head, v3(0.15f, 0.15f, 0.15f), y, steel, false);
        batch3d_box(v3_add(head, v3_scale(f, 0.15f)), v3(0.02f, 0.03f, 0.1f), y, v3(1.0f, 0.85f, 0.5f), true);
        /* weapon in the right hand, torch flame in the left */
        float swing = ps->st.swing > 0 ? ps->st.swing / fmaxf(ps->st.swing_len, 0.01f) : 0.0f;
        vec3 hand = v3_add(v3_add(torso, v3_scale(r, 0.3f)), v3_add(v3(0, -0.1f, 0), v3_scale(f, 0.25f + swing * 0.3f)));
        /* the weapon they hold, the same model as in their own view */
        vec3 wdir = ps->st.weapon == WPN_SWORD ? v3_add(f, v3(0, 1.2f - swing * 2.0f, 0)) : v3_add(f, v3(0, 1.6f, 0));
        weapon_draw_held(&ps->st.equipped[ps->st.weapon], hand, wdir, r, light, g->time, (float)i * 17.0f);
        vec3 lhand = v3_add(v3_add(torso, v3_scale(r, -0.3f)), v3_add(v3(0, 0.05f, 0), v3_scale(f, 0.2f)));
        batch3d_octa(lhand, 0.06f, 0.1f, g->time * 5.0f, v3(1.0f, 0.65f, 0.25f), true);
        /* a coloured marker above the head so you can find your friends */
        batch3d_octa(v3_add(head, v3(0, 0.45f, 0)), 0.07f, 0.1f, g->time * 2.0f, ps->color, true);
    }
}
