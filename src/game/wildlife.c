#include "game/game.h"

#include "render/batch.h"

/* Neutral wildlife (docs/FOOD.md, section 3): animals that never attack, that monsters
 * ignore and that give no XP. They graze, wander and flee; you hunt them for meat, or take
 * eggs, milk, honey and jelly without killing. A chunk holds a few, seeded like monsters
 * and placed with them (monsters.c spawn_chunk); a hunted one is gone for its 15-minute
 * span (looted keys by epoch), then comes back with its chunk. */

typedef struct CritterDef {
    const char *name;
    float hw, h, speed, flee_r;
    u8 hp;
    int meat;       /* FD_* when hunted, -1 none */
    vec3 color;
} CritterDef;

static const CritterDef DEFS[CR_COUNT] = {
    [CR_HARE] = { "cave hare", 0.2f, 0.32f, 6.5f, 6.0f, 1, FD_HARE, { 0.55f, 0.48f, 0.4f } },
    [CR_HEN] = { "fortress hen", 0.18f, 0.42f, 3.5f, 3.5f, 1, FD_FOWL, { 0.85f, 0.7f, 0.5f } },
    [CR_GOAT] = { "mountain goat", 0.3f, 0.85f, 4.5f, 3.0f, 3, FD_GOAT, { 0.82f, 0.8f, 0.74f } },
    [CR_RAM] = { "cloud ram", 0.36f, 0.85f, 3.2f, 4.0f, 3, FD_MUTTON, { 0.95f, 0.94f, 0.9f } },
    [CR_FISH] = { "blind cave fish", 0.16f, 0.14f, 3.0f, 3.0f, 1, FD_CAVE_FISH, { 0.78f, 0.82f, 0.88f } },
    [CR_CARP] = { "sky carp", 0.32f, 0.3f, 1.6f, 5.0f, 2, FD_SKY_CARP, { 1.0f, 0.78f, 0.35f } },
    [CR_CRAB] = { "shore crab", 0.22f, 0.16f, 2.2f, 3.0f, 2, FD_CRAB, { 0.85f, 0.3f, 0.18f } },
    [CR_SALAMANDER] = { "ember salamander", 0.16f, 0.12f, 4.5f, 5.0f, 1, FD_SALAMANDER, { 1.0f, 0.45f, 0.12f } },
    [CR_GROUSE] = { "frost grouse", 0.18f, 0.32f, 7.0f, 3.0f, 1, FD_GROUSE, { 0.9f, 0.9f, 0.92f } },
    [CR_BEETLE] = { "glow beetle", 0.14f, 0.1f, 0.8f, 0.0f, 1, -1, { 0.15f, 0.2f, 0.15f } },
    [CR_SNAIL] = { "salt snail", 0.12f, 0.12f, 0.25f, 0.0f, 1, -1, { 0.8f, 0.74f, 0.66f } },
    [CR_HIVE] = { "beehive", 0.35f, 0.7f, 0.0f, 0.0f, 255, -1, { 0.78f, 0.62f, 0.3f } },
    [CR_QUAIL] = { "steppe quail", 0.15f, 0.26f, 6.0f, 5.0f, 1, FD_QUAIL, { 0.62f, 0.48f, 0.32f } },
};

#define EPOCH_HUNT 900.0f   /* a hunted animal is back after this */
#define EPOCH_GIVE 600.0f   /* milk, eggs, honey */

static u64 epoch_key(Game *g, u64 key, u64 salt, float span) { return mix64(key ^ salt ^ ((u64)(g->play_time / span) << 20)) | 1; }

static float frand(u64 *s) {
    *s = mix64(*s + 0x9E3779B97F4A7C15ULL);
    return (float)(*s >> 40) / (float)(1ULL << 24);
}
static u64 g_wrng = 0xA11CE;

/* ---------- placing ---------- */

static bool standable(Game *g, int x, int y, int z) {
    return game_solid(g, x, y - 1, z) == 1 && game_solid(g, x, y, z) == 0 && game_solid(g, x, y + 1, z) == 0;
}

static bool block_at(Game *g, int x, int y, int z, Block want) {
    bool known;
    return chunkmap_block(&g->map, x, y, z, &known) == want && known;
}

static Critter *critter_new(Game *g, int kind, vec3 pos, u64 key, u64 ck) {
    if (key && looted_has(g, epoch_key(g, key, 0x4B11, EPOCH_HUNT))) return NULL; /* hunted this span */
    for (int i = 0; i < MAX_CRITTERS; i++) {
        Critter *c = &g->critters[i];
        if (c->alive) continue;
        memset(c, 0, sizeof *c);
        c->alive = true;
        c->kind = (u8)kind;
        c->pos = c->home = pos;
        c->hp = DEFS[kind].hp;
        c->key = key;
        c->chunk_key = ck;
        c->yaw = frand(&g_wrng) * 2.0f * PI_F;
        c->timer = frand(&g_wrng) * 3.0f;
        c->anim = frand(&g_wrng) * 10.0f;
        c->color = DEFS[kind].color;
        /* milk, eggs and honey: none if already taken this span */
        c->ready_t = key && looted_has(g, epoch_key(g, key, 0x61FE, EPOCH_GIVE)) ? EPOCH_GIVE - fmodf(g->play_time, EPOCH_GIVE) : frand(&g_wrng) * 60.0f;
        return c;
    }
    return NULL;
}

static float pick(float *w, int n, float r) {
    float sum = 0;
    for (int i = 0; i < n; i++) sum += w[i];
    if (sum <= 0) return -1;
    r *= sum;
    for (int i = 0; i < n; i++) {
        if (r < w[i]) return (float)i;
        r -= w[i];
    }
    return (float)(n - 1);
}

