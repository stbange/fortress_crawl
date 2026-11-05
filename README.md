# Fortress Crawl

An endless, procedurally generated 3D dungeon crawler in C (C11) using SDL3 and OpenGL 3.3.
You start in front of a huge castle built into a mountain. Everything behind the gate is
generated on the fly, without end, in every direction including up and down. The farther you
get from the gate, the more dangerous it gets.

This is an early prototype. It has **world generation, lighting, the retro renderer,
Hexen-style combat against seven kinds of primitive monsters, 12 weapon types with rarities
and affixes, 95 spells, loot, chests and relics, an inventory, a classless skill tree,
shrines, an explored-cells map, split-screen co-op for up to four players, saving, and
generated stories: a motive per character, an endless metaplot of named enemies, inscriptions,
notes of the dead and bounty boards**, and **sound**: every monster, weapon, spell, trap and
place has its own sound effects, made with ElevenLabs (see [Sound](#sound)). See [`docs/PLAN.md`](docs/PLAN.md)
for the full plan and [`docs/STORY.md`](docs/STORY.md) for how the stories work.

## Screenshots (prototype, seed 1337)

| | |
|---|---|
| ![Before the gates](docs/screenshots/exterior.png) | ![The Grand Vestibule](docs/screenshots/vestibule.png) |
| Before the gates | The Grand Vestibule |
| ![Great hall](docs/screenshots/hall_colossus.png) | ![Chasm](docs/screenshots/chasm.png) |
| A great hall: one layer high, its staircase standing in it like a tower | A chasm: the rare place where two layers meet in the open |
| ![Layers from above](docs/screenshots/layers.png) | ![Map of a layer](docs/screenshots/map.png) |
| Two layers from above (gate level, one below): rooms and corridors on one floor each | The map shows your layer and the stairs you have seen |
| ![Heaven](docs/screenshots/heaven.png) | ![Ember-forged dungeon](docs/screenshots/ember.png) |
| Heaven (far up) | Ember-forged dungeon (far south) |
| ![Jade dungeon](docs/screenshots/jade.png) | ![Gothic dungeon](docs/screenshots/gothic.png) |
| Jade dungeon (far east) | Gothic dungeon (west) |
| ![Combat](docs/screenshots/combat.png) | ![Combat in the mines](docs/screenshots/combat_mines.png) |
| A pack with an elite brute in the vestibule | A brute in a mine tunnel |
| ![Skill tree](docs/screenshots/skills.png) | ![Map](docs/screenshots/map.png) |
| Classless skill tree (TAB) | Map of explored cells (M) |
| ![Two players](docs/screenshots/coop2.png) | ![Three players](docs/screenshots/coop3.png) |
| Split screen with two players | Three players and the roster |
| ![Start screen](docs/screenshots/start.png) | ![Bestiary](docs/screenshots/bestiary.png) |
| Choose your world's seed | Crawler, brute, wisp, horned, skeleton, slime, bat |
| ![Inventory](docs/screenshots/inventory.png) | ![Spellbook](docs/screenshots/spellbook.png) |
| Inventory (I) | Spellbook (B): 8 elements x 11 forms + 7 utility spells |
| ![Loot](docs/screenshots/loot.png) | ![Spells](docs/screenshots/spells_cast.png) |
| A chest, a weapon, a scroll, a relic, gold and potions | Firestorm, Fireball and Chain Lightning at a pack |
| ![Motive](docs/screenshots/story_motive.png) | ![Story](docs/screenshots/story_story.png) |
| Every character picks a motive | The STORY tab (L): chapter, target, missions, lore |
| ![Reading](docs/screenshots/story_read.png) | ![Named enemy](docs/screenshots/story_named.png) |
| An inscription and the tracker | A chapter target with its name plate |
| ![Golden thread](docs/screenshots/golden_thread.png) | ![Vault](docs/screenshots/vault.png) |
| The Golden Thread leading to the next target (G) | A sealed vault: lamps over the gate, a hoard behind it |
| ![Light beam puzzle](docs/screenshots/puzzle_beam.png) | ![Statue puzzle](docs/screenshots/puzzle_statues.png) |
| A light beam puzzle: turn the mirrors toward the lock | The watchers must look at the gate |
| ![Spike trap](docs/screenshots/trap_spikes.png) | ![Flame vent](docs/screenshots/trap_flames.png) |
| Spikes stab every few seconds | A flame vent |

## Build

Requirements: a C11 compiler, CMake 3.16+, and OpenGL 3.3 drivers. SDL3 is used from the
system if CMake can find it; otherwise it is downloaded and built automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/fortress_crawl
```

On Linux, building SDL3 from source needs the X11 and GL development packages. On
Ubuntu (also inside WSL):

```bash
sudo apt install libx11-dev libxext-dev libxi-dev libxfixes-dev libxrandr-dev libxcursor-dev libxss-dev libgl-dev libegl-dev
```

`libxi-dev` (XInput2) matters: without it SDL3 has no relative mouse mode on X11, and
CMake prints a warning. If you installed it after a first build, delete `build/` and
configure again so SDL3 is rebuilt.

### Mouse under WSL (WSLg)

WSLg passes the Windows cursor to Linux as absolute positions and cannot lock it to the
window. The game detects WSL and stops SDL from re-centring the cursor, because under
WSLg those re-centring jumps come back as motion in the opposite direction (inverted,
jittery look). The cursor is then not locked to the window; that is a WSLg limitation.
If relative mouse mode is missing entirely, the game falls back to grabbing the cursor
itself. You can override the detection with `--recenter` / `--no-recenter`, and
F7 / F8 invert the mouse's Y / X axis. For the best experience, build natively on
Windows or play the web version.

## Web version

The same code builds for the browser with Emscripten (WebGL 2, single-threaded; chunk
generation runs in time slices between frames):

```bash
source /path/to/emsdk/emsdk_env.sh     # Emscripten 4.0.10 was used
./web/build.sh                         # -> build-web/dist: index.html, index.js, index.wasm
python3 -m http.server -d build-web/dist 8000
```

Open http://localhost:8000 and click into the page to lock the mouse; Esc releases it.

### Phones and tablets

On touch screens the web version shows on-screen controls (tap to start; it tries to go
fullscreen and landscape, which iPhones do not allow, so turn the phone yourself there):

| Touch | Action |
|-------|--------|
| Left part of the screen | move stick that appears under your thumb; push it past the ring to sprint |
| Right part of the screen | drag to look |
| Big button (weapon name) | attack while held; drag from it to aim at the same time |
| ALT | alternate attack (dimmed until the weapon's last skill is learned) |
| JUMP, DUCK, WPN | jump, crouch, next weapon |
| CAST, > | cast the selected spell (hold for breath spells), next spell |
| PATH, POT, MENU, MAP (top right) | Golden Thread to your target; drink a potion; character menu (tap tabs and items, tap again to equip, use or learn, X closes); map (tap to close) |

Bows and staffs get light aim assist on touch. Touch devices use a smaller view distance
(`--radius 2`) to keep generation smooth. A Bluetooth gamepad also works in mobile browsers.
To try the touch controls on a desktop, open the page with `?touch`; `?seed=dragon` skips the
start screen, and `?args=...` passes command line options, for example `?args=--debug+--seed+42`.

### GitHub Pages

The workflow also publishes the web version to GitHub Pages on every push to the default
branch: **https://stbange.github.io/fortress_crawl/**. This needs Pages to be switched to
GitHub Actions once (repository Settings -> Pages -> Build and deployment -> Source:
*GitHub Actions*). The workflow tries to do that itself; if it is not allowed to, the `pages`
job only prints a warning until the setting is changed.

### Deployment to Cloudflare Pages

`.github/workflows/build.yml` builds the desktop version with tests and the web
version on every push. If the repository has these two secrets, it also deploys the web
version to the Cloudflare Pages project `fortress-crawl` (created on first run):

- `CLOUDFLARE_API_TOKEN`: an API token with the *Cloudflare Pages: Edit* permission
  (Cloudflare dashboard -> My Profile -> API Tokens -> Create Token)
- `CLOUDFLARE_ACCOUNT_ID`: shown on the dashboard's Workers & Pages overview

Pushes to `main` go to production (`fortress-crawl.pages.dev`); other branches get
preview URLs.

## Playing

The game starts with the **start screen**: type a seed (a number or any word; the same seed
always makes the same world) or roll a random one, and enter the fortress. `--seed` on the
command line skips it.

**Layers:** the fortress is built like the floors of a building. Each layer has one floor:
you walk through its rooms and corridors without falling into another layer, and the only
ways up and down are spiral staircases. Every chunk (32 x 32 m) has one staircase up and one
down, so the next one is never far; great halls and caverns have one of each for the whole
hall, and the rare chasm joins two layers with a long stair of its own. The map shows the
layer you are on (gate level, "layer 3 below", ...) and marks the staircases you have seen;
directions to targets are given in layers.

**Monsters** come in packs in most chunks, two packs in structures, and bats in swarms.
Skeletons shoot arrows from a distance, slimes split in two, wisps throw fire, horned beasts
charge, brutes hit hard. Elites (with a burning crown) are tougher and drop better loot.

**Loot:** monsters drop gold, health and mana potions, spell scrolls and weapons. About a
third of the chunks hold a chest (walk into it); some hide a **relic**, a collectible that
permanently adds health, damage, mana or speed. There are 12 kinds; duplicates stack.
Weapons come in 12 types in three classes: blades (dagger, sword, axe, mace, spear,
greatsword), bows (shortbow, longbow, crossbow) and staves (wand, staff, scepter). Each has
its own speed, reach, arc or projectile, a rarity (common, magic, rare, epic) and up to two
affixes such as *Burning*, *Frozen*, *Crackling*, *Venomous*, *Vampiric*, *Swift*, *Keen*
or *Heavy*. Deeper finds are stronger. Spare weapons can be scrapped for gold.

**Spells:** every spell is an element (fire burns, frost slows, lightning arcs, poison
lingers, arcane pierces, holy heals you, shadow steals life, earth stuns and knocks back)
combined with a form: bolt, barrage, nova, chain, breath, orb, trap rune, halo of orbiting
orbs, storm, meteor and aura. That makes 88 attack spells, plus Heal, Blink, Ward, Haste,
Flare, Recall and the Golden Thread. You start with Flame Bolt, Heal and the Golden Thread;
scrolls teach new spells or raise a
known one's rank (up to 5). The stronger forms only drop deeper in. The Focus skill makes all
spells stronger and cheaper.

**Stories:** every character picks a motive (revenge, glory, riches or truth). It turns the
world's endless metaplot into chapters: a named captain, then the general above it, then the
Power they serve, each farther from the gate; every kill reveals more of what the fortress
hides and points to the next. The tracker at the top shows the current target and where it is;
the map marks it in red. The **Golden Thread** (G, right stick click, PATH on touch) lays a
fading golden line along a walkable route to the target; cast it again within a few seconds
to follow your next mission instead. It only knows the part of the dungeon that is loaded, so
for far targets it leads as close as it can: follow it and cast again. On the way, the dungeon talks: inscriptions, the notes of dead
adventurers (some ask you to take something home) and bounty boards on domain lords. Standing
next to one shows its text; everything read goes into the STORY tab. Each region (domain) has
a name, a ruling faction, a past and a lord; entering one shows a banner. See
[`docs/STORY.md`](docs/STORY.md).

**Traps and vaults:** from danger level 2 on, the floor is not always safe: a loose
flagstone fires darts from the nearest wall, spike patches stab every few seconds and flame
vents roar up after a short sputter (all of them hurt monsters too). Lava burns. About one
chunk in seven hides a **vault**, a small sealed room with a portcullis and a hoard (gold,
potions, a rare or epic weapon). Stand at its gate for a hint; the lamps above it show how
far you are. Its puzzle is nearby: **levers** (each pull moves two lamps), marked **plates**
to step on in order, **statues** that must look at the gate, or a **light beam** to lead over
mirrors to a crystal lock. Work levers, statues and mirrors by attacking them. An opened
vault stays open.

**Saving:** each world (seed) has its own save with every character that played in it
(level, XP, skills, inventory, gold, potions, relics, spells, position and shrine), the chests,
relics and vault hoards already taken, the vaults opened, and the explored map near you. The game saves every 30 seconds,
when you attune a shrine, when you quit, and in the browser when the tab is hidden or closed.
The browser keeps it in `localStorage` (a few KB per world); the desktop version writes files
to the user data folder (`~/.local/share/fortress-crawl/` on Linux,
`%APPDATA%\fortress-crawl\` on Windows). The start screen suggests the world you played
last and shows what is saved; *New characters* (press twice) deletes the save of that world.
`--fresh` starts without loading.

## Controls

| Key | Action |
|-----|--------|
| WASD / mouse | move / look |
| Left / right mouse button | attack / alternate attack (unlocked by skills) |
| 1, 2, 3 or mouse wheel | blade, bow, staff |
| Q (or middle mouse button) | cast the selected spell (hold for breath spells) |
| R / Shift+R | next / previous known spell |
| H / J | drink a health / mana potion |
| G | Golden Thread: a fading line to your story target (again: next mission) |
| TAB, I, B, L | character menu: last tab, inventory, spellbook, story (Q/E or [ ] switch tabs) |
| in the menu | arrows or WASD select, Enter equips / uses / learns / makes a spell active, Del or X scraps a weapon |
| M | map of the cells you have seen |
| Space, Shift, Ctrl | jump, sprint, crouch (you duck and crawl automatically in low tunnels) |
| F | fly / noclip (Space up, Ctrl down, Shift fast) |
| T | jump 8 chunks ahead in the view direction (enables fly) |
| Page Up / Page Down | one chunk (32 m) up / down |
| F1 | debug overlay (position, danger level, chunk and monster counts) |
| F2 | internal resolution 320x180 / 480x270 / 640x360 |
| F3 / F4 | ordered dithering / PS1-style vertex snapping |
| F5 / F6 | full-bright (inspect the generator) / player torch |
| F7 / F8 | invert mouse Y / X |
| F9 | sound on / off (`--mute` starts without sound) |
| - / = | mouse sensitivity down / up |
| F12 | save `screenshot.bmp` (desktop only) |
| Esc | release the mouse (press again to quit) |

### Split screen and gamepads

Up to four players can play on one screen. Player 1 uses keyboard and mouse. Press **START**
on a gamepad to join as a new player with your own character; the screen splits in two,
then in quarters. Press **BACK** on a gamepad that has not joined to control player 1 with it
instead. Everyone can go wherever they like: the world is loaded around each player.
If a pad disconnects, its character waits until a pad presses START again.

| Gamepad | Action |
|---------|--------|
| Left stick / right stick | move / look |
| RT / LT | attack / alternate attack (bows and staffs get a little aim assist) |
| LB / RB | previous / next weapon |
| X | cast the selected spell |
| Right stick click | Golden Thread to the story target (again: next mission) |
| D-pad left / right | previous / next spell |
| D-pad up / down | health / mana potion |
| A, B | jump, crouch |
| Left stick click | sprint (until you stop) |
| Y | character menu (LB/RB tabs, D-pad or stick to select, A to equip/use/learn, X to scrap, B to close) |
| BACK | map (B to close) |

Menus only pause the game when you play alone. Kills give the killer full XP and everyone
else within 30 m half. Monsters get tougher with each extra player.

Command line:

```
fortress_crawl [--seed N] [--pos x y z] [--look yaw pitch] [--fly] [--radius n] [--vradius n]
               [--window w h] [--lowres w h] [--debug]
               [--invert-x] [--invert-y] [--sens s] [--recenter | --no-recenter]
               [--shot file.bmp] [--shot-time seconds]
               [--demo combat|sword|death|skills|map|coop|menu|inventory|spells|loot|magic|bestiary|
                       motive|story|read|named|storytest|vault|levers|plates|statues|beam|
                       darts|spikes|flames|mechtest]
               [--players n] [--test-pad] [--touch] [--fresh] [--persist] [--fullbright] [--chapter n]
```

`--shot` waits until the view has loaded, saves one frame and exits. It is useful for
comparing seeds and changes to the generator. `--demo` sets up a scene for that frame: a
level-8 character facing a monster pack (`combat`, `sword`, `death` with 1 HP, `coop` with
the other players beside you, `magic` casting three spells), a character with some loot in
the `inventory` or `spells` screen, `loot` on the floor, one monster of each kind
(`bestiary`), the start screen (`menu`), the motive question (`motive`), a story in progress
(`story` tab, `read` at an inscription, `named` facing a chapter target), or the `skills` or
`map` screen. `--demo storytest --pos 20 6 0.5 --shot-time 10` plays a scripted story run
(read a note, kill a chapter target, bring a lost item home) and logs each step.
`vault`, `levers`, `plates`, `statues` and `beam` look at the nearest vault (of that puzzle)
and `darts`, `spikes` and `flames` at the nearest trap at work; `--demo mechtest` solves the
nearest vault through the game code, opens it, takes the hoard and logs each step
(`worldmap mech` lists where they are). `--players n` starts with n
characters (gamepads take them over with START). `--test-pad` attaches a virtual gamepad
that joins, walks, turns and attacks, to test the gamepad path without hardware. `--touch`
shows the touch controls from the start.

Shrines (glowing rune stones) are respawn points: walk up to one to attune it. When you die,
you return to the last attuned shrine and keep everything.

## Sound

All sound effects are generated with the [ElevenLabs](https://elevenlabs.io) sound effects
API and committed in `assets/sfx` (mono 22 kHz WAV, about 16 MB), so building and playing need
nothing extra. The game mixes them itself on an SDL3 audio stream (`src/engine/audio.c`):

- **monsters**: an alert, attack, hurt and death sound for each of the seven kinds, plus the
  horned charge, slime hops and splits, stuns, and named enemies appearing and dying. Big
  monsters sound deeper, elites deeper still.
- **weapons**: a swing or shot for each of the 12 weapon bases, the three alternate attacks,
  and hits on flesh, bone and stone.
- **spells**: a cast and an impact sound for each of the 8 elements, the forms (meteor, rune,
  storm, aura, orbit, breath) and the 7 utility spells.
- **the world**: one ambience loop per stratum and one for outside, crossfading as you move;
  torches, lava, water and burning flame vents heard as you come close; and now and then a
  sound from somewhere in the dark (drips, chains, distant screams, a bell) that fits the place.
- **you**: footsteps by floor (stone, dirt, wood, water, ice), jumps, landings, pain, death,
  level-ups, potions, pickups, chests, shrines, menus, traps and the vault puzzles.

Sounds are positional: they fade with distance and are panned for the nearest player. The
list of sounds with their prompts is `tools/sfx_list.mjs`; to change or add sounds, edit it
and run the generator (it needs Node.js and an ElevenLabs API key in `ELEVENLABS_API_KEY` or
in `.env.local`, which `npx -y @elevenlabs/cli@latest onboard init` writes):

```bash
npm install
npm run sfx                                        # makes every file that is missing
npm run sfx -- --only=brute_alert,loop_mines --force   # makes these again
```

It also rewrites `src/game/sfx_ids.h`, the list of sound ids the game uses.

## Tools and tests

```bash
./build/test_worldgen [seed]     # determinism, portal alignment, reachability, meshing
./build/worldmap places 0 20 places.ppm        # stratum/flavor map of one chunk layer
./build/worldmap section 0 20 section.ppm      # vertical slice through the world
./build/worldmap blocks 6 -100 -60 200 floor.ppm  # top-down block map of a world layer
./build/worldmap mech 6 [cy]                   # vaults (with a --pos to view each) and traps
```

## Code layout

```
src/core     hashing, RNG, noise, math (no dependencies)
src/engine   GL loader, shaders, bitmap font, worker threads, audio mixer
src/world    themes, texture generator, world layout, chunk generators, mesher + lighting,
             chunk streaming
src/render   low-res renderer, sky, post-processing, entity/UI batches
src/game     player controller, physics, monsters and AI, combat, skills, map, sound effects
tests, tools headless generator checks and map exporters, sound generator (tools/gen_sfx.mjs)
assets/sfx   the sound effects (generated with ElevenLabs)
```
