#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

#include <stdio.h>
#include <string.h>

/* Alchemy (docs/ALCHEMY.md). Reagents come from monsters (one kind each) and from strange
 * places; some larder ingredients serve too. At a shrine or at home, steps go into the
 * cauldron one by one (ingredients, and grinding, heating, stirring, chilling, distilling);
 * bottled, they make an elixir only if they follow a formula exactly. A drunk elixir takes
 * hold after a few seconds and is strong for 20 to 30. Formulae are found as pages or found
 * out by trying: a murky brew tells how many steps were right. */

const ReagentDef REAGENTS[RG_COUNT] = {
    [RG_ICHOR] = { "crawler ichor", { 0.55f, 0.75f, 0.2f }, RM_VIAL, false },
    [RG_GALL] = { "brute's gall", { 0.45f, 0.5f, 0.15f }, RM_STONE, false },
    [RG_WISP] = { "wisp ember", { 0.5f, 0.85f, 1.0f }, RM_FLAME, true },
    [RG_HORN] = { "horn shavings", { 0.8f, 0.7f, 0.55f }, RM_DUST, false },
    [RG_BONEDUST] = { "bone dust", { 0.9f, 0.88f, 0.8f }, RM_DUST, false },
    [RG_SLIME] = { "slime core", { 0.35f, 0.9f, 0.45f }, RM_LENS, true },
    [RG_BATWING] = { "bat membrane", { 0.35f, 0.25f, 0.3f }, RM_FEATHER, false },
    [RG_VENOM] = { "spider venom", { 0.6f, 0.9f, 0.1f }, RM_VIAL, true },
    [RG_WRAITH] = { "shroud ash", { 0.45f, 0.45f, 0.6f }, RM_DUST, false },
    [RG_HEARTSTONE] = { "golem heartstone", { 0.9f, 0.5f, 0.2f }, RM_STONE, true },
    [RG_FEATHER] = { "spirit feather", { 0.7f, 0.95f, 0.85f }, RM_FEATHER, true },
    [RG_BILE] = { "bloater bile", { 0.7f, 0.65f, 0.2f }, RM_VIAL, false },
    [RG_LENS] = { "gazing lens", { 0.95f, 0.4f, 0.4f }, RM_LENS, true },
    [RG_RUST] = { "knight's rust", { 0.65f, 0.35f, 0.2f }, RM_DUST, false },
    [RG_TOOTH] = { "worm tooth", { 0.85f, 0.8f, 0.65f }, RM_FANG, false },
    [RG_TONGUE] = { "mimic tongue", { 0.85f, 0.35f, 0.45f }, RM_ROOT, false },
    [RG_WAX] = { "grave wax", { 0.8f, 0.8f, 0.6f }, RM_STONE, false },
    [RG_PLUME] = { "harpy plume", { 0.7f, 0.55f, 0.85f }, RM_FEATHER, false },
    [RG_FANG] = { "hound fang", { 0.95f, 0.92f, 0.85f }, RM_FANG, false },
    [RG_TROLLBLOOD] = { "troll blood", { 0.5f, 0.75f, 0.35f }, RM_VIAL, false },
    [RG_QUICKSILVER] = { "quicksilver", { 0.85f, 0.88f, 0.95f }, RM_VIAL, true },
    [RG_BRIMSTONE] = { "brimstone", { 1.0f, 0.85f, 0.2f }, RM_STONE, false },
    [RG_ORCHID] = { "ghost orchid", { 0.85f, 0.9f, 1.0f }, RM_ROOT, true },
    [RG_DEW] = { "starlight dew", { 0.75f, 0.85f, 1.0f }, RM_VIAL, true },
    [RG_VOIDSALT] = { "void salt", { 0.35f, 0.2f, 0.55f }, RM_DUST, true },
    [RG_MANDRAKE] = { "mandrake root", { 0.6f, 0.45f, 0.3f }, RM_ROOT, false },
    [RG_AMBER] = { "amber", { 1.0f, 0.65f, 0.15f }, RM_STONE, true },
    [RG_PHOENIX] = { "phoenix feather", { 1.0f, 0.45f, 0.1f }, RM_FEATHER, true },
};

#define R(x) STEP_RG(RG_##x)
#define F(x) STEP_FOOD(FD_##x)
#define GRIND STEP_ACT(AS_GRIND)
#define HEAT STEP_ACT(AS_HEAT)
#define STIR STEP_ACT(AS_STIR)
#define CHILL STEP_ACT(AS_CHILL)
#define DISTILL STEP_ACT(AS_DISTILL)
#define E STEP_NONE

