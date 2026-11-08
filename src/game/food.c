#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

#include <stdio.h>
#include <string.h>

/* Food and cooking (docs/FOOD.md). Ingredients come from wildlife (wildlife.c), plants and
 * larders (items.c); they go into the larder (Stats.food). Cooked at a fire, at the
 * Sanctum's fireplace or in its kitchen, they become dishes (Stats.dishes); a dish eaten
 * is the meal, one at a time, for 10 to 20 minutes. Recipes are lists of tags, so any
 * meat with something sharp is a spicy skewer. */

#define T(x) TGB(TG_##x)
const FoodDef FOODS[FD_COUNT] = {
    [FD_HARE] = { "hare haunch", T(MEAT) | T(LEAN), { 0.75f, 0.4f, 0.35f }, FM_MEAT },
    [FD_FOWL] = { "fowl", T(MEAT), { 0.9f, 0.75f, 0.6f }, FM_MEAT },
    [FD_GROUSE] = { "grouse", T(MEAT) | T(FROST), { 0.85f, 0.82f, 0.88f }, FM_MEAT },
    [FD_QUAIL] = { "quail", T(MEAT) | T(SWIFT), { 0.8f, 0.6f, 0.45f }, FM_MEAT },
    [FD_GOAT] = { "goat meat", T(MEAT) | T(HEARTY), { 0.7f, 0.3f, 0.3f }, FM_MEAT },
    [FD_MUTTON] = { "mutton", T(MEAT) | T(HEARTY) | T(FAT), { 0.82f, 0.45f, 0.4f }, FM_MEAT },
    [FD_SALAMANDER] = { "salamander tail", T(MEAT) | T(FIRE), { 1.0f, 0.4f, 0.15f }, FM_MEAT },
    [FD_EGG] = { "hen egg", T(EGG), { 0.95f, 0.9f, 0.8f }, FM_EGG },
    [FD_QUAIL_EGG] = { "quail egg", T(EGG) | T(SWIFT), { 0.85f, 0.8f, 0.62f }, FM_EGG },
    [FD_MILK] = { "goat milk", T(DAIRY), { 0.95f, 0.95f, 0.9f }, FM_BOTTLE },
    [FD_CAVE_FISH] = { "cave fish", T(FISH), { 0.75f, 0.8f, 0.85f }, FM_FISH },
    [FD_SKY_CARP] = { "sky carp", T(FISH) | T(HOLY), { 1.0f, 0.85f, 0.5f }, FM_FISH },
    [FD_CRAB] = { "crab", T(FISH) | T(SALTY), { 0.9f, 0.35f, 0.2f }, FM_SHELL },
    [FD_HONEY] = { "honeycomb", T(SWEET), { 1.0f, 0.75f, 0.2f }, FM_COMB },
    [FD_ROYAL_JELLY] = { "royal jelly", T(SWEET) | T(RARE) | T(MANA), { 1.0f, 0.95f, 0.7f }, FM_BOTTLE },
    [FD_BEETLE_JELLY] = { "beetle jelly", T(SWEET) | T(MANA) | T(GLOW), { 0.4f, 1.0f, 0.7f }, FM_BOTTLE },
    [FD_SNAIL] = { "salt snail", T(EARTHY) | T(SALTY), { 0.82f, 0.76f, 0.7f }, FM_SHELL },
    [FD_MUSHROOM] = { "cave mushroom", T(EARTHY), { 0.75f, 0.6f, 0.45f }, FM_MUSHROOM },
    [FD_GLOWCAP] = { "glow cap", T(EARTHY) | T(MANA) | T(GLOW), { 0.4f, 0.7f, 1.0f }, FM_MUSHROOM },
    [FD_HERB] = { "wild herb", T(FRESH), { 0.4f, 0.8f, 0.3f }, FM_SPRIG },
    [FD_MOSS] = { "river moss", T(FRESH) | T(EARTHY), { 0.3f, 0.6f, 0.35f }, FM_SPRIG },
    [FD_TUBER] = { "root tuber", T(STAPLE) | T(EARTHY), { 0.65f, 0.5f, 0.3f }, FM_ROOT },
    [FD_BAMBOO] = { "bamboo shoot", T(FRESH) | T(STAPLE), { 0.7f, 0.85f, 0.4f }, FM_SPRIG },
    [FD_JADE_LEAF] = { "jade leaf", T(FRESH) | T(MANA), { 0.3f, 0.85f, 0.6f }, FM_SPRIG },
    [FD_PEPPER] = { "ember pepper", T(FIRE) | T(SHARP), { 1.0f, 0.25f, 0.1f }, FM_BERRY },
    [FD_ONION] = { "brimstone onion", T(SHARP) | T(FIRE), { 0.9f, 0.8f, 0.3f }, FM_ROOT },
    [FD_FROSTBERRY] = { "frost berry", T(FROST) | T(SWEET), { 0.5f, 0.7f, 1.0f }, FM_BERRY },
    [FD_MELON] = { "sun melon", T(SWEET) | T(FRESH), { 1.0f, 0.8f, 0.3f }, FM_MELON },
    [FD_SEAGRAPE] = { "sea grapes", T(FRESH) | T(SALTY), { 0.3f, 0.75f, 0.55f }, FM_BERRY },
    [FD_BARROW_ROOT] = { "barrow root", T(BITTER), { 0.55f, 0.45f, 0.5f }, FM_ROOT },
    [FD_GRAIN] = { "grain", T(STAPLE), { 0.9f, 0.8f, 0.5f }, FM_SACK },
    [FD_SALT] = { "salt", T(SALTY), { 0.95f, 0.95f, 0.95f }, FM_SACK },
    [FD_OIL] = { "oil", T(FAT), { 0.85f, 0.75f, 0.3f }, FM_BOTTLE },
    [FD_WINE] = { "wine", T(HOLY) | T(MANA), { 0.6f, 0.1f, 0.25f }, FM_BOTTLE },
    [FD_SPICES] = { "trade spices", T(SHARP) | T(RARE), { 0.8f, 0.45f, 0.2f }, FM_SACK },
    [FD_STARFRUIT] = { "star fruit", 0xFFFFFFFFu, { 1.0f, 1.0f, 0.6f }, FM_MELON },
};

