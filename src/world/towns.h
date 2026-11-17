#ifndef FC_TOWNS_H
#define FC_TOWNS_H

/* Settlements of the friendly peoples (docs/TOWNS.md): halls in the fortress where dwarves,
 * goblins, gnomes, elves, humans, halflings or orcs live. A town is a great hall of three by
 * three chunks (a hall structure, variant HALL_TOWN) with a plaza and a well, an inn, a temple,
 * the elder's hall, a workshop, houses and a market. This file is the data both sides read:
 * the generator (gen_town.c) and the game (town.c). */

#include "core/common.h"

enum { PE_DWARF, PE_GOBLIN, PE_GNOME, PE_ELF, PE_HUMAN, PE_HALFLING, PE_ORC, PEOPLE_COUNT };

/* What a building is for. */
enum { TB_NONE, TB_PLAZA, TB_INN, TB_TEMPLE, TB_ELDER, TB_WORKSHOP, TB_HOUSE, TB_MARKET, TB_GARDEN };

/* Who someone is in a town. */
enum { FR_INNKEEPER, FR_HEALER, FR_ELDER, FR_SMITH, FR_MERCHANT, FR_GUARD, FR_FOLK, FR_CHILD, FR_LOST, FR_COUNT };

/* What a market sells. */
enum { WA_FOOD, WA_REAGENT, WA_FORGE, WA_SUPPLY, WA_WEAPON, WA_POTION, WA_FORMULA, WA_KEYSTONE };
typedef struct Ware { u8 kind; i16 id; u8 n; i16 price; } Ware;

typedef struct PeopleDef {
    const char *name, *plural, *adj;  /* "dwarf", "dwarves", "dwarven" */
    const char *place;                /* "Hall" */
    const char *pre[8], *suf[8];      /* place names */
    const char *first[8], *last[8];   /* people's names */
    float height, width;              /* 1: a human */
    u8 skin[3][3], hair[3][3], cloth[3][3];
    u8 ears;                          /* 0 round, 1 long and pointed, 2 big, 3 small and pointed */
    u8 beard;                         /* chance in eighths */
    u8 hat;                           /* 0 none, 1 pointed hat, 2 hood, 3 helmet, 4 circlet, 5 bandana, 6 straw hat */
    bool tusks;
    u8 build;                         /* house style: 0 stone, 1 shanty, 2 brass, 3 wood and leaves, 4 timber, 5 burrow, 6 bone and iron */
    Ware wares[5];
    i16 need[3];                      /* what they ask to be brought: FD_* or 1000 + RG_* */
    const char *chatter[6];           /* what folk say; %F, %B, %S, %N from the history, %T the town */
    const char *greet;                /* the elder's greeting */
} PeopleDef;
/* What a people sells only to those it trusts: a friend (reputation 3), the honoured (6), and what
 * comes with the caravan you paid for (town.c). One of each a day. WA_WEAPON id 1: an epic. */
extern const Ware PEOPLE_SPECIAL[][3];
extern const PeopleDef PEOPLES[PEOPLE_COUNT];

/* A town as built: from its structure (world_structure_at, variant HALL_TOWN). */
#define HALL_TOWN 3
#define TOWN_LOTS 9
typedef struct TownLot {
    u8 role;                  /* TB_* */
    int x0, z0, x1, z1;       /* the building's footprint (inclusive) */
    int door_x, door_z;       /* the cell of its door */
    int spot_x, spot_z;       /* where its keeper stands */
} TownLot;
typedef struct Town {
    u64 key;                  /* the structure's seed: who the town is */
    int people;
    char name[48];            /* "the Dwarven Hall of Khazdum" */
    int x0, z0, x1, z1;       /* the hall's floor (inclusive) */
    int floor;                /* feet height */
    int px, pz;               /* the plaza's centre (the well) */
    int cx0, cy, cz0;         /* the hall's first chunk */
    TownLot lot[TOWN_LOTS];
    int stall_x[6], stall_z[6], nstalls;
} Town;

struct World;
struct Structure;
int town_people(int stratum, u64 seed); /* who lives in a town of this stratum */
bool town_from_structure(const struct Structure *s, Town *out);
bool world_town_at(const struct World *w, int cx, int cy, int cz, Town *out);
/* A name for someone of a people, from a key. */
void people_name(int people, u64 key, char *out, int n);

#endif
