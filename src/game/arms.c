#include "game/game.h"

#include <stdio.h>
#include <string.h>

/* The world's armory: what the weapons of one world are like.
 *
 * - The lost arts: besides the twelve weapons found everywhere, each world knows four of
 *   nine rarer kinds (whip, flail, scythe, rapier, sling, chakram, hand cannon, tome,
 *   censer), one of each class and one more, chosen by its seed.
 * - Materials: where a weapon was forged shows in its metal. Each direction from the gate
 *   has its own materials, and the world's seed decides which one rules there.
 * - Powers: rare and better weapons carry one or two: a trigger (on a kill, every third
 *   hit, in the air...) and an effect (lightning, a blast, a wisp at your side...).
 * - Legendaries: twenty weapons with names of their own, made for this world; its named
 *   enemies carry them.
 * The gameplay of powers and memory is in combat.c (arms_weapon_hit). */

const MaterialDef MATERIALS[MAT_COUNT] = {
    /*                 name           dmg    speed  crit   life   element    tint */
    [MAT_IRON] = { "Iron", 1.0f, 1.0f, 0.0f, 0.0f, EL_NONE, { 1.0f, 1.0f, 1.0f }, "" },
    [MAT_BRONZE] = { "Bronze", 0.95f, 1.0f, 0.0f, 0.0f, EL_NONE, { 1.15f, 0.8f, 0.5f }, "bronze: hits knock harder" },
    [MAT_BONE] = { "Bone", 0.95f, 0.95f, 0.0f, 0.04f, EL_NONE, { 1.1f, 1.05f, 0.85f }, "bone: hits draw a little life" },
    [MAT_OBSIDIAN] = { "Obsidian", 1.2f, 1.1f, 0.0f, 0.0f, EL_NONE, { 0.45f, 0.4f, 0.55f }, "obsidian: 20% more damage, 10% slower" },
    [MAT_JADE] = { "Jade", 1.0f, 1.0f, 0.0f, 0.0f, EL_POISON, { 0.55f, 1.05f, 0.7f }, "jade: hits sting with poison" },
    [MAT_CRYSTAL] = { "Crystal", 1.0f, 1.0f, 0.12f, 0.0f, EL_NONE, { 0.8f, 0.95f, 1.25f }, "crystal: more critical hits" },
    [MAT_SILVER] = { "Silver", 1.0f, 1.0f, 0.0f, 0.0f, EL_NONE, { 1.2f, 1.2f, 1.25f }, "silver: bane of the undead" },
    [MAT_STARMETAL] = { "Starmetal", 1.0f, 0.85f, 0.0f, 0.0f, EL_NONE, { 1.1f, 1.15f, 1.3f }, "starmetal: 15% faster" },
    [MAT_EMBERSTEEL] = { "Embersteel", 1.0f, 1.0f, 0.0f, 0.0f, EL_FIRE, { 1.25f, 0.6f, 0.4f }, "embersteel: hits smoulder" },
    [MAT_RIMEIRON] = { "Rime-iron", 1.0f, 1.0f, 0.0f, 0.0f, EL_FROST, { 0.75f, 0.95f, 1.2f }, "rime-iron: hits chill" },
};

static struct {
    u64 seed;
    int lost[4];
    int rule[7];      /* per REG_*: the material that rules there in this world */
    Item uniq[UNIQUE_COUNT];
    char uname[UNIQUE_COUNT][32];
    const char *ulore[UNIQUE_COUNT];
    bool ready;
} A;

static const int LOST_MELEE[4] = { WB_WHIP, WB_FLAIL, WB_SCYTHE, WB_RAPIER };
static const int LOST_BOW[3] = { WB_SLING, WB_CHAKRAM, WB_CANNON };
static const int LOST_STAFF[2] = { WB_TOME, WB_CENSER };

/* The materials of each direction (REG_*): the world picks one of them to rule. */
static const int REGION_MATS[7][3] = {
    [REG_NONE] = { MAT_IRON, MAT_BRONZE, MAT_IRON },
    [REG_NORTH] = { MAT_RIMEIRON, MAT_SILVER, MAT_CRYSTAL },
    [REG_SOUTH] = { MAT_EMBERSTEEL, MAT_OBSIDIAN, MAT_BRONZE },
    [REG_EAST] = { MAT_CRYSTAL, MAT_JADE, MAT_STARMETAL },
    [REG_WEST] = { MAT_JADE, MAT_BONE, MAT_BRONZE },
    [REG_UP] = { MAT_STARMETAL, MAT_SILVER, MAT_CRYSTAL },
    [REG_DOWN] = { MAT_OBSIDIAN, MAT_BONE, MAT_EMBERSTEEL },
};

