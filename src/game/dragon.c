#include "game/game.h"

#include <ctype.h>

#include "render/batch.h"

/* Dragons (dragons.h, docs/DRAGONS.md): monsters of type MON_DRAGON with a kind and a size.
 *
 *   dragonets  swarms that flit round you, bite and spit sparks of their element
 *   drakes     packs on the ground: charge, bite, a short breath
 *   wyverns    in the air: they circle, dive and sting, and climb away again
 *   dragons and ancient wyrms: bosses that sleep on their hoards in their lairs. Woken (too
 *              close, too loud, a step on the gold, a blow), they roar and the lair wakes with
 *              them. In the air they circle and make breath runs; on the ground they bite, sweep
 *              their tail behind them and stomp, and breathe at you. Three phases by their
 *              health: in the air, grounded and furious, desperate (the lair comes down).
 *              Torn wings ground them for good; a blow from below on the belly hurts more.
 * Their breath goes through the simulation: fire burns, frost freezes, lightning arcs, poison
 * and rot drift, acid eats stone, magma pools, the dead rise. */

enum { DST_SLEEP, DST_IDLE, DST_GROUND, DST_TAKEOFF, DST_FLY, DST_STRAFE, DST_LAND, DST_INHALE, DST_BREATH, DST_DIVE, DST_BURROW, DST_VANISH };

static int fl(float v) { return (int)floorf(v); }
static vec3 rgb3(const u8 c[3]) { return v3(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f); }
static void upper(char *s) { for (; *s; s++) *s = (char)toupper((unsigned char)*s); }
static float frnd(u64 *s) { *s = mix64(*s + 0x9E3779B97F4A7C15ULL); return (float)(*s >> 40) / (float)(1ULL << 24); }
static u64 g_drng = 0xD2A60;

static const DragonDef *KIND(const Monster *m) { return &DRAGON_KINDS[m->dkind]; }
static const DragonSize *SIZE(const Monster *m) { return &DRAGON_SIZES[m->dsize]; }
static bool great(const Monster *m) { return m->dsize >= DS_DRAGON; }
static bool can_fly(const Monster *m) { return SIZE(m)->flies && !(KIND(m)->traits & DT_WINGLESS) && !m->grounded; }

/* ---------- the shaking of the world, and the falling stones ---------- */

void dragons_shake(Game *g, vec3 p, float amount, float radius) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        float d = v3_len(v3_sub(ps->pl.pos, p));
        if (d < radius) ps->shake = fmaxf(ps->shake, amount * (1.0f - d / radius));
    }
}

#define MAX_ROCKS 32
static struct { bool on; vec3 p; float vy, dmg; } rocks[MAX_ROCKS];

static void rock_drop(Game *g, vec3 at, float dmg) {
    /* from the roof straight above */
    int x = fl(at.x), z = fl(at.z), y = fl(at.y) + 2;
    for (int k = 0; k < 30 && !game_solid(g, x, y + 1, z); k++) y++;
    for (int i = 0; i < MAX_ROCKS; i++)
        if (!rocks[i].on) {
            rocks[i].on = true;
            rocks[i].p = v3(x + 0.5f, (float)y, z + 0.5f);
            rocks[i].vy = 0;
            rocks[i].dmg = dmg;
            particles_burst(g, rocks[i].p, v3(0.5f, 0.45f, 0.4f), 6, 1.0f, 0.06f, false);
            return;
        }
}

static void rocks_update(Game *g, float dt) {
    for (int i = 0; i < MAX_ROCKS; i++) {
        if (!rocks[i].on) continue;
        rocks[i].vy -= 20.0f * dt;
        vec3 n = v3_add(rocks[i].p, v3(0, rocks[i].vy * dt, 0));
        if (game_solid(g, fl(n.x), fl(n.y), fl(n.z)) || n.y < rocks[i].p.y - 60.0f) {
            rocks[i].on = false;
            vec3 at = v3(n.x, (float)(fl(n.y) + 1), n.z);
            particles_burst(g, at, v3(0.5f, 0.45f, 0.4f), 14, 3.0f, 0.08f, false);
            sfx_at(g, SFX_COLLAPSE_RUMBLE, at, 0.5f);
            bool known;
            if (chunkmap_block(&g->map, fl(at.x), fl(at.y), fl(at.z), &known) == B_AIR && !town_chunk(chunkmap_get(&g->map, floordiv(fl(at.x), CHUNK), floordiv(fl(at.y), CHUNK), floordiv(fl(at.z), CHUNK))))
                chunkmap_edit(&g->map, fl(at.x), fl(at.y), fl(at.z), B_RUBBLE, 0, 0);
            for (int k = 0; k < MAX_PLAYERS; k++) {
                PlayerSlot *ps = &g->players[k];
                if (ps->active && !ps->st.dead && v3_len(v3_sub(ps->pl.pos, at)) < 2.2f) {
                    tele_src(HS_TRAP, MON_DRAGON, 0, 0);
                    game_hurt_player(g, ps, rocks[i].dmg, at);
                }
            }
            continue;
        }
        rocks[i].p = n;
    }
}

/* Called once a frame (from monsters_update). */
static void dragons_tick(Game *g, float dt) {
    static float last;
    if (g->time == last) return;
    last = g->time;
    rocks_update(g, dt);
    for (int i = 0; i < MAX_PLAYERS; i++) g->players[i].shake = fmaxf(0.0f, g->players[i].shake - dt * 1.5f);
}

/* ---------- spawning ---------- */

Monster *dragon_spawn(Game *g, int kind, int size, vec3 pos, int level, u64 key) {
    const DragonDef *D = &DRAGON_KINDS[kind];
    const DragonSize *S = &DRAGON_SIZES[size];
    Monster *m = monster_spawn(g, MON_DRAGON, pos, level, false, rgb3(D->body), key);
    if (!m) return NULL;
    m->dkind = (u8)kind;
    m->dsize = (u8)size;
    int np = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) np += g->players[i].active;
    float party = 1.0f + 0.5f * (float)MAXI(0, np - 1);
    float lv = m->level;
    m->scale = S->scale;
    m->maxhp = m->hp = MONSTERS[MON_DRAGON].hp * (1.0f + 0.18f * lv) * S->hp * party;
    m->dmg = MONSTERS[MON_DRAGON].dmg * (1.0f + 0.14f * lv) * S->dmg;
    m->xp = MONSTERS[MON_DRAGON].xp * (1.0f + 0.12f * lv) * fmaxf(0.3f, S->hp * 0.4f);
    m->color = rgb3(D->body);
    m->color2 = rgb3(D->belly);
    m->glow = rgb3(D->glow);
    m->dhome = pos;
    m->heads = (D->traits & DT_HEADS) ? (u8)(3 + key % 3) : (D->traits & DT_TWIN) ? 2 : 1;
    m->dbreath = S->breath_every * (0.5f + frnd(&g_drng));
    m->dstate = great(m) ? DST_SLEEP : DST_IDLE;
    m->awake = !great(m) && size != DS_DRAKE ? false : false;
    m->element = (u8)(D->breath == BR_FIRE || D->breath == BR_MAGMA ? EL_FIRE : D->breath == BR_FROST ? EL_FROST : D->breath == BR_STORM ? EL_LIGHTNING :
                      D->breath == BR_GAS || D->breath == BR_ROT || D->breath == BR_ACID ? EL_POISON : D->breath == BR_HOLY || D->breath == BR_LIGHT ? EL_HOLY :
                      D->breath == BR_SHADOW || D->breath == BR_NECRO || D->breath == BR_VOID ? EL_SHADOW : EL_ARCANE);
    if (great(m)) {
        m->grade = GRADE_BOSS;
        m->elite = true;
        dragon_name(kind, key, m->name, sizeof m->name);
        if (size == DS_ANCIENT) { char tmp[STORY_NAME]; snprintf(tmp, sizeof tmp, "%s, ancient", m->name); snprintf(m->name, sizeof m->name, "%s", tmp); }
        upper(m->name);
        /* the highest it can fly here: the roof over its home */
        int y = fl(pos.y) + 2, top = y;
        while (top < y + 30 && !game_solid(g, fl(pos.x) + 8, top, fl(pos.z))) top++;
        m->dalt = fmaxf(4.0f, (float)(top - fl(pos.y)) - MONSTERS[MON_DRAGON].h * m->scale - 2.0f);
    } else m->dalt = size == DS_WYVERN ? 6.0f : 1.5f;
    return m;
}

