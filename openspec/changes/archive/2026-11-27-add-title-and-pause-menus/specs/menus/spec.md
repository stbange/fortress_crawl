## MODIFIED Requirements

### Requirement: Pause
Esc, START on a gamepad or the II button on touch SHALL open a pause menu with RESUME, SETTINGS, HELP, SAVE AND QUIT TO TITLE and, on the desktop, SAVE AND QUIT GAME; it and every other menu SHALL pause the game when one plays alone and SHALL not in co-op, where the pause menu is that player's own.

#### Scenario: Alone
- **WHEN** a solo player presses Esc during a fight
- **THEN** the pause menu opens and the monsters stand still until RESUME

#### Scenario: Co-op
- **WHEN** one of two players opens the pause menu
- **THEN** it says the world goes on, and the other player keeps playing

#### Scenario: To the title
- **WHEN** a player chooses SAVE AND QUIT TO TITLE
- **THEN** the world is saved, the start screen shows, and CONTINUE brings them back where they were
