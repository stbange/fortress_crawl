## ADDED Requirements

### Requirement: Sound effects
Every monster, weapon, spell, trap, mechanism and place SHALL have its own sounds; sounds SHALL be positional (fading with distance, panned for the nearest player); big monsters SHALL sound deeper, elites deeper still; footsteps SHALL follow the floor (stone, dirt, wood, water, ice).

#### Scenario: Behind
- **WHEN** a brute roars to a player's left
- **THEN** it is heard from the left

### Requirement: Ambience
Each stratum and the outside SHALL have an ambience loop, crossfading as the player moves; torches, lava, water and flame vents SHALL be heard near; now and then a distant sound that fits the place SHALL play.

#### Scenario: Into the mines
- **WHEN** a player goes down into the mines
- **THEN** the ambience crossfades to the mines'

### Requirement: Music
Five pieces SHALL play by situation: The Gate on the start screen and outside, The Deep and The Heights now and then while exploring below or above with long silences between, Battle when monsters close in, The Champion when a named enemy is near; they SHALL crossfade; F9 SHALL mute sound and F10 the music.

#### Scenario: A fight
- **WHEN** monsters close in on a player
- **THEN** the battle music fades in
