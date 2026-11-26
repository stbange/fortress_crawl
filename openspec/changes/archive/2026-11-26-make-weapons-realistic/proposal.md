# Change: make-weapons-realistic

## Why
Every weapon was a stack of boxes with a pyramid for a point: blades were flat slabs, hafts square, grips plain, axe bits and mace heads blocks, and each face took one of three fixed shades, so steel did not read as steel. The weapon in hand is on screen nearly all the time.

## What Changes
- Blades lofted through cross-sections: an edge bevel on each side, a fuller, distal taper and an ogive point; kinds for the sword, dagger, greatsword (with a ricasso), rapier (a diamond) and spear (a leaf).
- Round, tapered hafts, grips, bow limbs and barrels; grips wound with cord between ferrules; crossguards that bend toward the blade and end in knobs, a langet, wheel pommels.
- Flat profiles with a wedge for axe bits (with a ground bevel), mace flanges, scythe blades, a chakram's ring, a crossbow's stock; lathed cups, sockets, censers and cannon barrels; arrows with a bodkin and three vanes.
- Weapons light their own facets: a key light from above and in front, and a glint on metal toward the eye, stepped like the world's light.
- --demo armoury --cast PAGE shows the weapons close up under even light (0 the twelve, 1 the lost arts, 2 and 3 in rarities and materials, 10 + base one alone).

## Impact
- Affected specs: weapon-models
- Affected code and docs: src/game/weapons.c, src/render/batch.c and batch.h (batch3d_tri_raw), src/game/game.c, game.h, src/main.c; tools/screenshots.sh; README.md; docs/screenshots.
