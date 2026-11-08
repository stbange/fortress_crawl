#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

#include <stdarg.h>

static void upper(char *s) {
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}

/* The SHOP tab: what gold buys.
 * Anywhere: scrap every weapon in the bag that is worse than the one of its class you wear.
 * At a shrine in the fortress: potions too.
 * At home in the Sanctum: everything: potions, a rank for the active spell, new affixes
 * for the weapon in hand, more of a relic you carry, the library's scroll, the stash, and
 * building the Sanctum up (gold, and keystones for the greater stages). */


typedef struct ShopItem {
    int kind, arg, price, keys;
    bool ok;            /* can be bought now */
    char label[72], why[40];
} ShopItem;

#define SHOP_MAX 160

/* building the Sanctum: per plot and stage, gold and keystones, and what it brings */
static const char *PLOT_NAMES[SP_COUNT] = { "THE ISLAND", "THE HALL", "THE BATH", "THE COURT", "THE DINING HALL", "THE LIBRARY", "THE GARDEN",
                                            "THE ARMORY" };
static const int COST[SP_COUNT][SANCTUM_STAGES + 1][2] = {
    [SP_ISLAND] = { { 0, 0 }, { 0, 0 }, { 1500, 1 }, { 5000, 2 } },
    [SP_HALL] = { { 0, 0 }, { 0, 0 }, { 1000, 1 }, { 3000, 2 } },
    [SP_BATH] = { { 0, 0 }, { 400, 0 }, { 1200, 1 }, { 3500, 2 } },
    [SP_COURT] = { { 0, 0 }, { 0, 0 }, { 600, 0 }, { 2500, 1 } },
    [SP_DINING] = { { 0, 0 }, { 500, 0 }, { 1500, 1 }, { 4000, 2 } },
    [SP_LIBRARY] = { { 0, 0 }, { 600, 0 }, { 1800, 1 }, { 5000, 2 } },
    [SP_GARDEN] = { { 0, 0 }, { 300, 0 }, { 1200, 0 }, { 3000, 1 } },
    [SP_ARMORY] = { { 0, 0 }, { 800, 0 }, { 2000, 1 }, { 5000, 2 } },
};
static const char *WHAT[SP_COUNT][SANCTUM_STAGES + 1] = {
    [SP_ISLAND] = { "", "", "twice as wide, room for three more plots", "towers, an outer wall, waterfalls" },
    [SP_HALL] = { "", "", "whole walls, a roof, banners, a canopy bed", "a throne, pillars, chandeliers" },
    [SP_BATH] = { "", "a cold pool: Refreshed for 10 min", "heated: Refreshed for 15 min", "a marble hall: Refreshed for 25 min" },
    [SP_COURT] = { "", "", "a fountain and lamps", "statues and hedges" },
    [SP_DINING] = { "", "a table: Well Fed for 10 min", "the long table and a fireplace: Well Fed for 20 min", "a kitchen: Well Fed for 30 min" },
    [SP_LIBRARY] = { "", "empty shelves", "books and a shrine: a scroll every 20 min", "a dome of glass: a scroll every 12 min" },
    [SP_GARDEN] = { "", "paths and lamps", "trees and a pond", "a walled garden with flowers" },
    [SP_ARMORY] = { "", "racks for 8 of your legendaries", "20 racks, the banners of your foes", "statues of your chapters" },
};
static float library_every(Game *g) { return sanctum_stage(g, SP_LIBRARY) >= 3 ? 720.0f : 1200.0f; }

/* the shrine the player stands at (within 4 m), or -1 */
static int shrine_here(Game *g, const PlayerSlot *ps) {
    int px = (int)floorf(ps->pl.pos.x), py = (int)floorf(ps->pl.pos.y), pz = (int)floorf(ps->pl.pos.z);
    for (int dy = -1; dy <= 2; dy++)
        for (int dz = -3; dz <= 3; dz++)
            for (int dx = -3; dx <= 3; dx++) {
                bool known;
                if (chunkmap_block(&g->map, px + dx, py + dy, pz + dz, &known) == B_SHRINE && known) return 0;
            }
    return -1;
}

bool shop_open_here(Game *g, const PlayerSlot *ps) { return realm_has(ps->pl.pos) || shrine_here(g, ps) >= 0; }

static float score(const Item *it) { return item_damage_mult(it) / fmaxf(0.3f, item_speed_mult(it)); }

