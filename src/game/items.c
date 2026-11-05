#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* Weapons with rarities and affixes, potions, gold, spell scrolls, chests and relics;
 * the inventory and spellbook screens of the character menu. */

const WeaponBase WEAPON_BASES[WB_COUNT] = {
    /*                 class      dmg  cd     range arc   knock speed pierce mana */
    [WB_DAGGER] = { "Dagger", WPN_SWORD, 0.7f, 0.28f, 2.2f, 70, 3, 0, 0, 0 },
    [WB_SWORD] = { "Sword", WPN_SWORD, 1.0f, 0.45f, 2.7f, 80, 5, 0, 0, 0 },
    [WB_AXE] = { "Axe", WPN_SWORD, 1.35f, 0.6f, 2.7f, 120, 6, 0, 0, 0 },
    [WB_MACE] = { "Mace", WPN_SWORD, 1.5f, 0.65f, 2.5f, 70, 10, 0, 0, 0 },
    [WB_SPEAR] = { "Spear", WPN_SWORD, 1.15f, 0.5f, 3.8f, 35, 4, 0, 0, 0 },
    [WB_GREATSWORD] = { "Greatsword", WPN_SWORD, 1.9f, 0.85f, 3.2f, 140, 8, 0, 0, 0 },
    [WB_SHORTBOW] = { "Shortbow", WPN_BOW, 0.8f, 0.4f, 0, 0, 3, 44, 0, 0 },
    [WB_LONGBOW] = { "Longbow", WPN_BOW, 1.2f, 0.6f, 0, 0, 3, 62, 0, 0 },
    [WB_CROSSBOW] = { "Crossbow", WPN_BOW, 1.8f, 0.9f, 0, 0, 5, 72, 2, 0 },
    [WB_WAND] = { "Wand", WPN_STAFF, 0.6f, 0.25f, 0, 0, 2, 30, 0, 3 },
    [WB_STAFF] = { "Staff", WPN_STAFF, 1.0f, 0.45f, 0, 0, 5, 24, 0, 6 },
    [WB_SCEPTER] = { "Scepter", WPN_STAFF, 1.6f, 0.7f, 0, 0, 7, 20, 0, 10 },
};

static const char *AFFIX_ADJ[AF_COUNT] = { "", "Burning", "Frozen", "Crackling", "Venomous", "Vampiric", "Swift", "Keen", "Heavy" };
static const char *AFFIX_SUFFIX[AF_COUNT] = { "", "of Embers", "of Winter", "of Storms", "of the Viper", "of the Leech",
                                              "of Haste", "of Precision", "of Ruin" };
static const char *AFFIX_DESC[AF_COUNT] = { "", "hits burn", "hits chill and slow", "hits arc to a nearby foe",
                                            "hits poison", "hits steal life", "attacks 20% faster", "+15% damage",
                                            "+25% damage and knockback, 10% slower" };
static const char *RARITY_NAMES[RAR_COUNT] = { "COMMON", "MAGIC", "RARE", "EPIC" };

const RelicDef RELICS[RELIC_COUNT] = {
    { "Ember Heart", RB_HEALTH }, { "Horn of the Pit", RB_DAMAGE }, { "Root Amulet", RB_MANA }, { "Miner's Lamp", RB_SPEED },
    { "Iron Crown", RB_HEALTH }, { "Saint's Finger", RB_DAMAGE }, { "Cloud Feather", RB_SPEED }, { "Star Shard", RB_MANA },
    { "Frost Lily", RB_HEALTH }, { "Jade Idol", RB_DAMAGE }, { "Gothic Rose", RB_MANA }, { "Drowned Bell", RB_SPEED },
};
static const char *RELIC_BONUS_TEXT[4] = { "+6% health", "+5% damage", "+8% mana", "+3% speed" };

vec3 rarity_color(int r) {
    static const vec3 C[RAR_COUNT] = { { 0.8f, 0.78f, 0.72f }, { 0.45f, 0.65f, 1.0f }, { 1.0f, 0.85f, 0.3f }, { 1.0f, 0.5f, 0.15f } };
    return C[r < 0 || r >= RAR_COUNT ? 0 : r];
}

bool item_has_affix(const Item *it, int affix) { return it->affix[0] == affix || it->affix[1] == affix; }

float item_damage_mult(const Item *it) {
    static const float RAR[RAR_COUNT] = { 1.0f, 1.12f, 1.28f, 1.5f };
    float m = WEAPON_BASES[it->base].dmg * (1.0f + 0.06f * it->level) * RAR[it->rarity];
    if (item_has_affix(it, AF_KEEN)) m *= 1.15f;
    if (item_has_affix(it, AF_HEAVY)) m *= 1.25f;
    return m;
}

float item_speed_mult(const Item *it) {
    float m = 1.0f;
    if (item_has_affix(it, AF_SWIFT)) m *= 0.8f;
    if (item_has_affix(it, AF_HEAVY)) m *= 1.1f;
    return m;
}

int item_affix_element(const Item *it) {
    for (int i = 0; i < 2; i++) switch (it->affix[i]) {
        case AF_FIRE: return EL_FIRE;
        case AF_FROST: return EL_FROST;
        case AF_SHOCK: return EL_LIGHTNING;
        case AF_VENOM: return EL_POISON;
        case AF_VAMPIRIC: return EL_SHADOW;
        default: break;
        }
    return EL_NONE;
}

void item_name(const Item *it, char *out, int n) {
    const char *base = WEAPON_BASES[it->base].name;
    if (it->affix[0] && it->affix[1]) snprintf(out, (size_t)n, "%s %s %s", AFFIX_ADJ[it->affix[0]], base, AFFIX_SUFFIX[it->affix[1]]);
    else if (it->affix[0]) snprintf(out, (size_t)n, "%s %s", AFFIX_ADJ[it->affix[0]], base);
    else snprintf(out, (size_t)n, "%s", base);
}

Item item_roll(u64 seed, float level, int min_rarity) {
    Rng r = rng_make(seed);
    Item it = { 0 };
    it.used = 1;
    it.base = (u8)rng_range(&r, 0, WB_COUNT - 1);
    float u = rng_float(&r) - fminf(0.15f, level * 0.004f); /* deeper: better odds */
    int rar = u < 0.03f ? RAR_EPIC : u < 0.13f ? RAR_RARE : u < 0.45f ? RAR_MAGIC : RAR_COMMON;
    it.rarity = (u8)MAXI(rar, min_rarity);
    it.level = (u8)clampf(level + rng_range(&r, -1, 2), 0, 250);
    int naff = it.rarity == RAR_COMMON ? 0 : it.rarity == RAR_MAGIC ? 1 : 2;
    for (int i = 0; i < naff; i++) {
        int a;
        do a = rng_range(&r, 1, AF_COUNT - 1);
        while (i == 1 && a == it.affix[0]);
        it.affix[i] = (u8)a;
    }
    return it;
}

