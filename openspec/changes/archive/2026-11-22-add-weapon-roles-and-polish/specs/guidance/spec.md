## MODIFIED Requirements

### Requirement: One line at the top
The top of the screen SHALL show one line: the Old King's next step by default; Y SHALL move it to the chronicle or a town's task and back. The line SHALL dim to a third once its text has been unchanged for 20 s and brighten again when it changes or on Y or G. When more than one could show, a hint SHALL say Y gives another, in the first five minutes of play and for 10 s after another line opens.

#### Scenario: Switching
- **WHEN** a player presses Y with a chronicle and the king's way open
- **THEN** the line follows the chronicle and a banner says so

#### Scenario: Read
- **WHEN** the line has shown the same step for half a minute
- **THEN** it is dim, and a press of G brightens it
