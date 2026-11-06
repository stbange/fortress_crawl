#ifndef FC_THEME_H
#define FC_THEME_H

#include "core/common.h"
#include "world/block.h"

/* A place in the world is described by two independent axes:
 *  - the STRATUM, picked by height: a new one every 10 chunks, ten down (mines, roots,
 *    ossuary, fungal deep, limbo, crystal abyss, inferno, iron hells, the maw, world core)
 *    and ten up (castle, spires, aeries, cloud cloisters, hanging gardens, heaven, sun
 *    court, stormcrown, orrery, astral void). It controls layout: which chunk archetypes
 *    and megastructures appear, how open the space is, features (lava, crystals...).
 *  - the FLAVOR, picked by compass direction: a new one every 10 chunks out, ten in each
 *    direction (north colder, south hotter, east and west each their own culture).
 *    It controls look: palettes, wall patterns, light colours, decoration.
 * stratum x flavor = theme, e.g. "Frostbitten Mines" or "Jade Heaven". */

/* bottom to top */
typedef enum {
    ST_CORE, ST_MAW, ST_IRONHELL, ST_INFERNO, ST_CRYSTAL, ST_LIMBO, ST_FUNGAL, ST_OSSUARY, ST_ROOTS, ST_MINES,
    ST_DUNGEON,
    ST_CASTLE, ST_SPIRES, ST_AERIE, ST_CLOUDS, ST_GARDENS, ST_HEAVEN, ST_SUNCOURT, ST_STORM, ST_ORRERY, ST_ASTRAL,
    ST_COUNT
} Stratum;

/* Ten tiers per direction, one per 10 chunks out (tier 1 from 4 chunks). */
#define FLAVOR_TIERS 10
typedef enum {
    FL_OLD,
    /* north */
    FL_FROST, FL_GLACIAL, FL_EVERWINTER, FL_RIMEIRON, FL_BARROW, FL_TUSK, FL_AURORA, FL_STARFROST, FL_HOLLOW, FL_LASTCOLD,
    /* south */
    FL_SANDSTONE, FL_SUNSCORCHED, FL_EMBER, FL_OBSIDIAN, FL_BRASS, FL_SALT, FL_SULPHUR, FL_GLASSBLOWN, FL_PHOENIX, FL_SUNBLAST,
    /* east */
    FL_BOYAR, FL_STEPPE, FL_JADE, FL_PORCELAIN, FL_BAMBOO, FL_TERRACOTTA, FL_SILK, FL_DRAGON, FL_MOONLIT, FL_IMPERIAL,
    /* west */
    FL_GOTHIC, FL_DRUIDIC, FL_DROWNED, FL_CORAL, FL_ABYSSAL, FL_VERDIGRIS, FL_PEARL, FL_LEVIATHAN, FL_BRINE, FL_ELDRITCH,
    FL_COUNT
} Flavor;
/* First flavor of each direction (axis 1 north, 2 south, 3 east, 4 west). */
static inline int flavor_first(int axis) { return 1 + (axis - 1) * FLAVOR_TIERS; }

typedef enum {
    ARCH_ROOMS,  /* rooms and corridors */
    ARCH_CRAWL,  /* narrow squeeze and crawl tunnels */
    ARCH_CAVE,   /* natural caves */
    ARCH_MINE,   /* timbered mine tunnels */
    ARCH_CELLS,  /* prison / crypt block */
    ARCH_ARENA,  /* one big room */
    ARCH_VOID,   /* open air: floating platforms and bridges */
    ARCH_COUNT,
    ARCH_STRUCTURE = 100, /* chunk belongs to a megastructure */
    ARCH_EXTERIOR = 101   /* outside, in front of the castle */
} Archetype;

typedef enum {
    STRUCT_NONE, STRUCT_VESTIBULE, STRUCT_HALL, STRUCT_CAVERN, STRUCT_CHASM, STRUCT_TYPE_COUNT
} StructType;

typedef enum { PAT_BRICK, PAT_BIGBRICK, PAT_ASHLAR, PAT_LOGS, PAT_PLASTER, PAT_ICE, PAT_MARBLE, PAT_BASALT, PAT_RUNES, PAT_SCALES } WallPattern;
typedef enum { FP_FLAGSTONE, FP_TILES, FP_PLANKS, FP_DIRT, FP_MOSAIC, FP_GRATE } FloorPattern;

/* What grows on the floating islands of open-air chunks. */
enum { ISLE_PLAIN, ISLE_TEMPLE, ISLE_CRYSTAL, ISLE_GARDEN, ISLE_ORRERY, ISLE_NEST };

typedef struct { u8 r, g, b; } Col;

typedef struct StratumDef {
    const char *name;
    int y_lo, y_hi; /* chunk-y band (inclusive) */
    float arch_w[ARCH_COUNT];
    float struct_w[STRUCT_TYPE_COUNT];
    Col tint; float tint_amt; float bright; float desat;
    Col fog; float fog_mix; float fog_density;
    Col light; float light_mix;
    u8 ambient;           /* minimum light level 0..15 */
    bool sky;             /* stars in the background */
    float shaft_chance;   /* vertical spiral stair connections */
    float lava, crystal, fungus, roots; /* feature frequencies */
    u8 isles;             /* ISLE_*: what stands on floating islands */
    u8 wall_pat;          /* 0: the flavor's, else WallPattern + 1 (marble halls up high) */
    bool ore, bony, fiery, stained; /* ore veins, bones about, fiery halls, stained glass */
    Col crystal_col;      /* crystal colour (0,0,0: from the flavor) */
} StratumDef;

typedef struct FlavorDef {
    const char *name, *adj;
    int axis, tier; /* axis 0 none, 1 north, 2 south, 3 east, 4 west */
    Col rock, wall, mortar, floor, pillar, wood, metal, accent, dirt, glass, roof;
    u8 wall_pat, floor_pat;
    Col torch, fog;
    u8 decor;       /* typical decoration sprite */
    u8 fluid;       /* B_WATER, B_LAVA, B_ICE or 0 */
    bool stained;   /* stained glass windows */
} FlavorDef;

extern const StratumDef STRATA[ST_COUNT];
extern const FlavorDef FLAVORS[FL_COUNT];
extern const char *ARCH_NAMES[ARCH_COUNT];
extern const char *STRUCT_NAMES[STRUCT_TYPE_COUNT];

#define THEME_COUNT (ST_COUNT * FL_COUNT)
static inline int theme_id(int stratum, int flavor) { return stratum * FL_COUNT + flavor; }
static inline int theme_stratum(int theme) { return theme / FL_COUNT; }
static inline int theme_flavor(int theme) { return theme % FL_COUNT; }

/* Resolved colours for one theme. */
typedef struct ThemeInfo {
    Col mat[BLOCK_TEX_SLOTS];  /* base colour per block */
    Col mortar, glass, roof;
    u8 light[5][3];            /* emitted colour per light kind, as 0..15 levels at full strength */
    float fog[3];              /* 0..1 */
    float fog_density;
    float ambient[3];          /* 0..1 */
    bool sky;
    u8 wall_pat, floor_pat;
} ThemeInfo;

void theme_info(int theme, ThemeInfo *out);
/* "Frostbitten Mines" */
void theme_name(int theme, char *buf, int n);

#endif
