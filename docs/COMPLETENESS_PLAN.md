# Completeness plan: what the game still lacks around the game

The second spec completeness audit (2026-11-27, against the whole-game OpenSpec) found the game
itself well specified and the frame round it missing: no pause or settings menu, nothing kept
between sessions, no accessibility options, one language, and quiet failures. This plan turns its
twelve proposals into work. All twelve were kept (decided 2026-11-27): localization,
achievements and statistics, the journal, the core-loop spec; text size, a colour-blind palette,
difficulty assists, safe areas and aspect ratios; full remapping, save export and import, crash
recovery, performance targets; auto-pause, credits with licences, a list of worlds, the version
on the title.

**Status (2026-11-27):** phases 1 and 2 are done (changes 1 to 7, archived under
`openspec/changes/archive/2026-11-27-*`); phases 3 and 4 are to come.

Each section is one OpenSpec change: proposed under `openspec/changes/<id>/` (proposal, design,
tasks, delta specs; the drafts are in the audit), built, checked locally (native build,
`test_worldgen 1337` and `42`, the browser build and smoke test, the change's own demo), then
archived. Sizes are for one developer: S up to a day, M two to five days, L one to three weeks.

## Order

```
 1 add-title-and-pause-menus ──┬─> 2 add-settings-menu ──┬─> 3 add-accessibility-options
   (UI kit, store.c, worlds)   │                         ├─> 4 add-input-remapping-and-device-switching
                               │                         └─> 6 add-responsive-layout ──> 10 add-localization-support
                               ├─> 5 add-save-management
                               └─> 9 add-achievements-and-stats
 7 add-error-messages (needs the worlds list from 1 for damaged saves; the rest stands alone)
 8 add-journal        11 set-performance-targets        12 tidy-specs (core loop and spec hygiene)
```

| Phase | Changes | Size | About |
|---|---|---|---|
| 1. The frame | 1, 2 | M + M | 2 weeks |
| 2. Everyone can play | 3, 4, 6, 5, 7 | M + L + M + M + S | 5 weeks |
| 3. Keeping players | 8, 9, 11, 12 | S + M + S + S | 1.5 weeks |
| 4. Other languages | 10 | L | 2 to 3 weeks |

About eleven weeks in all. Phase 1 comes first because almost everything else is reached from
its menus and kept in its settings. 12 can be done any time (it is only specs).

## Shared groundwork (done inside change 1)

- **`src/game/store.c`**: the storage now inside `save.c` (`store_read`, `store_write`,
  `store_remove`, the browser's `localStorage` bridge, the desktop's `SDL_GetPrefPath`) moves to
  its own file with one more call, `store_list(prefix, cb)`: the desktop lists files, the browser
  walks `localStorage` keys. Saves, settings (2), the profile of achievements (9) and the
  crash marker (7) all use it.
- **`src/game/ui.c`**: one small kit for every new screen, drawn in the low-res target with the
  bitmap font: a vertical list with scrolling and a selection, an option row (name, value, left
  and right to change), a dialog (title, text, buttons, the safe one selected), a text page that
  scrolls. Every widget takes the same navigation (`NAV_UP/DOWN/LEFT/RIGHT/BUY/BACK`) from the
  keyboard, a gamepad and taps, so no screen is mouse-only or pad-blind. The existing start screen
  (`menu.c`) and later the settings, worlds, credits, pause and remap screens are built from it.
- **The version**: CMake runs `git describe --always --dirty` (falling back to the date) into
  `FC_VERSION`, for the desktop and the browser build alike.

---

## 1. Title and pause menus (`add-title-and-pause-menus`, M, P0)

**Specs**: `front-end` → Start screen (MODIFIED), `menus` → Pause (MODIFIED), `controls` →
Keyboard and mouse (MODIFIED: Esc opens the pause menu and never quits); ADDED Confirmations,
Auto-pause, Credits, Worlds list, Version.

**Design**
- `StartMenu` (game.h, menu.c) becomes a small stack of screens: MAIN (Continue, New World,
  Worlds, Settings, Help, Credits, Quit), NEW (today's seed entry and keypad), WORLDS, CREDITS,
  and the shared dialog. Continue loads the last world (`save_last_seed`); it is hidden when there
  is none. Quit is hidden in the browser.
- **Worlds**: `save_list()` uses `store_list` and `save_peek` (seed text, level, gold, kills,
  play time) for each save; the list shows them newest first; Enter loads, Delete asks first.
  Today's "New characters (press twice)" becomes an entry with a dialog.
- **Pause**: a per-player `pause_open` and a PAUSE screen drawn like the menu (Resume, Settings,
  Help, Save and Quit). In solo play it stops the world exactly as an open menu does now
  (`game.c`, the `paused` flag); in co-op it opens only that player's list and the world goes on.
  Opened by Esc (main.c's Esc handler stops quitting; Esc while paused resumes), START on a
  joined pad (today START only joins), and a small pause button at the top left on touch.
- **Save and Quit**: saves, then returns to the start screen. The world must be torn down
  cleanly (`chunkmap` workers stopped, monsters, pickups, simulation, story caches freed) so a
  second world can start: a `world_unload()` beside `start_world()` in main.c. The browser can
  fall back to `location.reload()` if a leak turns up.
- **Auto-pause**: `SDL_EVENT_WINDOW_FOCUS_LOST` (desktop) and the visibility callback (browser,
  which already saves) open the pause menu in solo play.
- **Credits**: a scrolling page with the makers, SDL3 (zlib licence), Emscripten (MIT / UIUC),
  the sound effects and music (made with ElevenLabs, under its terms), and the font and textures
  (own work). The king's end credits reuse the same page.
- **Version** in the corner of the start screen and the pause menu, and in the crash report (7).

**Tasks**
1. store.c split out; `store_list` on both platforms.
2. ui.c kit: list, option row, dialog, text page; keyboard, pad and tap navigation.
3. Start screen rebuilt from it: MAIN, NEW, WORLDS, CREDITS; delete and new-characters dialogs.
4. `FC_VERSION` from CMake; shown.
5. Pause screen, Esc / START / touch button; `world_unload()` and Save and Quit.
6. Auto-pause on focus loss and hidden tab.
7. Specs; README (controls table: Esc), help pages (controls: Esc pauses).

**Checks**
- `--demo menu` screenshots of MAIN, WORLDS (with three saves made by a script), CREDITS, PAUSE.
- `--demo menutest`: makes two worlds, lists them, deletes one through the dialog, logs each step.
- `--demo pausetest`: a pack awake, Esc, 3 s, the monsters have not moved; Resume, they move.
- A load-play-quit-load cycle in one run (world_unload) under the bots for 5 minutes, no leak
  growth in the log of chunk counts.

**Risks**: world teardown is the one deep change; do it first and test it alone.

## 2. Settings menu (`add-settings-menu`, M, P0)

**Specs**: new `settings` (Settings menu, Saved settings); `controls` → Options keys (MODIFIED:
the keys change the same, kept values).

**Design**
- `src/game/settings.c`: one `Settings` struct (graphics: internal resolution, window mode,
  v-sync, frame cap, brightness, dithering, vertex snap, torch; audio: master, effects, music,
  mute; controls: mouse and stick sensitivity, invert X / Y, vibration; interface: the line at
  the top; later sections add accessibility (3), bindings (4), text size and safe-area margin (6),
  language (10)). Stored as `key=value` lines under the store key `fortress_crawl_settings`, so
  unknown keys from newer builds are skipped and missing ones take defaults.
- Applying: the renderer's `set` (resolution, dither, snap, torch); `SDL_SetWindowFullscreen`
  and `SDL_GL_SetSwapInterval`; a frame cap in the main loop (sleep to the next frame);
  **brightness** as one multiplier in the post-process shader before quantisation (so the bands
  stay); volumes as two buses in `audio.c` (sound and music each with its gain, the master on
  top) replacing the fixed `MUSIC_VOL` and `audio_set_master(0.8)`; vibration gates
  `SDL_RumbleGamepad`; invert and sensitivity replace `g->opt.invert_*` and `g->opt.sens`.
- UI: a SETTINGS screen of the ui.c kit, sections as tabs (Graphics, Audio, Controls, Interface,
  later Access); reached from the start screen and the pause menu; Reset to Defaults behind a
  dialog. F2-F10 and - / = change the same fields and save them.
- Command-line options still win for the session (`--mute`, `--lowres`), without being saved.

**Tasks**
1. settings.c, the file format, load at start, save on change.
2. Audio buses; brightness in the shader; frame cap; window mode; v-sync.
3. SETTINGS screen; from MAIN and PAUSE; Reset.
4. F-keys routed through Settings.
5. Specs; README options section; help (controls).

**Checks**: `--demo settingstest` sets every field, saves, reloads, compares and logs;
screenshots at brightness 0.8 / 1.0 / 1.4 in a dark hall; the audio test run (`--sound`) logs
gains per bus.

## 3. Accessibility options (`add-accessibility-options`, M, P1)

**Specs**: new `accessibility` (Reduced flashing, Not by colour alone, Assists);
`combat-feedback` → Camera impulse and Hitstop (MODIFIED); `death-and-respawn` → Respawn
(MODIFIED: no gold lost with the assist).

**Design**
- Settings gain an ACCESS section: screen shake off / half / full; hitstop on / off; reduced
  flashing; palette normal / protanopia / deuteranopia / tritanopia; damage taken 100 / 75 / 50 %;
  wind-ups x1 / x1.5; keep gold on falling.
- Shake: one multiplier where `ps->shake` is set (game.c hurt, monsters.c kill, dragons_shake)
  and on the landing dip. Hitstop: `game_hitstop` returns at once when off.
- Flashing: `flash_add`, the level-up flash, lightning bursts and the red vignette clamp their
  brightness to a fifth; full-screen white flashes become a short dim pulse.
- Colour: `element_color()` and `rarity_color()` read a palette table; the three alternative
  palettes are chosen so every pair of the eight elements and five rarities stays apart after a
  simulated colour-vision deficiency (Machado matrices, a minimum distance in CIELAB, checked
  by a script). Text where colour carried it: pickup and loot banners name rarity and element
  (most already name rarity), name plates and the boss bar add the element's word, the danger
  warning already names the level.
- Assists: `game_damage_player` multiplies damage taken; `windup_t` and the heavy blow's timer
  are multiplied where they are set (monsters.c, dragon.c, king.c); the gold loss in respawn is
  skipped. The world's save records that assists were used (save version 31, one flag), shown
  in the Worlds list; achievements (9) still count.

**Tasks**: settings fields and screen; shake, hitstop, flash clamps; palette tables and the
check script; text for rarity and element; assists and the save flag; specs, help (a new
ACCESSIBILITY page), README.

**Checks**: the palette script (fails if any pair is too close); screenshots of
`--demo armoury --cast 5` and a pack of each element under each palette; `--demo combat` with
shake off logs zero camera movement; a bot run with 50 % damage taken has fewer deaths.

## 4. Remapping and device switching (`add-input-remapping-and-device-switching`, L, P1)

**Specs**: `controls` → Gamepad (MODIFIED: every screen), `local-coop` → Characters kept
(MODIFIED: a solo disconnect pauses); ADDED Remapping, Prompts follow the device.

**Design**
- **An action layer** (`src/game/input.c`): about forty actions (move four ways, look, jump,
  crouch, sprint, attack, alternate, cast, next / previous spell, weapons 1-4, swap, throw,
  throw choice, campfire, potions, eat, elixir, thread, next line, map, menu and each tab, help,
  pause, skip tutorial). Bindings per device: up to two keys or mouse buttons each for the
  keyboard, a button or an axis direction each for a pad. Defaults are today's layout.
  `game_event`'s key switch, main.c's `SDL_GetKeyboardState` reads and `read_pad` all become
  "is this action pressed / held". Menu navigation keeps fixed keys (arrows, Enter, Esc, the
  D-pad, A, B) so a bad binding can always be undone.
- Hold or toggle for sprint and crouch per device (the pad's sprint toggle becomes the default
  there).
- **Remap screen** in SETTINGS > Controls: the list of actions with their bindings; select, then
  press the new input; a conflict offers to swap; Reset per device. Bindings are kept in the
  settings file.
- **Names of controls**: `input_name(ps, action)` gives "Q", "LMB", "X", "RT", or the touch
  button's label. The help pages, the tutorial, the hints and banners that name keys (potion hint,
  rise prompt, skills key, role hints) use placeholders such as `{CAST}` filled by it, instead of
  fixed per-device texts. This replaces help.c's three-texts-per-page with one text and
  placeholders.
- **The device in use**: player 1's last device changes on any key, mouse, touch or pad input;
  prompts follow within the frame.
- **A pad lost in solo play**: `SDL_EVENT_GAMEPAD_REMOVED` for a pad-only player 1 opens the
  pause menu with "reconnect the controller" until a pad presses a button.
- The start screen and every ui.c screen are already pad-complete from 1; this change adds a
  check over all of them.

**Tasks**: action table and defaults; replace the three input paths; hold / toggle; remap screen
and storage; `input_name` and placeholders through help, tutorial and hints; device tracking;
solo disconnect; specs, help, README controls tables.

**Checks**: `--demo remaptest` rebinds jump to a key via synthesized `SDL_PushEvent` input,
checks the jump, the help text and the tutorial card name the key, resets; `tuttest` still
passes; `--test-pad` plays through the start screen, settings and a menu tab; the bots (which
press pad inputs) still play 10 minutes with the same results.

**Risks**: the biggest refactor of the plan (three input paths); do it behind the defaults so
behaviour is unchanged until bindings change, and compare a bot run before and after.

## 5. Save management (`add-save-management`, M, P1)

**Specs**: `saves` → Where and which (MODIFIED: damaged and newer saves kept, not overwritten);
ADDED Saving indicator, Export and import.

**Design**
- `game_save` sets a short timer; the HUD draws a small mark in a corner while it runs.
- `game_load` returns why it failed (none, damaged, newer). A damaged or newer save is never
  overwritten: it is kept under `<key>.bad` (desktop: the file renamed; browser: the key copied)
  and the Worlds list shows it marked, with Keep or Delete. Starting that seed again asks first.
- **Export**: desktop through SDL3's save-file dialog (`SDL_ShowSaveFileDialog`) to a
  `.fcsave` file; browser through a download (a Blob made in an `EM_JS` helper). **Import**:
  SDL3's open-file dialog, and in the browser a hidden file input; the file is checked with
  `read_header` (magic, version, seed) before it is stored; a world already there asks first.

**Tasks**: indicator; failure reasons and `.bad` keeping; worlds list marks; export and import on
both platforms; specs, help (THE BASICS: saving), README saving section.

**Checks**: `--demo savetest` writes a save, flips bytes in a copy, loads it (damaged, kept),
writes one with a higher version (newer, kept), exports and imports a world and compares the
bytes; the browser smoke test imports a file through the file input.

## 6. Responsive layout and text size (`add-responsive-layout`, M, P1)

**Specs**: new `layout` (Safe areas and touch targets, Aspect ratios and text size).

**Design**
- **Aspect ratios**: today the low-res target is a fixed 16:9 letterboxed into the window, so a
  portrait phone shows a small band. The target keeps its height (270, or 180 / 360) and takes
  its width from the window's aspect, clamped from 4:3 to 21:9, and in portrait it keeps its
  width and grows in height. HUD pieces anchor to edges and corners instead of fixed x
  positions; menus centre and cap their width.
- **Text size**: `font_text` takes a scale (1, 1.5 by nearest pixel doubling on alternate
  columns and rows, 2); every draw function that computes characters per line from `W / 6`
  uses the font's cell size instead; long screens (help, STORY, items) scroll with the ui.c
  page. The retro look stays: the pixels just get bigger.
- **Safe areas** (browser): the page already sets `viewport-fit=cover`, so content can sit
  under a notch today. `env(safe-area-inset-*)` is read through an `EM_JS` helper on resize and
  turned into target pixels; touch buttons (`touch.c` `button_pos`) and HUD margins move
  inward by them.
- **Touch targets**: a minimum of 9 mm (about 34 CSS pixels) for every touch control, from the
  page's CSS pixel size; on small screens the layout drops to the compact one first.

**Tasks**: adaptive target size and anchors; font scale and every `W / 6`; scrolling pages;
safe-area insets and touch sizes; settings fields (text size, extra margin); specs, README
(phones), screenshots.

**Checks**: screenshots of the HUD, the menu tabs, help and the start screen at 4:3, 16:9, 21:9
and portrait, each at text 1x, 1.5x and 2x (a set made by `tools/screenshots.sh`); a debug mode
that logs every text rectangle and flags overlaps and clipping, run over all those screens;
Chromium's device emulation with a notch (iPhone) for the smoke test.

**Risks**: text size touches every screen; budget a pass over each tab.

## 7. Errors players can understand (`add-error-messages`, S, P1)

**Specs**: new `errors` (Error messages, After a crash).

**Design**
- **Browser start**: `web/shell.html` checks for WebGL 2 before the wasm loads and shows a page
  that says the browser cannot run the game and names browsers that can; Emscripten's abort
  shows "the game stopped" with a reload button instead of a frozen canvas.
- **Desktop start**: a failed OpenGL 3.3 context or window shows `SDL_ShowSimpleMessageBox`
  with what is needed.
- **Saving**: `store_write` reports failure (the browser bridge catches `QuotaExceededError`);
  the game shows a banner "COULD NOT SAVE: STORAGE IS FULL - EXPORT YOUR WORLD" (export from 5)
  and keeps trying at the next save.
- **Crash recovery**: a marker `fortress_crawl_session` is stored when a world starts and
  removed on a clean quit. At the next start, a marker left behind means the last session ended
  unexpectedly: the start screen says so and offers Continue (the last autosave). On desktop a
  signal handler (SIGSEGV, SIGABRT, SIGFPE; Windows' unhandled-exception filter) writes
  `crash.txt` next to the saves: the version, the seed, play time and the last 50 log lines kept
  in a ring by an `SDL_SetLogOutputFunction` hook, written with `write()` only.

**Tasks**: WebGL 2 check and abort page; desktop message box; save failure banner; session
marker and start-screen notice; crash handler and log ring; specs, README.

**Checks**: a `--crash-test` option that crashes 5 s into play: the next start shows the notice
and `crash.txt` exists; the smoke test with WebGL 2 disabled in Chromium shows the message; a
browser run with a tiny storage quota shows the banner.

## 8. Journal (`add-journal`, S, P2)

**Specs**: `chronicle` → Traces (MODIFIED).

**Design**: story messages are regenerable from ids (the chapter hooks, dying words, the king's
lore, the hints of the way), like lore entries now (`LORE_*` kinds with a, b, c). New lore kinds
keep them, so the journal costs a few bytes each in the save; plain banners are not kept. The
STORY tab gets a JOURNAL page, newest first, scrolling (ui.c page). Save version 31 (with 3's
flag and 9's statistics, so one bump).

**Checks**: `storytest` reads a note and kills a target; the journal holds both, in order, after
a save and reload.

## 9. Achievements and statistics (`add-achievements-and-stats`, M, P2)

**Specs**: new `achievements`.

**Design**
- **Statistics**: per character in `Stats` (saved with the world, version 31): play time,
  farthest distance and depth, kills by kind and grade, deaths by cause, chests, vaults, spells
  cast, dishes, elixirs, pieces forged, dragons slain, kings settled by ending, heirs. A profile
  across worlds (`fortress_crawl_profile` in the store) adds each world's numbers.
- **Achievements**: a table of about thirty (first blood; 100 / 1000 kills; every monster kind;
  a boss; a dragon; an ancient wyrm; each ending of the king; an heir settled; 1 km from the
  gate; the world core; heaven; every realm's ruler; a masterwork; ten formulae; every recipe; a
  town calls you kin; the Sanctum fully built; a chronicle finished each way; a weapon awakened;
  ...). Each has a condition over the statistics, checked every few seconds and on the events
  that matter; unlocking shows a banner and a sound, and is kept in the profile.
- **Records** screen from the start screen: the lifetime numbers and the achievements (locked
  ones show their name and how to earn them).
- Assisted worlds (3) count, as decided; the record notes it.

**Checks**: `--demo achtest` sets counters and sees unlocks, banners and the profile saved; a
30-minute bot run logs the achievements it reached (expect a handful).

## 10. Localization (`add-localization-support`, L, P2)

**Specs**: new `localization`.

**Design**
- **Strings**: about 4 100 string literals; the generated story text alone is about 800 grammar
  strings (`history_text.c`, `story.c`) built for English (articles, plurals, word order).
  - Phase A (one week): every fixed UI string (menus, HUD, help, the tutorial, banners, hints,
    item and spell names, about 1 500) moves to `T("KEY")` lookups from `lang/<code>.txt` files,
    embedded in the build; a script finds literals not yet moved and fails the check.
  - Phase B (one to two weeks): the generators get a language interface (names, the article
    and gender of a noun, plurals, case where the language needs it) and a grammar per language;
    English stays the reference and the validator in `test_worldgen` runs per language.
- **Font**: the 5 x 7 table grows to Latin-1 capitals and signs (Ä, Ö, Ü, É, È, Ñ, Ç, ß, ...),
  accents drawn into the top rows as retro fonts did; `font_text` decodes UTF-8.
- **Language setting** in Settings (Interface); help and the tutorial per language.
- The second language is to be chosen before Phase A starts, with someone to translate and play
  through it.

**Checks**: the literal finder; screenshots of every screen in both languages at text 2x (from
6) with the overlap logger; `test_worldgen` story validation in both languages; `storytest` and
`kingtest` in the second language.

**Risks**: Phase B is the largest single task in the plan; Phase A can ship alone, with the
generated story in English and a note saying so.

## 11. Performance targets (`set-performance-targets`, S, P2)

**Specs**: `playtest-bots` → Timing (MODIFIED: targets).

**Design**: `tools/perfcheck.sh` runs `FC_TIMING=1` on four scenes (the start, `--demo combat`,
`--demo weapons`, the Sanctum) and fails if the mean frame is over 16.7 ms or the worst over 33 ms
on the reference machine (a named desktop GPU, recorded in the script); in this container it
only reports (software rendering). The browser's time to play at 25 Mbit/s is measured by the
throttled Chromium script (`ttfa.js`) and must be under 3 s: today it is 3.3 s, so this change
also serves the wasm and data compressed (gzip / Brotli pre-compressed files and headers for
Pages) and checks the result.

**Checks**: the script on the reference machine; the throttled browser check under 3 s.

## 12. Tidy the specs (`tidy-specs`, S, P2)

**Specs only**: the core loop as a requirement (out, back and on); developer flags moved from
player specs to `dev-tools` (`--no-tutorial`, the armoury demo, `FC_WEB_RAW_SFX`); `encounters` →
Fight length in absolute numbers instead of "40% more than before"; the vague quantities
("about one chunk in seven", "now and then", "about 15 minutes") stated as rates checkable
over many samples, and `test_worldgen` given those checks; rules stated twice (damage per level,
keystone sources) kept in one place and referenced from the other.

---

## Saves, settings and versions across the plan

- Save version 31 once, carrying 3's assist flag, 8's journal lore kinds and 9's statistics;
  older saves load as always (`saves` → Where and which).
- The settings file is versionless by design (key=value, unknown keys skipped).
- The profile (9) and the session marker (7) are separate store keys, never inside a world's
  save, so deleting a world keeps the records.

## Done means

For every change: its specs archived, its demo or scripted check in the repository and passing,
its screenshots in `tools/screenshots.sh` where it has a screen, the help pages and README
updated, and the local checks passed before the push. After phase 2, the completeness audit is
run again; the expected result is every applicable checklist item present.
