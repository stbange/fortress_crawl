#include "world/story.h"
#include "world/pocket.h"

#include <stdio.h>
#include <string.h>

/* Text is short and grim: fragments, not prose. Templates use {key} for context
 * values and #symbol# for a random pick from a word list; picks come from an RNG
 * seeded by the thing being described, so the same place always says the same. */

const char *MOTIVE_NAMES[MOTIVE_COUNT] = { "REVENGE", "GLORY", "RICHES", "TRUTH" };
const char *MOTIVE_PITCH[MOTIVE_COUNT] = {
    "Someone in this fortress owes you blood.",
    "No songs are sung of those who stayed home.",
    "The old kings went into the dark with their gold.",
    "What did they seal beneath the mountain?",
};
const char *CULTURE_NAMES[CULT_COUNT] = { "old", "northern", "southern", "eastern", "western" };

#define LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define PICK(r, a) (a)[rng_range((r), 0, LEN(a) - 1)]

/* ---------- template expansion ---------- */

typedef struct Ctx { const char *key[24]; const char *val[24]; int n; } Ctx;

static void cset(Ctx *c, const char *k, const char *v) {
    for (int i = 0; i < c->n; i++)
        if (!strcmp(c->key[i], k)) { c->val[i] = v; return; }
    if (c->n < LEN(c->key)) { c->key[c->n] = k; c->val[c->n] = v; c->n++; }
}

static const char *const S_KIN[] = { "brother", "sister", "mother", "father", "son", "daughter", "wife", "husband" };
static const char *const S_TRADE[] = { "miner", "mason", "gravedigger", "lamplighter", "warden", "smith", "bellringer", "scribe" };
static const char *const S_DOOR[] = { "door", "stair", "well", "lower gate", "shaft", "crypt" };
static const char *const S_SEALMAT[] = { "salt and lead", "iron and prayer", "our own dead", "silver nails", "nine names", "wet clay" };
static const char *const S_LIGHT[] = { "lamps", "torches", "candles", "last lantern", "beacon" };
static const char *const S_NUM[] = { "three", "nine", "forty", "eleven", "a hundred", "seven" };
static const char *const S_ITEM[] = { "signet ring", "wedding band", "father's knife", "letters", "wooden horse", "regimental colours",
                                      "prayer beads", "lock of hair", "silver locket", "brother's ashes" };
static const char *const S_FEAR[] = { "the counting", "the warm walls", "the bells below", "the singing", "the dark that breathes",
                                      "the footsteps that match mine" };

typedef struct Sym { const char *name; const char *const *opts; int n; } Sym;
static const Sym SYMS[] = {
    { "kin", S_KIN, LEN(S_KIN) },       { "trade", S_TRADE, LEN(S_TRADE) }, { "door", S_DOOR, LEN(S_DOOR) },
    { "sealmat", S_SEALMAT, LEN(S_SEALMAT) }, { "light", S_LIGHT, LEN(S_LIGHT) }, { "num", S_NUM, LEN(S_NUM) },
    { "item", S_ITEM, LEN(S_ITEM) },    { "fear", S_FEAR, LEN(S_FEAR) },
};

static void put(char *out, int n, int *at, const char *s, int len) {
    for (int i = 0; i < len && *at < n - 1; i++) out[(*at)++] = s[i];
    out[*at] = 0;
}

static void expand_into(char *out, int n, int *at, const char *t, Rng *r, const Ctx *c, int depth) {
    for (const char *p = t; *p;) {
        if ((*p == '{' || *p == '#') && depth < 4) {
            char close = *p == '{' ? '}' : '#';
            const char *e = strchr(p + 1, close);
            if (!e) { put(out, n, at, p, 1); p++; continue; }
            int len = (int)(e - p - 1);
            const char *rep = NULL;
            if (*p == '{') {
                for (int i = 0; i < c->n && !rep; i++)
                    if ((int)strlen(c->key[i]) == len && !strncmp(c->key[i], p + 1, (size_t)len)) rep = c->val[i];
            } else {
                for (int i = 0; i < LEN(SYMS) && !rep; i++)
                    if ((int)strlen(SYMS[i].name) == len && !strncmp(SYMS[i].name, p + 1, (size_t)len))
                        rep = SYMS[i].opts[rng_range(r, 0, SYMS[i].n - 1)];
            }
            if (rep) expand_into(out, n, at, rep, r, c, depth + 1);
            p = e + 1;
        } else {
            put(out, n, at, p, 1);
            p++;
        }
    }
}

static void expand(char *out, int n, const char *t, Rng *r, const Ctx *c) {
    int at = 0;
    out[0] = 0;
    expand_into(out, n, &at, t, r, c, 0);
    /* a capital at the start, after a full stop, and at the start of a quotation */
    for (int i = 0; out[i]; i++) {
        if (out[i] < 'a' || out[i] > 'z') continue;
        bool start = i == 0 || (i >= 2 && out[i - 1] == ' ' && (out[i - 2] == '.' || out[i - 2] == '!' || out[i - 2] == '?')) ||
                     (out[i - 1] == '\'' && (i == 1 || out[i - 2] == ' '));
        if (start) out[i] = (char)(out[i] - 32);
    }
}

/* ---------- names ---------- */

