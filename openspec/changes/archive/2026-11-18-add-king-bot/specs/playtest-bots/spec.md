## ADDED Requirements

### Requirement: Bot styles
--bot STYLE SHALL play player 1 for --bot-minutes and log to --bot-log; styles explorer, delver, archer, caster, quester, cautious and king.

#### Scenario: A run
- **WHEN** the game runs with --bot king --bot-minutes 60
- **THEN** it plays an hour and writes SUMMARY and KING lines at the end

### Requirement: The king bot
The king bot SHALL go to the nearest step it can do now (following the Golden Thread with its own waypoint), go home when the vision is all it can do, build the island and the forge first and buy steel when the forge step is open, buy an orchid where sold, pay for a town's works with 500 gold or more, hunt the nearest living great dragon within its level + 2 when the way needs a dragon's heart or blood, and for the realm step take the Vestibule rift or the Gate, fight the ruler and leave by the arch (put through it, and counted, if it cannot find it in a minute).

#### Scenario: A dragon needed
- **WHEN** the forge step is open and the bot holds no titan heart
- **THEN** it logs a dragon hunt and goes to the nearest lair it can face