#define N TG_NONE
#define A TG_ANY
const Recipe RECIPES[RC_ALL] = {
    /* campfire: two ingredients */
    { "roast", 2, { TG_MEAT, TG_SALTY, N, N }, { { ME_MAXHP, 0.15f } }, "+15% max health" },
    { "spicy skewer", 2, { TG_MEAT, TG_SHARP, N, N }, { { ME_DAMAGE, 0.10f } }, "+10% weapon damage" },
    { "grilled fish", 2, { TG_FISH, TG_FRESH, N, N }, { { ME_REGEN, 1.0f } }, "health comes back twice as fast out of a fight" },
    { "fried eggs", 2, { TG_EGG, TG_FAT, N, N }, { { ME_MAXHP, 0.10f }, { ME_MAXMANA, 0.10f } }, "+10% max health and mana" },
    { "fried caps", 2, { TG_EARTHY, TG_FAT, N, N }, { { ME_MAXMANA, 0.20f } }, "+20% max mana" },
    { "glow tea", 2, { TG_MANA, TG_FRESH, N, N }, { { ME_MANA_REGEN, 1.0f } }, "mana flows twice as fast" },
    { "honey bread", 2, { TG_SWEET, TG_STAPLE, N, N }, { { ME_SPEED, 0.15f } }, "+15% movement speed" },
    { "flatbread", 2, { TG_STAPLE, TG_SALTY, N, N }, { { ME_MAXHP, 0.05f }, { ME_TRAVEL, 1.0f } }, "travel food: lasts three times as long, +5% health" },
    { "frost sorbet", 2, { TG_FROST, TG_SWEET, N, N }, { { ME_FROST_RES, 1.0f } }, "frost hurts less, never chilled" },
    { "pepper jerky", 2, { TG_MEAT, TG_FIRE, N, N }, { { ME_FIRE_RES, 1.0f } }, "fire hurts less, burns end at once" },
    { "bitter tonic", 2, { TG_BITTER, TG_FRESH, N, N }, { { ME_POISON_IMM, 1.0f } }, "never poisoned" },
    { "sweet curd", 2, { TG_DAIRY, TG_SWEET, N, N }, { { ME_POTION, 0.30f } }, "potions heal 30% more" },
    { "crab on the coals", 2, { TG_FISH, TG_SALTY, N, N }, { { ME_PROJ_RES, 0.15f } }, "projectiles hurt 15% less" },
    { "glowing broth", 2, { TG_GLOW, TG_SALTY, N, N }, { { ME_LIGHT, 1.0f } }, "a light round you, and hidden things show" },
    /* fireplace: three */
    { "hunter's stew", 3, { TG_MEAT, TG_EARTHY, TG_FRESH, N }, { { ME_MAXHP, 0.10f }, { ME_FIGHT_REGEN, 0.004f } },
      "+10% max health, health comes back even in a fight" },
    { "omelette", 3, { TG_EGG, TG_DAIRY, TG_FRESH, N }, { { ME_DAMAGE, 0.08f }, { ME_MAXHP, 0.08f } }, "+8% weapon damage, +8% max health" },
    { "mushroom pie", 3, { TG_EARTHY, TG_STAPLE, TG_FAT, N }, { { ME_MAXMANA, 0.25f }, { ME_SPELL_POWER, 0.05f } },
      "+25% max mana, spells 5% stronger" },
    { "fire broth", 3, { TG_FIRE, TG_EARTHY, TG_SALTY, N }, { { ME_FIRE_RES, 1.0f }, { ME_LAVA, 0.5f } }, "fire hurts less, lava burns half as much" },
    { "honey cake", 3, { TG_SWEET, TG_STAPLE, TG_EGG, N }, { { ME_SPEED, 0.20f }, { ME_JUMP, 0.15f } }, "+20% movement speed, jump higher" },
    { "milk porridge", 3, { TG_DAIRY, TG_STAPLE, TG_SWEET, N }, { { ME_STUN_RES, 0.5f } }, "blows push you half as far" },
    { "root mash", 3, { TG_STAPLE, TG_DAIRY, TG_SALTY, N }, { { ME_MAXHP, 0.20f } }, "+20% max health" },
    { "sea-grape salad", 3, { TG_FRESH, TG_SALTY, TG_FISH, N }, { { ME_POISON_IMM, 1.0f }, { ME_FIGHT_REGEN, 0.002f } },
      "never poisoned, health comes back slowly" },
    { "grouse pot", 3, { TG_FROST, TG_MEAT, TG_EARTHY, N }, { { ME_FROST_RES, 1.0f }, { ME_MAXHP, 0.10f } }, "frost hurts less, +10% max health" },
    { "quail on a stick", 3, { TG_SWIFT, TG_MEAT, TG_FIRE, N }, { { ME_ATTACK_SPEED, 0.10f } }, "attacks 10% faster" },
    /* kitchen: four */
    { "salamander curry", 4, { TG_FIRE, TG_SHARP, TG_MEAT, TG_STAPLE }, { { ME_DAMAGE, 0.15f }, { ME_IGNITE, 0.2f } },
      "+15% weapon damage, hits may set foes alight" },
    { "dragon's-breath chili", 4, { TG_FIRE, TG_SHARP, TG_SHARP, TG_FAT }, { { ME_DAMAGE, 0.25f }, { ME_TAKEN, 0.10f } },
      "+25% weapon damage, but you take 10% more" },
    { "pilgrim's supper", 4, { TG_HOLY, TG_MANA, TG_STAPLE, TG_FRESH }, { { ME_SPELL_COST, 0.15f } }, "spells cost 15% less" },
    { "sky-carp pie", 4, { TG_FISH, TG_HOLY, TG_STAPLE, TG_FAT }, { { ME_FALL, 0.5f }, { ME_GLIDE, 1.0f } }, "you drift down slowly, falls hurt half" },
    { "jade tea ceremony", 4, { TG_MANA, TG_FRESH, TG_SWEET, TG_GLOW }, { { ME_MANA_REGEN, 1.0f }, { ME_SPELL_POWER, 0.10f } },
      "mana flows twice as fast, spells 10% stronger" },
    { "royal tart", 4, { TG_RARE, TG_SWEET, TG_DAIRY, TG_STAPLE }, { { ME_COOLDOWN, 0.20f } }, "spell cooldowns 20% shorter" },
    { "fisherman's chowder", 4, { TG_FISH, TG_DAIRY, TG_SALTY, TG_EARTHY }, { { ME_MAXHP, 0.15f }, { ME_MAXMANA, 0.15f } },
      "+15% max health and mana" },
    { "feast of the far reaches", 4, { TG_STAR, A, A, A },
      { { ME_MAXHP, 0.10f }, { ME_MAXMANA, 0.10f }, { ME_DAMAGE, 0.10f }, { ME_SPEED, 0.10f } }, "a little of everything" },
    /* anything else */
    { "a simple meal", 0, { N, N, N, N }, { { ME_MAXHP, 0.05f } }, "+5% max health" },
};
#undef N
#undef A

