#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

#include <stdio.h>
#include <string.h>

/* The forge (docs/FORGE.md). Smithing is a skill from 1 to 10, learned by forging. At the
 * anvil in the Sanctum's forge you plan a weapon (a blueprint you know, a metal, up to two
 * essences, up to two runes, a titan heart and quenching oil), then forge it: the metal
 * cools, the bellows heat it, and each strike counts by how close the heat is to the
 * metal's window. The quality of the strikes, the metal and your skill make the weapon;
 * a master with starmetal makes weapons nothing in the dungeon matches. */

const ForgeMatDef FORGE_MATS[FG_COUNT] = {
    [FG_IRON_ORE] = { "iron ore", { 0.55f, 0.45f, 0.4f }, 0 },
    [FG_SILVER_ORE] = { "silver ore", { 0.75f, 0.78f, 0.82f }, 0 },
    [FG_MITHRIL_ORE] = { "mithril ore", { 0.55f, 0.8f, 0.95f }, 0 },
    [FG_ADAMANT_ORE] = { "adamant ore", { 0.35f, 0.3f, 0.45f }, 0 },
    [FG_STAR_ORE] = { "star ore", { 0.85f, 0.85f, 1.0f }, 0 },
    [FG_IRON] = { "iron ingot", { 0.6f, 0.6f, 0.62f }, 1 },
    [FG_STEEL] = { "steel ingot", { 0.75f, 0.78f, 0.82f }, 1 },
    [FG_SILVER] = { "silver ingot", { 0.92f, 0.93f, 0.97f }, 1 },
    [FG_MITHRIL] = { "mithril ingot", { 0.6f, 0.9f, 1.0f }, 1 },
    [FG_ADAMANT] = { "adamant ingot", { 0.4f, 0.32f, 0.55f }, 1 },
    [FG_MOONSILVER] = { "moonsilver ingot", { 0.8f, 0.85f, 1.0f }, 1 },
    [FG_STARMETAL] = { "starmetal ingot", { 1.0f, 0.95f, 0.75f }, 1 },
    [FG_ESS_FIRE] = { "ember essence", { 1.0f, 0.45f, 0.1f }, 2 },
    [FG_ESS_FROST] = { "frost essence", { 0.5f, 0.8f, 1.0f }, 2 },
    [FG_ESS_STORM] = { "storm essence", { 0.7f, 0.75f, 1.0f }, 2 },
    [FG_ESS_VENOM] = { "venom essence", { 0.5f, 0.95f, 0.2f }, 2 },
    [FG_ESS_HOLY] = { "holy essence", { 1.0f, 0.95f, 0.6f }, 2 },
    [FG_ESS_SHADOW] = { "shadow essence", { 0.5f, 0.25f, 0.6f }, 2 },
    [FG_ESS_EARTH] = { "earth essence", { 0.7f, 0.55f, 0.3f }, 2 },
    [FG_ESS_ARCANE] = { "arcane essence", { 0.8f, 0.4f, 1.0f }, 2 },
    [FG_TITAN_HEART] = { "titan heart", { 1.0f, 0.3f, 0.2f }, 3 },
    [FG_CHARCOAL] = { "charcoal", { 0.2f, 0.2f, 0.22f }, 4 },
    [FG_FLUX] = { "flux", { 0.85f, 0.8f, 0.6f }, 4 },
    [FG_QUENCH_OIL] = { "quenching oil", { 0.6f, 0.45f, 0.2f }, 4 },
    [FG_RUNE_INK] = { "rune ink", { 0.3f, 0.5f, 1.0f }, 4 },
};

const MetalDef METALS[MT_COUNT] = {
    /*            name         ingot          tier lo     hi     cool   bonus material       smith stage flux */
    [MT_IRON] = { "iron", FG_IRON, 1, 0.40f, 0.72f, 0.10f, 0, MAT_IRON, 1, 1, false },
    [MT_STEEL] = { "steel", FG_STEEL, 2, 0.50f, 0.74f, 0.12f, 3, MAT_IRON, 2, 1, false },
    [MT_SILVER] = { "silver", FG_SILVER, 2, 0.35f, 0.58f, 0.14f, 2, MAT_SILVER, 3, 1, false },
    [MT_MITHRIL] = { "mithril", FG_MITHRIL, 3, 0.60f, 0.78f, 0.15f, 6, MAT_CRYSTAL, 5, 2, true },
    [MT_ADAMANT] = { "adamant", FG_ADAMANT, 4, 0.70f, 0.85f, 0.17f, 9, MAT_OBSIDIAN, 7, 2, true },
    [MT_MOONSILVER] = { "moonsilver", FG_MOONSILVER, 4, 0.45f, 0.60f, 0.13f, 9, MAT_SILVER, 6, 2, false },
    [MT_STARMETAL] = { "starmetal", FG_STARMETAL, 5, 0.80f, 0.92f, 0.20f, 13, MAT_STARMETAL, 9, 3, true },
};

static const int SMITH_XP[SMITH_MAX + 1] = { 0, 0, 80, 200, 380, 620, 950, 1400, 2000, 2800, 3800 };
int smith_xp_for(int level) { return SMITH_XP[level < 1 ? 1 : level > SMITH_MAX ? SMITH_MAX : level]; }
int smith_level(int xp) {
    int l = 1;
    while (l < SMITH_MAX && xp >= SMITH_XP[l + 1]) l++;
    return l;
}

int forge_ingots(int base) {
    switch (base) {
    case WB_DAGGER: case WB_WAND: case WB_SLING: case WB_WHIP: return 1;
    case WB_GREATSWORD: case WB_FLAIL: case WB_SCYTHE: case WB_LONGBOW: case WB_CROSSBOW: case WB_STAFF: case WB_CANNON: return 3;
    default: return 2;
    }
}

static void upper(char *s) {
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}

#define FORGE_MAX 99
#define BLUEPRINTS_START ((1u << WB_DAGGER) | (1u << WB_SWORD) | (1u << WB_SHORTBOW) | (1u << WB_STAFF))

/* essences become affixes */
static const u8 ESS_AFFIX[FG_ESS_COUNT] = { AF_FIRE, AF_FROST, AF_SHOCK, AF_VENOM, AF_KEEN, AF_VAMPIRIC, AF_HEAVY, AF_SWIFT };

