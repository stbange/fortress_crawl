# Change: add-help-and-tutorial

## Why
The game had no help inside it: the keys and the systems were explained only in the README, F1 toggled a debug overlay, and a new character was dropped at the gate with the line at the top and nothing else. The fun audit's novice persona gets nowhere; a player new to it would fare little better.

## What Changes
- A HELP tab, the menu's last: nine topics (controls, the basics, fighting, spells, staying alive, the story, the world, making things, loot), each page written for the device in use (keyboard and mouse, a gamepad, touch). F1 or ? opens it; the debug overlay moves to F11.
- A tutorial for a new character, after they choose why they came: seven steps on a card at the left of the view (move, jump, strike, change weapon, cast, stay alive, the story), each done when the player does it, the last handing them to the line at the top and the Golden Thread.
- Backspace or holding BACK skips it; Enter on the help's first page runs it again; --no-tutorial starts without it. Bots, demos, screenshots and the test pad never see it.
- --demo help --cast TOPIC, --demo tutorial --cast STEP, and --demo tuttest, which does each step and logs it.

## Impact
- Affected specs: help-and-tutorial
- Affected code and docs: src/game/help.c (new), game.c, game.h, quests.c, items.c, src/main.c, CMakeLists.txt; README.md, tools/screenshots.sh, docs/screenshots.