const Elixir ELIXIRS[ELIXIR_COUNT] = {
    { "draught of vigor", { R(ICHOR), F(HERB), HEAT, STIR, E, E, E }, 2, 20, { { ME_FIGHT_REGEN, 0.025f } }, EX_NONE,
      { 0.5f, 1.0f, 0.4f }, "2.5% of your health back every second (counts as a health potion)" },
    { "swift tonic", { R(BATWING), GRIND, F(SALT), STIR, E, E, E }, 2, 25, { { ME_SPEED, 0.6f }, { ME_ATTACK_SPEED, 0.25f } }, EX_NONE,
      { 0.6f, 0.9f, 1.0f }, "60% faster, attacks 25% faster" },
    { "berserker's draught", { R(GALL), F(PEPPER), HEAT, HEAT, STIR, E, E }, 3, 25, { { ME_DAMAGE, 0.8f }, { ME_TAKEN, 0.25f } }, EX_NONE,
      { 1.0f, 0.25f, 0.15f }, "80% more weapon damage, 25% more taken" },
    { "stoneskin philtre", { R(HEARTSTONE), GRIND, R(BONEDUST), CHILL, STIR, E, E }, 4, 25, { { ME_TAKEN, -0.6f }, { ME_STUN_RES, 1.0f } },
      EX_NONE, { 0.75f, 0.7f, 0.6f }, "60% less damage taken, blows do not push you" },
    { "troll's blood", { R(TROLLBLOOD), R(ICHOR), HEAT, DISTILL, E, E, E }, 3, 20, { { ME_FIGHT_REGEN, 0.08f } }, EX_NONE,
      { 0.4f, 0.8f, 0.3f }, "8% of your health back every second" },
    { "ghost draught", { R(WRAITH), R(ORCHID), CHILL, DISTILL, E, E, E }, 3, 25, { { 0, 0 } }, EX_INVISIBLE,
      { 0.7f, 0.75f, 0.95f }, "unseen until you strike" },
    { "elixir of wings", { R(PLUME), R(PLUME), R(DEW), STIR, DISTILL, E, E }, 2, 25, { { 0, 0 } }, EX_WINGS,
      { 0.85f, 0.9f, 1.0f }, "wings: fly where you look" },
    { "phoenix elixir", { R(PHOENIX), R(BRIMSTONE), HEAT, HEAT, DISTILL, F(ROYAL_JELLY), E }, 5, 30,
      { { ME_REVIVE, 1.0f }, { ME_FIRE_RES, 1.0f }, { ME_LAVA, 1.0f } }, EX_NONE, { 1.0f, 0.5f, 0.1f },
      "a killing blow lifts you again; fire and lava cannot hurt you" },
    { "arcane surge", { R(WISP), F(GLOWCAP), R(DEW), DISTILL, E, E, E }, 3, 20,
      { { ME_SPELL_COST, 1.0f }, { ME_COOLDOWN, 0.5f }, { ME_SPELL_POWER, 0.3f } }, EX_NONE, { 0.55f, 0.6f, 1.0f },
      "spells cost nothing, cool half as long, 30% stronger" },
    { "storm in a bottle", { R(LENS), R(QUICKSILVER), GRIND, HEAT, STIR, E, E }, 3, 25, { { ME_CHAIN, 0.5f }, { ME_SHOCK_RES, 1.0f } },
      EX_NONE, { 0.6f, 0.75f, 1.0f }, "blows arc to the next foe" },
    { "dragonfire draught", { R(BRIMSTONE), F(PEPPER), F(PEPPER), HEAT, E, E, E }, 3, 25, { { ME_IGNITE, 1.0f }, { ME_FIRE_RES, 1.0f } },
      EX_NONE, { 1.0f, 0.4f, 0.1f }, "every blow sets the foe alight; fire cannot hurt you" },
    { "frost heart", { F(FROSTBERRY), R(VOIDSALT), CHILL, CHILL, STIR, E, E }, 3, 25, { { ME_FROST_AURA, 1.0f }, { ME_FROST_RES, 1.0f } },
      EX_NONE, { 0.55f, 0.8f, 1.0f }, "foes close by freeze slow; frost cannot hurt you" },
    { "vampire's kiss", { R(FANG), F(WINE), R(WAX), STIR, E, E, E }, 3, 25, { { ME_LIFESTEAL, 0.25f } }, EX_NONE,
      { 0.7f, 0.05f, 0.15f }, "a quarter of your weapon damage heals you" },
    { "eagle eye", { R(LENS), R(PLUME), CHILL, DISTILL, E, E, E }, 2, 25, { { ME_CRIT, 0.5f } }, EX_NONE,
      { 1.0f, 0.9f, 0.5f }, "half your hits are critical" },
    { "midas brew", { R(TONGUE), F(SALT), F(HONEY), HEAT, STIR, E, E }, 3, 30, { { ME_LUCK, 0.5f } }, EX_NONE,
      { 1.0f, 0.8f, 0.25f }, "what you kill drops far more" },
    { "draught of dread", { R(WAX), R(ORCHID), R(BONEDUST), GRIND, CHILL, E, E }, 4, 20, { { ME_FEAR_AURA, 1.0f } }, EX_NONE,
      { 0.35f, 0.3f, 0.45f }, "foes close by run from you" },
    { "titan's brew", { R(HORN), R(GALL), GRIND, HEAT, STIR, E, E }, 4, 25, { { ME_DAMAGE, 0.4f }, { ME_KNOCK, 2.0f }, { ME_MAXHP, 0.3f } },
      EX_NONE, { 0.8f, 0.55f, 0.3f }, "40% more damage, blows throw three times as far, 30% more health" },
    { "time syrup", { R(QUICKSILVER), R(AMBER), R(DEW), STIR, DISTILL, E, E }, 3, 20, { { 0, 0 } }, EX_TIMESLIP,
      { 1.0f, 0.7f, 0.3f }, "the world slows round you" },
    { "golem's ward", { R(HEARTSTONE), R(RUST), GRIND, HEAT, E, E, E }, 2, 25, { { 0, 0 } }, EX_WARD,
      { 0.9f, 0.6f, 0.3f }, "a ward of 60% of your health" },
    { "spirit mirror", { R(VOIDSALT), R(LENS), DISTILL, CHILL, E, E, E }, 3, 20, { { ME_PROJ_RES, 1.0f } }, EX_NONE,
      { 0.7f, 0.5f, 1.0f }, "arrows, bolts and fire pass through you" },
    { "mana font", { F(GLOWCAP), F(BEETLE_JELLY), R(SLIME), DISTILL, E, E, E }, 2, 25, { { ME_MANA_REGEN, 9.0f } }, EX_NONE,
      { 0.35f, 0.5f, 1.0f }, "mana flows ten times as fast" },
    { "tunneler's sight", { R(TOOTH), R(MANDRAKE), GRIND, STIR, E, E, E }, 2, 30, { { 0, 0 } }, EX_SIGHT,
      { 0.8f, 0.7f, 0.45f }, "the land round you is mapped; hidden things show" },
    { "purity elixir", { R(MANDRAKE), F(BARROW_ROOT), F(SALT), CHILL, E, E, E }, 2, 30,
      { { ME_FIRE_RES, 1.0f }, { ME_FROST_RES, 1.0f }, { ME_POISON_IMM, 1.0f }, { ME_SHOCK_RES, 1.0f } }, EX_NONE, { 0.95f, 0.95f, 1.0f },
      "fire, frost, poison and lightning cannot touch you" },
    { "volatile draught", { R(BILE), R(BRIMSTONE), STIR, HEAT, E, E, E }, 3, 20, { { ME_BLAST, 0.4f } }, EX_NONE,
      { 0.95f, 0.6f, 0.2f }, "blows burst round the foe" },
    { "venom draught", { R(VENOM), R(VENOM), GRIND, STIR, E, E, E }, 1, 30, { { ME_POISON_HITS, 0.3f } }, EX_NONE,
      { 0.55f, 0.95f, 0.15f }, "hits poison hard" },
    { "star elixir", { F(STARFRUIT), R(DEW), F(ROYAL_JELLY), DISTILL, DISTILL, E, E }, 5, 30,
      { { ME_DAMAGE, 0.3f }, { ME_SPEED, 0.3f }, { ME_MAXHP, 0.3f }, { ME_MANA_REGEN, 2.0f } }, EX_NONE, { 1.0f, 1.0f, 0.7f },
      "30% more of everything, mana three times as fast" },
    { "slime draught", { R(SLIME), R(SLIME), STIR, CHILL, E, E, E }, 2, 25, { { ME_JUMP, 1.2f }, { ME_FALL, 1.0f } }, EX_NONE,
      { 0.4f, 1.0f, 0.5f }, "jump twice as high, falls do not hurt" },
};
#undef R
#undef F
#undef E

static const char *ACT_NAMES[AS_COUNT] = { "grind", "heat", "stir", "chill", "distill" };

static void upper(char *s) {
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}

/* ---------- formulae ---------- */

int alchemy_steps(const u8 *steps) {
    int n = 0;
    while (n < FORMULA_STEPS && steps[n] != STEP_NONE) n++;
    return n;
}

