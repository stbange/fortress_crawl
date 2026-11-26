# Change: specify-whole-game

## Why
The specs covered only the systems changed since the realms (2026-11-16); the rest of the game (the world, monsters, combat, items, skills, spells, the menus, vaults, the chronicle, the Sanctum, food, alchemy, the forge, the simulation, sound, co-op, controls, the look, saving and the tools) was described only in the README, docs/ and the code. A spec completeness audit found most of its gaps there.

## What Changes
- 25 capabilities added or extended so the specs cover the whole game as it stands: world-generation, difficulty, monsters, combat, items-and-loot, character, spells, menus, mechanisms, chronicle, story-choices, sanctum, economy, food-and-wildlife, alchemy, forge, world-simulation, map, hud, audio, local-coop, controls, rendering, front-end, dev-tools; saves and web-loading extended.
- No behaviour changes: each requirement states what the game does today, from the README and docs/, with counts checked against the code.

## Impact
- Affected specs: alchemy, audio, character, chronicle, combat, controls, dev-tools, difficulty, economy, food-and-wildlife, forge, front-end, hud, items-and-loot, local-coop, map, mechanisms, menus, monsters, rendering, sanctum, saves, spells, story-choices, web-loading, world-generation, world-simulation
- Affected code and docs: openspec/ only (and project.md: the specs now cover the whole game).