/* ---------- materials ---------- */

void forge_give(Game *g, PlayerSlot *ps, int m, int n) {
    if (m < 0 || m >= FG_COUNT || n <= 0) return;
    Stats *s = &ps->st;
    s->forge[m] = (u8)MINI(FORGE_MAX, s->forge[m] + n);
    char buf[96], name[48];
    snprintf(name, sizeof name, "%s", FORGE_MATS[m].name);
    upper(name);
    if (n > 1) snprintf(buf, sizeof buf, "%d %s (%d)", n, name, s->forge[m]);
    else snprintf(buf, sizeof buf, "%s (%d)", name, s->forge[m]);
    game_banner(ps, buf);
    sfx_player(g, ps, FORGE_MATS[m].kind == 0 ? SFX_ORE_MINE : SFX_REAGENT_PICKUP, 0.8f);
    g_tele.forge_mats += n;
}

static int smith(const Stats *s) { return smith_level(s->smith_xp); }
static int forge_stage(Game *g) { return sanctum_stage(g, SP_ISLAND) >= 2 ? sanctum_stage(g, SP_FORGE) : 0; }

bool forge_here(Game *g, const PlayerSlot *ps) {
    if (!realm_has(ps->pl.pos) || forge_stage(g) < 1) return false;
    float x0 = (float)plot_x(SP_FORGE), z0 = (float)plot_z(SP_FORGE);
    vec3 p = ps->pl.pos;
    return p.x >= x0 && p.x < x0 + 32 && p.z >= z0 && p.z < z0 + 32;
}

static void gain_xp(Game *g, PlayerSlot *ps, int xp) {
    Stats *s = &ps->st;
    int before = smith(s);
    s->smith_xp += xp;
    int after = smith(s);
    if (after > before) {
        char buf[96];
        snprintf(buf, sizeof buf, "SMITHING %d: %s", after,
                 after == 2 ? "STEEL" : after == 3 ? "SILVER" : after == 4 ? "TWO ESSENCES, ONE RUNE" : after == 5 ? "MITHRIL"
                 : after == 6 ? "MOONSILVER" : after == 7 ? "ADAMANT" : after == 8 ? "TWO RUNES, MASTERWORKS" : after == 9 ? "STARMETAL" : "A MASTER SMITH");
        game_banner(ps, buf);
        sfx_ui(SFX_LEVEL_UP, 0.9f);
    }
    (void)g;
}

/* the metal a weapon is made of, for reforging and salvage */
static int metal_of(const Item *it) {
    switch (it->material) {
    case MAT_SILVER: return MT_SILVER;
    case MAT_CRYSTAL: return MT_MITHRIL;
    case MAT_OBSIDIAN: return MT_ADAMANT;
    case MAT_STARMETAL: return MT_STARMETAL;
    default: return MT_IRON;
    }
}

static bool metal_open(Game *g, const Stats *s, int mt) { return smith(s) >= METALS[mt].smith && forge_stage(g) >= METALS[mt].stage; }
static int rune_slots(Game *g, const Stats *s) { return (smith(s) >= 4 && forge_stage(g) >= 2) + (smith(s) >= 8 && forge_stage(g) >= 3); }
static int ess_slots(const Stats *s) { return 1 + (smith(s) >= 4); }

/* ---------- the plan and its cost ---------- */

typedef struct Cost { u8 m[8]; u8 n[8]; int k; } Cost;
static void cost_add(Cost *c, int m, int n) {
    for (int i = 0; i < c->k; i++)
        if (c->m[i] == m) { c->n[i] = (u8)(c->n[i] + n); return; }
    if (c->k < 8) { c->m[c->k] = (u8)m; c->n[c->k] = (u8)n; c->k++; }
}

static Cost plan_cost(Game *g, PlayerSlot *ps, bool reforge) {
    Cost c = { 0 };
    const Stats *s = &ps->st;
    if (reforge) {
        const Item *it = &s->equipped[s->weapon];
        int mt = metal_of(it);
        cost_add(&c, METALS[mt].ingot, forge_ingots(it->base));
        cost_add(&c, FG_CHARCOAL, 2);
        if (METALS[mt].flux) cost_add(&c, FG_FLUX, 1);
        return c;
    }
    const MetalDef *m = &METALS[ps->plan.metal];
    int n = forge_ingots(ps->plan.base);
    cost_add(&c, m->ingot, n);
    cost_add(&c, FG_CHARCOAL, 1 + n);
    if (m->flux) cost_add(&c, FG_FLUX, 1);
    for (int k = 0; k < 2; k++)
        if (ps->plan.ess[k]) cost_add(&c, ps->plan.ess[k] - 1, 1);
    for (int k = 0; k < 2; k++)
        if (ps->plan.rune[k]) cost_add(&c, FG_RUNE_INK, 1);
    if (ps->plan.heart) cost_add(&c, FG_TITAN_HEART, 1);
    if (ps->plan.oil) cost_add(&c, FG_QUENCH_OIL, 1);
    (void)g;
    return c;
}

/* what is missing for the plan, or "" */
static void missing(const Stats *s, const Cost *c, char *out, int n) {
    out[0] = 0;
    for (int i = 0; i < c->k; i++)
        if (s->forge[c->m[i]] < c->n[i])
            snprintf(out + strlen(out), (size_t)(n - (int)strlen(out)), "%s%d %s", out[0] ? ", " : "", c->n[i] - s->forge[c->m[i]], FORGE_MATS[c->m[i]].name);
}

static void cost_text(const Cost *c, char *out, int n) {
    out[0] = 0;
    for (int i = 0; i < c->k; i++)
        snprintf(out + strlen(out), (size_t)(n - (int)strlen(out)), "%s%d %s", i ? ", " : "", c->n[i], FORGE_MATS[c->m[i]].name);
}

/* keep the plan within what is known and open */
static void plan_fix(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    if (!s->blueprints) s->blueprints = BLUEPRINTS_START;
    if (!((s->blueprints >> ps->plan.base) & 1))
        for (int b = 0; b < WB_COUNT; b++)
            if ((s->blueprints >> b) & 1) { ps->plan.base = (u8)b; break; }
    if (ps->plan.metal >= MT_COUNT || !metal_open(g, s, ps->plan.metal)) ps->plan.metal = MT_IRON;
    if (ess_slots(s) < 2) ps->plan.ess[1] = 0;
    int rs = rune_slots(g, s);
    for (int k = rs; k < 2; k++) ps->plan.rune[k] = 0;
    if (!s->forge[FG_TITAN_HEART]) ps->plan.heart = 0;
    if (!s->forge[FG_QUENCH_OIL]) ps->plan.oil = 0;
}