void alchemy_step_name(u8 step, char *out, int n) {
    if (step_is_food(step) && step < FD_COUNT) snprintf(out, (size_t)n, "%s", FOODS[step].name);
    else if (step_is_rg(step) && step - 64 < RG_COUNT) snprintf(out, (size_t)n, "%s", REAGENTS[step - 64].name);
    else if (step_is_act(step)) snprintf(out, (size_t)n, "%s", ACT_NAMES[step - 128]);
    else snprintf(out, (size_t)n, "?");
}

int alchemy_match(const u8 *steps, int n) {
    for (int r = 0; r < ELIXIR_COUNT; r++) {
        if (alchemy_steps(ELIXIRS[r].steps) != n) continue;
        if (!memcmp(ELIXIRS[r].steps, steps, (size_t)n)) return r;
    }
    return -1;
}

/* Steps that are in formula r at all, wherever (each counted once): the mastermind's white pegs plus its black ones. */
int alchemy_misplaced(const u8 *steps, int n, int r) {
    bool used[FORMULA_STEPS] = { 0 };
    int l = alchemy_steps(ELIXIRS[r].steps), k = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < l; j++)
            if (!used[j] && ELIXIRS[r].steps[j] == steps[i]) { used[j] = true; k++; break; }
    return k;
}

int alchemy_closest(const u8 *steps, int n, int *right, int *len) {
    int best = -1, br = 0, bl = 0;
    for (int r = 0; r < ELIXIR_COUNT; r++) {
        int l = alchemy_steps(ELIXIRS[r].steps), k = 0;
        for (int i = 0; i < l && i < n; i++) k += ELIXIRS[r].steps[i] == steps[i];
        if (k > br || (k == br && k > 0 && abs(l - n) < abs(bl - n))) { best = r; br = k; bl = l; }
    }
    *right = br;
    *len = bl;
    return br >= 2 ? best : -1;
}

/* ---------- reagents ---------- */

#define REAGENT_MAX 20
#define TOXIC_AFTER 90.0f /* seconds after an elixir before the next works fully */
#define ELIXIR_MAX 5

void alchemy_give(Game *g, PlayerSlot *ps, int rg, int n) {
    if (rg < 0 || rg >= RG_COUNT || n <= 0) return;
    Stats *s = &ps->st;
    int got = MINI(n, REAGENT_MAX - s->reag[rg]);
    char buf[96], name[48];
    snprintf(name, sizeof name, "%s", REAGENTS[rg].name);
    upper(name);
    if (got <= 0) {
        snprintf(buf, sizeof buf, "NO ROOM FOR MORE %s", name);
        if (ps->banner_time <= 0) game_banner(ps, buf);
        return;
    }
    s->reag[rg] = (u8)(s->reag[rg] + got);
    g_tele.reagents += got;
    snprintf(buf, sizeof buf, "%s (%d)", name, s->reag[rg]);
    game_banner(ps, buf);
    sfx_player(g, ps, SFX_REAGENT_PICKUP, 0.8f);
}

bool alchemy_learn(Game *g, PlayerSlot *ps, int r, bool found) {
    if (r < 0 || r >= ELIXIR_COUNT) return false;
    Stats *s = &ps->st;
    char buf[128], name[64];
    snprintf(name, sizeof name, "%s", ELIXIRS[r].name);
    upper(name);
    if (s->formulae & (1u << r)) {
        if (found) { /* a page already known: a few coins for the paper */
            s->gold += 25;
            snprintf(buf, sizeof buf, "A FORMULA YOU KNOW: %s (+25 GOLD)", name);
            game_banner(ps, buf);
        }
        return false;
    }
    s->formulae |= 1u << r;
    s->formulae_torn &= ~(1u << r);
    snprintf(buf, sizeof buf, "%s: %s", found ? "A FORMULA" : "DISCOVERED", name);
    game_banner(ps, buf);
    sfx_ui(SFX_FORMULA_FOUND, 1.0f);
    (void)g;
    return true;
}

/* A torn page: the ingredients of a formula, not its steps. Two of the same make it whole. */
bool alchemy_learn_torn(Game *g, PlayerSlot *ps, int r) {
    if (r < 0 || r >= ELIXIR_COUNT) return false;
    Stats *s = &ps->st;
    if ((s->formulae | s->formulae_torn) & (1u << r)) {
        if (s->formulae_torn & (1u << r) && !(s->formulae & (1u << r))) { /* the other half */
            s->formulae_torn &= ~(1u << r);
            return alchemy_learn(g, ps, r, true);
        }
        return alchemy_learn(g, ps, r, true); /* known: a few coins */
    }
    s->formulae_torn |= 1u << r;
    char buf[160], name[64];
    snprintf(name, sizeof name, "%s", ELIXIRS[r].name);
    upper(name);
    snprintf(buf, sizeof buf, "A TORN PAGE: %s - WHAT GOES IN, NOT HOW", name);
    game_banner(ps, buf);
    sfx_ui(SFX_FORMULA_FOUND, 0.8f);
    return true;
}

int healing_held(const Stats *s) { return s->potions_hp + s->elixirs[0]; }

int alchemy_belt(const Stats *s) {
    int n = 0;
    for (int r = 1; r < ELIXIR_COUNT; r++) n += s->elixirs[r];
    return n;
}

/* A formula page for whoever finds it: one this player does not know, if there is one. */
int alchemy_random_formula(const Stats *s, u64 seed) {
    int unknown[ELIXIR_COUNT], n = 0;
    for (int r = 0; r < ELIXIR_COUNT; r++)
        if (!(s->formulae & (1u << r))) unknown[n++] = r;
    /* a torn page half known is the likelier find (so halves meet) */
    for (int r = 0; r < ELIXIR_COUNT; r++)
        if ((s->formulae_torn & (1u << r)) && !(s->formulae & (1u << r)) && n < ELIXIR_COUNT) unknown[n++] = r;
    if (n) return unknown[mix64(seed) % (u64)n];
    return (int)(mix64(seed) % ELIXIR_COUNT);
}

/* ---------- where to brew ---------- */

int alchemy_station(Game *g, const PlayerSlot *ps) {
    if (realm_has(ps->pl.pos)) return sanctum_stage(g, SP_LIBRARY) >= 2 ? 2 : 1; /* the library's alembic doubles */
    int px = (int)floorf(ps->pl.pos.x), py = (int)floorf(ps->pl.pos.y), pz = (int)floorf(ps->pl.pos.z);
    for (int dy = -1; dy <= 2; dy++)
        for (int dz = -3; dz <= 3; dz++)
            for (int dx = -3; dx <= 3; dx++) {
                bool known;
                if (chunkmap_block(&g->map, px + dx, py + dy, pz + dz, &known) == B_SHRINE && known) return 1;
            }
    return 0;
}

/* what is left of a step's ingredient after what is already in the cauldron */
static int free_of(const PlayerSlot *ps, u8 step) {
    if (step_is_act(step)) return 99;
    int n = step_is_food(step) ? ps->st.food[step] : ps->st.reag[step - 64];
    for (int i = 0; i < ps->ncauldron; i++)
        if (ps->cauldron[i] == step) n--;
    return n;
}

