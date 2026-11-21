# player-input Specification

## Purpose
How presses become actions: buffered attacks, spells and throws, and the forgiving parts of a jump.

## Requirements

### Requirement: Buffered attacks
An attack pressed while the weapon is still cooling SHALL be remembered for 120 ms and SHALL fire as soon as the cooldown allows within that time.

#### Scenario: Pressed a moment early
- **WHEN** a player taps attack 80 ms before the cooldown ends
- **THEN** the attack fires when the cooldown ends

### Requirement: Buffered spells and throws
A spell or a throw pressed while its cooldown runs SHALL be remembered for 120 ms and SHALL be cast or thrown as soon as allowed within that time.

#### Scenario: A spell a moment early
- **WHEN** a player presses cast 50 ms before the spell cooldown ends
- **THEN** the spell is cast when it ends

### Requirement: Coyote time
A jump pressed up to 100 ms after walking off an edge SHALL still jump.

#### Scenario: Off the edge
- **WHEN** a player presses jump 60 ms after leaving the ground without jumping
- **THEN** they jump

### Requirement: Jump buffer
A jump pressed up to 120 ms before landing SHALL jump on landing.

#### Scenario: Pressed early
- **WHEN** a player presses jump 90 ms before landing and releases it
- **THEN** they jump as they land

### Requirement: Hang at the top
While jump is held and the vertical speed is under 1.5 m/s in a jump, gravity SHALL be 0.6 of normal.

#### Scenario: At the apex
- **WHEN** a player holds jump at the top of a jump
- **THEN** they hang there a little longer