/* What lives here: by the layer (stratum) and the style (flavor). */
static void habitat(const ChunkInfo *ci, int tier, float *w) {
    switch (ci->stratum) {
    case ST_MINES: w[CR_HARE] += 1.5f; w[CR_SNAIL] += 1.0f; break;
    case ST_ROOTS: w[CR_HARE] += 2.0f; w[CR_BEETLE] += 0.5f; break;
    case ST_FUNGAL: w[CR_BEETLE] += 2.0f; w[CR_FISH] += 1.5f; break;
    case ST_CRYSTAL: w[CR_BEETLE] += 1.5f; w[CR_FISH] += 0.5f; break;
    case ST_OSSUARY: w[CR_SNAIL] += 0.6f; break;
    case ST_INFERNO: case ST_IRONHELL: case ST_MAW: case ST_CORE: w[CR_SALAMANDER] += 2.0f; break;
    case ST_DUNGEON: w[CR_HARE] += 0.8f; w[CR_HEN] += 0.8f; w[CR_FISH] += 0.5f; break;
    case ST_CASTLE: w[CR_HEN] += 2.0f; w[CR_HARE] += 0.6f; break;
    case ST_SPIRES: case ST_AERIE: w[CR_GOAT] += 2.0f; break;
    case ST_CLOUDS: w[CR_RAM] += 2.0f; break;
    case ST_GARDENS: w[CR_HIVE] += 1.0f; w[CR_RAM] += 0.8f; w[CR_CARP] += 0.5f; w[CR_QUAIL] += 0.5f; break;
    case ST_HEAVEN: case ST_SUNCOURT: case ST_ORRERY: w[CR_CARP] += 2.0f; w[CR_HIVE] += ci->stratum == ST_HEAVEN ? 0.6f : 0.0f; break;
    case ST_STORM: case ST_ASTRAL: w[CR_CARP] += 0.5f; break;
    default: break;
    }
    int f = ci->flavor;
    if (f >= FL_FROST && f <= FL_LASTCOLD) w[CR_GROUSE] += 2.0f;
    if (f == FL_STEPPE || f == FL_SANDSTONE || f == FL_SUNSCORCHED) w[CR_QUAIL] += 2.0f;
    if (f == FL_CORAL || f == FL_BRINE || f == FL_PEARL || f == FL_DROWNED || f == FL_LEVIATHAN) { w[CR_CRAB] += 2.0f; w[CR_FISH] += 1.0f; }
    if (f == FL_EMBER || f == FL_PHOENIX || f == FL_SULPHUR || f == FL_OBSIDIAN) w[CR_SALAMANDER] += 1.0f;
    if (f == FL_DRUIDIC || f == FL_BAMBOO) w[CR_HIVE] += 1.0f;
    if (f == FL_SALT) w[CR_SNAIL] += 2.0f;
    if (f == FL_BOYAR || f == FL_GOTHIC) w[CR_HEN] += 0.6f;
    (void)tier;
}

static bool spot_near(Game *g, Chunk *c, Rng *r, vec3 anchor, bool any, vec3 *out) {
    for (int attempt = 0; attempt < 30; attempt++) {
        int wx, wz;
        if (any) { wx = c->cx * CHUNK + rng_range(r, 1, 30); wz = c->cz * CHUNK + rng_range(r, 1, 30); }
        else { wx = (int)floorf(anchor.x) + rng_range(r, -3, 3); wz = (int)floorf(anchor.z) + rng_range(r, -3, 3); }
        int y0 = any ? c->cy * CHUNK + 30 : (int)floorf(anchor.y) + 2, y1 = any ? c->cy * CHUNK + 1 : (int)floorf(anchor.y) - 2;
        for (int y = y0; y >= y1; y--)
            if (standable(g, wx, y, wz)) {
                vec3 p = v3(wx + 0.5f, (float)y, wz + 0.5f);
                bool near = false;
                for (int k = 0; k < MAX_PLAYERS; k++)
                    if (g->players[k].active && v3_len(v3_sub(p, g->players[k].pl.pos)) < 7.0f) near = true;
                if (near) break;
                *out = p;
                return true;
            }
    }
    return false;
}

/* A water cell with water or air above (fish), a lava edge to bask on (salamanders). */
static bool water_spot(Game *g, Chunk *c, Rng *r, vec3 *out) {
    for (int attempt = 0; attempt < 200; attempt++) {
        int i = rng_range(r, 0, CHUNK_VOL - 1);
        if (c->blocks[i] != B_WATER) continue;
        int x = i % CHUNK, z = (i / CHUNK) % CHUNK, y = i / (CHUNK * CHUNK);
        int wx = c->cx * CHUNK + x, wy = c->cy * CHUNK + y, wz = c->cz * CHUNK + z;
        if (!block_at(g, wx, wy - 1, wz, B_WATER) && game_solid(g, wx, wy - 1, wz) != 1) continue;
        *out = v3(wx + 0.5f, wy + 0.4f, wz + 0.5f);
        return true;
    }
    return false;
}
static bool lava_spot(Game *g, Chunk *c, Rng *r, vec3 *out) {
    for (int attempt = 0; attempt < 300; attempt++) {
        int i = rng_range(r, 0, CHUNK_VOL - 1);
        if (c->blocks[i] != B_LAVA) continue;
        int x = i % CHUNK, z = (i / CHUNK) % CHUNK, y = i / (CHUNK * CHUNK);
        int wx = c->cx * CHUNK + x, wy = c->cy * CHUNK + y, wz = c->cz * CHUNK + z;
        static const int D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        for (int k = 0; k < 4; k++)
            for (int dy = 0; dy <= 2; dy++)
                if (standable(g, wx + D[k][0], wy + dy, wz + D[k][1])) {
                    *out = v3(wx + D[k][0] + 0.5f, (float)(wy + dy), wz + D[k][1] + 0.5f);
                    return true;
                }
    }
    return false;
}

/* The Sanctum garden (the realm's garden plot): hens from stage 2, hives and pond fish at 3. */
static void spawn_garden(Game *g, Chunk *c, u64 ck) {
    int gs = sanctum_stage(g, SP_GARDEN);
    if (sanctum_stage(g, SP_ISLAND) < 2 || gs < 2) return;
    int gx = plot_x(SP_GARDEN), gz = plot_z(SP_GARDEN);
    if (c->cx != gx / CHUNK + 1 || c->cz != gz / CHUNK || c->cy != POCKET_CY) return; /* the chunk with the pond */
    float y = (float)POCKET_G;
    for (int k = 0; k < 3; k++) critter_new(g, CR_HEN, v3(78.5f - k * 2.0f, y, 25.5f), mix64(ck ^ (0x4E40 + (u64)k)) | 1, ck);
    if (gs >= 3) {
        critter_new(g, CR_HIVE, v3(85.5f, y, 21.5f), mix64(ck ^ 0x41E1) | 1, ck);
        critter_new(g, CR_HIVE, v3(88.5f, y, 21.5f), mix64(ck ^ 0x41E2) | 1, ck);
        for (int k = 0; k < 4; k++)
            critter_new(g, CR_FISH, v3(71.5f + k * 1.2f, y - 0.6f, 8.5f + (k & 1)), mix64(ck ^ (0xF15 + (u64)k)) | 1, ck);
    }
}