static const char *TAG_NAMES[TG_COUNT] = { "meat", "fish", "egg", "dairy", "staple", "earthy", "fresh", "sweet", "sharp", "salty", "fat",
                                           "fire", "frost", "mana", "holy", "hearty", "rare", "swift", "glow", "bitter", "lean", "star fruit" };

static void upper(char *s) {
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}

/* ---------- recipes ---------- */

/* Can these ingredients fill these needs, one each? */
static bool fill_from(const Recipe *r, const u8 *ings, int n, int k, unsigned used) {
    if (k >= n) return true;
    if (k >= 4) return false;
    u8 t = r->need[k];
    for (int i = 0; i < n; i++) {
        if (used & (1u << i)) continue;
        if (t != TG_ANY && !(FOODS[ings[i]].tags & TGB(t))) continue;
        if (fill_from(r, ings, n, k + 1, used | (1u << i))) return true;
    }
    return false;
}
static bool fills(const Recipe *r, const u8 *ings, int n) { return r->slots == n && fill_from(r, ings, n, 0, 0); }

int food_match(const u8 *ings, int n) {
    if (n < 2 || n > 4) return RC_SIMPLE;
    for (int r = RECIPE_COUNT - 1; r >= 0; r--) /* the feast first: star fruit fills anything */
        if (RECIPES[r].need[0] == TG_STAR && fills(&RECIPES[r], ings, n)) return r;
    for (int r = 0; r < RECIPE_COUNT; r++)
        if (RECIPES[r].need[0] != TG_STAR && fills(&RECIPES[r], ings, n)) return r;
    return RC_SIMPLE;
}

int food_quality(const u8 *ings, int n, int place) {
    int q = place == CP_KITCHEN ? 1 : 0;
    for (int i = 0; i < n; i++)
        if (FOODS[ings[i]].tags & (TGB(TG_HEARTY) | TGB(TG_RARE))) q++;
    return MINI(q, DISH_QUALITIES - 1);
}

float food_meal_seconds(int recipe, int place) {
    float t = place >= CP_KITCHEN ? 1200.0f : place == CP_FIREPLACE ? 900.0f : 600.0f;
    for (int k = 0; k < RECIPE_EFFECTS; k++)
        if (RECIPES[recipe].fx[k].e == ME_TRAVEL && RECIPES[recipe].fx[k].v > 0) t *= 3.0f;
    return t;
}

const char *food_quality_name(int q) {
    static const char *Q[DISH_QUALITIES] = { "", "fine ", "excellent ", "legendary " };
    return Q[q < 0 ? 0 : q >= DISH_QUALITIES ? DISH_QUALITIES - 1 : q];
}

void food_need_text(const Recipe *r, char *out, int n, bool hint) {
    out[0] = 0;
    for (int k = 0; k < r->slots; k++) {
        const char *t = r->need[k] == TG_ANY ? "anything" : r->need[k] < TG_COUNT ? TAG_NAMES[r->need[k]] : "?";
        if (hint && k > 0) t = "?";
        snprintf(out + strlen(out), (size_t)(n - (int)strlen(out)), "%s%s", k ? " + " : "", t);
    }
}

void food_tags_text(u32 tags, char *out, int n) {
    out[0] = 0;
    if (tags == 0xFFFFFFFFu) { snprintf(out, (size_t)n, "everything"); return; }
    for (int t = 0; t < TG_COUNT; t++)
        if (tags & TGB(t)) snprintf(out + strlen(out), (size_t)(n - (int)strlen(out)), "%s%s", out[0] ? ", " : "", TAG_NAMES[t]);
}

/* ---------- the meal's effects ---------- */

/* What a raw ingredient does for a minute and a half: the strongest of its tags, half strength. */
static bool raw_effect(int food, int *e, float *v) {
    u32 t = FOODS[food].tags;
    if (food == FD_STARFRUIT) { *e = ME_MAXHP; *v = 0.1f; return true; }
    if (t & TGB(TG_FIRE)) { *e = ME_FIRE_RES; *v = 1.0f; return true; }
    if (t & TGB(TG_FROST)) { *e = ME_FROST_RES; *v = 1.0f; return true; }
    if (t & TGB(TG_BITTER)) { *e = ME_POISON_IMM; *v = 1.0f; return true; }
    if (t & TGB(TG_GLOW)) { *e = ME_LIGHT; *v = 1.0f; return true; }
    if (t & TGB(TG_SWIFT)) { *e = ME_SPEED; *v = 0.08f; return true; }
    if (t & TGB(TG_MANA)) { *e = ME_MANA_REGEN; *v = 0.5f; return true; }
    if (t & TGB(TG_HOLY)) { *e = ME_SPELL_POWER; *v = 0.05f; return true; }
    if (t & TGB(TG_HEARTY)) { *e = ME_MAXHP; *v = 0.05f; return true; }
    return false;
}