bool alchemy_add(Game *g, PlayerSlot *ps, u8 step) {
    if (!alchemy_station(g, ps)) { game_banner(ps, "BREW AT A SHRINE, OR AT HOME"); return false; }
    if (ps->ncauldron >= FORMULA_STEPS) { game_banner(ps, "THE CAULDRON IS FULL: BOTTLE IT, OR EMPTY IT"); return false; }
    if (free_of(ps, step) <= 0) return false;
    ps->cauldron[ps->ncauldron++] = step;
    sfx_player(g, ps, SFX_BREW_STEP, 0.6f);
    vec3 at = v3_add(ps->pl.pos, v3(cosf(ps->pl.yaw) * 0.8f, 1.0f, sinf(ps->pl.yaw) * 0.8f));
    particles_burst(g, at, step_is_act(step) && step == STEP_ACT(AS_HEAT) ? v3(1.0f, 0.5f, 0.15f) : v3(0.5f, 0.9f, 0.6f), 6, 1.2f, 0.04f, true);
    return true;
}

bool alchemy_bottle(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    int st = alchemy_station(g, ps);
    if (!st || ps->ncauldron == 0) return false;
    {   /* no room for what it would make: say so before anything is used up */
        int r0 = alchemy_match(ps->cauldron, ps->ncauldron);
        if (r0 == 0 && healing_held(s) >= POTION_HP_MAX) { game_banner(ps, "NO ROOM: HEALTH POTIONS AND VIGOR TOGETHER, FIVE AT MOST"); return false; }
        if (r0 > 0 && alchemy_belt(s) >= ELIXIR_BELT) { game_banner(ps, "NO ROOM: THE BELT HOLDS FOUR ELIXIRS. DRINK ONE FIRST"); return false; }
    }
    for (int i = 0; i < ps->ncauldron; i++) {
        u8 k = ps->cauldron[i];
        if (step_is_food(k) && s->food[k]) s->food[k]--;
        else if (step_is_rg(k) && s->reag[k - 64]) s->reag[k - 64]--;
    }
    int n = ps->ncauldron;
    u8 steps[FORMULA_STEPS];
    memcpy(steps, ps->cauldron, (size_t)n);
    ps->ncauldron = 0;
    vec3 at = v3_add(ps->pl.pos, v3(cosf(ps->pl.yaw) * 0.8f, 1.1f, sinf(ps->pl.yaw) * 0.8f));
    int r = alchemy_match(steps, n);
    char buf[160], name[64];
    if (r >= 0) {
        bool fresh = alchemy_learn(g, ps, r, false);
        int add = st;
        if (r == 0) add = MINI(add, POTION_HP_MAX - healing_held(s)); /* Vigor shares the health potions' cap */
        else add = MINI(add, ELIXIR_BELT - alchemy_belt(s));          /* the rest: a belt of four */
        if (add <= 0) game_banner(ps, r == 0 ? "NO ROOM: HEALTH POTIONS AND VIGOR TOGETHER, FIVE AT MOST" : "NO ROOM: THE BELT HOLDS FOUR ELIXIRS");
        s->elixirs[r] = (u8)MINI(ELIXIR_MAX, s->elixirs[r] + MAXI(0, add));
        ps->elixir_quick = r;
        g_tele.brewed++;
        snprintf(name, sizeof name, "%s", ELIXIRS[r].name);
        upper(name);
        if (!fresh) {
            snprintf(buf, sizeof buf, "BOTTLED: %s%s (%d)", name, st > 1 ? " X2" : "", s->elixirs[r]);
            game_banner(ps, buf);
        }
        sfx_player(g, ps, SFX_BREW_SUCCESS, 1.0f);
        particles_burst(g, at, ELIXIRS[r].color, 20, 2.0f, 0.05f, true);
        flash_add(g, at, ELIXIRS[r].color, 6.0f, 0.4f);
        return true;
    }
    /* a murky brew: how close was it? */
    g_tele.murky++;
    int right, len, elsewhere;
    int c = alchemy_closest(steps, n, &right, &len);
    if (c < 0) { /* nothing in place: the nearest by what went in at all */
        int best = -1, bw = 0;
        for (int k = 0; k < ELIXIR_COUNT; k++) {
            int w = alchemy_misplaced(steps, n, k);
            if (w > bw) { bw = w; best = k; }
        }
        if (bw >= 2) { c = best; right = 0; len = alchemy_steps(ELIXIRS[c].steps); }
    }
    elsewhere = c >= 0 ? alchemy_misplaced(steps, n, c) - right : 0;
    const char *count = n > len ? ", AND TOO MANY STEPS" : n < len ? ", AND TOO FEW STEPS" : "";
    if (c >= 0 && right >= len - 1 && n == len) snprintf(buf, sizeof buf, "MURKY - BUT SO CLOSE: %d OF %d STEPS IN THEIR PLACE", right, len);
    else if (c >= 0)
        snprintf(buf, sizeof buf, "MURKY. OF %d: %d RIGHT IN THEIR PLACE, %d RIGHT BUT ELSEWHERE%s", len, right, elsewhere, count);
    else snprintf(buf, sizeof buf, "THE BREW TURNS MURKY AND SINKS");
    game_banner(ps, buf);
    sfx_player(g, ps, SFX_BREW_FAIL, 1.0f);
    particles_burst(g, at, v3(0.35f, 0.3f, 0.25f), 16, 1.5f, 0.07f, false);
    if ((mix64((u64)(g->time * 1000.0f)) & 3) == 0) { /* it boils over */
        tele_src(HS_OTHER, 0, 0, 0);
        game_dot_player(g, ps, s->maxhp * 0.08f);
        particles_burst(g, at, v3(0.9f, 0.6f, 0.2f), 12, 3.0f, 0.05f, true);
    }
    return false;
}

/* ---------- drinking ---------- */

bool alchemy_drink(Game *g, PlayerSlot *ps, int r) {
    Stats *s = &ps->st;
    if (s->dead || r < 0 || r >= ELIXIR_COUNT || !s->elixirs[r]) return false;
    if (s->elixir && (s->elixir_onset > 0 || s->elixir_t > 0)) {
        if (ps->banner_time <= 0) game_banner(ps, "ANOTHER ELIXIR IS STILL WORKING IN YOU");
        return false;
    }
    s->elixirs[r]--;
    s->elixir = (u8)(r + 1);
    s->elixir_onset = ELIXIRS[r].onset;
    s->elixir_t = ELIXIRS[r].last;
    s->elixir_k = 1.0f;
    bool toxic = s->tox_t > 0;
    if (toxic) { /* too soon after the last: it burns, and works at half */
        s->elixir_k = 0.5f;
        tele_src(HS_OTHER, 0, 0, 0);
        game_dot_player(g, ps, s->maxhp * 0.15f);
    }
    ps->elixir_quick = r;
    g_tele.elixirs++;
    char buf[128], name[64];
    snprintf(name, sizeof name, "%s", ELIXIRS[r].name);
    upper(name);
    if (toxic) snprintf(buf, sizeof buf, "%s - TOO SOON AFTER THE LAST: IT BURNS, AND WILL WORK AT HALF", name);
    else snprintf(buf, sizeof buf, "%s - IT TAKES HOLD IN %d S", name, (int)ceilf(ELIXIRS[r].onset));
    game_banner(ps, buf);
    sfx_player(g, ps, SFX_ELIXIR_DRINK, 1.0f);
    return true;
}

