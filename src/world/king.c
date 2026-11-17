#include "world/king.h"

#include "world/dragons.h"
#include "world/layout.h"
#include "world/realms.h"
#include "world/towns.h"

/* The Old King and the way to him (docs/OLD_KING.md). Texts: %K is his name, %F the fortress. */

const KingKind KING_KINDS_DEF[KING_KINDS] = {
    [KK_TYRANT] = { "the Tyrant", "the old cruel king who never died, with an iron crown and an army of knights", "the Throne Hall",
        "the Breaking-Stone", KS_FORGE, 0,
        "You speak his true name, %K, and the knights round the throne remember they were men. One by one they kneel - to you. The iron crown falls from his head, and he is only old.",
        "You raise the Breaking-Stone and the iron crown splits. Every soul it bound in %F is free; the Tyrant is dust in a heap of iron.",
        "%K falls on the steps of his own throne, and %F has no king.", "his iron knights" },
    [KK_NECRO] = { "the Necromancer", "a lich-king whose life is kept in a phylactery hidden far from him", "the Necropolis",
        "the Phylactery", KS_RELIC, -1,
        "You call him by his living name, %K, the name he had before death. He remembers it - and with it, that he died. He lies down among the bones and is still.",
        "You break the Phylactery before his eyes. His death, held back for centuries, finds him all at once.",
        "%K is hewn apart, and without the phylactery to hold him he does not rise again.", "his risen dead" },
    [KK_MAGE] = { "the Old Mage", "an archmage who stopped time for himself and will not let it go", "the Tower of Stopped Hours",
        "the Lost Hour", KS_HOARD, 1,
        "You say his name, %K, and with it the year he was born. He laughs, a young man's laugh, and lets the hours go. He ages a thousand years in a breath and smiles.",
        "You give him back the Lost Hour. He holds it a long time, then turns the glass. Time takes him gently.",
        "His wards fail and %K falls, and every clock in %F starts again.", "his bound spirits" },
    [KK_DRAGON] = { "the Dragon-King", "a king who became a dragon to live forever", "the Dragon's Crown",
        "his Name-Scale", KS_HOARD, 1,
        "You name him, %K, a man's name, and the dragon remembers it was a man. It folds its wings, lies down on its gold and sleeps, and does not wake.",
        "You hold up the Name-Scale, the one scale he shed when he was still a man. He looks at it, and is ashamed, and flies out of %F forever.",
        "%K falls, the last of his brood, and the fortress shakes once more and is still.", "his brood" },
    [KK_HOLLOW] = { "the Hollow King", "a ghost bound to the throne by an oath he never kept", "the Empty Court",
        "the Oath-Ring", KS_RELIC, -1,
        "You speak his name, %K, and the oath he swore. He hears it kept at last, in your voice, and fades from the throne.",
        "You lay the Oath-Ring in his hand. The oath is fulfilled. The Empty Court is only empty.",
        "His court scatters and %K is torn from the throne he could not leave.", "his ghostly court" },
    [KK_CLOCK] = { "the Clockwork King", "an automaton that took the place of the king it served", "the Great Engine",
        "the Winding Key", KS_FORGE, 1,
        "You speak the name of the king it served, %K, and the engine stops to listen. It has been waiting all this time to hear it. It does not start again.",
        "You turn the Winding Key backward, and the Clockwork King unwinds to the day it was made: a servant, and glad of it.",
        "Its heart-engine bursts, and %K stands still forever in a rain of cogs.", "its brass servants" },
    [KK_BEAST] = { "the Beast-King", "a cursed king who is more wolf than man", "the Den",
        "the Cure", KS_BREW, -1,
        "You call him by his name, %K, and the man inside the beast answers. He comes out of it, weeping, old and free.",
        "He drinks the Cure from your hand. The wolf goes out of him. The king who remains has a great deal to answer for, and will.",
        "The beast that was %K dies snarling, and the howling in %F stops.", "his pack" },
    [KK_PRIEST] = { "the Priest-King of the Sealed", "a king who worships what is sealed below and would let it out", "the Black Altar",
        "the Seal-Stone", KS_RELIC, -1,
        "You speak his true name, %K, at his own altar, and his faithful hear what he was before he knelt. They turn from him. Alone, he is only a man at an altar.",
        "You set the Seal-Stone on the Black Altar. What he worshipped is sealed for good, and he with it.",
        "%K falls across his altar, and the thing beneath it is quiet again.", "his faithful" },
    [KK_ALCHEMIST] = { "the Alchemist-King", "a king who lives on elixirs, mad and always changing", "the Still",
        "the True Elixir", KS_BREW, -1,
        "You call him %K, the name he had before the first elixir. He stops changing, for a moment, and in that moment he is sane, and asks you to end it. Gently, you do.",
        "He drinks the True Elixir you brewed. For the first time in an age he is only himself, and himself is mortal.",
        "%K dies in a cloud of every colour, and the stills of %F go cold.", "his changed servants" },
    [KK_STONE] = { "the King-in-the-Stone", "the first king, asleep under the fortress - perhaps not evil at all", "the Sleeping Hall",
        "the Dream-Lantern", KS_HOARD, -1,
        "You wake him with his name, %K. He opens his eyes after a thousand years, sees %F, and weeps, and blesses you, and the long night of the fortress ends.",
        "You light the Dream-Lantern by his bed. His dreams, that held %F in its long night, go quiet. He sleeps on in peace; the fortress wakes.",
        "You defeat the guardians of his dream, and %K sleeps on, free of them, at rest at last.", "his dream-guardians" },
};