static Item start_weapon(int base) {
    Item it = { 0 };
    it.used = 1;
    it.base = (u8)base;
    return it;
}

void items_give_start(Stats *s) {
    s->equipped[WPN_SWORD] = start_weapon(WB_SWORD);
    s->equipped[WPN_BOW] = start_weapon(WB_SHORTBOW);
    s->equipped[WPN_STAFF] = start_weapon(WB_STAFF);
    s->potions_hp = 2;
    s->potions_mp = 1;
}

const WeaponBase *equipped_base(const Stats *s, int wclass) { return &WEAPON_BASES[s->equipped[wclass].base]; }

float relic_bonus(const Stats *s, int bonus) {
    static const float PER[4] = { 0.06f, 0.05f, 0.08f, 0.03f };
    float b = 0;
    for (int i = 0; i < RELIC_COUNT; i++)
        if (RELICS[i].bonus == bonus) b += PER[bonus] * s->relics[i];
    return b;
}

/* ---------- taken chests and relics ---------- */

static u64 *looted_slot(Game *g, u64 key) {
    for (int i = (int)(mix64(key) & (u64)(g->looted_cap - 1));; i = (i + 1) & (g->looted_cap - 1))
        if (g->looted[i] == key || g->looted[i] == 0) return &g->looted[i];
}

bool looted_has(Game *g, u64 key) { return g->looted_cap && *looted_slot(g, key) == key; }

static void looted_add(Game *g, u64 key);
void looted_mark(Game *g, u64 key) { looted_add(g, key); }

static void looted_add(Game *g, u64 key) {
    if ((g->looted_count + 1) * 2 > g->looted_cap) {
        int old_cap = g->looted_cap;
        u64 *old = g->looted;
        g->looted_cap = old_cap ? old_cap * 2 : 256;
        g->looted = calloc((size_t)g->looted_cap, sizeof(u64));
        g->looted_count = 0;
        for (int i = 0; i < old_cap; i++)
            if (old[i]) { *looted_slot(g, old[i]) = old[i]; g->looted_count++; }
        free(old);
    }
    u64 *s = looted_slot(g, key);
    if (*s != key) { *s = key; g->looted_count++; }
}

/* ---------- pickups ---------- */

static u64 g_loot_rng = 0x1234567;
static float lrand(void) {
    g_loot_rng = mix64(g_loot_rng + 0x9E3779B97F4A7C15ULL);
    return (float)(g_loot_rng >> 40) / (float)(1ULL << 24);
}

void pickup_spawn(Game *g, int kind, vec3 pos, int amount, const Item *item, u64 key, u64 chunk_key) {
    int slot = -1;
    for (int i = 0; i < MAX_PICKUPS && slot < 0; i++)
        if (!g->pickups[i].alive) slot = i;
    if (slot < 0) {
        /* full: the oldest gold goes */
        float oldest = -1;
        for (int i = 0; i < MAX_PICKUPS; i++)
            if (g->pickups[i].kind == PK_GOLD && g->pickups[i].t > oldest) { oldest = g->pickups[i].t; slot = i; }
        if (slot < 0) return;
    }
    Pickup *p = &g->pickups[slot];
    memset(p, 0, sizeof *p);
    p->alive = true;
    p->kind = (u8)kind;
    p->pos = pos;
    p->amount = amount;
    if (item) p->item = *item;
    p->key = key;
    p->chunk_key = chunk_key;
    if (!key) {
        float a = lrand() * 2.0f * PI_F;
        p->vel = v3(cosf(a) * 2.0f, 4.0f + lrand() * 2.0f, sinf(a) * 2.0f);
    }
}

static int random_scroll(float level) {
    int max_tier = level < 4 ? 0 : level < 12 ? 1 : level < 24 ? 2 : 3;
    for (int tries = 0; tries < 64; tries++) {
        int id = (int)(lrand() * SPELL_COUNT) % SPELL_COUNT;
        SpellInfo si;
        spell_info(id, &si);
        if (si.tier <= max_tier) return id;
    }
    return spell_attack_id(EL_FIRE, SF_BOLT);
}

void loot_drop(Game *g, vec3 pos, float level, bool elite, float luck) {
    float k = elite ? 4.0f : 1.0f;
    vec3 p = v3_add(pos, v3(0, 0.5f, 0));
    if (lrand() < 0.55f + luck) pickup_spawn(g, PK_GOLD, p, (int)((1 + level * 0.6f + lrand() * 5.0f) * k), NULL, 0, 0);
    if (lrand() < 0.08f + (elite ? 0.4f : 0.0f) + luck) pickup_spawn(g, lrand() < 0.6f ? PK_POTION_HP : PK_POTION_MP, p, 1, NULL, 0, 0);
    if (lrand() < 0.035f + (elite ? 0.35f : 0.0f) + luck * 0.6f) pickup_spawn(g, PK_SCROLL, p, random_scroll(level), NULL, 0, 0);
    if (lrand() < 0.045f + (elite ? 0.5f : 0.0f) + luck) {
        Item it = item_roll(mix64((u64)(lrand() * 1e9f)), level, elite ? RAR_MAGIC : RAR_COMMON);
        pickup_spawn(g, PK_WEAPON, p, 0, &it, 0, 0);
    }
}

static bool standable(Game *g, int x, int y, int z) {
    return game_solid(g, x, y - 1, z) == 1 && !mech_solid(g, x, y - 1, z) && game_solid(g, x, y, z) == 0 && game_solid(g, x, y + 1, z) == 0;
}

static bool find_spot(Game *g, Chunk *c, Rng *r, vec3 *out) {
    for (int attempt = 0; attempt < 40; attempt++) {
        int wx = c->cx * CHUNK + rng_range(r, 1, 30), wz = c->cz * CHUNK + rng_range(r, 1, 30);
        for (int y = 30; y >= 1; y--) {
            int wy = c->cy * CHUNK + y;
            if (standable(g, wx, wy, wz)) { *out = v3(wx + 0.5f, (float)wy, wz + 0.5f); return true; }
        }
    }
    return false;
}

