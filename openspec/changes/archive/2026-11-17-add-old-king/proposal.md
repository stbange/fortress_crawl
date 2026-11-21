# Change: add-old-king

## Why
The fortress had stories but no end the whole game leads to. The Old King is the main storyline of every world: ten steps that take the player through towns, the Sanctum, dragons, crafting and the realms, then the throne.

## What Changes
- One king per world of ten kinds, hidden at first; a way of ten steps generated from the seed and validated.
- Step handlers: the herald, the witness, the true name, the vision, the dragon, the hoard, the allegiance, the forge, the brew, the realm, the relic, the lieutenant, the seal, the wyrm.
- A throne hall per world; three endings (the sword, his true name, his artefact); credits, then explore on.
- Save version 28; --demo kingtest; test_worldgen checks every way.

## Impact
- Affected specs: old-king
- Affected code and docs: src/world/king.c, src/game/king.c, layout.c, gen_struct.c, hooks in town.c, realm.c, combat.c, monsters.c, gate.c, save.c; docs/OLD_KING.md.
