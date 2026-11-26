#include "game/game.h"

#include <string.h>

#include "render/batch.h"

/* What weapons look like: one model per weapon base, drawn the same in your hands, in
 * other players' hands and on the ground. The rarity picks the materials (iron, bright
 * steel, gilded, blackened with a glowing edge); magical weapons glow in the colour of
 * their affix and give off motes (embers, frost, sparks, venom drops, a blood mist) and
 * burst in that element where they hit. */

/* ---------- how a weapon is made: its style ----------
 * Every weapon's look comes from three things. Its tier (rarity) sets the vocabulary: plain
 * iron fittings for common weapons; brighter steel, finer fittings and a rune line for magic;
 * gilding, ornate guards, gem pommels and two fullers for rare; blackened steel, thorns,
 * serrations and glowing runes for epic; starmetal, wings and crowns for legendary. Its look
 * byte (rolled when it was made: dropped, forged, a world's legendary) picks within that
 * vocabulary, so two weapons of one tier differ. Its affixes add the rest: an element colours
 * the steel and grows its overlay (frost: blue steel, white edges, ice crystals, icicles; fire:
 * heat-darkened steel and flames; lightning: copper coils; venom: green steel and drips; the
 * vampiric: crimson steel and thorns), swift makes it slimmer, heavy bulkier and studded,
 * keen gives it a longer point. */

enum { GD_STRAIGHT, GD_DROOP, GD_UPSWEPT, GD_DISC, GD_THORNS, GD_WINGS };
enum { PM_WHEEL, PM_PEAR, PM_GEM, PM_SPIKE, PM_CROWN };
enum { BF_PLAIN, BF_BROAD, BF_WAVED, BF_SERRATED, BF_HOOKED };
enum { OV_NONE, OV_FROST, OV_FIRE, OV_SHOCK, OV_VENOM, OV_BLOOD };

typedef struct WStyle {
    u8 guard, pommel, form; /* GD_*, PM_*, BF_* */
    u8 fullers;             /* grooves down each flat: 1 or 2 */
    u8 inlay;               /* 0 none, 1 an inlaid line, 2 glowing runes */
    u8 overlay;             /* OV_* from the element */
    u8 tips;                /* bows and staves: 0 plain .. 4 the most ornate (by tier) */
    bool studs, gilded;     /* heavy: rivets; rare: gold fittings */
    float bulk, keen;       /* width/thickness factor; point length factor */
    u32 h;                  /* the look, hashed, for the small choices */
} WStyle;

static u32 style_hash(const Item *it) {
    u64 x = it->look ? (u64)it->look * 0x9E3779B97F4A7C15ULL
                     : ((u64)it->base << 40 | (u64)it->rarity << 32 | (u64)it->level << 24 | (u64)it->affix[0] << 16 | (u64)it->affix[1] << 8 |
                        (u64)it->material) ^ ((u64)it->unique << 48);
    return (u32)(mix64(x ^ 0x57A1EULL) >> 16);
}

static WStyle weapon_style(const Item *it) {
    static const u8 GUARD[RAR_COUNT][4] = { { GD_STRAIGHT, GD_STRAIGHT, GD_DROOP, GD_STRAIGHT }, { GD_DROOP, GD_STRAIGHT, GD_UPSWEPT, GD_DROOP },
                                            { GD_UPSWEPT, GD_DISC, GD_DROOP, GD_UPSWEPT },        { GD_THORNS, GD_THORNS, GD_UPSWEPT, GD_DISC },
                                            { GD_WINGS, GD_WINGS, GD_UPSWEPT, GD_THORNS } };
    static const u8 POMMEL[RAR_COUNT][4] = { { PM_WHEEL, PM_WHEEL, PM_PEAR, PM_WHEEL }, { PM_PEAR, PM_WHEEL, PM_PEAR, PM_GEM },
                                             { PM_GEM, PM_PEAR, PM_GEM, PM_WHEEL },     { PM_SPIKE, PM_GEM, PM_SPIKE, PM_SPIKE },
                                             { PM_CROWN, PM_CROWN, PM_GEM, PM_CROWN } };
    static const u8 FORM[RAR_COUNT][4] = { { BF_PLAIN, BF_PLAIN, BF_PLAIN, BF_BROAD }, { BF_PLAIN, BF_BROAD, BF_PLAIN, BF_HOOKED },
                                           { BF_BROAD, BF_PLAIN, BF_WAVED, BF_PLAIN },   { BF_SERRATED, BF_WAVED, BF_HOOKED, BF_SERRATED },
                                           { BF_BROAD, BF_WAVED, BF_PLAIN, BF_BROAD } };
    int r = it->rarity < RAR_COUNT ? it->rarity : 0;
    WStyle S = { 0 };
    S.h = style_hash(it);
    S.guard = GUARD[r][S.h & 3];
    S.pommel = POMMEL[r][(S.h >> 2) & 3];
    S.form = FORM[r][(S.h >> 4) & 3];
    S.fullers = r == RAR_RARE || r == RAR_LEGENDARY ? 2 : 1;
    S.inlay = r == RAR_COMMON ? 0 : r <= RAR_RARE ? 1 : 2;
    S.tips = (u8)r;
    S.gilded = r == RAR_RARE;
    S.bulk = 1.0f;
    S.keen = 1.0f;
    int el = item_affix_element(it);
    S.overlay = item_has_affix(it, AF_VAMPIRIC) ? OV_BLOOD : el == EL_FROST ? OV_FROST : el == EL_FIRE ? OV_FIRE : el == EL_LIGHTNING ? OV_SHOCK
              : el == EL_POISON ? OV_VENOM : OV_NONE;
    if (S.overlay == OV_VENOM && S.form == BF_PLAIN) S.form = BF_SERRATED; /* venom wants teeth to carry it */
    if (S.overlay == OV_BLOOD && r >= RAR_RARE) S.guard = GD_THORNS;
    if (item_has_affix(it, AF_SWIFT)) S.bulk = 0.86f, S.keen = 1.12f;
    if (item_has_affix(it, AF_HEAVY)) S.bulk = 1.2f, S.studs = true;
    if (item_has_affix(it, AF_KEEN)) S.keen = 1.25f;
    return S;
}

/* How a weapon is made, in a line for the inventory: "WAVED BLADE, SWEPT GUARD, GEM POMMEL - ICE GROWS ON IT". */
void weapon_style_text(const Item *it, char *out, int n) {
    static const char *GUARD[] = { "STRAIGHT GUARD", "DROOPING GUARD", "SWEPT GUARD", "DISC GUARD", "THORNED GUARD", "WINGED GUARD" };
    static const char *POMMEL[] = { "WHEEL POMMEL", "PEAR POMMEL", "GEM POMMEL", "SPIKED POMMEL", "CROWNED POMMEL" };
    static const char *FORM[] = { "STRAIGHT BLADE", "BROAD BLADE", "WAVED BLADE", "SERRATED BLADE", "CLIPPED BLADE" };
    static const char *TIPS[] = { "PLAIN TIPS", "HORN TIPS", "GILDED CURLS", "BLADED TIPS", "WINGED TIPS" };
    static const char *HEAD[] = { "TWO PRONGS", "A RING ABOUT THE GEM", "A CRESCENT", "A CLAW", "A HALO" };
    static const char *OVER[] = { "", "ICE GROWS ON IT", "IT BURNS", "COPPER-WOUND, IT CRACKLES", "VENOM BEADS ON IT", "THORNS GROW FROM IT" };
    WStyle S = weapon_style(it);
    int base = it->base, wc = WEAPON_BASES[base].wclass;
    char a[96] = "";
    if (base == WB_DAGGER || base == WB_SWORD || base == WB_GREATSWORD)
        snprintf(a, sizeof a, "%s, %s, %s", FORM[S.form], GUARD[S.guard], POMMEL[S.pommel]);
    else if (base == WB_RAPIER) snprintf(a, sizeof a, "%s, %s", S.form == BF_BROAD ? "BROAD BLADE" : "STRAIGHT BLADE", POMMEL[S.pommel]);
    else if (base == WB_AXE) {
        bool dbl = it->rarity == RAR_LEGENDARY || (it->rarity == RAR_EPIC && ((S.h >> 9) & 1));
        bool cres = S.form == BF_WAVED || it->rarity == RAR_LEGENDARY;
        snprintf(a, sizeof a, "%s%s", dbl ? "DOUBLE " : "", cres ? "CRESCENT BIT" : S.form == BF_HOOKED && !dbl ? "BEARDED BIT, BACK SPIKE" : "BEARDED BIT");
    } else if (base == WB_MACE)
        snprintf(a, sizeof a, "%d FLANGES%s", it->rarity == RAR_LEGENDARY ? 8 : it->rarity == RAR_RARE ? 7 : 6, it->rarity >= RAR_EPIC ? ", SPIKED" : "");
    else if (base == WB_SPEAR) snprintf(a, sizeof a, "%s", S.form == BF_WAVED ? "FLAME-BLADED HEAD" : S.form == BF_BROAD ? "BROAD LEAF HEAD" : S.form == BF_SERRATED ? "SERRATED HEAD" : "LEAF HEAD");
    else if (wc == WPN_BOW && base != WB_CROSSBOW && base < WB_CLASSIC) snprintf(a, sizeof a, "%s", TIPS[S.tips]);
    else if (base == WB_STAFF) snprintf(a, sizeof a, "HEAD: %s", HEAD[S.tips]);
    const char *o = OVER[S.overlay];
    const char *bulk = S.studs ? "STUDDED" : S.bulk < 1.0f ? "SLENDER" : "";
    snprintf(out, (size_t)n, "%s%s%s%s%s", a, a[0] && bulk[0] ? ", " : "", bulk, (a[0] || bulk[0]) && o[0] ? " - " : "", o);
}

/* ---------- materials ---------- */

typedef struct WLook {
    vec3 metal, edge, wood, grip, accent; /* lit */
    vec3 glow;                            /* emissive, already pulsing */
    float glow_amt;                       /* 0 for common weapons */
    int element;                          /* EL_* of the affix, EL_NONE */
    bool vampiric;
    int rarity;
    WStyle st;
    vec3 wrap;                            /* the cord on the grip */
    vec3 ice;                             /* the overlay's own colour (ice, copper, ...) lit */
} WLook;

static vec3 mulv(vec3 a, vec3 b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }

static float hashf(float x) {
    float s = sinf(x * 12.9898f + 78.233f) * 43758.5453f;
    return s - floorf(s);
}

vec3 weapon_glow_color(const Item *it) {
    if (item_has_affix(it, AF_VAMPIRIC)) return v3(0.95f, 0.1f, 0.18f);
    int el = item_affix_element(it);
    if (el != EL_NONE) return element_color(el);
    if (item_has_affix(it, AF_SWIFT)) return v3(0.6f, 1.0f, 0.75f);
    if (item_has_affix(it, AF_HEAVY)) return v3(1.0f, 0.55f, 0.25f);
    return rarity_color(it->rarity);
}

static WLook look_of(const Item *it, vec3 light, float time) {
    /* common iron, magic steel, rare gilded, epic blackened, legendary white starmetal */
    static const vec3 METAL[RAR_COUNT] = { { 0.5f, 0.5f, 0.53f }, { 0.74f, 0.77f, 0.85f }, { 0.8f, 0.8f, 0.82f }, { 0.3f, 0.28f, 0.36f },
                                           { 0.92f, 0.95f, 0.98f } };
    static const vec3 ACCENT[RAR_COUNT] = { { 0.42f, 0.32f, 0.2f }, { 0.62f, 0.68f, 0.8f }, { 1.0f, 0.78f, 0.3f }, { 0.95f, 0.6f, 0.22f },
                                            { 0.4f, 1.0f, 0.7f } };
    static const vec3 GRIP[RAR_COUNT] = { { 0.3f, 0.19f, 0.1f }, { 0.22f, 0.2f, 0.32f }, { 0.45f, 0.1f, 0.08f }, { 0.12f, 0.1f, 0.13f },
                                          { 0.1f, 0.3f, 0.22f } };
    /* the cord on the grip: by tier, a few to choose from */
    static const vec3 WRAP[RAR_COUNT][2] = { { { 0.2f, 0.12f, 0.06f }, { 0.16f, 0.14f, 0.12f } }, { { 0.18f, 0.2f, 0.42f }, { 0.36f, 0.14f, 0.12f } },
                                             { { 0.8f, 0.6f, 0.2f }, { 0.5f, 0.08f, 0.1f } },     { { 0.5f, 0.1f, 0.08f }, { 0.3f, 0.25f, 0.32f } },
                                             { { 0.85f, 0.8f, 0.6f }, { 0.2f, 0.55f, 0.45f } } };
    /* what an element does to the steel, the fittings and the grip */
    static const vec3 OV_METAL[6] = { { 0, 0, 0 }, { 0.62f, 0.82f, 1.0f }, { 0.42f, 0.28f, 0.24f }, { 0.6f, 0.62f, 0.85f }, { 0.48f, 0.62f, 0.42f },
                                      { 0.34f, 0.1f, 0.13f } };
    static const vec3 OV_EDGE[6] = { { 0, 0, 0 }, { 0.92f, 0.98f, 1.0f }, { 1.0f, 0.62f, 0.32f }, { 0.85f, 0.88f, 1.0f }, { 0.7f, 0.9f, 0.5f },
                                     { 0.8f, 0.2f, 0.22f } };
    static const vec3 OV_ACCENT[6] = { { 0, 0, 0 }, { 0.72f, 0.86f, 1.0f }, { 0.75f, 0.32f, 0.16f }, { 0.85f, 0.5f, 0.28f }, { 0.36f, 0.45f, 0.2f },
                                       { 0.22f, 0.17f, 0.2f } };
    static const vec3 OV_GRIP[6] = { { 0, 0, 0 }, { 0.32f, 0.42f, 0.58f }, { 0.36f, 0.08f, 0.05f }, { 0.12f, 0.14f, 0.3f }, { 0.12f, 0.22f, 0.1f },
                                     { 0.08f, 0.04f, 0.05f } };
    static const vec3 OV_OWN[6] = { { 0, 0, 0 }, { 0.55f, 0.82f, 1.08f }, { 1.0f, 0.5f, 0.15f }, { 0.85f, 0.5f, 0.28f }, { 0.35f, 0.8f, 0.25f },
                                    { 0.45f, 0.05f, 0.08f } };
    static const float GLOW[RAR_COUNT] = { 0.0f, 0.55f, 0.8f, 1.0f, 1.25f };
    int r = it->rarity < RAR_COUNT ? it->rarity : 0;
    WLook L;
    L.rarity = r;
    L.st = weapon_style(it);
    int ov = L.st.overlay;
    /* the material colours the metal: bronze, bone, obsidian, jade, starmetal... */
    vec3 tint = MATERIALS[it->material % MAT_COUNT].tint;
    vec3 metal = mulv(METAL[r], tint);
    vec3 edge = v3_add(metal, v3(0.15f, 0.15f, 0.15f)), accent = ACCENT[r], grip = GRIP[r], wrap = WRAP[r][(L.st.h >> 7) & 1];
    if (ov) { /* the element's colours over the tier's, the tier's brightness kept */
        float lum = (metal.x + metal.y + metal.z) / 3.0f;
        bool frost = ov == OV_FROST; /* ice is bright, even on blackened steel */
        metal = v3_lerp(metal, v3_scale(OV_METAL[ov], frost ? 0.72f + lum * 0.4f : 0.35f + lum), frost ? 0.78f : 0.6f);
        edge = v3_lerp(edge, OV_EDGE[ov], 0.7f);
        accent = v3_lerp(accent, OV_ACCENT[ov], 0.6f);
        grip = v3_lerp(grip, OV_GRIP[ov], 0.75f);
        wrap = v3_lerp(wrap, v3_scale(OV_GRIP[ov], 1.4f), 0.6f);
    }
    metal = v3(fminf(metal.x, 0.66f), fminf(metal.y, 0.69f), fminf(metal.z, 0.74f)); /* (white starmetal would burn out under a torch) */
    edge = v3(fminf(edge.x, 0.9f), fminf(edge.y, 0.92f), fminf(edge.z, 0.96f));
    L.metal = mulv(metal, light);
    L.edge = mulv(edge, light);
    L.wood = mulv(r == RAR_EPIC ? v3(0.34f, 0.2f, 0.17f) : r == RAR_LEGENDARY ? v3(0.85f, 0.82f, 0.74f) : v3(0.45f, 0.29f, 0.15f), light);
    if (ov == OV_FROST) L.wood = v3_lerp(L.wood, mulv(v3(0.6f, 0.7f, 0.82f), light), 0.45f); /* rimed */
    L.grip = mulv(grip, light);
    L.wrap = mulv(wrap, light);
    L.accent = mulv(accent, light);
    L.ice = mulv(OV_OWN[ov], light);
    L.element = item_affix_element(it);
    L.vampiric = item_has_affix(it, AF_VAMPIRIC);
    L.glow_amt = GLOW[r];
    float pulse = 0.75f + 0.25f * sinf(time * 4.0f + it->level * 0.7f);
    L.glow = v3_scale(weapon_glow_color(it), L.glow_amt * pulse);
    return L;
}

/* ---------- a frame to build a weapon in ----------
 * o: the hand; u: along the weapon (toward the tip, the top limb, or forward for a
 * crossbow); v: its width (the blade's edges); w: its thickness; up: which way is up
 * (for motes that rise or fall); s: scale. */
typedef struct WF {
    vec3 o, u, v, w, up;
    vec3 aim; /* bows: where a nocked arrow points (0: away from the string, -w) */
    float s;
} WF;

