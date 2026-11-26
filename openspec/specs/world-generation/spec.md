# world-generation Specification

## Purpose
The fortress itself: seeds, chunks, layers and stairs, strata above and below, regional styles, great structures.

## Requirements

### Requirement: Seeds
A world SHALL be a pure function of its seed, typed as a number or any word; the same seed SHALL always make the same fortress, and only what players changed SHALL be saved.

#### Scenario: Sharing a seed
- **WHEN** two players start the seed "dragon" on different machines
- **THEN** they find the same halls, towns and Old King

### Requirement: Without end
Behind the gate the fortress SHALL be generated around each player in chunks of 32 m, without end in every direction including up and down.

#### Scenario: Walking on
- **WHEN** a player walks 2 km west
- **THEN** chunks keep appearing ahead and there is no edge

### Requirement: The gate
Every world SHALL begin before a castle built into a mountain; its gate SHALL lead into the Grand Vestibule.

#### Scenario: A new world
- **WHEN** a new world starts
- **THEN** the player stands outside before the gate

### Requirement: Layers and stairs
The fortress SHALL be built in layers, one walkable floor each; every chunk SHALL have one spiral staircase up and one down (a great hall or cavern one of each for the whole), and the rare chasm SHALL join two layers with a long stair.

#### Scenario: Going down
- **WHEN** a player wants the layer below
- **THEN** a staircase down is within the chunk they stand in

### Requirement: Strata
The layer type SHALL change with height: above, castle, spires, eagle aeries, cloud cloisters, hanging gardens, heaven, sun court, stormcrown, celestial orrery and the astral void; below, mines, roots, ossuary, fungal deep, limbo, crystal abyss, inferno, iron hells, the maw and the world core.

#### Scenario: Climbing
- **WHEN** a player climbs far above the gate layer
- **THEN** the halls turn from castle to spires and on

### Requirement: Regional styles
Every 10 chunks (320 m) in each direction the style SHALL change, ten styles per direction (north from Frostbitten to the Lifeless cold, west from Gothic to the Eldritch, ...), combined with the stratum.

#### Scenario: North
- **WHEN** a player goes 30 chunks north
- **THEN** the dungeon looks frozen and is named for its style

### Requirement: Structures
The generator SHALL place great halls and caverns, chasms, towns, lairs, vaults (about one chunk in seven), shrines, chests (about a third of the chunks) and the Old King's throne hall, each reachable on foot from the gate.

#### Scenario: Reachable
- **WHEN** test_worldgen checks a seed
- **THEN** every structure near the gate can be reached on foot
