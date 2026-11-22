# weapon-roles Specification

## Purpose
Why the blade, the bow and the staff each have their place: what each is good against, the alternate attack each has from the start, and the swap between them.

## Requirements

### Requirement: Weapon roles
A hit SHALL deal 1.5 times its damage when an arrow strikes a flier or a shooter (skeletons, wisps, shamans, eyes, necromancers); against armour (knights, golems, mimics) a mace, flail, sling or cannon SHALL deal 1.4 times, the staff 1.3 times, other blades 0.8 times and arrows 0.7 times; dragons SHALL take no role modifier.

#### Scenario: A harpy
- **WHEN** a player's arrow hits a harpy
- **THEN** it deals half again its damage and a warm spark shows it bit

#### Scenario: A knight
- **WHEN** a player's sword hits a knight
- **THEN** it deals four fifths of its damage and a grey spark shows it glanced off

### Requirement: Spells against armour
Attack spells SHALL deal 1.3 times their damage to armoured foes.

#### Scenario: A golem
- **WHEN** a player's fire bolt hits a golem
- **THEN** it deals 1.3 times its damage

### Requirement: Role hints
When a weapon's role changes a hit, a banner SHALL say which weapon would serve (or that arrows bring fliers down), at most once in five minutes.

#### Scenario: Armour
- **WHEN** a player first strikes a knight with a sword
- **THEN** a banner says armour turns the edge and a mace, a flail or the staff bites

### Requirement: Alternate attacks from the start
Every weapon SHALL have its alternate attack without skills: the blade a small whirl (0.9x damage, 10 mana), the bow a power shot (one arrow at 2.2x that passes through three foes, 10 mana), the staff a small nova (12 short bolts, 16 mana); the last skill of each branch SHALL make it the Whirlwind, the Arrow Storm and the Nova.

#### Scenario: A new character
- **WHEN** a level-1 player with the bow presses the alternate attack with 10 mana
- **THEN** a power shot flies

### Requirement: Quick swap
Z SHALL take up the weapon held before the current one; the swing timer SHALL carry over unchanged.

#### Scenario: Bow to blade
- **WHEN** a player with the bow, who held the sword before, presses Z
- **THEN** the sword is in hand and its next blow waits for the same cooldown