/* ---------- the session at the anvil ---------- */

static int session_metal(const PlayerSlot *ps) { return ps->fs.reforge ? metal_of(&ps->st.equipped[ps->st.weapon]) : ps->plan.metal; }

void forge_window(const PlayerSlot *ps, float *lo, float *hi) {
    const MetalDef *m = &METALS[session_metal(ps)];
    float w = 0.008f * (float)smith(&ps->st); /* a practised hand has more room */
    *lo = m->lo - w;
    *hi = m->hi + w;
}

bool forge_start(Game *g, PlayerSlot *ps, bool reforge) {
    Stats *s = &ps->st;
    if (!forge_here(g, ps)) { game_banner(ps, "FORGE AT THE ANVIL IN THE SANCTUM'S FORGE"); return false; }
    if (ps->fs.on) return false;
    plan_fix(g, ps);
    if (reforge) {
        const Item *it = &s->equipped[s->weapon];
        if (!it->used) return false;
        if (!metal_open(g, s, metal_of(it))) { game_banner(ps, "YOU CANNOT WORK THAT METAL YET"); return false; }
    } else {
        int free = -1;
        for (int i = 0; i < BAG_SIZE && free < 0; i++) if (!s->bag[i].used) free = i;
        if (free < 0) { game_banner(ps, "THE BAG IS FULL"); return false; }
    }
    ps->fs.reforge = reforge;
    Cost c = plan_cost(g, ps, reforge);
    char miss[160];
    missing(s, &c, miss, sizeof miss);
    if (miss[0]) {
        char buf[200];
        snprintf(buf, sizeof buf, "MISSING: %s", miss);
        upper(buf);
        game_banner(ps, buf);
        return false;
    }
    for (int i = 0; i < c.k; i++) s->forge[c.m[i]] = (u8)(s->forge[c.m[i]] - c.n[i]);
    int base = reforge ? s->equipped[s->weapon].base : ps->plan.base;
    ps->fs.on = true;
    ps->fs.heat = 0.25f;
    ps->fs.need = 3 + 2 * forge_ingots(base);
    ps->fs.done = ps->fs.score = ps->fs.flaws = 0;
    ps->fs.last = -1;
    ps->fs.flash = 0;
    ps->fs.t = 0;
    snprintf(ps->fs.msg, sizeof ps->fs.msg, "BELLOWS (X) TO HEAT IT, STRIKE (ENTER) IN THE WINDOW");
    sfx_player(g, ps, SFX_SMELT, 0.8f);
    return true;
}

void forge_bellows(Game *g, PlayerSlot *ps) {
    if (!ps->fs.on) return;
    ps->fs.heat = fminf(1.0f, ps->fs.heat + 0.14f);
    sfx_player(g, ps, SFX_BELLOWS, 0.6f);
}

static void finish(Game *g, PlayerSlot *ps);

void forge_strike(Game *g, PlayerSlot *ps) {
    if (!ps->fs.on) return;
    float lo, hi, h = ps->fs.heat;
    forge_window(ps, &lo, &hi);
    float c = (lo + hi) * 0.5f, w = hi - lo;
    int pts;
    if (h >= lo && h <= hi) pts = fabsf(h - c) < w * 0.25f ? 3 : 2;
    else if (h >= lo - 0.08f && h <= hi + 0.08f) pts = 1;
    else pts = 0;
    ps->fs.score += pts;
    ps->fs.last = pts;
    ps->fs.done++;
    ps->fs.flash = 1.0f;
    if (pts == 0) ps->fs.flaws++;
    static const char *SAY[4] = { "", "CLOSE", "GOOD", "PERFECT" };
    if (pts == 0) snprintf(ps->fs.msg, sizeof ps->fs.msg, "%s", h < lo ? "TOO COLD: IT CRACKS" : "TOO HOT: IT BURNS");
    else snprintf(ps->fs.msg, sizeof ps->fs.msg, "%s", SAY[pts]);
    sfx_player(g, ps, pts >= 2 ? SFX_ANVIL : SFX_ANVIL_BAD, pts == 3 ? 1.0f : 0.8f);
    ps->fs.heat = fmaxf(0.0f, h - 0.06f);
    if (ps->fs.done >= ps->fs.need) finish(g, ps);
}

void forge_tick(Game *g, float dt) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || !ps->fs.on) continue;
        if (!forge_here(g, ps)) { ps->fs.on = false; continue; } /* walked away: the work is lost */
        ps->fs.t += dt;
        ps->fs.heat = fmaxf(0.0f, ps->fs.heat - METALS[session_metal(ps)].cool * dt);
        ps->fs.flash = fmaxf(0.0f, ps->fs.flash - dt * 3.0f);
    }
}

