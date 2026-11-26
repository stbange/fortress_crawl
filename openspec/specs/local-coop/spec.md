# local-coop Specification

## Purpose
Split-screen play for up to four on one machine.

## Requirements

### Requirement: Split screen
Up to four players SHALL play on one screen: player 1 with keyboard and mouse; START on a gamepad SHALL join a new player with their own character, the screen splitting in two, then in quarters; BACK on a pad that has not joined SHALL control player 1.

#### Scenario: Joining
- **WHEN** a second gamepad presses START
- **THEN** the screen splits and a new character enters

### Requirement: Everyone free
Every player SHALL go where they like; the world SHALL be loaded around each of them.

#### Scenario: Apart
- **WHEN** two players walk in opposite directions
- **THEN** both see their surroundings

### Requirement: Characters kept
Each player's character SHALL be saved with the world; a character whose pad disconnects SHALL wait until a pad presses START again, and characters that did not join this time SHALL be kept.

#### Scenario: A lost pad
- **WHEN** player 2's pad disconnects
- **THEN** their character waits until a pad presses START