float food_fx(const Stats *s, int effect) {
    float v = 0;
    if (s->meal && s->meal_t > 0) {
        const Recipe *r = &RECIPES[s->meal - 1];
        float k = 1.0f + 0.25f * s->meal_q;
        for (int i = 0; i < RECIPE_EFFECTS; i++)
            if (r->fx[i].e == effect && r->fx[i].v > 0) {
                float x = r->fx[i].v;
                /* switches stay switches; amounts grow with quality */
                bool flag = effect == ME_FIRE_RES || effect == ME_FROST_RES || effect == ME_POISON_IMM || effect == ME_LIGHT || effect == ME_GLIDE ||
                            effect == ME_TRAVEL || effect == ME_TAKEN;
                v += flag ? x : x * k;
            }
    }
    if (s->snack && s->snack_t > 0) {
        int e;
        float x;
        if (raw_effect(s->snack - 1, &e, &x) && e == effect) v += x;
    }
    v += alchemy_fx(s, effect); /* an elixir working */
    if (s->well_fed_t > 0) { /* the Sanctum's feast */
        if (effect == ME_MAXHP) v += 0.2f;
        if (effect == ME_FIGHT_REGEN) v += 0.004f;
    }
    return v;
}

/* ---------- the larder ---------- */

#define LARDER_MAX 20

int food_held(const Stats *s) {
    int n = 0;
    for (int i = 0; i < FD_COUNT; i++) n += s->food[i];
    return n;
}

int food_dishes_held(const Stats *s) {
    int n = 0;
    for (int r = 0; r < RC_ALL; r++)
        for (int q = 0; q < DISH_QUALITIES; q++)
            for (int p = 0; p < CP_COUNT; p++) n += s->dishes[r][q][p];
    return n;
}

void food_give(Game *g, PlayerSlot *ps, int food, int n) {
    if (food < 0 || food >= FD_COUNT || n <= 0) return;
    Stats *s = &ps->st;
    int got = MINI(n, LARDER_MAX - s->food[food]);
    char buf[96], name[48];
    snprintf(name, sizeof name, "%s", FOODS[food].name);
    upper(name);
    if (got <= 0) {
        snprintf(buf, sizeof buf, "NO ROOM FOR MORE %s", name);
        if (ps->banner_time <= 0) game_banner(ps, buf);
        return;
    }
    s->food[food] = (u8)(s->food[food] + got);
    g_tele.food++;
    if (got > 1) snprintf(buf, sizeof buf, "%d %s (%d)", got, name, s->food[food]);
    else snprintf(buf, sizeof buf, "%s (%d)", name, s->food[food]);
    game_banner(ps, buf);
    sfx_player(g, ps, SFX_PICKUP_FOOD, 0.8f);
}

/* ingredients of a kind not already in the pot */
static int free_count(const PlayerSlot *ps, int food) {
    int n = ps->st.food[food];
    for (int i = 0; i < ps->npot; i++)
        if (ps->pot[i] == food) n--;
    return n;
}

/* ---------- where to cook ---------- */

int food_cook_place(Game *g, const PlayerSlot *ps) {
    vec3 p = ps->pl.pos;
    if (realm_has(p)) {
        int ds = sanctum_stage(g, SP_DINING), x0 = plot_x(SP_DINING), z0 = plot_z(SP_DINING);
        if (ds >= 2 && p.x >= x0 && p.x < x0 + 32 && p.z >= z0 && p.z < z0 + 32) return ds >= 3 ? CP_KITCHEN : CP_FIREPLACE;
    }
    int px = (int)floorf(p.x), py = (int)floorf(p.y), pz = (int)floorf(p.z);
    for (int dy = -1; dy <= 2; dy++)
        for (int dz = -2; dz <= 2; dz++)
            for (int dx = -2; dx <= 2; dx++) {
                bool known;
                Block b = chunkmap_block(&g->map, px + dx, py + dy, pz + dz, &known);
                if (known && (b == B_FIRE || b == B_BRAZIER)) return CP_CAMPFIRE;
            }
    for (int i = 0; i < MAX_EFFECTS; i++) {
        const Effect *e = &g->effects[i];
        if (e->alive && e->type == EF_HEARTH && v3_len(v3_sub(e->pos, p)) < 4.5f) return CP_CAMPFIRE;
    }
    return CP_NONE;
}

static const char *PLACE_NAMES[CP_COUNT] = { "", "the campfire", "the fireplace", "the kitchen" };

/* ---------- cooking and eating ---------- */

bool food_pot_add(Game *g, PlayerSlot *ps, int food) {
    int place = food_cook_place(g, ps);
    if (place == CP_NONE) return false;
    if (ps->npot >= place + 1) {
        game_banner(ps, place == CP_CAMPFIRE ? "A CAMPFIRE TAKES TWO: COOK, OR CLEAR THE POT" : "THE POT IS FULL: COOK, OR CLEAR IT");
        return false;
    }
    if (free_count(ps, food) <= 0) return false;
    ps->pot[ps->npot++] = (u8)food;
    sfx_ui(SFX_PICKUP_FOOD, 0.5f);
    return true;
}

