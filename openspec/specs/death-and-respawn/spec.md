# death-and-respawn Specification

## Purpose
What happens when a character falls, and the way back to where they fell.

## Requirements

### Requirement: Respawn
A fallen character SHALL wake at the last shrine or bed after 3 s, or after 1 s on a press of attack or jump, with a fifth of their gold lost and half their health and mana (in full at home or on a second death within two minutes).

#### Scenario: Back quickly
- **WHEN** a player presses attack 1.2 s after falling
- **THEN** they wake at their shrine at once

### Requirement: Back where you fell
For 15 s after waking, the Golden Thread key SHALL take the character back where they fell for another fifth of their gold, but only once in five minutes.

#### Scenario: Too soon
- **WHEN** a character dies again within five minutes of going back
- **THEN** going back is not offered

### Requirement: What killed you
On death the game SHALL say in one line what killed the character (a monster by name and how, a trap, lava, a fall, burning or poison).

#### Scenario: An arrow
- **WHEN** a skeleton's arrow kills a player
- **THEN** the message says a skeleton's arrow killed them