static vec3 P(const WF *f, float a, float b, float c) {
    return v3_add(f->o, v3_add(v3_scale(f->u, a * f->s), v3_add(v3_scale(f->v, b * f->s), v3_scale(f->w, c * f->s))));
}
static void B(const WF *f, float a, float b, float c, float ha, float hb, float hc, vec3 col, bool em) {
    batch3d_obox(P(f, a, b, c), v3_scale(f->u, ha * f->s), v3_scale(f->v, hb * f->s), v3_scale(f->w, hc * f->s), col, em);
}
/* a box from point (a0,b0,c0) to (a1,b1,c1) with half widths hb across and hc thick */
static void SEG(const WF *f, float a0, float b0, float c0, float a1, float b1, float c1, float r, vec3 col, bool em) {
    vec3 p = P(f, a0, b0, c0), q = P(f, a1, b1, c1);
    vec3 d = v3_scale(v3_sub(q, p), 0.5f);
    vec3 n = v3_norm(d);
    vec3 x = fabsf(v3_dot(n, f->w)) < 0.9f ? f->w : f->v;
    vec3 sx = v3_norm(v3_cross(n, x)), sy = v3_cross(sx, n);
    batch3d_obox(v3_add(p, d), d, v3_scale(sx, r * f->s), v3_scale(sy, r * f->s), col, em);
}
static void SP(const WF *f, float a, float b, float c, vec3 dir, float r, float h, vec3 col, bool em) {
    batch3d_spike(P(f, a, b, c), dir, r * f->s, h * f->s, col, em);
}
static void O(const WF *f, float a, float b, float c, float r, float h, float spin, vec3 col, bool em) {
    batch3d_octa(P(f, a, b, c), r * f->s, h * f->s, spin, col, em);
}

/* ---------- motes around a magical weapon ---------- */

static void mote(vec3 p, float size, vec3 col) { batch3d_box(p, v3(size, size, size), p.x * 40.0f, col, true); }

/* Motes along the weapon from a0 to a1 (frame units). */
static void weapon_motes(const WF *f, const WLook *L, float a0, float a1, float time, float seed) {
    if (L->glow_amt <= 0) return;
    int n = L->rarity == RAR_LEGENDARY ? 12 : L->rarity == RAR_EPIC ? 9 : L->rarity == RAR_RARE ? 7 : 5;
    float sz = 0.007f * f->s;
    for (int i = 0; i < n; i++) {
        float k = (i + 0.5f) / n, h = hashf(seed + i * 7.31f);
        float a = a0 + (a1 - a0) * k;
        float ph;
        vec3 p;
        switch (L->vampiric ? EL_SHADOW : L->element) {
        case EL_FIRE: /* embers rising off the weapon */
            ph = fmodf(time * (0.9f + h * 0.6f) + h, 1.0f);
            p = v3_add(P(f, a, (h - 0.5f) * 0.05f, 0), v3_scale(f->up, ph * 0.14f * f->s));
            mote(p, sz * (1.3f - ph), v3_lerp(v3(1.0f, 0.85f, 0.3f), v3(1.0f, 0.25f, 0.05f), ph));
            break;
        case EL_FROST: /* frost drifting down and twinkling */
            ph = fmodf(time * (0.35f + h * 0.3f) + h, 1.0f);
            p = v3_add(P(f, a, (h - 0.5f) * 0.07f, sinf(time * 2.0f + i) * 0.02f), v3_scale(f->up, -ph * 0.09f * f->s));
            mote(p, sz * (0.6f + 0.6f * (hashf(floorf(time * 8.0f) + i) > 0.5f)), v3(0.75f, 0.92f, 1.0f));
            break;
        case EL_LIGHTNING: { /* sparks that jump along the edge */
            float tick = floorf(time * 18.0f);
            if (hashf(tick + i * 3.1f) < 0.45f) break;
            float j = hashf(tick * 1.7f + i);
            vec3 p0 = P(f, a, (j - 0.5f) * 0.08f, (hashf(tick + i) - 0.5f) * 0.04f);
            vec3 p1 = P(f, a + 0.05f, (hashf(tick * 2.3f + i) - 0.5f) * 0.08f, 0);
            vec3 d = v3_scale(v3_sub(p1, p0), 0.5f);
            vec3 sx = v3_scale(v3_norm(v3_cross(d, f->w)), sz * 0.4f);
            batch3d_obox(v3_add(p0, d), d, sx, v3_scale(f->w, sz * 0.4f), v3(0.85f, 0.9f, 1.0f), true);
            break;
        }
        case EL_POISON: /* venom dripping */
            ph = fmodf(time * (0.6f + h * 0.4f) + h, 1.0f);
            p = v3_add(P(f, a, (h - 0.5f) * 0.04f, 0), v3_scale(f->up, -ph * ph * 0.16f * f->s));
            mote(p, sz * (ph < 0.15f ? 1.4f : 1.0f), v3(0.4f, 1.0f, 0.3f));
            break;
        case EL_SHADOW: { /* a blood-red mist circling the weapon */
            float ang = time * 2.5f + i * 2.0f * PI_F / n;
            p = P(f, a, cosf(ang) * 0.05f, sinf(ang) * 0.05f);
            mote(p, sz * (0.8f + 0.4f * sinf(time * 5.0f + i)), L->vampiric ? v3(0.85f, 0.08f, 0.12f) : v3(0.55f, 0.25f, 0.85f));
            break;
        }
        default: { /* plain magic: twinkles in the rarity's colour */
            float tw = hashf(floorf(time * 5.0f + h * 5.0f) + i);
            if (tw < 0.5f) break;
            p = P(f, a, (h - 0.5f) * 0.06f, (hashf(h + 3.0f) - 0.5f) * 0.03f);
            mote(p, sz * (tw - 0.4f) * 1.6f, v3_scale(L->glow, 1.0f / fmaxf(0.3f, L->glow_amt)));
            break;
        }
        }
    }
    /* epic weapons carry a rune that slowly turns about them */
    if (L->rarity >= RAR_EPIC) {
        float ang = time * 1.3f;
        O(f, a0 + (a1 - a0) * 0.55f, cosf(ang) * 0.09f, sinf(ang) * 0.09f, 0.012f, 0.018f, time * 3.0f, L->glow, true);
        /* legendaries carry a second rune the other way round, in their own green-white */
        if (L->rarity == RAR_LEGENDARY)
            O(f, a0 + (a1 - a0) * 0.35f, cosf(-ang * 1.4f) * 0.11f, sinf(-ang * 1.4f) * 0.11f, 0.014f, 0.02f, -time * 3.0f,
              v3(0.6f, 1.0f, 0.8f), true);
    }
}

/* ---------- shaped parts ----------
 * The models are built from real shapes rather than boxes: blades lofted through cross-sections
 * (edge bevels, a fuller, distal taper, an ogive point), round tapered hafts and grips, lathed
 * pommels, cups and heads, and flat profiles with a wedge (axe bits, scythe blades, flanges).
 * Each facet is lit here: a key light from above and in front, and a glint on metal toward the
 * eye, both stepped like the world's light. */

typedef struct Mat {
    vec3 c;     /* lit base colour */
    float spec; /* 0 dull (wood, leather) .. 1.3 polished edge */
    bool em;    /* emissive: drawn as it is */
} Mat;

static vec3 g_weye;           /* the eye, in the space the weapon is drawn in */
static vec3 g_wlight = { 1, 1, 1 }; /* the light's colour where the weapon is */
static float g_wtime;                  /* the game's time, for what flickers */

void weapon_set_eye(vec3 eye) { g_weye = eye; }

static Mat MT(vec3 c, float spec) { return (Mat){ c, spec, false }; }
static Mat ME(vec3 c) { return (Mat){ c, 0, true }; }

static vec3 lit(vec3 n, vec3 mid, Mat m) {
    if (m.em) return m.c;
    static const vec3 KEY = { -0.36f, 0.82f, 0.44f };
    vec3 l = v3_norm(KEY);
    float d = v3_dot(n, l);
    float diff = 0.34f + 0.66f * fmaxf(0.0f, d);
    diff = floorf(diff * 9.0f + 0.5f) / 9.0f; /* stepped, like the world's light */
    vec3 c = v3_scale(m.c, diff);
    if (m.spec > 0) {
        vec3 view = v3_norm(v3_sub(g_weye, mid));
        vec3 r = v3_sub(v3_scale(n, 2.0f * d), l);
        float sp = powf(fmaxf(0.0f, v3_dot(r, view)), 9.0f) * m.spec;
        sp = floorf(sp * 5.0f) / 5.0f;
        if (sp > 0) c = v3_add(c, v3_scale(v3_lerp(g_wlight, v3(1, 1, 1), 0.4f), sp * 0.42f)); /* a glint, whiter than the light */
        c = v3(fminf(c.x, 1.15f), fminf(c.y, 1.15f), fminf(c.z, 1.15f));
    }
    return c;
}

/* a triangle, its normal turned away from `inside` */
static void wtri(vec3 a, vec3 b, vec3 c, vec3 inside, Mat m) {
    vec3 n = v3_cross(v3_sub(b, a), v3_sub(c, a));
    float len = v3_len(n);
    if (len < 1e-9f) return;
    n = v3_scale(n, 1.0f / len);
    vec3 mid = v3_scale(v3_add(v3_add(a, b), c), 1.0f / 3.0f);
    if (v3_dot(n, v3_sub(mid, inside)) < 0) n = v3_scale(n, -1.0f);
    batch3d_tri_raw(a, b, c, lit(n, mid, m), m.em);
}
static void wquad(vec3 a, vec3 b, vec3 c, vec3 d, vec3 inside, Mat m) {
    wtri(a, b, c, inside, m);
    wtri(a, c, d, inside, m);
}

#define RING_MAX 12

/* A round tube through k points (frame units) with a radius at each, n sides; closed at an end
 * whose radius is above zero. Bends follow the path. */
static void tube_path(const WF *f, const float (*pt)[3], const float *r, int k, int n, Mat m) {
    if (k < 2) return;
    n = n > RING_MAX ? RING_MAX : n < 3 ? 3 : n;
    vec3 prev[RING_MAX], cur[RING_MAX];
    vec3 e1 = { 0, 0, 0 };
    for (int i = 0; i < k; i++) {
        vec3 p = P(f, pt[i][0], pt[i][1], pt[i][2]);
        vec3 a = P(f, pt[i > 0 ? i - 1 : 0][0], pt[i > 0 ? i - 1 : 0][1], pt[i > 0 ? i - 1 : 0][2]);
        vec3 b = P(f, pt[i < k - 1 ? i + 1 : i][0], pt[i < k - 1 ? i + 1 : i][1], pt[i < k - 1 ? i + 1 : i][2]);
        vec3 t = v3_sub(b, a);
        if (v3_len(t) < 1e-7f) t = f->u;
        t = v3_norm(t);
        if (i == 0) e1 = v3_cross(t, fabsf(v3_dot(t, f->w)) > 0.9f ? f->v : f->w);
        else e1 = v3_sub(e1, v3_scale(t, v3_dot(e1, t))); /* carried along the path, so the rings do not twist */
        if (v3_len(e1) < 1e-6f) e1 = v3_cross(t, fabsf(t.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0));
        e1 = v3_norm(e1);
        vec3 e2 = v3_cross(t, e1);
        for (int j = 0; j < n; j++) {
            float ang = (j + 0.5f) * 2.0f * PI_F / n;
            cur[j] = v3_add(p, v3_scale(v3_add(v3_scale(e1, cosf(ang)), v3_scale(e2, sinf(ang))), r[i] * f->s));
        }
        if (i == 0 || i == k - 1) { /* end caps */
            if (r[i] > 1e-5f) {
                vec3 in = i == 0 ? b : a;
                for (int j = 1; j < n - 1; j++) wtri(cur[0], cur[j], cur[j + 1], in, m);
            }
        }
        if (i > 0) {
            vec3 axis_mid = v3_scale(v3_add(p, P(f, pt[i - 1][0], pt[i - 1][1], pt[i - 1][2])), 0.5f);
            for (int j = 0; j < n; j++) wquad(prev[j], prev[(j + 1) % n], cur[(j + 1) % n], cur[j], axis_mid, m);
        }
        memcpy(prev, cur, sizeof cur);
    }
}

/* a straight tapered tube from one point to another */
static void tube(const WF *f, float a0, float b0, float c0, float a1, float b1, float c1, float r0, float r1, int n, Mat m) {
    const float pt[2][3] = { { a0, b0, c0 }, { a1, b1, c1 } };
    const float r[2] = { r0, r1 };
    tube_path(f, pt, r, 2, n, m);
}

/* a lathed shape along u: k (a, radius) pairs */
static void lathe(const WF *f, const float (*prof)[2], int k, float b, float c, int n, Mat m) {
    float pt[16][3], r[16];
    if (k > 16) k = 16;
    for (int i = 0; i < k; i++) { pt[i][0] = prof[i][0]; pt[i][1] = b; pt[i][2] = c; r[i] = prof[i][1]; }
    tube_path(f, (const float (*)[3])pt, r, k, n, m);
}

/* A flat profile in the (a, b) plane, star-shaped about `ctr`, with a half thickness at each
 * point (thin at a cutting edge). Sides from point i to i+1 use `edge` where edge_mask has bit i. */
static void slab(const WF *f, const float (*pt)[2], const float *th, int k, const float ctr[2], float cth, Mat face, Mat side, Mat edge,
                 unsigned edge_mask) {
    vec3 in = P(f, ctr[0], ctr[1], 0);
    vec3 top_c = P(f, ctr[0], ctr[1], cth), bot_c = P(f, ctr[0], ctr[1], -cth);
    for (int i = 0; i < k; i++) {
        int j = (i + 1) % k;
        vec3 at = P(f, pt[i][0], pt[i][1], th[i]), bt = P(f, pt[j][0], pt[j][1], th[j]);
        vec3 ab = P(f, pt[i][0], pt[i][1], -th[i]), bb = P(f, pt[j][0], pt[j][1], -th[j]);
        wtri(top_c, at, bt, in, face);
        wtri(bot_c, bb, ab, in, face);
        wquad(at, bt, bb, ab, in, (edge_mask >> i) & 1 ? edge : side);
    }
}

/* ---------- blades ---------- */

enum { BL_SWORD, BL_DAGGER, BL_GREAT, BL_RAPIER, BL_LEAF, BL_COUNT };

#define BL_MAXSEC 26

/* A blade from a0, len long, half width hw, half thickness th: lofted through cross-sections
 * with an edge bevel each side, a fuller down the flat (the groove), and a point. The style
 * shapes it (broad, waved like a flamberge, serrated, a clipped point with a back hook), adds a
 * second fuller, an inlaid line or glowing runes, and the element's marks on the steel. */