static const char *const SYL_A[CULT_COUNT][10] = {
    { "Var", "Hal", "Mor", "Ced", "Ost", "Ul", "Bran", "Ed", "Ger", "Ald" },
    { "Sk", "Hr", "Vy", "Th", "Bj", "Ey", "Gr", "Sv", "Ulf", "Ing" },
    { "Am", "Kha", "Zar", "Sef", "Ish", "Tam", "Ra", "Nef", "Ash", "Il" },
    { "Mir", "Vel", "Rad", "Bog", "Yar", "Zla", "Ol", "Dra", "Lin", "Shen" },
    { "Mal", "Cor", "Aum", "Leof", "Ber", "Gwy", "Dun", "Ys", "Cael", "Mor" },
};
static const char *const SYL_B[CULT_COUNT][5] = {
    { "", "a", "e", "i", "o" }, { "a", "o", "i", "e", "y" }, { "a", "e", "i", "u", "" },
    { "o", "a", "e", "i", "" }, { "e", "a", "o", "i", "" },
};
static const char *const SYL_C[CULT_COUNT][8] = {
    { "ric", "mund", "wyn", "hold", "ard", "wen", "os", "eth" },
    { "ld", "rn", "gga", "frid", "vik", "mund", "dis", "ulf" },
    { "rah", "mun", "ket", "shar", "zad", "hep", "sut", "im" },
    { "slav", "mir", "ka", "gor", "ana", "dan", "wei", "rin" },
    { "vane", "dric", "mar", "wen", "oc", "aine", "ric", "ys" },
};
static const char *const EPITHETS[] = {
    "the Grey", "Ash-Tongue", "the Flayed", "Nine-Fingers", "the Hollow", "of the Black Lantern", "the Unburied",
    "Saltmouth", "the Patient", "Bone-Crowned", "the Weeping", "Hookhand", "the Last", "Ember-Eyed", "the Thrice-Hanged",
    ", Mother of Worms", "the Blind", "Coldhand", "the Smiling", "Gravewed", "the Unwashed", "Lantern-Jaw", "the Quiet",
};

static void make_name(u64 h, int cult, char *out, int n) {
    Rng r = rng_make(h);
    snprintf(out, (size_t)n, "%s%s%s", PICK(&r, SYL_A[cult]), PICK(&r, SYL_B[cult]), PICK(&r, SYL_C[cult]));
}

static void make_named(u64 h, int cult, char *out, int n) {
    char base[32];
    make_name(h, cult, base, sizeof base);
    Rng r = rng_make(h ^ 0xE91);
    const char *ep = PICK(&r, EPITHETS);
    snprintf(out, (size_t)n, ep[0] == ',' ? "%s%s" : "%s %s", base, ep); /* "Haleric, Mother of Worms" */
}

static int flavor_culture(int flavor) { return FLAVORS[flavor].axis; }

/* ---------- world myth ---------- */

void story_world(u64 seed, StoryWorld *out) {
    memset(out, 0, sizeof *out);
    out->seed = seed;
    Rng r = rng_make(seed ^ 0x5701);
    static const char *const SUFFIX[] = { "hold", "gard", "mark", "fast", "wall", "keep", "burg" };
    /* one syllable and a suffix: "Ostgard", "Halmark" */
    snprintf(out->fortress, sizeof out->fortress, "%s%s", PICK(&r, SYL_A[CULT_OLD]), PICK(&r, SUFFIX));
    static const char *const BUILDERS[] = { "the Ninefold Kings", "the Seven Wardens", "the Iron Synod", "the Lantern Kings",
                                            "the First Masons", "the Grey Abbots", "the Salt Queens", "the Old Covenant" };
    snprintf(out->builders, sizeof out->builders, "%s", PICK(&r, BUILDERS));
    static const char *const SEALED[] = { "the Pale Hunger", "the Under-Sun", "the Mouth Below", "the Red Silence", "the Sleeper",
                                          "the Hollow King", "the Ninth Dark", "the Grey Tide", "the Thing That Counts" };
    snprintf(out->sealed, sizeof out->sealed, "%s", PICK(&r, SEALED));
    static const char *const FALL[] = { "the Night of the Open Gate", "the Long Bell", "the Ashen Winter", "the Quiet Year",
                                        "the Last Siege", "the Night Without Lamps" };
    snprintf(out->fall, sizeof out->fall, "%s", PICK(&r, FALL));
}

void story_personal(u64 seed, int slot, Personal *out) {
    memset(out, 0, sizeof *out);
    Rng r = rng_make(seed ^ ((u64)(slot + 1) * 0x9E3779B97F4A7C15ULL) ^ 0x9E85);
    static const char *const VSUF[] = { "mere", "ford", "stead", "holm", "wick", "ham", "brook" };
    snprintf(out->village, sizeof out->village, "%s%s", PICK(&r, SYL_A[CULT_OLD]), PICK(&r, VSUF));
    snprintf(out->kin, sizeof out->kin, "%s", PICK(&r, S_KIN));
}

/* ---------- eras: depth is time, height is power ---------- */

static const char *const ERAS[ST_COUNT] = {
    [ST_CORE] = "the Sealing",          [ST_MAW] = "the Devouring",      [ST_IRONHELL] = "the Chaining",
    [ST_INFERNO] = "the War of Fire",   [ST_CRYSTAL] = "the Hardening",  [ST_LIMBO] = "the Drowning",
    [ST_FUNGAL] = "the Long Rot",       [ST_OSSUARY] = "the Great Burial", [ST_ROOTS] = "the First Hearths",
    [ST_MINES] = "the Deep Delving",    [ST_DUNGEON] = "the Last Siege",
    [ST_CASTLE] = "the reign of {builders}", [ST_SPIRES] = "the Ascent", [ST_AERIE] = "the Taming of Wings",
    [ST_CLOUDS] = "the Long Prayer",    [ST_GARDENS] = "the Green Age",  [ST_HEAVEN] = "the Exaltation",
    [ST_SUNCOURT] = "the Noon Without End", [ST_STORM] = "the Breaking of the Sky", [ST_ORRERY] = "the Counting of Stars",
    [ST_ASTRAL] = "the time before stars",
};

