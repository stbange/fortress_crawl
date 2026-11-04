#ifndef FC_STORY_H
#define FC_STORY_H

#include "world/layout.h"

/* Stories are part of the world: pure functions of (seed, coordinates), like the
 * blocks. Nothing is simulated. Any chunk can compute who rules its domain, what
 * happened there and which named enemy lives where, so a note can point at a place
 * that has not been generated yet. Only what players change or learn is saved.
 *
 * Layers:
 *   world myth   - one per seed: the fortress, its builders, what it seals, its fall
 *   arcs         - the metaplot: arc k is one Power with a captain and a general under
 *                  it, three chapters, each farther from the gate; the Power's death
 *                  points to the next arc's Power
 *   domains      - irregular regions of a few chunks with a ruling faction, a state,
 *                  a lord and a past
 *   traces       - inscriptions, dead adventurers' notes and bounty boards in chunks
 * Depth is time (the deeper, the older the dead), height is power, the compass gives
 * the cultures, distance gives rank. */

enum { CULT_OLD, CULT_NORTH, CULT_SOUTH, CULT_EAST, CULT_WEST, CULT_COUNT };
enum { MOTIVE_REVENGE, MOTIVE_GLORY, MOTIVE_RICHES, MOTIVE_TRUTH, MOTIVE_COUNT };
enum { DS_HELD, DS_ABANDONED, DS_PLAGUED, DS_WARRING, DS_SEALED, DS_COUNT };
enum { TR_STELE, TR_CORPSE, TR_BOUNTY, TR_COUNT };
enum { NK_CAPTAIN, NK_GENERAL, NK_POWER, NK_LORD };
/* Monster families as the game numbers them (game.h MON_*). */
enum { SM_CRAWLER, SM_BRUTE, SM_WISP, SM_HORNED, SM_SKELETON, SM_SLIME, SM_BAT };

#define STORY_NAME 48
#define STORY_TEXT 320

extern const char *MOTIVE_NAMES[MOTIVE_COUNT];
extern const char *MOTIVE_PITCH[MOTIVE_COUNT];
extern const char *CULTURE_NAMES[CULT_COUNT];

typedef struct StoryWorld {
    u64 seed;
    char fortress[STORY_NAME]; /* "Ostgard" */
    char builders[STORY_NAME]; /* "the Ninefold Kings" */
    char sealed[STORY_NAME];   /* "the Pale Hunger" */
    char fall[STORY_NAME];     /* "the Night of the Open Gate" */
} StoryWorld;

typedef struct Domain {
    u64 id;
    int gx, gy, gz;  /* domain cell */
    int ccx, ccy, ccz; /* centre chunk */
    u8 culture, stratum, state;
    int level;
    char name[STORY_NAME];    /* "the Cells of Morwen" */
    char faction[STORY_NAME]; /* "the Grey Wardens" */
    char rival[STORY_NAME];   /* the other side when the state is DS_WARRING */
    char era[STORY_NAME];     /* when its history happened (deeper = older) */
} Domain;

/* A named enemy: chapter targets and domain lords. */
typedef struct Named {
    u64 key;    /* never 0; remembered once killed */
    int kind;   /* NK_* */
    int chapter;
    int cx, cy, cz; /* home chunk */
    u8 culture, mon;
    float scale, hp_mult, dmg_mult;
    char name[STORY_NAME];
    char title[STORY_NAME];
} Named;

/* Something to read in a chunk. */
typedef struct Trace {
    u64 key;
    u8 kind;      /* TR_* */
    bool mission; /* a corpse with a retrieve request, or a bounty board */
} Trace;

/* A lost item that a dead adventurer asks you to take home. */
typedef struct Retrieve {
    u64 key; /* the item */
    int cx, cy, cz;
    char item[STORY_NAME];
    char owner[STORY_NAME];
} Retrieve;

/* What a character's motive makes of the metaplot (per character slot). */
typedef struct Personal {
    char village[STORY_NAME], kin[24];
} Personal;

void story_world(u64 seed, StoryWorld *out);
void story_personal(u64 seed, int slot, Personal *out);

/* The domain a chunk belongs to (nearest jittered centre on a coarse lattice). */
void story_domain(const World *w, int cx, int cy, int cz, Domain *out);
/* One-line description of a domain's state: "held by the Grey Wardens". */
void story_domain_line(const Domain *d, char *out, int n);
/* The lord of a domain and the chunk it holds. */
void story_lord(const World *w, const Domain *d, Named *out);
/* Is this chunk where its domain's lord lives? */
bool story_lord_here(const World *w, int cx, int cy, int cz, Named *out);

/* Chapter c of the metaplot: arc c/3, rank c%3 (captain, general, Power). */
void story_chapter_target(const World *w, int chapter, Named *out);
/* Texts. For the motive m (MOTIVE_*): the hook when chapter c starts, and the line
 * when its target falls. The revelation is the metaplot fact the kill uncovers. */
void story_chapter_hook(const World *w, int m, int chapter, const Personal *p, char *out, int n);
void story_chapter_done(const World *w, int m, int chapter, const Personal *p, char *out, int n);
void story_revelation(const World *w, int chapter, char *out, int n);
void story_arc_title(const World *w, int arc, char *out, int n);

/* Traces: whether chunk (cx,cy,cz) holds one, and what it says. */
bool story_trace(const World *w, int cx, int cy, int cz, Trace *out);
void story_trace_text(const World *w, int cx, int cy, int cz, char *out, int n);
/* The mission behind a corpse note or a bounty board. */
bool story_retrieve(const World *w, int cx, int cy, int cz, Retrieve *out);
bool story_bounty(const World *w, int cx, int cy, int cz, Named *lord, int *gold);

/* "140 M NORTH-EAST, 2 LAYERS DOWN" from a world position (feet + 1) to a chunk's centre. */
void story_direction(float fx, float fy, float fz, int cx, int cy, int cz, char *out, int n);

#endif