static void blade(const WF *f, const WLook *L, float a0, float len, float hw, float th, int kind) {
    /* where the sections are (0..1 of the length), and the width, thickness and fuller depth there */
    static const float K[BL_COUNT][7] = {
        { 0, 0.15f, 0.4f, 0.65f, 0.82f, 0.93f, 1 }, { 0, 0.2f, 0.45f, 0.65f, 0.8f, 0.92f, 1 }, { 0, 0.08f, 0.1f, 0.45f, 0.8f, 0.93f, 1 },
        { 0, 0.2f, 0.45f, 0.7f, 0.85f, 0.95f, 1 },   { 0, 0.15f, 0.35f, 0.55f, 0.75f, 0.9f, 1 },
    };
    static const float WID[BL_COUNT][7] = {
        { 1, 0.97f, 0.9f, 0.8f, 0.68f, 0.42f, 0 }, { 1, 0.92f, 0.78f, 0.62f, 0.45f, 0.22f, 0 }, { 0.62f, 0.62f, 1, 0.92f, 0.75f, 0.44f, 0 },
        { 1, 0.95f, 0.86f, 0.72f, 0.55f, 0.32f, 0 }, { 0.35f, 0.75f, 0.98f, 0.92f, 0.65f, 0.32f, 0 },
    };
    static const float THK[BL_COUNT][7] = {
        { 1, 0.95f, 0.8f, 0.66f, 0.55f, 0.42f, 0 }, { 1, 0.9f, 0.75f, 0.6f, 0.5f, 0.35f, 0 }, { 1.2f, 1.2f, 1, 0.8f, 0.62f, 0.45f, 0 },
        { 1, 0.95f, 0.85f, 0.75f, 0.6f, 0.4f, 0 },   { 1.3f, 1.1f, 1, 0.85f, 0.65f, 0.4f, 0 },
    };
    static const float FUL[BL_COUNT][7] = {
        { 0.55f, 0.55f, 0.5f, 0.2f, 0, 0, 0 }, { 0.35f, 0.3f, 0, 0, 0, 0, 0 }, { 0, 0, 0.6f, 0.55f, 0.2f, 0, 0 }, { 0 }, { 0 },
    };
    /* the flat runs to this fraction of the half width (0: a diamond, a ridge down the middle) */
    static const float FLAT[BL_COUNT] = { 0.55f, 0.45f, 0.6f, 0.0f, 0.12f };
    const WStyle *S = &L->st;
    int form = S->form;
    if (kind == BL_RAPIER && form != BF_BROAD) form = BF_PLAIN; /* a rapier stays a rapier */
    hw *= S->bulk * (form == BF_BROAD ? 1.16f : 1.0f);
    th *= S->bulk;
    len *= 1.0f + (S->keen - 1.0f) * 0.3f;
    Mat metal = MT(L->metal, L->rarity == RAR_EPIC ? 0.4f : 0.85f), edge = MT(L->edge, 1.3f);
    Mat groove = MT(v3_scale(L->metal, 0.72f), 0.5f);
    /* the sections: the table's own, or many more for waves and teeth */
    int n = form == BF_WAVED ? BL_MAXSEC : 7;
    float sa[BL_MAXSEC], sb[BL_MAXSEC], sw[BL_MAXSEC], st[BL_MAXSEC], sf[BL_MAXSEC];
    for (int i = 0; i < n; i++) {
        float k = n == 7 ? K[kind][i] : (float)i / (n - 1);
        int j = 0;
        while (j < 5 && K[kind][j + 1] < k) j++;
        float u = (k - K[kind][j]) / fmaxf(1e-5f, K[kind][j + 1] - K[kind][j]);
        u = clampf(u, 0, 1);
        float w = lerpf(WID[kind][j], WID[kind][j + 1], u), t = lerpf(THK[kind][j], THK[kind][j + 1], u), fd = lerpf(FUL[kind][j], FUL[kind][j + 1], u);
        if (form == BF_BROAD && k < 0.85f) w *= 1.0f + 0.12f * k; /* keeps its width toward the point */
        if (form == BF_WAVED && k > 0.06f) w *= 1.0f + 0.15f * sinf(k * PI_F * 9.0f) * fminf(1.0f, (1.0f - k) / 0.25f);
        float bc = 0;
        if (form == BF_HOOKED && k > 0.72f) bc = hw * 0.5f * (k - 0.72f) / 0.28f; /* a clipped point, on the edge's line */
        sa[i] = a0 + k * len; sb[i] = bc; sw[i] = hw * w; st[i] = th * t; sf[i] = fd;
    }
    vec3 ring[2][10];
    for (int i = 0; i < n; i++) {
        float a = sa[i], w = sw[i], t = st[i], bc = sb[i];
        float sh = FLAT[kind] * w;
        float fw = fminf(0.24f * w, sh * 0.6f);
        float ft = t * (1.0f - 0.55f * sf[i]);
        const float pts[10][2] = { { w, 0 }, { sh, t }, { fw, ft }, { -fw, ft }, { -sh, t }, { -w, 0 }, { -sh, -t }, { -fw, -ft }, { fw, -ft }, { sh, -t } };
        vec3 *cur = ring[i & 1], *prv = ring[(i + 1) & 1];
        for (int j = 0; j < 10; j++) cur[j] = P(f, a, pts[j][0] + bc, pts[j][1]);
        if (i == 0) /* the base, under the guard */
            for (int j = 1; j < 9; j++) wtri(cur[0], cur[j], cur[j + 1], P(f, a + 0.01f, 0, 0), metal);
        if (i > 0) {
            vec3 in = P(f, (a + sa[i - 1]) * 0.5f, (bc + sb[i - 1]) * 0.5f, 0);
            for (int j = 0; j < 10; j++) {
                Mat mm = (j == 0 || j == 4 || j == 5 || j == 9) ? edge : (j == 2 || j == 7) && sf[i - 1] > 0.05f ? groove : metal;
                wquad(prv[j], prv[(j + 1) % 10], cur[(j + 1) % 10], cur[j], in, mm);
            }
            if (L->glow_amt > 0) { /* a glowing line along each edge */
                SEG(f, sa[i - 1], sb[i - 1] + sw[i - 1] + 0.002f, 0, a, bc + w + 0.002f, 0, 0.0032f, L->glow, true);
                SEG(f, sa[i - 1], sb[i - 1] - sw[i - 1] - 0.002f, 0, a, bc - w - 0.002f, 0, 0.0032f, L->glow, true);
            }
        }
    }
    /* marks on the flats, at a fraction of the half width, both faces: they sit on the surface */
    float tip_a = a0 + len * 0.62f;
    for (int i = 1; i < n; i++) {
        if (sa[i - 1] > tip_a) break;
        for (int face = -1; face <= 1; face += 2) {
            float c0 = face * (st[i - 1] * (1.0f - 0.55f * sf[i - 1]) + 0.0006f), c1 = face * (st[i] * (1.0f - 0.55f * sf[i]) + 0.0006f);
            vec3 in = P(f, (sa[i] + sa[i - 1]) * 0.5f, 0, 0);
            if (S->fullers == 2 && sf[i - 1] > 0.05f && FLAT[kind] > 0.3f) /* a second groove beside each edge of the first */
                for (int side = -1; side <= 1; side += 2) {
                    float b0 = side * sw[i - 1] * 0.36f, b1 = side * sw[i] * 0.36f, d0 = side * sw[i - 1] * 0.06f, d1 = side * sw[i] * 0.06f;
                    float h0 = face * st[i - 1] * 0.97f + face * 0.0006f, h1 = face * st[i] * 0.97f + face * 0.0006f;
                    wquad(P(f, sa[i - 1], b0 - d0, h0), P(f, sa[i - 1], b0 + d0, h0), P(f, sa[i], b1 + d1, h1), P(f, sa[i], b1 - d1, h1), in,
                          MT(v3_scale(L->metal, 0.6f), 0.4f));
                }
            if (S->inlay == 1 && sf[i - 1] > 0.05f) { /* an inlaid line down the fuller, gold on gilded blades */
                float d0 = sw[i - 1] * 0.035f, d1 = sw[i] * 0.035f;
                wquad(P(f, sa[i - 1], -d0, c0), P(f, sa[i - 1], d0, c0), P(f, sa[i], d1, c1), P(f, sa[i], -d1, c1), in, MT(L->accent, 1.1f));
            }
        }
    }
    /* glowing runes down the middle (epic, legendary), or the element's own marks */
    vec3 rune = S->overlay == OV_FROST ? v3(0.6f, 0.85f, 1.0f) : S->overlay ? v3_scale(L->glow, 1.0f / fmaxf(0.3f, L->glow_amt)) : v3_scale(L->glow, 1.2f);
    if (S->inlay == 2 || S->overlay) {
        int marks = S->inlay == 2 ? 7 : 4;
        for (int m = 0; m < marks; m++) {
            float k = 0.1f + 0.5f * (m + 0.5f) / marks;
            float a = a0 + k * len;
            /* the surface there */
            int i = 1;
            while (i < n - 1 && sa[i] < a) i++;
            float u = (a - sa[i - 1]) / fmaxf(1e-5f, sa[i] - sa[i - 1]);
            float w = lerpf(sw[i - 1], sw[i], u), t = lerpf(st[i - 1] * (1.0f - 0.55f * sf[i - 1]), st[i] * (1.0f - 0.55f * sf[i]), u);
            float hr = hashf((float)(S->h & 0xFFF) + m * 3.7f);
            for (int face = -1; face <= 1; face += 2) {
                float c = face * (t + 0.0008f);
                if (S->overlay == OV_FROST) { /* rime: white frost creeping in from the edges */
                    for (int side = -1; side <= 1; side += 2)
                        B(f, a, side * w * (0.72f + 0.12f * hr), c, len * 0.03f * (0.6f + hr), w * 0.18f, 0.0006f,
                          v3(0.88f * g_wlight.x, 0.95f * g_wlight.y, 1.0f * g_wlight.z), false);
                } else if (S->overlay == OV_FIRE) { /* cracks glowing with heat */
                    float fl = 0.7f + 0.3f * sinf(g_wtime * 7.0f + m * 1.9f);
                    SEG(f, a - len * 0.03f, (hr - 0.5f) * w * 0.5f, c, a + len * 0.03f, (0.5f - hr) * w * 0.5f, c, 0.0018f, v3(1.2f * fl, 0.45f * fl, 0.1f), true);
                } else if (S->overlay == OV_SHOCK) { /* a zigzag of light */
                    SEG(f, a - len * 0.035f, -w * 0.25f, c, a + len * 0.035f, w * 0.25f, c, 0.0016f, v3(0.75f, 0.8f, 1.3f), true);
                } else if (S->overlay == OV_VENOM || S->overlay == OV_BLOOD) { /* a channel running with it */
                    B(f, a, 0, c, len * 0.04f, w * 0.06f, 0.0006f, S->overlay == OV_VENOM ? v3(0.35f, 1.0f, 0.3f) : v3(0.9f, 0.08f, 0.12f), true);
                } else { /* runes */
                    B(f, a, 0, c, len * 0.012f * (0.6f + hr), w * 0.12f, 0.0006f, rune, true);
                    if (hr > 0.45f) B(f, a + len * 0.016f, w * 0.1f * (hr > 0.7f ? 1 : -1), c, len * 0.006f, w * 0.05f, 0.0006f, rune, true);
                }
            }
        }
    }
    /* teeth along one edge (serrated), a hook on the back (clipped) */
    if (form == BF_SERRATED) {
        vec3 dir = v3_norm(v3_sub(f->v, v3_scale(f->u, 0.7f)));
        for (float k = 0.12f; k < 0.7f; k += 0.055f) {
            int i = 1;
            float a = a0 + k * len;
            while (i < n - 1 && sa[i] < a) i++;
            float u = (a - sa[i - 1]) / fmaxf(1e-5f, sa[i] - sa[i - 1]);
            float w = lerpf(sw[i - 1], sw[i], u);
            SP(f, a, w * 0.92f, 0, dir, 0.006f * S->bulk, 0.016f * S->bulk, L->edge, false);
        }
    }
    if (form == BF_HOOKED) SP(f, a0 + len * 0.66f, -sw[1] * 0.85f, 0, v3_norm(v3_sub(v3_scale(f->v, -1.0f), v3_scale(f->u, 0.9f))), 0.008f, 0.03f, L->edge, false);
}

/* A grip with a cord wrap, ferrules, a guard and a pommel, in the weapon's style. guard_w: half
 * the guard's span; drop: how far the quillons bend; pommel: its radius. */
static void hilt(const WF *f, const WLook *L, float grip_len, float guard_w, float drop, float pommel) {
    const WStyle *S = &L->st;
    Mat grip = MT(L->grip, 0.15f), wrap = MT(L->wrap, S->gilded ? 0.9f : 0.12f), acc = MT(L->accent, 0.9f);
    const float gp[3][3] = { { -grip_len, 0, 0 }, { 0, 0, 0 }, { grip_len, 0, 0 } };
    const float gr[3] = { 0.0145f, 0.0168f, 0.0148f };
    tube_path(f, gp, gr, 3, 8, grip);
    for (float a = -grip_len + 0.018f; a < grip_len - 0.012f; a += 0.026f) /* the cord, wound on */
        tube(f, a, 0, 0, a + 0.007f, 0, 0, 0.0175f, 0.0175f, 8, wrap);
    tube(f, grip_len - 0.006f, 0, 0, grip_len + 0.006f, 0, 0, 0.018f, 0.019f, 8, acc); /* ferrules */
    tube(f, -grip_len - 0.006f, 0, 0, -grip_len + 0.004f, 0, 0, 0.019f, 0.018f, 8, acc);
    float ga = grip_len + 0.016f;
    guard_w *= S->guard == GD_WINGS ? 1.2f : 1.0f;
    switch (S->guard) {
    case GD_DISC: { /* a round plate */
        const float dp[4][2] = { { ga - 0.006f, 0.0f }, { ga - 0.005f, guard_w * 0.42f }, { ga + 0.005f, guard_w * 0.42f }, { ga + 0.006f, 0.0f } };
        lathe(f, dp, 4, 0, 0, 12, acc);
        break;
    }
    case GD_WINGS: { /* two wings swept toward the blade, cut into feathers */
        for (int s = -1; s <= 1; s += 2) {
            const float wp[7][2] = { { ga - 0.004f, s * 0.012f }, { ga + 0.03f, s * guard_w * 0.45f }, { ga + 0.09f, s * guard_w * 1.05f }, { ga + 0.06f, s * guard_w * 0.82f },
                                     { ga + 0.055f, s * guard_w * 0.95f }, { ga + 0.025f, s * guard_w * 0.7f }, { ga + 0.004f, s * guard_w * 0.5f } };
            const float wt[7] = { 0.009f, 0.006f, 0.002f, 0.003f, 0.002f, 0.004f, 0.007f };
            slab(f, wp, wt, 7, (const float[2]){ ga + 0.03f, s * guard_w * 0.45f }, 0.006f, acc, acc, MT(L->edge, 1.3f), 0x6u);
        }
        tube(f, ga - 0.01f, 0, 0, ga + 0.012f, 0, 0, 0.02f, 0.016f, 8, acc);
        break;
    }
    default: {
        /* a bar, thicker in the middle: straight, drooping toward the hand or swept toward the blade */
        float d = S->guard == GD_STRAIGHT ? 0.0f : S->guard == GD_DROOP ? -drop * 0.8f : drop * 1.6f;
        const float qp[5][3] = { { ga + d, -guard_w, 0 }, { ga + d * 0.25f, -guard_w * 0.6f, 0 }, { ga, 0, 0 },
                                 { ga + d * 0.25f, guard_w * 0.6f, 0 }, { ga + d, guard_w, 0 } };
        const float qr[5] = { 0.0075f, 0.0095f, 0.0135f, 0.0095f, 0.0075f };
        tube_path(f, qp, qr, 5, 6, acc);
        for (int s = -1; s <= 1; s += 2) {
            if (S->guard == GD_THORNS) { /* thorns along it and at the ends */
                SP(f, ga + d, s * guard_w, 0, v3_norm(v3_add(v3_scale(f->v, (float)s), v3_scale(f->u, 0.3f))), 0.008f, 0.04f, L->edge, false);
                for (int k = 1; k <= 2; k++)
                    SP(f, ga + d * 0.25f * k, s * guard_w * 0.35f * k, 0, v3_norm(v3_add(f->u, v3_scale(f->v, s * 0.4f))), 0.005f, 0.022f, L->edge, false);
            } else if (S->guard == GD_UPSWEPT) { /* the ends curl back on themselves */
                const float cp[3][3] = { { ga + d, s * guard_w, 0 }, { ga + d + 0.016f, s * (guard_w + 0.012f), 0 }, { ga + d + 0.02f, s * (guard_w - 0.004f), 0 } };
                const float cr[3] = { 0.0075f, 0.0065f, 0.005f };
                tube_path(f, cp, cr, 3, 6, acc);
            } else { /* knobs */
                const float kp[3][2] = { { -0.011f, 0.0f }, { 0.0f, 0.011f }, { 0.011f, 0.0f } };
                float pt[3][3], r[3];
                for (int i = 0; i < 3; i++) { pt[i][0] = ga + d; pt[i][1] = s * (guard_w + 0.004f + kp[i][0]); pt[i][2] = 0; r[i] = kp[i][1]; }
                tube_path(f, (const float (*)[3])pt, r, 3, 6, acc);
            }
        }
        break;
    }
    }
    /* the langet: a little block where the blade meets the guard */
    const float lp[4][2] = { { ga - 0.012f, 0.0f }, { ga, 0.021f }, { ga + 0.024f, 0.0f }, { ga, -0.021f } };
    float lt[4] = { 0.016f, 0.016f, 0.012f, 0.016f };
    slab(f, lp, lt, 4, (const float[2]){ ga, 0 }, 0.017f, acc, acc, acc, 0);
    if (S->studs) for (int s = -1; s <= 1; s += 2) O(f, ga, 0, s * 0.018f, 0.006f, 0.005f, 0.0f, v3_scale(L->accent, 1.1f), false);
    /* the pommel and the peen of the tang */
    vec3 gem = L->glow_amt > 0 ? L->glow : S->overlay == OV_FROST ? L->ice : v3(0.55f * g_wlight.x, 0.08f * g_wlight.y, 0.1f * g_wlight.z);
    bool gem_em = L->glow_amt > 0;
    float pa = -grip_len - 0.006f - pommel;
    switch (S->pommel) {
    case PM_PEAR: {
        const float pp[5][2] = { { -grip_len - 0.004f, 0.012f }, { -grip_len - 0.012f, pommel * 0.9f }, { -grip_len - 0.026f, pommel * 1.05f },
                                 { -grip_len - 0.04f, pommel * 0.7f }, { -grip_len - 0.046f, 0.0f } };
        lathe(f, pp, 5, 0, 0, 10, acc);
        break;
    }
    case PM_GEM: {
        const float pp[4][2] = { { -grip_len - 0.004f, 0.012f }, { -grip_len - 0.018f, pommel * 0.95f }, { -grip_len - 0.03f, pommel * 0.9f }, { -grip_len - 0.032f, 0.0f } };
        lathe(f, pp, 4, 0, 0, 10, acc);
        O(f, -grip_len - 0.04f, 0, 0, pommel * 0.65f, pommel * 0.8f, 0.4f, gem, gem_em);
        break;
    }
    case PM_SPIKE: {
        const float pp[5][2] = { { -grip_len - 0.004f, 0.012f }, { -grip_len - 0.014f, pommel }, { -grip_len - 0.024f, pommel * 0.8f },
                                 { -grip_len - 0.03f, pommel * 0.45f }, { -grip_len - 0.07f, 0.0f } };
        lathe(f, pp, 5, 0, 0, 6, MT(L->edge, 1.2f));
        break;
    }
    case PM_CROWN: {
        const float pp[3][2] = { { -grip_len - 0.004f, 0.012f }, { -grip_len - 0.016f, pommel * 0.9f }, { -grip_len - 0.02f, pommel * 0.95f } };
        lathe(f, pp, 3, 0, 0, 10, acc);
        for (int k = 0; k < 5; k++) {
            float ang = k * 2.0f * PI_F / 5.0f;
            float b = cosf(ang) * pommel * 0.85f, c = sinf(ang) * pommel * 0.85f;
            SP(f, -grip_len - 0.02f, b, c, v3_norm(v3_sub(v3_add(v3_scale(f->v, b), v3_scale(f->w, c)), v3_scale(f->u, pommel * 1.5f))), 0.005f, 0.022f,
               L->accent, false);
        }
        O(f, -grip_len - 0.034f, 0, 0, pommel * 0.55f, pommel * 0.7f, g_wtime * 1.5f, gem, true);
        break;
    }
    default: /* a wheel */
        tube(f, pa, 0, -0.012f, pa, 0, 0.012f, pommel, pommel, 10, acc);
        tube(f, pa, 0, -0.016f, pa, 0, 0.016f, pommel * 0.55f, pommel * 0.55f, 8, MT(v3_scale(L->accent, 0.8f), 0.9f));
        tube(f, pa - pommel - 0.004f, 0, 0, pa - pommel + 0.002f, 0, 0, 0.004f, 0.006f, 6, acc);
        if (L->glow_amt > 0) O(f, pa, 0, 0.0165f, 0.008f, 0.01f, 0.0f, L->glow, true);
        break;
    }
    if (L->glow_amt > 0) O(f, ga, 0, 0.018f, 0.009f, 0.012f, 0.0f, L->glow, true); /* a gem in the guard */
}

