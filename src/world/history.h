#ifndef FC_HISTORY_H
#define FC_HISTORY_H

/* A world's hidden history (docs/STORY_GEN.md). At world start a small simulation runs the
 * fortress's past from the seed alone: people and factions with traits, goals and relations
 * act through events with causes and effects. The game then picks the best mystery in that
 * history, casts its survivors as the named enemies of the first three arcs, and spreads
 * its facts as clues over chapters, traces, legendaries and hunters. Everything the story
 * says in a world is a report about this one history.
 *
 * Pure and deterministic: history_of(seed) always gives the same history, and it is cached
 * (it takes well under a millisecond). Text is made in history_text.c. */

#include "core/common.h"

#define H_NONE 255
#define H_ACTORS 64
#define H_FACTIONS 10
#define H_EVENTS 112
#define H_ARTS 16
#define H_FACTS 96
#define H_CLUES 200
#define H_CAST 9 /* chapters 0-8: the main mystery, three arcs */
#define H_QUESTIONS 6
#define H_NAME 48
#define UNIQUE_SLOTS 20 /* the world legendaries (UNIQUE_COUNT in the game) */

enum { TONE_HORROR, TONE_HUMAN, TONE_TRAGEDY, TONE_COUNT };
/* What lies below. Horror: something old. Human: nothing; the horror was people.
 * Tragedy: something that was wronged. */
enum { BL_HUNGER, BL_SLEEPER, BL_FIRST, BL_KING, BL_MINERS, BL_PLAGUE, BL_COUNT };
enum { ERA_FOUND, ERA_OLD1, ERA_OLD2, ERA_BEFORE, ERA_FALL, ERA_NOW, ERA_COUNT };

enum { RO_FOUNDER, RO_RULER, RO_KEEPER, RO_CAPTAIN, RO_SCRIBE, RO_SMITH, RO_PRIEST, RO_MERCHANT, RO_SOLDIER, RO_HEIR,
       RO_KING, RO_WITNESS, RO_KIN, RO_COUNT };
enum { TT_AMBITIOUS, TT_DEVOUT, TT_GREEDY, TT_LOYAL, TT_COWARDLY, TT_CURIOUS, TT_CRUEL, TT_MERCIFUL, TT_PROUD, TT_COUNT };
/* What drives someone, and toward what (mtarget), because of what (mcause). */
enum { MV_NONE, MV_GRIEF, MV_REVENGE, MV_POWER, MV_GREED, MV_FAITH, MV_TRUTH, MV_DUTY, MV_PACT, MV_RECLAIM, MV_COUNT };
enum { CRD_BUILDERS, CRD_KEEPERS, CRD_GUARD, CRD_CULT, CRD_TRADERS, CRD_PRIESTS, CRD_COUNT };

enum {
    /* the founding */
    EV_FOUND, EV_SEAL, EV_BAIT, EV_IMPRISON, EV_BURY, EV_KEYS, EV_FEED,
    /* the old eras */
    EV_WAR, EV_MASSACRE, EV_HERESY, EV_PURGE, EV_PACT, EV_HOARD, EV_CHRONICLE, EV_WEAPON, EV_TAKE_RULE, EV_ROLL,
    /* the years before the fall */
    EV_BIRTH, EV_MARRY, EV_OATH, EV_LOSS, EV_VOICE, EV_LEARN, EV_SILENCE, EV_EXILE, EV_DEBT, EV_CONVERT, EV_RIVAL,
    /* the fall */
    EV_STEAL, EV_GIVE, EV_OPEN, EV_BREAKOUT, EV_DESCEND, EV_RETURN, EV_SEIZE, EV_DIE, EV_BLAME, EV_PUNISH, EV_FORGE,
    EV_ACCOUNT, EV_FLEE,
    /* now */
    EV_BURN, EV_SWEAR, EV_HIDE,
    EV_COUNT
};

/* Plot shapes: patterns over the history that make a mystery. */
enum { SH_BETRAYAL, SH_USURPATION, SH_PACT, SH_BAIT, SH_WRONG_MONSTER, SH_MERCY, SH_COVERUP, SH_VILLAGE, SH_VOICE, SH_HEIR,
       SH_COUNT };

