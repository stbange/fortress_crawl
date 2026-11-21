## ADDED Requirements

### Requirement: Danger warning
Entering a place whose danger level is 3 or more above the player's level SHALL show a warning banner naming the level, at most once a minute.

#### Scenario: Too deep
- **WHEN** a level-4 player walks into a chunk of danger level 8
- **THEN** a banner warns that this place is above them

### Requirement: Potion hint
When a player's health falls below 30% in a fight and they carry a health potion, a banner SHALL remind them how to drink one, at most once in 45 s.

#### Scenario: Low health
- **WHEN** a player with two potions drops to a quarter of their health
- **THEN** a banner says to drink a potion