void wildlife_spawn_chunk(Game *g, Chunk *c) {
    const ChunkInfo *ci = &c->info;
    u64 ck = chunk_spawn_key(c->cx, c->cy, c->cz);
    for (int i = 0; i < MAX_CRITTERS; i++)
        if (g->critters[i].alive && g->critters[i].chunk_key == ck) g->critters[i].alive = false;
    if (ci->arch == ARCH_POCKET) { spawn_garden(g, c, ck); return; }
    float w[CR_COUNT] = { 0 };
    int tier;
    world_region(&g->world, c->cx, c->cy, c->cz, &tier);
    if (ci->arch == ARCH_EXTERIOR) { w[CR_HARE] = 1.5f; w[CR_QUAIL] = 1.0f; }
    else habitat(ci, tier, w);
    bool water = false, lava = false;
    for (int i = 0; i < CHUNK_VOL && !(water && lava); i += 7) {
        if (c->blocks[i] == B_WATER) water = true;
        if (c->blocks[i] == B_LAVA) lava = true;
    }
    if (!water) w[CR_FISH] = 0;
    if (!lava) w[CR_SALAMANDER] = 0;
    Rng r = rng_make(ci->seed ^ 0xC0FFEEULL);
    if (!rng_chance(&r, ci->arch == ARCH_EXTERIOR ? 0.5f : 0.6f)) return;
    int groups = rng_chance(&r, 0.35f) ? 2 : 1, idx = 0;
    for (int gi = 0; gi < groups; gi++) {
        float k = pick(w, CR_COUNT, rng_float(&r));
        if (k < 0) return;
        int kind = (int)k;
        int n = 1;
        switch (kind) {
        case CR_HARE: case CR_QUAIL: case CR_HEN: n = rng_range(&r, 2, 3); break;
        case CR_FISH: n = rng_range(&r, 3, 5); break;
        case CR_GOAT: case CR_RAM: case CR_GROUSE: case CR_CRAB: case CR_BEETLE: n = rng_range(&r, 1, 3); break;
        case CR_SNAIL: n = rng_range(&r, 2, 4); break;
        default: n = 1; break;
        }
        vec3 anchor;
        bool ok = kind == CR_FISH ? water_spot(g, c, &r, &anchor) : kind == CR_SALAMANDER ? lava_spot(g, c, &r, &anchor) : spot_near(g, c, &r, v3(0, 0, 0), true, &anchor);
        if (!ok) continue;
        for (int i = 0; i < n; i++, idx++) {
            vec3 p = anchor;
            if (i > 0 && kind == CR_FISH) { if (!water_spot(g, c, &r, &p) || v3_len(v3_sub(p, anchor)) > 8.0f) p = anchor; }
            else if (i > 0 && kind != CR_SALAMANDER && !spot_near(g, c, &r, anchor, false, &p)) continue;
            if (kind == CR_CARP) p.y += 2.5f + rng_float(&r) * 2.0f;
            if (kind == CR_CARP && game_solid(g, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z))) p.y -= 2.0f;
            critter_new(g, kind, p, mix64(ck ^ ((u64)(idx + 1) * 0x9E3779B97F4A7C15ULL)) | 1, ck);
        }
    }
}

Critter *wildlife_spawn(Game *g, int kind, vec3 pos) { return kind >= 0 && kind < CR_COUNT ? critter_new(g, kind, pos, 0, 0) : NULL; }

void wildlife_spawn_home(Game *g) {
    /* the garden changed: its animals come again with its chunk */
    int cx = plot_x(SP_GARDEN) / CHUNK + 1, cz = plot_z(SP_GARDEN) / CHUNK;
    u64 ck = chunk_spawn_key(cx, POCKET_CY, cz);
    for (int i = 0; i < MAX_CRITTERS; i++)
        if (g->critters[i].alive && g->critters[i].chunk_key == ck) g->critters[i].alive = false;
    chunk_forget(g, ck);
}

void wildlife_despawn_far(Game *g) {
    for (int i = 0; i < MAX_CRITTERS; i++) {
        Critter *c = &g->critters[i];
        if (c->alive && !game_nearest_player(g, c->pos, 110.0f)) c->alive = false;
    }
}

/* ---------- living ---------- */

static bool water_at(Game *g, vec3 p) { return block_at(g, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z), B_WATER); }

static void move_ground(Game *g, Critter *c, vec3 want, float dt) {
    c->vel.x = want.x;
    c->vel.z = want.z;
    if (!c->on_ground) c->vel.y -= 18.0f * dt;
    else if (c->vel.y < 0) c->vel.y = 0;
    vec3 n = v3_add(c->pos, v3_scale(c->vel, dt));
    int fx = (int)floorf(n.x), fz = (int)floorf(n.z), fy = (int)floorf(c->pos.y + 0.05f);
    if (game_solid(g, fx, fy, fz)) {
        /* a step up, if there is room */
        if (c->on_ground && !game_solid(g, fx, fy + 1, fz) && !game_solid(g, fx, fy + 2, fz)) n.y = (float)(fy + 1);
        else { n.x = c->pos.x; n.z = c->pos.z; c->timer = 0; }
    }
    /* no running into lava or off into deep water (except fish, handled elsewhere) */
    if (block_at(g, (int)floorf(n.x), (int)floorf(n.y - 0.05f), (int)floorf(n.z), B_LAVA) && c->kind != CR_SALAMANDER) { n.x = c->pos.x; n.z = c->pos.z; }
    c->pos = n;
    int by = (int)floorf(c->pos.y - 0.02f);
    c->on_ground = game_solid(g, (int)floorf(c->pos.x), by, (int)floorf(c->pos.z)) == 1;
    if (c->on_ground && c->vel.y <= 0) c->pos.y = fmaxf(c->pos.y, (float)(by + 1));
    if (c->pos.y < c->home.y - 40.0f) c->alive = false; /* fell off the world */
}