static bool slain(Game *g, u64 key) {
    for (int i = 0; i < g->world.ndragons_slain; i++)
        if (g->world.dragons_slain[i] == key) return true;
    return false;
}

static bool lair_of(const Chunk *c, Lair *L) {
    return c && c->info.has_struct && c->info.st.type == STRUCT_HALL && c->info.st.variant == HALL_LAIR && lair_from_structure(&c->info.st, L);
}

/* The hoard: gold to walk through, chests, the world's legendaries and a dragon-slayer's blade;
 * each piece keyed, so what is taken stays taken. */
static void hoard(Game *g, const Lair *L, int level) {
    u64 ck = chunk_spawn_key(floordiv(L->cx, CHUNK), floordiv(L->floor, CHUNK), floordiv(L->cz, CHUNK));
    float y = (float)L->floor + 3.2f;
    int piles = L->size == DS_ANCIENT ? 24 : 14;
    for (int k = 0; k < piles; k++) {
        u64 key = mix64(L->key ^ (u64)(k + 1) * 0x601D) | 1;
        if (looted_has(g, key) || pickup_find_key(g, key)) continue;
        float a = k * 2.4f, r = 1.5f + (k % 5) * 1.3f;
        pickup_spawn(g, PK_GOLD, v3(L->cx + 0.5f + cosf(a) * r, y - (r > 5 ? 1.0f : 0.0f), L->cz + 0.5f + sinf(a) * r), 60 + level * 12 + (int)(mix64(key) % 80), NULL, key, ck);
    }
    int chests = L->size == DS_ANCIENT ? 6 : 3;
    for (int k = 0; k < chests; k++) {
        u64 key = mix64(L->key ^ (u64)(k + 1) * 0xC4E5) | 1;
        if (looted_has(g, key) || pickup_find_key(g, key)) continue;
        float a = k * 6.28f / chests + 0.3f;
        pickup_spawn(g, PK_CHEST, v3(L->cx + 0.5f + cosf(a) * 9.0f, (float)L->floor, L->cz + 0.5f + sinf(a) * 9.0f), level + 4, NULL, key, ck);
    }
    /* the world's legendaries not yet found: one, or two for an ancient */
    int want = L->size == DS_ANCIENT ? 2 : 1, got = 0;
    for (int t = 0; t < UNIQUE_COUNT && got < want; t++) {
        int id = (int)((L->key + (u64)t * 7) % UNIQUE_COUNT);
        u64 key = mix64(g->world.seed ^ 0x11E6ULL ^ (u64)id * 0x9E3779B9ULL) | 1;
        if (looted_has(g, key) || g->sanctum.trophy[id]) continue;
        got++;
        if (pickup_find_key(g, key)) continue;
        Item it = arms_unique(id, (float)level + 4.0f);
        pickup_spawn(g, PK_WEAPON, v3(L->cx + 0.5f + got, y + 0.5f, L->cz + 0.5f), 0, &it, key, ck);
    }
    /* a blade of a dragon-slayer before you, of the dragon's element */
    u64 key = mix64(L->key ^ 0xB1ADE) | 1;
    if (!looted_has(g, key) && !pickup_find_key(g, key)) {
        int tier, region = world_region(&g->world, floordiv(L->cx, CHUNK), floordiv(L->floor, CHUNK), floordiv(L->cz, CHUNK), &tier);
        Item it = item_roll_region(mix64(key), (float)level + 6.0f, RAR_LEGENDARY, tier, region);
        pickup_spawn(g, PK_WEAPON, v3(L->cx + 0.5f, y + 0.5f, L->cz - 1.5f), 0, &it, key, ck);
    }
}

bool dragon_spawn_chunk(Game *g, Chunk *c) {
    Lair L;
    if (!lair_of(c, &L)) return false;
    if (c->cx != floordiv(L.cx, CHUNK) || c->cz != floordiv(L.cz, CHUNK)) return true; /* (the rest of the lair: nothing else lives here) */
    int level = c->info.level + (L.size == DS_ANCIENT ? 6 : 3);
    hoard(g, &L, level);
    if (slain(g, L.key)) return true;
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (g->mons[i].alive && g->mons[i].type == MON_DRAGON && g->mons[i].lair_key == L.key && great(&g->mons[i])) return true;
    /* asleep on its hoard */
    Monster *m = dragon_spawn(g, L.kind, L.size, v3(L.cx + 0.5f, (float)L.floor + 3.0f, L.cz + 0.5f), level, L.key);
    if (m) {
        m->lair_key = L.key;
        m->dhome = v3(L.cx + 0.5f, (float)L.floor, L.cz + 0.5f);
        dragon_name(L.kind, L.key, m->name, sizeof m->name);
        if (L.size == DS_ANCIENT) { char tmp[STORY_NAME]; snprintf(tmp, sizeof tmp, "%s, ancient", m->name); snprintf(m->name, sizeof m->name, "%s", tmp); }
        upper(m->name);
    }
    /* its brood on the perches */
    int brood = L.size == DS_ANCIENT ? 4 : 2;
    for (int k = 0; k < brood; k++) {
        vec3 p = v3(L.perch_x[k % 4] + 0.5f, (float)(L.floor + L.perch_h[k % 4] + 1), L.perch_z[k % 4] + 0.5f);
        Monster *b = dragon_spawn(g, L.kind, (k & 1) ? DS_DRAKE : DS_WYVERN, p, c->info.level, L.key ^ (u64)(k + 1));
        if (b) b->lair_key = L.key;
    }
    return true;
}

bool dragon_pack(Game *g, Chunk *c, u32 seed) {
    const ChunkInfo *ci = &c->info;
    int level = ci->level;
    if (level < 5) return false;
    u64 h = mix64(ci->seed ^ 0xD1A6ULL ^ seed);
    if ((h % 1000) >= 55) return false; /* about one chunk in eighteen */
    int kind = ci->arch == ARCH_REALM ? dragon_kind_of_realm(realm_of_flavor(ci->flavor)) : dragon_kind_for(ci->stratum, h);
    if (kind < 0 || kind == DK_HYDRA || kind == DK_WYRM) kind = kind == DK_WYRM ? DK_SAND : kind == DK_HYDRA ? DK_GREEN : -1;
    if (kind < 0) return false;
    int size = level >= 12 && ((h >> 12) % 3 == 0) ? DS_WYVERN : ((h >> 16) & 1) ? DS_DRAKE : DS_DRAGONET;
    const DragonSize *S = &DRAGON_SIZES[size];
    int n = S->group_lo + (int)((h >> 20) % (u64)(S->group_hi - S->group_lo + 1));
    u64 ck = chunk_spawn_key(c->cx, c->cy, c->cz);
    int made = 0;
    for (int k = 0; k < n * 6 && made < n; k++) {
        u64 r = mix64(h ^ (u64)k * 0x51);
        int x = c->cx * CHUNK + 2 + (int)(r % 28), z = c->cz * CHUNK + 2 + (int)((r >> 8) % 28);
        for (int y = c->cy * CHUNK + 29; y > c->cy * CHUNK + 1; y--) {
            if (game_solid(g, x, y - 1, z) != 1 || game_solid(g, x, y, z) || game_solid(g, x, y + 1, z) || game_solid(g, x, y + 2, z)) continue;
            vec3 p = v3(x + 0.5f, (float)y + (S->flies ? 1.5f : 0.0f), z + 0.5f);
            if (game_nearest_player(g, p, 14.0f)) break;
            if (dragon_spawn(g, kind, size, p, level, ck ^ (u64)(k + 1))) made++;
            break;
        }
    }
    return made > 0;
}

/* ---------- breath ---------- */

static int breath_of(Monster *m, int head) {
    const DragonDef *D = KIND(m);
    int b = (head & 1) ? D->breath2 : D->breath;
    if (b == BR_CHAOS) b = (int)(frnd(&g_drng) * (BR_COUNT - 1));
    return b;
}

static int breath_sfx(int b) {
    switch (b) {
    case BR_FIRE: case BR_MAGMA: case BR_HOLY: case BR_STEAM: return SFX_DRAGON_BREATH_FIRE;
    case BR_FROST: case BR_WIND: case BR_VOID: case BR_LIGHT: case BR_FORCE: return SFX_DRAGON_BREATH_FROST;
    case BR_STORM: return SFX_DRAGON_BREATH_STORM;
    case BR_GAS: case BR_ROT: case BR_NECRO: case BR_SHADOW: return SFX_DRAGON_BREATH_GAS;
    default: return SFX_DRAGON_BREATH_ACID;
    }
}