/* a round haft, a little thicker toward the head, with a grip wound at the hand */
static void haft(const WF *f, const WLook *L, float a0, float a1, float r0, float r1, float grip0, float grip1, Mat wood) {
    tube(f, a0, 0, 0, a1, 0, 0, r0, r1, 8, wood);
    if (grip1 > grip0) {
        Mat g = MT(L->grip, 0.15f), w = MT(L->wrap, L->st.gilded ? 0.9f : 0.12f);
        float rg = fmaxf(r0, r1) + 0.0025f;
        tube(f, grip0, 0, 0, grip1, 0, 0, rg, rg, 8, g);
        for (float a = grip0 + 0.01f; a < grip1 - 0.008f; a += 0.022f) tube(f, a, 0, 0, a + 0.006f, 0, 0, rg + 0.0015f, rg + 0.0015f, 8, w);
    }
}

/* ---------- the element grown on the weapon ----------
 * Along a span of the weapon (a0..a1, w0..w1 its half width there): on a blade (flat) from the
 * edges, on anything round from all round. Frost: ice crystals, hexagonal and pointed, and
 * icicles hanging under it; fire: tongues of flame licking up; lightning: a copper coil and arcs;
 * venom: drops gathering and falling; the vampiric: thorns. More of it the higher the tier. */
static void overlay(const WF *f, const WLook *L, float a0, float a1, float w0, float w1, bool flat) {
    const WStyle *S = &L->st;
    if (S->overlay == OV_NONE) return;
    float span = clampf(fabsf(a1 - a0) / 0.35f, 0.45f, 1.0f); /* (a dagger carries less than a greatsword) */
    int n = (int)((3 + L->rarity * 2) * span + 0.5f);
    float big = (1.0f + 0.15f * L->rarity) * (0.55f + 0.45f * span);
    float t = g_wtime;
    for (int i = 0; i < n; i++) {
        float h1 = hashf((float)(S->h & 0xFFFF) * 0.013f + i * 7.13f), h2 = hashf(h1 * 91.7f + i), h3 = hashf(h2 * 37.1f + 0.5f);
        float k = (i + 0.3f + 0.4f * h1) / n;
        float a = a0 + (a1 - a0) * k, w = lerpf(w0, w1, k);
        float side = h2 < 0.5f ? -1.0f : 1.0f;
        float ang = h2 * 2.0f * PI_F;
        vec3 out = flat ? v3_scale(f->v, side) : v3_add(v3_scale(f->v, cosf(ang)), v3_scale(f->w, sinf(ang)));
        vec3 base = flat ? P(f, a, side * w * 0.8f, (h3 - 0.5f) * 0.004f) : P(f, a, cosf(ang) * w, sinf(ang) * w);
        switch (S->overlay) {
        case OV_FROST: { /* a cluster of two or three crystals */
            Mat ice = MT(L->ice, 1.5f);
            int m = 2 + (h3 > 0.6f);
            for (int c = 0; c < m; c++) {
                float hc = hashf(h1 * 13.0f + c * 5.3f);
                vec3 d = v3_norm(v3_add(v3_add(out, v3_scale(f->u, (hc - 0.45f) * 1.4f)), v3_scale(flat ? f->w : f->up, (hashf(hc * 7.0f) - 0.5f) * 1.2f)));
                float len = (0.022f + 0.03f * hc) * big * f->s, r = (0.0055f + 0.004f * hc) * big * f->s;
                WF g = *f;
                g.o = base;
                g.u = d;
                g.v = v3_norm(v3_cross(d, fabsf(v3_dot(d, f->w)) < 0.9f ? f->w : f->v));
                g.w = v3_cross(g.u, g.v);
                g.s = 1.0f;
                tube(&g, -0.004f, 0, 0, len * 0.7f, 0, 0, r * 0.8f, r, 6, ice);
                tube(&g, len * 0.7f, 0, 0, len, 0, 0, r, 0.0f, 6, MT(v3_lerp(L->ice, v3(1, 1, 1), 0.3f), 1.6f));
                if (L->rarity >= RAR_EPIC) tube(&g, 0.0f, 0, 0, len * 0.6f, 0, 0, r * 0.35f, r * 0.35f, 4, ME(v3(0.5f, 0.8f, 1.1f))); /* a cold light inside */
            }
            break;
        }
        case OV_FIRE: { /* flames licking up from it, flickering */
            float fl = 0.6f + 0.4f * sinf(t * (9.0f + h1 * 5.0f) + i * 2.1f);
            vec3 up = v3_norm(v3_add(f->up, v3_scale(out, 0.35f)));
            vec3 col = v3_lerp(v3(1.2f, 0.35f, 0.05f), v3(1.3f, 0.95f, 0.35f), h3);
            batch3d_spike(base, up, 0.012f * big * f->s, (0.03f + 0.03f * fl) * big * f->s, col, true);
            batch3d_spike(v3_add(base, v3_scale(up, 0.008f * f->s)), up, 0.006f * big * f->s, (0.02f + 0.03f * fl) * big * f->s, v3(1.4f, 1.15f, 0.6f), true);
            break;
        }
        case OV_SHOCK: { /* arcs that jump now and then */
            float tick = floorf(t * 14.0f + h1 * 10.0f);
            if (hashf(tick + i * 3.3f) < 0.55f) break;
            vec3 p1 = v3_add(base, v3_scale(v3_add(out, v3_scale(f->u, hashf(tick + i) - 0.5f)), 0.03f * f->s));
            vec3 mid = v3_add(v3_scale(v3_add(base, p1), 0.5f), v3_scale(f->u, (hashf(tick * 1.3f + i) - 0.5f) * 0.02f * f->s));
            WF g = *f;
            g.o = v3(0, 0, 0);
            g.s = 1.0f;
            g.u = v3(1, 0, 0); g.v = v3(0, 1, 0); g.w = v3(0, 0, 1);
            SEG(&g, base.x, base.y, base.z, mid.x, mid.y, mid.z, 0.0016f * f->s, v3(0.85f, 0.9f, 1.4f), true);
            SEG(&g, mid.x, mid.y, mid.z, p1.x, p1.y, p1.z, 0.0016f * f->s, v3(0.85f, 0.9f, 1.4f), true);
            break;
        }
        case OV_VENOM: { /* a drop gathering under it, falling, gathering again */
            float ph = fmodf(t * (0.35f + h1 * 0.3f) + h2, 1.0f);
            vec3 dn = v3_scale(f->up, -1.0f);
            vec3 at = v3_add(base, v3_scale(dn, ph < 0.7f ? 0.004f * f->s : (ph - 0.7f) * (ph - 0.7f) * 1.8f * f->s));
            float r = (ph < 0.7f ? 0.004f + 0.004f * ph / 0.7f : 0.0075f) * f->s;
            batch3d_octa(at, r, r * 1.4f, 0.0f, v3(0.3f, 0.9f, 0.25f), true);
            break;
        }
        case OV_BLOOD: { /* thorns growing out of it */
            vec3 d = v3_norm(v3_sub(out, v3_scale(f->u, 0.5f + h3 * 0.5f)));
            batch3d_spike(base, d, 0.006f * big * f->s, (0.02f + 0.018f * h3) * big * f->s, v3(0.32f * g_wlight.x, 0.05f * g_wlight.y, 0.07f * g_wlight.z), false);
            break;
        }
        }
    }
    if (S->overlay == OV_FROST) { /* icicles hanging from under the span's start */
        int m = (int)((2 + L->rarity) * span + 0.5f);
        for (int i = 0; i < m; i++) {
            float h = hashf((float)(S->h & 0xFFF) + i * 4.1f);
            float a = a0 + (a1 - a0) * (0.05f + 0.5f * (i + h) / m);
            float w = lerpf(w0, w1, (a - a0) / fmaxf(1e-5f, a1 - a0));
            vec3 base = P(f, a, flat ? -w * 0.6f : 0, flat ? 0 : -w);
            vec3 dn = v3_scale(f->up, -1.0f);
            float len = (0.025f + 0.035f * h) * big * f->s;
            WF g = *f;
            g.o = base; g.u = dn; g.s = 1.0f;
            g.v = v3_norm(v3_cross(dn, fabsf(dn.x) < 0.9f ? v3(1, 0, 0) : v3(0, 0, 1)));
            g.w = v3_cross(g.u, g.v);
            tube(&g, 0, 0, 0, len, 0, 0, 0.005f * f->s, 0.0f, 5, MT(L->ice, 1.5f));
        }
    }
    if (S->overlay == OV_SHOCK) { /* a copper coil wound round the start */
        float pt[25][3], r[25];
        float span = (a1 - a0) * 0.28f, rr = (flat ? w0 * 0.55f : w0) + 0.006f;
        for (int i = 0; i < 25; i++) {
            float k = i / 24.0f, ang = k * 2.0f * PI_F * 5.0f;
            pt[i][0] = a0 + k * span; pt[i][1] = cosf(ang) * rr; pt[i][2] = sinf(ang) * (flat ? fmaxf(rr * 0.45f, 0.012f) : rr);
            r[i] = 0.0024f;
        }
        tube_path(f, (const float (*)[3])pt, r, 25, 4, MT(L->ice, 1.1f));
    }
}

