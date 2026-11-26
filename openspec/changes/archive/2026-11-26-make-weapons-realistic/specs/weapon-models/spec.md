## ADDED Requirements

### Requirement: Shaped weapons
Weapons SHALL be built from shaped parts: blades with edge bevels, distal taper and a point (a fuller where the kind has one); round, tapered hafts, grips, limbs and barrels; wedge-thin cutting edges on axes and scythes.

#### Scenario: A sword
- **WHEN** a player holds a sword
- **THEN** its blade narrows and thins to a point, its edges catch the light apart from the flat, and its grip is wound

### Requirement: Lit facets
Weapons SHALL light each facet with a key light and, on metal, a stepped glint toward the eye, never brighter than 1.15 of full.

#### Scenario: Torchlight
- **WHEN** a player turns a sword under a torch
- **THEN** facets of the blade brighten in steps

### Requirement: The armoury
--demo armoury --cast PAGE SHALL show the weapons close up under even light with no HUD: page 0 the twelve, 1 the lost arts, 2 and 3 in rarities and materials, 10 plus a base number that weapon alone.

#### Scenario: A close-up
- **WHEN** the game runs with --demo armoury --cast 11
- **THEN** the arming sword fills the view