const char *KING_STEP_NAMES[KS_COUNT] = { "the herald", "the witness", "the true name", "the vision", "the dragon", "the hoard", "the allegiance",
                                          "the forge", "the brew", "the realm", "the relic", "the lieutenant", "the seal", "the wyrm", "the throne" };

int king_kind_of(u64 seed) { return (int)(mix64(seed ^ 0x01D1C) % KING_KINDS); }

void king_throne_cell(u64 seed, int *gx, int *gy, int *gz) {
    u64 h = mix64(seed ^ 0x7A0E);
    int kind = king_kind_of(seed);
    static const int DX[8] = { 4, 3, 0, -3, -4, -3, 0, 3 }, DZ[8] = { 0, 3, 4, 3, 0, -3, -4, -3 };
    int d = (int)(h % 8);
    *gx = DX[d];
    *gz = DZ[d];
    if (*gx == 1 && *gz == 0) *gx = 4; /* (never where the first town is) */
    int dir = KING_KINDS_DEF[kind].dir;
    *gy = dir < 0 ? -2 : dir > 0 ? 2 : 0;
}

static void king_name(int kind, u64 seed, char *out, int n) {
    static const char *FIRST[12] = { "Aldric", "Morvaine", "Ulthar", "Sevren", "Corvin", "Halvard", "Ithel", "Bregor", "Osric", "Vael", "Theodric", "Annoch" };
    static const char *EPI[KING_KINDS] = { "the Ninth", "the Deathless", "the Unaging", "the Wyrmcrowned", "the Hollow", "the Wound", "the Wolf-Crowned",
                                           "the Kneeling", "the Distilled", "the Sleeper" };
    snprintf(out, (size_t)n, "%s %s", FIRST[mix64(seed ^ 0x4A3E) % 12], EPI[kind]);
}

/* ---------- places ---------- */

typedef struct Cand { u64 key; int cx, cy, cz, people_or_kind; float d; } Cand;

static int find_structs(const struct World *w, int variant, Cand *out, int max) {
    int n = 0;
    for (int gy = -2; gy <= 2; gy++)
        for (int gz = -5; gz <= 5; gz++)
            for (int gx = -5; gx <= 5; gx++) {
                Structure s;
                if (!world_cell_structure(w, gx, gy, gz, &s) || s.type != STRUCT_HALL || s.variant != variant) continue;
                if (n >= max) return n;
                Cand *c = &out[n++];
                c->key = s.seed;
                c->cx = s.cx0 + s.sx / 2; c->cy = s.cy0; c->cz = s.cz0 + s.sz / 2;
                c->people_or_kind = s.people;
                c->d = sqrtf((float)(c->cx * c->cx + c->cz * c->cz)) + 1.5f * fabsf((float)c->cy);
            }
    return n;
}

/* the candidate in [lo, hi] (danger-distance) not used yet, by the seed; else the nearest unused */
static int pick(Cand *c, int n, float lo, float hi, u64 *used, int nused, Rng *r) {
    int ok[64], k = 0;
    for (int i = 0; i < n && k < 64; i++) {
        bool u = false;
        for (int j = 0; j < nused; j++) u |= used[j] == c[i].key;
        if (!u && c[i].d >= lo && c[i].d <= hi) ok[k++] = i;
    }
    if (k) return ok[rng_range(r, 0, k - 1)];
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < n; i++) {
        bool u = false;
        for (int j = 0; j < nused; j++) u |= used[j] == c[i].key;
        float d = fabsf(c[i].d - (lo + hi) * 0.5f);
        if (!u && d < bd) { bd = d; best = i; }
    }
    return best;
}