static const char *const PLACE_NOUN[ST_COUNT][4] = {
    [ST_CORE] = { "Heart", "Wound", "Seal", "Pit" },
    [ST_MAW] = { "Gullet", "Throat", "Teeth", "Belly" },
    [ST_IRONHELL] = { "Foundry", "Chains", "Anvils", "Racks" },
    [ST_INFERNO] = { "Furnace", "Pits", "Brand", "Kilns" },
    [ST_CRYSTAL] = { "Geodes", "Prisms", "Shards", "Facets" },
    [ST_LIMBO] = { "Mire", "Grey Reaches", "Fen", "Shallows" },
    [ST_FUNGAL] = { "Spore Halls", "Caps", "Mycelium", "Rot" },
    [ST_OSSUARY] = { "Ossuary", "Charnel", "Catacombs", "Bonepits" },
    [ST_ROOTS] = { "Roots", "Hollows", "Grottos", "Burrows" },
    [ST_MINES] = { "Shafts", "Delvings", "Seams", "Workings" },
    [ST_DUNGEON] = { "Cells", "Undercroft", "Galleries", "Vaults" },
    [ST_CASTLE] = { "Keep", "Court", "Bastion", "Ramparts" },
    [ST_SPIRES] = { "Spires", "Belfries", "Needles", "Lanterns" },
    [ST_AERIE] = { "Aeries", "Eyries", "Roosts", "Crags" },
    [ST_CLOUDS] = { "Cloisters", "Mists", "Gyres", "Bridges" },
    [ST_GARDENS] = { "Gardens", "Terraces", "Orchards", "Bowers" },
    [ST_HEAVEN] = { "Choir", "Sanctum", "Gardens", "Thrones" },
    [ST_SUNCOURT] = { "Court", "Dais", "Zenith", "Halls of Noon" },
    [ST_STORM] = { "Crown", "Thunderheads", "Rods", "Squalls" },
    [ST_ORRERY] = { "Orrery", "Wheels", "Spheres", "Clockwork" },
    [ST_ASTRAL] = { "Void", "Starfall", "Silence", "Drift" },
};

/* ---------- factions ---------- */

static void faction_name(u64 seed, int cult, int idx, char *out, int n) {
    static const char *const ADJ[] = { "Grey", "Ashen", "Pale", "Ninefold", "Black", "Hollow", "Drowned", "Iron",
                                       "Salt", "Lantern", "Weeping", "Unburied", "Red", "Silent", "Last", "Thorned" };
    static const char *const GROUP[] = { "Wardens", "Choir", "Court", "Hand", "Brotherhood", "Host", "Kin", "Covenant",
                                         "Watch", "Order", "Sisterhood", "Company" };
    Rng r = rng_make(seed ^ ((u64)(cult * 16 + idx + 1) * 0xC2B2AE3D27D4EB4FULL));
    snprintf(out, (size_t)n, "the %s %s", PICK(&r, ADJ), PICK(&r, GROUP));
}

/* ---------- domains ---------- */

#define DOM_W 4
#define DOM_H 2

static void dom_centre(u64 seed, int gx, int gy, int gz, int *cx, int *cy, int *cz) {
    u64 h = hash3(seed, gx, gy, gz, 0xD0A1);
    *cx = gx * DOM_W + (int)(h % DOM_W);
    *cy = gy * DOM_H + (int)((h >> 8) % DOM_H);
    *cz = gz * DOM_W + (int)((h >> 16) % DOM_W);
}

static void fill_domain(const World *w, int gx, int gy, int gz, Domain *d) {
    memset(d, 0, sizeof *d);
    d->gx = gx; d->gy = gy; d->gz = gz;
    dom_centre(w->seed, gx, gy, gz, &d->ccx, &d->ccy, &d->ccz);
    d->id = hash3(w->seed, gx, gy, gz, 0xD0A2) | 1;
    d->culture = (u8)flavor_culture(world_flavor(w, d->ccx, d->ccy, d->ccz));
    d->stratum = (u8)world_stratum(w, d->ccx, d->ccy, d->ccz);
    d->level = (int)world_difficulty(d->ccx, d->ccy, d->ccz);
    Rng r = rng_make(d->id);
    static const float W_NEAR[DS_COUNT] = { 4, 3, 1, 1, 1 }, W_FAR[DS_COUNT] = { 3, 2, 2, 2, 2 };
    d->state = (u8)pick_weighted(rng_float(&r), d->level < 4 ? W_NEAR : W_FAR, DS_COUNT);
    /* factions span several domains of one culture */
    int fx = floordiv(gx, 3), fy = floordiv(gy, 2), fz = floordiv(gz, 3);
    int fidx = (int)(hash3(w->seed, fx, fy, fz, 0xFAC7 + d->culture) % 3);
    faction_name(w->seed, d->culture, fidx, d->faction, sizeof d->faction);
    faction_name(w->seed, (d->culture + 1 + (int)(r.s % 4)) % CULT_COUNT, (fidx + 1) % 3, d->rival, sizeof d->rival);
    char root[32];
    make_name(rng_next(&r), d->culture, root, sizeof root);
    const char *noun = PICK(&r, PLACE_NOUN[d->stratum]);
    switch (rng_range(&r, 0, 2)) {
    case 0: snprintf(d->name, sizeof d->name, "the %s of %s", noun, root); break;
    case 1: snprintf(d->name, sizeof d->name, "%s's %s", root, noun); break;
    default: snprintf(d->name, sizeof d->name, "%s %s", root, noun); break;
    }
    StoryWorld sw;
    story_world(w->seed, &sw);
    Ctx c = { 0 };
    cset(&c, "builders", sw.builders);
    expand(d->era, sizeof d->era, ERAS[d->stratum], &r, &c);
    d->era[0] = (char)(d->era[0] >= 'A' && d->era[0] <= 'Z' ? d->era[0] + 32 : d->era[0]);
}

