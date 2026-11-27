# Change: add-accessibility-options

## Why
The audit found no way to calm the camera, the flashes or the hit pauses, colours that carried meaning alone (the elements and the tiers of loot; with deuteranopia frost and lightning are 1.7 apart in CIELAB), and no assists for players who cannot keep up with the fights.

## What Changes
- An ACCESSIBILITY part in the settings: screen shake off, half or full (the landing's dip too); hit pause on or off; reduced flashing (point flashes at a fifth, the red of being hurt and monsters' white flash dimmed); colour palettes for protanopia, deuteranopia and tritanopia; and three assists: damage taken 75 or 50%, enemy wind-ups half again as long (and a dragon's breath), no gold lost on death or on going back where you fell.
- palette.h holds the elements' and tiers' colours for each palette; tools/check_palettes.py sees each through its deficiency (Machado 2009) and fails if any two are closer than 20 in CIELAB (the alternatives keep 25.6 or more, the normal palette 20.3 seen normally).
- What colour said is also written: a find names its tier and element, a named foe's plate its element, a friend or one that yielded is marked so.
- A world played with an assist on is marked in its save (version 31: flags in the header) and in the list of worlds; nothing else changes.
- --set key=value sets a setting for one run without writing it; --demo accesstest logs what a blow, a kill's pause, a wind-up and a death come to.

## Impact
- Affected specs: accessibility, combat-feedback, death-and-respawn
- Affected code and docs: src/game/settings.c, palette.h (new), spells.c, items.c, quests.c, game.c, monsters.c, monster_draw.c, dragon.c, player.c, save.c, menu.c, help.c, src/main.c; tools/check_palettes.py (new); README.md.