static int scrap_value(const Item *it) { return (int)((4 + it->level * 2) * (1 + it->rarity * 2)); }

static void add(ShopItem *out, int *n, int kind, int arg, int price, bool ok, const char *why, const char *fmt, ...) {
    if (*n >= SHOP_MAX) return;
    ShopItem *it = &out[(*n)++];
    it->kind = kind; it->arg = arg; it->price = price; it->ok = ok; it->keys = 0;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(it->label, sizeof it->label, fmt, ap);
    va_end(ap);
    snprintf(it->why, sizeof it->why, "%s", why ? why : "");
}

/* The merchants at home (playtest 2): with the library, an alchemist sells three reagents a
 * day and the library a keystone; with the kitchen, a cook sells two rare goods. A "day" is
 * half an hour of play; each offer can be bought once a day. */
#define MERCHANT_DAY 1800.0f
enum { MERCHANT_SLOTS = 5 };
static u64 merchant_key(Game *g, int slot) {
    return mix64(g->world.seed ^ 0xDA11ULL ^ ((u64)(g->play_time / MERCHANT_DAY) << 8) ^ (u64)slot) | 1;
}
/* The alchemist moves in with the hall's second stage, the cook with the dining hall's. */
static bool alchemist_here(Game *g) { return sanctum_stage(g, SP_HALL) >= 2; }
static bool cook_here(Game *g) { return sanctum_stage(g, SP_DINING) >= 2; }

/* slot 0-2: a reagent, 3-4: a larder good. Returns false if the slot is not open. */
static bool merchant_offer(Game *g, int slot, bool *reagent, int *id, int *price) {
    u64 h = mix64(merchant_key(g, slot) ^ 0x5E11ULL);
    if (slot < 3) {
        if (!alchemist_here(g)) return false;
        *reagent = true;
        *id = (int)(h % RG_COUNT);
        *price = *id == RG_PHOENIX ? 800 : *id >= RG_MONSTER_COUNT ? 400 : 200;
        return true;
    }
    if (!cook_here(g)) return false;
    static const int GOODS[][2] = { { FD_SPICES, 250 }, { FD_ROYAL_JELLY, 300 }, { FD_STARFRUIT, 800 }, { FD_WINE, 120 }, { FD_HONEY, 120 }, { FD_SALT, 60 } };
    int k = (int)(h % 6);
    *reagent = false;
    *id = GOODS[k][0];
    *price = GOODS[k][1];
    return true;
}

/* Sealed weapons (gambling): three a day, at shrines and at home. You see the kind and the
 * grade, not what is inside; the dearest may hold one of this world's legendaries. */
#define SEALED_SLOTS 3
static const int SEALED_PRICE[SEALED_SLOTS] = { 300, 800, 1500 };
static Item sealed_item(Game *g, PlayerSlot *ps, int slot, bool *legend) {
    u64 h = mix64(merchant_key(g, 200 + slot) ^ (u64)ps->index);
    float level = (float)ps->st.level + (float)slot;
    *legend = false;
    if (slot == 2 && (h & 0xFF) < 10) { /* about 4% */
        int id = (int)((h >> 8) % UNIQUE_COUNT);
        if (!g->sanctum.trophy[id]) { *legend = true; return arms_unique(id, level + 2.0f); }
    }
    return item_roll(h, level, slot == 0 ? RAR_COMMON : slot == 1 ? RAR_MAGIC : RAR_RARE);
}

/* The forge: a weapon brought up to your level keeps everything it is; tempering raises its
 * rarity a step (an affix more, up to epic). */
static int reforge_price(const Stats *s) { return 150 * s->level; }
static int temper_price(int rarity, int *keys) {
    *keys = rarity >= RAR_MAGIC ? 1 : 0;
    return rarity == RAR_COMMON ? 600 : rarity == RAR_MAGIC ? 1500 : 4000;
}