static void move_free(Game *g, Critter *c, vec3 want, float dt, bool in_water) {
    c->vel = v3_lerp(c->vel, want, fminf(1.0f, dt * 3.0f));
    vec3 n = v3_add(c->pos, v3_scale(c->vel, dt));
    bool ok = in_water ? water_at(g, n) : game_solid(g, (int)floorf(n.x), (int)floorf(n.y), (int)floorf(n.z)) == 0;
    if (ok) c->pos = n;
    else { c->vel = v3_scale(c->vel, -0.5f); c->timer = 0; }
}

static PlayerSlot *threat(Game *g, Critter *c, float r) {
    PlayerSlot *best = NULL;
    float bd = r;
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead || ps->st.invis_t > 0) continue;
        float d = v3_len(v3_sub(ps->pl.pos, c->pos));
        /* sneaking up slowly (crouched) halves how far they notice you */
        if (ps->pl.stance != STANCE_STAND) d *= 2.0f;
        if (d < bd) { bd = d; best = ps; }
    }
    return best;
}

static void flee_sound(Game *g, Critter *c) {
    switch (c->kind) {
    case CR_HEN: sfx_at(g, SFX_HEN_FLEE, c->pos, 0.8f); break;
    case CR_GROUSE: case CR_QUAIL: sfx_at(g, SFX_WINGS_FLUTTER, c->pos, 0.8f); break;
    case CR_FISH: sfx_at(g, SFX_FISH_SPLASH, c->pos, 0.6f); break;
    case CR_SALAMANDER: sfx_at(g, SFX_SALAMANDER_HISS, c->pos, 0.8f); break;
    case CR_GOAT: sfx_at(g, SFX_GOAT, c->pos, 0.7f); break;
    case CR_CARP: break;
    default: sfx_at(g, SFX_CRITTER_SQUEAK, c->pos, 0.6f); break;
    }
}

static const Effect *lure_near(Game *g, vec3 p) {
    for (int i = 0; i < MAX_EFFECTS; i++) {
        const Effect *e = &g->effects[i];
        if (e->alive && e->type == EF_LURE && v3_len(v3_sub(e->pos, p)) < 16.0f) return e;
    }
    return NULL;
}

static void lay_egg(Game *g, Critter *c) {
    u64 k = epoch_key(g, c->key, 0xE66, EPOCH_GIVE);
    if (looted_has(g, k) || pickup_find_key(g, k)) return;
    pickup_spawn(g, PK_FOOD, v3_add(c->pos, v3(0, 0.05f, 0)), c->kind == CR_QUAIL ? FD_QUAIL_EGG : FD_EGG, NULL, k, c->chunk_key);
}

