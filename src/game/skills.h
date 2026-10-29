#ifndef FC_SKILLS_H
#define FC_SKILLS_H

#include "core/common.h"

/* Classless skill tree: four branches of four tiers. One point per level.
 * A tier needs at least one rank in the tier above it in the same branch.
 * Tier 4 of the three weapon branches unlocks that weapon's alternate attack. */

enum { BR_BLADE, BR_BOW, BR_ARCANE, BR_SURVIVAL, BRANCH_COUNT };

enum {
    SK_KEEN_EDGE, SK_CLEAVE, SK_BLOODTHIRST, SK_WHIRLWIND,
    SK_STEADY_AIM, SK_MULTISHOT, SK_PIERCING, SK_ARROW_STORM,
    SK_FOCUS, SK_FIREBALL, SK_CHAIN, SK_NOVA,
    SK_TOUGHNESS, SK_FLEET_FOOT, SK_SECOND_WIND, SK_LAST_STAND,
    SKILL_COUNT
};

typedef struct SkillDef {
    const char *name;
    const char *desc;
    int branch, tier, max_rank;
} SkillDef;

extern const SkillDef SKILLS[SKILL_COUNT];
extern const char *BRANCH_NAMES[BRANCH_COUNT];

static inline int skill_id(int branch, int tier) { return branch * 4 + tier; }
bool skill_can_buy(const u8 *ranks, int id, int points);

#endif
