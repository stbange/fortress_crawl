# encounters Specification

## Purpose
How fights are built: their length, who comes in a pack, how they arrive, heavy blows and where bosses wait.

## Requirements

### Requirement: Fight length
Monsters in tiers 0-2 SHALL have 40% more health than before, so that a fight lasts several seconds.

#### Scenario: A crawler near the gate
- **WHEN** a crawler spawns in tier 0
- **THEN** its health is 1.4 times its old value

### Requirement: Heavy blows
Elites and bosses of the melee kinds without an area attack of their own SHALL, about one attack in four, wind up a heavy blow for 0.9 s with a glowing ring on the ground (2.5 m, 3.5 m for a boss) that brightens as it comes; when it lands it SHALL hit everyone inside the ring for 1.5 times its damage.

#### Scenario: Stepping out
- **WHEN** a player leaves the ring before an elite's heavy blow lands
- **THEN** they take no damage

### Requirement: Shooters in packs
From danger level 1, seven melee packs in ten SHALL bring a shooter or a healer (a skeleton, a wisp or a shaman as their danger allows); fire SHALL fly at 13 m/s and enemy shots SHALL leave a short trail.

#### Scenario: A pack
- **WHEN** a pack of hounds spawns at level 3
- **THEN** usually one of them is a skeleton, a wisp or a shaman

### Requirement: Waves
When a pack wakes, members beyond the first two SHALL wait 3-6 s before coming, unless struck, so that fights arrive in waves.

#### Scenario: Two waves
- **WHEN** a pack of five wakes
- **THEN** two come at once and the rest within six seconds

### Requirement: Bosses near the gate
No boss SHALL hold a hall in tier 0 during the first 20 minutes of play.

#### Scenario: Early on
- **WHEN** a great hall in tier 0 spawns its monsters ten minutes into the game
- **THEN** none of them is a boss