static void model_melee(const WF *f, const WLook *L, int base, float time, bool fx, float seed) {
    Mat wood = MT(L->wood, 0.12f), metal = MT(L->metal, 0.8f), edge = MT(L->edge, 1.3f), acc = MT(L->accent, 0.9f);
    Mat dark = MT(v3_scale(L->metal, 0.7f), 0.6f);
    switch (base) {
    case WB_DAGGER:
        hilt(f, L, 0.042f, 0.042f, 0.008f, 0.016f);
        blade(f, L, 0.058f, 0.24f, 0.022f, 0.0065f, BL_DAGGER);
        overlay(f, L, 0.07f, 0.24f, 0.02f, 0.01f, true);
        if (fx) weapon_motes(f, L, 0.08f, 0.28f, time, seed);
        break;
    case WB_SWORD:
    default:
        hilt(f, L, 0.058f, 0.1f, 0.018f, 0.022f);
        blade(f, L, 0.078f, 0.52f, 0.029f, 0.0072f, BL_SWORD);
        overlay(f, L, 0.1f, 0.48f, 0.028f, 0.018f, true);
        if (fx) weapon_motes(f, L, 0.1f, 0.58f, time, seed);
        break;
    case WB_GREATSWORD:
        hilt(f, L, 0.11f, 0.17f, 0.03f, 0.026f);
        for (int s = -1; s <= 1; s += 2) /* side rings on the guard */
            tube(f, 0.135f, s * 0.045f, -0.004f, 0.135f, s * 0.045f, 0.004f, 0.022f, 0.022f, 8, acc);
        blade(f, L, 0.13f, 0.82f, 0.044f, 0.0085f, BL_GREAT);
        overlay(f, L, 0.15f, 0.75f, 0.042f, 0.03f, true);
        if (fx) weapon_motes(f, L, 0.25f, 0.95f, time, seed);
        break;
    case WB_AXE: {
        /* a slightly curved haft, a bearded head with a wedge-thin bit and a small poll */
        const float hp[4][3] = { { -0.14f, 0, 0 }, { 0.1f, 0.006f, 0 }, { 0.35f, 0.008f, 0 }, { 0.56f, 0, 0 } };
        const float hr[4] = { 0.0165f, 0.015f, 0.016f, 0.0175f };
        tube_path(f, hp, hr, 4, 8, wood);
        haft(f, L, -0.16f, -0.14f, 0.019f, 0.017f, -0.12f, 0.02f, wood); /* the grip and a swelled butt */
        tube(f, -0.175f, 0, 0, -0.16f, 0, 0, 0.012f, 0.02f, 8, wood);
        const float eye[4][2] = { { 0.415f, -0.03f }, { 0.525f, -0.03f }, { 0.525f, 0.035f }, { 0.415f, 0.035f } };
        const float et[4] = { 0.023f, 0.023f, 0.021f, 0.021f };
        slab(f, eye, et, 4, (const float[2]){ 0.47f, 0.0f }, 0.023f, metal, dark, dark, 0);
        /* the style: a bearded bit; a crescent (waved); a second bit where the poll was (epic and
         * legendary, by the look); a long back spike (hooked) */
        const WStyle *S = &L->st;
        bool dbl = L->rarity == RAR_LEGENDARY || (L->rarity == RAR_EPIC && ((S->h >> 9) & 1));
        bool crescent = S->form == BF_WAVED || L->rarity == RAR_LEGENDARY;
        if (S->form == BF_HOOKED && !dbl)
            SP(f, 0.47f, -0.03f, 0, v3_norm(v3_sub(v3_scale(f->v, -1.0f), v3_scale(f->u, 0.35f))), 0.016f, 0.1f, L->edge, false);
        else if (!dbl) {
            const float poll[4][2] = { { 0.43f, -0.03f }, { 0.51f, -0.03f }, { 0.5f, -0.062f }, { 0.44f, -0.062f } };
            const float pt_[4] = { 0.02f, 0.02f, 0.017f, 0.017f };
            slab(f, poll, pt_, 4, (const float[2]){ 0.47f, -0.046f }, 0.019f, metal, dark, dark, 0);
        }
        if (S->studs) for (int k = -1; k <= 1; k += 2) O(f, 0.47f, 0, k * 0.024f, 0.007f, 0.006f, 0.0f, v3_scale(L->accent, 1.1f), false);
        for (int sd = 1; sd >= (dbl ? -1 : 1); sd -= 2) {
        WF fb = *f;
        if (sd < 0) fb.v = v3_scale(f->v, -1.0f);
        const WF *fa = &fb;
        float cs = crescent ? 1.12f : 1.0f, cu = crescent ? 0.035f : 0.0f; /* a crescent: wider, the horns further out */
        /* the bit: the cheeks, and a ground bevel along the curved edge from the horn to the beard */
        const float E[6][2] = { { 0.588f + cu, 0.168f * cs }, { 0.565f + cu * 0.5f, 0.19f * cs }, { 0.515f, 0.2f * cs }, { 0.455f, 0.196f * cs }, { 0.395f - cu * 0.5f, 0.172f * cs },
                                { 0.335f - cu, 0.142f * cs } };
        const float ctr[2] = { 0.47f, 0.1f };
        float I[6][2];
        for (int i = 0; i < 6; i++) { /* the bevel's inner line, a little in from the edge */
            float dx = ctr[0] - E[i][0], dy = ctr[1] - E[i][1], dl = sqrtf(dx * dx + dy * dy);
            I[i][0] = E[i][0] + dx / dl * 0.026f;
            I[i][1] = E[i][1] + dy / dl * 0.026f;
        }
        const float bit[11][2] = { { 0.415f, 0.034f }, { 0.525f, 0.034f }, { 0.51f, 0.08f }, { I[0][0], I[0][1] }, { I[1][0], I[1][1] }, { I[2][0], I[2][1] },
                                   { I[3][0], I[3][1] }, { I[4][0], I[4][1] }, { I[5][0], I[5][1] }, { 0.37f, 0.104f }, { 0.41f, 0.072f } };
        const float bt[11] = { 0.016f, 0.016f, 0.011f, 0.0058f, 0.0055f, 0.0055f, 0.0055f, 0.0055f, 0.0058f, 0.009f, 0.013f };
        slab(fa, bit, bt, 11, (const float[2]){ 0.47f, 0.11f }, 0.009f, metal, dark, dark, 0);
        for (int i = 0; i < 5; i++) {
            const float q[4][2] = { { I[i][0], I[i][1] }, { E[i][0], E[i][1] }, { E[i + 1][0], E[i + 1][1] }, { I[i + 1][0], I[i + 1][1] } };
            const float qt[4] = { 0.0056f, 0.0014f, 0.0014f, 0.0056f };
            slab(fa, q, qt, 4, (const float[2]){ (I[i][0] + E[i + 1][0]) * 0.5f, (I[i][1] + E[i + 1][1]) * 0.5f }, 0.0035f, edge, edge, edge, 0);
        }
        if (S->inlay == 2) /* a rune on each cheek */
            for (int fc = -1; fc <= 1; fc += 2) O(fa, 0.47f, 0.1f, fc * 0.0075f, 0.012f, 0.004f, 0.8f, L->glow, true);
        if (L->glow_amt > 0) {
            float gp[6][3], gr[6];
            for (int i = 0; i < 6; i++) { gp[i][0] = E[i][0]; gp[i][1] = E[i][1] + 0.003f; gp[i][2] = 0; gr[i] = 0.003f; }
            tube_path(fa, (const float (*)[3])gp, gr, 6, 4, ME(L->glow));
        }
        }
        tube(f, 0.528f, 0, 0, 0.545f, 0, 0, 0.012f, 0.008f, 6, dark); /* the wedge through the eye */
        overlay(f, L, 0.36f, 0.56f, 0.035f, 0.035f, false);
        if (fx) weapon_motes(f, L, 0.38f, 0.56f, time, seed);
        break;
    }
    case WB_MACE: {
        haft(f, L, -0.12f, 0.47f, 0.0145f, 0.016f, -0.12f, 0.03f, dark);
        tube(f, -0.135f, 0, 0, -0.12f, 0, 0, 0.012f, 0.021f, 8, acc);
        const float core[6][2] = { { 0.455f, 0.017f }, { 0.47f, 0.026f }, { 0.53f, 0.03f }, { 0.575f, 0.026f }, { 0.6f, 0.016f }, { 0.612f, 0.0f } };
        lathe(f, core, 6, 0, 0, 8, metal);
        int nfl = L->rarity == RAR_LEGENDARY ? 8 : L->rarity == RAR_RARE ? 7 : 6;
        for (int k = 0; k < nfl; k++) { /* flanges, each a thin plate with a sharpened rim */
            float ang = k * 2.0f * PI_F / nfl + 0.26f;
            if (L->rarity >= RAR_EPIC) { /* and a spike between each two */
                float sa = ang + PI_F / nfl;
                vec3 out = v3_add(v3_scale(f->v, cosf(sa)), v3_scale(f->w, sinf(sa)));
                SP(f, 0.53f, cosf(sa) * 0.026f, sinf(sa) * 0.026f, out, 0.008f, 0.05f, L->edge, false);
            }
            WF g = *f;
            g.v = v3_add(v3_scale(f->v, cosf(ang)), v3_scale(f->w, sinf(ang)));
            g.w = v3_cross(f->u, g.v);
            const float fl[5][2] = { { 0.46f, 0.02f }, { 0.485f, 0.064f }, { 0.55f, 0.07f }, { 0.592f, 0.05f }, { 0.605f, 0.015f } };
            const float ft[5] = { 0.0055f, 0.003f, 0.0025f, 0.003f, 0.0055f };
            slab(&g, fl, ft, 5, (const float[2]){ 0.53f, 0.035f }, 0.0055f, metal, dark, edge, 0x6u);
        }
        tube(f, 0.44f, 0, 0, 0.455f, 0, 0, 0.02f, 0.02f, 8, acc); /* a collar */
        if (L->st.studs) for (int k = 0; k < 4; k++) O(f, 0.448f, cosf(k * PI_F * 0.5f) * 0.02f, sinf(k * PI_F * 0.5f) * 0.02f, 0.005f, 0.005f, 0, v3_scale(L->accent, 1.1f), false);
        if (L->glow_amt > 0) tube(f, 0.52f, 0, 0, 0.535f, 0, 0, 0.0315f, 0.0315f, 8, ME(L->glow)); /* a glowing band */
        overlay(f, L, 0.44f, 0.6f, 0.05f, 0.04f, false);
        if (fx) weapon_motes(f, L, 0.45f, 0.6f, time, seed);
        break;
    }
    case WB_WHIP: {
        /* a short handle with a knob, and a braided lash tapering to the cracker */
        haft(f, L, -0.01f, 0.15f, 0.016f, 0.014f, 0.0f, 0.13f, wood);
        const float kn[4][2] = { { -0.03f, 0.0f }, { -0.025f, 0.016f }, { -0.012f, 0.02f }, { -0.002f, 0.015f } };
        lathe(f, kn, 4, 0, 0, 8, acc);
        float pt[14][3], r[14];
        pt[0][0] = 0.15f; pt[0][1] = 0; pt[0][2] = 0; r[0] = 0.013f;
        for (int i = 1; i < 14; i++) {
            float k = i / 13.0f;
            pt[i][0] = 0.15f + k * 0.62f;
            pt[i][1] = sinf(time * 3.0f + k * 4.0f) * 0.07f * k + k * k * 0.18f;
            pt[i][2] = -k * k * 0.3f;
            r[i] = 0.012f * (1.0f - 0.8f * k);
        }
        tube_path(f, (const float (*)[3])pt, r, 14, 6, MT(v3_scale(L->grip, 1.25f), 0.2f));
        if (L->glow_amt > 0) O(f, pt[13][0], pt[13][1], pt[13][2], 0.008f, 0.014f, time * 3.0f, L->glow, true);
        overlay(f, L, 0.0f, 0.16f, 0.016f, 0.016f, false);
        if (fx) weapon_motes(f, L, 0.5f, 0.95f, time, seed);
        break;
    }
    case WB_FLAIL: {
        haft(f, L, -0.06f, 0.34f, 0.016f, 0.0165f, -0.05f, 0.07f, wood);
        tube(f, 0.33f, 0, 0, 0.355f, 0, 0, 0.02f, 0.016f, 8, acc); /* the cap and its eye */
        tube(f, 0.36f, 0, -0.008f, 0.36f, 0, 0.008f, 0.012f, 0.012f, 6, acc);
        vec3 ball = v3(0.56f, sinf(time * 4.0f) * 0.05f, -0.1f);
        float pa = 0.36f, pb = 0, pc = 0;
        for (int k = 1; k <= 5; k++) { /* oval links, each turned a quarter from the last */
            float t = k / 5.0f;
            float a = 0.36f + (ball.x - 0.07f - 0.36f) * t, b = ball.y * t, c = ball.z * t * t;
            float ma = (pa + a) * 0.5f, mb = (pb + b) * 0.5f, mc = (pc + c) * 0.5f;
            if (k & 1) tube(f, ma, mb - 0.009f, mc, ma, mb + 0.009f, mc, 0.009f, 0.009f, 6, dark);
            else tube(f, ma, mb, mc - 0.009f, ma, mb, mc + 0.009f, 0.009f, 0.009f, 6, dark);
            pa = a; pb = b; pc = c;
        }
        float sph[7][2];
        for (int i = 0; i < 7; i++) {
            float th = i * PI_F / 6.0f;
            sph[i][0] = ball.x - cosf(th) * 0.062f;
            sph[i][1] = sinf(th) * 0.062f;
        }
        lathe(f, (const float (*)[2])sph, 7, ball.y, ball.z, 10, metal);
        for (int k = 0; k < 6; k++) {
            vec3 d = k < 2 ? f->u : k < 4 ? f->v : f->w;
            if (k & 1) d = v3_scale(d, -1.0f);
            SP(f, ball.x, ball.y, ball.z, d, 0.012f, 0.085f, L->edge, false);
        }
        if (L->glow_amt > 0) O(f, ball.x, ball.y, ball.z, 0.03f, 0.066f, -time * 2.0f, L->glow, true);
        overlay(f, L, 0.0f, 0.33f, 0.018f, 0.018f, false);
        if (fx) weapon_motes(f, L, 0.45f, 0.6f, time, seed);
        break;
    }
    case WB_SCYTHE: {
        /* the snath, gently curved, with two nibs for the hands; the blade sweeps back from the heel */
        const float sp[4][3] = { { -0.08f, 0, 0 }, { 0.35f, 0.025f, 0 }, { 0.75f, 0.02f, 0 }, { 1.12f, 0, 0 } };
        const float sr[4] = { 0.016f, 0.015f, 0.0145f, 0.016f };
        tube_path(f, sp, sr, 4, 8, wood);
        tube(f, 0.3f, 0.02f, 0, 0.3f, 0.09f, 0, 0.012f, 0.011f, 6, wood);
        tube(f, 0.62f, 0.02f, 0, 0.62f, 0.085f, 0, 0.011f, 0.01f, 6, wood);
        tube(f, 1.08f, 0, 0, 1.12f, 0, 0, 0.022f, 0.022f, 8, acc); /* the ring holding the tang */
        float pa = 1.12f, pb = 0.0f, pw = 0.034f;
        for (int i = 1; i <= 9; i++) {
            float k = i / 9.0f;
            float a = 1.12f - 0.27f * k * k, b = -0.6f * k, w = 0.034f * (1.0f - k * 0.85f);
            /* each piece: the thick back (toward +a) and the thin edge (toward -a) */
            const float q[4][2] = { { pa + pw * 0.3f, pb }, { a + w * 0.3f, b }, { a - w, b - 0.003f }, { pa - pw, pb - 0.003f } };
            const float qt[4] = { 0.0055f, 0.0055f * (1.0f - k * 0.5f), 0.0012f, 0.0012f };
            slab(f, q, qt, 4, (const float[2]){ (pa + a) * 0.5f - (pw + w) * 0.25f, (pb + b) * 0.5f }, 0.004f, metal, dark, edge, 0x4u);
            if (L->glow_amt > 0) SEG(f, pa - pw - 0.002f, pb, 0, a - w - 0.002f, b, 0, 0.003f, L->glow, true);
            pa = a; pb = b; pw = w;
        }
        overlay(f, L, 0.75f, 1.1f, 0.017f, 0.017f, false);
        if (fx) weapon_motes(f, L, 0.9f, 1.12f, time, seed);
        break;
    }
    case WB_RAPIER: {
        const float gp[3][3] = { { -0.055f, 0, 0 }, { 0, 0, 0 }, { 0.05f, 0, 0 } };
        const float gr[3] = { 0.013f, 0.0145f, 0.013f };
        tube_path(f, gp, gr, 3, 8, MT(L->grip, 0.15f));
        for (float a = -0.045f; a < 0.045f; a += 0.012f) tube(f, a, 0, 0, a + 0.004f, 0, 0, 0.0152f, 0.0152f, 8, MT(L->accent, 1.0f)); /* wire */
        const float cup[5][2] = { { 0.052f, 0.012f }, { 0.058f, 0.032f }, { 0.068f, 0.046f }, { 0.08f, 0.05f }, { 0.084f, 0.0f } };
        lathe(f, cup, 5, 0, 0, 12, acc);
        const float kb[5][3] = { { 0.06f, 0, 0.045f }, { 0.035f, 0, 0.06f }, { -0.01f, 0, 0.058f }, { -0.05f, 0, 0.04f }, { -0.062f, 0, 0.014f } };
        const float kr[5] = { 0.005f, 0.0045f, 0.0045f, 0.0045f, 0.005f };
        tube_path(f, kb, kr, 5, 6, acc); /* the knuckle bow */
        tube(f, 0.085f, -0.07f, 0, 0.085f, 0.07f, 0, 0.0055f, 0.0055f, 6, acc); /* quillons */
        const float pm[5][2] = { { -0.056f, 0.012f }, { -0.062f, 0.02f }, { -0.075f, 0.021f }, { -0.086f, 0.014f }, { -0.092f, 0.0f } };
        lathe(f, pm, 5, 0, 0, 8, acc);
        blade(f, L, 0.084f, 0.74f, 0.011f, 0.0055f, BL_RAPIER);
        overlay(f, L, 0.1f, 0.5f, 0.011f, 0.008f, true);
        if (fx) weapon_motes(f, L, 0.12f, 0.8f, time, seed);
        break;
    }
    case WB_SPEAR:
        haft(f, L, -0.02f, 0.86f, 0.0135f, 0.0145f, -0.02f, 0.1f, wood);
        tube(f, -0.06f, 0, 0, -0.02f, 0, 0, 0.004f, 0.0155f, 8, dark); /* the butt spike */
        {
            const float sock[4][2] = { { 0.84f, 0.0165f }, { 0.88f, 0.016f }, { 0.92f, 0.012f }, { 0.935f, 0.009f } };
            lathe(f, sock, 4, 0, 0, 8, dark);
        }
        for (int k = 0; k < 3; k++) tube(f, 0.79f + k * 0.012f, 0, 0, 0.795f + k * 0.012f, 0, 0, 0.0165f, 0.0165f, 8, acc); /* binding */
        blade(f, L, 0.92f, 0.2f, 0.034f, 0.0075f, BL_LEAF);
        overlay(f, L, 0.6f, 1.0f, 0.016f, 0.028f, false);
        if (fx) weapon_motes(f, L, 0.88f, 1.1f, time, seed);
        break;
    }
}

/* An arrow from p0 along aim: a round shaft, a bodkin head and three vanes. */
static void arrow_model(const WF *f, const WLook *L, vec3 p0, vec3 aim, float len) {
    WF a = *f;
    a.o = p0;
    a.u = aim;
    a.v = v3_norm(v3_cross(aim, fabsf(aim.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0)));
    a.w = v3_cross(a.u, a.v);
    tube(&a, 0.0f, 0, 0, len - 0.04f, 0, 0, 0.0042f, 0.0042f, 6, MT(v3(0.55f * g_wlight.x, 0.45f * g_wlight.y, 0.3f * g_wlight.z), 0.1f));
    bool magic = L->element != EL_NONE || L->vampiric;
    const float head[4][2] = { { len - 0.045f, 0.0065f }, { len - 0.03f, 0.0075f }, { len - 0.005f, 0.003f }, { len + 0.012f, 0.0f } };
    lathe(&a, head, 4, 0, 0, 4, magic ? ME(L->glow) : MT(L->metal, 1.0f));
    vec3 fl = L->glow_amt > 0 ? L->glow : v3(0.78f * g_wlight.x, 0.72f * g_wlight.y, 0.62f * g_wlight.z);
    for (int k = 0; k < 3; k++) { /* three vanes, a third of a turn apart */
        float ang = k * 2.0f * PI_F / 3.0f;
        WF g = a;
        g.v = v3_add(v3_scale(a.v, cosf(ang)), v3_scale(a.w, sinf(ang)));
        g.w = v3_cross(a.u, g.v);
        const float vane[4][2] = { { 0.012f, 0.004f }, { 0.075f, 0.004f }, { 0.06f, 0.016f }, { 0.016f, 0.016f } };
        const float vt[4] = { 0.0008f, 0.0008f, 0.0008f, 0.0008f };
        slab(&g, vane, vt, 4, (const float[2]){ 0.04f, 0.01f }, 0.0008f, L->glow_amt > 0 ? ME(fl) : MT(fl, 0.05f), MT(fl, 0.05f), MT(fl, 0.05f), 0);
    }
    tube(&a, -0.006f, 0, 0, 0.004f, 0, 0, 0.0055f, 0.005f, 6, MT(L->accent, 0.4f)); /* the nock */
}

/* Bows: u along the limbs (up), w toward the archer (the string side). The grip is in the
 * hand (w = 0) and the limbs curve back toward the archer, so the bow bulges away from them;
 * the string runs straight from tip to tip and is pulled back by draw 0..1. arrow: one rests
 * on the string. */
static float bow_curve(int base, float t, float bend) {
    float c = bend * t * t;
    if (base == WB_SHORTBOW && fabsf(t) > 0.75f) c -= (fabsf(t) - 0.75f) * 0.3f; /* recurved tips flick forward */
    return c;
}