static const char *const U_PRE[] = { "Ash", "Grief", "Hollow", "Dawn", "Thorn", "Moon", "Bitter", "Star", "Wyrm", "Raven", "Ember",
                                     "Frost", "Iron", "Last", "Pale", "Storm", "Widow", "Gloam", "Oath", "Ruin" };
static const char *const U_SUF[] = { "fang", "song", "brand", "reaver", "call", "wake", "fall", "thorn", "mourn", "bite", "kiss",
                                     "light", "word", "veil", "heart", "spite", "keeper", "maw", "knell", "warden" };
static const char *const U_LORE[] = {
    "It hums when the dark is near.",
    "Its last owner never lost a fight, they say, until the last one.",
    "Forged in the year the gate first closed.",
    "The name on the grip has been scratched out, twice.",
    "Warm to the touch, even in the deep cold.",
    "It was buried with a king, and did not stay buried.",
    "Three smiths died making it. The fourth would not speak of it.",
    "Monsters remember it, and hate it.",
    "It was meant as a gift. It became a sentence.",
    "Whoever holds it dreams of the same door.",
    "It weighs more after every kill. Or so it feels.",
    "Blessed once, cursed twice, and still loyal.",
};

static u64 h2(u64 a, u64 b) { return mix64(a * 0x9E3779B97F4A7C15ULL ^ b); }

static void make_powers(Item *it, Rng *r, int n) {
    it->power[0] = it->power[1] = 0;
    for (int k = 0; k < n; k++) {
        int t, e;
        do {
            t = rng_range(r, 1, PT_COUNT - 1);
            e = rng_range(r, 1, PE_COUNT - 1);
        } while (k == 1 && (e == (it->power[0] & 15) || t == (it->power[0] >> 4)));
        /* summons and doubles on every third hit would crowd the screen: those trigger rarer */
        if (t == PT_THIRD && (e == PE_WISP || e == PE_DECOY)) t = PT_KILL;
        it->power[k] = (u8)(t << 4 | e);
    }
}

void arms_init(u64 world_seed) {
    memset(&A, 0, sizeof A);
    A.seed = world_seed;
    Rng r = rng_make(h2(world_seed, 0xA4415ULL));
    A.lost[0] = LOST_MELEE[rng_range(&r, 0, 3)];
    A.lost[1] = LOST_BOW[rng_range(&r, 0, 2)];
    A.lost[2] = LOST_STAFF[rng_range(&r, 0, 1)];
    /* and one more of any kind, not yet chosen */
    static const int ALL[9] = { WB_WHIP, WB_FLAIL, WB_SCYTHE, WB_RAPIER, WB_SLING, WB_CHAKRAM, WB_CANNON, WB_TOME, WB_CENSER };
    do A.lost[3] = ALL[rng_range(&r, 0, 8)];
    while (A.lost[3] == A.lost[0] || A.lost[3] == A.lost[1] || A.lost[3] == A.lost[2]);
    for (int reg = 0; reg < 7; reg++) A.rule[reg] = REGION_MATS[reg][rng_range(&r, 0, 2)];
    A.ready = true;
    if (world_seed != 0x5EEDULL)
        SDL_Log("Armory: lost arts %s, %s, %s, %s", WEAPON_BASES[A.lost[0]].name, WEAPON_BASES[A.lost[1]].name, WEAPON_BASES[A.lost[2]].name,
                WEAPON_BASES[A.lost[3]].name);
    /* the legendaries of this world */
    for (int id = 0; id < UNIQUE_COUNT; id++) {
        Rng u = rng_make(h2(world_seed, 0x0117AULL + (u64)id));
        Item it = { 0 };
        it.used = 1;
        it.base = (u8)arms_pick_base(rng_next(&u));
        it.rarity = RAR_LEGENDARY;
        static const int ELEM[5] = { AF_FIRE, AF_FROST, AF_SHOCK, AF_VENOM, AF_VAMPIRIC };
        static const int OTHER[3] = { AF_SWIFT, AF_KEEN, AF_HEAVY };
        it.affix[0] = (u8)ELEM[rng_range(&u, 0, 4)];
        it.affix[1] = (u8)OTHER[rng_range(&u, 0, 2)];
        it.material = (u8)rng_range(&u, 1, MAT_COUNT - 1);
        make_powers(&it, &u, 2);
        it.unique = (u8)(id + 1);
        A.uniq[id] = it;
        /* a name nobody else's world has: two halves, never the same twice in one world */
        for (int tries = 0; tries < 50; tries++) {
            snprintf(A.uname[id], sizeof A.uname[id], "%s%s", U_PRE[rng_range(&u, 0, ARRAY_LEN(U_PRE) - 1)], U_SUF[rng_range(&u, 0, ARRAY_LEN(U_SUF) - 1)]);
            bool dup = false;
            for (int k = 0; k < id; k++) dup |= !strcmp(A.uname[k], A.uname[id]);
            if (!dup) break;
        }
        A.ulore[id] = U_LORE[rng_range(&u, 0, ARRAY_LEN(U_LORE) - 1)];
    }
}

