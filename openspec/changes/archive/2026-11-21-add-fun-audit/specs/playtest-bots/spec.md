## ADDED Requirements

### Requirement: Reaction delay
--bot-react MS SHALL make any bot see and act only every MS milliseconds (with +-35 ms jitter), holding its inputs in between.

#### Scenario: A human's reaction
- **WHEN** the game runs with --bot explorer --bot-react 200
- **THEN** the bot decides about five times a second

### Requirement: Fun telemetry
At the end of a run the bot SHALL log a FUN line: the tension index (threats within 15 m, the hands' business and health missing, once a second) as mean, sd, CV and longest run above 0.9; enemy shots that passed within 1-1.5 times the box they hit; attack presses held one reaction window that never fired; the use of blade, bow, staff, alternate attack, attack spell, utility spell, throw and elixir.

#### Scenario: The FUN line
- **WHEN** a bot run ends
- **THEN** its log ends with SUMMARY, FUN and KING lines

### Requirement: Timing
With FC_TIMING set, the game SHALL log the time to its first frame, to the first frame with the world round the player ready, and ten seconds of mean and worst frame times.

#### Scenario: Time to play
- **WHEN** the game starts with FC_TIMING=1 and a seed
- **THEN** it logs TIMING lines for the first frame, playable, and the frame times

## MODIFIED Requirements

### Requirement: Bot styles
--bot STYLE SHALL play player 1 for --bot-minutes and log to --bot-log; styles explorer, delver, archer, caster, quester, cautious, king, novice (random valid inputs at 2-4 Hz) and spam (the primary attack only).

#### Scenario: The novice
- **WHEN** the game runs with --bot novice
- **THEN** the bot presses random moves, turns, jumps, attacks and spells 2 to 4 times a second
