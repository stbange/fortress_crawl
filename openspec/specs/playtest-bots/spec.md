# playtest-bots Specification

## Purpose
The bots that play the game to measure it, their personas, and the telemetry they log.

## Requirements

### Requirement: Bot styles
--bot STYLE SHALL play player 1 for --bot-minutes and log to --bot-log; styles explorer, delver, archer, caster, quester, cautious, king, novice (random valid inputs at 2-4 Hz) and spam (the primary attack only).

#### Scenario: The novice
- **WHEN** the game runs with --bot novice
- **THEN** the bot presses random moves, turns, jumps, attacks and spells 2 to 4 times a second

### Requirement: The king bot
The king bot SHALL go to the nearest step it can do now (following the Golden Thread with its own waypoint), go home when the vision is all it can do, build the island and the forge first and buy steel when the forge step is open, buy an orchid where sold, pay for a town's works with 500 gold or more, hunt the nearest living great dragon within its level + 2 when the way needs a dragon's heart or blood, and for the realm step take the Vestibule rift or the Gate, fight the ruler and leave by the arch (put through it, and counted, if it cannot find it in a minute).

#### Scenario: A dragon needed
- **WHEN** the forge step is open and the bot holds no titan heart
- **THEN** it logs a dragon hunt and goes to the nearest lair it can face

### Requirement: Reaction delay
--bot-react MS SHALL make any bot see and act only every MS milliseconds (with +-35 ms jitter), holding its inputs in between.

#### Scenario: A human's reaction
- **WHEN** the game runs with --bot explorer --bot-react 200
- **THEN** the bot decides about five times a second

### Requirement: Fun telemetry
At the end of a run the bot SHALL log a FUN line: the tension index (threats within 15 m, the hands' business and health missing, once a second) as mean, sd, CV and longest run above 0.9; enemy shots that passed within 1-1.5 times the box they hit; attack presses held one reaction window that never fired, waiting out the input buffer before counting one as lost, and of them the presses made within 100 ms of the weapon being ready; the use of blade, bow, staff, alternate attack, attack spell, utility spell, throw and elixir; and the number of weapon swaps.

#### Scenario: The FUN line
- **WHEN** a bot run ends
- **THEN** its log ends with SUMMARY, FUN and KING lines

#### Scenario: A press the buffer saves
- **WHEN** the human-proxy bot lets go of attack 60 ms before the weapon is ready
- **THEN** the attack fires from the buffer and the press is not counted as lost

### Requirement: Timing
With FC_TIMING set, the game SHALL log the time to its first frame, to the first frame with the world round the player ready, and ten seconds of mean and worst frame times.

#### Scenario: Time to play
- **WHEN** the game starts with FC_TIMING=1 and a seed
- **THEN** it logs TIMING lines for the first frame, playable, and the frame times

### Requirement: Bot survival
Bots SHALL heal (the spell, then a potion) below half their health, and the cautious style below 55%.

#### Scenario: Hurt
- **WHEN** an explorer bot falls to 45% of its health with a potion
- **THEN** it heals or drinks

### Requirement: Bot weapons
Bots other than spam SHALL take up the bow against fliers and shooters more than 4 m away, the staff (or a blunt blade, or the blade when low on mana) against armour, and otherwise their style's weapon, keeping a choice for 3 s; they SHALL use the alternate attack into two or more foes within 3.5 m (blade, staff) and at elites and bosses more than 6 m away (bow).

#### Scenario: Wisps
- **WHEN** an explorer bot fights a wisp 10 m away
- **THEN** it switches to the bow
