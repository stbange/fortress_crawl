## ADDED Requirements

### Requirement: What killed you
On death the game SHALL say in one line what killed the character (a monster by name and how, a trap, lava, a fall, burning or poison).

#### Scenario: An arrow
- **WHEN** a skeleton's arrow kills a player
- **THEN** the message says a skeleton's arrow killed them

## MODIFIED Requirements

### Requirement: Respawn
A fallen character SHALL wake at the last shrine or bed after 3 s, or after 1 s on a press of attack or jump, with a fifth of their gold lost and half their health and mana (in full at home or on a second death within two minutes).

#### Scenario: Back quickly
- **WHEN** a player presses attack 1.2 s after falling
- **THEN** they wake at their shrine at once