static vec3 breath_col(int b) {
    static const vec3 C[BR_COUNT] = {
        { 1.4f, 0.6f, 0.15f }, { 1.3f, 0.4f, 0.1f }, { 0.7f, 0.9f, 1.3f }, { 0.7f, 0.8f, 1.5f }, { 0.5f, 1.1f, 0.3f }, { 0.6f, 1.2f, 0.2f },
        { 0.6f, 0.6f, 0.3f }, { 1.4f, 1.3f, 0.7f }, { 1.2f, 1.3f, 1.4f }, { 0.9f, 0.5f, 1.3f }, { 0.3f, 0.15f, 0.45f }, { 0.4f, 1.1f, 0.6f },
        { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.5f, 0.25f }, { 1.1f, 0.9f, 0.6f }, { 0.4f, 0.8f, 1.2f }, { 0.9f, 0.95f, 1.0f }, { 0.5f, 0.3f, 1.0f }, { 1.3f, 0.4f, 1.2f },
    };
    return C[b];
}

/* A breath from origin along dir: everyone in the cone is hurt by what it is; the world where
 * it lands is changed by it. k scales its damage (a strafe is many short puffs). */
static void breathe(Game *g, Monster *m, vec3 origin, vec3 dir, int b, float k) {
    const DragonSize *S = SIZE(m);
    float range = S->breath_range, cone = S->breath_cone, dmg = m->dmg * 0.9f * k;
    vec3 col = breath_col(b);
    /* the sight of it */
    for (int i = 0; i < (great(m) ? 26 : 8); i++) {
        float t = frnd(&g_drng), sp = cone * t * range;
        vec3 off = v3((frnd(&g_drng) - 0.5f) * sp, (frnd(&g_drng) - 0.5f) * sp * 0.6f, (frnd(&g_drng) - 0.5f) * sp);
        vec3 p = v3_add(v3_add(origin, v3_scale(dir, t * range)), off);
        particles_burst(g, p, col, 1, 1.5f + t * 2.0f, 0.06f + t * 0.12f * (great(m) ? 2.0f : 1.0f), b != BR_SHADOW);
    }
    flash_add(g, v3_add(origin, v3_scale(dir, range * 0.4f)), col, range * 0.6f, 0.2f);
    /* who it reaches */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->st.dead) continue;
        vec3 eye = player_eye(&ps->pl), v = v3_sub(eye, origin);
        float d = v3_len(v);
        if (d > range || d < 0.01f) continue;
        if (v3_dot(v3_scale(v, 1.0f / d), dir) < cosf(cone)) continue;
        if (!phys_line_of_sight(origin, eye, game_solid, g)) continue;
        Stats *s = &ps->st;
        float hit = dmg * (1.0f - d / range * 0.4f);
        switch (b) {
        case BR_FIRE: case BR_MAGMA: case BR_HOLY: s->burn_t = fmaxf(s->burn_t, 3.0f); s->burn_dps = fmaxf(s->burn_dps, hit * 0.12f); break;
        case BR_STEAM: s->burn_t = fmaxf(s->burn_t, 1.5f); s->burn_dps = fmaxf(s->burn_dps, hit * 0.08f); s->wet = 1.0f; break;
        case BR_FROST: s->chill_t = fmaxf(s->chill_t, 3.0f); s->cold = fminf(1.0f, s->cold + 0.15f); break;
        case BR_STORM: hit *= 1.0f + s->wet; break;
        case BR_GAS: case BR_ROT: case BR_NECRO: s->poison_t = fmaxf(s->poison_t, 4.0f); s->poison_dps = fmaxf(s->poison_dps, hit * 0.1f); break;
        case BR_ACID: hit *= 1.25f; break;
        case BR_FORCE: case BR_WIND: case BR_WAVE: ps->pl.vel = v3_add(ps->pl.vel, v3_add(v3_scale(dir, 13.0f), v3(0, 4.0f, 0))); break;
        case BR_VOID: ps->pl.vel = v3_add(ps->pl.vel, v3_scale(dir, -9.0f)); break;
        case BR_SAND: s->chill_t = fmaxf(s->chill_t, 2.0f); break; /* blinded, slowed */
        case BR_RUST: s->chill_t = fmaxf(s->chill_t, 1.0f); if (ps->banner_time <= 0) game_banner(ps, "RUST BITES YOUR ARMOUR"); break;
        default: break;
        }
        tele_src(HS_BLAST, MON_DRAGON, m->grade, m->rank);
        game_hurt_player(g, ps, hit, origin);
    }
    /* where it lands */
    vec3 p = origin;
    bool hit_wall = false;
    for (float t = 1.0f; t < range; t += 0.7f) {
        vec3 q = v3_add(origin, v3_scale(dir, t));
        if (game_solid(g, fl(q.x), fl(q.y), fl(q.z))) { hit_wall = true; break; }
        p = q;
    }
    vec3 mid = v3_add(origin, v3_scale(dir, range * 0.5f));
    float r = frnd(&g_drng);
    switch (b) {
    case BR_FIRE: case BR_HOLY: sim_element_hit(g, p, EL_FIRE, -1, dmg * 0.5f); break;
    case BR_FROST: sim_element_hit(g, p, EL_FROST, -1, dmg * 0.5f); sim_element_hit(g, mid, EL_FROST, -1, dmg * 0.3f); break;
    case BR_STORM: sim_element_hit(g, p, EL_LIGHTNING, -1, dmg * 0.5f); break;
    case BR_GAS: sim_gas(g, GAS_POISON, p, 0.5f * k + 0.1f); break;
    case BR_ROT: sim_gas(g, GAS_SWAMP, p, 0.5f * k + 0.1f); break;
    case BR_STEAM: sim_gas(g, GAS_STEAM, p, 0.5f * k + 0.1f); break;
    case BR_MAGMA: case BR_ACID:
        if (hit_wall || r < 0.5f) {
            int x = fl(p.x), y = fl(p.y), z = fl(p.z);
            for (int dy = 0; dy < 6 && !game_solid(g, x, y - 1, z); dy++) y--;
            bool known;
            Chunk *c = chunkmap_get(&g->map, floordiv(x, CHUNK), floordiv(y, CHUNK), floordiv(z, CHUNK));
            if (chunkmap_block(&g->map, x, y, z, &known) == B_AIR && !town_chunk(c) && !dig_protected(g, x, y, z))
                chunkmap_edit(&g->map, x, y, z, b == BR_MAGMA ? B_LAVA : B_ACID, 2, 0);
            if (b == BR_MAGMA) sim_element_hit(g, p, EL_FIRE, -1, dmg * 0.4f);
        }
        break;
    case BR_NECRO:
        if (r < 0.35f * k) { /* the dead rise where it breathed */
            int n = 0;
            for (int i = 0; i < MAX_MONSTERS; i++) n += g->mons[i].alive && g->mons[i].type == MON_SKELETON && v3_len(v3_sub(g->mons[i].pos, m->pos)) < 40.0f;
            if (n < 6 && !game_solid(g, fl(p.x), fl(p.y), fl(p.z)) && game_solid(g, fl(p.x), fl(p.y) - 1, fl(p.z))) {
                Monster *s = monster_spawn(g, MON_SKELETON, v3(p.x, (float)fl(p.y), p.z), (int)m->level, false, v3(0.4f, 1.0f, 0.6f), m->chunk_key);
                if (s) { s->awake = true; s->state = MS_CHASE; }
            }
        }
        break;
    default: break;
    }
}

/* ---------- moving ---------- */

static bool free_at(Game *g, vec3 p) { return !game_solid(g, fl(p.x), fl(p.y), fl(p.z)); }

static void fly_to(Game *g, Monster *m, vec3 goal, float speed, float dt) {
    vec3 to = v3_sub(goal, m->pos);
    float d = v3_len(to);
    vec3 want = d > 0.2f ? v3_scale(to, fminf(speed, d * 2.0f) / d) : v3(0, 0, 0);
    m->vel = v3_lerp(m->vel, want, fminf(1.0f, dt * (great(m) ? 1.6f : 4.0f)));
    vec3 n = v3_add(m->pos, v3_scale(m->vel, dt));
    float h = MONSTERS[MON_DRAGON].h * m->scale;
    vec3 c = v3_add(n, v3(0, h * 0.5f, 0));
    vec3 ahead = v3_add(c, v3_scale(v3_norm(v3(m->vel.x, 0, m->vel.z)), MONSTERS[MON_DRAGON].hw * m->scale));
    if (free_at(g, c) && free_at(g, ahead) && free_at(g, n)) m->pos = n;
    else { m->vel = v3_scale(m->vel, -0.3f); m->dtimer = fminf(m->dtimer, 0.3f); }
    if (v3_len(v3(m->vel.x, 0, m->vel.z)) > 0.5f) {
        float want_yaw = atan2f(m->vel.z, m->vel.x), dy = want_yaw - m->yaw;
        while (dy > PI_F) dy -= 2 * PI_F;
        while (dy < -PI_F) dy += 2 * PI_F;
        m->yaw += dy * fminf(1.0f, dt * 3.0f);
    }
}

