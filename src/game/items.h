#ifndef FC_ITEMS_H
#define FC_ITEMS_H

#include "core/fmath.h"

/* ---------- spells ----------
 * Attack spells are every element combined with every form (8 x 11 = 88), plus a few
 * utility spells. Scrolls teach a spell; finding it again raises its rank. */

enum { EL_FIRE, EL_FROST, EL_LIGHTNING, EL_POISON, EL_ARCANE, EL_HOLY, EL_SHADOW, EL_EARTH, EL_COUNT, EL_NONE = 255 };
enum {
    SF_BOLT, SF_BARRAGE, SF_NOVA, SF_CHAIN, SF_BREATH, SF_ORB, SF_RUNE, SF_ORBIT, SF_STORM, SF_METEOR, SF_AURA,
    SF_COUNT
};
enum {
    SU_HEAL, SU_BLINK, SU_WARD, SU_HASTE, SU_FLARE, SU_RECALL, SU_THREAD,
    /* moving around */
    SU_LEVITATE, SU_WINGS, SU_FEATHER, SU_CLIMB, SU_STRIDE, SU_PHASE, SU_ANCHOR, SU_GATES, SU_STAIRWAY,
    /* defence and safe places */
    SU_KEEP, SU_WALL, SU_HUT, SU_SANCTUARY, SU_MIRROR, SU_STONESKIN,
    /* seeing and knowing */
    SU_SCRY, SU_REVEAL, SU_MAPMIND, SU_DOWSE, SU_SPEAKDEAD,
    /* bending monsters */
    SU_INVIS, SU_LURE, SU_CHARM, SU_FEAR, SU_TIMESLIP, SU_GRAVWELL,
    /* changing the world */
    SU_STONESHAPE, SU_BRIDGE, SU_QUENCH, SU_STEED,
    /* the way home */
    SU_SANCTUM,
    SU_COUNT
};
/* Spells known in save version 3 and earlier (88 attack + 7 utility). */
#define SPELL_COUNT_V3 95

#define SPELL_ATTACK_COUNT (EL_COUNT * SF_COUNT)
#define SPELL_COUNT (SPELL_ATTACK_COUNT + SU_COUNT)
#define SPELL_MAX_RANK 5

static inline int spell_attack_id(int element, int form) { return element * SF_COUNT + form; }
static inline bool spell_is_attack(int id) { return id < SPELL_ATTACK_COUNT; }
static inline int spell_element(int id) { return spell_is_attack(id) ? id / SF_COUNT : EL_NONE; }
static inline int spell_form(int id) { return spell_is_attack(id) ? id % SF_COUNT : -1; }

typedef struct SpellInfo {
    char name[32];
    const char *desc;
    float mana, cooldown, damage; /* damage before skills and rank */
    int tier;                     /* 0-3: how deep you need to be to find its scroll */
} SpellInfo;

void spell_info(int id, SpellInfo *out);
const char *spell_effect_text(int id);
extern const char *ELEMENT_NAMES[EL_COUNT];
vec3 element_color(int element);

/* ---------- items ---------- */

/* Legendary weapons are only found far out (region tier 6 and beyond). */
enum { RAR_COMMON, RAR_MAGIC, RAR_RARE, RAR_EPIC, RAR_LEGENDARY, RAR_COUNT };

/* Weapon bases: each belongs to one of the three weapon classes (WPN_SWORD = blades,
 * WPN_BOW = bows, WPN_STAFF = staves) and plays differently. */
enum {
    WB_DAGGER, WB_SWORD, WB_AXE, WB_MACE, WB_SPEAR, WB_GREATSWORD,
    WB_SHORTBOW, WB_LONGBOW, WB_CROSSBOW,
    WB_WAND, WB_STAFF, WB_SCEPTER,
    /* the lost arts: each world knows only four of them (arms.c) */
    WB_WHIP, WB_FLAIL, WB_SCYTHE, WB_RAPIER,
    WB_SLING, WB_CHAKRAM, WB_CANNON,
    WB_TOME, WB_CENSER,
    WB_COUNT
};
#define WB_CLASSIC 12 /* the first 12 are found in every world */

/* What a weapon base does beyond its numbers. */
enum {
    WBF_PULL = 1,        /* hits pull the foe toward you (whip) */
    WBF_SHIELDBREAK = 2, /* a knight's shield does not stop it (flail) */
    WBF_CRIT = 4,        /* a third of its thrusts are critical (rapier) */
    WBF_STUN = 8,        /* hits stun for a moment (sling) */
    WBF_RETURN = 16,     /* flies out and comes back (chakram) */
    WBF_EXPLODE = 32,    /* bursts where it lands (hand cannon) */
    WBF_SPELLBOOST = 64, /* spells hit harder and cost less while it is your staff (tome) */
    WBF_PULSE = 128,     /* a burst round you instead of a bolt (censer) */
};

/* Affixes: elemental ones add that element's effect to every hit. */
enum { AF_NONE, AF_FIRE, AF_FROST, AF_SHOCK, AF_VENOM, AF_VAMPIRIC, AF_SWIFT, AF_KEEN, AF_HEAVY, AF_COUNT };

typedef struct WeaponBase {
    const char *name;
    int wclass;
    float dmg, cooldown, range, arc_deg, knock, speed; /* speed: projectiles */
    int pierce;
    float mana; /* staves: mana per bolt */
    unsigned flags; /* WBF_* */
    float gravity;  /* projectiles: how fast they drop */
} WeaponBase;
extern const WeaponBase WEAPON_BASES[WB_COUNT];