/* Kinds of fact. */
enum {
    FK_DEED,  /* event happened, done by its agent */
    FK_ANON,  /* event happened, the doer unknown */
    FK_CLAIM, /* a lie or a mistaken belief: the holder faction says b did the event */
    FK_WHY,   /* why the event's agent did it (its cause) */
    FK_KIN,   /* actors a and b are kin (or sworn) */
    FK_CRACK, /* evidence against a claim */
    FK_FATE,  /* where actor a is now (alive, returned, hidden) */
    FK_COUNT
};
/* Where a clue is found. */
enum { SRC_BODY, SRC_DYING, SRC_STELE, SRC_NOTE, SRC_BOUNTY, SRC_LEGEND, SRC_PAGE, SRC_ORDERS, SRC_COUNT };
/* Questions the STORY tab keeps open until answered. */
enum { Q_WHO, Q_GUILTY, Q_WHY, Q_BELOW, Q_VILLAGE, Q_WHERE, Q_COUNT };

/* actor flags */
#define AF_RETURNED 1 /* what came back up wearing them */
#define AF_HIDDEN 2   /* believed dead, alive */
#define AF_EXILED 4
#define AF_HEARD 8    /* heard the voice below */
#define AF_KIN_OF_PLAYER 16 /* "your grandmother": the player's own kin */
#define AF_WALLED 32  /* walled in alive */

/* relation kinds */
enum { RL_NONE, RL_SIBLING, RL_SPOUSE, RL_PARENT, RL_CHILD, RL_OATH, RL_RIVAL, RL_COUNT };

typedef struct HActor {
    char name[H_NAME];
    u8 culture, faction, role, female;
    u8 trait[2];
    u8 born, died;  /* eras; died H_NONE: alive now */
    u8 death_ev;    /* how they died (event), H_NONE */
    u8 parent, sibling, spouse, child, sworn; /* actors or H_NONE */
    u8 motive, mtarget, mcause; /* MV_*, actor, event (H_NONE) */
    u8 key;         /* artifact held, H_NONE */
    u8 flags;
    i8 cast;        /* chapter, -1 */
} HActor;

typedef struct HFaction {
    char name[H_NAME];
    u8 culture, creed, leader;
    i8 stance[H_FACTIONS];
    u8 flags; /* 1 heretic, 2 destroyed, 4 forger */
} HFaction;

enum { AK_KEY, AK_HOARD, AK_BOOK, AK_FORGED, AK_ACCOUNT, AK_WEAPON, AK_PACT, AK_COUNT };
typedef struct HArtifact {
    char name[H_NAME];
    u8 kind, maker, holder, event; /* holder: actor now, H_NONE lost */
    i8 legend; /* the world legendary it is (0..19), -1 */
} HArtifact;

typedef struct HEvent {
    u8 type, era;
    u8 a, b;      /* agent, patient (actors) */
    u8 f1, f2;    /* factions */
    u8 art;       /* artifact */
    u8 cause;     /* earlier event, H_NONE */
    u8 sub;       /* event-specific: cause of death, kind of punishment, price... */
    u8 place;     /* index into the history's places */
} HEvent;

typedef struct HFact {
    u8 kind, ev;
    u8 a, b;      /* FK_KIN, FK_FATE, FK_CLAIM (b: the one accused) */
    u8 holder;    /* faction that tells it (claims, partisan stones), H_NONE */
    u8 weight;    /* 3 key, 2 important, 1 colour */
    u8 order;     /* the chapter whose revelation carries it, H_NONE */
} HFact;

typedef struct HClue {
    u8 fact, src;
    u8 speaker;   /* actor (dying words, notes), H_NONE */
    u8 chapter;   /* SRC_BODY/DYING/PAGE/ORDERS: the chapter; SRC_LEGEND: the legendary */
    u8 dist;      /* traces: the nearest (in chunks from the gate) it may lie */
} HClue;

typedef struct HQuestion {
    u8 kind;
    u8 need[3], nneed; /* facts; answered when all are known (any one of the alternatives in alt) */
    u8 alt;            /* a second fact that also answers it, H_NONE */
} HQuestion;