void wildlife_update(Game *g, float dt) {
    for (int i = 0; i < MAX_CRITTERS; i++) {
        Critter *c = &g->critters[i];
        if (!c->alive) continue;
        if (!game_nearest_player(g, c->pos, 90.0f)) continue; /* asleep far away */
        const CritterDef *d = &DEFS[c->kind];
        c->anim += dt;
        c->flash = fmaxf(0.0f, c->flash - dt * 4.0f);
        c->timer -= dt;
        c->sound_t -= dt;
        if (c->ready_t > 0) c->ready_t -= dt;
        if (c->calm_t > 0) c->calm_t -= dt;
        /* sounds of life, now and then, when someone is close */
        PlayerSlot *near = game_nearest_player(g, c->pos, 14.0f);
        if (near && c->sound_t <= 0) {
            c->sound_t = 6.0f + frand(&g_wrng) * 10.0f;
            if (c->kind == CR_HEN) sfx_at(g, SFX_HEN, c->pos, 0.5f);
            else if (c->kind == CR_GOAT && frand(&g_wrng) < 0.4f) sfx_at(g, SFX_GOAT, c->pos, 0.5f);
            else if (c->kind == CR_HIVE) { sfx_at(g, SFX_BEES, c->pos, 0.45f); c->sound_t = 2.0f; }
        }
        if (c->kind == CR_HIVE) {
            /* angry bees sting whoever is close */
            if (c->angry_t > 0) {
                c->angry_t -= dt;
                for (int k = 0; k < MAX_PLAYERS; k++) {
                    PlayerSlot *ps = &g->players[k];
                    if (ps->active && !ps->st.dead && v3_len(v3_sub(ps->pl.pos, c->pos)) < 4.0f) {
                        tele_src(HS_OTHER, 0, 0, 0);
                        game_dot_player(g, ps, ps->st.maxhp * 0.02f * dt);
                    }
                }
            }
            continue;
        }
        if (c->kind == CR_HEN || c->kind == CR_QUAIL) {
            if (c->ready_t <= 0 && c->state != CS_FLEE && c->on_ground) {
                lay_egg(g, c);
                c->ready_t = c->kind == CR_QUAIL ? 900.0f : EPOCH_GIVE;
            }
        }
        /* snails are picked up by walking into them */
        if (c->kind == CR_SNAIL) {
            PlayerSlot *ps = game_nearest_player(g, c->pos, 0.9f);
            if (ps && !ps->st.dead) {
                food_give(g, ps, FD_SNAIL, 1);
                g_tele.harvested++;
                looted_mark(g, epoch_key(g, c->key, 0x4B11, EPOCH_HUNT));
                c->alive = false;
                continue;
            }
        }
        /* afraid? */
        PlayerSlot *t = d->flee_r > 0 ? threat(g, c, d->flee_r) : NULL;
        if (t && c->state != CS_FLEE) {
            c->state = CS_FLEE;
            c->timer = 2.0f + frand(&g_wrng) * 2.0f;
            flee_sound(g, c);
            if (c->kind == CR_SALAMANDER) c->timer = 1.2f; /* then into the lava */
        }
        if (c->state == CS_FLEE && c->timer <= 0) {
            if (c->kind == CR_SALAMANDER) { /* gone under the lava */
                particles_burst(g, c->pos, v3(1.0f, 0.5f, 0.1f), 10, 2.0f, 0.05f, true);
                c->alive = false;
                continue;
            }
            c->state = CS_GRAZE;
            c->timer = 1.0f;
        }
        vec3 want = v3(0, 0, 0);
        float sp = d->speed;
        if (c->state == CS_FLEE) {
            vec3 away = t ? v3_sub(c->pos, t->pl.pos) : v3(cosf(c->yaw), 0, sinf(c->yaw));
            away.y = 0;
            if (v3_len(away) < 0.01f) away = v3(1, 0, 0);
            away = v3_norm(away);
            /* a little zigzag */
            float zig = sinf(c->anim * 7.0f) * 0.5f;
            away = v3_norm(v3_add(away, v3(-away.z * zig, 0, away.x * zig)));
            want = v3_scale(away, sp);
            c->yaw = atan2f(away.z, away.x);
        } else {
            const Effect *lure = lure_near(g, c->pos);
            if (lure) {
                vec3 to = v3_sub(lure->pos, c->pos);
                to.y = 0;
                if (v3_len(to) > 1.5f) {
                    to = v3_norm(to);
                    want = v3_scale(to, sp * 0.5f);
                    c->yaw = atan2f(to.z, to.x);
                }
            } else if (c->state == CS_WALK) {
                vec3 f = v3(cosf(c->yaw), 0, sinf(c->yaw));
                want = v3_scale(f, sp * 0.3f);
                if (c->timer <= 0) { c->state = CS_GRAZE; c->timer = 1.5f + frand(&g_wrng) * 4.0f; }
            } else if (c->timer <= 0) {
                c->state = CS_WALK;
                c->timer = 0.8f + frand(&g_wrng) * 1.8f;
                /* back toward home when far from it */
                vec3 h = v3_sub(c->home, c->pos);
                h.y = 0;
                c->yaw = v3_len(h) > 6.0f ? atan2f(h.z, h.x) + (frand(&g_wrng) - 0.5f) : frand(&g_wrng) * 2.0f * PI_F;
            }
        }
        switch (c->kind) {
        case CR_FISH: {
            if (!water_at(g, c->pos)) { /* stranded: flop back down */
                c->pos.y -= dt * 3.0f;
                if (c->pos.y < c->home.y - 3.0f) c->alive = false;
                break;
            }
            if (c->state != CS_FLEE) want = v3_scale(v3(cosf(c->yaw), sinf(c->anim * 0.7f) * 0.2f, sinf(c->yaw)), sp * 0.4f);
            move_free(g, c, want, dt, true);
            break;
        }
        case CR_CARP: {
            /* floats in slow circles round home, bobbing */
            if (c->state != CS_FLEE) {
                float a = c->anim * 0.25f + (float)(c->key & 7);
                vec3 tgt = v3_add(c->home, v3(cosf(a) * 4.0f, sinf(c->anim * 0.6f) * 0.5f, sinf(a) * 4.0f));
                vec3 to = v3_sub(tgt, c->pos);
                want = v3_len(to) > 0.1f ? v3_scale(v3_norm(to), sp) : v3(0, 0, 0);
            } else want.y = 1.5f;
            if (v3_len(v3(want.x, 0, want.z)) > 0.1f) c->yaw = atan2f(want.z, want.x);
            move_free(g, c, want, dt, false);
            break;
        }
        default:
            /* grouse and quail burst into flight when fleeing */
            if ((c->kind == CR_GROUSE || c->kind == CR_QUAIL || c->kind == CR_HEN) && c->state == CS_FLEE && c->on_ground && frand(&g_wrng) < dt * 2.0f)
                c->vel.y = c->kind == CR_HEN ? 3.0f : 5.0f;
            if (c->kind == CR_HARE && c->on_ground && v3_len(want) > 0.5f && frand(&g_wrng) < dt * 3.0f) c->vel.y = 3.5f; /* hops */
            move_ground(g, c, want, dt);
            break;
        }
    }
}

/* ---------- being hit ---------- */

static void die(Game *g, Critter *c, int owner) {
    const CritterDef *d = &DEFS[c->kind];
    c->alive = false;
    looted_mark(g, epoch_key(g, c->key, 0x4B11, EPOCH_HUNT));
    sfx_at(g, SFX_CRITTER_DEATH, c->pos, 0.8f);
    particles_burst(g, v3_add(c->pos, v3(0, d->h * 0.5f, 0)), c->kind == CR_FISH ? v3(0.6f, 0.7f, 0.9f) : v3(0.55f, 0.1f, 0.08f), 8, 2.0f, 0.04f, false);
    if (d->meat >= 0) pickup_spawn(g, PK_FOOD, v3_add(c->pos, v3(0, 0.3f, 0)), d->meat, NULL, 0, 0);
    if (owner >= 0) g_tele.hunted++;
}

static PlayerSlot *owner_slot(Game *g, int owner) { return owner >= 0 && owner < MAX_PLAYERS && g->players[owner].active ? &g->players[owner] : NULL; }

