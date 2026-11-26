# spells Specification

## Purpose
Attack and utility spells: elements, forms, learning, ranks and mana.

## Requirements

### Requirement: Attack spells
There SHALL be 88 attack spells: eight elements (fire burns, frost slows, lightning arcs, poison lingers, arcane pierces, holy heals the caster, shadow steals life, earth stuns and knocks back) in eleven forms (bolt, barrage, nova, chain, breath, orb, trap rune, halo, storm, meteor, aura).

#### Scenario: A frost breath
- **WHEN** a player holds a frost breath on a pack
- **THEN** they slow and take damage while it lasts

### Requirement: Utility spells
There SHALL be 38 utility spells for moving (Levitate, Wings of the Gale, Feather Fall, Spider Climb, Strider's Grace, Phase Step, Anchor, Twin Gates, Stairway of Light), safety (Warden's Keep, Wall of Stone, Hut of Rest, Sanctuary, Mirror Image, Stoneskin), knowing (Clairvoyance, Reveal the Hidden, Map of the Mind, Dowsing, Speak with the Dead), monsters (Invisibility, Lure, Charm, Fear, Time Slip, Gravity Well), the world (Stone Shape, Bridge of Light, Quench, Spectral Steed), the basics (Heal, Blink, Ward, Haste, Flare, Recall, Golden Thread) and home (the Sanctum Door); walls, keeps, huts, bridges, tunnels and ice they make SHALL be real blocks for a while, then crumble.

#### Scenario: Warden's Keep
- **WHEN** a player casts Warden's Keep
- **THEN** a keep of stone stands round them that monsters cannot enter, and it becomes their shrine

### Requirement: Learning
Every character SHALL start with Flame Bolt, the Golden Thread and the Sanctum Door; scrolls SHALL teach new spells or raise a known one a rank, up to 5; the stronger forms SHALL drop only deeper in.

#### Scenario: A second scroll
- **WHEN** a player reads a scroll of a spell they know at rank 2
- **THEN** it is rank 3

### Requirement: Mana
Spells SHALL cost mana; the Focus skill SHALL make all spells stronger and cheaper.

#### Scenario: No mana
- **WHEN** a player without enough mana casts
- **THEN** nothing is cast

### Requirement: Recall
Recall cast again within a minute SHALL take the player back to where they cast it.

#### Scenario: Back
- **WHEN** a player recalls home and casts it again within a minute
- **THEN** they return to where they were
