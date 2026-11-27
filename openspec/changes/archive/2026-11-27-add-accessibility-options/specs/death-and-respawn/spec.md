## MODIFIED Requirements

### Requirement: Respawn
A fallen character SHALL wake at the last shrine or bed after 3 s, or after 1 s on a press of attack or jump, with a fifth of their gold lost (none with the keep-gold assist) and half their health and mana (in full at home or on a second death within two minutes).

#### Scenario: Back quickly
- **WHEN** a player presses attack 1.2 s after falling
- **THEN** they wake at their shrine at once

#### Scenario: Keep gold
- **WHEN** a player with the keep-gold assist falls with 500 gold
- **THEN** they wake with 500
