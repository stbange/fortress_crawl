#ifndef FC_REALMS_H
#define FC_REALMS_H

/* Other worlds behind the Gate (docs/REALMS.md): twenty-one places beyond the fortress, the
 * three moons and the keeps of their wizard-kings, the fortress at the heart of the sun, the
 * keep under the ocean, the icelands and more. Each fortress (world seed) is joined to six
 * to eight of them, chosen at random; the Gate in the Sanctum court opens the ones you are
 * attuned to, and rifts in the fortress attune you (and take you through).
 *
 * Every realm is a land of REALM_SPAN x REALM_SPAN chunks (256 m on a side) with a margin of
 * empty or walled chunks around it, one band of layers each, far above the pocket realm:
 * realm r has its ground in chunk layer REALM_CY(r) + 1. It has its own look (a flavor
 * FL_R_*), its own danger (monster level), its own rules (gravity, air, heat, tides, storms,
 * growth), a keep with its ruler, and three plants and three animals of its own, each in
 * three variants. This file is the data both sides read: the generator (gen_realm.c), the
 * textures (texgen.c) and the game (realms.c in game/). */

#include "core/common.h"

enum {
    RM_PALE_MOON, RM_RED_MOON, RM_BLACK_MOON, RM_SUN, RM_FORGE, RM_GLASS_DESERT, RM_OCEAN_KEEP,
    RM_ICELANDS, RM_SEA_OF_NIGHT, RM_STORM_PEAKS, RM_SHINING_COURT, RM_ASTRAL_SHORE, RM_HOLLOW_EARTH,
    RM_FUNGAL_DEEP, RM_BONE_FIELDS, RM_ORRERY, RM_LIBRARY, RM_GARDEN, RM_FALLEN_STAR, RM_MIRROR, RM_DREAM,
    REALM_COUNT
};

#define REALM_CY0 400      /* the first realm's band of layers */
#define REALM_DY 16        /* layers between realms */
#define REALM_SPAN 8       /* chunks per side of the land */
#define REALM_MARGIN 10    /* chunks of nothing (or walls) around it: nothing else is in view */
static inline int REALM_CY(int r) { return REALM_CY0 + r * REALM_DY; }
#define REALM_GROUND_OFF 48 /* the mean ground: this far above the band's first cell */
static inline int realm_ground_y(int r) { return REALM_CY(r) * 32 + REALM_GROUND_OFF; }
#define REALM_W (REALM_SPAN * 32)
/* The gate home (an arch, you arrive in front of it) and the keep, in every realm. */
#define REALM_ARRIVE_X 44
#define REALM_ARRIVE_Z 44
#define REALM_KEEP_X 168
#define REALM_KEEP_Z 168
#define REALM_KEEP_R 12 /* half the keep's side */

/* Which realm a chunk belongs to, or -1. */
static inline int realm_at(int cx, int cy, int cz) {
    if (cx < -REALM_MARGIN || cx >= REALM_SPAN + REALM_MARGIN || cz < -REALM_MARGIN || cz >= REALM_SPAN + REALM_MARGIN) return -1;
    int d = cy - REALM_CY0 + 6;
    if (d < 0) return -1;
    int r = d / REALM_DY;
    if (r >= REALM_COUNT || d - r * REALM_DY > 12) return -1;
    return r;
}
static inline bool world_is_realm(int cx, int cy, int cz) { return realm_at(cx, cy, cz) >= 0; }
static inline int realm_at_w(float x, float y, float z) {
    return realm_at((int)floorf(x / 32.0f), (int)floorf(y / 32.0f), (int)floorf(z / 32.0f));
}

/* The land's shape. */
enum {
    TR_PLAIN,   /* rolling ground */
    TR_CRATER,  /* rolling ground pocked with craters */
    TR_DUNES,   /* long dunes */
    TR_PEAKS,   /* sharp mountains, valleys between */
    TR_ISLES,   /* islands in the sky, bridges between */
    TR_SEA,     /* a sea with islands; the keep on its own isle */
    TR_DEEP,    /* all under water: the seabed, air domes */
    TR_CAVERN,  /* a ground and a roof of rock */
    TR_MIRROR,  /* the fortress, mirrored */
    TR_DREAM,   /* pieces of the fortress, stitched */
};

/* What stands on it. */
enum {
    FE_NONE, FE_DOMES, FE_SPIRES, FE_MUSHROOMS, FE_LIGHTRIVERS, FE_MAGMA, FE_SHARDS, FE_KELP, FE_ICEBERGS,
    FE_CAUSEWAY, FE_CONDUCTORS, FE_CLOUDS, FE_STARROCKS, FE_INNERSUN, FE_STALKS, FE_RIBS, FE_RINGS, FE_SHELVES,
    FE_GROVES, FE_CRYSTALS, FE_NONE2
};

