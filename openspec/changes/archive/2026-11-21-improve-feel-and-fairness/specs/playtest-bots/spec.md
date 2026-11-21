## MODIFIED Requirements

### Requirement: Fun telemetry
At the end of a run the bot SHALL log a FUN line: the tension index (threats within 15 m, the hands' business and health missing, once a second) as mean, sd, CV and longest run above 0.9; enemy shots that passed within 1-1.5 times the box they hit; attack presses held one reaction window that never fired, waiting out the input buffer before counting one as lost, and of them the presses made within 100 ms of the weapon being ready; the use of blade, bow, staff, alternate attack, attack spell, utility spell, throw and elixir.

#### Scenario: The FUN line
- **WHEN** a bot run ends
- **THEN** its log ends with SUMMARY, FUN and KING lines

#### Scenario: A press the buffer saves
- **WHEN** the human-proxy bot lets go of attack 60 ms before the weapon is ready
- **THEN** the attack fires from the buffer and the press is not counted as lost
