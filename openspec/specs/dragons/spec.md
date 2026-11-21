# dragons Specification

## Purpose
Dragons from swarming dragonets to ancient wyrms: lairs, hoards, three-phase fights and the loot of their bodies.

## Requirements

### Requirement: Kinds and sizes
There SHALL be 25 kinds of dragon in five sizes: dragonets (swarms), drakes (packs), wyverns (diving), great dragons and ancient wyrms (alone in lairs).

#### Scenario: A lair's dragon
- **WHEN** a lair is generated
- **THEN** a great dragon or an ancient wyrm of one kind sleeps on its hoard

### Requirement: Waking
A great dragon SHALL wake when a player comes within 12 m standing or 5 m crouched, steps on its gold (with a chance even crouched), or strikes it; it SHALL roar, shake the view and wake the monsters near.

#### Scenario: Sneaking
- **WHEN** a player crouches past a sleeping dragon at 8 m
- **THEN** it stays asleep

### Requirement: Three phases
A great dragon SHALL fight in the air, then grounded and furious below two thirds of its life, then desperate below one third while its lair comes down; wounds to its wings SHALL tear them so it cannot fly again; it SHALL not chase a player out of its lair, and SHALL heal and sleep again when left.

#### Scenario: Furious
- **WHEN** a flying dragon falls below two thirds of its life
- **THEN** it lands and fights faster

### Requirement: Breath
A dragon's breath SHALL act through the world simulation: fire burns, frost freezes, acid eats, and so on by element.

#### Scenario: Fire breath
- **WHEN** a fire dragon breathes on wood
- **THEN** the wood catches fire

### Requirement: Loot
A slain great dragon SHALL leave gold, rich drops, a titan heart, essences of its element (its scales), teeth and its blood; its hoard SHALL hold gold, chests, one of the world's legendaries and a dragon-slayer's blade; an ancient leaves more.

#### Scenario: Slain
- **WHEN** a great dragon dies
- **THEN** its heart, scales, teeth and blood lie where it fell

### Requirement: Dragons stay dead
A slain great dragon SHALL stay dead (its lair is saved), its skull SHALL hang in the Sanctum's armory and the world SHALL call the player dragonslayer.

#### Scenario: Back at the lair
- **WHEN** a player returns to the lair of a slain dragon
- **THEN** no dragon is there
