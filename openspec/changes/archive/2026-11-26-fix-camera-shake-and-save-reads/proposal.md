# Change: fix-camera-shake-and-save-reads

## Why
Two bugs found on 2026-11-26. The camera shook all the time after the first hard hit or kill: the shake wore off only in the dragons' tick, which runs only while a dragon is alive, and the P2 camera kicks were never undone without one. And reloading a save lost the map, the broken and dug blocks and more: four reads passed r8() into MINI(), a macro that evaluates its argument twice, so everything after the first character was read out of step (since save version 19).

## What Changes
- The camera's shake settles in every player's update, near dragons or not.
- The loader reads each value into a variable before clamping it; existing saves (whose writer was right) load whole.

## Impact
- Affected specs: combat-feedback, saves
- Affected code and docs: src/game/game.c, src/game/dragon.c, src/game/save.c.
