#ifndef FC_TELE_H
#define FC_TELE_H

/* Playtest telemetry (bot.c reads it): where damage comes from, what was killed and found.
 * The game only adds to these counters; nothing reads them unless --bot runs. */

enum { HS_MELEE, HS_PROJ, HS_BLAST, HS_TRAP, HS_LAVA, HS_FALL, HS_DOT, HS_OTHER, HS_COUNT };

typedef struct Tele {
    int src, src_mon, src_grade, src_rank;   /* set before game_hurt_player */
    float taken[HS_COUNT];
    float taken_mon[32];
    int last_src, last_mon, last_grade, last_rank; /* what hurt you last (the cause of a death) */
    float dealt;
    int kills[32][3];
    int potions_hp, potions_mp;
    int chests, vaults, scrolls, relics, traces, weapons[5];
    int gold;
    int casts_attack, casts_utility;
    int blocked_knight;
    int lost_found, uniques_found, powers_fired, awakened, banes;
    int food, cooked, eaten, hunted, harvested;
    int reagents, brewed, murky, elixirs, formulae;
} Tele;

extern Tele g_tele;

static inline void tele_src(int src, int mon, int grade, int rank) {
    g_tele.src = src; g_tele.src_mon = mon; g_tele.src_grade = grade; g_tele.src_rank = rank;
}

#endif
