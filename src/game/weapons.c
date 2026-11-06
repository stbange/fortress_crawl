#include "game/game.h"

#include "render/batch.h"

/* What weapons look like: one model per weapon base, drawn the same in your hands, in
 * other players' hands and on the ground. The rarity picks the materials (iron, bright
 * steel, gilded, blackened with a glowing edge); magical weapons glow in the colour of
 * their affix and give off motes (embers, frost, sparks, venom drops, a blood mist) and
 * burst in that element where they hit. */

/* ---------- materials ---------- */

typedef struct WLook {
    vec3 metal, edge, wood, grip, accent; /* lit */
    vec3 glow;                            /* emissive, already pulsing */
    float glow_amt;                       /* 0 for common weapons */
    int element;                          /* EL_* of the affix, EL_NONE */
    bool vampiric;
    int rarity;
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
    static const float GLOW[RAR_COUNT] = { 0.0f, 0.55f, 0.8f, 1.0f, 1.25f };
    int r = it->rarity < RAR_COUNT ? it->rarity : 0;
    WLook L;
    L.rarity = r;
    L.metal = mulv(METAL[r], light);
    L.edge = mulv(v3_add(METAL[r], v3(0.15f, 0.15f, 0.15f)), light);
    L.wood = mulv(r == RAR_EPIC ? v3(0.34f, 0.2f, 0.17f) : r == RAR_LEGENDARY ? v3(0.85f, 0.82f, 0.74f) : v3(0.45f, 0.29f, 0.15f), light);
    L.grip = mulv(GRIP[r], light);
    L.accent = mulv(ACCENT[r], light);
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

/* A glowing line along an edge: the mark of a magical blade. */
static void edge_glow(const WF *f, const WLook *L, float a0, float a1, float b, float hc) {
    if (L->glow_amt <= 0) return;
    float ha = (a1 - a0) * 0.5f;
    B(f, a0 + ha, b, 0, ha, 0.004f, hc, L->glow, true);
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

/* ---------- the models ---------- */

static void blade(const WF *f, const WLook *L, float a0, float len, float hw, float tip) {
    B(f, a0 + len * 0.5f, 0, 0, len * 0.5f, hw, 0.008f, L->metal, false);
    B(f, a0 + len * 0.45f, 0, 0, len * 0.42f, hw * 0.25f, 0.0095f, v3_scale(L->metal, 0.75f), false); /* fuller */
    SP(f, a0 + len, 0, 0, f->u, hw, tip, L->edge, false);
    edge_glow(f, L, a0 + 0.01f, a0 + len, hw + 0.002f, 0.009f);
    edge_glow(f, L, a0 + 0.01f, a0 + len, -hw - 0.002f, 0.009f);
}

static void hilt(const WF *f, const WLook *L, float guard_w, float grip_len) {
    B(f, 0, 0, 0, grip_len, 0.016f, 0.016f, L->grip, false);
    for (float a = -grip_len + 0.02f; a < grip_len; a += 0.035f) B(f, a, 0, 0, 0.006f, 0.018f, 0.018f, v3_scale(L->grip, 0.7f), false);
    B(f, grip_len + 0.012f, 0, 0, 0.012f, guard_w, 0.022f, L->accent, false);
    O(f, -grip_len - 0.02f, 0, 0, 0.022f, 0.022f, 0.4f, L->accent, false);
    if (L->glow_amt > 0) O(f, grip_len + 0.012f, 0, 0.024f, 0.011f, 0.014f, 0.0f, L->glow, true); /* a gem in the guard */
}

static void model_melee(const WF *f, const WLook *L, int base, float time, bool fx, float seed) {
    switch (base) {
    case WB_DAGGER:
        hilt(f, L, 0.055f, 0.045f);
        blade(f, L, 0.06f, 0.2f, 0.021f, 0.05f);
        if (fx) weapon_motes(f, L, 0.08f, 0.28f, time, seed);
        break;
    case WB_SWORD:
    default:
        hilt(f, L, 0.11f, 0.06f);
        blade(f, L, 0.08f, 0.44f, 0.03f, 0.07f);
        if (fx) weapon_motes(f, L, 0.1f, 0.55f, time, seed);
        break;
    case WB_GREATSWORD:
        hilt(f, L, 0.17f, 0.11f);
        SP(f, 0.13f, 0.17f, 0, v3_sub(v3_scale(f->u, 0.6f), f->v), 0.012f, 0.05f, L->accent, false); /* swept guard */
        SP(f, 0.13f, -0.17f, 0, v3_add(v3_scale(f->u, 0.6f), f->v), 0.012f, 0.05f, L->accent, false);
        B(f, 0.18f, 0, 0, 0.04f, 0.03f, 0.011f, L->metal, false); /* ricasso */
        blade(f, L, 0.22f, 0.7f, 0.045f, 0.1f);
        if (fx) weapon_motes(f, L, 0.25f, 0.95f, time, seed);
        break;
    case WB_AXE:
        B(f, 0.2f, 0, 0, 0.34f, 0.017f, 0.017f, L->wood, false); /* haft */
        B(f, -0.06f, 0, 0, 0.07f, 0.02f, 0.02f, L->grip, false);
        B(f, 0.46f, 0.07f, 0, 0.045f, 0.06f, 0.012f, L->metal, false); /* cheek */
        B(f, 0.46f, 0.145f, 0, 0.1f, 0.018f, 0.009f, L->edge, false); /* the bit */
        SP(f, 0.46f, -0.02f, 0, v3_scale(f->v, -1.0f), 0.022f, 0.08f, L->metal, false); /* back spike */
        B(f, 0.55f, 0, 0, 0.012f, 0.022f, 0.022f, L->accent, false);
        if (L->glow_amt > 0) B(f, 0.46f, 0.165f, 0, 0.1f, 0.004f, 0.01f, L->glow, true);
        if (fx) weapon_motes(f, L, 0.38f, 0.56f, time, seed);
        break;
    case WB_MACE:
        B(f, 0.18f, 0, 0, 0.3f, 0.016f, 0.016f, v3_scale(L->metal, 0.8f), false);
        B(f, -0.04f, 0, 0, 0.08f, 0.02f, 0.02f, L->grip, false);
        B(f, 0.52f, 0, 0, 0.055f, 0.045f, 0.045f, L->metal, false); /* head */
        for (int k = 0; k < 4; k++) { /* flanges */
            float b = k == 0 ? 0.06f : k == 1 ? -0.06f : 0, c = k == 2 ? 0.06f : k == 3 ? -0.06f : 0;
            B(f, 0.52f, b, c, 0.05f, b != 0 ? 0.018f : 0.006f, c != 0 ? 0.018f : 0.006f, L->edge, false);
        }
        SP(f, 0.58f, 0, 0, f->u, 0.02f, 0.06f, L->edge, false);
        B(f, 0.46f, 0, 0, 0.008f, 0.03f, 0.03f, L->accent, false);
        if (L->glow_amt > 0) B(f, 0.52f, 0, 0, 0.02f, 0.047f, 0.047f, L->glow, true); /* a glowing band */
        if (fx) weapon_motes(f, L, 0.45f, 0.6f, time, seed);
        break;
    case WB_SPEAR:
        B(f, 0.25f, 0, 0, 0.62f, 0.014f, 0.014f, L->wood, false);
        B(f, 0.0f, 0, 0, 0.06f, 0.017f, 0.017f, L->grip, false);
        B(f, 0.88f, 0, 0, 0.02f, 0.02f, 0.02f, L->accent, false); /* binding */
        B(f, 0.97f, 0, 0, 0.07f, 0.032f, 0.008f, L->metal, false); /* leaf blade */
        SP(f, 1.04f, 0, 0, f->u, 0.032f, 0.09f, L->edge, false);
        SP(f, 0.91f, 0.03f, 0, v3_sub(f->v, v3_scale(f->u, 0.5f)), 0.01f, 0.04f, L->metal, false); /* lugs */
        SP(f, 0.91f, -0.03f, 0, v3_add(v3_scale(f->v, -1.0f), v3_scale(f->u, -0.5f)), 0.01f, 0.04f, L->metal, false);
        edge_glow(f, L, 0.9f, 1.06f, 0.034f, 0.009f);
        edge_glow(f, L, 0.9f, 1.06f, -0.034f, 0.009f);
        if (fx) weapon_motes(f, L, 0.88f, 1.1f, time, seed);
        break;
    }
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
    float thick = base == WB_LONGBOW ? 0.014f : 0.018f;
    float pa = -half, pc = bow_curve(base, -1.0f, bend);
    for (int i = 1; i <= 12; i++) {
        float t = (i / 12.0f - 0.5f) * 2.0f;
        float a = t * half, c = bow_curve(base, t, bend);
        bool g = L->glow_amt > 0 && (i == 1 || i == 12);
        SEG(f, pa, 0, pc, a, 0, c, thick, g ? L->glow : L->wood, g);
        pa = a;
        pc = c;
    }
    float tipc = bow_curve(base, 1.0f, bend);
    B(f, 0, 0, 0, 0.06f, thick * 1.4f, thick * 1.4f, L->grip, false); /* grip */
    if (L->glow_amt > 0) O(f, 0.075f, 0, 0, 0.012f, 0.016f, time * 2.0f, L->glow, true);
    else B(f, 0.07f, 0, 0, 0.01f, thick * 1.5f, thick * 1.5f, L->accent, false);
    float nock = tipc + 0.16f * draw; /* straight string at rest */
    vec3 sc = L->glow_amt > 0 ? v3_lerp(v3(0.8f, 0.8f, 0.75f), v3_scale(L->glow, 1.0f / fmaxf(0.3f, L->glow_amt)), 0.5f) : v3(0.8f, 0.8f, 0.75f);
    SEG(f, half, 0, tipc, 0, 0, nock, 0.003f, sc, L->glow_amt > 0);
    SEG(f, -half, 0, tipc, 0, 0, nock, 0.003f, sc, L->glow_amt > 0);
    if (arrow) {
        /* the nocked arrow, along the aim (away from the archer unless told otherwise) */
        vec3 aim = v3_len(f->aim) > 0.1f ? v3_norm(f->aim) : v3_scale(f->w, -1.0f);
        vec3 p0 = P(f, 0, 0, nock), p1 = v3_add(p0, v3_scale(aim, 0.62f * f->s));
        vec3 d = v3_scale(v3_sub(p1, p0), 0.5f);
        vec3 sx = v3_scale(v3_norm(v3_cross(aim, f->u)), 0.006f * f->s), sy = v3_scale(v3_norm(v3_cross(sx, aim)), 0.006f * f->s);
        batch3d_obox(v3_add(p0, d), d, sx, sy, v3(0.55f, 0.45f, 0.3f), false);
        vec3 head = L->element != EL_NONE || L->vampiric ? L->glow : L->metal;
        batch3d_spike(p1, aim, 0.014f * f->s, 0.05f * f->s, head, L->glow_amt > 0);
        batch3d_obox(v3_add(p0, v3_scale(aim, 0.04f * f->s)), v3_scale(sx, 3.0f), v3_scale(sy, 0.6f), v3_scale(aim, 0.03f * f->s),
                     L->glow_amt > 0 ? L->glow : v3(0.75f, 0.7f, 0.6f), L->glow_amt > 0); /* fletching */
    }
    if (fx) weapon_motes(f, L, -half * 0.9f, half * 0.9f, time, seed);
}

/* Crossbow: u forward along the stock, v right, w up. draw 0..1 (string back). */
static void model_crossbow(const WF *f, const WLook *L, float draw, float time, bool fx, float seed) {
    B(f, 0.0f, 0, 0, 0.26f, 0.025f, 0.03f, L->wood, false); /* stock */
    B(f, -0.24f, 0, -0.02f, 0.07f, 0.03f, 0.05f, L->wood, false); /* butt */
    B(f, -0.05f, 0, -0.06f, 0.025f, 0.015f, 0.04f, L->grip, false); /* grip */
    B(f, 0.22f, 0, 0.0f, 0.03f, 0.03f, 0.035f, L->metal, false); /* bow mount */
    /* the prod: two arms swept back */
    for (int k = -1; k <= 1; k += 2) {
        SEG(f, 0.24f, 0, 0.0f, 0.2f, 0.17f * k, 0.0f, 0.012f, L->metal, false);
        SEG(f, 0.2f, 0.17f * k, 0, 0.13f, 0.27f * k, 0.0f, 0.01f, L->metal, false);
        if (L->glow_amt > 0) O(f, 0.13f, 0.27f * k, 0, 0.01f, 0.014f, time * 2.0f, L->glow, true);
        float nut = 0.22f - 0.24f * draw;
        SEG(f, 0.13f, 0.27f * k, 0.0f, nut, 0, 0.03f, 0.003f, v3(0.8f, 0.8f, 0.75f), false);
    }
    B(f, -0.02f, 0, 0.035f, 0.025f, 0.02f, 0.008f, L->accent, false); /* the nut and trigger */
    if (draw > 0.6f) {
        SEG(f, -0.02f, 0, 0.04f, 0.28f, 0, 0.04f, 0.006f, v3_scale(L->metal, 0.8f), false); /* the bolt */
        SP(f, 0.28f, 0, 0.04f, f->u, 0.012f, 0.04f, L->element != EL_NONE || L->vampiric ? L->glow : L->edge, L->glow_amt > 0);
    }
    if (L->glow_amt > 0) B(f, 0.05f, 0.026f, 0, 0.15f, 0.002f, 0.006f, L->glow, true); /* inlaid line */
    if (fx) weapon_motes(f, L, 0.1f, 0.25f, time, seed);
}

/* Staves: u from the butt (o) to the head. charge: brighter right after a cast. */
static vec3 gem_color(const WLook *L, vec3 fallback) {
    if (L->vampiric) return v3(0.95f, 0.1f, 0.18f);
    if (L->element != EL_NONE) return element_color(L->element);
    return L->glow_amt > 0 ? v3_scale(L->glow, 1.0f / fmaxf(0.3f, L->glow_amt)) : fallback;
}

static void model_staff(const WF *f, const WLook *L, int base, float charge, vec3 gem_base, float time, bool fx, float seed) {
    vec3 gem = v3_scale(gem_color(L, gem_base), 0.75f + 0.25f * sinf(time * 6.0f) + charge);
    switch (base) {
    case WB_WAND:
        B(f, 0.16f, 0, 0, 0.16f, 0.009f, 0.009f, L->wood, false);
        B(f, 0.04f, 0, 0, 0.05f, 0.012f, 0.012f, L->grip, false);
        for (int k = 0; k < 3; k++) B(f, 0.12f + k * 0.07f, 0, 0, 0.004f, 0.012f, 0.012f, L->accent, false);
        SP(f, 0.32f, 0, 0, f->u, 0.009f, 0.03f, L->accent, false);
        O(f, 0.355f, 0, 0, 0.014f, 0.02f, time * 3.0f, gem, true);
        if (fx) weapon_motes(f, L, 0.2f, 0.36f, time, seed);
        break;
    case WB_SCEPTER:
        B(f, 0.2f, 0, 0, 0.22f, 0.013f, 0.013f, L->accent, false); /* gilded shaft */
        B(f, 0.05f, 0, 0, 0.06f, 0.016f, 0.016f, L->grip, false);
        O(f, -0.03f, 0, 0, 0.022f, 0.022f, 0.0f, L->accent, false);
        B(f, 0.42f, 0, 0, 0.02f, 0.03f, 0.03f, L->metal, false);
        for (int k = 0; k < 4; k++) { /* a crown of prongs around the orb */
            float ang = k * PI_F * 0.5f + 0.4f;
            float b = cosf(ang) * 0.04f, c = sinf(ang) * 0.04f;
            SEG(f, 0.43f, b * 0.6f, c * 0.6f, 0.53f, b * 1.2f, c * 1.2f, 0.006f, L->accent, false);
            SP(f, 0.53f, b * 1.2f, c * 1.2f, f->u, 0.007f, 0.03f, L->accent, false);
        }
        O(f, 0.5f, 0, 0, 0.035f, 0.04f, time * 1.5f, gem, true);
        O(f, 0.5f, 0, 0, 0.02f, 0.055f, -time * 2.5f, v3_scale(v3(1, 1, 1), 0.6f + charge * 0.4f), true);
        if (fx) weapon_motes(f, L, 0.4f, 0.56f, time, seed);
        break;
    case WB_STAFF:
    default:
        B(f, 0.42f, 0, 0, 0.42f, 0.018f, 0.018f, L->wood, false);
        B(f, 0.3f, 0, 0, 0.07f, 0.021f, 0.021f, L->grip, false);
        B(f, 0.02f, 0, 0, 0.02f, 0.022f, 0.022f, L->accent, false); /* iron shoe */
        /* the head: two curling prongs holding the gem */
        SEG(f, 0.82f, 0, 0, 0.9f, 0.05f, 0, 0.011f, L->wood, false);
        SEG(f, 0.82f, 0, 0, 0.9f, -0.05f, 0, 0.011f, L->wood, false);
        SEG(f, 0.9f, 0.05f, 0, 0.97f, 0.02f, 0, 0.009f, L->wood, false);
        SEG(f, 0.9f, -0.05f, 0, 0.97f, -0.02f, 0, 0.009f, L->wood, false);
        B(f, 0.82f, 0, 0, 0.012f, 0.024f, 0.024f, L->accent, false);
        O(f, 0.91f, 0, 0, 0.03f, 0.045f, time * 2.0f, gem, true);
        if (fx) weapon_motes(f, L, 0.7f, 0.98f, time, seed);
        break;
    }
}

/* ---------- drawing in the world ---------- */

static void model(const WF *f, const Item *it, vec3 light, float time, bool fx, float seed, float draw, float charge, vec3 gem) {
    WLook L = look_of(it, light, time);
    const WeaponBase *wb = &WEAPON_BASES[it->base < WB_COUNT ? it->base : WB_SWORD];
    if (wb->wclass == WPN_SWORD) model_melee(f, &L, it->base, time, fx, seed);
    else if (it->base == WB_CROSSBOW) model_crossbow(f, &L, draw, time, fx, seed);
    else if (wb->wclass == WPN_BOW) model_bow(f, &L, it->base, draw, time, fx, seed, charge > 0); /* charge: an arrow on the string */
    else model_staff(f, &L, it->base, charge, gem, time, fx, seed);
}

/* Length of a model along u (to centre it on the ground). */
static float model_len(int base) {
    static const float LEN[WB_COUNT] = { 0.3f, 0.6f, 0.58f, 0.62f, 1.1f, 0.98f, 0.56f, 0.88f, 0.55f, 0.37f, 1.0f, 0.57f };
    return base < WB_COUNT ? LEN[base] : 0.6f;
}

void weapon_draw_ground(const Item *it, vec3 c, float yaw, vec3 light, float time, float seed) {
    WF f = { 0 };
    f.u = v3(cosf(yaw), 0, sinf(yaw));
    f.s = 0.9f;
    int wc = WEAPON_BASES[it->base].wclass;
    if (wc == WPN_BOW && it->base != WB_CROSSBOW) {
        /* bows stand up, slowly turning */
        f.u = v3(0, 1, 0);
        f.w = v3(cosf(yaw), 0, sinf(yaw));
        f.v = v3(-sinf(yaw), 0, cosf(yaw));
        f.o = v3_add(c, v3(0, 0.2f, 0));
    } else if (it->base == WB_CROSSBOW) {
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
    model(&f, it, light, time, true, seed, it->base == WB_CROSSBOW ? 1.0f : 0.0f, 0.0f, v3(0.55f, 0.6f, 1.0f));
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
    if (wc == WPN_BOW && it->base != WB_CROSSBOW) {
        /* held upright, string toward the archer */
        f.u = v3(0, 1, 0);
        f.w = v3_scale(v3_norm(v3(dir.x, 0, dir.z)), -1.0f);
        f.v = v3_cross(f.w, f.u);
    } else if (it->base == WB_CROSSBOW) {
        f.u = v3_norm(v3(dir.x, 0, dir.z));
        f.w = v3(0, 1, 0);
        f.v = v3_cross(f.u, f.w);
    }
    model(&f, it, light, time, true, seed, it->base == WB_CROSSBOW ? 1.0f : 0.0f, 1.0f, v3(0.55f, 0.6f, 1.0f));
}

/* ---------- the weapon in your hands ---------- */

void weapon_draw_view(Game *g, PlayerSlot *ps, vec3 light) {
    Stats *s = &ps->st;
    const Item *it = &s->equipped[s->weapon];
    float t = s->swing_len > 0 ? 1.0f - s->swing / s->swing_len : 1.0f; /* 0 at the start of an attack */
    bool attacking = s->swing > 0;
    float walk = sinf(ps->pl.bob * 2.0f) * 0.015f;
    float time = g->time;
    float seed = (float)ps->index * 17.0f;
    WF f = { 0 };
    f.up = v3(0, 1, 0);
    f.s = 1.0f;
    switch (s->weapon) {
    case WPN_SWORD: {
        int base = it->base;
        float reach = base == WB_GREATSWORD ? 0.85f : base == WB_DAGGER ? 1.15f : 1.0f;
        if (base == WB_SPEAR) {
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
        if (it->base == WB_CROSSBOW) {
            float kick = attacking && t < 0.2f ? (0.2f - t) * 0.4f : 0.0f;
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
        if (it->base == WB_WAND) {
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