static bool pickup_alive_with_key(Game *g, u64 key) {
    for (int i = 0; i < MAX_PICKUPS; i++)
        if (g->pickups[i].alive && g->pickups[i].key == key) return true;
    return false;
}

void loot_place_in_chunk(Game *g, Chunk *c) {
    const ChunkInfo *ci = &c->info;
    if (ci->arch == ARCH_EXTERIOR) return;
    u64 ck = chunk_spawn_key(c->cx, c->cy, c->cz);
    Rng r = rng_make(ci->seed ^ 0xC4E57ULL);
    u64 chest_key = mix64(ck ^ 0xC1C1ULL) | 1, relic_key = mix64(ck ^ 0xE1E1ULL) | 1;
    vec3 p;
    if (rng_chance(&r, ci->has_struct ? 0.5f : 0.3f) && !looted_has(g, chest_key) && !pickup_alive_with_key(g, chest_key) &&
        find_spot(g, c, &r, &p))
        pickup_spawn(g, PK_CHEST, p, ci->level, NULL, chest_key, ck);
    if (rng_chance(&r, 0.08f) && !looted_has(g, relic_key) && !pickup_alive_with_key(g, relic_key) && find_spot(g, c, &r, &p))
        pickup_spawn(g, PK_RELIC, v3_add(p, v3(0, 0.6f, 0)), (ci->stratum * 5 + ci->flavor * 7) % RELIC_COUNT, NULL, relic_key, ck);
    /* something to read: an inscription, a dead adventurer, a bounty board */
    Trace t;
    Rng tr = rng_make(ci->seed ^ 0x7ACEULL);
    if (story_trace(&g->world, c->cx, c->cy, c->cz, &t) && !pickup_alive_with_key(g, t.key) && find_spot(g, c, &tr, &p))
        pickup_spawn(g, PK_TRACE, p, t.kind, NULL, t.key, ck);
}

static int bag_free(Stats *s) {
    for (int i = 0; i < BAG_SIZE; i++)
        if (!s->bag[i].used) return i;
    return -1;
}

static bool collect(Game *g, PlayerSlot *ps, Pickup *p) {
    Stats *s = &ps->st;
    char buf[128], name[64];
    vec3 at = v3_add(p->pos, v3(0, 0.3f, 0));
    switch (p->kind) {
    case PK_GOLD:
        s->gold += p->amount;
        particles_burst(g, at, v3(1.0f, 0.85f, 0.3f), 4, 1.5f, 0.03f, true);
        return true;
    case PK_POTION_HP:
    case PK_POTION_MP: {
        int *n = p->kind == PK_POTION_HP ? &s->potions_hp : &s->potions_mp;
        if (*n >= 9) return false;
        (*n)++;
        return true;
    }
    case PK_SCROLL: {
        int rank = spell_learn(ps, p->amount);
        SpellInfo si;
        spell_info(p->amount, &si);
        if (rank == 1) snprintf(buf, sizeof buf, "NEW SPELL: %s", si.name);
        else snprintf(buf, sizeof buf, "%s - RANK %d", si.name, rank);
        game_banner(ps, buf);
        flash_add(g, at, spell_is_attack(p->amount) ? element_color(spell_element(p->amount)) : v3(1, 1, 0.8f), 5.0f, 0.3f);
        return true;
    }
    case PK_WEAPON: {
        int slot = bag_free(s);
        if (slot < 0) {
            if (ps->banner_time <= 0) game_banner(ps, "BAG FULL - SCRAP SOMETHING");
            return false;
        }
        s->bag[slot] = p->item;
        item_name(&p->item, name, sizeof name);
        snprintf(buf, sizeof buf, "%s %s", p->item.rarity >= RAR_RARE ? RARITY_NAMES[p->item.rarity] : "FOUND", name);
        game_banner(ps, buf);
        return true;
    }
    case PK_RELIC:
        s->relics[p->amount]++;
        stats_refresh(ps);
        snprintf(buf, sizeof buf, "RELIC: %s (%s)", RELICS[p->amount].name, RELIC_BONUS_TEXT[RELICS[p->amount].bonus]);
        game_banner(ps, buf);
        flash_add(g, at, v3(1.0f, 0.85f, 0.4f), 10.0f, 0.6f);
        looted_add(g, p->key);
        return true;
    case PK_TRACE:
        return false; /* read, not taken (quests.c) */
    case PK_QUEST:
        return story_collect_quest(g, ps, p);
    case PK_CHEST:
        looted_add(g, p->key);
        if (p->amount & CHEST_VAULT) {
            /* a vault's hoard: gold, potions, a rare weapon at least */
            int level = p->amount & 0xFFFF;
            flash_add(g, at, v3(1.2f, 0.95f, 0.5f), 10.0f, 0.7f);
            particles_burst(g, at, v3(1.0f, 0.85f, 0.35f), 30, 5.0f, 0.06f, true);
            for (int i = 0; i < 5; i++) loot_drop(g, p->pos, (float)level + 2, false, 0.4f);
            pickup_spawn(g, PK_GOLD, v3_add(p->pos, v3(0, 0.6f, 0)), 20 + level * 3, NULL, 0, 0);
            pickup_spawn(g, PK_POTION_HP, v3_add(p->pos, v3(0, 0.6f, 0)), 1, NULL, 0, 0);
            Item it = item_roll(mix64(p->key ^ 0x7A), (float)level + 3, lrand() < 0.25f ? RAR_EPIC : RAR_RARE);
            pickup_spawn(g, PK_WEAPON, v3_add(p->pos, v3(0, 0.7f, 0)), 0, &it, 0, 0);
            game_banner(ps, "THE VAULT'S HOARD");
            return true;
        }
        flash_add(g, at, v3(1.0f, 0.8f, 0.4f), 8.0f, 0.4f);
        particles_burst(g, at, v3(1.0f, 0.8f, 0.3f), 18, 4.0f, 0.05f, true);
        for (int i = 0; i < 3; i++) loot_drop(g, p->pos, (float)p->amount, false, 0.25f);
        {
            Item it = item_roll(mix64(p->key ^ 0x77), (float)p->amount + 1, lrand() < 0.5f ? RAR_MAGIC : RAR_COMMON);
            pickup_spawn(g, PK_WEAPON, v3_add(p->pos, v3(0, 0.6f, 0)), 0, &it, 0, 0);
        }
        return true;
    }
    return false;
}