bool alchemy_drink_quick(Game *g, PlayerSlot *ps) {
    const Stats *s = &ps->st;
    int r = ps->elixir_quick;
    if (r < 0 || r >= ELIXIR_COUNT || !s->elixirs[r]) {
        r = -1;
        for (int k = 0; k < ELIXIR_COUNT && r < 0; k++)
            if (s->elixirs[k]) r = k;
    }
    if (r < 0) { if (ps->banner_time <= 0) game_banner(ps, "NO ELIXIRS"); return false; }
    return alchemy_drink(g, ps, r);
}

float alchemy_fx(const Stats *s, int effect) {
    if (!s->elixir || s->elixir_onset > 0 || s->elixir_t <= 0) return 0;
    const Elixir *x = &ELIXIRS[s->elixir - 1];
    float v = 0;
    bool flag = effect == ME_FIRE_RES || effect == ME_FROST_RES || effect == ME_POISON_IMM || effect == ME_SHOCK_RES || effect == ME_REVIVE ||
                effect == ME_FROST_AURA || effect == ME_FEAR_AURA || effect == ME_LAVA;
    float k = flag || s->elixir_k <= 0 ? 1.0f : s->elixir_k;
    for (int i = 0; i < ELIXIR_EFFECTS; i++)
        if (x->fx[i].e == effect && x->fx[i].v != 0) v += x->fx[i].v * k;
    return v;
}

static void take_hold(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    const Elixir *x = &ELIXIRS[s->elixir - 1];
    if (s->elixir_k > 0 && s->elixir_k < 1.0f) s->elixir_t *= s->elixir_k; /* too soon: half as long, too */
    switch (x->start) {
    case EX_INVISIBLE: s->invis_t = fmaxf(s->invis_t, x->last); break;
    case EX_WINGS: s->wings_t = fmaxf(s->wings_t, x->last); break;
    case EX_TIMESLIP: g->timeslip_t = fmaxf(g->timeslip_t, x->last); break;
    case EX_WARD: s->ward = fmaxf(s->ward, s->maxhp * 0.6f); s->ward_t = x->last; break;
    case EX_SIGHT: explore_reveal_area(g, ps->pl.pos, 3); s->reveal_t = fmaxf(s->reveal_t, x->last); break;
    default: break;
    }
    stats_refresh(ps);
    char buf[160], name[64], what[96];
    snprintf(name, sizeof name, "%s", x->name);
    snprintf(what, sizeof what, "%s", x->text);
    upper(name);
    upper(what);
    snprintf(buf, sizeof buf, "%s: %s", name, what);
    game_banner(ps, buf);
    sfx_player(g, ps, SFX_ELIXIR_ONSET, 1.0f);
    vec3 c = v3_add(ps->pl.pos, v3(0, 1.0f, 0));
    particles_burst(g, c, x->color, 30, 4.0f, 0.06f, true);
    flash_add(g, c, x->color, 10.0f, 0.5f);
}

void alchemy_update(Game *g, float dt) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (ps->active && ps->st.tox_t > 0 && !ps->st.elixir) ps->st.tox_t = fmaxf(0.0f, ps->st.tox_t - dt);
        if (!ps->active || !ps->st.elixir) continue;
        Stats *s = &ps->st;
        const Elixir *x = &ELIXIRS[s->elixir - 1];
        vec3 c = v3_add(ps->pl.pos, v3(0, 0.9f, 0));
        if (s->dead) { s->elixir = 0; s->elixir_onset = s->elixir_t = 0; stats_refresh(ps); continue; }
        if (s->elixir_onset > 0) {
            /* it bubbles in you */
            if (fmodf(s->elixir_onset, 0.2f) < dt) particles_burst(g, c, x->color, 2, 1.0f, 0.03f, true);
            if ((s->elixir_onset -= dt) <= 0) { s->elixir_onset = 0; take_hold(g, ps); }
            continue;
        }
        if ((s->elixir_t -= dt) <= 0) {
            s->elixir = 0;
            s->elixir_t = 0;
            s->tox_t = TOXIC_AFTER;
            stats_refresh(ps);
            game_banner(ps, "THE ELIXIR WEARS OFF");
            continue;
        }
        if (fmodf(s->elixir_t, 0.35f) < dt) particles_burst(g, v3_add(ps->pl.pos, v3(0, 0.2f, 0)), x->color, 1, 1.2f, 0.03f, true);
        /* auras */
        bool frost = alchemy_fx(s, ME_FROST_AURA) > 0, fear = alchemy_fx(s, ME_FEAR_AURA) > 0;
        if (frost || fear)
            for (int k = 0; k < MAX_MONSTERS; k++) {
                Monster *m = &g->mons[k];
                if (!m->alive || m->charm_t > 0) continue;
                float d = v3_len(v3_sub(m->pos, ps->pl.pos));
                if (frost && d < 7.0f) {
                    m->slow_t = fmaxf(m->slow_t, 0.6f);
                    if (fmodf(g->time, 0.5f) < dt) particles_burst(g, v3_add(m->pos, v3(0, 0.5f, 0)), v3(0.6f, 0.85f, 1.0f), 2, 1.0f, 0.04f, true);
                }
                if (fear && d < 9.0f && m->grade != GRADE_BOSS) m->fear_t = fmaxf(m->fear_t, 1.0f);
            }
    }
}

/* The Phoenix Elixir: a blow that would kill lifts you again. True if it did. */
bool alchemy_revive(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    if (alchemy_fx(s, ME_REVIVE) <= 0) return false;
    s->elixir = 0;
    s->elixir_t = 0;
    s->tox_t = TOXIC_AFTER;
    stats_refresh(ps);
    s->hp = s->maxhp * 0.5f;
    s->invuln = fmaxf(s->invuln, 1.5f);
    game_banner(ps, "THE PHOENIX ELIXIR BURNS - YOU RISE AGAIN");
    vec3 c = v3_add(ps->pl.pos, v3(0, 1.0f, 0));
    particles_burst(g, c, v3(1.0f, 0.5f, 0.1f), 50, 6.0f, 0.08f, true);
    flash_add(g, c, v3(1.5f, 0.7f, 0.2f), 16.0f, 0.8f);
    sfx_player(g, ps, SFX_ELIXIR_ONSET, 1.0f);
    return true;
}

/* ---------- drops and finds ---------- */