typedef struct History {
    u64 seed;
    int attempt;  /* how many histories were thrown away as dull */
    u8 tone, below;
    char fortress[H_NAME], builders[H_NAME], sealed[H_NAME], fall[H_NAME];
    char night[H_NAME + 16]; /* "the Night of the Open Gate", "the night of the Long Bell" */
    char era_name[ERA_COUNT][H_NAME];
    char places[8][H_NAME];
    int nplaces;
    HActor actor[H_ACTORS];
    int nactors;
    HFaction faction[H_FACTIONS];
    int nfactions;
    HArtifact art[H_ARTS];
    int narts;
    HEvent ev[H_EVENTS];
    int nevents;
    /* the mystery */
    u8 shape, shape2, shape3;
    int score[SH_COUNT];
    u8 opener;       /* actor, H_NONE: it came up by itself */
    u8 scapegoat;    /* actor blamed, H_NONE */
    u8 cover;        /* faction that tells the lie */
    u8 ev_open, ev_blame, ev_punish, ev_burn, ev_founding, ev_secret, ev_why, ev_deep, ev_hoard, ev_account;
    u8 keyholder;    /* whose key opened it */
    u8 burner, orderer; /* who burned the village, on whose word */
    u8 witness;      /* who wrote the true account */
    u8 player_kin;   /* actor who is the player's kin, H_NONE */
    int chain;       /* causal steps from the deepest cause to the fall */
    u8 cast[H_CAST]; /* chapter -> actor */
    u8 cast_legend[H_CAST]; /* chapter -> the world legendary its target carries */
    u8 arc_faction[3];
    u8 culprit_role; /* RO_* of the opener */
    /* facts and clues */
    HFact fact[H_FACTS];
    int nfacts;
    HClue clue[H_CLUES];
    int nclues;
    u8 reveal[H_CAST];  /* chapter -> clue (SRC_BODY or SRC_DYING) */
    u8 page[H_CAST];    /* chapter -> clue (SRC_PAGE), the truth motive's pages */
    u8 orders[H_CAST];  /* chapter -> clue (SRC_ORDERS), carried by hunters */
    HQuestion q[H_QUESTIONS];
    int nq;
    char problems[256]; /* validator: empty when sound */
} History;

extern const char *SHAPE_NAMES[SH_COUNT];
extern const char *TONE_NAMES[TONE_COUNT];
extern const char *BELOW_NAMES[BL_COUNT];
extern const char *EV_NAMES[EV_COUNT];
extern const char *ROLE_NAMES[RO_COUNT];
extern const char *MOTIVE_V_NAMES[MV_COUNT];
extern const char *QUESTION_NAMES[Q_COUNT];

/* The history of a world (cached; the pointer stays valid until four other seeds are asked). */
const History *history_of(u64 seed);
/* Generates into out without the cache (tools). */
void history_generate(u64 seed, History *out);
/* Checks a history: every key fact has its clues, every claim a crack, no one acts dead...
 * Writes the problems into h->problems; true if there are none. */
bool history_validate(History *h);

/* The arc's culture (story.c decides it from the seed: the arc's direction). */
int story_arc_culture(u64 seed, int arc);

/* ---------- text (history_text.c) ---------- */
/* Context for a player: their village and kin ("your sister"). NULL: generic. */
typedef struct HPersonal { const char *village, *kin; } HPersonal;

/* A clue as the player reads it. */
void history_clue_text(const History *h, int clue, const HPersonal *p, char *out, int n);
/* The same, worded one of several ways (a trace's key picks the voice). */
void history_clue_text_v(const History *h, int clue, const HPersonal *p, u64 variant, char *out, int n);
/* What chapter c's target reveals when it falls; past the mystery, what no one else told. */
void history_reveal(const History *h, int chapter, const HPersonal *p, const char *next, char *out, int n);
/* The clue a revelation carries (-1 past the mystery and its leftovers). */
int history_reveal_clue(const History *h, int chapter);
/* A fact in the STORY tab's neutral voice, with where it came from. */
void history_fact_line(const History *h, int fact, const HPersonal *p, char *out, int n);
/* The motive texts of chapter c (< H_CAST): the hook, and the line when its target falls. */
void history_hook(const History *h, int motive, int chapter, const HPersonal *p, const char *next_power, char *out, int n);
void history_done(const History *h, int motive, int chapter, const HPersonal *p, const char *next_power, char *out, int n);
/* A cast actor's title ("captain of the Grey Wardens, who held the bar"). */
void history_title(const History *h, int chapter, char *out, int n);
/* The lore of a world legendary that belongs to the history (false: not one of them). */
bool history_legend_lore(const History *h, int legend, char *out, int n);
/* A trace's text: false if the trace carries no clue (then the old atmosphere text is used). */
bool history_trace_clue(const History *h, u64 trace_key, int kind, int dist, u8 domain_culture, int *clue);
/* Questions: the text, and the answer if answered. */
void history_question(const History *h, int q, const HPersonal *p, char *out, int n);
void history_answer(const History *h, int q, const HPersonal *p, char *out, int n);
/* The fact a clue carries. */
static inline int history_clue_fact(const History *h, int clue) { return clue >= 0 && clue < h->nclues ? h->clue[clue].fact : -1; }
/* A readable account of the whole history (worldmap). */
void history_dump(const History *h, FILE *f);

#endif
