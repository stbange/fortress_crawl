# guidance Specification

## Purpose
What tells the player where to go: the line at the top of the screen, the Golden Thread, waypoints and the hints of the way.

## Requirements

### Requirement: One line at the top
The top of the screen SHALL show one line: the Old King's next step by default; Y SHALL move it to the chronicle or a town's task and back; when more than one could show, a hint says Y gives another.

#### Scenario: Switching
- **WHEN** a player presses Y with a chronicle and the king's way open
- **THEN** the line follows the chronicle and a banner says so

### Requirement: Hints of the way
The first time the line follows a step of the king's way, a message SHALL say how that step is done (the Sanctum Door, the STORY tab, crouching past a sleeping dragon, the forge, the Gate, a town's works).

#### Scenario: The dragon step
- **WHEN** the line first follows a dragon step
- **THEN** a message says to crouch to come close unheard

### Requirement: The Golden Thread's goals
The Golden Thread SHALL lead first to the player's waypoint if one is set, then to the Old King's step the line follows (unless the line follows the chronicle), then to the chronicle's target and the missions; casting again within 8 s SHALL follow the next goal.

#### Scenario: The king's step
- **WHEN** a player casts the thread with the line on the king's way
- **THEN** it leads toward that step's place

### Requirement: Where a step is
Each step with a place SHALL lead to it: the one to talk to, its marker, the lair, the lieutenant, the king; the realm step to the Grand Vestibule's rift (or the realm's rift if loaded), to the Gate at home if it shows that realm, and in the realm to its ruler or keep. Markers SHALL lie on the walkable floor of their layer where one is found.

#### Scenario: The realm step
- **WHEN** the realm step is followed from the fortress
- **THEN** the thread leads to the Grand Vestibule's rift
