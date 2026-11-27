# Change: add-title-and-pause-menus

## Why
The completeness audit found no pause menu (Esc let the mouse go, and a second Esc quit the game, saving on the way out), no way back to the start screen without quitting, a start screen that only typed seeds (deleting a save took a second press of the same row, with no question asked), no list of the worlds saved here, no credits with the licences of what the game is built with, and no build number to report a bug against.

## What Changes
- A pause menu (Esc, START on a gamepad, the II button on touch): RESUME, SETTINGS, HELP, SAVE AND QUIT TO TITLE, and on the desktop SAVE AND QUIT GAME. Alone it stops the world as any open menu does; in co-op it is the player's own and the world goes on. Esc no longer quits the game.
- Auto-pause when playing alone: the window losing the focus, the browser tab being hidden, or the browser's pointer lock ending (its own Esc) opens the pause menu.
- Save and quit to the start screen: the world is saved, then everything of it freed (chunks, edits, fluids, stains, the explored map, lore, the flow fields; the loops of sound stopped), keeping the window, renderer, options, touch controls and gamepads, so another world starts as if the game had just been opened.
- A new start screen: CONTINUE (the world played last, with its level, gold, kills and time), NEW WORLD (the seed entry, a random seed, the on-screen keypad on touch), WORLDS (every world saved here, to play or delete), SETTINGS, HELP, CREDITS, and QUIT on the desktop. Typing on the first screen starts a new world's seed.
- Questions before what cannot be undone: starting a saved world again with new characters, deleting a world, and quitting from the start screen each ask in a dialog whose safe answer is chosen first.
- Credits with the licences (SDL 3: zlib; Emscripten: MIT and UIUC/NCSA) and where the sounds and music come from.
- The build (git describe at configure time, else "dev") in a corner of the start screen and in the pause menu.
- store.c: the store of saves and settings split out of save.c, with a listing of keys by prefix (files on the desktop, localStorage in the browser). ui.c: one kit of lists, dialogs and text pages for every screen, driven by the same moves from keyboard, gamepad, mouse and touch.
- --demo pause --cast 0|1|2, menu, menu-new, menu-worlds, menu-settings, menu-help, menu-credits (screenshots); --demo pausetest (the world stands still while paused) and --demo titletest (save and quit to title, the world is listed, CONTINUE loads it where it was left).

## Impact
- Affected specs: controls, front-end, menus, saves
- Affected code and docs: src/game/pause.c, menu.c, ui.c, store.c (new or rewritten), game.c, game.h, save.c, touch.c, help.c, sfx.c, src/world/chunkmap.c, src/main.c, CMakeLists.txt; README.md, tools/screenshots.sh, docs/screenshots.
