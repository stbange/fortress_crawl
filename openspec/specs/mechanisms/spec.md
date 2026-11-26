# mechanisms Specification

## Purpose
Shrines, traps and the sealed vaults with their puzzles.

## Requirements

### Requirement: Shrines
Shrines (glowing rune stones) SHALL be attuned by walking up to one: it heals fully, becomes where one wakes after falling, and saves the game; at a shrine the shop sells potions and sealed weapons, and the cauldron can be used.

#### Scenario: Attuning
- **WHEN** a hurt player walks up to a shrine
- **THEN** they are healed and it becomes their shrine

### Requirement: Traps
From danger level 2 the floor SHALL hold traps: a loose flagstone that fires darts from the nearest wall, spike patches that stab every few seconds, flame vents that roar up after a short sputter; traps SHALL hurt monsters too, and lava SHALL burn.

#### Scenario: Spikes
- **WHEN** a player stands on a spike patch as it stabs
- **THEN** they take damage

### Requirement: Vaults
About one chunk in seven SHALL hide a vault behind a portcullis with a hoard (gold, potions, a rare or epic weapon, a keystone, a formula page); standing at its gate SHALL give a hint and its lamps SHALL show how far the puzzle is from solved; an opened vault SHALL stay open.

#### Scenario: A hint
- **WHEN** a player stands at a vault's gate
- **THEN** a line tells what its puzzle wants

### Requirement: Puzzles
A vault's puzzle SHALL be nearby and one of four: levers (each pull moves two lamps), marked plates stepped on in order, statues that must look at the gate, or a light beam led over mirrors to a crystal lock; levers, statues and mirrors SHALL be worked by striking them.

#### Scenario: Statues
- **WHEN** every statue is turned to look at the gate
- **THEN** the portcullis opens