static void model_bow(const WF *f, const WLook *L, int base, float draw, float time, bool fx, float seed, bool arrow) {
    float half = base == WB_LONGBOW ? 0.44f : 0.28f;
    float bend = base == WB_LONGBOW ? 0.07f : 0.1f;
    float thick = base == WB_LONGBOW ? 0.0145f : 0.017f;
    Mat wood = MT(L->wood, 0.18f), acc = MT(L->accent, 0.8f);
    /* the limbs: thick at the handle, tapering to the tips */
    float pt[17][3], r[17];
    for (int i = 0; i <= 16; i++) {
        float t = (i / 16.0f - 0.5f) * 2.0f;
        pt[i][0] = t * half; pt[i][1] = 0; pt[i][2] = bow_curve(base, t, bend);
        r[i] = thick * (1.0f - 0.55f * fabsf(t));
    }
    tube_path(f, (const float (*)[3])pt, r, 17, 6, wood);
    float tipc = bow_curve(base, 1.0f, bend);
    for (int s = -1; s <= 1; s += 2) { /* the tips, by tier: plain, horn, gilded curls, blades, wings */
        int tp = L->st.tips;
        Mat tipm = L->glow_amt > 0 && tp < 3 ? ME(L->glow) : MT(v3_scale(L->accent, 0.9f), 0.6f);
        if (tp >= 1) tube(f, s * (half - 0.015f), 0, tipc, s * (half + 0.012f), 0, tipc + (base == WB_SHORTBOW ? -0.006f : 0.0f), thick * 0.55f, 0.002f, 6, tipm);
        if (tp == 2) { /* a gilded curl turning back toward the bow */
            const float cp[4][3] = { { s * (half - 0.01f), 0, tipc }, { s * (half + 0.012f), 0, tipc - 0.018f }, { s * (half + 0.0f), 0, tipc - 0.03f },
                                     { s * (half - 0.012f), 0, tipc - 0.022f } };
            const float cr[4] = { 0.004f, 0.0035f, 0.003f, 0.0025f };
            tube_path(f, cp, cr, 4, 6, MT(L->accent, 1.0f));
        } else if (tp >= 3) { /* a small blade (epic) or a feathered wing (legendary) off each tip, away from the archer */
            WF g = *f;
            g.v = v3_scale(f->w, -1.0f);
            g.w = f->v;
            float a = s * (half - 0.02f), l = tp == 4 ? 0.09f : 0.06f;
            const float bp[4][2] = { { a, -tipc + 0.004f }, { a + s * l * 0.5f, -tipc + l * 0.55f }, { a + s * l, -tipc + l * 0.25f }, { a - s * 0.03f, -tipc + 0.004f } };
            const float bt[4] = { 0.004f, 0.0015f, 0.0012f, 0.004f };
            slab(&g, bp, bt, 4, (const float[2]){ a + s * l * 0.35f, -tipc + l * 0.2f }, 0.003f, MT(tp == 4 ? L->accent : L->metal, 1.0f),
                 MT(L->metal, 0.8f), MT(L->edge, 1.3f), 0x3u);
            if (L->glow_amt > 0) O(f, s * (half - 0.005f), 0, tipc, 0.007f, 0.01f, g_wtime * 2.0f, L->glow, true);
        }
    }
    /* the handle: wrapped, with an arrow shelf */
    Mat grip = MT(L->grip, 0.15f), wrap = MT(v3_scale(L->grip, 0.62f), 0.1f);
    tube(f, -0.06f, 0, 0, 0.06f, 0, 0, thick * 1.35f, thick * 1.35f, 8, grip);
    for (float a = -0.05f; a < 0.05f; a += 0.02f) tube(f, a, 0, 0, a + 0.006f, 0, 0, thick * 1.45f, thick * 1.45f, 8, wrap);
    tube(f, 0.06f, 0, 0, 0.072f, 0, 0, thick * 1.45f, thick * 1.2f, 8, acc);
    if (L->glow_amt > 0) O(f, 0.0f, thick * 1.5f, 0, 0.009f, 0.012f, time * 2.0f, L->glow, true);
    float nock = tipc + 0.16f * draw; /* straight string at rest */
    vec3 sc = L->glow_amt > 0 ? v3_lerp(v3(0.8f, 0.8f, 0.75f), v3_scale(L->glow, 1.0f / fmaxf(0.3f, L->glow_amt)), 0.5f) : v3(0.8f, 0.8f, 0.75f);
    SEG(f, half, 0, tipc, 0, 0, nock, 0.0022f, sc, L->glow_amt > 0);
    SEG(f, -half, 0, tipc, 0, 0, nock, 0.0022f, sc, L->glow_amt > 0);
    if (arrow) {
        vec3 aim = v3_len(f->aim) > 0.1f ? v3_norm(f->aim) : v3_scale(f->w, -1.0f);
        arrow_model(f, L, P(f, 0, 0, nock), aim, 0.62f);
    }
    overlay(f, L, -half * 0.8f, half * 0.8f, thick, thick * 0.7f, false);
    if (fx) weapon_motes(f, L, -half * 0.9f, half * 0.9f, time, seed);
}

/* Crossbow: u forward along the stock, v right, w up. draw 0..1 (string back). */
static void model_crossbow(const WF *f, const WLook *L, float draw, float time, bool fx, float seed) {
    Mat wood = MT(L->wood, 0.2f), metal = MT(L->metal, 0.85f), acc = MT(L->accent, 0.8f), dark = MT(v3_scale(L->metal, 0.7f), 0.6f);
    /* the stock seen from the side: a tiller widening into the butt */
    WF g = *f; /* a frame with the profile in (u, w) */
    g.v = f->w;
    g.w = v3_scale(f->v, -1.0f);
    const float st[8][2] = { { 0.25f, -0.01f }, { 0.25f, 0.028f }, { -0.05f, 0.03f }, { -0.18f, 0.02f },
                             { -0.31f, 0.03f }, { -0.31f, -0.07f }, { -0.2f, -0.045f }, { -0.07f, -0.02f } };
    const float stt[8] = { 0.02f, 0.02f, 0.022f, 0.022f, 0.026f, 0.026f, 0.024f, 0.022f };
    slab(&g, st, stt, 8, (const float[2]){ -0.08f, 0.0f }, 0.024f, wood, wood, wood, 0);
    const float gr[4][2] = { { -0.04f, -0.02f }, { -0.01f, -0.02f }, { -0.03f, -0.1f }, { -0.065f, -0.1f } };
    const float grt[4] = { 0.016f, 0.016f, 0.015f, 0.015f };
    slab(&g, gr, grt, 4, (const float[2]){ -0.035f, -0.06f }, 0.016f, MT(L->grip, 0.15f), MT(L->grip, 0.15f), MT(L->grip, 0.15f), 0);
    B(f, 0.06f, 0, 0.031f, 0.19f, 0.004f, 0.002f, v3_scale(L->wood, 0.6f), false); /* the groove for the bolt */
    /* the steel prod: two arms tapering and swept forward, and the stirrup in front */
    for (int k = -1; k <= 1; k += 2) {
        const float pp[4][3] = { { 0.235f, 0, 0.005f }, { 0.25f, 0.1f * k, 0.005f }, { 0.235f, 0.2f * k, 0.005f }, { 0.2f, 0.28f * k, 0.005f } };
        const float pr[4] = { 0.011f, 0.0095f, 0.008f, 0.0065f };
        tube_path(f, pp, pr, 4, 6, metal);
        if (L->glow_amt > 0) O(f, 0.2f, 0.28f * k, 0.005f, 0.008f, 0.012f, time * 2.0f, L->glow, true);
        float nut = 0.22f - 0.24f * draw;
        SEG(f, 0.2f, 0.28f * k, 0.005f, nut, 0, 0.032f, 0.0025f, v3(0.8f, 0.8f, 0.75f), false);
    }
    const float stir[5][3] = { { 0.25f, -0.025f, 0 }, { 0.3f, -0.03f, 0 }, { 0.32f, 0, 0 }, { 0.3f, 0.03f, 0 }, { 0.25f, 0.025f, 0 } };
    const float srr[5] = { 0.005f, 0.005f, 0.005f, 0.005f, 0.005f };
    tube_path(f, stir, srr, 5, 6, dark);
    tube(f, 0.225f, 0, -0.012f, 0.225f, 0, 0.022f, 0.016f, 0.016f, 8, acc); /* the bindings of the prod */
    tube(f, -0.02f, 0, 0.026f, -0.02f, 0, 0.042f, 0.011f, 0.011f, 8, acc); /* the nut */
    tube(f, -0.06f, 0, -0.02f, -0.11f, 0, -0.05f, 0.004f, 0.004f, 6, dark); /* the trigger lever */
    if (draw > 0.6f) {
        WF bf = *f;
        bf.o = P(f, -0.02f, 0, 0.037f);
        tube(&bf, 0, 0, 0, 0.27f, 0, 0, 0.0055f, 0.0055f, 6, MT(v3_scale(L->wood, 0.9f), 0.1f));
        const float hd[3][2] = { { 0.265f, 0.0085f }, { 0.28f, 0.007f }, { 0.31f, 0.0f } };
        lathe(&bf, hd, 3, 0, 0, 4, L->element != EL_NONE || L->vampiric ? ME(L->glow) : MT(L->edge, 1.2f));
    }
    if (L->glow_amt > 0) B(f, 0.05f, 0.021f, 0.0f, 0.15f, 0.002f, 0.006f, L->glow, true); /* inlaid line */
    overlay(f, L, -0.2f, 0.25f, 0.025f, 0.025f, false);
    if (fx) weapon_motes(f, L, 0.1f, 0.25f, time, seed);
}

static vec3 gem_color(const WLook *L, vec3 fallback);

/* The lost ranged arts: u forward, v right, w up (like the crossbow). */
static void model_lost_ranged(const WF *f, const WLook *L, int base, float draw, float time, bool fx, float seed) {
    Mat metal = MT(L->metal, 0.85f), acc = MT(L->accent, 0.8f), dark = MT(v3_scale(L->metal, 0.65f), 0.7f);
    switch (base) {
    case WB_SLING: {
        /* two braided cords from the hand to a leather cradle with a stone, swinging */
        float sw = sinf(time * 9.0f * (0.3f + draw)) * 0.08f;
        vec3 pouch = v3(0.12f + sw, 0, -0.3f);
        Mat cord = MT(v3_scale(L->grip, 1.3f), 0.1f);
        tube(f, 0, 0.012f, 0, pouch.x, pouch.y + 0.024f, pouch.z + 0.01f, 0.0028f, 0.0028f, 4, cord);
        tube(f, 0, -0.012f, 0, pouch.x, pouch.y - 0.024f, pouch.z + 0.01f, 0.0028f, 0.0028f, 4, cord);
        const float cr[5][2] = { { pouch.x - 0.04f, 0.01f }, { pouch.x - 0.02f, 0.03f }, { pouch.x, 0.035f }, { pouch.x + 0.02f, 0.03f }, { pouch.x + 0.04f, 0.01f } };
        lathe(f, cr, 5, pouch.y, pouch.z, 8, MT(v3_scale(L->grip, 1.6f), 0.12f));
        O(f, pouch.x, pouch.y, pouch.z - 0.03f, 0.03f, 0.03f, time, L->glow_amt > 0 ? L->glow : v3_scale(L->metal, 0.8f), L->glow_amt > 0);
        tube(f, -0.01f, 0, -0.012f, -0.01f, 0, 0.012f, 0.016f, 0.016f, 8, acc); /* the loop round a finger */
        overlay(f, L, -0.02f, 0.05f, 0.015f, 0.015f, false);
        if (fx) weapon_motes(f, L, -0.05f, 0.1f, time, seed);
        break;
    }
    case WB_CHAKRAM: {
        /* a flat steel ring, sharpened on the outside, with a wrapped grip across it */
        float spin = time * 0.6f;
        for (int k = 0; k < 16; k++) {
            float a0 = spin + k * 2.0f * PI_F / 16.0f, a1 = a0 + 2.0f * PI_F / 16.0f;
            const float q[4][2] = { { 0.12f + cosf(a0) * 0.1f, sinf(a0) * 0.1f }, { 0.12f + cosf(a1) * 0.1f, sinf(a1) * 0.1f },
                                    { 0.12f + cosf(a1) * 0.135f, sinf(a1) * 0.135f }, { 0.12f + cosf(a0) * 0.135f, sinf(a0) * 0.135f } };
            const float qt[4] = { 0.006f, 0.006f, 0.0012f, 0.0012f };
            float am = (a0 + a1) * 0.5f;
            slab(f, q, qt, 4, (const float[2]){ 0.12f + cosf(am) * 0.115f, sinf(am) * 0.115f }, 0.005f, metal, dark, MT(L->edge, 1.3f), 0x4u);
        }
        tube(f, 0.12f, -0.1f, 0, 0.12f, 0.1f, 0, 0.011f, 0.011f, 8, MT(L->grip, 0.15f)); /* where the hand holds it */
        if (L->glow_amt > 0) O(f, 0.12f, 0, 0, 0.015f, 0.015f, time * 3.0f, L->glow, true);
        overlay(f, L, 0.02f, 0.22f, 0.012f, 0.012f, false);
        if (fx) weapon_motes(f, L, 0.02f, 0.24f, time, seed);
        break;
    }
    case WB_CANNON:
    default: {
        /* a cast barrel with a flared muzzle and reinforcing bands on a wooden tiller */
        const float br[8][2] = { { 0.04f, 0.0f }, { 0.045f, 0.044f }, { 0.08f, 0.046f }, { 0.12f, 0.04f },
                                 { 0.34f, 0.034f }, { 0.375f, 0.036f }, { 0.41f, 0.046f }, { 0.42f, 0.046f } };
        lathe(f, br, 8, 0, 0.02f, 10, MT(v3_scale(L->metal, 0.62f), 0.7f));
        tube(f, 0.42f, 0, 0.02f, 0.422f, 0, 0.02f, 0.03f, 0.03f, 10, ME(v3(0.03f, 0.03f, 0.03f))); /* the bore */
        for (int k = 0; k < 3; k++) tube(f, 0.13f + k * 0.08f, 0, 0.02f, 0.14f + k * 0.08f, 0, 0.02f, 0.041f - k * 0.002f, 0.041f - k * 0.002f, 10, acc);
        WF g = *f;
        g.v = f->w;
        g.w = v3_scale(f->v, -1.0f);
        const float tl[6][2] = { { 0.1f, -0.025f }, { 0.1f, 0.0f }, { -0.02f, 0.0f }, { -0.24f, -0.02f }, { -0.24f, -0.06f }, { -0.06f, -0.04f } };
        const float tt[6] = { 0.02f, 0.02f, 0.022f, 0.026f, 0.026f, 0.022f };
        slab(&g, tl, tt, 6, (const float[2]){ -0.06f, -0.025f }, 0.022f, MT(L->wood, 0.2f), MT(L->wood, 0.2f), MT(L->wood, 0.2f), 0);
        tube(f, -0.03f, 0, -0.045f, -0.06f, 0, -0.1f, 0.014f, 0.015f, 8, MT(L->grip, 0.15f));
        tube(f, 0.06f, 0, 0.062f, 0.06f, 0, 0.09f, 0.004f, 0.003f, 6, MT(v3(0.4f, 0.35f, 0.3f), 0.1f)); /* the touch hole's match */
        if (draw > 0.9f && fmodf(time * 6.0f, 1.0f) < 0.5f) O(f, 0.06f, 0, 0.095f, 0.006f, 0.008f, 0, v3(1.3f, 0.6f, 0.2f), true);
        if (L->glow_amt > 0) B(f, 0.25f, 0.035f, 0.02f, 0.12f, 0.002f, 0.006f, L->glow, true);
        overlay(f, L, 0.05f, 0.4f, 0.045f, 0.04f, false);
        if (fx) weapon_motes(f, L, 0.0f, 0.4f, time, seed);
        break;
    }
    }
}

/* The lost staff arts: u from the hand outward. */
static void model_lost_staff(const WF *f, const WLook *L, int base, float charge, vec3 gem_base, float time, bool fx, float seed) {
    vec3 gem = v3_scale(gem_color(L, gem_base), 0.75f + 0.25f * sinf(time * 6.0f) + charge);
    Mat acc = MT(L->accent, 0.9f);
    if (base == WB_TOME) {
        /* a book held open toward the foe: boards, a rounded spine, pages, corner fittings */
        Mat cover = MT(L->grip, 0.2f), page = MT(v3(0.85f * g_wlight.x, 0.8f * g_wlight.y, 0.68f * g_wlight.z), 0.05f);
        B(f, 0.1f, 0.0f, 0, 0.1f, 0.088f, 0.007f, L->grip, false);
        B(f, 0.1f, 0.0f, 0.012f, 0.094f, 0.082f, 0.006f, page.c, false);
        tube(f, 0.0f, -0.088f, 0.006f, 0.2f, -0.088f, 0.006f, 0.012f, 0.012f, 8, cover); /* the spine */
        for (int k = 0; k < 4; k++) B(f, 0.04f + k * 0.04f, 0.045f, 0.0185f, 0.012f, 0.025f, 0.0008f, v3(0.4f, 0.35f, 0.3f), false); /* lines */
        for (int k = 0; k < 2; k++) {
            float a = k ? 0.19f : 0.01f;
            SP(f, a, 0.08f, 0, k ? f->u : v3_scale(f->u, -1.0f), 0.012f, 0.012f, L->accent, false);
            B(f, a, 0.078f, 0, 0.012f, 0.012f, 0.009f, L->accent, false);
        }
        O(f, 0.1f, 0, 0.03f + 0.01f * sinf(time * 3.0f), 0.018f, 0.025f, time * 2.0f, gem, true); /* a sigil over the page */
        overlay(f, L, 0.02f, 0.18f, 0.09f, 0.09f, true);
        if (fx) weapon_motes(f, L, 0.02f, 0.2f, time, seed);
        return;
    }
    /* the censer: a chain and a pierced lantern of holy smoke */
    for (int k = 0; k < 8; k++) { /* chain links */
        float a = 0.02f + k * 0.034f;
        if (k & 1) tube(f, a, -0.006f, 0, a, 0.006f, 0, 0.006f, 0.006f, 6, MT(L->metal, 0.8f));
        else tube(f, a, 0, -0.006f, a, 0, 0.006f, 0.006f, 0.006f, 6, MT(L->metal, 0.8f));
    }
    const float body[7][2] = { { 0.3f, 0.0f }, { 0.31f, 0.02f }, { 0.33f, 0.045f }, { 0.37f, 0.056f }, { 0.41f, 0.045f }, { 0.43f, 0.02f }, { 0.44f, 0.0f } };
    lathe(f, body, 7, 0, 0, 10, MT(L->metal, 0.8f));
    tube(f, 0.365f, 0, 0, 0.375f, 0, 0, 0.059f, 0.059f, 10, acc);
    for (int k = 0; k < 6; k++) {
        float a = k * PI_F / 3.0f + time * 0.5f;
        O(f, 0.345f, cosf(a) * 0.049f, sinf(a) * 0.049f, 0.008f, 0.011f, 0, gem, true); /* light through the holes */
        O(f, 0.4f, cosf(a + 0.5f) * 0.049f, sinf(a + 0.5f) * 0.049f, 0.008f, 0.011f, 0, gem, true);
    }
    tube(f, 0.44f, 0, 0, 0.47f, 0, 0, 0.012f, 0.0f, 6, acc);
    float ph = fmodf(time * 0.8f, 1.0f);
    O(f, 0.3f - ph * 0.15f, sinf(time * 2.0f) * 0.02f, 0, 0.012f + ph * 0.02f, 0.012f + ph * 0.02f, time, v3(0.55f, 0.52f, 0.5f), false);
    overlay(f, L, 0.3f, 0.44f, 0.05f, 0.05f, false);
    if (fx) weapon_motes(f, L, 0.3f, 0.44f, time, seed);
}

