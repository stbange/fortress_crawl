## ADDED Requirements

### Requirement: Hitstop
Monsters and their shots SHALL pause for 50 ms when the player kills a monster, 40 ms when the player's blow takes a quarter of a monster's life, and 60 ms when the player loses more than 15% of their health in one hit; the pause SHALL not exceed 80 ms and SHALL not stop the player's own movement, view or shots.

#### Scenario: A kill
- **WHEN** a player's blow kills a monster
- **THEN** the monsters and their shots stand still for 50 ms

### Requirement: Camera impulse
The view SHALL shake briefly when the player loses more than 10% of their health in one hit (more for bigger hits) and slightly on the player's own kills; the eye SHALL dip on landing from a fall faster than 7 m/s.

#### Scenario: A heavy hit
- **WHEN** a brute takes a fifth of the player's health
- **THEN** the view jolts and settles within half a second

### Requirement: Dust
Jumps and hard landings SHALL raise a puff of dust at the player's feet.

#### Scenario: A jump
- **WHEN** a player jumps from the ground
- **THEN** dust rises where they stood

### Requirement: Hit markers
The crosshair SHALL flash white for 0.12 s when the player's attack hits a monster and glow gold for 0.25 s when it kills one.

#### Scenario: A kill
- **WHEN** a player's arrow kills a skeleton
- **THEN** the crosshair glows gold
