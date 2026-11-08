#ifndef FC_FOOD_H
#define FC_FOOD_H

/* Food and cooking (food.c): ingredients from wildlife, plants and larders, recipes made
 * of tags, meals that last for minutes. See docs/FOOD.md. */

#include "core/common.h"
#include "core/fmath.h"

/* What recipes ask for. An ingredient has one to three. */
enum {
    TG_MEAT, TG_FISH, TG_EGG, TG_DAIRY, TG_STAPLE, TG_EARTHY, TG_FRESH, TG_SWEET, TG_SHARP, TG_SALTY, TG_FAT,
    TG_FIRE, TG_FROST, TG_MANA, TG_HOLY, TG_HEARTY, TG_RARE, TG_SWIFT, TG_GLOW, TG_BITTER, TG_LEAN, TG_STAR,
    TG_COUNT,
    TG_ANY = 254, /* a recipe slot anything fills */
    TG_NONE = 255
};
#define TGB(t) (1u << (t))

enum {
    /* from animals */
    FD_HARE, FD_FOWL, FD_GROUSE, FD_QUAIL, FD_GOAT, FD_MUTTON, FD_SALAMANDER, FD_EGG, FD_QUAIL_EGG, FD_MILK,
    FD_CAVE_FISH, FD_SKY_CARP, FD_CRAB,
    /* hives and small creatures */
    FD_HONEY, FD_ROYAL_JELLY, FD_BEETLE_JELLY, FD_SNAIL,
    /* growing */
    FD_MUSHROOM, FD_GLOWCAP, FD_HERB, FD_MOSS, FD_TUBER, FD_BAMBOO, FD_JADE_LEAF, FD_PEPPER, FD_ONION,
    FD_FROSTBERRY, FD_MELON, FD_SEAGRAPE, FD_BARROW_ROOT,
    /* larders and cellars */
    FD_GRAIN, FD_SALT, FD_OIL, FD_WINE, FD_SPICES, FD_STARFRUIT,
    FD_COUNT
};

/* How an ingredient looks on the ground and in the larder. */
enum { FM_MEAT, FM_EGG, FM_BOTTLE, FM_FISH, FM_SHELL, FM_MUSHROOM, FM_SPRIG, FM_ROOT, FM_BERRY, FM_MELON, FM_SACK, FM_COMB, FM_COUNT };

typedef struct FoodDef {
    const char *name;
    u32 tags;
    vec3 color;
    u8 model;
} FoodDef;
extern const FoodDef FOODS[FD_COUNT];

/* What a meal does. Values add up from the meal, a raw snack and the feast. */
enum {
    ME_MAXHP,         /* fraction more max health */
    ME_MAXMANA,       /* fraction more max mana */
    ME_DAMAGE,        /* fraction more weapon damage */
    ME_SPEED,         /* fraction faster */
    ME_REGEN,         /* out of a fight: health comes back this much faster (1 = twice) */
    ME_FIGHT_REGEN,   /* fraction of max health per second, even in a fight */
    ME_MANA_REGEN,    /* mana flows this much faster (1 = twice) */
    ME_FIRE_RES,      /* 1: fire hurts less and burns end at once */
    ME_FROST_RES,     /* 1: frost hurts less, never chilled */
    ME_POISON_IMM,    /* 1: never poisoned */
    ME_POTION,        /* potions heal this fraction more */
    ME_PROJ_RES,      /* projectiles hurt this fraction less */
    ME_LIGHT,         /* 1: a light round you, hidden things show close by */
    ME_SPELL_POWER,   /* spells this fraction stronger */
    ME_STUN_RES,      /* knockback this fraction less */
    ME_JUMP,          /* jump this fraction higher */
    ME_ATTACK_SPEED,  /* attacks this fraction faster */
    ME_IGNITE,        /* chance a weapon hit sets the foe alight */
    ME_TAKEN,         /* fraction more damage taken */
    ME_SPELL_COST,    /* spells cost this fraction less */
    ME_FALL,          /* falls hurt this fraction less */
    ME_GLIDE,         /* 1: a moment of glide after a jump */
    ME_COOLDOWN,      /* spell cooldowns this fraction shorter */
    ME_LAVA,          /* lava burns this fraction less */
    ME_TRAVEL,        /* 1: the meal lasts three times as long */
    /* elixirs (alchemy.c) */
    ME_LIFESTEAL,     /* this fraction of weapon damage heals you */
    ME_CRIT,          /* this much more chance of a critical hit */
    ME_REVIVE,        /* 1: a killing blow lifts you again at half health */
    ME_LUCK,          /* more loot from what you kill */
    ME_CHAIN,         /* weapon hits arc to the nearest foe for this fraction */
    ME_FROST_AURA,    /* 1: foes close by are slowed */
    ME_FEAR_AURA,     /* 1: foes close by run */
    ME_KNOCK,         /* blows throw this much farther */
    ME_BLAST,         /* blows burst for this fraction round the foe */
    ME_POISON_HITS,   /* hits poison for this fraction a second */
    ME_SHOCK_RES,     /* 1: lightning does not jolt you */
    ME_COUNT
};

#define RECIPE_EFFECTS 4
typedef struct Recipe {
    const char *name;
    u8 slots;                 /* 2 campfire, 3 fireplace, 4 kitchen */
    u8 need[4];               /* TG_* (TG_NONE past slots) */
    struct { u8 e; float v; } fx[RECIPE_EFFECTS];
    const char *text;         /* what it does, for the cookbook */
} Recipe;

/* 32 recipes and the simple meal (anything that fits none). */
#define RECIPE_COUNT 32
#define RC_SIMPLE RECIPE_COUNT
#define RC_ALL (RECIPE_COUNT + 1)
extern const Recipe RECIPES[RC_ALL];

/* Where you cook: nothing, a campfire (2 ingredients), the dining hall's fireplace (3), the
 * Sanctum kitchen (4). */
enum { CP_NONE, CP_CAMPFIRE, CP_FIREPLACE, CP_KITCHEN, CP_COUNT };
#define DISH_QUALITIES 4

/* The recipe some ingredients make (RC_SIMPLE when none fits). */
int food_match(const u8 *ings, int n);
/* Quality 0-3 of a dish from these ingredients cooked here. */
int food_quality(const u8 *ings, int n, int place);
float food_meal_seconds(int recipe, int place);
const char *food_quality_name(int q);
/* "meat + salty" */
void food_need_text(const Recipe *r, char *out, int n, bool hint);
void food_tags_text(u32 tags, char *out, int n);

#endif
