## ADDED Requirements

### Requirement: Experience
Kills SHALL give experience: the killer all of it and every other player within 30 m half.

#### Scenario: In co-op
- **WHEN** one player kills a brute with another 10 m away
- **THEN** both gain experience, the killer twice as much

### Requirement: Levels
Each level SHALL raise maximum health by 12 (from 100) and mana by 6 (from 40), damage by 7%, and give one skill point.

#### Scenario: Level 2
- **WHEN** a character reaches level 2
- **THEN** their health maximum is 112 and they have a skill point

### Requirement: Skill tree
The skill tree SHALL be classless: four branches (blade, bow, arcane, survival) of four tiers, a tier open once the tier above it has a rank: Keen Edge, Cleave, Bloodthirst, Whirlwind; Steady Aim, Multishot, Piercing, Arrow Storm; Focus, Fireball, Chain Lightning, Nova; Toughness, Fleet Foot, Second Wind, Last Stand.

#### Scenario: A locked tier
- **WHEN** a player without Keen Edge tries to learn Cleave
- **THEN** it cannot be learned yet
