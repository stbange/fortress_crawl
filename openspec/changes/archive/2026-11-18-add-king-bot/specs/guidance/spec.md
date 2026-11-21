## ADDED Requirements

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