/* The domain cell a chunk belongs to (cheap: no names). */
static void domain_cell(u64 seed, int cx, int cy, int cz, int *ogx, int *ogy, int *ogz) {
    int gx0 = floordiv(cx, DOM_W), gy0 = floordiv(cy, DOM_H), gz0 = floordiv(cz, DOM_W);
    int best = 1 << 30;
    *ogx = gx0; *ogy = gy0; *ogz = gz0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++) {
                int ccx, ccy, ccz;
                dom_centre(seed, gx0 + dx, gy0 + dy, gz0 + dz, &ccx, &ccy, &ccz);
                int ex = ccx - cx, ey = ccy - cy, ez = ccz - cz;
                int d = ex * ex + 4 * ey * ey + ez * ez; /* domains are flatter than tall */
                if (d < best) { best = d; *ogx = gx0 + dx; *ogy = gy0 + dy; *ogz = gz0 + dz; }
            }
}

void story_domain(const World *w, int cx, int cy, int cz, Domain *out) {
    int bx, by, bz;
    domain_cell(w->seed, cx, cy, cz, &bx, &by, &bz);
    fill_domain(w, bx, by, bz, out);
}

void story_domain_line(const Domain *d, char *out, int n) {
    switch (d->state) {
    case DS_HELD: snprintf(out, (size_t)n, "held by %s", d->faction); break;
    case DS_ABANDONED: snprintf(out, (size_t)n, "empty since %s", d->era); break;
    case DS_PLAGUED: snprintf(out, (size_t)n, "plague ground"); break;
    case DS_WARRING: snprintf(out, (size_t)n, "%s against %s", d->faction, d->rival); break;
    default: snprintf(out, (size_t)n, "sealed since %s", d->era); break;
    }
}

/* ---------- sites: where named things live ---------- */

static int site_score(const ChunkInfo *ci, bool big) {
    switch (ci->arch) {
    case ARCH_EXTERIOR: return -1;
    case ARCH_STRUCTURE: return ci->st.type == STRUCT_CHASM ? 1 : 5;
    case ARCH_ARENA: return 5;
    case ARCH_ROOMS: return big ? 2 : 3;
    case ARCH_CELLS: return 2;
    case ARCH_CAVE: return big ? 2 : 2;
    case ARCH_MINE: return 1;
    default: return 0; /* crawlspaces and voids */
    }
}

/* The best chunk for something to live in, near (cx,cy,cz); with dom set, only chunks of
 * that domain cell count (so two lords never share a chunk). */
static void pick_site(const World *w, int cx, int cy, int cz, int radius, bool big, const int *dom, int *ox, int *oy, int *oz) {
    int best = -1000000;
    *ox = cx; *oy = cy; *oz = cz;
    for (int dy = -radius; dy <= radius; dy++)
        for (int dz = -radius; dz <= radius; dz++)
            for (int dx = -radius; dx <= radius; dx++) {
                if (abs(dy) > 1) continue;
                if (dom) {
                    int gx, gy, gz;
                    domain_cell(w->seed, cx + dx, cy + dy, cz + dz, &gx, &gy, &gz);
                    if (gx != dom[0] || gy != dom[1] || gz != dom[2]) continue;
                }
                ChunkInfo ci;
                world_chunk_info(w, cx + dx, cy + dy, cz + dz, &ci);
                int s = site_score(&ci, big);
                if (s < 0) continue;
                int v = s * 100 - (dx * dx + dy * dy + dz * dz);
                if (v > best) { best = v; *ox = cx + dx; *oy = cy + dy; *oz = cz + dz; }
            }
}

void story_lord(const World *w, const Domain *d, Named *out) {
    memset(out, 0, sizeof *out);
    out->kind = NK_LORD;
    out->chapter = -1;
    out->key = hash3(w->seed, d->gx, d->gy, d->gz, 0x10AD) | 1;
    out->culture = d->culture;
    int dom[3] = { d->gx, d->gy, d->gz };
    pick_site(w, d->ccx, d->ccy, d->ccz, 1, false, dom, &out->cx, &out->cy, &out->cz);
    Rng r = rng_make(out->key);
    static const u8 MON[] = { SM_BRUTE, SM_HORNED, SM_SKELETON, SM_BRUTE, SM_WISP };
    out->mon = PICK(&r, MON);
    out->scale = 1.35f;
    out->hp_mult = 4.0f;
    out->dmg_mult = 1.3f;
    make_named(out->key, d->culture, out->name, sizeof out->name);
    static const char *const T[] = { "lord of %.36s", "keeper of %.34s", "who holds %.34s", "warden of %.34s" };
    snprintf(out->title, sizeof out->title, PICK(&r, T), d->name);
}

bool story_lord_here(const World *w, int cx, int cy, int cz, Named *out) {
    if (world_is_pocket(cx, cy, cz)) return false;
    Domain d;
    story_domain(w, cx, cy, cz, &d);
    if (d.level < 2) return false; /* the first rooms behind the gate are quiet */
    story_lord(w, &d, out);
    return out->cx == cx && out->cy == cy && out->cz == cz;
}

/* ---------- the metaplot: arcs and chapters ---------- */

enum { AD_DOWN, AD_UP, AD_NORTH, AD_SOUTH, AD_EAST, AD_WEST, AD_COUNT };

static int arc_dir(u64 seed, int arc) {
    static const float W[AD_COUNT] = { 3, 1, 2, 2, 2, 2 };
    int prev = -1, d = 0;
    for (int a = 0; a <= arc; a++) {
        float wts[AD_COUNT];
        memcpy(wts, W, sizeof wts);
        if (prev >= 0) wts[prev] = 0;
        d = pick_weighted(hash_unit(hash3(seed, a, 0, 0, 0xA2C5)), wts, AD_COUNT);
        prev = d;
    }
    return d;
}

static int arc_culture(u64 seed, int arc) {
    switch (arc_dir(seed, arc)) {
    case AD_NORTH: return CULT_NORTH;
    case AD_SOUTH: return CULT_SOUTH;
    case AD_EAST: return CULT_EAST;
    case AD_WEST: return CULT_WEST;
    default: return CULT_OLD;
    }
}