static void finish(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    ps->fs.on = false;
    float q = (float)ps->fs.score / (3.0f * (float)ps->fs.need);
    if (!ps->fs.reforge) q += (ps->plan.heart ? 0.10f : 0.0f) + (ps->plan.oil ? 0.06f : 0.0f);
    if (ps->fs.flaws >= 3) q -= 0.1f;
    q = clampf(q, 0.0f, 1.0f);
    int sk = smith(s), mt = session_metal(ps);
    const MetalDef *m = &METALS[mt];
    char buf[200], name[96];
    u64 h = mix64((u64)(g->time * 1000.0f) ^ (u64)s->smith_xp ^ 0xF0E6ULL);
    sfx_player(g, ps, SFX_QUENCH, 1.0f);
    vec3 at = v3((float)POCKET_ANVIL_X + 0.5f, (float)POCKET_G + 1.0f, (float)POCKET_ANVIL_Z + 0.5f);
    particles_burst(g, at, v3(0.85f, 0.85f, 0.85f), 20, 2.0f, 0.07f, false);
    if (ps->fs.reforge) {
        Item *it = &s->equipped[s->weapon];
        int lv = s->level + m->bonus / 2 + (int)(q * 4.0f) + sk / 3;
        it->level = (u8)clampf((float)MAXI(it->level, lv), 1, 250);
        bool step = q >= 0.95f && it->rarity < RAR_EPIC && !it->unique;
        if (step) {
            it->rarity++;
            int slots = it->rarity >= RAR_RARE ? 2 : 1;
            for (int k = 0; k < slots; k++)
                if (it->affix[k] == AF_NONE) { h = mix64(h + 1); it->affix[k] = (u8)(1 + h % (AF_COUNT - 1)); }
        }
        item_name(it, name, sizeof name);
        snprintf(buf, sizeof buf, "REFORGED (%d%%): %s, LEVEL %d%s", (int)(q * 100.0f), name, it->level, step ? ", A STEP RARER" : "");
        upper(buf);
        game_banner(ps, buf);
        gain_xp(g, ps, 10 + 6 * m->tier + (int)(25.0f * q));
        stats_refresh(ps);
        g_tele.forged++;
        return;
    }
    Item it = { 0 };
    it.used = 1;
    it.base = ps->plan.base;
    it.material = m->material;
    int lv = s->level + m->bonus + (int)(q * 6.0f) + sk / 2 + (ps->plan.heart ? 2 : 0) - (ps->fs.flaws >= 3 ? 3 : 0);
    it.level = (u8)clampf((float)lv, 1, 250);
    bool master = q >= 0.92f && sk >= 8 && m->tier >= 4;
    it.rarity = master ? RAR_LEGENDARY : q < 0.35f ? RAR_COMMON : q < 0.55f ? RAR_MAGIC : q < 0.75f ? RAR_RARE : RAR_EPIC;
    for (int k = 0; k < 2; k++)
        if (ps->plan.ess[k]) it.affix[k] = ESS_AFFIX[ps->plan.ess[k] - 1 - FG_ESS0];
    int slots = it.rarity >= RAR_RARE ? 2 : it.rarity >= RAR_MAGIC ? 1 : 0;
    for (int k = 0; k < slots; k++)
        if (it.affix[k] == AF_NONE) {
            int a;
            do { h = mix64(h + 1); a = 1 + (int)(h % (AF_COUNT - 1)); } while (k == 1 && a == it.affix[0]);
            it.affix[k] = (u8)a;
        }
    for (int k = 0; k < 2; k++) it.power[k] = ps->plan.rune[k];
    if (master) { h = mix64(h + 7); it.hidden = (u8)(1 + h % (AF_COUNT - 1)); it.kills = AWAKEN_KILLS; } /* a masterwork wakes at once */
    int slot = -1;
    for (int i = 0; i < BAG_SIZE && slot < 0; i++) if (!s->bag[i].used) slot = i;
    if (slot >= 0) s->bag[slot] = it;
    item_name(&it, name, sizeof name);
    snprintf(buf, sizeof buf, "%s (%d%%%s): %s, LEVEL %d", master ? "A MASTERWORK" : "FORGED", (int)(q * 100.0f), ps->fs.flaws ? ", FLAWED" : "", name, it.level);
    upper(buf);
    game_banner(ps, buf);
    if (master) { sfx_ui(SFX_PICKUP_RELIC, 1.0f); flash_add(g, at, v3(1.2f, 0.9f, 0.5f), 12.0f, 0.8f); }
    gain_xp(g, ps, 20 + 12 * m->tier + (int)(50.0f * q));
    g_tele.forged++;
    if (master) g_tele.masterworks++;
}

/* ---------- smelting and salvage ---------- */

static const u8 ORE_INGOT[5] = { FG_IRON, FG_SILVER, FG_MITHRIL, FG_ADAMANT, FG_STARMETAL };
static const u8 ORE_METAL[5] = { MT_IRON, MT_SILVER, MT_MITHRIL, MT_ADAMANT, MT_STARMETAL };

bool forge_smelt(Game *g, PlayerSlot *ps, int ore) {
    Stats *s = &ps->st;
    if (!forge_here(g, ps) || ore < FG_IRON_ORE || ore > FG_STAR_ORE) return false;
    const MetalDef *m = &METALS[ORE_METAL[ore]];
    if (forge_stage(g) < (m->tier >= 3 ? 2 : 1) || forge_stage(g) < m->stage) { game_banner(ps, "IT NEEDS A HOTTER FURNACE"); return false; }
    if (s->forge[ore] < 2 || s->forge[FG_CHARCOAL] < 1 || (m->flux && s->forge[FG_FLUX] < 1)) {
        game_banner(ps, m->flux ? "TWO ORE, CHARCOAL AND FLUX" : "TWO ORE AND CHARCOAL");
        return false;
    }
    s->forge[ore] -= 2;
    s->forge[FG_CHARCOAL]--;
    if (m->flux) s->forge[FG_FLUX]--;
    forge_give(g, ps, ORE_INGOT[ore], 1);
    sfx_player(g, ps, SFX_SMELT, 1.0f);
    gain_xp(g, ps, 4 + 2 * m->tier);
    return true;
}

bool forge_salvage(Game *g, PlayerSlot *ps, int i) {
    Stats *s = &ps->st;
    if (!forge_here(g, ps) || i < 0 || i >= BAG_SIZE || !s->bag[i].used) return false;
    Item *it = &s->bag[i];
    if (it->unique) { game_banner(ps, "A LEGENDARY IS NOT BROKEN UP"); return false; }
    char buf[200], learned[96] = "";
    int mt = metal_of(it), n = 1 + MINI(2, it->level / 15);
    bool bp = !((s->blueprints >> it->base) & 1);
    s->blueprints |= 1u << it->base;
    if (bp) snprintf(learned, sizeof learned, ", THE BLUEPRINT OF THE %s", WEAPON_BASES[it->base].name);
    for (int k = 0; k < 2; k++) {
        int t = (it->power[k] >> 4) & 15, e = it->power[k] & 15;
        if (t > 0 && t < PT_COUNT && e > 0 && e < PE_COUNT) {
            bool fresh = !((s->runes_t >> t) & 1) || !((s->runes_e >> e) & 1);
            s->runes_t |= (u16)(1u << t);
            s->runes_e |= (u16)(1u << e);
            if (fresh) snprintf(learned + strlen(learned), sizeof learned - strlen(learned), ", A RUNE");
        }
    }
    s->forge[METALS[mt].ingot] = (u8)MINI(FORGE_MAX, s->forge[METALS[mt].ingot] + n);
    char name[96];
    item_name(it, name, sizeof name);
    snprintf(buf, sizeof buf, "BROKEN UP: %s - %d %s%s", name, n, FORGE_MATS[METALS[mt].ingot].name, learned);
    upper(buf);
    game_banner(ps, buf);
    memset(it, 0, sizeof *it);
    sfx_player(g, ps, SFX_SALVAGE, 1.0f);
    gain_xp(g, ps, 6);
    return true;
}