typedef struct Item {
    u8 used;
    u8 base, rarity, level;
    u8 affix[2];
    u8 material;        /* MAT_*: where it was forged (arms.c) */
    u8 unique;          /* 0, or 1 + one of this world's legendaries */
    u8 power[2];        /* trigger << 4 | effect (arms.c), 0 none */
    u8 hidden;          /* AF_* revealed after 100 kills, 0 not yet */
    u8 bane;            /* MON_* + 1 it has killed most of lately, 0 none */
    u16 kills, bane_kills;
} Item;

float item_damage_mult(const Item *it);
float item_speed_mult(const Item *it); /* cooldown factor, < 1 is faster */
void item_name(const Item *it, char *out, int n);
/* The quality word of an item level ("", "Fine" ... "Mythic"). */
const char *item_grade(int level);
/* A weapon found in a region (REG_*) tier steps out: better rarity the farther out, and
 * the first affix leans to the region's element. tier 0 is the same as item_roll. */
Item item_roll_region(u64 seed, float level, int min_rarity, int tier, int region);
vec3 rarity_color(int rarity);
int item_affix_element(const Item *it); /* EL_* of its first elemental affix or EL_NONE */
bool item_has_affix(const Item *it, int affix);

/* ---------- the world's armory (arms.c) ----------
 * Materials by region, powers (a trigger and an effect), the lost arts a world knows,
 * its legendaries, and what a weapon remembers of its kills. */
enum { MAT_IRON, MAT_BRONZE, MAT_BONE, MAT_OBSIDIAN, MAT_JADE, MAT_CRYSTAL, MAT_SILVER, MAT_STARMETAL, MAT_EMBERSTEEL, MAT_RIMEIRON, MAT_COUNT };
enum { PT_NONE, PT_KILL, PT_THIRD, PT_LOW, PT_AIR, PT_CRIT, PT_FIRST, PT_ELITE, PT_COUNT };
enum { PE_NONE, PE_CHAIN, PE_BLAST, PE_FREEZE, PE_WISP, PE_BLINK, PE_HEAL, PE_SPLIT, PE_DECOY, PE_COUNT };
#define UNIQUE_COUNT 20
#define BANE_KILLS 50    /* kills of one kind that earn a weapon its title */
#define AWAKEN_KILLS 100 /* kills that reveal its hidden affix */

typedef struct MaterialDef {
    const char *name;
    float dmg, speed, crit, life; /* damage and cooldown factors, crit chance, share of damage drawn as life */
    int element;                  /* a faint element on hits, EL_NONE */
    vec3 tint;
    const char *desc;
} MaterialDef;
extern const MaterialDef MATERIALS[MAT_COUNT];

void arms_init(u64 world_seed);             /* at world start: the lost arts and the legendaries */
bool arms_base_known(int base);             /* a classic weapon, or a lost art of this world */
int arms_lost_art(int k);                   /* this world's lost arts, k 0..3 */
int arms_pick_base(u64 seed);               /* a random weapon base this world knows */
int arms_material(int region, int tier, u64 seed); /* what weapons are forged of there */
void arms_roll_powers(Item *it, u64 seed);
Item arms_unique(int id, float level);      /* legendary id (0..UNIQUE_COUNT-1) of this world */
const char *arms_unique_name(int id);
const char *arms_unique_lore(int id);
void arms_power_text(int power, char *out, int n);
const char *arms_bane_title(int mon_type);  /* "Crawlerbane" */

/* ---------- relics ---------- */

enum { RB_HEALTH, RB_DAMAGE, RB_MANA, RB_SPEED };
#define RELIC_COUNT 12
typedef struct RelicDef { const char *name; int bonus; } RelicDef;
extern const RelicDef RELICS[RELIC_COUNT];
extern const char *RELIC_BONUS_TEXT[4];

/* ---------- things on the ground ---------- */

/* PK_TRACE: something to read (amount = TR_*), PK_QUEST: an item a mission asks for. */
enum { PK_GOLD, PK_POTION_HP, PK_POTION_MP, PK_SCROLL, PK_WEAPON, PK_RELIC, PK_CHEST, PK_TRACE, PK_QUEST, PK_COUNT };

typedef struct Pickup {
    bool alive;
    u8 kind;
    vec3 pos, vel;
    int amount;    /* gold, spell id, relic id, chest level */
    Item item;
    float t;       /* age */
    u64 key;       /* chests and relics: remembered once taken; 0 for drops */
    u64 chunk_key; /* placed with this chunk (despawns with it) */
} Pickup;

/* ---------- lasting spell effects ---------- */

enum {
    EF_STORM, EF_RUNE, EF_METEOR, EF_ORBIT, EF_AURA, EF_FLARE,
    /* utility spells (utility.c) */
    EF_DECOY,     /* a mirror image: monsters strike it, it bursts */
    EF_LURE,      /* a light monsters go to */
    EF_SANCTUARY, /* a ring monsters will not cross */
    EF_GRAVWELL,  /* pulls monsters and loot in */
    EF_GATE,      /* one of a pair of linked portals (link: the other) */
    EF_DOOR,      /* the door to the pocket realm */
    EF_HEARTH,    /* the heart of a keep or hut: heals players near it */
    EF_ANCHOR,    /* where Anchor brings you back */
};

typedef struct Effect {
    bool alive;
    u8 type, element;
    int owner;
    vec3 pos;
    float t, life, tick, dmg, radius;
    float yaw;
    int link;     /* gates: the other gate's index, -1 none */
} Effect;

#define BAG_SIZE 18
#define POTION_HP_MAX 5 /* few, so each one counts */
#define POTION_MP_MAX 9
#define MAX_PICKUPS 384
#define MAX_EFFECTS 64

#endif