static bool on_ground(Game *g, Monster *m) { return game_solid(g, fl(m->pos.x), fl(m->pos.y - 0.05f), fl(m->pos.z)) == 1; }

static void walk_to(Game *g, Monster *m, vec3 goal, float speed, float dt) {
    vec3 to = v3_sub(goal, m->pos);
    to.y = 0;
    float d = v3_len(to);
    float sp = d > 0.3f ? fminf(speed, d * 3.0f) : 0.0f;
    vec3 dir = d > 0.01f ? v3_scale(to, 1.0f / d) : v3(cosf(m->yaw), 0, sinf(m->yaw));
    float dy = atan2f(dir.z, dir.x) - m->yaw;
    while (dy > PI_F) dy -= 2 * PI_F;
    while (dy < -PI_F) dy += 2 * PI_F;
    m->yaw += dy * fminf(1.0f, dt * (great(m) ? 2.0f : 6.0f));
    vec3 f = v3(cosf(m->yaw), 0, sinf(m->yaw));
    m->vel.x = f.x * sp * fmaxf(0.0f, cosf(dy)) + m->vel.x * 0.0f;
    m->vel.z = f.z * sp * fmaxf(0.0f, cosf(dy));
    if (!on_ground(g, m)) m->vel.y -= 20.0f * dt;
    else if (m->vel.y < 0) m->vel.y = 0;
    vec3 n = v3_add(m->pos, v3_scale(m->vel, dt));
    int step = great(m) ? 3 : 1;
    float reach = MONSTERS[MON_DRAGON].hw * m->scale * 0.8f;
    vec3 front = v3_add(n, v3_scale(f, reach));
    int fy = fl(m->pos.y + 0.05f);
    if (game_solid(g, fl(front.x), fy, fl(front.z)) || game_solid(g, fl(n.x), fy, fl(n.z))) {
        int up = 0;
        while (up < step && game_solid(g, fl(front.x), fy + up, fl(front.z))) up++;
        if (up < step && !game_solid(g, fl(front.x), fy + up + 1, fl(front.z))) n.y = (float)(fy + up);
        else { n.x = m->pos.x; n.z = m->pos.z; }
    }
    if (game_solid(g, fl(n.x), fl(n.y), fl(n.z)) == 1) n.y = (float)(fl(n.y) + 1);
    m->pos = n;
    if (on_ground(g, m) && m->vel.y <= 0) m->pos.y = fmaxf(m->pos.y, (float)fl(m->pos.y - 0.05f) + 1.0f);
    if (sp > 1.0f && great(m) && (m->dstep -= dt * sp / m->scale) <= 0) { /* its tread */
        m->dstep = 1.2f;
        sfx_at(g, SFX_DRAGON_TREAD, m->pos, m->dsize == DS_ANCIENT ? 1.0f : 0.8f);
        dragons_shake(g, m->pos, m->dsize == DS_ANCIENT ? 0.5f : 0.3f, 30.0f);
    }
}

static PlayerSlot *target(Game *g, Monster *m, float range) {
    PlayerSlot *best = NULL;
    float bd = range;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->st.dead || ps->st.invis_t > 0) continue;
        float d = v3_len(v3_sub(ps->pl.pos, m->pos));
        if (d < bd) { bd = d; best = ps; }
    }
    return best;
}

static vec3 head_pos(const Monster *m) {
    float s = m->scale;
    vec3 f = v3(cosf(m->yaw), 0, sinf(m->yaw));
    bool air = m->dstate == DST_FLY || m->dstate == DST_STRAFE || m->dstate == DST_DIVE || m->dstate == DST_TAKEOFF;
    return v3_add(m->pos, v3_add(v3_scale(f, 1.9f * s), v3(0, (air ? 1.0f : 1.7f) * s, 0)));
}

static void roar(Game *g, Monster *m) {
    sfx_at(g, m->dsize == DS_ANCIENT ? SFX_DRAGON_ROAR_ANCIENT : great(m) ? SFX_DRAGON_ROAR_GREAT : SFX_DRAGON_ROAR_SMALL, m->pos, 1.0f);
    if (great(m)) {
        dragons_shake(g, m->pos, m->dsize == DS_ANCIENT ? 1.0f : 0.6f, 60.0f);
        monsters_alert(g, m->pos, 45.0f);
    }
}

static void wake(Game *g, Monster *m) {
    if (m->dstate != DST_SLEEP && m->awake) return;
    m->awake = true;
    m->dstate = can_fly(m) ? DST_TAKEOFF : DST_GROUND;
    m->dtimer = 2.0f;
    roar(g, m);
    if (great(m)) {
        char buf[96];
        snprintf(buf, sizeof buf, "%s WAKES", m->name);
        game_banner_all(g, buf);
    }
}

/* ---------- a dragon's mind ---------- */

static void small_update(Game *g, Monster *m, PlayerSlot *tp, float dt) {
    const DragonSize *S = SIZE(m);
    float d = tp ? v3_len(v3_sub(tp->pl.pos, m->pos)) : 1e9f;
    if (!m->awake) {
        if (tp && d < (tp->pl.stance == STANCE_STAND ? 13.0f : 6.0f)) { m->awake = true; roar(g, m); }
        else { m->anim += dt * 0.3f; return; }
    }
    if (!tp || d > 45.0f || v3_len(v3_sub(m->pos, m->dhome)) > 50.0f) { /* home again */
        if (S->flies) fly_to(g, m, v3_add(m->dhome, v3(0, 1.0f, 0)), S->speed * 0.5f, dt);
        else walk_to(g, m, m->dhome, S->speed * 0.5f, dt);
        if (!tp || d > 45.0f) m->awake = v3_len(v3_sub(m->pos, m->dhome)) > 2.0f;
        return;
    }
    vec3 eye = player_eye(&tp->pl);
    m->cooldown -= dt;
    m->dbreath -= dt;
    switch (m->dsize) {
    case DS_DRAGONET: { /* flit round you, bite, spit */
        float a = m->anim * 2.0f + (float)(m->chunk_key & 7) + (float)(m - g->mons);
        vec3 goal = v3_add(eye, v3(cosf(a) * 2.5f, 0.3f + sinf(a * 1.7f) * 0.8f, sinf(a) * 2.5f));
        if (m->cooldown <= 0 && d < 3.5f) goal = eye;
        fly_to(g, m, goal, S->speed, dt);
        if (m->cooldown <= 0 && v3_len(v3_sub(eye, v3_add(m->pos, v3(0, 0.2f, 0)))) < 1.3f) {
            m->cooldown = 1.0f + frnd(&g_drng);
            tele_src(HS_MELEE, MON_DRAGON, m->grade, m->rank);
            game_hurt_player(g, tp, m->dmg, m->pos);
            sfx_at(g, SFX_DRAGONET_SCREECH, m->pos, 0.5f);
        }
        break;
    }
    case DS_WYVERN: { /* circle overhead, dive and sting, climb away */
        if (m->dstate == DST_DIVE) {
            fly_to(g, m, v3_add(tp->pl.pos, v3(0, 0.6f, 0)), S->speed * 1.6f, dt);
            if (v3_len(v3_sub(eye, v3_add(m->pos, v3(0, 0.8f, 0)))) < 2.0f) {
                tele_src(HS_MELEE, MON_DRAGON, m->grade, m->rank);
                game_hurt_player(g, tp, m->dmg * 1.3f, m->pos);
                tp->st.poison_t = fmaxf(tp->st.poison_t, 4.0f);
                tp->st.poison_dps = fmaxf(tp->st.poison_dps, m->dmg * 0.1f);
                m->dstate = DST_FLY;
                m->dtimer = 4.0f + frnd(&g_drng) * 3.0f;
                sfx_at(g, SFX_DRAGON_BITE, m->pos, 0.8f);
            }
            if ((m->dtimer -= dt) <= -3.0f) { m->dstate = DST_FLY; m->dtimer = 3.0f; }
        } else {
            float a = m->anim * 0.7f + (float)(m->chunk_key & 7);
            vec3 goal = v3_add(tp->pl.pos, v3(cosf(a) * 8.0f, m->dalt, sinf(a) * 8.0f));
            fly_to(g, m, goal, S->speed, dt);
            if ((m->dtimer -= dt) <= 0) { m->dstate = DST_DIVE; m->dtimer = 0; roar(g, m); }
        }
        break;
    }
    default: /* a drake: charge and bite */
        if (d > 1.8f) walk_to(g, m, tp->pl.pos, S->speed, dt);
        else walk_to(g, m, m->pos, 0, dt);
        if (m->cooldown <= 0 && d < 2.2f) {
            m->cooldown = 1.2f;
            tele_src(HS_MELEE, MON_DRAGON, m->grade, m->rank);
            game_hurt_player(g, tp, m->dmg, m->pos);
            sfx_at(g, SFX_DRAGON_BITE, m->pos, 0.7f);
        }
        break;
    }
    /* breath */
    if (m->dbreath <= 0 && d < S->breath_range * 0.9f && phys_line_of_sight(head_pos(m), eye, game_solid, g)) {
        m->dbreath = S->breath_every * (0.8f + frnd(&g_drng) * 0.4f);
        int b = breath_of(m, 0);
        vec3 o = head_pos(m);
        if (m->dsize == DS_DRAGONET) o = v3_add(m->pos, v3(0, 0.2f, 0));
        breathe(g, m, o, v3_norm(v3_sub(eye, o)), b, 0.8f);
        sfx_at(g, breath_sfx(b), m->pos, m->dsize == DS_DRAGONET ? 0.4f : 0.7f);
    }
}

