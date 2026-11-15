#ifndef FC_ECO_H
#define FC_ECO_H

#include "core/common.h"

/* Populations and the food chain (docs/SIMULATION.md): each region (chunk) keeps numbers, not
 * animals. Plants and flowers are counted in the world; grazers live off the plants and are
 * eaten by predators (hounds, bats); insects follow flowers and carrion; carrion is what died
 * and rots away. Only the animals near the players are real, and they are made from these
 * numbers. Time here is "ecological hours" (the game runs them faster than play time). */
typedef struct Eco {
    float plants, flowers;   /* cells counted in the chunk */
    float grazers, predators, insects, carrion;
    bool init;
} Eco;

/* How many grazers the plants feed. */
static inline float eco_capacity(const Eco *e) { return 0.5f + e->plants / 25.0f; }

/* Moves the numbers on by `hours`; returns how many plant cells the grazers ate beyond what
 * grows back (the caller removes them from the world). */
float eco_step(Eco *e, float hours);
/* Starting numbers for a region seen for the first time. */
void eco_start(Eco *e, float plants, float flowers, bool predators_live_here);
/* Neighbouring regions even out a little (animals wander to where there is more to eat). */
void eco_wander(Eco *a, Eco *b, float hours);

#endif
