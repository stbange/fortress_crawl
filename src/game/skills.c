#include "game/skills.h"

const char *BRANCH_NAMES[BRANCH_COUNT] = { "BLADE", "BOW", "ARCANE", "SURVIVAL" };

const SkillDef SKILLS[SKILL_COUNT] = {
    { "Keen Edge", "+30% sword damage per rank", BR_BLADE, 0, 3 },
    { "Cleave", "sword swings hit everything in a wide arc", BR_BLADE, 1, 1 },
    { "Bloodthirst", "each sword hit heals 3 HP per rank", BR_BLADE, 2, 2 },
    { "Whirlwind", "RIGHT CLICK with sword: spinning strike (12 mana)", BR_BLADE, 3, 1 },
    { "Steady Aim", "+30% arrow damage per rank", BR_BOW, 0, 3 },
    { "Multishot", "fire three arrows in a fan", BR_BOW, 1, 1 },
    { "Piercing", "arrows pass through up to 3 foes", BR_BOW, 2, 1 },
    { "Arrow Storm", "RIGHT CLICK with bow: volley of 9 arrows (15 mana)", BR_BOW, 3, 1 },
    { "Focus", "+30% spell damage and -15% mana cost per rank", BR_ARCANE, 0, 3 },
    { "Fireball", "bolts explode and burn nearby foes", BR_ARCANE, 1, 1 },
    { "Chain Lightning", "bolts arc to two more foes", BR_ARCANE, 2, 1 },
    { "Nova", "RIGHT CLICK with staff: ring of fire around you (25 mana)", BR_ARCANE, 3, 1 },
    { "Toughness", "+25 max health per rank", BR_SURVIVAL, 0, 3 },
    { "Fleet Foot", "+12% movement speed per rank", BR_SURVIVAL, 1, 2 },
    { "Second Wind", "faster health regeneration per rank", BR_SURVIVAL, 2, 2 },
    { "Last Stand", "take half damage below 30% health", BR_SURVIVAL, 3, 1 },
};

bool skill_can_buy(const u8 *ranks, int id, int points) {
    if (points <= 0 || ranks[id] >= SKILLS[id].max_rank) return false;
    if (SKILLS[id].tier == 0) return true;
    return ranks[id - 1] > 0;
}
