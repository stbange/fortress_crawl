# Change: add-settings-menu

## Why
The game had no settings screen: the resolution, dithering, the torch, inverting and the mouse speed were F-keys that were forgotten when the game closed, the volume had only an on/off key, and there was nothing for brightness, effects against music, the stick's speed, vibration, the window mode, v-sync or a frame cap.

## What Changes
- A SETTINGS screen, from the start screen and the pause menu: GRAPHICS (resolution, window or full screen, v-sync, frame cap 30/60/120, brightness 60-160%, dithering, vertex snap, torch), AUDIO (master, effects and music volume in steps of 10%, mute), CONTROLS (mouse and stick sensitivity, invert X and Y, vibration), INTERFACE (the line at the top), and RESET TO DEFAULTS behind a question.
- Changes apply at once and are kept between sessions as key=value lines (a file on the desktop, localStorage in the browser); unknown keys are skipped and values clamped, so older and newer builds share the file.
- The option keys (F2-F4, F6-F10, - and =) change the same values and are kept too; the command line (--lowres, --sens, --invert-x/y, --mute) wins over the saved values for that run. Test, screenshot and bot runs neither read nor write the settings.
- Brightness is a gamma on the final image (the post shader); effects and music volumes scale every effect, loop and piece; vibration gates the gamepad's rumble; the browser greys out v-sync, frame cap and window mode.

## Impact
- Affected specs: controls, settings
- Affected code and docs: src/game/settings.c (new), sfx.c, sfx.h, game.c, src/render/renderer.c, renderer.h, src/main.c; README.md.
