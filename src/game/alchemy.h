#ifndef FC_ALCHEMY_H
#define FC_ALCHEMY_H

/* Alchemy (alchemy.c, docs/ALCHEMY.md): reagents from monsters and strange places, exact
 * formulae of ingredients and steps in order, elixirs that take hold after a few seconds
 * and are strong for 20-30. */

#include "core/common.h"
#include "core/fmath.h"

/* Reagents: one from each kind of monster (in MON_* order), then strange finds. */
enum {
    RG_ICHOR, RG_GALL, RG_WISP, RG_HORN, RG_BONEDUST, RG_SLIME, RG_BATWING, RG_VENOM, RG_WRAITH, RG_HEARTSTONE,
    RG_FEATHER, RG_BILE, RG_LENS, RG_RUST, RG_TOOTH, RG_TONGUE, RG_WAX, RG_PLUME, RG_FANG, RG_TROLLBLOOD,
    RG_MONSTER_COUNT,
    RG_QUICKSILVER = RG_MONSTER_COUNT, RG_BRIMSTONE, RG_ORCHID, RG_DEW, RG_VOIDSALT, RG_MANDRAKE, RG_AMBER, RG_PHOENIX,
    RG_COUNT
};

enum { RM_VIAL, RM_STONE, RM_FEATHER, RM_FANG, RM_DUST, RM_LENS, RM_ROOT, RM_FLAME, RM_COUNT };
typedef struct ReagentDef {
    const char *name;
    vec3 color;
    u8 model;
    bool glow;
} ReagentDef;
extern const ReagentDef REAGENTS[RG_COUNT];

/* A step of a formula: an ingredient from the larder (FD_*), a reagent, or something done. */
enum { AS_GRIND, AS_HEAT, AS_STIR, AS_CHILL, AS_DISTILL, AS_COUNT };
#define STEP_FOOD(f) ((u8)(f))
#define STEP_RG(r) ((u8)(64 + (r)))
#define STEP_ACT(a) ((u8)(128 + (a)))
#define STEP_NONE 255
static inline bool step_is_food(u8 s) { return s < 64; }
static inline bool step_is_rg(u8 s) { return s >= 64 && s < 128; }
static inline bool step_is_act(u8 s) { return s >= 128 && s < 128 + AS_COUNT; }

#define FORMULA_STEPS 7
#define ELIXIR_EFFECTS 4
typedef struct Elixir {
    const char *name;
    u8 steps[FORMULA_STEPS]; /* STEP_NONE past the end */
    float onset, last;       /* seconds before it takes hold, and how long it holds */
    struct { u8 e; float v; } fx[ELIXIR_EFFECTS]; /* ME_* (food.h) while it holds */
    u8 start;                /* EX_*: something done once when it takes hold */
    vec3 color;
    const char *text;
} Elixir;

/* Done once as an elixir takes hold. */
enum { EX_NONE, EX_INVISIBLE, EX_WINGS, EX_TIMESLIP, EX_WARD, EX_SIGHT };

#define ELIXIR_COUNT 27
extern const Elixir ELIXIRS[ELIXIR_COUNT];
#define ELIXIRS_START ((1u << 0) | (1u << 1)) /* the formulae every character knows */

int alchemy_steps(const u8 *steps); /* how many steps a formula has */
void alchemy_step_name(u8 step, char *out, int n);
/* The elixir these steps make exactly, or -1. */
int alchemy_match(const u8 *steps, int n);
/* The formula these steps come closest to (most steps right, in their places); *right and
 * *len say how close. -1 if nothing is close. */
int alchemy_closest(const u8 *steps, int n, int *right, int *len);

#endif
