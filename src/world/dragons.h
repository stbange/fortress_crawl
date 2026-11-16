#ifndef FC_DRAGONS_H
#define FC_DRAGONS_H

/* Dragons (docs/DRAGONS.md): the hardest enemies of the fortress, in five sizes and many
 * kinds, each with its breath (through the simulation: fire burns, frost freezes, acid eats
 * stone...), its home and its way of fighting. The great ones sleep in lairs on hoards. This
 * file is the data both sides read: the lair generator (gen_lair.c) and the game (dragon.c). */

#include "core/common.h"

enum { DS_DRAGONET, DS_DRAKE, DS_WYVERN, DS_DRAGON, DS_ANCIENT, DSIZE_COUNT };

enum {
    DK_RED, DK_MAGMA, DK_WHITE, DK_BLUE, DK_GREEN, DK_BLACK, DK_SWAMP, DK_GOLD, DK_CRYSTAL, DK_AMETHYST,
    DK_SHADOW, DK_BONE, DK_IRON, DK_RUST, DK_SAND, DK_SEA, DK_CLOUD, DK_VOID, DK_PALE_MOON, DK_RED_MOON,
    DK_BLACK_MOON, DK_HYDRA, DK_TWIN, DK_WYRM, DK_CHAOS, DK_COUNT
};

/* What a breath is and does. */
enum {
    BR_FIRE, BR_MAGMA, BR_FROST, BR_STORM, BR_GAS, BR_ACID, BR_ROT, BR_HOLY, BR_LIGHT, BR_FORCE, BR_SHADOW,
    BR_NECRO, BR_STEAM, BR_RUST, BR_SAND, BR_WAVE, BR_WIND, BR_VOID, BR_CHAOS, BR_COUNT
};

/* Traits. */
enum {
    DT_HEADS = 1 << 0,    /* a hydra: three to five heads, each breathes */
    DT_TWIN = 1 << 1,     /* two heads, two breaths */
    DT_WINGLESS = 1 << 2, /* never flies; burrows and bursts up */
    DT_REVIVE = 1 << 3,   /* rises once more unless burned */
    DT_STEALTH = 1 << 4,  /* vanishes and strikes from behind */
    DT_ARMOR = 1 << 5,    /* plates: half damage until they break (the second phase) */
    DT_LIGHTHEAL = 1 << 6,/* heals itself when left alone a moment */
    DT_SWIM = 1 << 7,     /* fights from the water */
    DT_DARK = 1 << 8,     /* puts out the light round it */
};

typedef struct DragonDef {
    const char *name;        /* "red" */
    const char *title;       /* "the Red" */
    u8 body[3], belly[3], wing[3], glow[3];
    u8 breath, breath2;      /* BR_*; the second head's for twins (else = breath) */
    u16 traits;              /* DT_* */
    u32 strata;              /* bits of ST_* where it lives */
    const char *lore;        /* one line */
} DragonDef;
extern const DragonDef DRAGON_KINDS[DK_COUNT];

typedef struct DragonSize {
    const char *name;        /* "drake" */
    float scale;             /* the model's scale (1: a drake, a horse's size) */
    float hp, dmg;           /* times a drake's */
    float speed;
    bool flies;
    u8 group_lo, group_hi;
    float breath_range, breath_cone, breath_every;
} DragonSize;
extern const DragonSize DRAGON_SIZES[DSIZE_COUNT];

extern const char *BREATH_NAMES[BR_COUNT];

/* The kind of dragon that lives in a stratum (ST_*), by the seed; -1 none. */
int dragon_kind_for(int stratum, u64 seed);
/* The kind of a realm behind the Gate (RM_*), -1 none. */
int dragon_kind_of_realm(int realm);
/* A great dragon's name: "Vyrmathax the Red". */
void dragon_name(int kind, u64 key, char *out, int n);

/* A lair: a hall structure of variant HALL_LAIR, its kind in Structure.people. */
#define HALL_LAIR 4
typedef struct Lair {
    u64 key;
    int kind, size;          /* DK_*, DS_DRAGON or DS_ANCIENT */
    char name[48];           /* the dragon's name */
    int x0, z0, x1, z1;      /* the cave's floor (inclusive) */
    int floor;               /* feet height */
    int cx, cz;              /* the hoard, in the middle */
    int perch_x[4], perch_z[4], perch_h[4];
} Lair;
struct Structure;
bool lair_from_structure(const struct Structure *s, Lair *out);

#endif
