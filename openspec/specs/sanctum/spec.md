# sanctum Specification

## Purpose
The player's home on a floating island: its door, plots and stages, bath, table, bed, library, armory and stash.

## Requirements

### Requirement: The door
The Sanctum Door spell SHALL open a door home after a 3-second ritual in a circle of light (the caster moves slowly inside it); any hit SHALL break it, the mana spent and 5 s to wait; the door in the court SHALL take the player back to where they opened it.

#### Scenario: Interrupted
- **WHEN** a monster hits a player during the ritual
- **THEN** the ritual breaks and the mana is gone

### Requirement: One home per world
Each world SHALL have one Sanctum on a floating island far above, shared by everyone playing it.

#### Scenario: Co-op
- **WHEN** two players open the door in one world
- **THEN** they arrive at the same home

### Requirement: Building
The island SHALL be a grid of 4 x 4 plots of 32 m; the island, hall, court, bath, dining hall, library, garden, armory and forge SHALL each be built up in up to three stages for gold and keystones, and appear at once; the island's second stage SHALL make room for the library, garden, armory and forge, its third add towers, an outer wall and waterfalls.

#### Scenario: Building the bath
- **WHEN** a player pays for the bath's first stage
- **THEN** the bath stands in its plot

### Requirement: Keystones
Keystones SHALL come from every vault hoard, every named enemy from danger level 6, now and then a boss, realm rulers and the shop (one a day, 1500 gold and 500 more each time).

#### Scenario: A hoard
- **WHEN** a player opens a vault
- **THEN** its hoard holds a keystone

### Requirement: Bath, table and bed
Bathing SHALL close wounds, wash off burns and poison and give Refreshed (mana twice as fast, 10 to 25 minutes by stage); the table SHALL restore everything and give Well Fed (10% more health that comes back by itself, 10 to 30 minutes); resting in the bed SHALL set where one wakes and save the game.

#### Scenario: The bed
- **WHEN** a player rests in the bed and later falls in the fortress
- **THEN** they wake in the Sanctum

### Requirement: Library, armory and stash
The library SHALL give a free scroll every 20 minutes (12 with the dome); the armory SHALL show the legendaries found, the banners of named enemies killed, statues for chapters finished and the skulls of dragons slain; the stash SHALL keep 30 weapons.

#### Scenario: A legendary
- **WHEN** a player finds a legendary
- **THEN** it hangs on a rack in the armory