/* A hit on an animal. Hives and beetles give rather than die. */
static void hurt(Game *g, Critter *c, float dmg, int element, int owner) {
    PlayerSlot *ps = owner_slot(g, owner);
    switch (c->kind) {
    case CR_HIVE:
        if (element == EL_FIRE) { c->calm_t = 30.0f; c->angry_t = 0; particles_burst(g, c->pos, v3(0.6f, 0.6f, 0.6f), 12, 1.0f, 0.08f, false); return; }
        if (c->calm_t > 0 && ps) {
            if (c->ready_t <= 0) {
                food_give(g, ps, FD_HONEY, 2);
                /* the hives of heaven keep royal jelly */
                int tier;
                int region = world_region(&g->world, floordiv((int)floorf(c->pos.x), CHUNK), floordiv((int)floorf(c->pos.y), CHUNK),
                                          floordiv((int)floorf(c->pos.z), CHUNK), &tier);
                if ((region == REG_UP && tier >= 3) || frand(&g_wrng) < 0.1f) food_give(g, ps, FD_ROYAL_JELLY, 1);
                c->ready_t = EPOCH_GIVE;
                looted_mark(g, epoch_key(g, c->key, 0x61FE, EPOCH_GIVE));
                g_tele.harvested++;
            } else if (ps->banner_time <= 0) game_banner(ps, "THE HIVE IS EMPTY FOR NOW");
            return;
        }
        if (c->angry_t <= 0) sfx_at(g, SFX_BEES_ANGRY, c->pos, 1.0f);
        c->angry_t = 5.0f;
        if (ps && ps->banner_time <= 0) game_banner(ps, "THE BEES! SMOKE THE HIVE WITH FIRE FIRST");
        return;
    case CR_BEETLE:
        if (ps) { food_give(g, ps, FD_BEETLE_JELLY, 1); g_tele.harvested++; }
        looted_mark(g, epoch_key(g, c->key, 0x4B11, EPOCH_HUNT));
        sfx_at(g, SFX_WINGS_FLUTTER, c->pos, 0.5f);
        particles_burst(g, c->pos, v3(0.4f, 1.0f, 0.6f), 10, 2.5f, 0.04f, true);
        c->alive = false;
        return;
    case CR_SNAIL:
        if (ps) { food_give(g, ps, FD_SNAIL, 1); g_tele.harvested++; }
        looted_mark(g, epoch_key(g, c->key, 0x4B11, EPOCH_HUNT));
        c->alive = false;
        return;
    default: break;
    }
    if (c->kind == CR_FISH && element == EL_FROST) { die(g, c, owner); return; } /* frozen, floats up */
    int n = dmg >= 45.0f ? 2 : 1;
    if (c->kind == CR_CRAB && c->state != CS_FLEE) n = 1; /* in its shell */
    c->hp = (u8)(c->hp > n ? c->hp - n : 0);
    c->flash = 1.0f;
    if (c->hp == 0) { die(g, c, owner); return; }
    c->state = CS_FLEE;
    c->timer = 3.0f;
    flee_sound(g, c);
    if (c->kind == CR_GOAT && ps) { /* butts back */
        vec3 push = v3_sub(ps->pl.pos, c->pos);
        push.y = 0;
        ps->pl.vel = v3_add(ps->pl.vel, v3_add(v3_scale(v3_norm(push), 6.0f), v3(0, 2.0f, 0)));
        c->state = CS_GRAZE;
        c->timer = 1.0f;
    }
}

static vec3 centre(const Critter *c) { return v3_add(c->pos, v3(0, DEFS[c->kind].h * 0.5f, 0)); }

bool wildlife_hit(Game *g, vec3 p, float radius, float dmg, int element, int owner) {
    bool any = false;
    for (int i = 0; i < MAX_CRITTERS; i++) {
        Critter *c = &g->critters[i];
        if (!c->alive) continue;
        const CritterDef *d = &DEFS[c->kind];
        vec3 cc = centre(c);
        float reach = d->hw + 0.15f + radius;
        if (fabsf(p.x - cc.x) > reach || fabsf(p.z - cc.z) > reach || fabsf(p.y - cc.y) > d->h * 0.5f + 0.15f + radius) continue;
        if (radius <= 0 && c->kind == CR_HIVE && element != EL_FIRE) {} /* an arrow into a hive angers it, as a blow does */
        hurt(g, c, dmg, element, owner);
        any = true;
        if (radius <= 0) break;
    }
    return any;
}

bool wildlife_strike(Game *g, PlayerSlot *ps, vec3 eye, vec3 fwd, float range, float arc, float dmg) {
    Critter *best = NULL;
    float bd = 1e9f;
    vec3 chest = v3_add(eye, v3(0, -0.5f, 0));
    for (int i = 0; i < MAX_CRITTERS; i++) {
        Critter *c = &g->critters[i];
        if (!c->alive) continue;
        vec3 cc = centre(c), dv = v3_sub(cc, chest);
        float dy = dv.y;
        dv.y = 0;
        float dist = v3_len(dv) - DEFS[c->kind].hw;
        if (dist > range || dy > 2.0f || dy < -2.8f) continue;
        float ang = acosf(clampf(v3_dot(v3_norm(dv), fwd), -1.0f, 1.0f));
        if (ang > arc * 0.5f && dist > 0.4f) continue;
        if (!phys_line_of_sight(eye, cc, game_solid, g)) continue;
        if (dist < bd) { bd = dist; best = c; }
    }
    if (!best) return false;
    hurt(g, best, dmg, item_affix_element(&ps->st.equipped[WPN_SWORD]), ps->index);
    return true;
}

bool wildlife_use(Game *g, PlayerSlot *ps) {
    vec3 f = v3(cosf(ps->pl.yaw), 0, sinf(ps->pl.yaw));
    for (int i = 0; i < MAX_CRITTERS; i++) {
        Critter *c = &g->critters[i];
        if (!c->alive || c->kind != CR_GOAT || c->state == CS_FLEE) continue;
        vec3 dv = v3_sub(c->pos, ps->pl.pos);
        dv.y = 0;
        float d = v3_len(dv);
        if (d > 1.9f || v3_dot(v3_scale(dv, 1.0f / fmaxf(d, 0.01f)), f) < 0.6f) continue;
        if (c->ready_t > 0) {
            if (ps->banner_time <= 0) game_banner(ps, "THE GOAT HAS NO MILK FOR NOW");
            return false; /* the blow goes on */
        }
        food_give(g, ps, FD_MILK, 1);
        c->ready_t = EPOCH_GIVE;
        c->timer = 3.0f;
        looted_mark(g, epoch_key(g, c->key, 0x61FE, EPOCH_GIVE));
        g_tele.harvested++;
        sfx_at(g, SFX_MILK, c->pos, 0.9f);
        return true;
    }
    return false;
}

Critter *wildlife_nearest(Game *g, vec3 p, float max_dist, bool huntable) {
    Critter *best = NULL;
    float bd = max_dist;
    for (int i = 0; i < MAX_CRITTERS; i++) {
        Critter *c = &g->critters[i];
        if (!c->alive || (huntable && (DEFS[c->kind].meat < 0 && c->kind != CR_BEETLE && c->kind != CR_SNAIL))) continue;
        float d = v3_len(v3_sub(c->pos, p));
        if (d < bd) { bd = d; best = c; }
    }
    return best;
}

/* ---------- drawing ---------- */

typedef struct Frame { vec3 p, f, r; float s; } Frame;
static vec3 at(const Frame *b, float fw, float rt, float up) {
    return v3_add(b->p, v3_add(v3_scale(b->f, fw * b->s), v3_add(v3_scale(b->r, rt * b->s), v3(0, up * b->s, 0))));
}
static void bx(const Frame *b, vec3 c, float hf, float hr, float hu, vec3 col, bool em) {
    batch3d_obox(c, v3_scale(b->f, hf * b->s), v3_scale(b->r, hr * b->s), v3(0, hu * b->s, 0), col, em);
}
static vec3 lit(vec3 c, vec3 l) { return v3(c.x * l.x, c.y * l.y, c.z * l.z); }