static void great_update(Game *g, Monster *m, PlayerSlot *tp, float dt) {
    const DragonDef *D = KIND(m);
    const DragonSize *S = SIZE(m);
    float d = tp ? v3_len(v3_sub(tp->pl.pos, m->pos)) : 1e9f;
    /* asleep on its hoard: too close, too loud, a step on the gold */
    if (m->dstate == DST_SLEEP) {
        m->anim += dt * 0.2f;
        if (tp) {
            float hear = tp->pl.stance == STANCE_STAND ? 12.0f : 5.0f;
            float hoard = v3_len(v3_sub(tp->pl.pos, m->dhome));
            if (d < hear || (hoard < 8.0f && frnd(&g_drng) < dt * (tp->pl.stance == STANCE_STAND ? 0.6f : 0.12f))) wake(g, m);
        }
        return;
    }
    /* the phase of the fight */
    float frac = m->hp / m->maxhp;
    int phase = frac > 0.66f ? 0 : frac > 0.33f ? 1 : 2;
    if (phase > m->dphase) {
        m->dphase = (u8)phase;
        roar(g, m);
        char buf[128];
        snprintf(buf, sizeof buf, phase == 1 ? "%s IS FURIOUS" : "%s IS DESPERATE - THE ROOF IS COMING DOWN", m->name);
        game_banner_all(g, buf);
        if (phase == 1 && can_fly(m)) { m->dstate = DST_LAND; m->dtimer = 3.0f; }
    }
    /* gold in the light mends itself */
    if ((D->traits & DT_LIGHTHEAL) && (m->dheal += dt) > 3.0f) m->hp = fminf(m->maxhp, m->hp + m->maxhp * 0.012f * dt);
    /* the lair comes down */
    if (m->dphase == 2 && tp && frnd(&g_drng) < dt * 0.9f) {
        vec3 at = v3_add(tp->pl.pos, v3((frnd(&g_drng) - 0.5f) * 8.0f, 0, (frnd(&g_drng) - 0.5f) * 8.0f));
        rock_drop(g, at, m->dmg * 0.7f);
        dragons_shake(g, at, 0.3f, 20.0f);
    }
    /* too far from its lair, or nobody to fight: home, and it heals */
    bool leash = v3_len(v3(m->pos.x - m->dhome.x, 0, m->pos.z - m->dhome.z)) > 48.0f;
    if (!tp || d > 70.0f || leash) {
        if (can_fly(m)) fly_to(g, m, v3_add(m->dhome, v3(0, 4.0f, 0)), S->speed * 0.6f, dt);
        else walk_to(g, m, m->dhome, S->speed * 0.6f, dt);
        m->hp = fminf(m->maxhp, m->hp + m->maxhp * 0.01f * dt);
        if (!tp || d > 70.0f) {
            if (v3_len(v3_sub(m->pos, m->dhome)) < 6.0f && m->hp >= m->maxhp * 0.99f) { m->dstate = DST_SLEEP; m->awake = false; m->dphase = 0; }
        }
        return;
    }
    vec3 eye = player_eye(&tp->pl);
    vec3 f = v3(cosf(m->yaw), 0, sinf(m->yaw));
    m->dtimer -= dt;
    m->cooldown -= dt;
    m->dbreath -= dt;
    float speed = S->speed * (m->dphase == 1 ? 1.25f : 1.0f);
    float base_y = m->dhome.y;
    switch (m->dstate) {
    case DST_TAKEOFF:
        fly_to(g, m, v3(m->pos.x + f.x * 4.0f, base_y + m->dalt, m->pos.z + f.z * 4.0f), speed * 0.7f, dt);
        if ((m->dtimer -= 0) <= 0 || m->pos.y > base_y + m->dalt * 0.8f) { m->dstate = DST_FLY; m->dtimer = 3.0f + frnd(&g_drng) * 3.0f; }
        if (fmodf(m->anim, 0.9f) < dt) sfx_at(g, SFX_DRAGON_WINGS, m->pos, 0.9f);
        break;
    case DST_FLY: { /* circle the player, high */
        float a = m->anim * 0.35f;
        vec3 goal = v3(tp->pl.pos.x + cosf(a) * 18.0f, base_y + m->dalt, tp->pl.pos.z + sinf(a) * 18.0f);
        if (v3_len(v3(goal.x - m->dhome.x, 0, goal.z - m->dhome.z)) > 40.0f) goal = v3(m->dhome.x + cosf(a) * 18.0f, base_y + m->dalt, m->dhome.z + sinf(a) * 18.0f);
        fly_to(g, m, goal, speed, dt);
        if (fmodf(m->anim, 1.1f) < dt) sfx_at(g, SFX_DRAGON_WINGS, m->pos, 0.7f);
        if (m->dtimer <= 0) {
            float r = frnd(&g_drng);
            if (!can_fly(m)) m->dstate = DST_LAND;
            else if (r < 0.45f) { /* a breath run along a line through you */
                m->dstate = DST_STRAFE;
                vec3 across = v3_sub(tp->pl.pos, m->pos);
                across.y = 0;
                m->dgoal = v3_add(tp->pl.pos, v3_add(v3_scale(v3_norm(across), 20.0f), v3(0, m->dalt * 0.55f, 0)));
                m->dtimer = 4.0f;
                sfx_at(g, SFX_DRAGON_INHALE, m->pos, 1.0f);
            } else if (r < 0.75f || m->dphase >= 1) { m->dstate = DST_LAND; m->dtimer = 4.0f; }
            else { m->dstate = DST_INHALE; m->dtimer = 0.9f; sfx_at(g, SFX_DRAGON_INHALE, m->pos, 1.0f); }
        }
        break;
    }
    case DST_STRAFE: {
        fly_to(g, m, m->dgoal, speed * 1.3f, dt);
        vec3 down = v3_norm(v3_add(v3_scale(f, 1.0f), v3(0, -1.2f, 0)));
        if (fmodf(m->anim, 0.22f) < dt) {
            int b = breath_of(m, (int)(m->anim * 4.0f) % MAXI(1, m->heads));
            breathe(g, m, head_pos(m), down, b, 0.45f);
            if (fmodf(m->anim, 0.9f) < dt) sfx_at(g, breath_sfx(b), m->pos, 1.0f);
        }
        if (m->dtimer <= 0 || v3_len(v3_sub(m->dgoal, m->pos)) < 3.0f) { m->dstate = DST_FLY; m->dtimer = 3.0f + frnd(&g_drng) * 3.0f; }
        break;
    }
    case DST_LAND: {
        vec3 goal = v3_add(tp->pl.pos, v3_scale(v3_norm(v3_sub(m->pos, tp->pl.pos)), 9.0f));
        goal.y = base_y;
        if (!can_fly(m)) { m->vel.y -= 20.0f * dt; m->pos = v3_add(m->pos, v3_scale(m->vel, dt)); }
        else fly_to(g, m, goal, speed * 0.8f, dt);
        if (on_ground(g, m) || m->pos.y < base_y + 0.6f) {
            m->dstate = DST_GROUND;
            m->dtimer = 8.0f + frnd(&g_drng) * 6.0f + m->dphase * 4.0f;
            dragons_shake(g, m->pos, 0.6f, 40.0f);
            sfx_at(g, SFX_DRAGON_TREAD, m->pos, 1.0f);
        }
        break;
    }
    case DST_GROUND: {
        vec3 to = v3_sub(tp->pl.pos, m->pos);
        to.y = 0;
        float reach = MONSTERS[MON_DRAGON].hw * m->scale + 2.0f;
        float facing = v3_dot(v3_norm(to), f);
        if (d > reach * 0.9f) walk_to(g, m, tp->pl.pos, speed * 0.6f, dt);
        else walk_to(g, m, m->pos, 0, dt);
        if (m->cooldown <= 0 && d < reach + 1.5f) {
            m->cooldown = 1.6f - m->dphase * 0.3f;
            if (facing > 0.3f) { /* bite and claw */
                tele_src(HS_MELEE, MON_DRAGON, m->grade, m->rank);
                game_hurt_player(g, tp, m->dmg, m->pos);
                sfx_at(g, SFX_DRAGON_BITE, m->pos, 1.0f);
            } else if (facing < -0.3f) { /* the tail sweeps behind it */
                tele_src(HS_MELEE, MON_DRAGON, m->grade, m->rank);
                game_hurt_player(g, tp, m->dmg * 0.8f, m->pos);
                tp->pl.vel = v3_add(tp->pl.vel, v3_add(v3_scale(v3_norm(to), 11.0f), v3(0, 5.0f, 0)));
                sfx_at(g, SFX_DRAGON_TAIL, m->pos, 1.0f);
            } else { /* a stomp: the ground shakes, stones fall */
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    PlayerSlot *ps = &g->players[i];
                    if (ps->active && !ps->st.dead && v3_len(v3_sub(ps->pl.pos, m->pos)) < reach + 3.0f && ps->pl.on_ground) {
                        tele_src(HS_BLAST, MON_DRAGON, m->grade, m->rank);
                        game_hurt_player(g, ps, m->dmg * 0.6f, m->pos);
                        ps->pl.vel.y += 6.0f;
                    }
                }
                dragons_shake(g, m->pos, 0.9f, 35.0f);
                sfx_at(g, SFX_DRAGON_TREAD, m->pos, 1.0f);
                if (frnd(&g_drng) < 0.5f) rock_drop(g, tp->pl.pos, m->dmg * 0.5f);
            }
        }
        if (m->dbreath <= 0 && d > reach * 0.6f) { m->dstate = DST_INHALE; m->dtimer = 0.9f - m->dphase * 0.15f; sfx_at(g, SFX_DRAGON_INHALE, m->pos, 1.0f); }
        /* burrowers go under; the stealthy vanish */
        if ((D->traits & DT_WINGLESS) && m->dkind == DK_WYRM && m->dtimer < 4.0f && frnd(&g_drng) < dt * 0.4f) { m->dstate = DST_BURROW; m->dtimer = 2.5f; m->hidden = 1; dragons_shake(g, m->pos, 0.5f, 30.0f); }
        else if ((D->traits & DT_STEALTH) && frnd(&g_drng) < dt * 0.08f) { m->dstate = DST_VANISH; m->dtimer = 2.0f; m->hidden = 1; }
        else if (m->dtimer <= 0 && can_fly(m) && m->dphase < 2) { m->dstate = DST_TAKEOFF; m->dtimer = 2.0f; roar(g, m); }
        break;
    }
    case DST_INHALE: { /* the throat glows, the head rears: then the breath */
        if (on_ground(g, m)) walk_to(g, m, tp->pl.pos, 0.01f, dt);
        else fly_to(g, m, m->pos, 0, dt);
        if (m->dtimer <= 0) {
            vec3 o = head_pos(m);
            for (int h = 0; h < MAXI(1, m->heads); h++) {
                int b = breath_of(m, h);
                vec3 dir = v3_norm(v3_sub(eye, o));
                if (m->heads > 1) dir = v3_norm(v3_add(dir, v3(-dir.z * (h - (m->heads - 1) * 0.5f) * 0.12f, 0, dir.x * (h - (m->heads - 1) * 0.5f) * 0.12f)));
                breathe(g, m, o, dir, b, 1.0f + m->dphase * 0.2f);
                if (h == 0) sfx_at(g, breath_sfx(b), m->pos, 1.0f);
            }
            m->dbreath = S->breath_every * (0.8f + frnd(&g_drng) * 0.5f) - m->dphase;
            m->dstate = on_ground(g, m) ? DST_GROUND : DST_FLY;
            if (m->dstate == DST_GROUND) m->dtimer = fmaxf(m->dtimer, 3.0f);
            else m->dtimer = 2.0f;
        }
        break;
    }
    case DST_BURROW: /* under the ground toward you, then up beneath your feet */
        m->pos.x += (tp->pl.pos.x - m->pos.x) * fminf(1.0f, dt * 1.5f);
        m->pos.z += (tp->pl.pos.z - m->pos.z) * fminf(1.0f, dt * 1.5f);
        if (fmodf(m->anim, 0.3f) < dt) { particles_burst(g, v3(m->pos.x, tp->pl.pos.y + 0.1f, m->pos.z), v3(0.5f, 0.4f, 0.3f), 6, 2.0f, 0.08f, false); dragons_shake(g, m->pos, 0.3f, 20.0f); }
        if (m->dtimer <= 0) {
            m->hidden = 0;
            m->pos.y = tp->pl.pos.y;
            m->dstate = DST_GROUND;
            m->dtimer = 6.0f;
            roar(g, m);
            for (int i = 0; i < MAX_PLAYERS; i++) {
                PlayerSlot *ps = &g->players[i];
                if (ps->active && !ps->st.dead && v3_len(v3_sub(ps->pl.pos, m->pos)) < 4.0f) {
                    tele_src(HS_BLAST, MON_DRAGON, m->grade, m->rank);
                    game_hurt_player(g, ps, m->dmg * 1.4f, m->pos);
                    ps->pl.vel.y += 9.0f;
                }
            }
            particles_burst(g, m->pos, v3(0.5f, 0.4f, 0.3f), 30, 6.0f, 0.12f, false);
        }
        break;
    case DST_VANISH: /* gone, and back behind you */
        if (m->dtimer <= 0) {
            vec3 back = v3_sub(tp->pl.pos, v3_scale(dir_from_angles(tp->pl.yaw, 0), 6.0f));
            if (free_at(g, v3_add(back, v3(0, 1.0f, 0)))) m->pos = v3(back.x, tp->pl.pos.y, back.z);
            m->hidden = 0;
            m->yaw = tp->pl.yaw;
            m->dstate = DST_GROUND;
            m->dtimer = 6.0f;
            m->cooldown = 0.4f;
            roar(g, m);
        }
        break;
    default: m->dstate = can_fly(m) ? DST_FLY : DST_GROUND; break;
    }
}

