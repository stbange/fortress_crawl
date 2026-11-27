## MODIFIED Requirements

### Requirement: Hitstop
Monsters and their shots SHALL pause for 50 ms when the player kills a monster, 40 ms when the player's blow takes a quarter of a monster's life, and 60 ms when the player loses more than 15% of their health in one hit; the pause SHALL not exceed 80 ms, SHALL not stop the player's own movement, view or shots, and SHALL not happen with the hit pause turned off in the settings.

#### Scenario: A kill
- **WHEN** a player's blow kills a monster
- **THEN** the monsters and their shots stand still for 50 ms

#### Scenario: Turned off
- **WHEN** the hit pause is off and a player kills a monster
- **THEN** nothing stands still

### Requirement: Camera impulse
The view SHALL shake briefly when the player loses more than 10% of their health in one hit (more for bigger hits) and slightly on the player's own kills, and the shake SHALL settle within a second wherever the player is; the eye SHALL dip on landing from a fall faster than 7 m/s; both SHALL scale with the screen shake setting (off, half, full).

#### Scenario: A heavy hit
- **WHEN** a brute takes a fifth of the player's health
- **THEN** the view jolts and settles within half a second

#### Scenario: No dragon near
- **WHEN** a player kills a crawler far from any dragon
- **THEN** the view is still again a moment later