/* ---------- drops and ore ---------- */

int forge_region_ore(Game *g, int x, int y, int z, u64 h) {
    int cx = floordiv(x, CHUNK), cy = floordiv(y, CHUNK), cz = floordiv(z, CHUNK), tier;
    world_region(&g->world, cx, cy, cz, &tier);
    ChunkInfo ci;
    world_chunk_info(&g->world, cx, cy, cz, &ci);
    float r = (float)(h & 0xFFFF) / 65536.0f;
    if (ci.stratum == ST_ORRERY || ci.stratum == ST_ASTRAL || ci.stratum == ST_STORM || ci.flavor == FL_STARFROST || tier >= 9) return r < 0.5f ? FG_STAR_ORE : FG_ADAMANT_ORE;
    if (tier >= 6 || ci.stratum == ST_IRONHELL || ci.stratum == ST_MAW || ci.stratum == ST_CORE) return r < 0.5f ? FG_ADAMANT_ORE : FG_MITHRIL_ORE;
    if (tier >= 4 || ci.stratum == ST_CRYSTAL || ci.stratum == ST_LIMBO || ci.stratum == ST_FUNGAL) return r < 0.6f ? FG_MITHRIL_ORE : FG_SILVER_ORE;
    if ((ci.flavor >= FL_FROST && ci.flavor <= FL_LASTCOLD) || ci.stratum == ST_OSSUARY || (tier >= 2 && r < 0.4f)) return FG_SILVER_ORE;
    return FG_IRON_ORE;
}

bool forge_mine(Game *g, PlayerSlot *ps, int x, int y, int z) {
    bool known;
    if (chunkmap_block(&g->map, x, y, z, &known) != B_ORE || !known) return false;
    u64 key = mix64(((u64)(u32)x << 40) ^ ((u64)(u32)y << 20) ^ (u64)(u32)z ^ 0x0BE5ULL) | 1;
    if (looted_has(g, key)) {
        if (ps->banner_time <= 0) game_banner(ps, "THIS VEIN IS WORKED OUT");
        return true;
    }
    looted_mark(g, key);
    int ore = forge_region_ore(g, x, y, z, mix64(key));
    vec3 at = v3(x + 0.5f, y + 0.5f, z + 0.5f);
    particles_burst(g, at, FORGE_MATS[ore].color, 12, 3.0f, 0.05f, false);
    forge_give(g, ps, ore, (mix64(key ^ 9) & 3) == 0 ? 2 : 1);
    return true;
}

void forge_monster_drop(Game *g, const Monster *m) {
    u64 h = mix64((u64)(i64)(m->pos.x * 311.0f) ^ ((u64)(i64)(m->pos.z * 733.0f) << 22) ^ (u64)(g->time * 977.0f) ^ 0xF0E6ULL);
    float r = (float)(h & 0xFFFF) / 65536.0f, r2 = (float)((h >> 16) & 0xFFFF) / 65536.0f;
    vec3 p = v3_add(m->pos, v3(0.3f, 0.5f, 0));
    if (m->element < EL_COUNT) { /* essences of the region's element */
        float ch = m->grade == GRADE_BOSS ? 1.0f : m->elite ? 0.35f : 0.04f;
        if (r < ch) pickup_spawn(g, PK_FORGE, p, FG_ESS0 + m->element, NULL, 0, 0);
        if (m->grade == GRADE_BOSS) pickup_spawn(g, PK_FORGE, p, FG_ESS0 + m->element, NULL, 0, 0);
    }
    if (m->grade == GRADE_BOSS && r2 < 0.3f) pickup_spawn(g, PK_FORGE, p, FG_TITAN_HEART, NULL, 0, 0);
    if (m->type == MON_GOLEM && r2 < 0.6f) /* golems are half ore */
        pickup_spawn(g, PK_FORGE, p, forge_region_ore(g, (int)floorf(m->pos.x), (int)floorf(m->pos.y), (int)floorf(m->pos.z), h >> 32), NULL, 0, 0);
}

/* ---------- the FORGE tab ---------- */

enum { RR_SMITH, RR_BASE, RR_METAL, RR_ESS, RR_RUNE, RR_HEART, RR_OIL, RR_FORGE, RR_REFORGE, RR_SMELT, RR_SALVAGE, RR_HEAD, RR_MAT, RR_AWAY };
typedef struct FRow { u8 type; int a; } FRow;
#define FROWS (16 + BAG_SIZE + FG_COUNT)

static int frows(Game *g, PlayerSlot *ps, FRow *r) {
    const Stats *s = &ps->st;
    int n = 0;
    r[n++] = (FRow){ RR_SMITH, 0 };
    if (forge_here(g, ps)) {
        plan_fix(g, ps);
        r[n++] = (FRow){ RR_BASE, 0 };
        r[n++] = (FRow){ RR_METAL, 0 };
        for (int k = 0; k < ess_slots(s); k++) r[n++] = (FRow){ RR_ESS, k };
        for (int k = 0; k < rune_slots(g, s); k++) r[n++] = (FRow){ RR_RUNE, k };
        if (s->forge[FG_TITAN_HEART]) r[n++] = (FRow){ RR_HEART, 0 };
        if (s->forge[FG_QUENCH_OIL]) r[n++] = (FRow){ RR_OIL, 0 };
        r[n++] = (FRow){ RR_FORGE, 0 };
        if (s->equipped[s->weapon].used) r[n++] = (FRow){ RR_REFORGE, 0 };
        for (int o = FG_IRON_ORE; o <= FG_STAR_ORE; o++)
            if (s->forge[o] >= 2) r[n++] = (FRow){ RR_SMELT, o };
        for (int i = 0; i < BAG_SIZE; i++)
            if (s->bag[i].used && !s->bag[i].unique) r[n++] = (FRow){ RR_SALVAGE, i };
    } else r[n++] = (FRow){ RR_AWAY, 0 };
    r[n++] = (FRow){ RR_HEAD, 0 };
    for (int m = 0; m < FG_COUNT; m++)
        if (s->forge[m]) r[n++] = (FRow){ RR_MAT, m };
    return n;
}

