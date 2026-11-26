# saves Specification

## Purpose
What a save keeps and how it is read back.

## Requirements

### Requirement: Whole reloads
Loading a save SHALL restore everything it holds: every character, the chests and relics taken, the lore, the explored map, the Sanctum and the changed blocks.

#### Scenario: A reload
- **WHEN** a player who dug, looted and explored saves and loads again
- **THEN** the map, the dug blocks and the emptied chests are as they were

### Requirement: Weapon looks
Saves SHALL keep each weapon's look (from version 30).

#### Scenario: Reloaded
- **WHEN** a player reloads
- **THEN** every weapon looks as it did