void alchemy_monster_drop(Game *g, const Monster *m) {
    if (m->type >= RG_MONSTER_COUNT) return;
    u64 h = mix64((u64)(i64)(m->pos.x * 131.0f) ^ ((u64)(i64)(m->pos.z * 977.0f) << 21) ^ (u64)(g->time * 1000.0f));
    float r = (float)(h & 0xFFFF) / 65536.0f;
    float chance = m->grade == GRADE_BOSS ? 1.0f : m->elite ? 0.6f : 0.18f;
    if (m->type == MON_SLIME && m->scale < 0.75f) chance *= 0.3f; /* the little ones that split off */
    vec3 p = v3_add(m->pos, v3(0, 0.5f, 0));
    if (r < chance) pickup_spawn(g, PK_REAGENT, p, m->type, NULL, 0, 0);
    if (m->grade == GRADE_BOSS) pickup_spawn(g, PK_REAGENT, p, m->type, NULL, 0, 0);
    /* now and then a page of formulae on the stronger ones */
    float fr = (float)((h >> 20) & 0xFFFF) / 65536.0f;
    if (fr < (m->grade == GRADE_BOSS ? 0.25f : m->elite ? 0.04f : 0.0f)) pickup_spawn(g, PK_FORMULA, p, ((h >> 40) & 1) ? -2 : -1, NULL, 0, 0); /* half torn */
}

/* Strange reagents that lie about in some places (items.c places them with the plants). */
void alchemy_world_weights(const ChunkInfo *ci, float *w) {
    switch (ci->stratum) {
    case ST_MINES: w[RG_QUICKSILVER] += 1.0f; break;
    case ST_IRONHELL: w[RG_QUICKSILVER] += 0.6f; w[RG_BRIMSTONE] += 1.0f; break;
    case ST_INFERNO: case ST_MAW: case ST_CORE: w[RG_BRIMSTONE] += 1.5f; break;
    case ST_LIMBO: case ST_OSSUARY: w[RG_ORCHID] += 1.0f; break;
    case ST_HEAVEN: case ST_ASTRAL: case ST_ORRERY: w[RG_DEW] += 1.0f; break;
    case ST_CRYSTAL: w[RG_VOIDSALT] += 0.8f; break;
    case ST_ROOTS: case ST_FUNGAL: w[RG_MANDRAKE] += 1.0f; break;
    case ST_SUNCOURT: w[RG_PHOENIX] += 0.15f; w[RG_DEW] += 0.5f; break;
    default: break;
    }
    int f = ci->flavor;
    if (f == FL_BRASS) w[RG_QUICKSILVER] += 1.0f;
    if (f == FL_SULPHUR || f == FL_OBSIDIAN || f == FL_EMBER) w[RG_BRIMSTONE] += 1.0f;
    if (f == FL_BARROW || f == FL_HOLLOW) w[RG_ORCHID] += 1.0f;
    if (f == FL_STARFROST || f == FL_AURORA) w[RG_DEW] += 0.8f;
    if (f == FL_ABYSSAL || f == FL_ELDRITCH || f == FL_LEVIATHAN) w[RG_VOIDSALT] += 1.0f;
    if (f == FL_DRUIDIC) w[RG_MANDRAKE] += 1.0f;
    if (f == FL_BAMBOO || f == FL_JADE || f == FL_TERRACOTTA || f == FL_STEPPE) w[RG_AMBER] += 1.0f;
    if (f == FL_PHOENIX || f == FL_SUNBLAST) w[RG_PHOENIX] += 0.25f;
}

/* ---------- the ALCHEMY tab ---------- */

enum { AR_INFO, AR_BOTTLE, AR_EMPTY, AR_ACT, AR_ELIXIR, AR_REAGENT, AR_FOOD, AR_HEAD, AR_FORMULA };
typedef struct AlchRow { u8 type; int a; } AlchRow;
#define ALCH_ROWS (8 + AS_COUNT + ELIXIR_COUNT + RG_COUNT + FD_COUNT + ELIXIR_COUNT)

static bool used_in_alchemy(int food) {
    for (int r = 0; r < ELIXIR_COUNT; r++)
        for (int k = 0; k < FORMULA_STEPS; k++)
            if (ELIXIRS[r].steps[k] == STEP_FOOD(food)) return true;
    return false;
}

static int alch_rows(Game *g, PlayerSlot *ps, AlchRow *rows, int *station) {
    const Stats *s = &ps->st;
    int n = 0, st = alchemy_station(g, ps);
    if (station) *station = st;
    if (st) {
        rows[n++] = (AlchRow){ AR_BOTTLE, 0 };
        if (ps->ncauldron) rows[n++] = (AlchRow){ AR_EMPTY, 0 };
        for (int a = 0; a < AS_COUNT; a++) rows[n++] = (AlchRow){ AR_ACT, a };
    }
    for (int r = 0; r < ELIXIR_COUNT; r++)
        if (s->elixirs[r]) rows[n++] = (AlchRow){ AR_ELIXIR, r };
    for (int r = 0; r < RG_COUNT; r++)
        if (s->reag[r]) rows[n++] = (AlchRow){ AR_REAGENT, r };
    for (int f = 0; f < FD_COUNT; f++)
        if (s->food[f] && used_in_alchemy(f)) rows[n++] = (AlchRow){ AR_FOOD, f };
    if (n == 0) rows[n++] = (AlchRow){ AR_INFO, 0 };
    rows[n++] = (AlchRow){ AR_HEAD, 0 };
    for (int r = 0; r < ELIXIR_COUNT; r++)
        if (s->formulae & (1u << r)) rows[n++] = (AlchRow){ AR_FORMULA, r };
    for (int r = 0; r < ELIXIR_COUNT; r++)
        if (!(s->formulae & (1u << r)) && (s->formulae_torn & (1u << r))) rows[n++] = (AlchRow){ AR_FORMULA, r };
    for (int r = 0; r < ELIXIR_COUNT; r++)
        if (!((s->formulae | s->formulae_torn) & (1u << r))) rows[n++] = (AlchRow){ AR_FORMULA, r };
    return n;
}

static void act(Game *g, PlayerSlot *ps, const AlchRow *row) {
    switch (row->type) {
    case AR_BOTTLE: alchemy_bottle(g, ps); break;
    case AR_EMPTY: ps->ncauldron = 0; break;
    case AR_ACT: alchemy_add(g, ps, STEP_ACT(row->a)); break;
    case AR_ELIXIR: alchemy_drink(g, ps, row->a); break;
    case AR_REAGENT: alchemy_add(g, ps, STEP_RG(row->a)); break;
    case AR_FOOD: alchemy_add(g, ps, STEP_FOOD(row->a)); break;
    default: break;
    }
}