void pickups_update(Game *g, float dt) {
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &g->pickups[i];
        if (!p->alive) continue;
        p->t += dt;
        PlayerSlot *near = game_nearest_player(g, p->pos, 130.0f);
        if (!near) {
            /* nobody near: placed things come back with their chunk, drops are lost */
            p->alive = false;
            if (p->chunk_key) chunk_forget(g, p->chunk_key);
            continue;
        }
        if (!p->key && p->t > 240.0f) { p->alive = false; continue; }
        /* gold and potions drift to a player standing close */
        vec3 feet = near->pl.pos;
        float d = v3_len(v3_sub(feet, p->pos));
        bool magnet = (p->kind == PK_GOLD || p->kind == PK_POTION_HP || p->kind == PK_POTION_MP) && d < 3.5f && p->t > 0.4f;
        if (magnet) {
            p->vel = v3_scale(v3_norm(v3_sub(v3_add(feet, v3(0, 0.5f, 0)), p->pos)), 7.0f);
            p->pos = v3_add(p->pos, v3_scale(p->vel, dt));
        } else if (!p->key) {
            /* dropped things fall and settle */
            p->vel.y -= 18.0f * dt;
            vec3 np = v3_add(p->pos, v3_scale(p->vel, dt));
            if (game_solid(g, (int)floorf(np.x), (int)floorf(np.y - 0.1f), (int)floorf(np.z)) != 0) {
                p->vel = v3(0, 0, 0);
            } else if (game_solid(g, (int)floorf(np.x), (int)floorf(np.y), (int)floorf(np.z)) == 0) {
                p->pos = np;
            } else {
                p->vel.x = p->vel.z = 0;
            }
        }
        if (p->t < 0.4f && !p->key) continue;
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *ps = &g->players[k];
            if (!ps->active || ps->st.dead) continue;
            vec3 f = ps->pl.pos;
            float reach = p->kind == PK_CHEST ? 1.7f : 1.2f;
            float dx = p->pos.x - f.x, dz = p->pos.z - f.z;
            if (dx * dx + dz * dz > reach * reach || p->pos.y < f.y - 1.0f || p->pos.y > f.y + 2.2f) continue;
            if (collect(g, ps, p)) { p->alive = false; break; }
        }
    }
}

void pickups_draw(Game *g, vec3 eye) {
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &g->pickups[i];
        if (!p->alive || v3_len(v3_sub(p->pos, eye)) > 70.0f) continue;
        float bob = sinf(p->t * 3.0f + i) * 0.06f;
        vec3 l = game_light_at(g, p->pos);
        l = v3(fmaxf(l.x, 0.3f), fmaxf(l.y, 0.28f), fmaxf(l.z, 0.25f));
        vec3 c = v3_add(p->pos, v3(0, 0.25f + bob, 0));
        switch (p->kind) {
        case PK_GOLD: {
            int n = MINI(4, 1 + p->amount / 10);
            for (int k = 0; k < n; k++)
                batch3d_box(v3_add(p->pos, v3((k % 2) * 0.12f - 0.06f, 0.05f + k * 0.04f, (k / 2) * 0.1f - 0.05f)), v3(0.07f, 0.02f, 0.07f),
                            p->t * 2.0f + k, v3(1.0f, 0.8f, 0.25f), true);
            break;
        }
        case PK_POTION_HP:
        case PK_POTION_MP: {
            vec3 col = p->kind == PK_POTION_HP ? v3(0.95f, 0.15f, 0.15f) : v3(0.2f, 0.4f, 1.0f);
            batch3d_box(c, v3(0.09f, 0.12f, 0.09f), p->t, col, true);
            batch3d_box(v3_add(c, v3(0, 0.16f, 0)), v3(0.04f, 0.05f, 0.04f), p->t, v3(0.6f * l.x, 0.5f * l.y, 0.4f * l.z), false);
            break;
        }
        case PK_SCROLL: {
            vec3 col = spell_is_attack(p->amount) ? element_color(spell_element(p->amount)) : v3(1.0f, 1.0f, 0.8f);
            batch3d_box(v3_add(c, v3(0, 0.15f, 0)), v3(0.16f, 0.05f, 0.05f), p->t * 1.5f, v3(0.9f * l.x, 0.85f * l.y, 0.7f * l.z), false);
            batch3d_octa(v3_add(c, v3(0, 0.38f, 0)), 0.07f, 0.09f, p->t * 3.0f, col, true);
            break;
        }
        case PK_WEAPON: {
            vec3 col = rarity_color(p->item.rarity);
            float yaw = p->t * 1.2f;
            batch3d_box(v3_add(c, v3(0, 0.15f, 0)), v3(0.35f, 0.04f, 0.04f), yaw, v3(0.75f * l.x, 0.75f * l.y, 0.8f * l.z), false);
            batch3d_box(v3_add(c, v3(0, 0.15f, 0)), v3(0.05f, 0.05f, 0.14f), yaw, v3_scale(col, 0.8f), true);
            if (p->item.rarity > RAR_COMMON) batch3d_octa(v3_add(c, v3(0, 0.5f, 0)), 0.05f, 0.07f, p->t * 2.0f, col, true);
            break;
        }
        case PK_RELIC:
            batch3d_octa(c, 0.18f, 0.26f, p->t * 1.5f, v3(1.0f, 0.85f, 0.35f), true);
            batch3d_octa(c, 0.08f, 0.34f, -p->t * 2.0f, v3(1.0f, 1.0f, 0.8f), true);
            break;
        case PK_TRACE: {
            float yaw = (float)(p->key % 8) * PI_F * 0.25f;
            vec3 stone = v3(0.42f * l.x, 0.4f * l.y, 0.38f * l.z);
            vec3 f = v3(cosf(yaw), 0, sinf(yaw));
            if (p->amount == TR_STELE) {
                /* a standing stone with a faint glowing line of script */
                batch3d_box(v3_add(p->pos, v3(0, 0.55f, 0)), v3(0.12f, 0.55f, 0.32f), yaw, stone, false);
                batch3d_box(v3_add(p->pos, v3(0, 1.12f, 0)), v3(0.14f, 0.04f, 0.36f), yaw, v3_scale(stone, 0.8f), false);
                float glow = 0.35f + 0.15f * sinf(p->t * 1.7f);
                batch3d_box(v3_add(v3_add(p->pos, v3_scale(f, 0.125f)), v3(0, 0.75f, 0)), v3(0.01f, 0.03f, 0.2f), yaw,
                            v3(0.9f * glow, 0.75f * glow, 0.45f * glow), true);
            } else if (p->amount == TR_CORPSE) {
                /* bones and a pale scrap of paper */
                vec3 bone = v3(0.7f * l.x, 0.66f * l.y, 0.58f * l.z), r = v3(-sinf(yaw), 0, cosf(yaw));
                batch3d_box(v3_add(p->pos, v3(0, 0.1f, 0)), v3(0.35f, 0.08f, 0.14f), yaw, bone, false);
                batch3d_box(v3_add(v3_add(p->pos, v3_scale(f, 0.46f)), v3(0, 0.12f, 0)), v3(0.11f, 0.1f, 0.1f), yaw, bone, false);
                batch3d_box(v3_add(v3_add(p->pos, v3_scale(r, 0.3f)), v3(0, 0.05f, 0)), v3(0.25f, 0.04f, 0.04f), yaw + 0.4f, v3_scale(bone, 0.8f), false);
                batch3d_box(v3_add(v3_add(p->pos, v3_scale(r, -0.35f)), v3(0, 0.03f, 0)), v3(0.08f, 0.01f, 0.1f), yaw + 0.3f,
                            v3(0.85f, 0.8f, 0.65f), true);
            } else {
                /* a bounty board on two posts */
                vec3 wood = v3(0.42f * l.x, 0.27f * l.y, 0.14f * l.z), r = v3(-sinf(yaw), 0, cosf(yaw));
                for (int k = -1; k <= 1; k += 2) batch3d_box(v3_add(v3_add(p->pos, v3_scale(r, 0.45f * k)), v3(0, 0.75f, 0)), v3(0.05f, 0.75f, 0.05f), yaw, wood, false);
                batch3d_box(v3_add(p->pos, v3(0, 1.2f, 0)), v3(0.04f, 0.32f, 0.5f), yaw, v3_scale(wood, 1.2f), false);
                batch3d_box(v3_add(v3_add(p->pos, v3_scale(f, 0.05f)), v3(0, 1.22f, 0)), v3(0.01f, 0.18f, 0.14f), yaw, v3(0.95f, 0.88f, 0.7f), true);
                batch3d_box(v3_add(v3_add(p->pos, v3_scale(f, 0.06f)), v3(0, 1.3f, 0)), v3(0.01f, 0.03f, 0.08f), yaw, v3(0.7f, 0.1f, 0.08f), true);
            }
            break;
        }
        case PK_QUEST:
            batch3d_box(c, v3(0.12f, 0.1f, 0.12f), p->t, v3(0.85f, 0.75f, 0.55f), true);
            batch3d_octa(v3_add(c, v3(0, 0.45f, 0)), 0.06f, 0.09f, p->t * 2.0f, v3(1.0f, 0.9f, 0.6f), true);
            break;
        case PK_CHEST: {
            vec3 wood = v3(0.45f * l.x, 0.28f * l.y, 0.14f * l.z);
            float yaw = (float)(p->key % 4) * PI_F * 0.5f;
            batch3d_box(v3_add(p->pos, v3(0, 0.25f, 0)), v3(0.42f, 0.25f, 0.3f), yaw, wood, false);
            batch3d_box(v3_add(p->pos, v3(0, 0.53f, 0)), v3(0.44f, 0.05f, 0.32f), yaw, v3_scale(wood, 0.8f), false);
            batch3d_box(v3_add(p->pos, v3(0, 0.36f, 0)), v3(0.07f, 0.08f, 0.33f), yaw, v3(1.0f, 0.8f, 0.3f), true);
            break;
        }
        }
    }
}

