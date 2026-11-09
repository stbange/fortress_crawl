#ifndef FC_FORGE_H
#define FC_FORGE_H

/* The forge (forge.c, docs/FORGE.md): smithing as a skill to learn and master. Ores are
 * mined from veins, essences and titan hearts drop from monsters, supplies are bought from
 * the smith at home; blueprints and runes are learned by breaking weapons down. A weapon
 * is forged at the anvil by keeping the metal at the right heat and striking in time. */

#include "core/common.h"
#include "core/fmath.h"

enum {
    /* mined from ore veins */
    FG_IRON_ORE, FG_SILVER_ORE, FG_MITHRIL_ORE, FG_ADAMANT_ORE, FG_STAR_ORE,
    /* ingots: smelted, salvaged, or (steel, moonsilver) bought */
    FG_IRON, FG_STEEL, FG_SILVER, FG_MITHRIL, FG_ADAMANT, FG_MOONSILVER, FG_STARMETAL,
    /* essences: monsters of a region's element drop them */
    FG_ESS_FIRE, FG_ESS_FROST, FG_ESS_STORM, FG_ESS_VENOM, FG_ESS_HOLY, FG_ESS_SHADOW, FG_ESS_EARTH, FG_ESS_ARCANE,
    FG_TITAN_HEART, /* from bosses */
    /* only bought */
    FG_CHARCOAL, FG_FLUX, FG_QUENCH_OIL, FG_RUNE_INK,
    FG_COUNT
};
#define FG_ESS0 FG_ESS_FIRE
#define FG_ESS_COUNT 8

typedef struct ForgeMatDef {
    const char *name;
    vec3 color;
    u8 kind; /* 0 ore, 1 ingot, 2 essence, 3 heart, 4 supply */
} ForgeMatDef;
extern const ForgeMatDef FORGE_MATS[FG_COUNT];

/* The metals a weapon can be forged of. */
enum { MT_IRON, MT_STEEL, MT_SILVER, MT_MITHRIL, MT_ADAMANT, MT_MOONSILVER, MT_STARMETAL, MT_COUNT };
typedef struct MetalDef {
    const char *name;
    u8 ingot;          /* FG_* */
    u8 tier;
    float lo, hi;      /* the heat to strike it at (0..1) */
    float cool;        /* how fast it cools */
    int bonus;         /* levels above your own it can reach */
    u8 material;       /* MAT_* the weapon is made of */
    u8 smith, stage;   /* smithing level and forge stage it needs */
    bool flux;         /* needs flux to work */
} MetalDef;
extern const MetalDef METALS[MT_COUNT];

#define SMITH_MAX 10
int smith_level(int xp);
int smith_xp_for(int level); /* total xp to reach a level */
/* Ingots a blueprint needs. */
int forge_ingots(int base);

#endif