static const struct { const char *name; int price, plot, stage; } DECOR[SD_COUNT] = {
    [SD_STATUE] = { "a statue of the founder in the court", 1500, SP_COURT, 1 },
    [SD_BANNERS] = { "banners of your house in the hall", 800, SP_HALL, 1 },
    [SD_CARPET] = { "a long carpet to the hall's north wall", 500, SP_HALL, 1 },
    [SD_BRAZIERS] = { "golden braziers round the court", 1200, SP_COURT, 1 },
    [SD_FLOWERS] = { "flower beds in the garden", 700, SP_GARDEN, 1 },
    [SD_KOI] = { "koi in the garden pond", 1000, SP_GARDEN, 2 },
    [SD_LANTERNS] = { "lanterns in the garden's trees", 900, SP_GARDEN, 2 },
    [SD_THRONE] = { "a golden throne", 5000, SP_HALL, 1 },
    [SD_CHANDELIER] = { "a crystal chandelier over the fire", 2500, SP_HALL, 1 },
    [SD_SUNDIAL] = { "a sundial and benches in the court", 600, SP_COURT, 1 },
    [SD_ORRERY] = { "an orrery in the library", 4000, SP_LIBRARY, 1 },
};

static int shop_items(Game *g, PlayerSlot *ps, ShopItem *out) {
    Stats *s = &ps->st;
    int n = 0;
    /* anywhere: clear the bag of what you would never use */
    int worse = 0, gold = 0;
    for (int i = 0; i < BAG_SIZE; i++) {
        const Item *it = &s->bag[i];
        if (!it->used) continue;
        int wc = WEAPON_BASES[it->base].wclass;
        if (score(it) <= score(&s->equipped[wc])) { worse++; gold += scrap_value(it); }
    }
    add(out, &n, SH_SCRAP, 0, -gold, worse > 0, worse ? NULL : "NOTHING WORSE IN THE BAG", "SCRAP %d WEAPONS WORSE THAN YOURS", worse);
    bool home = realm_has(ps->pl.pos);
    if (!home && shrine_here(g, ps) < 0) return n;
    char why[40];
    add(out, &n, SH_POTION_HP, 0, 40, healing_held(s) < POTION_HP_MAX, healing_held(s) < POTION_HP_MAX ? NULL : "YOU CARRY ALL YOU CAN",
        "HEALTH POTION (%d/%d)", s->potions_hp, POTION_HP_MAX);
    add(out, &n, SH_POTION_MP, 0, 30, s->potions_mp < POTION_MP_MAX, s->potions_mp < POTION_MP_MAX ? NULL : "YOU CARRY ALL YOU CAN",
        "MANA POTION (%d/%d)", s->potions_mp, POTION_MP_MAX);
    {   /* sealed weapons */
        int bag = 0;
        for (int i = 0; i < BAG_SIZE; i++) bag += s->bag[i].used;
        for (int k = 0; k < SEALED_SLOTS; k++) {
            bool legend;
            Item it = sealed_item(g, ps, k, &legend);
            bool sold = looted_has(g, merchant_key(g, 200 + k) ^ (u64)ps->index);
            const char *grade = item_grade(it.level);
            add(out, &n, SH_SEALED, k, SEALED_PRICE[k], !sold && bag < BAG_SIZE, sold ? "OPENED: MORE TOMORROW" : "THE BAG IS FULL",
                "A SEALED %s%s%s (%s)", grade, grade[0] ? " " : "", WEAPON_BASES[it.base].name, k == 0 ? "ANYTHING" : k == 1 ? "MAGIC OR BETTER" : "RARE OR BETTER");
        }
    }
    if (!home) return n; /* a shrine out there sells potions and sealed weapons; the rest is traded at home */
    {   /* the forge, by the hall's fire */
        const Item *it = &s->equipped[s->weapon];
        char name[64];
        item_name(it, name, sizeof name);
        bool low = it->level < s->level;
        add(out, &n, SH_REFORGE, s->weapon, reforge_price(s), it->used && low, "ALREADY AT YOUR LEVEL", "FORGE %s TO LEVEL %d", name, s->level);
        int keys, price = temper_price(it->rarity, &keys);
        bool ok = it->used && it->rarity < RAR_EPIC && !it->unique;
        add(out, &n, SH_TEMPER, s->weapon, price, ok, it->unique ? "A LEGENDARY IS WHAT IT IS" : "AS FINE AS THE FORGE CAN MAKE IT",
            "TEMPER %s: A STEP RARER", name);
        out[n - 1].keys = keys;
    }
    {
        int id = s->spell, r = s->spell_rank[id];
        SpellInfo si;
        spell_info(id, &si);
        bool ok = r > 0 && r < SPELL_MAX_RANK;
        add(out, &n, SH_RANK, id, 100 * r * r, ok, ok ? NULL : "AT ITS HIGHEST RANK", "%s: RANK %d -> %d", si.name, r, r + 1);
    }
    {
        const Item *it = &s->equipped[s->weapon];
        char name[64];
        item_name(it, name, sizeof name);
        bool ok = it->used && it->rarity > RAR_COMMON;
        add(out, &n, SH_REROLL, s->weapon, 50 + 20 * it->level, ok, ok ? NULL : "A PLAIN WEAPON HAS NONE", "NEW AFFIXES FOR %s", name);
    }
    for (int k = 0; k < RELIC_COUNT; k++)
        if (s->relics[k]) add(out, &n, SH_RELIC, k, 500 * (s->relics[k] + 1), true, NULL, "ANOTHER %s (%s)", RELICS[k].name, RELIC_BONUS_TEXT[RELICS[k].bonus]);
    if (sanctum_stage(g, SP_LIBRARY) >= 2 && sanctum_stage(g, SP_ISLAND) >= 2) {
        float t = g->sanctum.library_t;
        snprintf(why, sizeof why, "NEXT IN %d MIN", (int)ceilf(t / 60.0f));
        add(out, &n, SH_SCROLL, 0, 0, t <= 0, t <= 0 ? NULL : why, "TODAY'S SCROLL FROM THE LIBRARY");
    }
    /* building */
    for (int p = 0; p < SP_COUNT; p++) {
        int cur = sanctum_stage(g, p);
        if (cur >= SANCTUM_STAGES) continue;
        bool needs_island = (p == SP_LIBRARY || p == SP_GARDEN || p == SP_ARMORY) && sanctum_stage(g, SP_ISLAND) < 2;
        int gold = COST[p][cur + 1][0], keys = COST[p][cur + 1][1];
        bool ok = !needs_island && keys <= g->sanctum.keystones;
        add(out, &n, SH_BUILD, p, gold, ok, needs_island ? "NEEDS A WIDER ISLAND" : keys > g->sanctum.keystones ? "NOT ENOUGH KEYSTONES" : NULL,
            "BUILD %s %d: %s", PLOT_NAMES[p], cur + 1, WHAT[p][cur + 1]);
        out[n - 1].keys = keys;
    }
    /* the merchants */
    for (int k = 0; k < MERCHANT_SLOTS; k++) {
        bool rg;
        int id, price;
        if (!merchant_offer(g, k, &rg, &id, &price)) continue;
        bool sold = looted_has(g, merchant_key(g, k));
        add(out, &n, SH_MERCHANT, k, price, !sold, "SOLD: MORE TOMORROW", "%s: %s", rg ? "THE ALCHEMIST" : "THE COOK", rg ? REAGENTS[id].name : FOODS[id].name);
    }
    if (alchemist_here(g)) {
        bool sold = looted_has(g, merchant_key(g, 99));
        add(out, &n, SH_KEYSTONE, 99, 1500 + 500 * g->sanctum.keys_bought, !sold, "ONE A DAY: MORE TOMORROW", "A KEYSTONE (ONE A DAY, DEARER EACH TIME)");
        /* knowledge: a torn page a day, and the other half of any torn page you hold */
        int unknown = 0;
        for (int r = 0; r < ELIXIR_COUNT; r++) unknown += !((s->formulae | s->formulae_torn) & (1u << r));
        bool sold_page = looted_has(g, merchant_key(g, 98) ^ (u64)ps->index);
        add(out, &n, SH_PAGE, 0, 400, unknown && !sold_page, unknown ? "ONE A DAY: MORE TOMORROW" : "YOU HAVE SEEN THEM ALL",
            "THE ALCHEMIST: A TORN PAGE OF A FORMULA YOU DO NOT KNOW");
        for (int r = 0; r < ELIXIR_COUNT; r++)
            if ((s->formulae_torn & (1u << r)) && !(s->formulae & (1u << r)))
                add(out, &n, SH_COMPLETE, r, 900, true, NULL, "THE ALCHEMIST: THE OTHER HALF OF %s", ELIXIRS[r].name);
    }
    if (cook_here(g)) {
        int unknown = 0;
        for (int r = 0; r < RECIPE_COUNT; r++) unknown += !((s->recipes >> r) & 1);
        bool sold = looted_has(g, merchant_key(g, 97) ^ (u64)ps->index);
        add(out, &n, SH_RECIPE, 0, 300, unknown && !sold, unknown ? "ONE A DAY: MORE TOMORROW" : "YOU KNOW THEM ALL", "THE COOK: A RECIPE YOU DO NOT KNOW");
    }
    /* beauty */
    for (int d = 0; d < SD_COUNT; d++) {
        if ((g->world.sanctum_decor >> d) & 1) continue;
        bool island = DECOR[d].plot == SP_GARDEN || DECOR[d].plot == SP_LIBRARY;
        bool ok = sanctum_stage(g, DECOR[d].plot) >= DECOR[d].stage && (!island || sanctum_stage(g, SP_ISLAND) >= 2);
        snprintf(why, sizeof why, "NEEDS %s %d", PLOT_NAMES[DECOR[d].plot], DECOR[d].stage);
        add(out, &n, SH_DECOR, d, DECOR[d].price, ok, why, "FOR BEAUTY: %s", DECOR[d].name);
    }
    /* the stash */
    int bag = 0, stash = 0;
    for (int i = 0; i < BAG_SIZE; i++) bag += s->bag[i].used;
    for (int i = 0; i < STASH_SIZE; i++) stash += g->sanctum.stash[i].used;
    add(out, &n, SH_STORE, 0, 0, bag > 0 && stash < STASH_SIZE, bag == 0 ? "THE BAG IS EMPTY" : "THE STASH IS FULL",
        "STASH: PUT THE BAG'S WEAPONS AWAY (%d/%d)", stash, STASH_SIZE);
    for (int i = 0; i < STASH_SIZE; i++) {
        const Item *it = &g->sanctum.stash[i];
        if (!it->used) continue;
        char name[96];
        item_name(it, name, sizeof name);
        add(out, &n, SH_TAKE, i, 0, bag < BAG_SIZE, "THE BAG IS FULL", "TAKE %s", name);
    }
    return n;
}