void alchemy_nav(Game *g, PlayerSlot *ps, int nav) {
    AlchRow rows[ALCH_ROWS];
    int n = alch_rows(g, ps, rows, NULL);
    if (nav == NAV_UP) ps->alch_sel = (ps->alch_sel + n - 1) % n;
    else if (nav == NAV_DOWN) ps->alch_sel = (ps->alch_sel + 1) % n;
    else if (nav == NAV_BUY && ps->alch_sel < n) act(g, ps, &rows[ps->alch_sel]);
    else if (nav == NAV_DROP && ps->ncauldron) ps->ncauldron--; /* take the last step back */
    if (ps->alch_sel >= n) ps->alch_sel = n - 1;
}

#define ROW 11
static int rows_fit(int H, int top) { return MAXI(3, (H - top - 34 - 18) / ROW); }
static int first_row(const PlayerSlot *ps, int n, int fit) { return MAXI(0, MINI(ps->alch_sel - fit / 2, n - fit)); }
#define LIST_Y(top) ((top) + 34)

void alchemy_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y) {
    (void)W; (void)x;
    AlchRow rows[ALCH_ROWS];
    int n = alch_rows(g, ps, rows, NULL);
    int fit = rows_fit(H, top), f = first_row(ps, n, fit);
    int row = (int)floorf((y - LIST_Y(top)) / ROW);
    if (row < 0 || row >= fit || f + row >= n) return;
    row += f;
    if (row == ps->alch_sel) act(g, ps, &rows[row]);
    else ps->alch_sel = row;
}

static void formula_text(const Elixir *x, char *out, int n) {
    out[0] = 0;
    for (int k = 0; k < FORMULA_STEPS && x->steps[k] != STEP_NONE; k++) {
        char st[40];
        alchemy_step_name(x->steps[k], st, sizeof st);
        snprintf(out + strlen(out), (size_t)(n - (int)strlen(out)), "%s%s", k ? " > " : "", st);
    }
}

void alchemy_draw(Game *g, PlayerSlot *ps, int W, int H, int top) {
    const Stats *s = &ps->st;
    AlchRow rows[ALCH_ROWS];
    int st;
    int n = alch_rows(g, ps, rows, &st);
    if (ps->alch_sel >= n) ps->alch_sel = n - 1;
    int fit = rows_fit(H, top), f = first_row(ps, n, fit);
    int x = MAXI(6, (W - 74 * 6) / 2);
    char buf[240], t[160];
    batch2d_begin(W, H);
    for (int i = f; i < n && i < f + fit; i++) {
        if (rows[i].type == AR_HEAD) continue;
        float ry = (float)(LIST_Y(top) + (i - f) * ROW) - 2.0f;
        bool sel = i == ps->alch_sel;
        batch2d_rect((float)x - 3, ry, (float)(W - 2 * x + 6), ROW - 1, sel ? 0.2f : 0.06f, sel ? 0.22f : 0.08f, sel ? 0.12f : 0.05f, 0.85f);
    }
    batch2d_flush();
    font_begin(W, H);
    if (s->elixir && s->elixir_onset > 0) snprintf(buf, sizeof buf, "ELIXIR: %s - TAKING HOLD IN %.0f S", ELIXIRS[s->elixir - 1].name, ceilf(s->elixir_onset));
    else if (s->elixir && s->elixir_t > 0) snprintf(buf, sizeof buf, "ELIXIR: %s - %.0f S LEFT", ELIXIRS[s->elixir - 1].name, ceilf(s->elixir_t));
    else snprintf(buf, sizeof buf, "NO ELIXIR WORKING");
    upper(buf);
    font_text(x, top + 6, buf, 0.6f, 1.0f, 0.65f);
    if (st) snprintf(buf, sizeof buf, "%s: STEPS GO IN IN ORDER. X TAKES THE LAST BACK", st > 1 ? "AT HOME, THE LIBRARY'S ALEMBIC (TWO BOTTLES)" : realm_has(ps->pl.pos) ? "AT HOME" : "AT A SHRINE");
    else snprintf(buf, sizeof buf, "BREW AT A SHRINE OR AT HOME. ENTER DRINKS. V DRINKS");
    font_text(x, top + 18, buf, 0.6f, 0.65f, 0.55f);
    if (f > 0) font_text(W - x - 6, LIST_Y(top) - 10, "^", 0.7f, 0.8f, 0.6f);
    if (f + fit < n) font_text(W - x - 6, LIST_Y(top) + fit * ROW - 2, "V", 0.7f, 0.8f, 0.6f);
    int known = __builtin_popcount(s->formulae);
    int maxc = (W - 2 * x) / 6;
    for (int i = f; i < n && i < f + fit; i++) {
        const AlchRow *row = &rows[i];
        int ry = LIST_Y(top) + (i - f) * ROW;
        char rbuf[40] = "";
        float cr = 0.85f, cg = 0.9f, cb = 0.8f;
        switch (row->type) {
        case AR_INFO: snprintf(buf, sizeof buf, "NO REAGENTS: MONSTERS DROP THEM, STRANGE PLACES HOLD THEM"); cr = cg = cb = 0.6f; break;
        case AR_BOTTLE:
            if (!ps->ncauldron) snprintf(buf, sizeof buf, "THE CAULDRON IS EMPTY: ADD STEPS BELOW, IN ORDER");
            else {
                snprintf(buf, sizeof buf, "BOTTLE: ");
                for (int k = 0; k < ps->ncauldron; k++) {
                    alchemy_step_name(ps->cauldron[k], t, 40);
                    snprintf(buf + strlen(buf), sizeof buf - strlen(buf), "%s%s", k ? " > " : "", t);
                }
            }
            cr = 0.6f; cg = 1.0f; cb = 0.6f;
            break;
        case AR_EMPTY: snprintf(buf, sizeof buf, "EMPTY THE CAULDRON"); cr = cg = cb = 0.7f; break;
        case AR_ACT: snprintf(buf, sizeof buf, "+ %s", ACT_NAMES[row->a]); cr = 0.8f; cg = 0.75f; cb = 0.55f; break;
        case AR_ELIXIR: {
            const Elixir *e = &ELIXIRS[row->a];
            snprintf(buf, sizeof buf, "%s: %s", e->name, e->text);
            snprintf(rbuf, sizeof rbuf, "X%d  %.0fS", s->elixirs[row->a], e->last);
            cr = 0.5f + 0.5f * e->color.x; cg = 0.5f + 0.5f * e->color.y; cb = 0.5f + 0.5f * e->color.z;
            break;
        }
        case AR_REAGENT:
            snprintf(buf, sizeof buf, "+ %s", REAGENTS[row->a].name);
            snprintf(rbuf, sizeof rbuf, "X%d", free_of(ps, STEP_RG(row->a)));
            cr = 0.55f + 0.45f * REAGENTS[row->a].color.x; cg = 0.55f + 0.45f * REAGENTS[row->a].color.y; cb = 0.55f + 0.45f * REAGENTS[row->a].color.z;
            break;
        case AR_FOOD:
            snprintf(buf, sizeof buf, "+ %s (from the larder)", FOODS[row->a].name);
            snprintf(rbuf, sizeof rbuf, "X%d", free_of(ps, STEP_FOOD(row->a)));
            cr = 0.8f; cg = 0.75f; cb = 0.6f;
            break;
        case AR_HEAD: snprintf(buf, sizeof buf, "FORMULAE: %d OF %d", known, ELIXIR_COUNT); cr = 0.6f; cg = 1.0f; cb = 0.65f; break;
        case AR_FORMULA: {
            const Elixir *e = &ELIXIRS[row->a];
            if (s->formulae & (1u << row->a)) {
                formula_text(e, t, sizeof t);
                snprintf(buf, sizeof buf, "%s: %s", e->name, t);
                snprintf(rbuf, sizeof rbuf, "%.0f+%.0fS", e->onset, e->last);
            } else if (s->formulae_torn & (1u << row->a)) { /* a torn page: what goes in, not how */
                t[0] = 0;
                int acts = 0;
                for (int k = 0; k < FORMULA_STEPS && e->steps[k] != STEP_NONE; k++) {
                    if (step_is_act(e->steps[k])) { acts++; continue; }
                    char st[40];
                    alchemy_step_name(e->steps[k], st, sizeof st);
                    snprintf(t + strlen(t), sizeof t - strlen(t), "%s%s", t[0] ? ", " : "", st);
                }
                snprintf(buf, sizeof buf, "%s (TORN): %s, AND %d STEPS, IN SOME ORDER", e->name, t, acts);
                snprintf(rbuf, sizeof rbuf, "%d STEPS", alchemy_steps(e->steps));
                cr = 0.75f; cg = 0.8f; cb = 0.6f;
            } else {
                snprintf(buf, sizeof buf, "???: A FORMULA OF %d STEPS", alchemy_steps(e->steps));
                cr = cg = cb = 0.5f;
            }
            break;
        }
        }
        upper(buf);
        int room = maxc - (int)strlen(rbuf) - 2;
        if ((int)strlen(buf) > room) buf[MAXI(0, room)] = 0;
        font_text(x, ry, buf, cr, cg, cb);
        if (rbuf[0]) font_text(W - x - font_text_width(rbuf), ry, rbuf, 0.7f, 0.95f, 0.6f);
    }
    font_text(x, H - 14, "ENTER: ADD, BOTTLE OR DRINK   X: TAKE A STEP BACK   V: DRINK", 0.55f, 0.6f, 0.5f);
    font_end();
}