void dragon_update(Game *g, Monster *m, float dt) {
    dragons_tick(g, dt);
    m->anim += dt;
    m->flash = fmaxf(0.0f, m->flash - dt * 4.0f);
    m->air_used = 0;
    /* fire and poison on it, as on any monster */
    float dot = 0;
    if (m->burn_t > 0) { m->burn_t -= dt; dot += m->burn_dps * dt; }
    if (m->poison_t > 0) { m->poison_t -= dt; dot += m->poison_dps * dt; }
    if (m->slow_t > 0) m->slow_t -= dt;
    if (dot > 0) {
        m->hp -= dot;
        if (m->hp <= 0) { monster_damage(g, m, 1.0f, m->pos, 0, m->last_hitter); return; }
    }
    PlayerSlot *tp = target(g, m, 90.0f);
    if (tp) m->target = tp->index;
    if (great(m)) great_update(g, m, tp, dt);
    else small_update(g, m, tp, dt);
    if (m->pos.y < m->dhome.y - 60.0f) m->pos = m->dhome; /* (fell out of the world) */
}

/* ---------- being hurt, dying ---------- */

float dragon_damage(Game *g, Monster *m, float dmg, vec3 from, int attacker) {
    if (m->dstate == DST_SLEEP || !m->awake) wake(g, m);
    m->awake = true;
    m->dheal = 0;
    const DragonDef *D = KIND(m);
    if ((D->traits & DT_ARMOR) && m->dphase == 0) dmg *= 0.5f; /* plates, until they break */
    bool air = m->dstate == DST_FLY || m->dstate == DST_STRAFE || m->dstate == DST_DIVE || m->dstate == DST_TAKEOFF;
    if (air && from.y < m->pos.y - 0.5f) dmg *= 1.4f; /* from below: the soft belly */
    if (air && great(m) && !(D->traits & DT_WINGLESS) && !m->grounded) {
        m->dwing += dmg;
        if (m->dwing > m->maxhp * 0.22f) {
            m->grounded = 1;
            m->dstate = DST_LAND;
            char buf[128];
            snprintf(buf, sizeof buf, "%s'S WINGS ARE TORN - IT CANNOT FLY", m->name);
            game_banner_all(g, buf);
            roar(g, m);
        }
    }
    (void)attacker;
    return dmg;
}

