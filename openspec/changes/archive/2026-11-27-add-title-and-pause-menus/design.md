# Design

## Decisions
- **Unload in place, not a restart**: a world's state lives in the Game struct and a few module
  statics that `start_world` already resets for a new seed. `game_unload` frees what Game owns,
  `chunkmap_shutdown` now also frees each chunk's fluid, stain and carve sets and zeroes the map,
  and Game is cleared except the window side (options, renderer, touch, gamepads, the HUD switch,
  the resolution mode). Checked with AddressSanitizer across quit to title and back.
- **Command-line choices are for the first world**: --fresh, --pos and --seed are cleared on the
  way back to the start screen, so CONTINUE loads the save.
- **Esc**: closes what is open, else opens the pause menu and lets the mouse go. Closing the pause
  menu catches the mouse again on the desktop; in the browser the pointer lock needs a click.
- **Pause counts as a menu**: the existing rule (menus pause only a solo game) covers it, so co-op
  keeps running and nothing new can desynchronise.
- **The list of worlds** reads the store's keys by prefix and each save's header (`save_peek`); a
  save written by an older build still lists (its seed text is rebuilt from the key).