void drink_potion(Game *g, PlayerSlot *ps, bool mana) {
    Stats *s = &ps->st;
    if (s->dead || ps->potion_cd > 0) return;
    int *n = mana ? &s->potions_mp : &s->potions_hp;
    if (*n <= 0) { game_banner(ps, mana ? "NO MANA POTIONS" : "NO HEALTH POTIONS"); return; }
    if (mana ? s->mana >= s->maxmana : s->hp >= s->maxhp) return;
    (*n)--;
    if (mana) s->mana = fminf(s->maxmana, s->mana + s->maxmana * 0.6f);
    else s->hp = fminf(s->maxhp, s->hp + s->maxhp * 0.5f);
    ps->potion_cd = 0.6f;
    particles_burst(g, player_eye(&ps->pl), mana ? v3(0.3f, 0.5f, 1.0f) : v3(1.0f, 0.3f, 0.3f), 10, 1.5f, 0.04f, true);
}

/* ---------- inventory screen ----------
 * Cells: 0-2 equipped weapons, 3-20 bag, 21 health potions, 22 mana potions. */

#define INV_CELLS (3 + BAG_SIZE + 2)

typedef struct InvLayout { int cs, gap, ex, ey, ew, bx, by, info_y; bool compact; } InvLayout;

static InvLayout inv_layout(int W, int H, int top) {
    InvLayout l;
    l.compact = W < 440;
    l.cs = l.compact ? 16 : 24;
    l.gap = l.compact ? 2 : 4;
    l.ew = l.compact ? 90 : 150;
    int bag_w = 6 * (l.cs + l.gap);
    int total = l.ew + 10 + bag_w;
    l.ex = (W - total) / 2;
    l.ey = top + (l.compact ? 4 : 14);
    l.bx = l.ex + l.ew + 10;
    l.by = l.ey;
    l.info_y = l.ey + 3 * (l.cs + l.gap) + (l.compact ? 4 : 10);
    (void)H;
    return l;
}

static void cell_rect(const InvLayout *l, int i, float *x, float *y, float *w, float *h) {
    if (i < 3) {
        *x = (float)l->ex; *y = (float)(l->ey + i * (l->cs + l->gap)); *w = (float)l->ew; *h = (float)l->cs;
    } else if (i < 3 + BAG_SIZE) {
        int k = i - 3;
        *x = (float)(l->bx + (k % 6) * (l->cs + l->gap));
        *y = (float)(l->by + (k / 6) * (l->cs + l->gap));
        *w = *h = (float)l->cs;
    } else {
        int k = i - 3 - BAG_SIZE;
        *w = (float)(l->ew / 2 - 2); *h = (float)(l->compact ? 10 : 14);
        *x = (float)(l->ex + k * (l->ew / 2 + 2));
        *y = (float)(l->ey + 3 * (l->cs + l->gap) + 2);
    }
}

static Item *inv_item(Stats *s, int i) {
    if (i < 3) return &s->equipped[i];
    if (i < 3 + BAG_SIZE) return &s->bag[i - 3];
    return NULL;
}