bool dragon_rises(Game *g, Monster *m) {
    if (!(KIND(m)->traits & DT_REVIVE) || m->revived || m->burn_t > 0) return false;
    m->revived = 1;
    m->hp = m->maxhp * 0.4f;
    m->dphase = 1;
    roar(g, m);
    particles_burst(g, v3_add(m->pos, v3(0, m->scale, 0)), v3(0.4f, 1.0f, 0.6f), 40, 5.0f, 0.1f, true);
    game_banner_all(g, "IT RISES AGAIN - ONLY FIRE ENDS A DRACOLICH");
    return true;
}

void dragon_slain(Game *g, Monster *m) {
    vec3 at = v3_add(m->pos, v3(0, 0.8f, 0));
    const DragonDef *D = KIND(m);
    int el = m->element < EL_COUNT ? m->element : EL_FIRE;
    if (great(m)) {
        bool ancient = m->dsize == DS_ANCIENT;
        sfx_at(g, SFX_DRAGON_DEATH, m->pos, 1.0f);
        dragons_shake(g, m->pos, 1.0f, 60.0f);
        for (int k = 0; k < (ancient ? 10 : 6); k++) pickup_spawn(g, PK_GOLD, v3_add(at, v3(cosf(k * 1.3f) * 2.0f, 0, sinf(k * 1.3f) * 2.0f)), 50 + (int)m->level * 10, NULL, 0, 0);
        for (int k = 0; k < (ancient ? 3 : 2); k++) loot_drop(g, m->pos, m->level + 4.0f, true, 1.0f);
        /* what a dragon is made of: its heart, its scales, its teeth and its blood */
        pickup_spawn(g, PK_FORGE, at, FG_TITAN_HEART, NULL, 0, 0);
        for (int k = 0; k < (ancient ? 6 : 3); k++) pickup_spawn(g, PK_FORGE, v3_add(at, v3(0.5f * k, 0, 0)), FG_ESS0 + el, NULL, 0, 0);
        for (int k = 0; k < 2; k++) pickup_spawn(g, PK_REAGENT, v3_add(at, v3(0, 0, 0.5f * k)), RG_TOOTH, NULL, 0, 0);
        pickup_spawn(g, PK_REAGENT, at, D->breath == BR_FIRE || D->breath == BR_MAGMA || D->breath == BR_HOLY ? RG_PHOENIX : RG_HEARTSTONE, NULL, 0, 0);
        if (ancient) { int tier, region = world_region(&g->world, floordiv(fl(m->pos.x), CHUNK), floordiv(fl(m->pos.y), CHUNK), floordiv(fl(m->pos.z), CHUNK), &tier);
            Item it = item_roll_region(mix64(m->lair_key ^ 0xA9C1E47), m->level + 8.0f, RAR_LEGENDARY, tier, region);
            pickup_spawn(g, PK_WEAPON, v3_add(at, v3(0, 0.6f, 0)), 0, &it, 0, 0); }
        if (m->lair_key && g->world.ndragons_slain < 64) g->world.dragons_slain[g->world.ndragons_slain++] = m->lair_key;
        sanctum_trophy_named(g, m->lair_key ? m->lair_key : m->chunk_key);
        char buf[128];
        snprintf(buf, sizeof buf, "%s IS SLAIN - DRAGONSLAYER", m->name);
        game_banner_all(g, buf);
        flash_add(g, at, v3_scale(m->glow, 2.0f), 30.0f, 1.0f);
        g_tele.dragons++;
        g->autosave_timer = 1e9f;
    } else {
        if (m->dsize >= DS_DRAKE && (mix64(m->chunk_key ^ (u64)(m - g->mons)) % 3) == 0) pickup_spawn(g, PK_REAGENT, at, RG_TOOTH, NULL, 0, 0);
        if (m->dsize == DS_WYVERN) pickup_spawn(g, PK_FORGE, at, FG_ESS0 + el, NULL, 0, 0);
        g_tele.drakes++;
    }
}

/* ---------- drawing ---------- */

typedef struct DFrame { vec3 o, f, r, u; float s; } DFrame;
static vec3 P(const DFrame *b, float fw, float rt, float up) {
    return v3_add(b->o, v3_add(v3_scale(b->f, fw * b->s), v3_add(v3_scale(b->r, rt * b->s), v3_scale(b->u, up * b->s))));
}
static void BX(const DFrame *b, vec3 c, float hf, float hr, float hu, vec3 col, bool em) {
    batch3d_obox(c, v3_scale(b->f, hf * b->s), v3_scale(b->r, hr * b->s), v3_scale(b->u, hu * b->s), col, em);
}
static vec3 litc(vec3 c, vec3 l) { return v3(c.x * l.x, c.y * l.y, c.z * l.z); }

static void draw_head(const Monster *m, const DFrame *b, vec3 neck0, float lean, float sway, vec3 col, vec3 dark, vec3 glow, float jaw, float throat) {
    float s = b->s;
    vec3 prev = neck0;
    int N = 5;
    for (int i = 1; i <= N; i++) {
        float t = (float)i / N;
        vec3 p = v3_add(neck0, v3_add(v3_scale(b->f, (0.28f * i + lean * 0.05f * i) * s), v3_add(v3_scale(b->u, (0.22f * i * (1.0f - lean)) * s),
                                                                                             v3_scale(b->r, sinf(m->anim * 1.5f + i * 0.6f) * 0.04f * i * s + sway * t * s))));
        float w = 0.24f - 0.06f * t;
        batch3d_obox(v3_scale(v3_add(prev, p), 0.5f), v3_scale(b->f, 0.18f * s), v3_scale(b->r, w * s), v3_scale(b->u, w * s), col, false);
        if (i % 2 == 0) batch3d_spike(v3_add(p, v3_scale(b->u, w * s)), v3_norm(v3_sub(b->u, b->f)), 0.05f * s, 0.25f * s, dark, false);
        prev = p;
    }
    vec3 h = v3_add(prev, v3_scale(b->f, 0.3f * s));
    BX(b, h, 0.36f, 0.19f, 0.15f, col, false);                                                      /* skull */
    BX(b, v3_add(h, v3_add(v3_scale(b->f, 0.18f * s), v3_scale(b->u, -(0.13f + jaw * 0.12f) * s))), 0.28f, 0.16f, 0.06f, dark, false); /* jaw */
    for (int k = -1; k <= 1; k += 2) {
        batch3d_box(v3_add(h, v3_add(v3_scale(b->f, 0.16f * s), v3_add(v3_scale(b->r, 0.16f * k * s), v3_scale(b->u, 0.07f * s)))), v3(0.035f * s, 0.03f * s, 0.035f * s), 0, glow, true);
        batch3d_spike(v3_add(h, v3_add(v3_scale(b->f, -0.25f * s), v3_add(v3_scale(b->r, 0.12f * k * s), v3_scale(b->u, 0.12f * s)))),
                      v3_norm(v3_add(v3_scale(b->f, -1.0f), v3_add(v3_scale(b->u, 0.7f), v3_scale(b->r, 0.25f * k)))), 0.06f * s, 0.55f * s, dark, false); /* horns */
    }
    if (jaw > 0.3f) batch3d_octa(v3_add(h, v3_scale(b->f, 0.45f * s)), 0.1f * s * jaw, 0.1f * s * jaw, m->anim * 8.0f, glow, true);
    if (throat > 0) batch3d_octa(v3_add(neck0, v3_scale(b->u, 0.1f * s)), 0.25f * s * throat, 0.25f * s * throat, m->anim * 4.0f, glow, true);
}