/* ---------- how reagents and pages look ---------- */

void alchemy_draw_reagent(int rg, vec3 c, float yaw, float s, vec3 l) {
    if (rg < 0 || rg >= RG_COUNT) return;
    const ReagentDef *d = &REAGENTS[rg];
    vec3 col = d->glow ? d->color : v3(d->color.x * l.x, d->color.y * l.y, d->color.z * l.z);
    vec3 glass = v3(0.6f * l.x, 0.7f * l.y, 0.75f * l.z);
    switch (d->model) {
    case RM_VIAL:
        batch3d_box(v3_add(c, v3(0, 0.1f * s, 0)), v3(0.05f * s, 0.1f * s, 0.05f * s), yaw, col, d->glow);
        batch3d_box(v3_add(c, v3(0, 0.23f * s, 0)), v3(0.025f * s, 0.035f * s, 0.025f * s), yaw, glass, false);
        batch3d_box(v3_add(c, v3(0, 0.28f * s, 0)), v3(0.03f * s, 0.015f * s, 0.03f * s), yaw, v3(0.4f * l.x, 0.28f * l.y, 0.15f * l.z), false);
        break;
    case RM_STONE:
        batch3d_octa(v3_add(c, v3(0, 0.1f * s, 0)), 0.09f * s, 0.1f * s, yaw, col, d->glow);
        break;
    case RM_FEATHER:
        batch3d_box(v3_add(c, v3(0, 0.05f * s, 0)), v3(0.17f * s, 0.01f * s, 0.05f * s), yaw, col, d->glow);
        batch3d_box(v3_add(c, v3(0, 0.06f * s, 0)), v3(0.19f * s, 0.008f * s, 0.008f * s), yaw, v3_scale(col, 0.7f), false);
        break;
    case RM_FANG:
        batch3d_spike(v3_add(c, v3(0, 0.02f * s, 0)), v3(0.3f, 1, 0), 0.04f * s, 0.2f * s, col, false);
        batch3d_spike(v3_add(c, v3(0.06f * s, 0.02f * s, 0)), v3(-0.2f, 1, 0.2f), 0.03f * s, 0.14f * s, col, false);
        break;
    case RM_DUST:
        batch3d_box(v3_add(c, v3(0, 0.05f * s, 0)), v3(0.09f * s, 0.05f * s, 0.07f * s), yaw, v3(0.55f * l.x, 0.42f * l.y, 0.28f * l.z), false);
        batch3d_box(v3_add(c, v3(0, 0.11f * s, 0)), v3(0.07f * s, 0.02f * s, 0.05f * s), yaw, col, d->glow);
        break;
    case RM_LENS:
        batch3d_octa(v3_add(c, v3(0, 0.12f * s, 0)), 0.08f * s, 0.08f * s, yaw, col, d->glow);
        batch3d_octa(v3_add(c, v3(0, 0.12f * s, 0)), 0.04f * s, 0.05f * s, -yaw, v3_scale(col, 0.4f), false);
        break;
    case RM_ROOT:
        batch3d_box(v3_add(c, v3(0, 0.07f * s, 0)), v3(0.05f * s, 0.07f * s, 0.05f * s), yaw + 0.5f, col, d->glow);
        batch3d_box(v3_add(c, v3(0.04f * s, 0.03f * s, 0)), v3(0.06f * s, 0.015f * s, 0.015f * s), yaw - 0.4f, v3_scale(col, 0.8f), false);
        batch3d_box(v3_add(c, v3(-0.04f * s, 0.03f * s, 0)), v3(0.06f * s, 0.015f * s, 0.015f * s), yaw + 0.9f, v3_scale(col, 0.8f), false);
        break;
    case RM_FLAME:
        batch3d_box(v3_add(c, v3(0, 0.1f * s, 0)), v3(0.06f * s, 0.1f * s, 0.06f * s), yaw, glass, false);
        batch3d_octa(v3_add(c, v3(0, 0.1f * s, 0)), 0.04f * s, 0.07f * s, yaw * 3.0f, col, true);
        break;
    }
}

void alchemy_draw_formula(vec3 c, float yaw, vec3 l) {
    batch3d_box(v3_add(c, v3(0, 0.12f, 0)), v3(0.16f, 0.04f, 0.04f), yaw, v3(0.85f * l.x, 0.8f * l.y, 0.65f * l.z), false);
    batch3d_box(v3_add(c, v3(0, 0.12f, 0)), v3(0.12f, 0.045f, 0.045f), yaw, v3(0.2f, 0.6f, 0.25f), false);
    batch3d_octa(v3_add(c, v3(0, 0.35f, 0)), 0.05f, 0.07f, yaw * 2.0f, v3(0.5f, 1.0f, 0.55f), true);
}