int inventory_hit(PlayerSlot *ps, int W, int H, int top, float x, float y) {
    InvLayout l = inv_layout(W, H, top);
    (void)ps;
    for (int i = 0; i < INV_CELLS; i++) {
        float cx, cy, cw, ch;
        cell_rect(&l, i, &cx, &cy, &cw, &ch);
        if (x >= cx - 1 && x < cx + cw + 1 && y >= cy - 1 && y < cy + ch + 1) return i;
    }
    return -1;
}

static void scrap(Game *g, PlayerSlot *ps, int i) {
    Stats *s = &ps->st;
    if (i < 3 || i >= 3 + BAG_SIZE || !s->bag[i - 3].used) return;
    Item *it = &s->bag[i - 3];
    int gold = (int)((4 + it->level * 2) * (1 + it->rarity * 2));
    s->gold += gold;
    char buf[96], name[64];
    item_name(it, name, sizeof name);
    snprintf(buf, sizeof buf, "SCRAPPED %s: +%d GOLD", name, gold);
    game_banner(ps, buf);
    memset(it, 0, sizeof *it);
    (void)g;
}

static void activate(Game *g, PlayerSlot *ps, int i) {
    Stats *s = &ps->st;
    if (i == 3 + BAG_SIZE) { drink_potion(g, ps, false); return; }
    if (i == 4 + BAG_SIZE) { drink_potion(g, ps, true); return; }
    if (i < 3) { s->weapon = i; return; }
    Item *it = &s->bag[i - 3];
    if (!it->used) return;
    int wc = WEAPON_BASES[it->base].wclass;
    Item old = s->equipped[wc];
    s->equipped[wc] = *it;
    *it = old;
    s->weapon = wc;
}

void inventory_nav(Game *g, PlayerSlot *ps, int nav) {
    if (nav == NAV_BUY) { activate(g, ps, ps->inv_sel); return; }
    if (nav == NAV_DROP) { scrap(g, ps, ps->inv_sel); return; }
    /* move to the nearest cell in that direction */
    InvLayout l = inv_layout(480, 270, 0);
    float x0, y0, w0, h0;
    cell_rect(&l, ps->inv_sel, &x0, &y0, &w0, &h0);
    float cx0 = x0 + w0 * 0.5f, cy0 = y0 + h0 * 0.5f;
    float dx = nav == NAV_LEFT ? -1.0f : nav == NAV_RIGHT ? 1.0f : 0.0f, dy = nav == NAV_UP ? -1.0f : nav == NAV_DOWN ? 1.0f : 0.0f;
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < INV_CELLS; i++) {
        if (i == ps->inv_sel) continue;
        float x, y, w, h;
        cell_rect(&l, i, &x, &y, &w, &h);
        float vx = x + w * 0.5f - cx0, vy = y + h * 0.5f - cy0;
        float along = vx * dx + vy * dy;
        if (along <= 1.0f) continue;
        float d = along + fabsf(vx * dy - vy * dx) * 2.5f;
        if (d < bd) { bd = d; best = i; }
    }
    if (best >= 0) ps->inv_sel = best;
}

void inventory_tap(Game *g, PlayerSlot *ps, int i) {
    if (i < 0) return;
    if (i == ps->inv_sel) activate(g, ps, i);
    else ps->inv_sel = i;
}

static void frame_rect(float x, float y, float w, float h, vec3 c, float a) {
    batch2d_rect(x, y, w, 1, c.x, c.y, c.z, a);
    batch2d_rect(x, y + h - 1, w, 1, c.x, c.y, c.z, a);
    batch2d_rect(x, y, 1, h, c.x, c.y, c.z, a);
    batch2d_rect(x + w - 1, y, 1, h, c.x, c.y, c.z, a);
}

static const char *WEAPON_ICON[WB_COUNT] = { "D", "S", "A", "M", "P", "G", "B", "L", "C", "W", "T", "Z" };