static void power_title(u64 seed, int arc, const StoryWorld *sw, char *out, int n) {
    static const char *const T[CULT_COUNT][3] = {
        { "the Last Steward", "the Gatewarden", "the Hollow Regent" },
        { "Queen under the Ice", "Jarl of the Long Winter", "the Frost-Crowned" },
        { "Prophet of the Ember", "Sultan of Ash", "the Burning Mother" },
        { "Tsar of Bones", "Khan of the Steppe Below", "the Jade Regent" },
        { "Abbot of the Drowned Choir", "the Black Bishop", "Mother of Roots" },
    };
    Rng r = rng_make(hash3(seed, arc, 0, 0, 0x7171));
    int d = arc_dir(seed, arc);
    if (d == AD_DOWN) snprintf(out, (size_t)n, "Herald of %.36s", sw->sealed);
    else if (d == AD_UP) snprintf(out, (size_t)n, "the Steward Above");
    else snprintf(out, (size_t)n, "%s", T[arc_culture(seed, arc)][rng_range(&r, 0, 2)]);
}

/* Who chapter c is, without finding where it lives (cheap). */
static void chapter_identity(u64 seed, int chapter, Named *out) {
    memset(out, 0, sizeof *out);
    int arc = chapter / 3, rank = chapter % 3;
    StoryWorld sw;
    story_world(seed, &sw);
    out->chapter = chapter;
    out->kind = rank;
    out->key = hash3(seed, chapter, 0, 0, 0xC4A9) | 1;
    out->culture = (u8)arc_culture(seed, arc);
    Rng r = rng_make(out->key);
    char faction[STORY_NAME];
    faction_name(seed, out->culture, 3 + arc % 3, faction, sizeof faction);
    if (rank == NK_POWER) {
        make_name(out->key, out->culture, out->name, sizeof out->name);
        power_title(seed, arc, &sw, out->title, sizeof out->title);
    } else {
        make_named(out->key, out->culture, out->name, sizeof out->name);
        char power[STORY_NAME];
        make_name(hash3(seed, arc * 3 + 2, 0, 0, 0xC4A9) | 1, out->culture, power, sizeof power);
        if (rank == NK_CAPTAIN) snprintf(out->title, sizeof out->title, "captain of %.36s", faction);
        else if (rng_chance(&r, 0.5f)) snprintf(out->title, sizeof out->title, "marshal of %.36s", faction);
        else snprintf(out->title, sizeof out->title, "the hand of %.32s", power);
    }
    static const u8 CAPTAIN[] = { SM_BRUTE, SM_HORNED, SM_SKELETON };
    static const u8 GENERAL[] = { SM_BRUTE, SM_HORNED };
    static const u8 POWER[CULT_COUNT] = { SM_BRUTE, SM_WISP, SM_HORNED, SM_BRUTE, SM_SKELETON };
    if (rank == NK_CAPTAIN) out->mon = PICK(&r, CAPTAIN);
    else if (rank == NK_GENERAL) out->mon = PICK(&r, GENERAL);
    else out->mon = arc_dir(seed, arc) == AD_UP ? SM_WISP : POWER[out->culture];
    static const float SCALE[3] = { 1.5f, 1.7f, 2.0f }, HP[3] = { 6, 10, 18 }, DMG[3] = { 1.4f, 1.6f, 1.9f };
    out->scale = SCALE[rank];
    out->hp_mult = HP[rank];
    out->dmg_mult = DMG[rank];
}

void story_chapter_target(const World *w, int chapter, Named *out) {
    chapter_identity(w->seed, chapter, out);
    int arc = chapter / 3, rank = chapter % 3;
    static const float FRAC[3] = { 0.42f, 0.7f, 1.0f };
    float d = (12.0f + 16.0f * arc) * FRAC[rank];
    Rng r = rng_make(out->key ^ 0x51E);
    int dir = arc_dir(w->seed, arc);
    float jit = (rng_float(&r) - 0.5f) * 0.5f * d;
    int cx = 0, cy = 0, cz = 0;
    if (chapter == 0) {
        /* the first captain is close: 3-4 chunks out, on the gate's layer or one below, so a
         * new character meets the story within its first half hour */
        d = 3.0f + rng_float(&r);
        if (dir == AD_UP || dir == AD_DOWN) dir = AD_NORTH + (int)(rng_float(&r) * 4.0f) % 4;
        jit = (rng_float(&r) - 0.5f) * 0.5f * d;
        switch (dir) {
        case AD_NORTH: cz = -(int)d; cx = (int)jit; break;
        case AD_SOUTH: cz = (int)d; cx = (int)jit; break;
        case AD_EAST: cx = (int)d; cz = (int)jit; break;
        default: cx = -(int)d - 1; cz = (int)jit; break;
        }
        cy = rng_chance(&r, 0.5f) ? 0 : -1;
        pick_site(w, cx, cy, cz, 1, false, NULL, &out->cx, &out->cy, &out->cz);
        return;
    }
    switch (dir) {
    case AD_DOWN:
    case AD_UP: {
        float a = rng_float(&r) * 6.2832f, h = d * 0.3f;
        cx = (int)(cosf(a) * h) + 1;
        cz = (int)(sinf(a) * h);
        cy = (int)((dir == AD_DOWN ? -1.0f : 1.0f) * d / 1.5f);
        break;
    }
    case AD_NORTH: cz = -(int)d; cx = (int)jit; cy = rng_range(&r, -1, 1); break;
    case AD_SOUTH: cz = (int)d; cx = (int)jit; cy = rng_range(&r, -1, 1); break;
    case AD_EAST: cx = (int)d; cz = (int)jit; cy = rng_range(&r, -1, 1); break;
    default: cx = -(int)d; cz = (int)jit; cy = rng_range(&r, -2, -1); break; /* west lies below the valley */
    }
    pick_site(w, cx, cy, cz, rank == NK_CAPTAIN ? 1 : 2, rank != NK_CAPTAIN, NULL, &out->cx, &out->cy, &out->cz);
}