static Item reroll(Item it, u64 seed) {
    Rng r = rng_make(seed);
    int naff = it.rarity == RAR_MAGIC ? 1 : 2;
    it.affix[0] = it.affix[1] = AF_NONE;
    for (int i = 0; i < naff; i++) {
        int a;
        do a = rng_range(&r, 1, AF_COUNT - 1);
        while (i == 1 && a == it.affix[0]);
        it.affix[i] = (u8)a;
    }
    return it;
}

static void buy(Game *g, PlayerSlot *ps, const ShopItem *it) {
    Stats *s = &ps->st;
    char buf[128];
    if (!it->ok) { game_banner(ps, it->why[0] ? it->why : "NOT NOW"); return; }
    if (it->price > s->gold) { game_banner(ps, "NOT ENOUGH GOLD"); sfx_ui(SFX_SPELL_FIZZLE, 0.8f); return; }
    if (it->keys > g->sanctum.keystones) { game_banner(ps, "NOT ENOUGH KEYSTONES"); sfx_ui(SFX_SPELL_FIZZLE, 0.8f); return; }
    if (it->price > 0) s->gold -= it->price;
    g->sanctum.keystones -= it->keys;
    switch (it->kind) {
    case SH_SCRAP: {
        int gold = 0, n = 0;
        for (int i = 0; i < BAG_SIZE; i++) {
            Item *b = &s->bag[i];
            if (!b->used) continue;
            int wc = WEAPON_BASES[b->base].wclass;
            if (score(b) <= score(&s->equipped[wc])) { gold += scrap_value(b); n++; memset(b, 0, sizeof *b); }
        }
        s->gold += gold;
        snprintf(buf, sizeof buf, "SCRAPPED %d WEAPONS: +%d GOLD", n, gold);
        sfx_ui(SFX_SCRAP_ITEM, 1.0f);
        break;
    }
    case SH_POTION_HP: s->potions_hp++; snprintf(buf, sizeof buf, "A HEALTH POTION"); sfx_ui(SFX_PICKUP_POTION, 1.0f); break;
    case SH_POTION_MP: s->potions_mp++; snprintf(buf, sizeof buf, "A MANA POTION"); sfx_ui(SFX_PICKUP_POTION, 1.0f); break;
    case SH_RANK: {
        int r = spell_learn(ps, it->arg);
        SpellInfo si;
        spell_info(it->arg, &si);
        snprintf(buf, sizeof buf, "%s - RANK %d", si.name, r);
        sfx_ui(SFX_PICKUP_SCROLL, 1.0f);
        break;
    }
    case SH_REROLL: {
        Item *w = &s->equipped[it->arg];
        *w = reroll(*w, mix64((u64)(g->time * 1000.0f) ^ (u64)s->gold * 2654435761ULL));
        char name[64];
        item_name(w, name, sizeof name);
        snprintf(buf, sizeof buf, "NOW: %s", name);
        sfx_ui(SFX_EQUIP, 1.0f);
        break;
    }
    case SH_RELIC:
        s->relics[it->arg]++;
        stats_refresh(ps);
        snprintf(buf, sizeof buf, "RELIC: %s", RELICS[it->arg].name);
        sfx_ui(SFX_PICKUP_RELIC, 1.0f);
        break;
    case SH_BUILD: {
        int stage = sanctum_stage(g, it->arg) + 1;
        sanctum_set_stage(g, it->arg, stage);
        snprintf(buf, sizeof buf, "%s, STAGE %d: %s", PLOT_NAMES[it->arg], stage, WHAT[it->arg][stage]);
        sfx_ui(SFX_SPELL_EARTH, 1.0f);
        break;
    }
    case SH_STORE: {
        int moved = 0;
        for (int i = 0; i < BAG_SIZE; i++) {
            if (!s->bag[i].used) continue;
            int k = 0;
            while (k < STASH_SIZE && g->sanctum.stash[k].used) k++;
            if (k >= STASH_SIZE) break;
            g->sanctum.stash[k] = s->bag[i];
            memset(&s->bag[i], 0, sizeof s->bag[i]);
            moved++;
        }
        snprintf(buf, sizeof buf, "%d WEAPONS PUT AWAY IN THE STASH", moved);
        sfx_ui(SFX_EQUIP, 1.0f);
        break;
    }
    case SH_TAKE: {
        int k = 0;
        while (k < BAG_SIZE && s->bag[k].used) k++;
        if (k >= BAG_SIZE) return;
        s->bag[k] = g->sanctum.stash[it->arg];
        memset(&g->sanctum.stash[it->arg], 0, sizeof g->sanctum.stash[it->arg]);
        char name[96];
        item_name(&s->bag[k], name, sizeof name);
        snprintf(buf, sizeof buf, "TAKEN: %s", name);
        sfx_ui(SFX_EQUIP, 1.0f);
        break;
    }
    case SH_SCROLL: {
        int id = (int)(mix64((u64)(g->time * 1000.0f) + 77) % SPELL_COUNT);
        int r = spell_learn(ps, id);
        SpellInfo si;
        spell_info(id, &si);
        g->sanctum.library_t = library_every(g);
        snprintf(buf, sizeof buf, "FROM THE LIBRARY: %s (RANK %d)", si.name, r);
        sfx_ui(SFX_PICKUP_SCROLL, 1.0f);
        break;
    }
    case SH_MERCHANT: {
        bool rg;
        int id, price;
        if (!merchant_offer(g, it->arg, &rg, &id, &price)) return;
        if (rg) alchemy_give(g, ps, id, 1);
        else food_give(g, ps, id, 1);
        looted_mark(g, merchant_key(g, it->arg));
        sfx_ui(SFX_PICKUP_GOLD, 0.8f);
        return;
    }
    case SH_SEALED: {
        bool legend;
        Item w = sealed_item(g, ps, it->arg, &legend);
        int slot = -1;
        for (int i = 0; i < BAG_SIZE && slot < 0; i++) if (!s->bag[i].used) slot = i;
        if (slot < 0) { s->gold += it->price; return; }
        s->bag[slot] = w;
        looted_mark(g, merchant_key(g, 200 + it->arg) ^ (u64)ps->index);
        if (legend) {
            looted_mark(g, mix64(g->world.seed ^ 0x11E6ULL ^ (u64)(w.unique - 1) * 0x9E3779B9ULL) | 1); /* no second one from its bearer */
            sanctum_trophy_unique(g, w.unique - 1);
            flash_add(g, player_eye(&ps->pl), v3(0.6f, 1.0f, 0.8f), 10.0f, 0.6f);
        }
        char name[96];
        item_name(&w, name, sizeof name);
        snprintf(buf, sizeof buf, "%s: %s", legend ? "A LEGENDARY" : "INSIDE", name);
        sfx_ui(legend ? SFX_PICKUP_RELIC : SFX_PICKUP_WEAPON, 1.0f);
        break;
    }
    case SH_REFORGE: {
        Item *w = &s->equipped[it->arg];
        w->level = (u8)MINI(250, s->level);
        char name[96];
        item_name(w, name, sizeof name);
        snprintf(buf, sizeof buf, "REFORGED: %s, LEVEL %d", name, w->level);
        sfx_ui(SFX_EQUIP, 1.0f);
        stats_refresh(ps);
        break;
    }
    case SH_TEMPER: {
        Item *w = &s->equipped[it->arg];
        u64 h = mix64((u64)(g->time * 1000.0f) ^ (u64)w->kills ^ 0x7E5ULL);
        w->rarity++;
        /* a new affix where there is room for one */
        int slots = w->rarity >= RAR_RARE ? 2 : w->rarity >= RAR_MAGIC ? 1 : 0;
        for (int k = 0; k < slots; k++)
            if (w->affix[k] == AF_NONE) {
                int a;
                do { h = mix64(h + 1); a = 1 + (int)(h % (AF_COUNT - 1)); } while (k == 1 && a == w->affix[0]);
                w->affix[k] = (u8)a;
            }
        char name[96];
        item_name(w, name, sizeof name);
        snprintf(buf, sizeof buf, "TEMPERED: %s", name);
        sfx_ui(SFX_PICKUP_RELIC, 1.0f);
        break;
    }
    case SH_PAGE: {
        int unknown[ELIXIR_COUNT], k = 0;
        for (int r = 0; r < ELIXIR_COUNT; r++) if (!((s->formulae | s->formulae_torn) & (1u << r))) unknown[k++] = r;
        if (!k) { s->gold += it->price; return; }
        looted_mark(g, merchant_key(g, 98) ^ (u64)ps->index);
        alchemy_learn_torn(g, ps, unknown[mix64((u64)(g->time * 1000.0f)) % (u64)k]);
        return;
    }
    case SH_COMPLETE: alchemy_learn_torn(g, ps, it->arg); return;
    case SH_RECIPE: {
        int unknown[RECIPE_COUNT], k = 0;
        for (int r = 0; r < RECIPE_COUNT; r++) if (!((s->recipes >> r) & 1)) unknown[k++] = r;
        if (!k) { s->gold += it->price; return; }
        int r = unknown[mix64((u64)(g->time * 1000.0f) ^ 0xC00C) % (u64)k];
        s->recipes |= 1u << r;
        looted_mark(g, merchant_key(g, 97) ^ (u64)ps->index);
        char need[64];
        food_need_text(&RECIPES[r], need, sizeof need, false);
        snprintf(buf, sizeof buf, "THE COOK TEACHES YOU %s: %s", RECIPES[r].name, need);
        sfx_ui(SFX_RECIPE_LEARNED, 1.0f);
        break;
    }
    case SH_DECOR:
        sanctum_add_decor(g, it->arg);
        snprintf(buf, sizeof buf, "FOR BEAUTY: %s", DECOR[it->arg].name);
        sfx_ui(SFX_PICKUP_RELIC, 0.8f);
        break;
    case SH_KEYSTONE:
        g->sanctum.keystones++;
        g->sanctum.keys_bought++;
        looted_mark(g, merchant_key(g, 99));
        snprintf(buf, sizeof buf, "A KEYSTONE (%d) - FOR BUILDING THE SANCTUM", g->sanctum.keystones);
        sfx_ui(SFX_PICKUP_RELIC, 1.0f);
        break;
    default: return;
    }
    upper(buf);
    game_banner(ps, buf);
    g->autosave_timer = 1e9f;
}