bool food_cook(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    int place = food_cook_place(g, ps);
    if (place == CP_NONE) { game_banner(ps, "COOK AT A FIRE, A HEARTH OR AT HOME"); return false; }
    if (ps->npot < 2) { game_banner(ps, "PUT TWO OR MORE INGREDIENTS IN THE POT"); return false; }
    for (int i = 0; i < ps->npot; i++)
        if (s->food[ps->pot[i]] == 0) { ps->npot = 0; return false; }
    int r = food_match(ps->pot, ps->npot), q = r == RC_SIMPLE ? 0 : food_quality(ps->pot, ps->npot, place);
    for (int i = 0; i < ps->npot; i++) s->food[ps->pot[i]]--;
    ps->npot = 0;
    if (s->dishes[r][q][place] < 250) s->dishes[r][q][place]++;
    g_tele.cooked++;
    char buf[128], name[64];
    snprintf(name, sizeof name, "%s%s", food_quality_name(q), RECIPES[r].name);
    upper(name);
    if (r < RECIPE_COUNT && !(s->recipes & (1u << r))) {
        s->recipes |= 1u << r;
        snprintf(buf, sizeof buf, "A NEW RECIPE: %s", name);
        sfx_ui(SFX_RECIPE_LEARNED, 1.0f);
    } else snprintf(buf, sizeof buf, "COOKED: %s", name);
    game_banner(ps, buf);
    vec3 at = v3_add(ps->pl.pos, v3(cosf(ps->pl.yaw) * 0.8f, 0.9f, sinf(ps->pl.yaw) * 0.8f));
    sfx_player(g, ps, SFX_COOK, 0.9f);
    particles_burst(g, at, v3(0.85f, 0.85f, 0.8f), 14, 1.2f, 0.06f, false); /* steam */
    particles_burst(g, at, v3(1.0f, 0.6f, 0.2f), 6, 2.0f, 0.03f, true);
    return true;
}

bool food_eat_dish(Game *g, PlayerSlot *ps, int r, int q, int place) {
    Stats *s = &ps->st;
    if (s->dead || ps->potion_cd > 0 || r < 0 || r >= RC_ALL || !s->dishes[r][q][place]) return false;
    s->dishes[r][q][place]--;
    s->well_fed_t = 0; /* a meal replaces the feast, and the feast a meal */
    s->meal = (u8)(r + 1);
    s->meal_q = (u8)q;
    s->meal_t = s->meal_len = food_meal_seconds(r, place);
    stats_refresh(ps);
    s->hp = fminf(s->maxhp, s->hp + s->maxhp * 0.2f);
    s->mana = fminf(s->maxmana, s->mana + s->maxmana * 0.2f);
    ps->potion_cd = 0.8f;
    g_tele.eaten++;
    char buf[160], name[64], what[96];
    snprintf(name, sizeof name, "%s%s", food_quality_name(q), RECIPES[r].name);
    snprintf(what, sizeof what, "%s", RECIPES[r].text);
    upper(name);
    upper(what);
    snprintf(buf, sizeof buf, "%s: %s, %d MIN", name, what, (int)(s->meal_t / 60.0f + 0.5f));
    game_banner(ps, buf);
    sfx_player(g, ps, SFX_EAT, 1.0f);
    particles_burst(g, player_eye(&ps->pl), v3(1.0f, 0.8f, 0.45f), 10, 1.5f, 0.04f, true);
    return true;
}

bool food_eat_best(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    int br = -1, bq = 0, bp = 0;
    float best = -1;
    for (int r = 0; r < RC_ALL; r++)
        for (int q = 0; q < DISH_QUALITIES; q++)
            for (int p = 1; p < CP_COUNT; p++) {
                if (!s->dishes[r][q][p]) continue;
                float v = (r == RC_SIMPLE ? 0.0f : 10.0f) + q * 3.0f + p;
                if (v > best) { best = v; br = r; bq = q; bp = p; }
            }
    if (br < 0) {
        if (ps->banner_time <= 0) game_banner(ps, food_held(s) ? "NOTHING COOKED: COOK AT A FIRE (FOOD TAB)" : "NOTHING TO EAT");
        return false;
    }
    return food_eat_dish(g, ps, br, bq, bp);
}

bool food_eat_raw(Game *g, PlayerSlot *ps, int food) {
    Stats *s = &ps->st;
    if (s->dead || ps->potion_cd > 0 || food < 0 || food >= FD_COUNT || free_count(ps, food) <= 0) return false;
    s->food[food]--;
    u32 t = FOODS[food].tags;
    s->hp = fminf(s->maxhp, s->hp + s->maxhp * ((t & TGB(TG_SWEET)) ? 0.08f : 0.03f));
    if (t & TGB(TG_MANA)) s->mana = fminf(s->maxmana, s->mana + s->maxmana * 0.1f);
    if (t & TGB(TG_BITTER)) s->poison_t = 0;
    int e;
    float v;
    char buf[128], name[48];
    snprintf(name, sizeof name, "%s", FOODS[food].name);
    upper(name);
    if (raw_effect(food, &e, &v)) {
        s->snack = (u8)(food + 1);
        s->snack_t = 90.0f;
        snprintf(buf, sizeof buf, "%s, RAW: A TASTE OF IT FOR A MINUTE", name);
    } else snprintf(buf, sizeof buf, "%s, RAW: BETTER COOKED", name);
    stats_refresh(ps);
    ps->potion_cd = 0.6f;
    game_banner(ps, buf);
    sfx_player(g, ps, SFX_EAT_RAW, 0.9f);
    return true;
}

void food_feast(PlayerSlot *ps) {
    ps->st.meal = 0;
    ps->st.meal_t = 0;
}

