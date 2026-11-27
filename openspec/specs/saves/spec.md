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

### Requirement: What is saved
Each world SHALL have its own save with every character that played in it (level, experience, skills, inventory, gold, potions, relics, spells, position and shrine, food, alchemy, smithing), the chests, relics and hoards taken, the vaults opened, the Sanctum, the towns, dragons, realms and the king, what was broken, and the explored map near the players.

#### Scenario: A second session
- **WHEN** a player returns to a world
- **THEN** their character, home and the world's changes are as they left them

### Requirement: When it saves
The game SHALL save every 30 seconds, when a shrine is attuned, at an inn or bed, when saving and quitting (to the start screen or out of the game), when quitting, and in the browser when the tab is hidden or closed.

#### Scenario: Tab closed
- **WHEN** a browser player closes the tab
- **THEN** the world is saved

#### Scenario: Save and quit
- **WHEN** a player chooses SAVE AND QUIT TO TITLE
- **THEN** the world is saved before the start screen shows

### Requirement: Where and which
The browser SHALL keep saves in its local storage and the desktop in the user data folder; saves SHALL be versioned, older ones SHALL always load and newer ones SHALL not; --fresh SHALL start without loading.

#### Scenario: An old save
- **WHEN** a save written two versions ago is loaded
- **THEN** it loads