void inventory_draw(Game *g, PlayerSlot *ps, int W, int H, int top) {
    Stats *s = &ps->st;
    InvLayout l = inv_layout(W, H, top);
    bool touch = ps->keyboard && g->touch.active;
    batch2d_begin(W, H);
    for (int i = 0; i < INV_CELLS; i++) {
        float x, y, w, h;
        cell_rect(&l, i, &x, &y, &w, &h);
        Item *it = inv_item(s, i);
        bool has = it ? it->used : true;
        vec3 rc = it && it->used ? rarity_color(it->rarity) : v3(0.3f, 0.25f, 0.2f);
        batch2d_rect(x, y, w, h, 0.07f, 0.055f, 0.045f, 0.9f);
        if (has && it) batch2d_rect(x + 1, y + 1, w - 2, h - 2, rc.x * 0.18f, rc.y * 0.18f, rc.z * 0.18f, 1.0f);
        if (i < 3 && s->weapon == i) batch2d_rect(x, y, 2, h, 1.0f, 0.8f, 0.4f, 1.0f);
        frame_rect(x, y, w, h, ps->inv_sel == i ? v3(1.0f, 0.8f, 0.4f) : v3_scale(rc, 0.6f), 1.0f);
    }
    /* relics */
    int ry = l.info_y + (l.compact ? 40 : 60);
    for (int i = 0; i < RELIC_COUNT; i++) {
        float x = (float)(l.ex + i * (l.compact ? 12 : 16)), y = (float)ry;
        bool got = s->relics[i] > 0;
        batch2d_rect(x, y, l.compact ? 10.0f : 13.0f, l.compact ? 8.0f : 10.0f, got ? 0.9f : 0.12f, got ? 0.72f : 0.1f, got ? 0.3f : 0.08f, 1.0f);
    }
    batch2d_flush();

    font_begin(W, H);
    char buf[160], name[64];
    static const char *SLOT[3] = { "BLADE", "BOW", "STAFF" };
    for (int i = 0; i < 3; i++) {
        float x, y, w, h;
        cell_rect(&l, i, &x, &y, &w, &h);
        item_name(&s->equipped[i], name, sizeof name);
        vec3 c = rarity_color(s->equipped[i].rarity);
        if (l.compact) {
            name[MINI(13, (int)strlen(name))] = 0;
            font_text((int)x + 3, (int)y + 5, name, c.x, c.y, c.z);
        } else {
            font_text((int)x + 4, (int)y + 2, SLOT[i], 0.5f, 0.45f, 0.4f);
            name[MINI(23, (int)strlen(name))] = 0;
            font_text((int)x + 4, (int)y + 13, name, c.x, c.y, c.z);
        }
    }
    for (int i = 0; i < BAG_SIZE; i++) {
        if (!s->bag[i].used) continue;
        float x, y, w, h;
        cell_rect(&l, 3 + i, &x, &y, &w, &h);
        vec3 c = rarity_color(s->bag[i].rarity);
        font_text((int)(x + (w - 5) / 2), (int)(y + (h - 7) / 2), WEAPON_ICON[s->bag[i].base], c.x, c.y, c.z);
    }
    for (int k = 0; k < 2; k++) {
        float x, y, w, h;
        cell_rect(&l, 3 + BAG_SIZE + k, &x, &y, &w, &h);
        snprintf(buf, sizeof buf, "%s %d", k ? (l.compact ? "MP" : "MANA") : (l.compact ? "HP" : "HEALTH"), k ? s->potions_mp : s->potions_hp);
        font_text((int)x + 3, (int)y + (l.compact ? 2 : 4), buf, k ? 0.5f : 1.0f, k ? 0.6f : 0.45f, k ? 1.0f : 0.45f);
    }
    /* details of the selected cell */
    int y = l.info_y + (l.compact ? 8 : 12);
    Item *it = inv_item(s, ps->inv_sel);
    if (it && it->used) {
        const WeaponBase *b = &WEAPON_BASES[it->base];
        item_name(it, name, sizeof name);
        vec3 c = rarity_color(it->rarity);
        snprintf(buf, sizeof buf, "%s (%s, LV %d)", name, RARITY_NAMES[it->rarity], it->level);
        font_text(l.ex, y, buf, c.x, c.y, c.z);
        float cd = b->cooldown * item_speed_mult(it);
        if (b->wclass == WPN_SWORD) snprintf(buf, sizeof buf, "DAMAGE x%.2f  %.2fs  REACH %.1fm  ARC %.0f", item_damage_mult(it), cd, b->range, b->arc_deg);
        else if (b->wclass == WPN_BOW) snprintf(buf, sizeof buf, "DAMAGE x%.2f  %.2fs  %s", item_damage_mult(it), cd, b->pierce ? "PIERCES" : "");
        else snprintf(buf, sizeof buf, "DAMAGE x%.2f  %.2fs  %.0f MANA PER BOLT", item_damage_mult(it), cd, b->mana);
        font_text(l.ex, y + 10, buf, 0.75f, 0.72f, 0.65f);
        buf[0] = 0;
        for (int a = 0; a < 2; a++)
            if (it->affix[a]) {
                strncat(buf, AFFIX_DESC[it->affix[a]], sizeof buf - strlen(buf) - 1);
                strncat(buf, a == 0 && it->affix[1] ? ", " : "", sizeof buf - strlen(buf) - 1);
            }
        if (buf[0]) font_text(l.ex, y + 20, buf, 0.6f, 0.8f, 0.6f);
        if (ps->inv_sel >= 3 && !l.compact) {
            const Item *eq = &s->equipped[b->wclass];
            float diff = item_damage_mult(it) - item_damage_mult(eq);
            snprintf(buf, sizeof buf, "%+.2f DAMAGE VS EQUIPPED", diff);
            font_text(l.ex + l.ew + 10 + 6 * (l.cs + l.gap) - font_text_width(buf), y + 20, buf, diff >= 0 ? 0.5f : 1.0f,
                      diff >= 0 ? 1.0f : 0.5f, 0.4f);
        }
    } else if (ps->inv_sel >= 3 + BAG_SIZE) {
        font_text(l.ex, y, ps->inv_sel == 3 + BAG_SIZE ? "HEALTH POTION: HEALS HALF YOUR HEALTH" : "MANA POTION: 60% MANA", 0.8f, 0.75f, 0.65f);
    }
    snprintf(buf, sizeof buf, "GOLD %d", s->gold);
    font_text(l.ex + (l.compact ? 150 : 210), ry + (l.compact ? 0 : 1), buf, 1.0f, 0.85f, 0.35f);
    if (!l.compact) {
        int found = 0;
        for (int i = 0; i < RELIC_COUNT; i++) found += s->relics[i] > 0;
        snprintf(buf, sizeof buf, "RELICS %d/%d", found, RELIC_COUNT);
        font_text(l.ex, ry - 10, buf, 0.7f, 0.62f, 0.45f);
        const char *help = touch ? "TAP TO SELECT, TAP AGAIN TO EQUIP OR USE - SCRAP BUTTON SELLS"
                           : ps->pad && !(ps->keyboard && g->mouse) ? "A EQUIP/USE  X SCRAP  LB/RB TABS  B CLOSE"
                                                                    : "ENTER EQUIP/USE  DEL SCRAP  Q/E TABS  TAB CLOSE";
        font_text((W - font_text_width(help)) / 2, H - 10, help, 0.5f, 0.46f, 0.4f);
    }
    font_end();
}

/* ---------- spellbook screen ----------
 * A grid: one row per element (forms across), and a row of utility spells. */

typedef struct BookLayout { int cw, ch, gx, gy, lw; bool compact; } BookLayout;

static BookLayout book_layout(int W, int H, int top) {
    BookLayout l;
    l.compact = W < 440;
    l.lw = l.compact ? 0 : 66;
    l.cw = l.compact ? MINI(18, (W - 8) / SF_COUNT) : 30;
    l.ch = MAXI(7, MINI(l.compact ? 9 : 15, (H - top - (l.compact ? 30 : 64)) / (EL_COUNT + 1)));
    l.gx = (W - (l.lw + SF_COUNT * l.cw)) / 2 + l.lw;
    l.gy = top + (l.compact ? 2 : 14);
    return l;
}

static void book_cell(const BookLayout *l, int id, int *x, int *y) {
    if (spell_is_attack(id)) {
        *x = l->gx + spell_form(id) * l->cw;
        *y = l->gy + spell_element(id) * l->ch;
    } else {
        *x = l->gx + (id - SPELL_ATTACK_COUNT) * l->cw;
        *y = l->gy + EL_COUNT * l->ch + 2;
    }
}

int spellbook_hit(PlayerSlot *ps, int W, int H, int top, float x, float y) {
    BookLayout l = book_layout(W, H, top);
    (void)ps;
    for (int id = 0; id < SPELL_COUNT; id++) {
        int cx, cy;
        book_cell(&l, id, &cx, &cy);
        if (x >= cx && x < cx + l.cw && y >= cy && y < cy + l.ch) return id;
    }
    return -1;
}

