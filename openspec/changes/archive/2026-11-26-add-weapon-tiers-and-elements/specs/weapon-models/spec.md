## ADDED Requirements

### Requirement: Tier styles
A weapon's shape SHALL follow its tier: plain iron fittings for common; brighter steel, finer fittings and a rune line for magic; gilding, ornate guards, gem pommels and two fullers for rare; blackened steel, thorns, serrated, waved or clipped blades and glowing runes for epic; starmetal, winged guards, crowned pommels, double crescent axes and haloed staves for legendary.

#### Scenario: An epic sword
- **WHEN** an epic sword drops
- **THEN** its guard is thorned or swept, its pommel spiked or set with a gem, and runes glow down its blade

### Requirement: Looks
Every weapon SHALL carry a look rolled when it is made, which picks its guard, pommel, form and grip cord within its tier; a weapon from an older save SHALL take a look from its other fields, the same each load.

#### Scenario: Two rare swords
- **WHEN** two rare swords drop
- **THEN** they usually differ in guard, pommel or blade

### Requirement: Elements on the weapon
An element SHALL show on the weapon itself: frost as blue steel with white edges, ice crystals and icicles; fire as heat-darkened steel with flames and glowing cracks; lightning as a copper coil and arcs; venom as green serrated steel with drops; the vampiric as crimson steel with thorns. Swift weapons SHALL be slimmer, heavy ones bulkier and studded, keen ones longer in the point.

#### Scenario: An icy sword
- **WHEN** a player holds a sword of frost
- **THEN** its steel is blue, its edges white, and ice crystals grow from it with icicles hanging below

### Requirement: How it is made
The inventory SHALL say in a line how the selected weapon is made: its blade or head, guard, pommel or tips, and its element.

#### Scenario: A frost sword
- **WHEN** a player selects a frost sword with a waved blade
- **THEN** the line reads like WAVED BLADE, SWEPT GUARD, GEM POMMEL - ICE GROWS ON IT

## MODIFIED Requirements

### Requirement: The armoury
--demo armoury --cast PAGE SHALL show the weapons close up under even light with no HUD: page 0 the twelve, 1 the lost arts, 2 and 3 in rarities and materials, 4 the tiers, 5 the elements, 6 the twelve frozen, 7 twelve looks of one rare sword, 10 plus a base number that weapon alone; with --weapon, that weapon alone.

#### Scenario: A close-up
- **WHEN** the game runs with --demo armoury --weapon axe:legendary:frost
- **THEN** that axe fills the view