/* Staves: u from the butt (o) to the head. charge: brighter right after a cast. */
static vec3 gem_color(const WLook *L, vec3 fallback) {
    if (L->vampiric) return v3(0.95f, 0.1f, 0.18f);
    if (L->element != EL_NONE) return element_color(L->element);
    return L->glow_amt > 0 ? v3_scale(L->glow, 1.0f / fmaxf(0.3f, L->glow_amt)) : fallback;
}

static void model_staff(const WF *f, const WLook *L, int base, float charge, vec3 gem_base, float time, bool fx, float seed) {
    vec3 gem = v3_scale(gem_color(L, gem_base), 0.75f + 0.25f * sinf(time * 6.0f) + charge);
    Mat wood = MT(L->wood, 0.15f), acc = MT(L->accent, 0.9f), metal = MT(L->metal, 0.85f);
    switch (base) {
    case WB_WAND: {
        const float wp[6][2] = { { 0.0f, 0.0f }, { 0.005f, 0.011f }, { 0.1f, 0.012f }, { 0.25f, 0.0085f }, { 0.31f, 0.007f }, { 0.32f, 0.0f } };
        lathe(f, wp, 6, 0, 0, 8, wood);
        for (int k = 0; k < 3; k++) tube(f, 0.12f + k * 0.06f, 0, 0, 0.128f + k * 0.06f, 0, 0, 0.0125f - k * 0.0012f, 0.0122f - k * 0.0012f, 8, acc);
        tube(f, 0.0f, 0, 0, 0.1f, 0, 0, 0.0135f, 0.0135f, 8, MT(L->grip, 0.15f));
        const float cl[4][2] = { { 0.31f, 0.008f }, { 0.33f, 0.013f }, { 0.345f, 0.011f }, { 0.35f, 0.0f } };
        lathe(f, cl, 4, 0, 0, 6, acc);
        O(f, 0.36f, 0, 0, 0.012f, 0.018f, time * 3.0f, gem, true);
        overlay(f, L, 0.1f, 0.33f, 0.012f, 0.008f, false);
        if (fx) weapon_motes(f, L, 0.2f, 0.36f, time, seed);
        break;
    }
    case WB_SCEPTER: {
        tube(f, 0.0f, 0, 0, 0.42f, 0, 0, 0.0115f, 0.013f, 8, acc); /* gilded shaft */
        tube(f, 0.0f, 0, 0, 0.12f, 0, 0, 0.0145f, 0.0145f, 8, MT(L->grip, 0.15f));
        const float knob[4][2] = { { -0.035f, 0.0f }, { -0.03f, 0.016f }, { -0.015f, 0.02f }, { 0.0f, 0.014f } };
        lathe(f, knob, 4, 0, 0, 8, acc);
        const float head[5][2] = { { 0.4f, 0.014f }, { 0.415f, 0.026f }, { 0.43f, 0.03f }, { 0.445f, 0.024f }, { 0.45f, 0.012f } };
        lathe(f, head, 5, 0, 0, 10, metal);
        for (int k = 0; k < 4; k++) { /* a crown of prongs around the orb */
            float ang = k * PI_F * 0.5f + 0.4f;
            float b = cosf(ang), c = sinf(ang);
            const float pp[3][3] = { { 0.44f, b * 0.022f, c * 0.022f }, { 0.5f, b * 0.05f, c * 0.05f }, { 0.55f, b * 0.035f, c * 0.035f } };
            const float pr[3] = { 0.006f, 0.005f, 0.002f };
            tube_path(f, pp, pr, 3, 6, acc);
        }
        O(f, 0.5f, 0, 0, 0.035f, 0.04f, time * 1.5f, gem, true);
        O(f, 0.5f, 0, 0, 0.02f, 0.055f, -time * 2.5f, v3_scale(v3(1, 1, 1), 0.6f + charge * 0.4f), true);
        overlay(f, L, 0.2f, 0.48f, 0.014f, 0.03f, false);
        if (fx) weapon_motes(f, L, 0.4f, 0.56f, time, seed);
        break;
    }
    case WB_STAFF:
    default: {
        /* a gnarled shaft (its bends from the seed of the shape, not of the world), knots,
         * a wrapped hand hold, an iron shoe, and a head of two curling prongs about the gem */
        float pt[9][3], r[9];
        for (int i = 0; i < 9; i++) {
            float k = i / 8.0f;
            pt[i][0] = 0.02f + k * 0.8f;
            pt[i][1] = sinf(k * 7.0f) * 0.008f;
            pt[i][2] = cosf(k * 5.0f + 1.0f) * 0.006f;
            r[i] = 0.016f + 0.004f * k + (i == 3 || i == 6 ? 0.004f : 0.0f);
        }
        tube_path(f, (const float (*)[3])pt, r, 9, 7, wood);
        tube(f, 0.25f, 0, 0, 0.37f, 0, 0, 0.0215f, 0.0215f, 8, MT(L->grip, 0.15f));
        for (float a = 0.26f; a < 0.36f; a += 0.02f) tube(f, a, 0, 0, a + 0.006f, 0, 0, 0.023f, 0.023f, 8, MT(v3_scale(L->grip, 0.62f), 0.1f));
        tube(f, -0.01f, 0, 0, 0.03f, 0, 0, 0.01f, 0.019f, 8, MT(v3_scale(L->metal, 0.7f), 0.6f)); /* iron shoe */
        /* the head, by tier: two prongs; a ring about the gem; a crescent; a claw; a halo with wings */
        int tp = L->st.tips;
        if (tp == 0 || tp == 1) {
            for (int s = -1; s <= 1; s += 2) {
                const float pp[5][3] = { { 0.8f, 0.0f, 0 }, { 0.86f, 0.045f * s, 0 }, { 0.92f, 0.05f * s, 0 }, { 0.97f, 0.025f * s, 0 }, { 0.985f, 0.005f * s, 0 } };
                const float pr[5] = { 0.013f, 0.011f, 0.0095f, 0.0075f, 0.004f };
                tube_path(f, pp, pr, 5, 6, wood);
            }
            if (tp == 1) { /* a ring of metal round the gem */
                float rp[13][3], rr[13];
                for (int i = 0; i < 13; i++) { float a = i * 2.0f * PI_F / 12.0f; rp[i][0] = 0.91f + cosf(a) * 0.05f; rp[i][1] = sinf(a) * 0.05f; rp[i][2] = 0; rr[i] = 0.0045f; }
                tube_path(f, (const float (*)[3])rp, rr, 13, 5, MT(L->accent, 1.0f));
            }
        } else if (tp == 2) { /* a crescent moon, horns up, the gem in its bowl */
            float cp[9][3], cr[9];
            for (int i = 0; i < 9; i++) {
                float a = PI_F * (0.1f + 0.8f * i / 8.0f);
                cp[i][0] = 0.93f - sinf(a) * 0.075f; cp[i][1] = cosf(a) * 0.085f; cp[i][2] = 0;
                cr[i] = 0.013f * sinf(a) + 0.002f;
            }
            tube_path(f, (const float (*)[3])cp, cr, 9, 6, MT(L->accent, 1.1f));
            tube(f, 0.8f, 0, 0, 0.86f, 0, 0, 0.016f, 0.01f, 8, wood);
        } else if (tp == 3) { /* a claw of three talons gripping the gem */
            for (int k = 0; k < 3; k++) {
                float a = k * 2.0f * PI_F / 3.0f;
                float b = cosf(a), c = sinf(a);
                const float pp[4][3] = { { 0.82f, b * 0.012f, c * 0.012f }, { 0.87f, b * 0.05f, c * 0.05f }, { 0.94f, b * 0.045f, c * 0.045f }, { 0.97f, b * 0.012f, c * 0.012f } };
                const float pr[4] = { 0.01f, 0.008f, 0.006f, 0.0015f };
                tube_path(f, pp, pr, 4, 6, MT(v3_scale(L->metal, 0.9f), 0.9f));
            }
        } else { /* a halo turning about a floating gem, and two small wings */
            float rp[17][3], rr[17];
            float sp = time * 0.7f;
            for (int i = 0; i < 17; i++) {
                float a = i * 2.0f * PI_F / 16.0f;
                rp[i][0] = 0.93f + cosf(a) * 0.065f; rp[i][1] = sinf(a) * 0.065f * cosf(sp); rp[i][2] = sinf(a) * 0.065f * sinf(sp);
                rr[i] = 0.004f;
            }
            tube_path(f, (const float (*)[3])rp, rr, 17, 4, MT(L->accent, 1.2f));
            for (int s = -1; s <= 1; s += 2) {
                const float wp[5][2] = { { 0.8f, s * 0.015f }, { 0.86f, s * 0.08f }, { 0.89f, s * 0.11f }, { 0.85f, s * 0.07f }, { 0.81f, s * 0.035f } };
                const float wt[5] = { 0.005f, 0.003f, 0.0015f, 0.003f, 0.005f };
                slab(f, wp, wt, 5, (const float[2]){ 0.835f, s * 0.05f }, 0.004f, MT(L->accent, 1.0f), MT(L->accent, 1.0f), MT(L->edge, 1.3f), 0x6u);
            }
        }
        tube(f, 0.8f, 0, 0, 0.82f, 0, 0, 0.022f, 0.022f, 8, acc);
        O(f, tp == 2 ? 0.905f : 0.91f, 0, 0, tp >= 3 ? 0.034f : 0.03f, tp >= 3 ? 0.05f : 0.045f, time * 2.0f, gem, true);
        overlay(f, L, 0.45f, 0.95f, 0.02f, 0.03f, false);
        if (fx) weapon_motes(f, L, 0.7f, 0.98f, time, seed);
        break;
    }
    }
}

/* ---------- drawing in the world ---------- */

/* Held pointing forward like a crossbow (not upright like a bow). */
static bool forward_held(int base) { return base == WB_CROSSBOW || base == WB_SLING || base == WB_CHAKRAM || base == WB_CANNON; }

static void model(const WF *f, const Item *it, vec3 light, float time, bool fx, float seed, float draw, float charge, vec3 gem) {
    WLook L = look_of(it, light, time);
    g_wlight = light;
    g_wtime = time;
    const WeaponBase *wb = &WEAPON_BASES[it->base < WB_COUNT ? it->base : WB_SWORD];
    if (wb->wclass == WPN_SWORD) model_melee(f, &L, it->base, time, fx, seed);
    else if (it->base == WB_CROSSBOW) model_crossbow(f, &L, draw, time, fx, seed);
    else if (it->base == WB_SLING || it->base == WB_CHAKRAM || it->base == WB_CANNON) model_lost_ranged(f, &L, it->base, draw, time, fx, seed);
    else if (it->base == WB_TOME || it->base == WB_CENSER) model_lost_staff(f, &L, it->base, charge, gem, time, fx, seed);
    else if (wb->wclass == WPN_BOW) model_bow(f, &L, it->base, draw, time, fx, seed, charge > 0); /* charge: an arrow on the string */
    else model_staff(f, &L, it->base, charge, gem, time, fx, seed);
}

/* Length of a model along u (to centre it on the ground). */
static float model_len(int base) {
    static const float LEN[WB_COUNT] = { 0.3f, 0.6f, 0.58f, 0.62f, 1.1f, 0.98f, 0.56f, 0.88f, 0.55f, 0.37f, 1.0f, 0.57f,
                                         0.95f, 0.62f, 1.15f, 0.82f, 0.25f, 0.26f, 0.45f, 0.2f, 0.45f };
    return base < WB_COUNT ? LEN[base] : 0.6f;
}

void weapon_draw_ground(const Item *it, vec3 c, float yaw, vec3 light, float time, float seed) {
    WF f = { 0 };
    f.u = v3(cosf(yaw), 0, sinf(yaw));
    f.s = 0.9f;
    int wc = WEAPON_BASES[it->base].wclass;
    if (wc == WPN_BOW && !forward_held(it->base)) {
        /* bows stand up, slowly turning */
        f.u = v3(0, 1, 0);
        f.w = v3(cosf(yaw), 0, sinf(yaw));
        f.v = v3(-sinf(yaw), 0, cosf(yaw));
        f.o = v3_add(c, v3(0, 0.2f, 0));
    } else if (forward_held(it->base)) {
        f.v = v3(-sinf(yaw), 0, cosf(yaw));
        f.w = v3(0, 1, 0);
        f.o = c;
    } else {
        /* blades and staves lie flat, centred on c */
        f.v = v3(-sinf(yaw), 0, cosf(yaw));
        f.w = v3(0, 1, 0);
        f.o = v3_sub(c, v3_scale(f.u, model_len(it->base) * 0.45f * f.s));
    }
    f.up = v3(0, 1, 0);
    model(&f, it, light, time, true, seed, forward_held(it->base) ? 1.0f : 0.0f, 0.0f, v3(0.55f, 0.6f, 1.0f));
}

void weapon_draw_held(const Item *it, vec3 hand, vec3 dir, vec3 side, vec3 light, float time, float seed) {
    WF f = { 0 };
    f.o = hand;
    f.u = v3_norm(dir);
    f.v = v3_norm(side);
    f.w = v3_norm(v3_cross(f.u, f.v));
    f.up = v3(0, 1, 0);
    f.s = 1.0f;
    int wc = WEAPON_BASES[it->base].wclass;
    if (wc == WPN_STAFF) f.o = v3_sub(hand, v3_scale(f.u, 0.3f * model_len(it->base)));
    if (wc == WPN_BOW && !forward_held(it->base)) {
        /* held upright, string toward the archer */
        f.u = v3(0, 1, 0);
        f.w = v3_scale(v3_norm(v3(dir.x, 0, dir.z)), -1.0f);
        f.v = v3_cross(f.w, f.u);
    } else if (forward_held(it->base)) {
        f.u = v3_norm(v3(dir.x, 0, dir.z));
        f.w = v3(0, 1, 0);
        f.v = v3_cross(f.u, f.w);
    }
    model(&f, it, light, time, true, seed, forward_held(it->base) ? 1.0f : 0.0f, 1.0f, v3(0.55f, 0.6f, 1.0f));
}

/* ---------- the weapon in your hands ---------- */

