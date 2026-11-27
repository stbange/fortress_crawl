# Change: add-input-remapping-and-device-switching

## Why
Every key and button was fixed in the code (and letter keys followed the keyboard's labels, so WASD moved elsewhere on other layouts), sprint and crouch could not be held or toggled by choice, the help and hints named the keys of the default layout whatever the player used, and a solo player whose controller was unplugged kept playing blind.

## What Changes
- An action layer (input.c): 38 actions of play, each with two keyboard or mouse inputs and one gamepad input (a button or a trigger), kept in the settings file as key_JUMP=44,0 and pad_JUMP=0. Keys are scancodes (where a key sits) named as the keyboard labels them. Menus keep fixed keys (arrows, Enter, Esc, the D-pad, A, B) so a bad binding can always be undone; the option and debug keys stay where they were.
- SETTINGS > CONTROLS: KEYS and BUTTONS pages list every action with its inputs; Enter (A) takes the next input, Right a second key, Delete (X) clears; an input another action has is offered as a swap; each page resets on its own. Sprint and crouch can be held or toggled, for the keys and the pad apart.
- The help's pages, the tutorial and the hints write {JUMP} and the like, filled in with the input of the device the player used last (keyboard and mouse, gamepad, touch), changing within the frame.
- Alone, a player whose gamepad is unplugged while playing with it gets the pause menu with RECONNECT THE CONTROLLER until a pad presses a button.
- --demo remaptest: a new key for jump through the screen, a clash swapped, the help naming the key, a remapped action used in play, the keys reset; --demo pause --cast 3 and 4 show the pages.

## Impact
- Affected specs: controls, local-coop
- Affected code and docs: src/game/input.c (new), game.c, game.h, settings.c, help.c, pause.c, src/main.c, CMakeLists.txt; README.md.
