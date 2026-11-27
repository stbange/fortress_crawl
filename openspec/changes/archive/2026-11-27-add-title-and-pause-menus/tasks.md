## 1. Pause
- [x] 1.1 pause.c: the menu, its moves and taps; Esc, START and the II button
- [x] 1.2 Auto-pause on focus loss, a hidden tab and the pointer lock ending
- [x] 1.3 Mouse clicks go to the menu while it is open; the mouse is caught again on resume

## 2. Back to the start screen
- [x] 2.1 game_unload; chunkmap_shutdown frees fluids, stains and carve sets
- [x] 2.2 main.c: quit requests (to title, or out), the command line's first-world options cleared

## 3. Start screen
- [x] 3.1 store.c with store_list; ui.c lists, dialogs, pages
- [x] 3.2 menu.c: CONTINUE, NEW WORLD, WORLDS, SETTINGS, HELP, CREDITS, QUIT; confirmations
- [x] 3.3 The build in a corner (CMake: FC_VERSION)

## 4. Verification
- [x] 4.1 --demo pausetest: a monster in front stands still 1.5 s while paused, moves after
- [x] 4.2 --demo titletest: listed, continued, level and position back; AddressSanitizer clean
- [x] 4.3 Screenshots of every screen; build, test_worldgen 1337 and 42, the browser build and smoke test