/* Buys the offer of that kind with that argument (a plot to build), if it can. */
bool shop_buy_arg(Game *g, PlayerSlot *ps, int kind, int arg) {
    ShopItem items[SHOP_MAX];
    int n = shop_items(g, ps, items);
    for (int i = 0; i < n; i++)
        if (items[i].kind == kind && items[i].arg == arg && items[i].ok && items[i].price <= ps->st.gold && items[i].keys <= g->sanctum.keystones) {
            buy(g, ps, &items[i]);
            return true;
        }
    return false;
}

/* Buys the first offer of that kind that can be bought (for the playtest bot). */
bool shop_buy_kind(Game *g, PlayerSlot *ps, int kind) {
    ShopItem items[SHOP_MAX];
    int n = shop_items(g, ps, items);
    for (int i = 0; i < n; i++)
        if (items[i].kind == kind && items[i].ok && items[i].price <= ps->st.gold) {
            int gold = ps->st.gold;
            buy(g, ps, &items[i]);
            return ps->st.gold != gold || kind == SH_SCRAP;
        }
    return false;
}

void shop_nav(Game *g, PlayerSlot *ps, int nav) {
    ShopItem items[SHOP_MAX];
    int n = shop_items(g, ps, items);
    if (nav == NAV_UP) ps->shop_sel = (ps->shop_sel + n - 1) % n;
    else if (nav == NAV_DOWN) ps->shop_sel = (ps->shop_sel + 1) % n;
    else if (nav == NAV_BUY && ps->shop_sel < n) buy(g, ps, &items[ps->shop_sel]);
    if (ps->shop_sel >= n) ps->shop_sel = n - 1;
}

