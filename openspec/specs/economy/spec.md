# economy Specification

## Purpose
Gold: the shop, sealed weapons, keystones, decorations and the other things gold buys.

## Requirements

### Requirement: The shop
The SHOP tab SHALL sell by place: anywhere, scrapping weapons worse than the one worn; at a shrine, health and mana potions and sealed weapons; at home, everything (potions, a rank for the active spell, new affixes for the weapon in hand, another of a relic carried, the library's scroll, the stash and building the Sanctum).

#### Scenario: At home
- **WHEN** a player opens the SHOP tab in the Sanctum
- **THEN** building and the stash are offered

### Requirement: Sealed weapons
At a shrine and at home, three sealed weapons SHALL be sold a day, showing kind and grade but not what is inside: 300 gold for anything, 800 for magic or better, 1500 for rare or better, which now and then holds one of the world's legendaries.

#### Scenario: A gamble
- **WHEN** a player buys a sealed weapon for 800 gold
- **THEN** it is at least magic

### Requirement: Gold sinks
Gold SHALL buy more than it brings: the Sanctum's stages, keystones, the forge's supplies, eleven decorations for beauty (500 to 5000 gold, built at once), the towns' works and their wares.

#### Scenario: A statue
- **WHEN** a player buys the founder's statue
- **THEN** it stands in the court at once
