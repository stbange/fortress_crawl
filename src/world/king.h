#ifndef FC_KING_H
#define FC_KING_H

/* The Old King (docs/OLD_KING.md): the main storyline of every fortress. One king per world,
 * of ten kinds, hidden at first; ten steps lead to his throne, drawn from the towns, the
 * Sanctum, the realms behind the Gate, the dragons, the forge and alchemy, and the dungeons.
 * Settling with him (a fight, his true name, an artefact) wins the game. Like the history, the
 * way is a pure function of the seed; only what players did is saved. */

#include "core/common.h"

struct World;

enum { KK_TYRANT, KK_NECRO, KK_MAGE, KK_DRAGON, KK_HOLLOW, KK_CLOCK, KK_BEAST, KK_PRIEST, KK_ALCHEMIST, KK_STONE, KING_KINDS };

/* The kinds of step. Group A: finding out (steps 1-3, any order); B: the means (4-7, any
 * order); C: the approach (8, then 9); then the throne (10). */
enum {
    KS_HERALD, KS_WITNESS, KS_TRUENAME, KS_VISION,                                 /* A */
    KS_DRAGON, KS_HOARD, KS_ALLEGIANCE, KS_FORGE, KS_BREW, KS_REALM, KS_RELIC,       /* B */
    KS_LIEUTENANT, KS_SEAL, KS_WYRM,                                                 /* C */
    KS_THRONE, KS_COUNT
};
#define KING_STEPS 10

typedef struct KingKind {
    const char *name;        /* "the Tyrant" */
    const char *what;        /* one line: what he is */
    const char *throne;      /* "the Throne Hall" */
    const char *artefact;    /* "the Breaking-Stone" */
    u8 artefact_step;        /* KS_* that gives it */
    i8 dir;                  /* -1 his throne is deep, +1 high, 0 on the gate's layers */
    const char *word_end;    /* the ending by his true name */
    const char *art_end;     /* the ending by the artefact */
    const char *fight_end;   /* the ending by the sword */
    const char *minions;     /* who serves him: "his iron knights" */
} KingKind;
extern const KingKind KING_KINDS_DEF[KING_KINDS];
extern const char *KING_STEP_NAMES[KS_COUNT];

typedef struct KingStep {
    u8 type;                 /* KS_* */
    u8 group;                /* 0 A, 1 B, 2 C (8), 3 C (9), 4 the throne */
    int cx, cy, cz;          /* the chunk it happens in (structures: the town's, the lair's centre chunk) */
    u64 key;                 /* a town's or a lair's key */
    int arg;                 /* KS_REALM: the realm; KS_DRAGON / KS_WYRM: the lair's dragon kind */
} KingStep;

typedef struct KingWay {
    u64 seed;
    int kind;                /* KK_* */
    char name[40];           /* "Aldric the Ninth" */
    KingStep step[KING_STEPS];
    int tgx, tgy, tgz;       /* the throne's placement cell (layout.c) */
    bool valid;
} KingWay;

int king_kind_of(u64 seed);
void king_throne_cell(u64 seed, int *gx, int *gy, int *gz);
/* The whole way of a world (cached per seed). realms: the realms joined to the world (bits). */
const KingWay *king_way(const struct World *w, u32 realms);

/* The throne hall: a hall structure of this variant (gen_struct.c builds a hall, then the dais). */
#define HALL_THRONE 5

#endif
