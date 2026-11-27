## ADDED Requirements

### Requirement: Worlds list
The start screen SHALL list every world saved on this device with its seed, level and time played; choosing one SHALL load it, and deleting one SHALL ask first.

#### Scenario: Two worlds
- **WHEN** a player has played seeds 42 and DRAGON
- **THEN** WORLDS (2) lists both, and Enter on DRAGON loads it

### Requirement: Confirmations
Starting a saved world again with new characters, deleting a world and quitting from the start screen SHALL each ask in a dialog naming what is lost, with the safe answer chosen first; Esc or B SHALL answer no.

#### Scenario: Delete
- **WHEN** a player presses Delete on a world in the list
- **THEN** a dialog names the world's level, gold and kills, KEEP IT is chosen, and only DELETE removes the save

### Requirement: Credits
The start screen SHALL have CREDITS naming what the game is built with and its licences (SDL 3, zlib; Emscripten, MIT and UIUC/NCSA) and where the sounds and music come from.

#### Scenario: Licences
- **WHEN** a player opens CREDITS
- **THEN** SDL's zlib licence is named

### Requirement: Version
The start screen and the pause menu SHALL show the build (the commit the game was built from, or "dev").

#### Scenario: A bug report
- **WHEN** a player opens the pause menu
- **THEN** the world's seed and the build are shown at its top

### Requirement: Auto-pause
Playing alone, the game SHALL pause (opening the pause menu) when the window loses the focus, the browser tab is hidden, or the browser's pointer lock ends; it SHALL not in co-op.

#### Scenario: Alt-tab
- **WHEN** a solo player switches to another window mid-fight
- **THEN** the pause menu is open and the monsters stand still when they come back

## MODIFIED Requirements

### Requirement: Start screen
The game SHALL start on a start screen with CONTINUE (the world played last, showing its level, gold, kills and time played), NEW WORLD, WORLDS, SETTINGS, HELP, CREDITS and, on the desktop, QUIT; NEW WORLD SHALL type a seed (a number or any word) or roll a random one and show what is saved for it; --seed SHALL skip the start screen.

#### Scenario: Last world
- **WHEN** a player who played seed 42 starts the game
- **THEN** the start screen offers CONTINUE: 42 with its save's level and time

#### Scenario: Typing
- **WHEN** a player types a letter on the first screen
- **THEN** the NEW WORLD screen opens with the letter as the seed