void weapon_draw_view(Game *g, PlayerSlot *ps, vec3 light) {
    Stats *s = &ps->st;
    const Item *it = &s->equipped[s->weapon];
    Item pick = { 0 };
    if (ps->pick_out) { /* the pickaxe, swung like an axe */
        pick.used = 1;
        pick.base = WB_AXE;
        pick.rarity = (u8)(s->pick >= 3 ? RAR_RARE : s->pick == 2 ? RAR_MAGIC : RAR_COMMON);
        it = &pick;
    }
    float t = s->swing_len > 0 ? 1.0f - s->swing / s->swing_len : 1.0f; /* 0 at the start of an attack */
    bool attacking = s->swing > 0;
    float walk = sinf(ps->pl.bob * 2.0f) * 0.015f;
    float time = g->time;
    float seed = (float)ps->index * 17.0f;
    WF f = { 0 };
    f.up = v3(0, 1, 0);
    f.s = 1.0f;
    switch (ps->pick_out ? WPN_SWORD : s->weapon) {
    case WPN_SWORD: {
        int base = it->base;
        float reach = base == WB_GREATSWORD ? 0.85f : base == WB_DAGGER ? 1.15f : 1.0f;
        if (base == WB_SPEAR || base == WB_RAPIER) {
            /* a thrust: the point goes forward and comes back */
            float th = attacking ? sinf(fminf(1.0f, t * 1.4f) * PI_F) * 0.35f : 0.0f;
            f.o = v3(0.3f + walk, -0.4f + fabsf(walk), -0.3f - th);
            f.u = v3_norm(v3(-0.22f, 0.32f, -1.0f));
            f.v = v3_norm(v3_cross(f.u, v3(0, 1, 0)));
            f.w = v3_cross(f.v, f.u);
            model(&f, it, light, time, true, seed, 0, 0, v3(0, 0, 0));
            break;
        }
        bool heavy = base == WB_AXE || base == WB_MACE;
        float a, tilt;
        if (attacking && s->swing_kind) a = t * 2.0f * PI_F, tilt = 0.9f;
        else if (attacking && heavy) a = lerpf(0.2f, -0.2f, smoothf(t)), tilt = 0.4f; /* chop from above */
        else if (attacking) a = lerpf(0.9f, -1.3f, smoothf(t)), tilt = 0.9f;
        else a = 0.25f, tilt = 0.35f;
        vec3 base_p = v3(0.36f + walk, -0.42f + fabsf(walk), -0.72f);
        if (attacking && !s->swing_kind && !heavy) base_p.x -= 0.25f * smoothf(t);
        float fwd = -0.55f;
        if (attacking && heavy && !s->swing_kind) {
            /* raised back, then brought down hard */
            float k = t < 0.35f ? -t / 0.35f : -1.0f + (t - 0.35f) / 0.65f * 2.6f;
            fwd = -0.55f - 0.9f * fmaxf(-0.6f, fminf(1.0f, k));
            base_p.y += t < 0.35f ? 0.1f * t / 0.35f : 0.1f - 0.25f * (t - 0.35f);
        }
        f.o = base_p;
        f.u = v3_norm(v3(-sinf(a) * tilt, cosf(a) * 0.9f, fwd));
        f.v = v3_norm(v3_cross(f.u, v3(0, 0, 1)));
        f.w = v3_cross(f.v, f.u);
        f.s = 0.9f * reach;
        model(&f, it, light, time, true, seed, 0, 0, v3(0, 0, 0));
        break;
    }
    case WPN_BOW: {
        float draw = attacking ? (t < 0.3f ? 0.0f : (t - 0.3f) / 0.7f) : 1.0f; /* crossbow: cocking again after a shot */
        if (it->base == WB_CHAKRAM || it->base == WB_SLING) {
            /* held low in front; the chakram leaves the hand when thrown */
            if (it->base == WB_CHAKRAM && attacking && t < 0.85f) break;
            f.o = v3(0.2f + walk, (it->base == WB_SLING ? -0.12f : -0.32f) + fabsf(walk), -0.5f);
            f.u = v3_norm(v3(-0.25f, 0.15f, -1.0f));
            f.v = v3_norm(v3_cross(f.u, v3(0, 1, 0)));
            f.w = v3_cross(f.v, f.u);
            model(&f, it, light, time, true, seed, attacking ? 1.0f - t : 0.0f, 0, v3(0, 0, 0));
        } else if (forward_held(it->base)) {
            float kick = attacking && t < 0.2f ? (0.2f - t) * (it->base == WB_CANNON ? 1.0f : 0.4f) : 0.0f;
            f.o = v3(0.2f + walk, -0.2f + fabsf(walk) + kick * 0.5f, -0.5f + kick);
            f.u = v3_norm(v3(-0.1f, 0.08f, -1.0f));
            f.s = 0.8f;
            f.v = v3_norm(v3_cross(f.u, v3(0, 1, 0)));
            f.w = v3_cross(f.v, f.u);
            model(&f, it, light, time, true, seed, draw, 0, v3(0, 0, 0));
        } else {
            /* held turned well to the side and tilted, so the curve of the limbs shows;
             * the arrow still points at the crosshair */
            f.o = v3(-0.22f + walk, -0.14f, -0.58f);
            f.u = v3_norm(v3(0.18f, 1.0f, 0.0f));
            f.w = v3_norm(v3(0.85f, 0, 0.55f));
            f.w = v3_norm(v3_sub(f.w, v3_scale(f.u, v3_dot(f.w, f.u))));
            f.v = v3_cross(f.u, f.w);
            f.aim = v3_norm(v3(0.3f, 0.12f, -1.0f));
            f.s = it->base == WB_LONGBOW ? 0.8f : 1.0f;
            /* at rest the string is straight with an arrow on it; right after a shot it is empty */
            bool nocked = !attacking || t > 0.5f;
            model(&f, it, light, time, true, seed, 0.0f, nocked ? 1.0f : 0.0f, v3(0, 0, 0));
        }
        break;
    }
    case WPN_STAFF: {
        float thrust = attacking ? sinf(t * PI_F) * 0.12f : 0.0f;
        float charge = attacking ? 0.5f * (1.0f - t) : 0.0f;
        vec3 gem = s->ranks[SK_FIREBALL] ? v3(1.0f, 0.55f, 0.2f) : v3(0.55f, 0.6f, 1.0f);
        if (it->base == WB_TOME) {
            f.o = v3(0.08f + walk, -0.24f + fabsf(walk), -0.4f - thrust);
            f.u = v3_norm(v3(0.1f, 0.35f, -1.0f));
            f.v = v3_norm(v3_cross(f.u, v3(0, 1, 0)));
            f.w = v3_cross(f.v, f.u);
            model(&f, it, light, time, true, seed, 0, charge, gem);
            break;
        } else if (it->base == WB_CENSER) {
            /* swinging on its chain from the hand */
            float sw = sinf(time * 2.2f) * 0.25f + (attacking ? sinf(t * PI_F) * 0.8f : 0.0f);
            f.o = v3(0.26f + walk, 0.12f, -0.62f);
            f.u = v3_norm(v3(sinf(sw) * 0.5f, -1.0f, -cosf(sw) * 0.3f));
            f.v = v3_norm(v3_cross(f.u, v3(0, 0, 1)));
            f.w = v3_cross(f.v, f.u);
            model(&f, it, light, time, true, seed, 0, charge, gem);
            break;
        } else if (it->base == WB_WAND) {
            f.o = v3(0.24f + walk, -0.3f + fabsf(walk), -0.42f - thrust);
            f.u = v3_norm(v3(-0.1f, 0.35f, -1.0f));
        } else if (it->base == WB_SCEPTER) {
            f.o = v3(0.3f + walk, -0.42f + fabsf(walk), -0.5f - thrust);
            f.u = v3_norm(v3(-0.12f, 0.75f, -0.75f));
        } else {
            vec3 tip = v3(0.25f + walk, -0.1f, -0.75f - thrust);
            vec3 butt = v3(0.42f, -0.65f, -0.35f);
            f.u = v3_norm(v3_sub(tip, butt));
            f.o = v3_sub(tip, v3_scale(f.u, 0.9f));
        }
        f.v = v3_norm(v3_cross(f.u, v3(0, 0, 1)));
        f.w = v3_cross(f.v, f.u);
        model(&f, it, light, time, true, seed, 0, charge, gem);
        break;
    }
    }
}

/* ---------- where a magical weapon hits ---------- */

static void spark(Game *g, vec3 p, vec3 vel, vec3 col, float life, float size, bool emissive, bool gravity) {
    if (g->nparts >= MAX_PARTICLES) return;
    Particle *q = &g->parts[g->nparts++];
    q->pos = p;
    q->vel = vel;
    q->color = col;
    q->life = q->max_life = life;
    q->size = size;
    q->emissive = emissive;
    q->gravity = gravity;
}

static float fx_rand(void) {
    static u64 rs = 0xA11CE;
    rs = mix64(rs + 0x9E3779B97F4A7C15ULL);
    return (float)(rs >> 40) / (float)(1ULL << 24);
}

void weapon_impact_fx(Game *g, int element, int rarity, bool vampiric, vec3 at, int owner) {
    if (rarity <= RAR_COMMON && element == EL_NONE) return;
    float power = 1.0f + 0.35f * (rarity > 0 ? rarity - 1 : 0);
    int el = vampiric ? EL_SHADOW : element;
    switch (el) {
    case EL_FIRE:
        flash_add(g, at, v3(1.4f, 0.6f, 0.2f), 5.0f * power, 0.25f);
        for (int i = 0; i < (int)(14 * power); i++)
            spark(g, at, v3((fx_rand() - 0.5f) * 3.0f, 1.5f + fx_rand() * 3.5f, (fx_rand() - 0.5f) * 3.0f), fx_rand() < 0.5f ? v3(1.0f, 0.55f, 0.12f) : v3(1.0f, 0.85f, 0.3f),
                  0.4f + fx_rand() * 0.5f, 0.05f + fx_rand() * 0.04f, true, false);
        break;
    case EL_FROST:
        flash_add(g, at, v3(0.5f, 0.8f, 1.3f), 4.0f * power, 0.2f);
        for (int i = 0; i < (int)(12 * power); i++) { /* shards that fall */
            vec3 d = v3_norm(v3(fx_rand() - 0.5f, fx_rand() * 0.8f, fx_rand() - 0.5f));
            spark(g, at, v3_scale(d, 3.0f + fx_rand() * 3.0f), v3(0.75f, 0.9f, 1.0f), 0.6f + fx_rand() * 0.5f, 0.035f + fx_rand() * 0.03f, false, true);
        }
        for (int i = 0; i < 6; i++) spark(g, at, v3((fx_rand() - 0.5f) * 0.6f, 0.2f + fx_rand() * 0.4f, (fx_rand() - 0.5f) * 0.6f), v3(0.8f, 0.95f, 1.0f), 0.9f, 0.06f, true, false);
        break;
    case EL_LIGHTNING: {
        flash_add(g, at, v3(0.8f, 0.85f, 1.6f), 7.0f * power, 0.15f);
        for (int a = 0; a < 3; a++) { /* jagged arcs leaping out */
            vec3 p = at, d = v3_norm(v3(fx_rand() - 0.5f, fx_rand() - 0.3f, fx_rand() - 0.5f));
            for (int s = 0; s < 7; s++) {
                p = v3_add(p, v3_add(v3_scale(d, 0.18f), v3((fx_rand() - 0.5f) * 0.15f, (fx_rand() - 0.5f) * 0.15f, (fx_rand() - 0.5f) * 0.15f)));
                spark(g, p, v3(0, 0, 0), s % 2 ? v3(0.85f, 0.9f, 1.0f) : v3(0.55f, 0.65f, 1.0f), 0.22f + fx_rand() * 0.12f, 0.055f, true, false);
            }
        }
        break;
    }
    case EL_POISON:
        for (int i = 0; i < (int)(12 * power); i++) /* a sickly cloud that lingers */
            spark(g, v3_add(at, v3((fx_rand() - 0.5f) * 0.5f, (fx_rand() - 0.5f) * 0.4f, (fx_rand() - 0.5f) * 0.5f)), v3((fx_rand() - 0.5f) * 0.8f, 0.2f + fx_rand() * 0.5f, (fx_rand() - 0.5f) * 0.8f),
                  fx_rand() < 0.6f ? v3(0.35f, 0.85f, 0.25f) : v3(0.6f, 1.0f, 0.3f), 0.8f + fx_rand() * 0.8f, 0.06f + fx_rand() * 0.05f, true, false);
        for (int i = 0; i < 5; i++) spark(g, at, v3((fx_rand() - 0.5f) * 2.0f, 1.0f + fx_rand() * 1.5f, (fx_rand() - 0.5f) * 2.0f), v3(0.3f, 0.7f, 0.2f), 0.7f, 0.04f, false, true);
        break;
    case EL_SHADOW: {
        /* blood drawn back to whoever struck */
        vec3 to = at;
        if (owner >= 0 && owner < MAX_PLAYERS && g->players[owner].active) to = player_eye(&g->players[owner].pl);
        vec3 d = v3_sub(to, at);
        float dist = fmaxf(0.5f, v3_len(d));
        vec3 col = vampiric ? v3(0.9f, 0.08f, 0.12f) : v3(0.55f, 0.25f, 0.85f);
        for (int i = 0; i < (int)(10 * power); i++) {
            float life = 0.35f + fx_rand() * 0.3f;
            vec3 v = v3_add(v3_scale(d, 1.0f / life * (dist > 6.0f ? 6.0f / dist : 1.0f)), v3((fx_rand() - 0.5f) * 1.5f, (fx_rand() - 0.5f) * 1.5f, (fx_rand() - 0.5f) * 1.5f));
            spark(g, v3_add(at, v3((fx_rand() - 0.5f) * 0.3f, (fx_rand() - 0.5f) * 0.3f, (fx_rand() - 0.5f) * 0.3f)), v, col, life, 0.045f, true, false);
        }
        flash_add(g, at, v3_scale(col, 1.2f), 4.0f, 0.2f);
        break;
    }
    default: {
        /* plain magic: a ring of sparks in the rarity's colour */
        vec3 col = rarity_color(rarity);
        int n = 10 + rarity * 4;
        for (int i = 0; i < n; i++) {
            float a = i * 2.0f * PI_F / n;
            spark(g, at, v3(cosf(a) * 3.5f, (fx_rand() - 0.3f) * 1.5f, sinf(a) * 3.5f), col, 0.3f + fx_rand() * 0.2f, 0.035f, true, false);
        }
        flash_add(g, at, col, 3.5f * power, 0.15f);
        break;
    }
    }
    if (rarity >= RAR_EPIC) {
        /* epic weapons also leave a flat shock ring */
        for (int i = 0; i < 18; i++) {
            float a = i * 2.0f * PI_F / 18.0f;
            spark(g, at, v3(cosf(a) * 6.0f, 0.0f, sinf(a) * 6.0f), v3(1.0f, 0.75f, 0.35f), 0.25f, 0.04f, true, false);
        }
    }
}

/* ---------- the armoury: every weapon close up (--demo armoury --cast PAGE) ----------
 * Pages: 0 the twelve weapons of every world in plain iron; 1 the nine lost arts; 2 the twelve in
 * rarities and affixes; 3 the lost arts in other materials; 4 the tiers side by side (a sword,
 * an axe, a bow and a staff from common to legendary); 5 one rare sword in each element and
 * swift and heavy; 6 the twelve, epic and frozen; 7 twelve rare swords, each its own look; 9 the
 * weapon given with --weapon, alone; 10 + a base number: that weapon alone. Drawn in view space
 * before a plain backdrop, under even light, for looking at the models. */
Item g_armoury_item;

void weapon_draw_armoury(int page, float time) {
    static const u8 RAR[WB_CLASSIC] = { RAR_COMMON, RAR_MAGIC, RAR_RARE, RAR_EPIC, RAR_LEGENDARY, RAR_MAGIC,
                                        RAR_COMMON, RAR_RARE, RAR_EPIC, RAR_MAGIC, RAR_RARE, RAR_EPIC };
    static const u8 AFX[WB_CLASSIC] = { AF_NONE, AF_FIRE, AF_FROST, AF_SHOCK, AF_VENOM, AF_VAMPIRIC,
                                        AF_NONE, AF_FIRE, AF_SHOCK, AF_FROST, AF_VENOM, AF_KEEN };
    Item items[24];
    int n = 0, cols = 3;
    memset(items, 0, sizeof items);
    for (int k = 0; k < 24; k++) { items[k].used = 1; items[k].level = 8; }
    if (page >= 10 || page == 9) {
        if (page == 9) items[0] = g_armoury_item;
        else items[0].base = (u8)((page - 10) % WB_COUNT);
        n = 1;
    } else if (page == 4) {
        static const u8 B4[4] = { WB_SWORD, WB_AXE, WB_LONGBOW, WB_STAFF };
        cols = 5;
        for (int row = 0; row < 4; row++)
            for (int r = 0; r < RAR_COUNT; r++) { items[n].base = B4[row]; items[n].rarity = (u8)r; items[n].look = (u8)(11 + r * 13); n++; }
    } else if (page == 5) {
        static const u8 A5[8] = { AF_NONE, AF_FROST, AF_FIRE, AF_SHOCK, AF_VENOM, AF_VAMPIRIC, AF_SWIFT, AF_HEAVY };
        cols = 4;
        for (int k = 0; k < 8; k++) { items[n].base = WB_SWORD; items[n].rarity = RAR_RARE; items[n].affix[0] = A5[k]; items[n].look = 40; n++; }
    } else if (page == 6) {
        for (int k = 0; k < WB_CLASSIC; k++) { items[n].base = (u8)k; items[n].rarity = RAR_EPIC; items[n].affix[0] = AF_FROST; items[n].look = (u8)(3 + k * 7); n++; }
    } else if (page == 7) {
        for (int k = 0; k < 12; k++) { items[n].base = WB_SWORD; items[n].rarity = RAR_RARE; items[n].look = (u8)(1 + k * 19); n++; }
    } else {
        bool lost = page & 1, fancy = page >= 2;
        int cnt = lost ? WB_COUNT - WB_CLASSIC : WB_CLASSIC;
        for (int k = 0; k < cnt; k++) {
            int base = lost ? WB_CLASSIC + k : k;
            items[n].base = (u8)base;
            if (fancy) {
                items[n].rarity = RAR[base % WB_CLASSIC];
                items[n].affix[0] = AFX[base % WB_CLASSIC];
                if (lost) items[n].material = (u8)(1 + k % (MAT_COUNT - 1));
            }
            n++;
        }
    }
    batch3d_box(v3(0, 0, -4.0f), v3(6.0f, 4.0f, 0.05f), 0, v3(0.13f, 0.12f, 0.14f), true); /* the backdrop */
    vec3 light = v3(1.0f, 0.95f, 0.88f);
    int rows = (n + cols - 1) / cols;
    const float z = -1.75f;
    float cw = 3.36f / cols, ch = fminf(0.5f, 1.9f / rows);
    for (int k = 0; k < n; k++) {
        const Item *it = &items[k];
        int base = it->base;
        int col = k % cols, row = k / cols;
        vec3 c = n == 1 ? v3(0, 0, z) : v3((col - (cols - 1) * 0.5f) * cw, (rows - 1) * 0.5f * ch - row * ch, z);
        WF f = { 0 };
        f.up = v3(0, 1, 0);
        float len = model_len(base);
        f.s = n == 1 ? 2.2f / len : fminf(1.1f, cw * 0.74f / len);
        int wc = WEAPON_BASES[base].wclass;
        if (wc == WPN_BOW && !forward_held(base)) { /* the curve of the limbs in profile */
            f.u = v3_norm(v3(1, 0, -0.25f));
            f.w = v3(0, -1, 0);
            f.v = v3_cross(f.w, f.u);
            f.o = v3_add(c, v3(0, 0.05f, 0));
        } else if (forward_held(base)) { /* from the side, a little from above */
            f.u = v3_norm(v3(1, -0.05f, -0.35f));
            f.w = v3_norm(v3(0, 1, 0.45f));
            f.v = v3_norm(v3_cross(f.w, f.u));
            f.w = v3_cross(f.u, f.v);
            f.o = v3_sub(c, v3_scale(f.u, len * 0.4f * f.s));
        } else { /* the flat toward the eye, turned a little so the bevels catch the light */
            f.u = v3_norm(v3(1, 0.08f, -0.3f));
            f.w = v3_norm(v3(0.1f, 0.4f, 1));
            f.v = v3_norm(v3_cross(f.w, f.u));
            f.w = v3_cross(f.u, f.v);
            f.o = v3_sub(c, v3_scale(f.u, len * (n == 1 ? 0.4f : 0.5f) * f.s)); /* (the grip runs back from the hand) */
        }
        bool fx = it->rarity > RAR_COMMON || it->affix[0];
        model(&f, it, light, time, fx, (float)k, forward_held(base) ? 1.0f : 0.0f, 0.0f, v3(0.55f, 0.6f, 1.0f));
    }
}