void spellbook_nav(Game *g, PlayerSlot *ps, int nav) {
    Stats *s = &ps->st;
    int id = ps->book_sel;
    int row = spell_is_attack(id) ? spell_element(id) : EL_COUNT, col = spell_is_attack(id) ? spell_form(id) : id - SPELL_ATTACK_COUNT;
    switch (nav) {
    case NAV_LEFT: col--; break;
    case NAV_RIGHT: col++; break;
    case NAV_UP: row--; break;
    case NAV_DOWN: row++; break;
    case NAV_BUY:
        if (s->spell_rank[id]) {
            s->spell = id;
            SpellInfo si;
            spell_info(id, &si);
            char buf[64];
            snprintf(buf, sizeof buf, "SPELL: %s", si.name);
            game_banner(ps, buf);
        }
        return;
    default: return;
    }
    row = (row + EL_COUNT + 1) % (EL_COUNT + 1);
    int ncol = row == EL_COUNT ? SU_COUNT : SF_COUNT;
    col = (col % ncol + ncol) % ncol;
    ps->book_sel = row == EL_COUNT ? SPELL_ATTACK_COUNT + col : spell_attack_id(row, col);
    (void)g;
}

void spellbook_tap(Game *g, PlayerSlot *ps, int id) {
    if (id < 0) return;
    if (id == ps->book_sel) spellbook_nav(g, ps, NAV_BUY);
    else ps->book_sel = id;
}

void spellbook_draw(Game *g, PlayerSlot *ps, int W, int H, int top) {
    Stats *s = &ps->st;
    BookLayout l = book_layout(W, H, top);
    bool touch = ps->keyboard && g->touch.active;
    batch2d_begin(W, H);
    for (int id = 0; id < SPELL_COUNT; id++) {
        int x, y;
        book_cell(&l, id, &x, &y);
        int rank = s->spell_rank[id];
        vec3 c = spell_is_attack(id) ? element_color(spell_element(id)) : v3(0.95f, 0.9f, 0.7f);
        if (rank) batch2d_rect((float)x + 1, (float)y + 1, l.cw - 2.0f, l.ch - 2.0f, c.x * 0.45f, c.y * 0.45f, c.z * 0.45f, 1.0f);
        else batch2d_rect((float)x + 1, (float)y + 1, l.cw - 2.0f, l.ch - 2.0f, 0.06f, 0.05f, 0.05f, 1.0f);
        for (int r = 0; r < rank; r++) batch2d_rect((float)x + 2 + r * 3, (float)y + l.ch - 3, 2, 1, c.x, c.y, c.z, 1.0f);
        if (id == s->spell) frame_rect((float)x, (float)y, (float)l.cw, (float)l.ch, c, 1.0f);
        if (id == ps->book_sel) frame_rect((float)x - 1, (float)y - 1, l.cw + 2.0f, l.ch + 2.0f, v3(1.0f, 0.85f, 0.45f), 1.0f);
    }
    batch2d_flush();
    font_begin(W, H);
    char buf[160];
    if (!l.compact) {
        for (int e = 0; e < EL_COUNT; e++) {
            vec3 c = element_color(e);
            font_text(l.gx - l.lw, l.gy + e * l.ch + (l.ch - 7) / 2, ELEMENT_NAMES[e], c.x * 0.8f, c.y * 0.8f, c.z * 0.8f);
        }
        font_text(l.gx - l.lw, l.gy + EL_COUNT * l.ch + 2 + (l.ch - 7) / 2, "OTHER", 0.75f, 0.72f, 0.6f);
        static const char *ABBR[SF_COUNT] = { "BLT", "BAR", "NOV", "CHN", "BRE", "ORB", "RUN", "HAL", "STM", "MET", "AUR" };
        for (int f = 0; f < SF_COUNT; f++) font_text(l.gx + f * l.cw + (l.cw - 18) / 2, l.gy - 9, ABBR[f], 0.5f, 0.46f, 0.4f);
    }
    int known = 0;
    for (int id = 0; id < SPELL_COUNT; id++) known += s->spell_rank[id] > 0;
    int iy = l.gy + (EL_COUNT + 1) * l.ch + 6;
    SpellInfo si;
    spell_info(ps->book_sel, &si);
    int rank = s->spell_rank[ps->book_sel];
    if (rank) {
        float cost = si.mana * (1.0f - 0.15f * s->ranks[SK_FOCUS]);
        snprintf(buf, sizeof buf, "%s  RANK %d  %.0f MANA%s", si.name, rank, cost, ps->book_sel == s->spell ? "  (ACTIVE)" : "");
        vec3 c = spell_is_attack(ps->book_sel) ? element_color(spell_element(ps->book_sel)) : v3(1.0f, 0.95f, 0.75f);
        font_text((W - font_text_width(buf)) / 2, iy, buf, c.x, c.y, c.z);
        if (spell_is_attack(ps->book_sel)) snprintf(buf, sizeof buf, "%s; %s", si.desc, spell_effect_text(ps->book_sel));
        else snprintf(buf, sizeof buf, "%s", si.desc);
        if ((int)strlen(buf) * 6 > W) buf[W / 6] = 0;
        font_text((W - font_text_width(buf)) / 2, iy + 10, buf, 0.72f, 0.7f, 0.62f);
    } else {
        snprintf(buf, sizeof buf, "UNKNOWN SPELL - FIND ITS SCROLL%s", si.tier ? " DEEPER IN" : "");
        font_text((W - font_text_width(buf)) / 2, iy, buf, 0.5f, 0.46f, 0.42f);
    }
    if (!l.compact) {
        snprintf(buf, sizeof buf, "SPELLS KNOWN %d/%d", known, SPELL_COUNT);
        font_text((W - font_text_width(buf)) / 2, iy + 22, buf, 0.65f, 0.6f, 0.5f);
        const char *help = touch ? "TAP A SPELL, TAP AGAIN TO MAKE IT ACTIVE - SPELL BUTTON CASTS"
                           : ps->pad && !(ps->keyboard && g->mouse) ? "A MAKE ACTIVE  X CASTS IN GAME  LB/RB TABS"
                                                                    : "ENTER MAKE ACTIVE  Q CASTS, R NEXT SPELL  [ ] TABS";
        font_text((W - font_text_width(help)) / 2, H - 10, help, 0.5f, 0.46f, 0.4f);
    }
    font_end();
}