/* the known rune combinations, in order: trigger * 16 + effect */
static int rune_list(const Stats *s, u8 *out) {
    int n = 0;
    for (int t = 1; t < PT_COUNT; t++)
        for (int e = 1; e < PE_COUNT; e++)
            if (((s->runes_t >> t) & 1) && ((s->runes_e >> e) & 1)) out[n++] = (u8)(t << 4 | e);
    return n;
}

static void cycle(Game *g, PlayerSlot *ps, const FRow *row, int dir) {
    Stats *s = &ps->st;
    switch (row->type) {
    case RR_BASE:
        for (int k = 1; k <= WB_COUNT; k++) {
            int b = (ps->plan.base + dir * k + WB_COUNT * 4) % WB_COUNT;
            if ((s->blueprints >> b) & 1) { ps->plan.base = (u8)b; break; }
        }
        break;
    case RR_METAL:
        for (int k = 1; k <= MT_COUNT; k++) {
            int m = (ps->plan.metal + dir * k + MT_COUNT * 4) % MT_COUNT;
            if (metal_open(g, s, m)) { ps->plan.metal = (u8)m; break; }
        }
        break;
    case RR_ESS: { /* none, then the essences held */
        int cur = ps->plan.ess[row->a], v0 = cur ? cur - 1 - FG_ESS0 + 1 : 0;
        for (int k = 1; k <= FG_ESS_COUNT + 1; k++) {
            int v = ((v0 + dir * k) % (FG_ESS_COUNT + 1) + FG_ESS_COUNT + 1) % (FG_ESS_COUNT + 1); /* 0 none, 1.. essences */
            int m = FG_ESS0 + v - 1;
            if (v == 0 || s->forge[m]) { ps->plan.ess[row->a] = (u8)(v ? m + 1 : 0); break; }
        }
        break;
    }
    case RR_RUNE: {
        u8 list[PT_COUNT * PE_COUNT];
        int n = rune_list(s, list), cur = -1;
        for (int i = 0; i < n; i++) if (list[i] == ps->plan.rune[row->a]) cur = i;
        int v = ((cur + 1) + dir + (n + 1) * 4) % (n + 1); /* 0 none */
        ps->plan.rune[row->a] = v ? list[v - 1] : 0;
        break;
    }
    case RR_HEART: ps->plan.heart = !ps->plan.heart; break;
    case RR_OIL: ps->plan.oil = !ps->plan.oil; break;
    default: break;
    }
}

static void act(Game *g, PlayerSlot *ps, const FRow *row) {
    switch (row->type) {
    case RR_FORGE: forge_start(g, ps, false); break;
    case RR_REFORGE: forge_start(g, ps, true); break;
    case RR_SMELT: forge_smelt(g, ps, row->a); break;
    case RR_SALVAGE: forge_salvage(g, ps, row->a); break;
    case RR_BASE: case RR_METAL: case RR_ESS: case RR_RUNE: case RR_HEART: case RR_OIL: cycle(g, ps, row, 1); break;
    default: break;
    }
}

void forge_nav(Game *g, PlayerSlot *ps, int nav) {
    if (ps->fs.on) {
        if (nav == NAV_BUY || nav == NAV_UP) forge_strike(g, ps);
        else forge_bellows(g, ps);
        return;
    }
    FRow rows[FROWS];
    int n = frows(g, ps, rows);
    if (nav == NAV_UP) ps->forge_sel = (ps->forge_sel + n - 1) % n;
    else if (nav == NAV_DOWN) ps->forge_sel = (ps->forge_sel + 1) % n;
    else if (ps->forge_sel < n && (nav == NAV_LEFT || nav == NAV_RIGHT)) cycle(g, ps, &rows[ps->forge_sel], nav == NAV_LEFT ? -1 : 1);
    else if (ps->forge_sel < n && nav == NAV_BUY) act(g, ps, &rows[ps->forge_sel]);
    if (ps->forge_sel >= n) ps->forge_sel = n - 1;
}

#define ROW 11
static int rows_fit(int H, int top) { return MAXI(3, (H - top - 34 - 18) / ROW); }
static int first_row(const PlayerSlot *ps, int n, int fit) { return MAXI(0, MINI(ps->forge_sel - fit / 2, n - fit)); }
#define LIST_Y(top) ((top) + 34)

void forge_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y) {
    if (ps->fs.on) { /* left half: the bellows, right half: strike */
        if (x < W * 0.5f) forge_bellows(g, ps);
        else forge_strike(g, ps);
        return;
    }
    FRow rows[FROWS];
    int n = frows(g, ps, rows);
    int fit = rows_fit(H, top), f = first_row(ps, n, fit);
    int row = (int)floorf((y - LIST_Y(top)) / ROW);
    if (row < 0 || row >= fit || f + row >= n) return;
    row += f;
    if (row == ps->forge_sel) act(g, ps, &rows[row]);
    else ps->forge_sel = row;
}

