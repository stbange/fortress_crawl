# combat Specification

## Purpose
Weapons in hand, hitting and being hit: damage, critical hits, elements against monsters, falling.

## Requirements

### Requirement: Three weapons
A character SHALL carry three weapons, a blade, a bow and a staff, and hold one (or the pickaxe) at a time; blades strike within their reach and arc, bows shoot arrows, staves shoot bolts that cost mana.

#### Scenario: A staff
- **WHEN** a player shoots a bolt from a staff
- **THEN** mana is spent and the bolt flies

### Requirement: Damage
A hit's damage SHALL grow 7% per character level above the first and with the weapon's grade, rarity and affixes; every weapon SHALL land a critical hit for double damage 5% of the time (more with a rapier or crystal).

#### Scenario: A critical
- **WHEN** a hit is critical
- **THEN** it deals double damage with a flash and its own sound

### Requirement: Elements of hits
A weapon's element SHALL act on the monster: fire burns, frost chills and slows, lightning jolts, venom poisons, vampiric hits draw life back.

#### Scenario: Venom
- **WHEN** a venomous blade hits a brute
- **THEN** it keeps taking damage for a few seconds

### Requirement: Falling
Falls from high up SHALL hurt, at most a third of the player's health; Feather Fall, Levitate, Wings and water SHALL save one.

#### Scenario: Into water
- **WHEN** a player falls 20 m into deep water
- **THEN** they take no harm

### Requirement: Healing
Health SHALL come back by itself only out of a fight (some time unhurt and nothing awake within 12 m); mana SHALL always come back.

#### Scenario: Mid-fight
- **WHEN** a player stands hurt beside an awake brute
- **THEN** their health does not come back by itself