void food_update(Game *g, float dt) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        Stats *s = &ps->st;
        bool had = s->meal_t > 0, snack = s->snack_t > 0;
        if (s->meal_t > 0) s->meal_t = fmaxf(0.0f, s->meal_t - dt);
        if (s->snack_t > 0) s->snack_t = fmaxf(0.0f, s->snack_t - dt);
        if (had && s->meal_t <= 0) {
            s->meal = 0;
            stats_refresh(ps);
            game_banner(ps, "THE MEAL WEARS OFF");
        }
        if (snack && s->snack_t <= 0) { s->snack = 0; stats_refresh(ps); }
        if (food_fx(s, ME_LIGHT) > 0) s->reveal_t = fmaxf(s->reveal_t, 0.5f);
        if (food_fx(s, ME_FIRE_RES) > 0) s->burn_t = 0;
        if (food_fx(s, ME_FROST_RES) > 0) s->chill_t = 0;
        if (food_fx(s, ME_POISON_IMM) > 0) s->poison_t = 0;
        /* the pot is emptied when you walk away from the fire */
        if (ps->npot && food_cook_place(g, ps) == CP_NONE) ps->npot = 0;
    }
}

/* ---------- the FOOD tab ---------- */

enum { FR_INFO, FR_COOK, FR_CLEAR, FR_DISH, FR_ING, FR_HEAD, FR_BOOK };
typedef struct FoodRow { u8 type; int a, b, c; } FoodRow;
#define FOOD_ROWS (8 + RC_ALL * 3 + FD_COUNT + RECIPE_COUNT)

/* Can the larder make this recipe (ingredients not already in the pot)? */
static bool can_make_rec(const PlayerSlot *ps, const Recipe *r, int k, int *used) {
    if (k >= r->slots || k >= 4) return true;
    for (int f = 0; f < FD_COUNT; f++) {
        u8 t = r->need[k];
        if (free_count(ps, f) - used[f] <= 0) continue;
        if (t != TG_ANY && !(FOODS[f].tags & TGB(t))) continue;
        used[f]++;
        bool ok = can_make_rec(ps, r, k + 1, used);
        used[f]--;
        if (ok) return true;
    }
    return false;
}
static bool can_make(const PlayerSlot *ps, int r) {
    int used[FD_COUNT] = { 0 };
    return can_make_rec(ps, &RECIPES[r], 0, used);
}

static int food_rows(Game *g, PlayerSlot *ps, FoodRow *rows, int *place_out) {
    const Stats *s = &ps->st;
    int n = 0, place = food_cook_place(g, ps);
    if (place_out) *place_out = place;
    if (place != CP_NONE) {
        rows[n++] = (FoodRow){ FR_COOK, 0, 0, 0 };
        if (ps->npot) rows[n++] = (FoodRow){ FR_CLEAR, 0, 0, 0 };
    }
    for (int r = 0; r < RC_ALL; r++)
        for (int q = DISH_QUALITIES - 1; q >= 0; q--)
            for (int p = CP_COUNT - 1; p >= 1; p--)
                if (s->dishes[r][q][p]) rows[n++] = (FoodRow){ FR_DISH, r, q, p };
    for (int f = 0; f < FD_COUNT; f++)
        if (s->food[f]) rows[n++] = (FoodRow){ FR_ING, f, 0, 0 };
    if (n == 0) rows[n++] = (FoodRow){ FR_INFO, 0, 0, 0 };
    rows[n++] = (FoodRow){ FR_HEAD, 0, 0, 0 };
    for (int r = 0; r < RECIPE_COUNT; r++) rows[n++] = (FoodRow){ FR_BOOK, r, 0, 0 };
    return n;
}

static void act(Game *g, PlayerSlot *ps, const FoodRow *row, bool raw) {
    switch (row->type) {
    case FR_COOK: food_cook(g, ps); break;
    case FR_CLEAR: ps->npot = 0; break;
    case FR_DISH: food_eat_dish(g, ps, row->a, row->b, row->c); break;
    case FR_ING:
        if (raw || !food_pot_add(g, ps, row->a)) food_eat_raw(g, ps, row->a);
        break;
    default: break;
    }
}

void food_nav(Game *g, PlayerSlot *ps, int nav) {
    FoodRow rows[FOOD_ROWS];
    int n = food_rows(g, ps, rows, NULL);
    if (nav == NAV_UP) ps->food_sel = (ps->food_sel + n - 1) % n;
    else if (nav == NAV_DOWN) ps->food_sel = (ps->food_sel + 1) % n;
    else if ((nav == NAV_BUY || nav == NAV_DROP) && ps->food_sel < n) act(g, ps, &rows[ps->food_sel], nav == NAV_DROP);
    if (ps->food_sel >= n) ps->food_sel = n - 1;
}

#define ROW 11
static int rows_fit(int H, int top) { return MAXI(3, (H - top - 34 - 18) / ROW); }
static int first_row(const PlayerSlot *ps, int n, int fit) { return MAXI(0, MINI(ps->food_sel - fit / 2, n - fit)); }
#define LIST_Y(top) ((top) + 34)

void food_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y) {
    (void)W; (void)x;
    FoodRow rows[FOOD_ROWS];
    int n = food_rows(g, ps, rows, NULL);
    int fit = rows_fit(H, top), f = first_row(ps, n, fit);
    int row = (int)floorf((y - LIST_Y(top)) / ROW);
    if (row < 0 || row >= fit || f + row >= n) return;
    row += f;
    if (row == ps->food_sel) act(g, ps, &rows[row], false);
    else ps->food_sel = row;
}

static void mmss(float t, char *out, int n) { snprintf(out, (size_t)n, "%d:%02d", (int)t / 60, (int)t % 60); }

