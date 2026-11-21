## ADDED Requirements

### Requirement: One line at the top
The top of the screen SHALL show one line: the Old King's next step by default; Y SHALL move it to the chronicle or a town's task and back; when more than one could show, a hint says Y gives another.

#### Scenario: Switching
- **WHEN** a player presses Y with a chronicle and the king's way open
- **THEN** the line follows the chronicle and a banner says so

### Requirement: Hints of the way
The first time the line follows a step of the king's way, a message SHALL say how that step is done (the Sanctum Door, the STORY tab, crouching past a sleeping dragon, the forge, the Gate, a town's works).

#### Scenario: The dragon step
- **WHEN** the line first follows a dragon step
- **THEN** a message says to crouch to come close unheard