static void legs(const Frame *b, float fw, float rt, float h, float r, float t, vec3 col) {
    for (int k = 0; k < 4; k++) {
        float sw = sinf(t + (k == 0 || k == 3 ? 0.0f : PI_F)) * 0.06f;
        bx(b, at(b, (k < 2 ? fw : -fw) + sw, k % 2 ? rt : -rt, h * 0.5f), r, r, h * 0.5f, col, false);
    }
}

void wildlife_draw(Game *g, PlayerSlot *viewer) {
    vec3 eye = game_view_eye(viewer);
    for (int i = 0; i < MAX_CRITTERS; i++) {
        Critter *c = &g->critters[i];
        if (!c->alive || v3_len(v3_sub(c->pos, eye)) > 60.0f) continue;
        vec3 l = game_light_at(g, v3_add(c->pos, v3(0, 0.3f, 0)));
        l = v3(fmaxf(l.x, 0.25f), fmaxf(l.y, 0.24f), fmaxf(l.z, 0.22f));
        if (c->flash > 0) l = v3_add(l, v3(c->flash, c->flash * 0.5f, c->flash * 0.5f));
        Frame b = { c->pos, v3(cosf(c->yaw), 0, sinf(c->yaw)), v3(-sinf(c->yaw), 0, cosf(c->yaw)), 1.0f };
        vec3 col = lit(c->color, l), dark = v3_scale(col, 0.6f);
        bool moving = c->state != CS_GRAZE;
        float t = c->anim * (c->state == CS_FLEE ? 18.0f : 8.0f) * (moving ? 1.0f : 0.0f);
        float peck = !moving ? fmaxf(0.0f, sinf(c->anim * 2.5f)) : 0.0f;
        switch (c->kind) {
        case CR_HARE: {
            float hop = moving ? fabsf(sinf(c->anim * 9.0f)) * 0.08f : 0.0f;
            b.p.y += hop;
            bx(&b, at(&b, -0.02f, 0, 0.14f), 0.16f, 0.09f, 0.09f, col, false);
            bx(&b, at(&b, 0.17f, 0, 0.2f - peck * 0.06f), 0.07f, 0.06f, 0.06f, col, false);
            for (int k = -1; k <= 1; k += 2) bx(&b, at(&b, 0.14f, 0.03f * k, 0.32f - peck * 0.06f), 0.015f, 0.02f, 0.08f, dark, false);
            bx(&b, at(&b, -0.19f, 0, 0.17f), 0.03f, 0.03f, 0.03f, lit(v3(0.95f, 0.95f, 0.95f), l), false);
            legs(&b, 0.09f, 0.06f, 0.06f, 0.025f, t, dark);
            break;
        }
        case CR_HEN: case CR_QUAIL: case CR_GROUSE: {
            float s = c->kind == CR_QUAIL ? 0.7f : c->kind == CR_GROUSE ? 0.85f : 1.0f;
            b.s = s;
            bool air = !c->on_ground;
            bx(&b, at(&b, 0, 0, 0.2f), 0.13f, 0.09f, 0.09f, col, false);
            bx(&b, at(&b, -0.14f, 0, 0.26f), 0.04f, 0.06f, 0.06f, dark, false); /* tail */
            bx(&b, at(&b, 0.12f + peck * 0.05f, 0, 0.33f - peck * 0.15f), 0.05f, 0.045f, 0.05f, col, false);
            bx(&b, at(&b, 0.18f + peck * 0.05f, 0, 0.32f - peck * 0.15f), 0.025f, 0.015f, 0.01f, lit(v3(0.95f, 0.75f, 0.2f), l), false);
            if (c->kind == CR_HEN) bx(&b, at(&b, 0.12f + peck * 0.05f, 0, 0.39f - peck * 0.15f), 0.03f, 0.008f, 0.02f, lit(v3(0.9f, 0.1f, 0.1f), l), false);
            if (c->kind == CR_QUAIL) bx(&b, at(&b, 0.13f, 0, 0.42f), 0.01f, 0.006f, 0.04f, dark, false);
            float wf = air ? sinf(c->anim * 30.0f) * 0.08f : 0.0f;
            for (int k = -1; k <= 1; k += 2) bx(&b, at(&b, 0, 0.1f * k, 0.22f + wf), 0.08f, 0.015f, 0.05f, dark, false);
            for (int k = -1; k <= 1; k += 2)
                bx(&b, at(&b, sinf(t + k) * 0.04f, 0.04f * k, 0.05f), 0.01f, 0.01f, 0.05f, lit(v3(0.9f, 0.7f, 0.25f), l), false);
            break;
        }
        case CR_GOAT: case CR_RAM: {
            bool ram = c->kind == CR_RAM;
            float bw = ram ? 0.24f : 0.16f;
            bx(&b, at(&b, 0, 0, 0.55f), 0.32f, bw, ram ? 0.2f : 0.15f, col, false);
            vec3 head = at(&b, 0.36f + peck * 0.05f, 0, 0.72f - peck * 0.3f);
            bx(&b, head, 0.1f, 0.07f, 0.08f, ram ? lit(v3(0.25f, 0.22f, 0.2f), l) : col, false);
            vec3 horn = lit(ram ? v3(0.7f, 0.62f, 0.45f) : v3(0.4f, 0.35f, 0.3f), l);
            for (int k = -1; k <= 1; k += 2) {
                if (ram) bx(&b, v3_add(head, v3_add(v3_scale(b.r, 0.08f * k), v3(0, 0.03f, 0))), 0.06f, 0.03f, 0.06f, horn, false);
                else batch3d_spike(v3_add(head, v3_add(v3_scale(b.r, 0.04f * k), v3(0, 0.07f, 0))), v3_add(v3(0, 1, 0), v3_scale(b.f, -0.8f)), 0.02f, 0.2f, horn, false);
            }
            if (!ram) bx(&b, v3_add(head, v3_add(v3_scale(b.f, 0.04f), v3(0, -0.1f, 0))), 0.02f, 0.02f, 0.05f, dark, false);
            legs(&b, 0.22f, ram ? 0.14f : 0.1f, 0.4f, 0.04f, t, ram ? lit(v3(0.25f, 0.22f, 0.2f), l) : dark);
            break;
        }
        case CR_FISH: case CR_CARP: {
            bool carp = c->kind == CR_CARP;
            float s = carp ? 1.6f : 0.8f, wag = sinf(c->anim * (c->state == CS_FLEE ? 20.0f : 6.0f)) * 0.3f;
            b.s = s;
            vec3 body = at(&b, 0, 0, 0.06f);
            bx(&b, body, 0.16f, 0.04f, 0.06f, col, carp);
            Frame tb = b;
            float ty = c->yaw + PI_F + wag;
            tb.f = v3(cosf(ty), 0, sinf(ty));
            tb.r = v3(-sinf(ty), 0, cosf(ty));
            tb.p = at(&b, -0.16f, 0, 0);
            bx(&tb, at(&tb, 0.06f, 0, 0.06f), 0.06f, 0.01f, 0.07f, carp ? v3_scale(col, 0.85f) : dark, carp);
            if (carp) for (int k = -1; k <= 1; k += 2) bx(&b, at(&b, 0.02f, 0.06f * k, 0.03f), 0.05f, 0.03f, 0.01f, v3_scale(col, 0.8f), true);
            break;
        }
        case CR_CRAB: {
            float sc = sinf(c->anim * 14.0f) * (moving ? 0.03f : 0.0f);
            bx(&b, at(&b, 0, sc, 0.08f), 0.12f, 0.15f, 0.05f, col, false);
            for (int k = -1; k <= 1; k += 2) {
                bx(&b, at(&b, 0.16f, 0.12f * k, 0.1f + peck * 0.04f), 0.05f, 0.04f, 0.03f, col, false);
                for (int j = 0; j < 3; j++) bx(&b, at(&b, -0.06f + j * 0.06f, (0.18f + (j == 1 ? 0.02f : 0.0f)) * k, 0.03f), 0.01f, 0.05f, 0.01f, dark, false);
            }
            for (int k = -1; k <= 1; k += 2) bx(&b, at(&b, 0.12f, 0.05f * k, 0.15f), 0.01f, 0.01f, 0.03f, lit(v3(0.1f, 0.1f, 0.1f), l), false);
            break;
        }
        case CR_SALAMANDER: {
            vec3 glow = v3(1.0f, 0.55f, 0.15f);
            for (int k = 0; k < 4; k++) {
                float w = sinf(c->anim * (moving ? 14.0f : 2.0f) + k) * 0.05f;
                bx(&b, at(&b, 0.12f - k * 0.1f, w, 0.05f), 0.06f, 0.05f - k * 0.008f, 0.035f, k ? col : v3_scale(col, 1.1f), false);
                if (k < 3) bx(&b, at(&b, 0.12f - k * 0.1f, w, 0.09f), 0.015f, 0.015f, 0.01f, glow, true);
            }
            legs(&b, 0.06f, 0.07f, 0.03f, 0.015f, t, dark);
            break;
        }
        case CR_BEETLE: {
            vec3 glow = v3(0.35f, 1.0f, 0.6f);
            bx(&b, at(&b, 0.04f, 0, 0.05f), 0.05f, 0.05f, 0.03f, col, false);
            float pulse = 0.7f + 0.3f * sinf(c->anim * 2.0f);
            bx(&b, at(&b, -0.06f, 0, 0.05f), 0.07f, 0.06f, 0.04f, v3_scale(glow, pulse), true);
            legs(&b, 0.04f, 0.07f, 0.02f, 0.008f, t, col);
            break;
        }
        case CR_SNAIL: {
            bx(&b, at(&b, 0.02f, 0, 0.02f), 0.12f, 0.04f, 0.02f, lit(v3(0.7f, 0.65f, 0.55f), l), false);
            bx(&b, at(&b, -0.02f, 0, 0.09f), 0.07f, 0.05f, 0.07f, col, false);
            bx(&b, at(&b, 0.13f, 0, 0.06f), 0.005f, 0.005f, 0.035f, dark, false);
            break;
        }
        case CR_HIVE: {
            for (int k = 0; k < 4; k++)
                bx(&b, at(&b, 0, 0, 0.1f + k * 0.15f), 0.3f - k * 0.05f, 0.3f - k * 0.05f, 0.075f, k % 2 ? col : v3_scale(col, 0.85f), false);
            bx(&b, at(&b, 0.26f, 0, 0.12f), 0.02f, 0.06f, 0.04f, lit(v3(0.1f, 0.08f, 0.05f), l), false);
            int bees = c->angry_t > 0 ? 14 : c->calm_t > 0 ? 2 : 6;
            float r = c->angry_t > 0 ? 1.6f : 0.8f, sp = c->angry_t > 0 ? 6.0f : 2.0f;
            for (int k = 0; k < bees; k++) {
                float a = c->anim * sp + k * 2.4f, h = 0.4f + 0.3f * sinf(c->anim * 3.0f + k);
                vec3 bp = v3_add(c->pos, v3(cosf(a) * r * (0.6f + 0.4f * sinf(k * 1.3f)), h, sinf(a) * r));
                batch3d_box(bp, v3(0.025f, 0.02f, 0.02f), a, v3(0.9f, 0.75f, 0.1f), true);
            }
            if (c->calm_t > 0) /* a thin smoke */
                batch3d_box(v3_add(c->pos, v3(0, 0.9f + fmodf(c->anim, 1.0f) * 0.5f, 0)), v3(0.08f, 0.08f, 0.08f), c->anim, lit(v3(0.6f, 0.6f, 0.6f), l), false);
            break;
        }
        }
        /* Reveal the Hidden: nests and hives show */
        if (viewer->st.reveal_t > 0 && (c->kind == CR_HIVE || ((c->kind == CR_HEN || c->kind == CR_QUAIL || c->kind == CR_GOAT) && c->ready_t <= 0)))
            batch3d_octa(v3_add(c->pos, v3(0, DEFS[c->kind].h + 0.4f, 0)), 0.05f, 0.08f, c->anim * 2.0f, v3(0.6f, 1.0f, 0.5f), true);
    }
}