void food_draw(Game *g, PlayerSlot *ps, int W, int H, int top) {
    const Stats *s = &ps->st;
    FoodRow rows[FOOD_ROWS];
    int place;
    int n = food_rows(g, ps, rows, &place);
    if (ps->food_sel >= n) ps->food_sel = n - 1;
    int fit = rows_fit(H, top), f = first_row(ps, n, fit);
    int x = MAXI(6, (W - 74 * 6) / 2);
    char buf[200], name[96], t[32];
    batch2d_begin(W, H);
    for (int i = f; i < n && i < f + fit; i++) {
        if (rows[i].type == FR_HEAD) continue;
        float ry = (float)(LIST_Y(top) + (i - f) * ROW) - 2.0f;
        bool sel = i == ps->food_sel;
        batch2d_rect((float)x - 3, ry, (float)(W - 2 * x + 6), ROW - 1, sel ? 0.25f : 0.08f, sel ? 0.18f : 0.06f, sel ? 0.1f : 0.05f, 0.85f);
    }
    batch2d_flush();
    font_begin(W, H);
    /* the meal, and where you are */
    if (s->meal && s->meal_t > 0) {
        mmss(s->meal_t, t, sizeof t);
        snprintf(buf, sizeof buf, "MEAL: %s%s (%s)", food_quality_name(s->meal_q), RECIPES[s->meal - 1].name, t);
    } else if (s->well_fed_t > 0) {
        mmss(s->well_fed_t, t, sizeof t);
        snprintf(buf, sizeof buf, "MEAL: THE FEAST, WELL FED (%s)", t);
    } else snprintf(buf, sizeof buf, "NO MEAL");
    upper(buf);
    font_text(x, top + 6, buf, 1.0f, 0.82f, 0.35f);
    if (place != CP_NONE) snprintf(buf, sizeof buf, "AT %s: COOK WITH UP TO %d. X EATS RAW", PLACE_NAMES[place], place + 1);
    else snprintf(buf, sizeof buf, "COOK AT A FIRE, A HEARTH, OR AT HOME. ENTER EATS");
    upper(buf);
    font_text(x, top + 18, buf, 0.65f, 0.6f, 0.5f);
    if (f > 0) font_text(W - x - 6, LIST_Y(top) - 10, "^", 0.8f, 0.7f, 0.5f);
    if (f + fit < n) font_text(W - x - 6, LIST_Y(top) + fit * ROW - 2, "V", 0.8f, 0.7f, 0.5f);
    int known = 0;
    for (int r = 0; r < RECIPE_COUNT; r++) known += (s->recipes >> r) & 1;
    int maxc = (W - 2 * x) / 6;
    for (int i = f; i < n && i < f + fit; i++) {
        const FoodRow *row = &rows[i];
        int ry = LIST_Y(top) + (i - f) * ROW;
        const char *right = "";
        float cr = 0.9f, cg = 0.85f, cb = 0.75f;
        char rbuf[40] = "";
        switch (row->type) {
        case FR_INFO: snprintf(buf, sizeof buf, "THE LARDER IS EMPTY: HUNT, GATHER, RAID THE LARDERS"); cr = cg = cb = 0.6f; break;
        case FR_COOK: {
            if (!ps->npot) snprintf(buf, sizeof buf, "THE POT IS EMPTY: CHOOSE INGREDIENTS BELOW");
            else {
                buf[0] = 0;
                for (int k = 0; k < ps->npot; k++)
                    snprintf(buf + strlen(buf), sizeof buf - strlen(buf), "%s%s", k ? " + " : "COOK: ", FOODS[ps->pot[k]].name);
                if (ps->npot >= 2) {
                    int r = food_match(ps->pot, ps->npot);
                    bool k = r == RC_SIMPLE || (s->recipes & (1u << r));
                    snprintf(buf + strlen(buf), sizeof buf - strlen(buf), " -> %s", k ? RECIPES[r].name : "a new dish?");
                }
            }
            cr = 1.0f; cg = 0.75f; cb = 0.4f;
            break;
        }
        case FR_CLEAR: snprintf(buf, sizeof buf, "CLEAR THE POT"); cr = cg = cb = 0.7f; break;
        case FR_DISH:
            snprintf(buf, sizeof buf, "%s%s: %s", food_quality_name(row->b), RECIPES[row->a].name, RECIPES[row->a].text);
            mmss(food_meal_seconds(row->a, row->c), t, sizeof t);
            snprintf(rbuf, sizeof rbuf, "X%d  %s", s->dishes[row->a][row->b][row->c], t);
            right = rbuf;
            cr = 1.0f; cg = 0.9f; cb = 0.6f;
            break;
        case FR_ING: {
            char tg[64];
            food_tags_text(FOODS[row->a].tags, tg, sizeof tg);
            snprintf(buf, sizeof buf, "%s (%s)", FOODS[row->a].name, tg);
            snprintf(rbuf, sizeof rbuf, "X%d", free_count(ps, row->a));
            right = rbuf;
            vec3 c = FOODS[row->a].color;
            cr = 0.55f + 0.45f * c.x; cg = 0.55f + 0.45f * c.y; cb = 0.55f + 0.45f * c.z;
            break;
        }
        case FR_HEAD:
            snprintf(buf, sizeof buf, "COOKBOOK: %d OF %d RECIPES", known, RECIPE_COUNT);
            cr = 1.0f; cg = 0.82f; cb = 0.35f;
            break;
        case FR_BOOK: {
            const Recipe *r = &RECIPES[row->a];
            bool k = (s->recipes >> row->a) & 1;
            food_need_text(r, name, sizeof name, !k);
            if (k) snprintf(buf, sizeof buf, "%s: %s - %s", r->name, name, r->text);
            else snprintf(buf, sizeof buf, "???: %s", name);
            snprintf(rbuf, sizeof rbuf, "%s", r->slots == 2 ? "FIRE" : r->slots == 3 ? "HEARTH" : "KITCHEN");
            if (k && can_make(ps, row->a)) snprintf(rbuf, sizeof rbuf, "READY");
            right = rbuf;
            float m = k ? 0.85f : 0.5f;
            cr = m; cg = m * 0.95f; cb = m * 0.85f;
            break;
        }
        }
        upper(buf);
        int room = maxc - (int)strlen(right) - 2;
        if ((int)strlen(buf) > room) buf[MAXI(0, room)] = 0;
        font_text(x, ry, buf, cr, cg, cb);
        if (right[0]) font_text(W - x - font_text_width(right), ry, right, 0.95f, 0.8f, 0.35f);
    }
    font_text(x, H - 14, "UP/DOWN CHOOSE   ENTER EAT OR INTO THE POT   X EAT RAW   K EAT", 0.6f, 0.55f, 0.45f);
    font_end();
}

