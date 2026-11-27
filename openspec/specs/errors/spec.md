# errors Specification

## Purpose
What the player is told when something goes wrong, and what is kept for a report.

## Requirements

### Requirement: Error messages
The game SHALL tell the player in words when it cannot run or keep their game: a browser without WebGL 2 (which browsers run it), the game stopping in the browser (with a reload), a desktop without a window or OpenGL 3.3 (a message box), and a save that could not be written (a banner to export the world).

#### Scenario: No WebGL 2
- **WHEN** the page is opened in a browser without WebGL 2
- **THEN** it says the game needs WebGL 2 and names browsers that have it, and the game is not downloaded

#### Scenario: Storage full
- **WHEN** the browser's storage refuses a save
- **THEN** a banner says the world could not be saved and to export it

### Requirement: After a crash
A game that did not end with a quit SHALL be noticed at the next start, which SHALL say so and offer CONTINUE from the last save; on the desktop a crash SHALL write crash.txt beside the saves with the build, the world, the time played and the last 50 lines of the log.

#### Scenario: A crash
- **WHEN** the game crashes in world 1337 and is started again
- **THEN** the start screen says the last game in world 1337 ended unexpectedly, and crash.txt has the last log lines