/* An ordinary chunk at about this distance, toward a direction, on these layers. */
static bool place_chunk(const struct World *w, Rng *r, float lo, float hi, int cy_lo, int cy_hi, float ang0, float spread, KingStep *st) {
    for (int t = 0; t < 80; t++) {
        float a = ang0 + (rng_float(r) - 0.5f) * spread, d = lo + rng_float(r) * (hi - lo);
        int cx = (int)lroundf(cosf(a) * d), cz = (int)lroundf(sinf(a) * d), cy = rng_range(r, cy_lo, cy_hi);
        if (world_is_exterior(cx, cy, cz) || abs(cx) + abs(cz) < 3) continue;
        Structure s;
        if (world_structure_at(w, cx, cy, cz, &s)) continue;
        st->cx = cx; st->cy = cy; st->cz = cz;
        return true;
    }
    return false;
}

static bool in(const u8 *a, int n, int v) {
    for (int i = 0; i < n; i++) if (a[i] == v) return true;
    return false;
}

static bool make_way(const struct World *w, u32 realms, int attempt, KingWay *K) {
    memset(K, 0, sizeof *K);
    u64 seed = w->seed;
    K->seed = seed;
    K->kind = king_kind_of(seed);
    king_name(K->kind, seed, K->name, sizeof K->name);
    king_throne_cell(seed, &K->tgx, &K->tgy, &K->tgz);
    const KingKind *KD = &KING_KINDS_DEF[K->kind];
    Rng r = rng_make(mix64(seed ^ 0x6A11) + (u64)attempt * 0x9E37);
    float throne_ang = atan2f((float)K->tgz, (float)K->tgx);
    /* the kinds of step */
    u8 A[3], B[4], C[2];
    A[0] = KS_HERALD; A[1] = KS_TRUENAME; A[2] = rng_chance(&r, 0.5f) ? KS_WITNESS : KS_VISION;
    int nb = 0;
    B[nb++] = KD->artefact_step;
    if (!in(B, nb, KS_DRAGON) && !in(B, nb, KS_HOARD)) B[nb++] = rng_chance(&r, 0.6f) ? KS_DRAGON : KS_HOARD; /* a dragon on every way */
    if (A[2] != KS_VISION && !in(B, nb, KS_FORGE)) B[nb++] = KS_FORGE;                                    /* and the Sanctum */
    u8 pool[7] = { KS_DRAGON, KS_HOARD, KS_ALLEGIANCE, KS_FORGE, KS_BREW, KS_REALM, KS_RELIC };
    for (int t = 0; t < 40 && nb < 4; t++) {
        int c = pool[rng_range(&r, 0, 6)];
        if (in(B, nb, c)) continue;
        if (c == KS_REALM && !realms) continue;
        B[nb++] = (u8)c;
    }
    if (nb < 4) return false;
    C[0] = rng_chance(&r, 0.5f) ? KS_LIEUTENANT : KS_SEAL;
    C[1] = K->kind == KK_DRAGON ? KS_WYRM : (rng_chance(&r, 0.5f) ? KS_WYRM : (C[0] == KS_LIEUTENANT ? KS_SEAL : KS_LIEUTENANT));
    for (int i = 0; i < 3; i++) { K->step[i].type = A[i]; K->step[i].group = 0; }
    for (int i = 0; i < 4; i++) { K->step[3 + i].type = B[i]; K->step[3 + i].group = 1; }
    K->step[7].type = C[0]; K->step[7].group = 2;
    K->step[8].type = C[1]; K->step[8].group = 3;
    K->step[9].type = KS_THRONE; K->step[9].group = 4;
    /* the places */
    Cand towns[64], lairs[64];
    int nt = find_structs(w, HALL_TOWN, towns, 64), nl = find_structs(w, HALL_LAIR, lairs, 64);
    u64 used[16];
    int nused = 0;
    u64 herald_town = 0;
    for (int i = 0; i < KING_STEPS; i++) {
        KingStep *st = &K->step[i];
        float lo = st->group == 0 ? 4 : st->group == 1 ? 10 : 18, hi = st->group == 0 ? 14 : st->group == 1 ? 26 : 34;
        switch (st->type) {
        case KS_HERALD: case KS_ALLEGIANCE: case KS_BREW: {
            int k = -1;
            if (st->type == KS_HERALD) { /* the nearest town: the one east of the gate */
                float bd = 1e9f;
                for (int j = 0; j < nt; j++) if (towns[j].d < bd) { bd = towns[j].d; k = j; }
            } else k = pick(towns, nt, lo, hi, used, nused, &r);
            if (k < 0 && herald_town) { /* (the herald's town again) */
                for (int j = 0; j < nt; j++) if (towns[j].key == herald_town) k = j;
            }
            if (k < 0) return false;
            st->key = towns[k].key; st->cx = towns[k].cx; st->cy = towns[k].cy; st->cz = towns[k].cz; st->arg = towns[k].people_or_kind;
            if (st->type == KS_HERALD) herald_town = st->key;
            else if (nused < 16) used[nused++] = st->key;
            break;
        }
        case KS_DRAGON: case KS_HOARD: case KS_WYRM: {
            int k = pick(lairs, nl, lo, hi + 6, used, nused, &r);
            if (st->type == KS_WYRM) { /* the guardian: the lair nearest the throne */
                float bd = 1e9f;
                int tx = K->tgx * 6 + 2, tz = K->tgz * 6 + 2, ty = K->tgy * 6;
                for (int j = 0; j < nl; j++) {
                    bool u = false;
                    for (int q = 0; q < nused; q++) u |= used[q] == lairs[j].key;
                    float d = sqrtf((float)((lairs[j].cx - tx) * (lairs[j].cx - tx) + (lairs[j].cz - tz) * (lairs[j].cz - tz))) + 1.5f * fabsf((float)(lairs[j].cy - ty));
                    if (!u && d < bd) { bd = d; k = j; }
                }
            }
            if (k < 0) return false;
            st->key = lairs[k].key; st->cx = lairs[k].cx; st->cy = lairs[k].cy; st->cz = lairs[k].cz; st->arg = lairs[k].people_or_kind;
            if (nused < 16) used[nused++] = st->key;
            break;
        }
        case KS_REALM: {
            int best = -1;
            for (int rr = 0; rr < REALM_COUNT; rr++)
                if ((realms >> rr) & 1 && !(REALMS[rr].rules & (RR_STORY | RR_SPARED)) && (best < 0 || REALMS[rr].level < REALMS[best].level)) best = rr;
            if (best < 0) return false;
            st->arg = best;
            break;
        }
        case KS_VISION: case KS_FORGE: break; /* (in the Sanctum) */
        case KS_THRONE:
            st->cx = K->tgx * 6 + 2; st->cy = K->tgy * 6; st->cz = K->tgz * 6 + 2; /* (the hall's middle chunk, layout.c) */
            break;
        default: { /* an ordinary chunk: the witness, the stele, a relic, a lieutenant, a seal */
            int cy_lo = st->group <= 1 ? -1 : MINI(0, K->tgy * 6), cy_hi = st->group <= 1 ? 1 : MAXI(0, K->tgy * 6);
            if (st->group >= 2) { cy_lo = MINI(cy_lo, cy_hi); cy_hi = MAXI(cy_lo, cy_hi); }
            float ang = st->group >= 2 ? throne_ang : rng_float(&r) * 6.283f;
            if (!place_chunk(w, &r, lo, hi, cy_lo, cy_hi, ang, st->group >= 2 ? 1.2f : 6.283f, st)) return false;
            break;
        }
        }
    }
    /* checks: no two places in one chunk, the danger rises from group to group */
    for (int i = 0; i < KING_STEPS; i++)
        for (int j = i + 1; j < KING_STEPS; j++) {
            const KingStep *a = &K->step[i], *b = &K->step[j];
            bool placed = a->type != KS_VISION && a->type != KS_FORGE && a->type != KS_REALM && b->type != KS_VISION && b->type != KS_FORGE && b->type != KS_REALM;
            if (placed && a->key == 0 && b->key == 0 && a->cx == b->cx && a->cy == b->cy && a->cz == b->cz) return false;
        }
    K->valid = true;
    return true;
}

const KingWay *king_way(const struct World *w, u32 realms) {
    static KingWay cache;
    static u32 cache_realms;
    if (cache.valid && cache.seed == w->seed && cache_realms == realms) return &cache;
    for (int attempt = 0; attempt < 16; attempt++)
        if (make_way(w, realms, attempt, &cache)) { cache_realms = realms; return &cache; }
    /* (a way without the realm or a second lair, at worst: the fortress always has towns) */
    for (int attempt = 0; attempt < 16; attempt++)
        if (make_way(w, 0, attempt, &cache)) { cache_realms = realms; return &cache; }
    cache.valid = false;
    return &cache;
}
