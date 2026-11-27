# Change: add-save-management

## Why
A damaged save failed quietly (the world started again and the next autosave wrote over it), a save from a newer build was refused the same way, there was no sign that the game was saving, and no way to keep a world elsewhere or move it to another computer or browser.

## What Changes
- A CRC-32 at the end of every save (version 31): a damaged save is known before anything of it is used. A damaged save, or one from a newer version, is never written over: it is kept aside ("<key>.bad") and the world starts with new characters, after a question on the start screen.
- WORLDS shows each world's state (damaged, newer version, kept aside) and opens a world's own screen: PLAY (or START AGAIN), EXPORT TO A FILE, DELETE. IMPORT A WORLD is at the end of the list, even when it is empty.
- Export and import as .fcsave files: on the desktop SDL's file dialogs (without one, the saves' folder); in the browser a download and a file chosen in the page. An import is checked (magic, version, check) and replacing a world asks first.
- SAVING shows in the top left while the game saves.
- --demo savetest: damaged, newer, kept aside, a file out and back the same, an old (version 30) save still read.

## Impact
- Affected specs: saves
- Affected code and docs: src/game/save.c, transfer.c (new), menu.c, game.c, game.h, help.c, src/main.c, CMakeLists.txt; README.md.
