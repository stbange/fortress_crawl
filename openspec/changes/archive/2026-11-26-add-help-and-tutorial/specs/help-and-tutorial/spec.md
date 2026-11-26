## ADDED Requirements

### Requirement: Online help
The menu SHALL have a HELP tab with pages on the controls, the basics, fighting, spells, staying alive, the story, the world, making things and loot, written for the device the player uses; F1 or ? SHALL open it.

#### Scenario: At a gamepad
- **WHEN** a player on a gamepad opens the help's controls page
- **THEN** it names the sticks and buttons, not keys

### Requirement: Tutorial
A new character SHALL, once they have chosen why they came, be shown seven steps in turn (move, jump, strike, change weapon, cast, stay alive, the story), each done when the player does it; the last SHALL point them to the line at the top and the Golden Thread, and end with the story beginning.

#### Scenario: Casting
- **WHEN** the card says to cast a flame bolt and the player casts one
- **THEN** the step ticks done and the next one shows

#### Scenario: Into the story
- **WHEN** the player spins the Golden Thread on the last step
- **THEN** the tutorial ends with a banner that their story begins

### Requirement: Skipping
The tutorial SHALL be skippable (Backspace, or holding BACK on a gamepad), SHALL run again from the help's first page, and SHALL never start for bots, demos, screenshot runs or with --no-tutorial.

#### Scenario: Skip
- **WHEN** a player presses Backspace during the tutorial
- **THEN** it ends and a banner says the help has it all