void story_arc_title(const World *w, int arc, char *out, int n) {
    Named p;
    chapter_identity(w->seed, arc * 3 + 2, &p);
    static const char *const ROMAN[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X" };
    if (arc < LEN(ROMAN)) snprintf(out, (size_t)n, "%s. %s, %s", ROMAN[arc], p.name, p.title);
    else snprintf(out, (size_t)n, "%d. %s, %s", arc + 1, p.name, p.title);
}

static void vault_name(u64 seed, int arc, char *out, int n) {
    char king[32];
    make_name(hash3(seed, arc, 0, 0, 0x7A17), CULT_OLD, king, sizeof king);
    Rng r = rng_make(hash3(seed, arc, 0, 0, 0x7A18));
    static const char *const T[] = { "the Vault of King %s", "the Hoard of %s", "%s's Treasury", "the Drowned Gold of %s" };
    snprintf(out, (size_t)n, PICK(&r, T), king);
}

static void codex_name(u64 seed, int arc, char *out, int n) {
    char who[32];
    make_name(hash3(seed, arc, 0, 0, 0xC0DE), CULT_OLD, who, sizeof who);
    Rng r = rng_make(hash3(seed, arc, 0, 0, 0xC0DF));
    static const char *const T[] = { "the Codex of %s", "the Book of %s", "%s's Ledger", "the Black Psalter of %s" };
    snprintf(out, (size_t)n, PICK(&r, T), who);
}

/* Context for chapter texts: this chapter's target, the next one, the Powers. */
static void chapter_ctx(const World *w, int chapter, const Personal *p, Ctx *c, Named *t, Named *nx, Named *pw, Named *npw,
                        StoryWorld *sw, char *vault, char *codex) {
    story_world(w->seed, sw);
    int arc = chapter / 3;
    chapter_identity(w->seed, chapter, t);
    chapter_identity(w->seed, chapter + 1, nx);
    chapter_identity(w->seed, arc * 3 + 2, pw);
    chapter_identity(w->seed, arc * 3 + 5, npw);
    vault_name(w->seed, arc, vault, STORY_NAME);
    codex_name(w->seed, arc, codex, STORY_NAME);
    cset(c, "target", t->name);
    cset(c, "title", t->title);
    cset(c, "next", nx->name);
    cset(c, "power", pw->name);
    cset(c, "nextpower", npw->name);
    cset(c, "fort", sw->fortress);
    cset(c, "builders", sw->builders);
    cset(c, "sealed", sw->sealed);
    cset(c, "village", p ? p->village : "your village");
    cset(c, "kin", p ? p->kin : "kin");
    cset(c, "vault", vault);
    cset(c, "codex", codex);
}

static const char *const HOOK[MOTIVE_COUNT][4] = {
    /* [0] the very first chapter, then ranks 0-2 of later arcs */
    { "They burned {village}. Your {kin} was in the chapel when they barred the door. The one who held the bar: "
      "{target}. Gone into {fort}.",
      "{target} rode with the ones who burned {village}. {target} is next.",
      "{target} paid for the fire that took your {kin}.",
      "Everything burns for {target}. End it." },
    { "The Roll of the Dead hangs by the gate of {fort}. The first name on it: {target}. Kill it, and they carve "
      "yours below.",
      "A new name on the Roll of the Dead: {target}.",
      "The singers want a bigger name: {target}.",
      "Only one name stands above yours: {target}." },
    { "{builders} took their gold into the dark. {target} keeps the first piece of the map to {vault}.",
      "{target} keeps the first piece of the map to {vault}.",
      "The rest of the map is with {target}.",
      "{vault} lies under {target}'s feet." },
    { "What lies beneath {fort}? {target} keeps the first page of {codex}. It will not give it freely.",
      "{target} keeps the first page of {codex}.",
      "{target} has the next page.",
      "{target} wrote {codex}. Ask it." },
};

static const char *const DONE[MOTIVE_COUNT][3] = {
    { "{target} died with {next}'s coin in its mouth.", "'Your {kin}? Ask {next},' said {target}, and died.",
      "{target} is dead. {village} is still ash." },
    { "They carved your name on the Roll. Above it now: {next}.", "The songs grow. So does the next name: {next}.",
      "Your name is first on the Roll. Until tomorrow." },
    { "Half a map. {next} holds the rest.", "The map is whole. It ends at {next}.", "{vault} is open. Gold calls to gold." },
    { "The page ends mid-sentence. {next} has the next one.", "Two pages, one truth. {next} wrote the book.",
      "{codex} is complete. It names another book." },
};

/* The metaplot, revealed one kill at a time; darker the farther you get. */
static const char *const REVEAL[4][3] = {
    { "Sealed orders on the body. The wax bears the sign of {power}.",
      "A ledger: 'The gate of {fort} was not broken. It was opened from inside.'",
      "{power}, dying: 'I only opened the door. Ask {nextpower} who walked through.'" },
    { "A brand on the corpse: the mark of {builders}. They are not gone.",
      "'{builders} did not seal {sealed}. They fed it, every winter, for a thousand years.'",
      "{power}, dying: 'We kept it fed. Now it is hungry. {nextpower} wants it hungry.'" },
    { "The body is old. Older than {fort}.",
      "'{sealed} was here first. {fort} is the cage, and we are the bait.'",
      "{power}, dying: 'You think you hunt? {nextpower} has hunted you since the gate.'" },
    { "Its blood runs upward.",
      "'The deeper you go, the older the dead. At the bottom the dead are not dead.'",
      "{power}, dying: 'There is no bottom. {nextpower} waits where you think it ends.'" },
};

void story_chapter_hook(const World *w, int m, int chapter, const Personal *p, char *out, int n) {
    Ctx c = { 0 };
    Named t, nx, pw, npw;
    StoryWorld sw;
    char vault[STORY_NAME], codex[STORY_NAME];
    chapter_ctx(w, chapter, p, &c, &t, &nx, &pw, &npw, &sw, vault, codex);
    Rng r = rng_make(t.key);
    int i = chapter == 0 ? 0 : 1 + chapter % 3;
    expand(out, n, HOOK[m % MOTIVE_COUNT][i], &r, &c);
}

void story_chapter_done(const World *w, int m, int chapter, const Personal *p, char *out, int n) {
    Ctx c = { 0 };
    Named t, nx, pw, npw;
    StoryWorld sw;
    char vault[STORY_NAME], codex[STORY_NAME];
    chapter_ctx(w, chapter, p, &c, &t, &nx, &pw, &npw, &sw, vault, codex);
    Rng r = rng_make(t.key ^ 1);
    expand(out, n, DONE[m % MOTIVE_COUNT][chapter % 3], &r, &c);
}

void story_revelation(const World *w, int chapter, char *out, int n) {
    Ctx c = { 0 };
    Named t, nx, pw, npw;
    StoryWorld sw;
    char vault[STORY_NAME], codex[STORY_NAME];
    chapter_ctx(w, chapter, NULL, &c, &t, &nx, &pw, &npw, &sw, vault, codex);
    Rng r = rng_make(t.key ^ 2);
    expand(out, n, REVEAL[(chapter / 3) % 4][chapter % 3], &r, &c);
}

/* ---------- traces ---------- */

static const char *const STELE[] = {
    "Here {faction} held {domain} for #num# days. Then they did not.",
    "{builders} raised {fort} over {sealed}. They called it a gate. It was a lid.",
    "In {era} this was {faction} ground. Ask them what it cost.",
    "Do not dig below. - {name}, last of the #trade#s",
    "We sealed the #door# with #sealmat#. It was not enough.",
    "Count the steps down. If the number changes, turn back.",
    "Remembered by {faction}. Not forgiven.",
    "The #light# went out in {era}. We kept digging.",
    "{sealed} does not sleep. It waits for the door.",
    "Blessed are the buried. They are done.",
    "Here lies {name}, who saw {sealed} and laughed. Once.",
    "{fall}: the night the gate stood open. Who opened it?",
    "{name} the #trade# built this. {name} is under it.",
    "Pray quietly. Something below counts the prayers.",
    "The dead of {era} are buried facing down. So they cannot find the way up.",
    "Tithe to {builders}: one #trade# each winter. Do not ask where they go.",
    "{builders} counted their dead in {era}. They ran out of numbers.",
    "Whoever reads this: the lamps here are lit by no one.",
    "{faction} buried {name} here. Standing up. In case.",
    "Do not answer if it calls you by name.",
    "Three knocks below: close the #door#. Four: it is too late.",
    "{domain} was dug for silver. They found {sealed} instead.",
    "In {era} the bells rang for nine days. No one knows who rang them.",
    "Here {faction} hanged a #trade# for listening at the #door#.",
};
static const char *const STELE_STATE[DS_COUNT] = {
    "{faction} holds {domain}. Pass quietly.",
    "{domain}: empty since {era}. Leave it so.",
    "The water is bad. The bread is bad. The dead are not.",
    "{faction} to the left gate. {rival} to the right. We are between.",
    "Sealed by order of {builders}. Do not open. Do not listen.",
};
static const char *const NOTE[] = {
    "{name}'s note: 'Three days without light. The walls are warm here. Why are the walls warm?'",
    "A torn page: '...{lord} keeps the keys. Do not let it see you count...'",
    "Scrawled in blood: 'NOT DOWN. NOT DOWN. NOT DOWN.'",
    "{name}'s diary: 'Came for gold. Found {faction}. Should have stayed a #trade#.'",
    "A letter never sent: 'Tell my #kin# I found {fort}. Tell no one else.'",
    "{name}'s last page: 'I hear #fear#. The others say there is nothing. The others are gone.'",
    "A list of names, all crossed out. The last one is {name}. Not crossed out yet.",
    "{name}, scratched into a shield: 'Held {domain} for {faction}. Paid in full.'",
};

bool story_trace(const World *w, int cx, int cy, int cz, Trace *out) {
    if (world_is_exterior(cx, cy, cz) || world_is_pocket(cx, cy, cz)) return false;
    u64 h = hash3(w->seed, cx, cy, cz, 0x7ACE);
    if (hash_unit(h) >= 0.32f) return false;
    memset(out, 0, sizeof *out);
    out->key = h | 1;
    float u = hash_unit(mix64(h));
    out->kind = u < 0.5f ? TR_STELE : u < 0.85f ? TR_CORPSE : TR_BOUNTY;
    if (out->kind == TR_CORPSE) {
        Retrieve rt;
        out->mission = hash_unit(mix64(h ^ 0x2)) < 0.45f && story_retrieve(w, cx, cy, cz, &rt);
    } else if (out->kind == TR_BOUNTY) {
        out->mission = true;
    }
    return true;
}

static void trace_ctx(const World *w, int cx, int cy, int cz, Ctx *c, StoryWorld *sw, Domain *d, char *name, char *lord, Rng *r) {
    story_world(w->seed, sw);
    story_domain(w, cx, cy, cz, d);
    make_name(rng_next(r), d->culture, name, 32);
    Named l;
    story_lord(w, d, &l);
    snprintf(lord, STORY_NAME, "%s", l.name);
    cset(c, "fort", sw->fortress);
    cset(c, "builders", sw->builders);
    cset(c, "sealed", sw->sealed);
    cset(c, "fall", sw->fall);
    cset(c, "domain", d->name);
    cset(c, "faction", d->faction);
    cset(c, "rival", d->rival);
    cset(c, "era", d->era);
    cset(c, "name", name);
    cset(c, "lord", lord);
}

bool story_retrieve(const World *w, int cx, int cy, int cz, Retrieve *out) {
    memset(out, 0, sizeof *out);
    u64 h = hash3(w->seed, cx, cy, cz, 0x7ACE) | 1;
    Rng r = rng_make(h ^ 0x1E3);
    int dir = rng_range(&r, 0, 3), dist = rng_range(&r, 1, 3);
    int tx = cx + DIR_X[dir] * dist, tz = cz + DIR_Z[dir] * dist;
    pick_site(w, tx, cy, tz, 1, false, NULL, &out->cx, &out->cy, &out->cz);
    ChunkInfo ci;
    world_chunk_info(w, out->cx, out->cy, out->cz, &ci);
    if (ci.arch == ARCH_EXTERIOR || ci.arch == ARCH_POCKET || (out->cx == cx && out->cy == cy && out->cz == cz)) return false;
    out->key = mix64(h ^ 0x17E3) | 1;
    Domain d;
    story_domain(w, cx, cy, cz, &d);
    make_name(rng_next(&r), d.culture, out->owner, sizeof out->owner);
    snprintf(out->item, sizeof out->item, "%.24s's %s", out->owner, PICK(&r, S_ITEM));
    return true;
}

bool story_bounty(const World *w, int cx, int cy, int cz, Named *lord, int *gold) {
    u64 h = hash3(w->seed, cx, cy, cz, 0x7ACE) | 1;
    Rng r = rng_make(h ^ 0xB0B);
    /* a neighbouring domain's lord, or this one's */
    static const int OFF[7][3] = { { 0, 0, 0 }, { DOM_W, 0, 0 }, { -DOM_W, 0, 0 }, { 0, 0, DOM_W }, { 0, 0, -DOM_W }, { 0, -DOM_H, 0 }, { 0, DOM_H, 0 } };
    const int *o = OFF[rng_range(&r, 0, 6)];
    Domain d;
    story_domain(w, cx + o[0], cy + o[1], cz + o[2], &d);
    story_lord(w, &d, lord);
    *gold = 40 + d.level * 12;
    return true;
}

void story_trace_text(const World *w, int cx, int cy, int cz, char *out, int n) {
    Trace t;
    out[0] = 0;
    if (!story_trace(w, cx, cy, cz, &t)) return;
    Ctx c = { 0 };
    StoryWorld sw;
    Domain d;
    char name[32], lord[STORY_NAME];
    Rng r = rng_make(t.key);
    trace_ctx(w, cx, cy, cz, &c, &sw, &d, name, lord, &r);
    if (t.kind == TR_STELE) {
        const char *tmpl = rng_chance(&r, 0.3f) ? STELE_STATE[d.state] : PICK(&r, STELE);
        expand(out, n, tmpl, &r, &c);
    } else if (t.kind == TR_CORPSE && t.mission) {
        Retrieve rt;
        story_retrieve(w, cx, cy, cz, &rt);
        char where[64];
        story_direction(cx * CHUNK + 16.0f, cy * CHUNK + 16.0f, cz * CHUNK + 16.0f, rt.cx, rt.cy, rt.cz, where, sizeof where);
        cset(&c, "owner", rt.owner);
        cset(&c, "item", rt.item);
        cset(&c, "where", where);
        static const char *const T[] = {
            "Beside {owner}, a last note: '{item}. Lost it {where}. Take it home. Someone should go home.'",
            "{owner}, in charcoal: 'They took {item}. Their nest is {where}. Bring it back to the light.'",
            "A dead hand still points. The note: '{item}, {where}. Tell my #kin#.'",
        };
        expand(out, n, PICK(&r, T), &r, &c);
    } else if (t.kind == TR_CORPSE) {
        expand(out, n, PICK(&r, NOTE), &r, &c);
    } else {
        Named l;
        int gold;
        story_bounty(w, cx, cy, cz, &l, &gold);
        char g[16], where[64];
        snprintf(g, sizeof g, "%d", gold);
        story_direction(cx * CHUNK + 16.0f, cy * CHUNK + 16.0f, cz * CHUNK + 16.0f, l.cx, l.cy, l.cz, where, sizeof where);
        cset(&c, "lname", l.name);
        cset(&c, "ltitle", l.title);
        cset(&c, "gold", g);
        cset(&c, "where", where);
        expand(out, n, "BOUNTY. {lname}, {ltitle}. {where}. {gold} crowns, paid in blood or gold.", &r, &c);
    }
}

/* ---------- directions ---------- */

void story_direction(float fx, float fy, float fz, int cx, int cy, int cz, char *out, int n) {
    float dx = cx * CHUNK + 16.0f - fx, dz = cz * CHUNK + 16.0f - fz;
    float h = sqrtf(dx * dx + dz * dz);
    /* height in layers: the world is built like floors of a building */
    int layers = cy - floordiv((int)floorf(fy) - 3, CHUNK);
    static const char *const DIRS[8] = { "east", "south-east", "south", "south-west", "west", "north-west", "north", "north-east" };
    char hs[48] = "", vs[32] = "";
    if (h >= 20.0f) {
        int oct = (int)floorf(atan2f(dz, dx) / (3.14159265f / 4.0f) + 0.5f);
        oct = ((oct % 8) + 8) % 8;
        snprintf(hs, sizeof hs, "%d m %s", (int)(h / 10.0f + 0.5f) * 10, DIRS[oct]);
    }
    if (layers) snprintf(vs, sizeof vs, "%d layer%s %s", abs(layers), abs(layers) == 1 ? "" : "s", layers < 0 ? "down" : "up");
    if (hs[0] && vs[0]) snprintf(out, (size_t)n, "%s, %s", hs, vs);
    else if (hs[0]) snprintf(out, (size_t)n, "%s", hs);
    else if (vs[0]) snprintf(out, (size_t)n, "%s", vs);
    else snprintf(out, (size_t)n, "very close");
}
