# combat-fairness Specification

## Purpose
That hits on the player are fair: the boxes enemy shots and blows hit, and the warning before an attack.

## Requirements

### Requirement: The box enemy shots hit
An enemy shot SHALL hit a player only within 0.25 m of their centre horizontally (the body is 0.3 m), between their feet and head; blasts keep their own radius.

#### Scenario: A near miss
- **WHEN** an arrow passes 0.35 m from a player's centre
- **THEN** it does not hit them and counts as a near-miss

### Requirement: Warning before an attack
Every monster attack SHALL be preceded by a wind-up of at least 0.3 s that can be seen (red eyes, the raised pose, sparks for casters) or heard; wisps and shamans SHALL wind up 0.35 s before a volley; mimics, spiders and the knight's second blow SHALL wind up at least 0.35 s.

#### Scenario: A wisp
- **WHEN** a wisp attacks
- **THEN** its eyes turn red and sparks gather for 0.35 s before the volley

### Requirement: Melee reach
A monster's blow SHALL land only if the player is within its reach plus its body plus 0.25 m when the wind-up ends.

#### Scenario: Stepped out
- **WHEN** a player steps 0.5 m back from a brute's reach during its wind-up
- **THEN** the blow misses