/* Rules of a realm (bits). */
enum {
    RR_AIRLESS = 1 << 0,  /* no air outside the domes: breath runs out */
    RR_SCORCH = 1 << 1,   /* burns without a sun-ward */
    RR_STORM = 1 << 2,    /* lightning out of the sky */
    RR_WIND = 1 << 3,     /* gusts push you */
    RR_TIDE = 1 << 4,     /* the sea rises and falls with the moons */
    RR_DAYNIGHT = 1 << 5, /* scorching days, freezing nights */
    RR_SANDSTORM = 1 << 6,/* the air fills with sand now and then */
    RR_SPORES = 1 << 7,   /* spores in the air; the mycelium tells the monsters */
    RR_GROWTH = 1 << 8,   /* plants grow twenty times as fast */
    RR_DRAIN = 1 << 9,    /* the crystals drink mana */
    RR_SHIFT = 1 << 10,   /* gravity shifts */
    RR_DARK = 1 << 11,    /* no sky light */
    RR_MIASMA = 1 << 12,  /* rot rises from the bones */
    RR_FLOAT = 1 << 13,   /* next to no weight */
    RR_STORY = 1 << 14,   /* opens only after the finale */
    RR_SPARED = 1 << 15,  /* opens only after a finale that spared */
};

/* A plant: the three of a realm are its blocks B_RFLORA_LOW (ground cover), B_RFLORA_TALL
 * (tall) and B_RFLORA_GLOW (it glows). Three variants, by texture. */
enum { FS_TUFT, FS_BLOOM, FS_REED, FS_CACTUS, FS_FERN, FS_CAP, FS_SPIKE, FS_KELP, FS_POD, FS_SAPLING, FS_ORB, FS_LEAVES };
enum {
    FX_FOOD,    /* only food */
    FX_AIR,     /* a breath of air */
    FX_HEAL,    /* closes wounds */
    FX_MANA,    /* gives mana */
    FX_THORNS,  /* hurts to walk through */
    FX_SPORES,  /* a cloud of spores when broken */
    FX_ALERT,   /* stepping on it wakes the monsters near */
    FX_WARM,    /* warms you */
    FX_COOL,    /* a sun-ward for a while */
    FX_SEEDS,   /* seeds */
    FX_SHOCK,   /* holds lightning: shocks whoever breaks it */
    FX_PAGE,    /* a page that answers a question of the history */
    FX_LIGHT,   /* a light that lasts (night sight) */
};
typedef struct FloraDef {
    const char *name;           /* "frost lily" */
    const char *var[3];         /* variant adjectives: "white", "blue", "silver-veined" */
    u8 shape;                   /* FS_* */
    u8 stem[3];                 /* stem colour */
    u8 col[3][3];               /* bloom colour per variant */
    u8 fx;                      /* FX_* */
    i16 food, reagent;          /* FD_*, RG_* dropped (-1 none) */
} FloraDef;

/* An animal: drawn from a body plan with its sizes and colours. */
enum { BP_QUAD, BP_HOPPER, BP_BIRD, BP_FLYER, BP_FISH, BP_CRAWLER, BP_SERPENT, BP_JELLY, BP_WHALE, BP_STRIDER };
enum { TM_SHY, TM_CALM, TM_FIERCE };
typedef struct FaunaDef {
    const char *name;
    const char *var[3];
    u8 plan, temper;            /* BP_*, TM_* */
    float size, speed;          /* size: 1 is a hare */
    u8 hp;
    u8 col[3][3];               /* body colour per variant */
    u8 glow;                    /* 1: a glowing part */
    i16 food, reagent;
    const char *say;            /* what it does when used (E), NULL nothing */
} FaunaDef;

typedef struct RealmDef {
    const char *name;           /* "the Pale Moon" */
    const char *keep;           /* "the Silver Keep" */
    const char *ruler;          /* "the Silver King" */
    const char *blurb;          /* one line, at the Gate */
    const char *lore;           /* the story tie-in; %F fortress, %B builders, %S what was sealed, %N the night */
    u8 stratum;                 /* ST_*: the monsters and the sky's look */
    u8 level;                   /* the danger: monster level */
    u8 terrain, feature;        /* TR_*, FE_* */
    u8 liquid;                  /* B_WATER, B_LAVA, B_ICE, B_ACID, B_OIL, 0 */
    i8 sea;                     /* the liquid's top, from the mean ground (cells) */
    u8 amp;                     /* hills, in cells */
    float gravity;              /* 1 is the fortress */
    float temp;                 /* base temperature, degrees */
    u8 sky[3];                  /* light from the sky (0..15), 0: none */
    float fog[3], fog_density;
    float stars, moons;
    u32 rules;                  /* RR_* */
} RealmDef;
/* Realm r's plants and animals are REALM_FLORA[r * 3 + i] and REALM_FAUNA[r * 3 + i]: plant 0
 * is low ground cover, 1 grows tall, 2 glows. */

#define REALM_FLORA_COUNT (REALM_COUNT * 3)
#define REALM_FAUNA_COUNT (REALM_COUNT * 3)
extern const RealmDef REALMS[REALM_COUNT];
extern const FloraDef REALM_FLORA[REALM_FLORA_COUNT];
extern const FaunaDef REALM_FAUNA[REALM_FAUNA_COUNT];

/* The realm a flavor belongs to (FL_R_*), or -1. */
int realm_of_flavor(int flavor);
int realm_flavor(int r);

/* The realms joined to a world: 6 to 8 of the 21, by its seed (bits). */
u32 realm_pick(u64 seed);

/* Places in a realm, world coordinates: where you arrive (by the gate home) and the keep. */
void realm_arrival(int r, int *x, int *y, int *z);
void realm_keep(int r, int *x, int *y, int *z);
/* The ground height the generator builds at a column (the land, without its keep). */
int realm_height(u64 seed, int r, int wx, int wz);

#endif
