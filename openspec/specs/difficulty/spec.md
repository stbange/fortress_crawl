# difficulty Specification

## Purpose
How danger rises with distance from the gate, the danger floor, regional elements and co-op scaling.

## Requirements

### Requirement: Danger by distance
Danger SHALL rise with distance from the gate in steps of 10 chunks: every step monsters are a little tougher and bigger and elites more common; from step 4 they wear armour, from step 6 they are faster, from step 8 they mend their wounds.

#### Scenario: Farther out
- **WHEN** a player crosses from step 3 into step 4
- **THEN** the monsters there are armoured

### Requirement: Danger floor
Monsters SHALL never be weaker than about 60% of the player's level, wherever the player is.

#### Scenario: Near the gate at level 20
- **WHEN** a level-20 player fights near the gate
- **THEN** the monsters are at least level 12

### Requirement: Regional elements
From the second step out, monsters SHALL carry their region's element: frost north (chill and slow), fire south (set alight), lightning east (a jolt), poison west, holy above (a smite), shadow below (drain mana and life); they SHALL take half damage from their own element and half again from its opposite (fire and frost, lightning and earth, poison and arcane, holy and shadow).

#### Scenario: Fire against frost
- **WHEN** a player's fire bolt hits a frost monster
- **THEN** it deals half again its damage

### Requirement: More players
Monsters SHALL have 50% more health for each extra player in the world.

#### Scenario: Two players
- **WHEN** a monster is born with two players in the world
- **THEN** it has one and a half times the health it would have alone
