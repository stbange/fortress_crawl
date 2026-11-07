#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

#include <stdarg.h>

static void upper(char *s) {
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}

/* The SHOP tab: what gold buys. At a shrine: potions, a rank for the active spell, new
 * affixes for the weapon in hand, more of a relic you carry. At the Sanctum library's
 * shrine also the realm's comforts: the bath, the feast, the library's daily scroll.
 * Anywhere: scrap every weapon in the bag that is worse than the one of its class you
 * wear. */


typedef struct ShopItem {
    int kind, arg, price;
    bool ok;            /* can be bought now */
    char label[72], why[40];
} ShopItem;

#define SHOP_MAX 24
#define LIBRARY_EVERY 1200.0f /* seconds between the library's scrolls */

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

bool shop_open_here(Game *g, const PlayerSlot *ps) { return shrine_here(g, ps) >= 0; }

static float score(const Item *it) { return item_damage_mult(it) / fmaxf(0.3f, item_speed_mult(it)); }

static int scrap_value(const Item *it) { return (int)((4 + it->level * 2) * (1 + it->rarity * 2)); }

static void add(ShopItem *out, int *n, int kind, int arg, int price, bool ok, const char *why, const char *fmt, ...) {
    if (*n >= SHOP_MAX) return;
    ShopItem *it = &out[(*n)++];
    it->kind = kind; it->arg = arg; it->price = price; it->ok = ok;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(it->label, sizeof it->label, fmt, ap);
    va_end(ap);
    snprintf(it->why, sizeof it->why, "%s", why ? why : "");
}

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
    if (shrine_here(g, ps) < 0) return n;
    bool home = realm_has(ps->pl.pos);
    char why[40];
    add(out, &n, SH_POTION_HP, 0, 40, s->potions_hp < POTION_HP_MAX, s->potions_hp < POTION_HP_MAX ? NULL : "YOU CARRY ALL YOU CAN",
        "HEALTH POTION (%d/%d)", s->potions_hp, POTION_HP_MAX);
    add(out, &n, SH_POTION_MP, 0, 30, s->potions_mp < POTION_MP_MAX, s->potions_mp < POTION_MP_MAX ? NULL : "YOU CARRY ALL YOU CAN",
        "MANA POTION (%d/%d)", s->potions_mp, POTION_MP_MAX);
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
    if (home) {
        bool bath = s->sanctum & SANCTUM_BATH, feast = s->sanctum & SANCTUM_FEAST, lib = s->sanctum & SANCTUM_LIBRARY;
        add(out, &n, SH_BATH, 0, 500, !bath, bath ? "DONE" : NULL, "RESTORE THE BATH HALL");
        add(out, &n, SH_FEAST, 0, 1000, !feast, feast ? "DONE" : NULL, "STOCK THE DINING HALL");
        add(out, &n, SH_LIBRARY, 0, 1500, !lib, lib ? "DONE" : NULL, "FILL THE LIBRARY");
        if (lib) {
            snprintf(why, sizeof why, "NEXT IN %d MIN", (int)ceilf(s->library_t / 60.0f));
            add(out, &n, SH_SCROLL, 0, 0, s->library_t <= 0, s->library_t <= 0 ? NULL : why, "TODAY'S SCROLL FROM THE LIBRARY");
        }
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
    if (it->price > 0) s->gold -= it->price;
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
    case SH_BATH: s->sanctum |= SANCTUM_BATH; snprintf(buf, sizeof buf, "THE BATH HALL STEAMS AGAIN"); sfx_ui(SFX_REALM_BATH, 0.8f); break;
    case SH_FEAST: s->sanctum |= SANCTUM_FEAST; snprintf(buf, sizeof buf, "THE TABLE IS LAID"); sfx_ui(SFX_REALM_FEAST, 0.8f); break;
    case SH_LIBRARY: s->sanctum |= SANCTUM_LIBRARY; snprintf(buf, sizeof buf, "THE SHELVES FILL WITH SCROLLS"); sfx_ui(SFX_PICKUP_SCROLL, 1.0f); break;
    case SH_SCROLL: {
        int id = (int)(mix64((u64)(g->time * 1000.0f) + 77) % SPELL_COUNT);
        int r = spell_learn(ps, id);
        SpellInfo si;
        spell_info(id, &si);
        s->library_t = LIBRARY_EVERY;
        snprintf(buf, sizeof buf, "FROM THE LIBRARY: %s (RANK %d)", si.name, r);
        sfx_ui(SFX_PICKUP_SCROLL, 1.0f);
        break;
    }
    default: return;
    }
    upper(buf);
    game_banner(ps, buf);
    g->autosave_timer = 1e9f;
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

void shop_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y) {
    (void)W; (void)H; (void)x;
    ShopItem items[SHOP_MAX];
    int n = shop_items(g, ps, items);
    int row = (int)floorf((y - (top + 22)) / ROW);
    if (row < 0 || row >= n) return;
    if (row == ps->shop_sel) buy(g, ps, &items[row]);
    else ps->shop_sel = row;
}

void shop_draw(Game *g, PlayerSlot *ps, int W, int H, int top) {
    ShopItem items[SHOP_MAX];
    int n = shop_items(g, ps, items);
    if (ps->shop_sel >= n) ps->shop_sel = n - 1;
    int x = MAXI(6, (W - 74 * 6) / 2), y = top + 6;
    char buf[96];
    batch2d_begin(W, H);
    for (int i = 0; i < n; i++) {
        float ry = (float)(top + 22 + i * ROW) - 2.0f;
        bool sel = i == ps->shop_sel;
        batch2d_rect((float)x - 3, ry, (float)(W - 2 * x + 6), ROW - 1, sel ? 0.25f : 0.08f, sel ? 0.18f : 0.06f, sel ? 0.1f : 0.05f, 0.85f);
    }
    batch2d_flush();
    font_begin(W, H);
    snprintf(buf, sizeof buf, "GOLD %d%s", ps->st.gold, n > 1 ? "" : "    (A SHRINE TRADES MORE)");
    font_text(x, y, buf, 1.0f, 0.82f, 0.35f);
    for (int i = 0; i < n; i++) {
        const ShopItem *it = &items[i];
        int ry = top + 22 + i * ROW;
        float k = it->ok ? 1.0f : 0.5f;
        snprintf(buf, sizeof buf, "%s", it->label);
        upper(buf);
        int maxc = (W - 2 * x) / 6 - 12;
        if ((int)strlen(buf) > maxc) buf[MAXI(0, maxc)] = 0;
        font_text(x, ry, buf, 0.9f * k, 0.85f * k, 0.75f * k);
        if (it->ok && it->price) snprintf(buf, sizeof buf, it->price < 0 ? "+%d" : "%d", abs(it->price));
        else snprintf(buf, sizeof buf, "%s", it->ok ? "FREE" : it->why);
        font_text(W - x - font_text_width(buf), ry, buf, it->price > ps->st.gold && it->ok ? 1.0f : 0.95f * k,
                  it->price > ps->st.gold && it->ok ? 0.4f : 0.8f * k, 0.35f * k);
    }
    font_text(x, H - 14, "UP/DOWN CHOOSE   ENTER BUY", 0.6f, 0.55f, 0.45f);
    font_end();
}
