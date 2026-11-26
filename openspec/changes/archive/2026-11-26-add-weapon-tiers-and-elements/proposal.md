# Change: add-weapon-tiers-and-elements

## Why
Weapons of every tier had the same shape: only the colours of the materials changed with the rarity, and an icy weapon looked like a plain one with a blue glow. The player should see a weapon's worth and its element at a glance, and two weapons of a tier should not be twins.

## What Changes
- A style for every weapon (weapon_style): the tier sets the vocabulary (guards, pommels, blade forms, fullers, inlay, bow tips, staff heads, axe bits, mace flanges), a look byte rolled when the weapon is made picks within it, and the affixes add the rest.
- Elements change the weapon: frost blue steel, white edges, ice crystals and icicles; fire heat-darkened steel, flames and glowing cracks; lightning copper coils and arcs; venom green, serrated steel and drops; the vampiric crimson steel and thorns. Swift is slimmer, heavy bulkier and studded, keen longer in the point.
- Item.look, saved from version 30; generated with every dropped, forged and legendary weapon; older weapons take a look from their other fields.
- The inventory says how a weapon is made.
- Armoury pages 4-7 and 9 (--weapon alone).

## Impact
- Affected specs: saves, weapon-models
- Affected code and docs: src/game/weapons.c, items.h, items.c, save.c, arms.c, forge.c, game.h, src/main.c; README.md; docs/screenshots.