static void draw_session(Game *g, PlayerSlot *ps, int W, int H, int top) {
    float lo, hi;
    forge_window(ps, &lo, &hi);
    int x = MAXI(10, W / 2 - 160), w = MINI(320, W - 20), y = top + 70, bh = 18;
    float c = (lo + hi) * 0.5f, ww = hi - lo;
    batch2d_begin(W, H);
    batch2d_rect((float)x - 2, (float)y - 2, (float)w + 4, (float)bh + 4, 0.15f, 0.12f, 0.1f, 1.0f);
    for (int i = 0; i < w; i++) { /* cold blue to white hot */
        float t = (float)i / (float)w;
        batch2d_rect((float)(x + i), (float)y, 1, (float)bh, 0.25f + 0.75f * t, 0.15f + 0.6f * t * t, 0.35f * (1.0f - t) + 0.4f * t * t * t, 0.9f);
    }
    batch2d_rect(x + lo * w, (float)y - 6, (hi - lo) * w, 4, 0.9f, 0.8f, 0.3f, 1.0f);                   /* the window */
    batch2d_rect(x + (c - ww * 0.25f) * w, (float)y - 6, ww * 0.5f * w, 4, 1.0f, 1.0f, 0.7f, 1.0f); /* perfect */
    float hx = x + ps->fs.heat * w;
    batch2d_rect(hx - 1, (float)y - 3, 3, (float)bh + 6, 1.0f, 1.0f, 1.0f, 1.0f);
    if (ps->fs.flash > 0) batch2d_rect(0, (float)top + 30, (float)W, (float)H - top - 40, 1.0f, 0.8f, 0.4f, 0.12f * ps->fs.flash);
    batch2d_flush();
    font_begin(W, H);
    char buf[160];
    snprintf(buf, sizeof buf, "%s %s OF %s", ps->fs.reforge ? "REFORGING THE" : "FORGING A",
             ps->fs.reforge ? WEAPON_BASES[ps->st.equipped[ps->st.weapon].base].name : WEAPON_BASES[ps->plan.base].name, METALS[session_metal(ps)].name);
    upper(buf);
    font_text((W - font_text_width(buf)) / 2, top + 30, buf, 1.0f, 0.8f, 0.45f);
    snprintf(buf, sizeof buf, "STRIKE %d OF %d   SCORE %d   FLAWS %d", MINI(ps->fs.done + 1, ps->fs.need), ps->fs.need, ps->fs.score, ps->fs.flaws);
    font_text((W - font_text_width(buf)) / 2, top + 46, buf, 0.85f, 0.8f, 0.7f);
    float k = ps->fs.last == 3 ? 1.0f : 0.8f;
    font_text((W - font_text_width(ps->fs.msg)) / 2, y + bh + 14, ps->fs.msg, k, ps->fs.last == 0 ? 0.4f : k * 0.9f, ps->fs.last == 0 ? 0.3f : 0.5f);
    font_text((W - font_text_width("X / LEFT: BELLOWS     ENTER / UP: STRIKE")) / 2, y + bh + 30, "X / LEFT: BELLOWS     ENTER / UP: STRIKE", 0.6f, 0.55f, 0.5f);
    font_end();
    (void)g;
}

