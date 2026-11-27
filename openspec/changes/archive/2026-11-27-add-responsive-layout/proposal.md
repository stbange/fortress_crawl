# Change: add-responsive-layout

## Why
The low-res target was a fixed 16:9 picture letterboxed in the window (only touch screens took their width), a portrait phone was told to turn sideways, the text could not be made bigger, a phone's notch could cover the controls, and touch buttons could be smaller than a finger.

## What Changes
- The target takes the window's shape: the resolution's height with the width from the window (4:3 to 21:9), and in portrait the 16:9 width growing in height. --lowres is kept exactly.
- A UI frame for every screen round the game (ui.c): a viewport inset by the browser's safe areas (env(safe-area-inset-*)) and a SCREEN MARGIN (0-10%, televisions), and pixels TEXT SIZE times bigger (100, 150, 200%), never leaving less than 240 x 135 to lay out in; with touch controls at least big enough that the smallest button is 9 mm across. Taps are read in the same frame.
- Screens made to fit: footers that stack when too wide, the help's topics closer on a short screen, menu tabs with short names, long item names cut, the line at the top in two lines, the start screen packed on a short screen (the keypad beside the rows), the build number in screen pixels.
- FC_TEXTCHECK=1 logs any text drawn past the edge of its screen; run over the screens at 4:3, 16:9 and portrait at 100 and 200%, it finds none.

## Impact
- Affected specs: layout, settings
- Affected code and docs: src/main.c, src/game/ui.c, touch.c, menu.c, game.c, help.c, items.c, explore.c, quests.c, pause.c, settings.c, src/engine/font.c; README.md.
