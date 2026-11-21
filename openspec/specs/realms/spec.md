# realms Specification

## Purpose
The realms behind the Gate: other worlds joined to each fortress, reached by rifts and the Sanctum's Gate, each with its own rules and a ruler.

## Requirements

### Requirement: Realms joined to a world
Each world SHALL have 6 to 8 of the 21 realms joined to it, chosen by its seed alone.

#### Scenario: Same seed, same realms
- **WHEN** two worlds are made from the same seed
- **THEN** the same realms are joined to both

### Requirement: Rifts
About one fortress chunk in thirty SHALL hold a rift to the joined realm whose danger is closest to the chunk's; the Grand Vestibule SHALL always hold a rift to the gentlest joined realm. Walking into a rift SHALL attune the Gate to its realm and take the player there.

#### Scenario: The first rift
- **WHEN** a player walks into the Grand Vestibule's rift
- **THEN** they arrive at the arch of the gentlest joined realm
- **AND** the Gate in the Sanctum is attuned to it

### Requirement: The Gate
The Gate in the Sanctum's court SHALL take the player to the realm its dial shows, if the Gate is attuned to it; striking the dial SHALL turn it to the next joined realm.

#### Scenario: Not attuned
- **WHEN** a player walks through the Gate set to a realm whose rift they never walked
- **THEN** nothing happens and a banner says to find its rift

### Requirement: The way back
Every realm SHALL have an arch where one arrives; walking through it SHALL return the player where they came from (the rift or the Gate). Falling out of a realm SHALL put the player back at the arch.

#### Scenario: Back through the arch
- **WHEN** a player who came by a rift walks through the arch
- **THEN** they stand by that rift in the fortress

### Requirement: Rulers
Each realm's keep SHALL hold its ruler, a boss of the realm's danger; a beaten ruler SHALL stay beaten and leave two keystones, a reagent from each of the realm's animals and a boss's loot.

#### Scenario: A ruler beaten
- **WHEN** a realm's ruler dies
- **THEN** it is saved as beaten and never spawns again in that world

### Requirement: Realm rules
Each realm SHALL apply its rules while a player is in it: its weight, air (no air outside domes on the Pale Moon), heat (the Sun burns without a sun-ward), storms, tides, spores, rot, mana drain or growth.

#### Scenario: No air
- **WHEN** a player is outside the domes on the Pale Moon
- **THEN** their breath runs out as under water

### Requirement: Sealed realms
The Astral Shore SHALL open only once the Old King is settled; the Dream of the Sleeper SHALL open only after an end that spared (the chronicle's, or a king's by his name or artefact).

#### Scenario: Before the king
- **WHEN** the chronicle has ended but no king is settled
- **THEN** the Astral Shore stays sealed
