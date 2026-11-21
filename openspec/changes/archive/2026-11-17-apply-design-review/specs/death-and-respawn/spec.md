## ADDED Requirements

### Requirement: Respawn
A fallen character SHALL wake after 3 s at the last shrine or bed with a fifth of their gold lost, and half their health and mana (in full at home or on a second death within two minutes).

#### Scenario: At the shrine
- **WHEN** a character dies away from home
- **THEN** they wake at their shrine with half their health

### Requirement: Back where you fell
For 15 s after waking, the Golden Thread key SHALL take the character back where they fell for another fifth of their gold, but only once in five minutes.

#### Scenario: Too soon
- **WHEN** a character dies again within five minutes of going back
- **THEN** going back is not offered