static void ensure(void) {
    if (!A.ready) arms_init(0x5EEDULL);
}

bool arms_base_known(int base) {
    ensure();
    if (base < WB_CLASSIC) return true;
    for (int k = 0; k < 4; k++)
        if (A.lost[k] == base) return true;
    return false;
}

int arms_lost_art(int k) {
    ensure();
    return A.lost[k & 3];
}

int arms_pick_base(u64 seed) {
    ensure();
    /* the twelve classic weapons once each, the four lost arts one and a half times each:
     * rarer than all the rest together, but met every few finds */
    float w = (float)(seed >> 11) / (float)(1ULL << 53) * 18.0f;
    if (w < 12.0f) return (int)w;
    return A.lost[MINI(3, (int)((w - 12.0f) / 1.5f))];
}

int arms_material(int region, int tier, u64 seed) {
    ensure();
    if (region < 0 || region > REG_DOWN) region = REG_NONE;
    float u = (float)(seed >> 11) / (float)(1ULL << 53);
    if (tier == 0 || region == REG_NONE) return u < 0.75f ? MAT_IRON : MAT_BRONZE;
    /* farther out, more of the local material: half at tier 1, most from tier 4 */
    float local = fminf(0.85f, 0.4f + 0.12f * (float)tier);
    if (u < local * 0.7f) return A.rule[region];
    if (u < local) return REGION_MATS[region][(int)(u * 997.0f) % 3];
    return u < local + (1.0f - local) * 0.6f ? MAT_IRON : MAT_BRONZE;
}

void arms_roll_powers(Item *it, u64 seed) {
    Rng r = rng_make(seed);
    float u = rng_float(&r);
    int n = 0;
    switch (it->rarity) {
    case RAR_MAGIC: n = u < 0.1f; break;
    case RAR_RARE: n = u < 0.45f; break;
    case RAR_EPIC: n = 1 + (u < 0.25f); break;
    case RAR_LEGENDARY: n = 2; break;
    default: break;
    }
    make_powers(it, &r, n);
}

Item arms_unique(int id, float level) {
    ensure();
    Item it = A.uniq[((id % UNIQUE_COUNT) + UNIQUE_COUNT) % UNIQUE_COUNT];
    it.level = (u8)clampf(level, 1, 250);
    return it;
}

const char *arms_unique_name(int id) {
    ensure();
    return A.uname[((id % UNIQUE_COUNT) + UNIQUE_COUNT) % UNIQUE_COUNT];
}

/* In a world with generated stories, the legendaries its named enemies carry tell their
 * deeds (world/history.c). Called once the world's save is read. */
void arms_story_lore(const World *w) {
    static char lore[UNIQUE_COUNT][STORY_TEXT];
    ensure();
    for (int id = 0; id < UNIQUE_COUNT; id++)
        if (story_legend_lore(w, id, lore[id], sizeof lore[id])) A.ulore[id] = lore[id];
}

const char *arms_unique_lore(int id) {
    ensure();
    return A.ulore[((id % UNIQUE_COUNT) + UNIQUE_COUNT) % UNIQUE_COUNT];
}

void arms_power_text(int power, char *out, int n) {
    static const char *TRIG[PT_COUNT] = { "", "ON A KILL", "EVERY THIRD HIT", "BELOW A THIRD OF YOUR HEALTH", "STRUCK FROM THE AIR",
                                          "ON A CRITICAL HIT", "FIRST HIT ON AN UNHURT FOE", "HITTING ELITES AND BOSSES" };
    static const char *EFF[PE_COUNT] = { "", "LIGHTNING LEAPS TO TWO FOES", "A BLAST", "THE GROUND FREEZES ROUND IT",
                                         "A WISP FIGHTS AT YOUR SIDE", "YOU STEP BEHIND IT", "YOU MEND A LITTLE",
                                         "SHARDS FLY OUT OF IT", "A DOUBLE OF YOU APPEARS" };
    int t = (power >> 4) & 15, e = power & 15;
    if (t <= 0 || t >= PT_COUNT || e <= 0 || e >= PE_COUNT) { if (n) out[0] = 0; return; }
    snprintf(out, (size_t)n, "%s: %s", TRIG[t], EFF[e]);
}

const char *arms_bane_title(int mon_type) {
    static char names[32][24];
    if (mon_type < 0 || mon_type >= MON_TYPE_COUNT) return "Slayer";
    if (!names[mon_type][0]) snprintf(names[mon_type], sizeof names[mon_type], "%sbane", MONSTERS[mon_type].name);
    return names[mon_type];
}