#define ROW 11

/* the rows that fit, and the first one shown (the list scrolls with the selection) */
static int rows_fit(int H, int top) { return MAXI(3, (H - top - 22 - 18) / ROW); }
static int first_row(const PlayerSlot *ps, int n, int fit) {
    int f = ps->shop_sel - fit / 2;
    return MAXI(0, MINI(f, n - fit));
}

void shop_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y) {
    (void)W; (void)x;
    ShopItem items[SHOP_MAX];
    int n = shop_items(g, ps, items);
    int fit = rows_fit(H, top), f = first_row(ps, n, fit);
    int row = (int)floorf((y - (top + 22)) / ROW);
    if (row < 0 || row >= fit || f + row >= n) return;
    row += f;
    if (row == ps->shop_sel) buy(g, ps, &items[row]);
    else ps->shop_sel = row;
}

void shop_draw(Game *g, PlayerSlot *ps, int W, int H, int top) {
    ShopItem items[SHOP_MAX];
    int n = shop_items(g, ps, items);
    if (ps->shop_sel >= n) ps->shop_sel = n - 1;
    int fit = rows_fit(H, top), f = first_row(ps, n, fit);
    int x = MAXI(6, (W - 74 * 6) / 2), y = top + 6;
    char buf[160];
    batch2d_begin(W, H);
    for (int i = f; i < n && i < f + fit; i++) {
        float ry = (float)(top + 22 + (i - f) * ROW) - 2.0f;
        bool sel = i == ps->shop_sel;
        batch2d_rect((float)x - 3, ry, (float)(W - 2 * x + 6), ROW - 1, sel ? 0.25f : 0.08f, sel ? 0.18f : 0.06f, sel ? 0.1f : 0.05f, 0.85f);
    }
    batch2d_flush();
    font_begin(W, H);
    bool home = realm_has(ps->pl.pos);
    snprintf(buf, sizeof buf, "GOLD %d   KEYSTONES %d%s", ps->st.gold, g->sanctum.keystones,
             home ? "   (HOME)" : n > 1 ? "   (A SHRINE: POTIONS. THE REST AT HOME)" : "   (SHRINES SELL POTIONS, THE SANCTUM ALL)");
    font_text(x, y, buf, 1.0f, 0.82f, 0.35f);
    if (f > 0) font_text(W - x - 6, top + 12, "^", 0.8f, 0.7f, 0.5f);
    if (f + fit < n) font_text(W - x - 6, top + 22 + fit * ROW - 2, "V", 0.8f, 0.7f, 0.5f);
    for (int i = f; i < n && i < f + fit; i++) {
        const ShopItem *it = &items[i];
        int ry = top + 22 + (i - f) * ROW;
        float k = it->ok ? 1.0f : 0.5f;
        snprintf(buf, sizeof buf, "%s", it->label);
        upper(buf);
        int maxc = (W - 2 * x) / 6 - 14;
        if ((int)strlen(buf) > maxc) buf[MAXI(0, maxc)] = 0;
        font_text(x, ry, buf, 0.9f * k, 0.85f * k, 0.75f * k);
        if (it->ok && it->price && it->keys) snprintf(buf, sizeof buf, "%d +%dK", it->price, it->keys);
        else if (it->ok && it->price) snprintf(buf, sizeof buf, it->price < 0 ? "+%d" : "%d", abs(it->price));
        else snprintf(buf, sizeof buf, "%s", it->ok ? "FREE" : it->why);
        font_text(W - x - font_text_width(buf), ry, buf, it->price > ps->st.gold && it->ok ? 1.0f : 0.95f * k,
                  it->price > ps->st.gold && it->ok ? 0.4f : 0.8f * k, 0.35f * k);
    }
    font_text(x, H - 14, "UP/DOWN CHOOSE   ENTER BUY", 0.6f, 0.55f, 0.45f);
    font_end();
}
