#ifndef FC_BLOCK_H
#define FC_BLOCK_H

#include "core/common.h"

/* One byte per 1 m cell. The look of a block comes from the chunk's theme. The top bit marks
 * a block that has been broken into: its 4x4x4 pieces of 25 cm are kept in the chunk's carve
 * set (carve.h); the low bits stay its material (docs/DESTRUCTION.md). */
typedef u8 Block;
#define BLOCK_CARVED 0x80
#define BLOCK_MAT 0x7F

enum {
    B_AIR = 0,
    /* full cubes */
    B_ROCK,      /* natural rock */
    B_WALL,      /* masonry / built walls */
    B_FLOOR,     /* paved floor */
    B_CEIL,      /* ceiling masonry */
    B_PILLAR,    /* carved columns */
    B_WOOD,      /* beams, planks */
    B_METAL,     /* iron */
    B_ACCENT,    /* ornament: gold, jade, bronze... */
    B_DIRT,      /* earth, gravel, ash */
    B_LAVA,      /* glowing, not walkable later (damage) */
    B_WATER,     /* non-solid, opaque surface */
    B_CRYSTAL,   /* glowing */
    B_ICE,
    B_BRAZIER,   /* iron bowl, has fire above */
    B_GLASS,     /* glowing window */
    B_ROOF,      /* roof tiles, statues */
    B_ORE,       /* veins in rock (mines) */
    B_SHRINE,    /* glowing shrine stone: touching it sets the respawn point */
    /* sprites: drawn as two crossed quads */
    B_TORCH,
    B_FIRE,
    B_WEB,
    B_CHAIN,
    B_BARS,
    B_ROOTS,
    B_FUNGUS,
    B_LANTERN,
    B_BONES,
    B_GRASS,
    /* living things and what the simulation leaves (docs/SIMULATION.md) */
    B_MOSS,      /* cube: stone grown over with moss */
    B_VINE,      /* sprite: hangs, grows down, can be climbed */
    B_FLOWER,    /* sprite: grows where there is light, feeds bees */
    B_HIVE,      /* sprite: a wild beehive hanging from above */
    B_ASH,       /* sprite: what fire leaves; things grow well in it */
    B_OBSIDIAN,  /* cube: lava cooled by water */
    B_OIL,       /* liquid: floats on water, burns hot */
    B_ACID,      /* liquid: eats stone, hurts, water dilutes it */
    B_RUBBLE,    /* cube: what a collapse leaves; loose, it slides off a pile */
    B_COUNT
};

#define BLOCK_TEX_SLOTS 48 /* texture slots per theme (>= B_COUNT; the atlas holds 32 themes of them) */

enum { RENDER_NONE, RENDER_CUBE, RENDER_LIQUID, RENDER_CROSS };

typedef struct BlockDef {
    const char *name;
    u8 render;
    bool solid;     /* collides with the player */
    bool opaque;    /* hides neighbouring faces and blocks light */
    bool emissive;  /* drawn full-bright */
    u8 light;       /* emitted light level 0..15 (colour comes from the theme) */
    u8 light_kind;  /* which theme light colour to use */
    bool fire;      /* contributes to flicker */
} BlockDef;

enum { LIGHT_TORCH, LIGHT_LAVA, LIGHT_CRYSTAL, LIGHT_GLASS, LIGHT_FUNGUS };

extern const BlockDef BLOCKS[B_COUNT];

static inline Block block_mat(Block b) { return (Block)(b & BLOCK_MAT); }
static inline bool block_carved(Block b) { return (b & BLOCK_CARVED) != 0; }
static inline bool block_solid(Block b) { return BLOCKS[b & BLOCK_MAT].solid; }
/* a broken block lets light and sight through (its pieces are drawn on their own) */
static inline bool block_opaque(Block b) { return !(b & BLOCK_CARVED) && BLOCKS[b & BLOCK_MAT].opaque; }

/* How hard a material is to break: 0 never, 1 soft (anything breaks it), 2 rock and ore (a
 * pickaxe), 3 masonry (explosives), 4 metal (nothing the player has). */
extern const u8 BLOCK_HARDNESS[B_COUNT];
enum { HARD_NEVER, HARD_SOFT, HARD_ROCK, HARD_MASONRY, HARD_METAL };

/* What a cell shows besides its block (docs/SIMULATION.md): two bits each. */
#define STAIN_WET(s) ((s) & 3)
#define STAIN_SOOT(s) (((s) >> 2) & 3)
#define STAIN_SCORCH(s) (((s) >> 4) & 3)
#define STAIN_AGE(s) (((s) >> 6) & 3) /* a plant: 1 a shoot, 2 young, 0 grown */
#define STAIN_MAKE(wet, soot, scorch, age) ((u8)((wet) | ((soot) << 2) | ((scorch) << 4) | ((age) << 6)))

/* What a material is like, for the rules of the simulation (docs/SIMULATION.md): what reads
 * this table needs no special cases for a new block. */
typedef struct Substance {
    i16 ignite;      /* degrees it catches fire at (0: does not burn) */
    u8 fuel;         /* how long it burns, in fire's level (eighths of a long burn) */
    i16 melt;        /* degrees it melts at (0: does not) ... */
    u8 melt_to;      /* ... into this */
    bool lightning;  /* carries lightning (metal) */
    u8 density;      /* tenths of water's: below 10 floats */
    bool porous;     /* water seeps through it when broken */
    bool nutrient;   /* plants root in it */
    u8 span;         /* how far it reaches without support (support.h); 0: bears no load */
} Substance;
extern const Substance SUBSTANCES[B_COUNT];

#endif