void forge_draw(Game *g, PlayerSlot *ps, int W, int H, int top) {
    if (ps->fs.on) { draw_session(g, ps, W, H, top); return; }
    const Stats *s = &ps->st;
    FRow rows[FROWS];
    int n = frows(g, ps, rows);
    if (ps->forge_sel >= n) ps->forge_sel = n - 1;
    int fit = rows_fit(H, top), f = first_row(ps, n, fit);
    int x = MAXI(6, (W - 74 * 6) / 2);
    char buf[240], t[160];
    batch2d_begin(W, H);
    for (int i = f; i < n && i < f + fit; i++) {
        if (rows[i].type == RR_HEAD || rows[i].type == RR_SMITH) continue;
        float ry = (float)(LIST_Y(top) + (i - f) * ROW) - 2.0f;
        bool sel = i == ps->forge_sel;
        batch2d_rect((float)x - 3, ry, (float)(W - 2 * x + 6), ROW - 1, sel ? 0.28f : 0.08f, sel ? 0.16f : 0.06f, sel ? 0.08f : 0.05f, 0.85f);
    }
    batch2d_flush();
    font_begin(W, H);
    int sk = smith(s);
    snprintf(buf, sizeof buf, "SMITHING %d%s   GOLD %d", sk, sk < SMITH_MAX ? "" : " (MASTER)", s->gold);
    font_text(x, top + 6, buf, 1.0f, 0.7f, 0.4f);
    if (sk < SMITH_MAX) {
        snprintf(buf, sizeof buf, "%d / %d XP TO THE NEXT", s->smith_xp, smith_xp_for(sk + 1));
        font_text(x, top + 18, buf, 0.65f, 0.6f, 0.5f);
    } else font_text(x, top + 18, "A MASTER: MASTERWORKS OF ADAMANT, MOONSILVER AND STARMETAL", 0.65f, 0.6f, 0.5f);
    if (f > 0) font_text(W - x - 6, LIST_Y(top) - 10, "^", 0.8f, 0.6f, 0.4f);
    if (f + fit < n) font_text(W - x - 6, LIST_Y(top) + fit * ROW - 2, "V", 0.8f, 0.6f, 0.4f);
    int maxc = (W - 2 * x) / 6;
    for (int i = f; i < n && i < f + fit; i++) {
        const FRow *row = &rows[i];
        int ry = LIST_Y(top) + (i - f) * ROW;
        char rbuf[48] = "";
        float cr = 0.9f, cg = 0.85f, cb = 0.75f;
        switch (row->type) {
        case RR_SMITH: {
            int known = __builtin_popcount(s->blueprints ? s->blueprints : BLUEPRINTS_START), runes = 0;
            u8 tmp[PT_COUNT * PE_COUNT];
            runes = rune_list(s, tmp);
            snprintf(buf, sizeof buf, "BLUEPRINTS %d OF %d   RUNES %d   FORGE STAGE %d", known, WB_COUNT, runes, forge_stage(g));
            cr = 0.75f; cg = 0.7f; cb = 0.6f;
            break;
        }
        case RR_AWAY: snprintf(buf, sizeof buf, forge_stage(g) ? "THE ANVIL IS IN THE SANCTUM'S FORGE" : "BUILD THE FORGE IN THE SANCTUM (ISLAND STAGE 2)"); cr = cg = cb = 0.6f; break;
        case RR_BASE: snprintf(buf, sizeof buf, "< BLUEPRINT: %s >", WEAPON_BASES[ps->plan.base].name); snprintf(rbuf, sizeof rbuf, "%d INGOTS", forge_ingots(ps->plan.base)); break;
        case RR_METAL: {
            const MetalDef *m = &METALS[ps->plan.metal];
            snprintf(buf, sizeof buf, "< METAL: %s (+%d LEVELS%s) >", m->name, m->bonus, m->flux ? ", FLUX" : "");
            snprintf(rbuf, sizeof rbuf, "HAVE %d", s->forge[m->ingot]);
            break;
        }
        case RR_ESS: {
            int e = ps->plan.ess[row->a];
            snprintf(buf, sizeof buf, "< ESSENCE %d: %s >", row->a + 1, e ? FORGE_MATS[e - 1].name : "none");
            if (e) snprintf(rbuf, sizeof rbuf, "HAVE %d", s->forge[e - 1]);
            cr = 0.85f; cg = 0.75f; cb = 0.95f;
            break;
        }
        case RR_RUNE: {
            u8 r = ps->plan.rune[row->a];
            if (r) arms_power_text(r, t, sizeof t);
            else snprintf(t, sizeof t, "%s", rune_list(s, (u8[PT_COUNT * PE_COUNT]){ 0 }) ? "none" : "none known: break up weapons with powers");
            snprintf(buf, sizeof buf, "< RUNE %d: %s >", row->a + 1, t);
            cr = 0.6f; cg = 0.75f; cb = 1.0f;
            break;
        }
        case RR_HEART: snprintf(buf, sizeof buf, "< A TITAN HEART: %s >", ps->plan.heart ? "YES (+10% QUALITY, +2 LEVELS)" : "NO"); break;
        case RR_OIL: snprintf(buf, sizeof buf, "< QUENCHING OIL: %s >", ps->plan.oil ? "YES (+6% QUALITY)" : "NO"); break;
        case RR_FORGE: case RR_REFORGE: {
            bool re = row->type == RR_REFORGE;
            Cost c = plan_cost(g, ps, re);
            char miss[160];
            missing(s, &c, miss, sizeof miss);
            cost_text(&c, t, sizeof t);
            if (re) {
                char name[64];
                item_name(&s->equipped[s->weapon], name, sizeof name);
                snprintf(buf, sizeof buf, "REFORGE %s: %s", name, miss[0] ? miss : t);
            } else snprintf(buf, sizeof buf, "FORGE IT: %s", miss[0] ? miss : t);
            snprintf(rbuf, sizeof rbuf, "%s", miss[0] ? "MISSING" : "READY");
            cr = 1.0f; cg = miss[0] ? 0.55f : 0.8f; cb = 0.4f;
            break;
        }
        case RR_SMELT: {
            const MetalDef *m = &METALS[ORE_METAL[row->a]];
            snprintf(buf, sizeof buf, "SMELT 2 %s + CHARCOAL%s -> %s", FORGE_MATS[row->a].name, m->flux ? " + FLUX" : "", FORGE_MATS[ORE_INGOT[row->a]].name);
            cr = 0.85f; cg = 0.7f; cb = 0.55f;
            break;
        }
        case RR_SALVAGE: {
            char name[64];
            item_name(&s->bag[row->a], name, sizeof name);
            bool bp = !((s->blueprints >> s->bag[row->a].base) & 1);
            snprintf(buf, sizeof buf, "BREAK UP %s", name);
            snprintf(rbuf, sizeof rbuf, "%s", bp ? "NEW BLUEPRINT" : s->bag[row->a].power[0] ? "RUNES" : "");
            cr = 0.75f; cg = 0.72f; cb = 0.68f;
            break;
        }
        case RR_HEAD: snprintf(buf, sizeof buf, "MATERIALS"); cr = 1.0f; cg = 0.7f; cb = 0.4f; break;
        case RR_MAT: {
            static const char *KIND[5] = { "ore", "ingot", "essence", "heart", "bought" };
            snprintf(buf, sizeof buf, "%s (%s)", FORGE_MATS[row->a].name, KIND[FORGE_MATS[row->a].kind]);
            snprintf(rbuf, sizeof rbuf, "X%d", s->forge[row->a]);
            vec3 c = FORGE_MATS[row->a].color;
            cr = 0.5f + 0.5f * c.x; cg = 0.5f + 0.5f * c.y; cb = 0.5f + 0.5f * c.z;
            break;
        }
        }
        upper(buf);
        int room = maxc - (int)strlen(rbuf) - 2;
        if ((int)strlen(buf) > room) buf[MAXI(0, room)] = 0;
        font_text(x, ry, buf, cr, cg, cb);
        if (rbuf[0]) font_text(W - x - font_text_width(rbuf), ry, rbuf, 0.95f, 0.75f, 0.4f);
    }
    font_text(x, H - 14, "UP/DOWN CHOOSE   LEFT/RIGHT CHANGE   ENTER FORGE, SMELT OR BREAK UP", 0.6f, 0.55f, 0.45f);
    font_end();
}

/* ---------- how materials look on the ground ---------- */

void forge_draw_mat(int m, vec3 c, float yaw, vec3 l) {
    if (m < 0 || m >= FG_COUNT) return;
    const ForgeMatDef *d = &FORGE_MATS[m];
    vec3 col = v3(d->color.x * l.x, d->color.y * l.y, d->color.z * l.z);
    switch (d->kind) {
    case 0: /* ore: rock with a shine */
        batch3d_octa(v3_add(c, v3(0, 0.1f, 0)), 0.12f, 0.1f, yaw, v3(0.4f * l.x, 0.38f * l.y, 0.36f * l.z), false);
        batch3d_box(v3_add(c, v3(0.03f, 0.14f, 0.0f)), v3(0.05f, 0.04f, 0.05f), yaw, col, m >= FG_MITHRIL_ORE);
        break;
    case 1: /* an ingot */
        batch3d_box(v3_add(c, v3(0, 0.05f, 0)), v3(0.14f, 0.05f, 0.07f), yaw, col, m == FG_STARMETAL || m == FG_MOONSILVER);
        break;
    case 2: /* an essence: a glowing orb in a cage */
        batch3d_octa(v3_add(c, v3(0, 0.18f, 0)), 0.07f, 0.09f, yaw * 2.0f, d->color, true);
        batch3d_box(v3_add(c, v3(0, 0.18f, 0)), v3(0.08f, 0.1f, 0.08f), yaw, v3(0.3f * l.x, 0.25f * l.y, 0.2f * l.z), false);
        break;
    case 3: /* a titan heart */
        batch3d_octa(v3_add(c, v3(0, 0.2f, 0)), 0.15f, 0.18f, yaw, d->color, true);
        break;
    default:
        batch3d_box(v3_add(c, v3(0, 0.08f, 0)), v3(0.08f, 0.08f, 0.08f), yaw, col, false);
        break;
    }
}