static void draw_wing(const Monster *m, const DFrame *b, int k, bool air, vec3 col, vec3 dark) {
    float s = b->s;
    float flap = air ? sinf(m->anim * (great(m) ? 3.0f : 9.0f)) * 0.75f : 0.0f;
    float raise = air ? 0.15f + flap : 1.1f;
    float span = air ? 2.1f : 1.1f;
    vec3 root = P(b, 0.15f, 0.3f * k, 0.35f);
    vec3 arm = v3_norm(v3_add(v3_scale(b->r, (float)k * cosf(raise)), v3_scale(b->u, sinf(raise))));
    vec3 elbow = v3_add(root, v3_scale(arm, span * 0.5f * s));
    vec3 tip = v3_add(elbow, v3_add(v3_scale(arm, span * 0.5f * s), v3_scale(b->f, -0.2f * s)));
    batch3d_spike(root, arm, 0.07f * s, span * 0.5f * s, dark, false);
    batch3d_spike(elbow, v3_norm(v3_sub(tip, elbow)), 0.05f * s, span * 0.5f * s, dark, false);
    /* the membrane between the arm and the body, in three panels */
    for (int p = 0; p < 3; p++) {
        float t = (p + 0.5f) / 3.0f;
        vec3 along = v3_add(root, v3_scale(v3_sub(tip, root), t));
        float back = (0.7f - 0.15f * p) * s * (air ? 1.0f : 0.6f);
        vec3 c = v3_add(along, v3_scale(b->f, -back * 0.5f));
        vec3 ax = v3_scale(v3_sub(tip, root), 1.0f / 6.0f);
        batch3d_obox(c, ax, v3_scale(b->f, back * 0.5f), v3_scale(v3_norm(v3_cross(ax, b->f)), 0.012f * s), col, false);
        batch3d_spike(along, v3_norm(v3_sub(v3_scale(b->f, -1.0f), v3_scale(arm, 0.2f))), 0.025f * s, back, dark, false); /* finger */
    }
}

void dragon_draw(Game *g, const Monster *m, vec3 light) {
    if (m->hidden) return;
    const DragonDef *D = KIND(m);
    float s = m->scale;
    float lo = great(m) ? 0.55f : 0.4f; /* (it must be seen: it is the biggest thing here) */
    light = v3(fmaxf(light.x, lo), fmaxf(light.y, lo * 0.95f), fmaxf(light.z, lo * 0.9f));
    vec3 col = m->flash > 0 ? v3(1.6f, 1.6f, 1.6f) : litc(m->color, light);
    vec3 belly = litc(m->color2, light), dark = litc(rgb3(D->wing), light), glow = m->glow;
    float y = m->yaw;
    DFrame B = { m->pos, v3(cosf(y), 0, sinf(y)), v3(-sinf(y), 0, cosf(y)), v3(0, 1, 0), s };
    bool air = m->dstate == DST_FLY || m->dstate == DST_STRAFE || m->dstate == DST_DIVE || m->dstate == DST_TAKEOFF ||
               (m->dsize != DS_DRAKE && SIZE(m)->flies && m->dsize < DS_DRAGON && m->awake);
    bool asleep = m->dstate == DST_SLEEP;
    float jaw = m->dstate == DST_INHALE ? 0.3f : m->dstate == DST_STRAFE || (m->cooldown > 0.9f && m->dstate == DST_GROUND) ? 1.0f : 0.0f;
    float throat = m->dstate == DST_INHALE ? 1.0f - fmaxf(0.0f, m->dtimer) : 0.0f;
    bool serpent = m->dkind == DK_WYRM;
    float h = air ? 0.6f : asleep ? 0.45f : 0.75f;
    float breathe_y = asleep ? sinf(m->anim * 2.0f) * 0.03f : 0.0f;
    vec3 c = P(&B, 0, 0, h + breathe_y);
    if (serpent) { /* a wyrm: a long coiling body, no legs, no wings */
        vec3 prev = v3_add(c, v3_scale(B.f, 1.2f * s));
        for (int i = 0; i < 16; i++) {
            float t = i / 15.0f;
            vec3 p = v3_add(c, v3_add(v3_scale(B.f, (1.2f - i * 0.35f) * s), v3_scale(B.r, sinf(m->anim * 2.0f - i * 0.5f) * 0.4f * s * t)));
            float w = 0.45f - 0.3f * t;
            batch3d_obox(v3_scale(v3_add(prev, p), 0.5f), v3_scale(B.f, 0.22f * s), v3_scale(B.r, w * s), v3_scale(B.u, w * 0.9f * s), i & 1 ? col : belly, false);
            prev = p;
        }
        draw_head(m, &B, v3_add(c, v3_scale(B.f, 1.2f * s)), 0.6f, 0, col, dark, glow, jaw, throat);
        return;
    }
    /* the body */
    BX(&B, c, 0.78f, 0.42f, 0.4f, col, false);
    BX(&B, v3_add(c, v3_scale(B.u, -0.25f * s)), 0.62f, 0.36f, 0.18f, belly, false);
    for (int k = 0; k < 4; k++) batch3d_spike(P(&B, 0.5f - k * 0.35f, 0, h + 0.38f), v3_norm(v3_sub(B.u, v3_scale(B.f, 0.4f))), 0.07f * s, 0.32f * s, dark, false); /* spines */
    /* the tail */
    vec3 prev = P(&B, -0.75f, 0, h);
    for (int i = 1; i <= 9; i++) {
        float t = i / 9.0f;
        vec3 p = v3_add(P(&B, -0.75f - i * 0.32f, 0, h - t * 0.35f * (air ? 0.2f : 1.0f)), v3_scale(B.r, sinf(m->anim * (asleep ? 0.5f : 2.0f) - i * 0.6f) * 0.12f * t * 3.0f * s));
        float w = 0.22f - 0.17f * t;
        batch3d_obox(v3_scale(v3_add(prev, p), 0.5f), v3_scale(B.f, 0.18f * s), v3_scale(B.r, w * s), v3_scale(B.u, w * s), col, false);
        if (i % 2 == 0) batch3d_spike(v3_add(p, v3_scale(B.u, w * s)), B.u, 0.04f * s, 0.18f * s, dark, false);
        prev = p;
    }
    /* legs (tucked in the air) */
    bool moving = v3_len(v3(m->vel.x, 0, m->vel.z)) > 0.5f && !air;
    int legs = m->dsize == DS_WYVERN ? 2 : 4;
    for (int k = 0; k < legs; k++) {
        float fw = legs == 2 ? -0.3f : (k < 2 ? 0.45f : -0.45f), rt = (k % 2 ? 0.36f : -0.36f);
        float sw = moving ? sinf(m->anim * 6.0f + k * 1.6f) * 0.2f : 0.0f;
        vec3 hip = P(&B, fw, rt, h - 0.15f);
        vec3 foot = air ? P(&B, fw - 0.35f, rt * 0.8f, h - 0.35f) : P(&B, fw + sw, rt * 1.1f, 0.05f);
        if (asleep) foot = P(&B, fw + 0.3f, rt * 1.2f, 0.05f);
        vec3 mid = v3_scale(v3_add(hip, foot), 0.5f);
        batch3d_obox(mid, v3_scale(v3_norm(v3_sub(foot, hip)), v3_len(v3_sub(foot, hip)) * 0.5f), v3_scale(B.r, 0.09f * s), v3_scale(B.f, 0.09f * s), col, false);
        BX(&B, foot, 0.13f, 0.08f, 0.04f, dark, false);
    }
    /* wings */
    if (!(D->traits & DT_WINGLESS)) for (int k = -1; k <= 1; k += 2) draw_wing(m, &B, k, air && !m->grounded, col, dark);
    /* heads: one, two, or the hydra's many */
    int n = MAXI(1, m->heads);
    for (int i = 0; i < n; i++) {
        float off = (i - (n - 1) * 0.5f) * (n > 2 ? 0.28f : 0.35f);
        vec3 neck0 = P(&B, 0.7f, off, h + 0.15f);
        float lean = air ? 0.8f : asleep ? 0.95f : 0.25f;
        draw_head(m, &B, neck0, lean, off * 0.8f, col, dark, glow, asleep ? 0.0f : jaw, throat);
    }
    /* its shadow under it in the air */
    if (air && great(m)) {
        int fy = fl(m->pos.y), x = fl(m->pos.x), z = fl(m->pos.z);
        for (int k = 0; k < 40 && !game_solid(g, x, fy - 1, z); k++) fy--;
        batch3d_obox(v3((float)m->pos.x, (float)fy + 0.03f, (float)m->pos.z), v3_scale(B.f, 1.2f * s), v3_scale(B.r, 2.0f * s), v3(0, 0.01f, 0), v3(0.02f, 0.02f, 0.025f), false);
    }
}