/* ---------- how an ingredient looks ---------- */

void food_draw_model(int food, vec3 c, float yaw, float s, vec3 l) {
    if (food < 0 || food >= FD_COUNT) return;
    const FoodDef *d = &FOODS[food];
    bool glow = (d->tags & TGB(TG_GLOW)) && d->tags != 0xFFFFFFFFu;
    vec3 col = glow ? d->color : v3(d->color.x * l.x, d->color.y * l.y, d->color.z * l.z);
    vec3 dark = v3_scale(col, 0.7f);
    vec3 f = v3(cosf(yaw), 0, sinf(yaw)), r = v3(-sinf(yaw), 0, cosf(yaw));
#define P(fw, rt, up) v3_add(c, v3_add(v3_scale(f, (fw) * s), v3_add(v3_scale(r, (rt) * s), v3(0, (up) * s, 0))))
    switch (d->model) {
    case FM_MEAT:
        batch3d_box(P(0, 0, 0.08f), v3(0.14f * s, 0.08f * s, 0.1f * s), yaw, col, false);
        batch3d_box(P(0.17f, 0, 0.08f), v3(0.07f * s, 0.025f * s, 0.025f * s), yaw, v3(0.9f * l.x, 0.88f * l.y, 0.8f * l.z), false);
        break;
    case FM_EGG:
        batch3d_octa(P(0, 0, 0.08f), 0.06f * s, 0.09f * s, yaw, col, false);
        batch3d_octa(P(0.08f, 0.06f, 0.06f), 0.05f * s, 0.07f * s, yaw + 0.5f, col, false);
        break;
    case FM_BOTTLE:
        batch3d_box(P(0, 0, 0.1f), v3(0.07f * s, 0.1f * s, 0.07f * s), yaw, col, glow);
        batch3d_box(P(0, 0, 0.23f), v3(0.03f * s, 0.04f * s, 0.03f * s), yaw, v3(0.5f * l.x, 0.4f * l.y, 0.3f * l.z), false);
        break;
    case FM_FISH:
        batch3d_box(P(0, 0, 0.06f), v3(0.16f * s, 0.06f * s, 0.035f * s), yaw, col, false);
        batch3d_box(P(-0.19f, 0, 0.06f), v3(0.04f * s, 0.07f * s, 0.01f * s), yaw, dark, false);
        break;
    case FM_SHELL:
        batch3d_box(P(0, 0, 0.05f), v3(0.11f * s, 0.05f * s, 0.11f * s), yaw + 0.78f, col, false);
        batch3d_box(P(0.1f, 0.08f, 0.04f), v3(0.04f * s, 0.02f * s, 0.02f * s), yaw, dark, false);
        batch3d_box(P(0.1f, -0.08f, 0.04f), v3(0.04f * s, 0.02f * s, 0.02f * s), yaw, dark, false);
        break;
    case FM_MUSHROOM:
        batch3d_box(P(0, 0, 0.06f), v3(0.03f * s, 0.06f * s, 0.03f * s), yaw, v3(0.85f * l.x, 0.8f * l.y, 0.7f * l.z), false);
        batch3d_box(P(0, 0, 0.14f), v3(0.1f * s, 0.035f * s, 0.1f * s), yaw + 0.78f, col, glow);
        batch3d_box(P(0.1f, 0.05f, 0.04f), v3(0.02f * s, 0.04f * s, 0.02f * s), yaw, v3(0.85f * l.x, 0.8f * l.y, 0.7f * l.z), false);
        batch3d_box(P(0.1f, 0.05f, 0.09f), v3(0.05f * s, 0.02f * s, 0.05f * s), yaw, col, glow);
        break;
    case FM_SPRIG:
        for (int k = 0; k < 4; k++) {
            float a = k * 1.6f;
            batch3d_box(P(cosf(a) * 0.05f, sinf(a) * 0.05f, 0.1f + 0.02f * (k & 1)), v3(0.02f * s, (0.1f + 0.02f * k) * s, 0.04f * s), yaw + a,
                        k & 1 ? dark : col, false);
        }
        break;
    case FM_ROOT:
        batch3d_box(P(0, 0, 0.07f), v3(0.07f * s, 0.07f * s, 0.07f * s), yaw + 0.4f, col, false);
        batch3d_box(P(0, 0, 0.17f), v3(0.02f * s, 0.05f * s, 0.02f * s), yaw, v3(0.35f * l.x, 0.6f * l.y, 0.25f * l.z), false);
        break;
    case FM_BERRY:
        for (int k = 0; k < 3; k++) batch3d_octa(P(cosf(k * 2.1f) * 0.06f, sinf(k * 2.1f) * 0.06f, 0.05f + 0.03f * k), 0.045f * s, 0.05f * s, yaw, col, false);
        break;
    case FM_MELON:
        batch3d_octa(P(0, 0, 0.12f), 0.13f * s, 0.12f * s, yaw, col, food == FD_STARFRUIT);
        break;
    case FM_SACK:
        batch3d_box(P(0, 0, 0.11f), v3(0.1f * s, 0.11f * s, 0.09f * s), yaw, v3(0.72f * l.x, 0.6f * l.y, 0.42f * l.z), false);
        batch3d_box(P(0, 0, 0.23f), v3(0.06f * s, 0.02f * s, 0.05f * s), yaw, col, false);
        break;
    case FM_COMB:
        batch3d_box(P(0, 0, 0.05f), v3(0.12f * s, 0.05f * s, 0.08f * s), yaw, col, false);
        batch3d_box(P(0, 0, 0.1f), v3(0.1f * s, 0.01f * s, 0.06f * s), yaw, v3(1.0f, 0.75f, 0.25f), true);
        break;
    }
#undef P
}
