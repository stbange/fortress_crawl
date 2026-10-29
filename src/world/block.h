#ifndef FC_BLOCK_H
#define FC_BLOCK_H

#include "core/common.h"

/* One byte per 1 m cell. The look of a block comes from the chunk's theme. */
typedef u8 Block;

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
    B_COUNT
};

#define BLOCK_TEX_SLOTS 32 /* texture slots per theme (>= B_COUNT) */

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

static inline bool block_solid(Block b) { return BLOCKS[b].solid; }
static inline bool block_opaque(Block b) { return BLOCKS[b].opaque; }

#endif
