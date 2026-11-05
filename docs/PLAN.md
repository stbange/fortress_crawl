# Fortress Crawl: Development Plan (v0.8)

An endless, procedurally generated 3D dungeon crawler written in C with SDL3 and OpenGL 3.3.
The player starts in front of a giant castle built into a mountain. The gate is
**danger level 0**. Every direction (including up and down) leads deeper, and the farther you
get from the gate, the harder it gets.

v0.3 added the combat milestone: Hexen-style combat, a classless skill tree, monsters, shrines
and a map of explored cells. v0.4 added local split-screen co-op for up to four players with
gamepads (section 6.4) and touch controls (6.5). v0.5 adds a start screen with the seed choice,
denser monsters with three new kinds, loot, weapons, relics, an inventory and 95 spells
(section 6.6) and saving (6.7). v0.6 adds generated stories: motives, an endless metaplot of
named enemies, domains, inscriptions, notes and bounty boards (6.8, details in
[STORY.md](STORY.md)). v0.7 rebuilds the world in layers like the floors of a building, with
one floor height per layer and an up and a down staircase in every chunk (section 4). v0.8
adds traps, easy mechanical puzzles and vaults (section 6.9).

---

## 1. Decisions

| Topic | Decision |
|-------|----------|
| Engine | Own small engine: SDL3 + OpenGL 3.3 core. No other runtime dependencies (GL loader, math and noise are our own code). |
| Camera | First person. |
| Character | Customisation is gameplay only for now (origin/class, attributes, starting skills). |
| World extent | Endless in all 6 directions. Height and compass direction both drive the look and content (section 3). |
| Death | Respawn at the last shrine, keep levels and gear. |
| Combat | Fast, retro Hexen-style action: instant swings and projectiles, no combos. Sword, bow and staff; what you can do with each depends on your skills. |
| Progression | Classless skill tree (section 6.2). One point per level. |
| Map | Map screen in the inventory that shows only the cells you have seen. |
| Monsters | Families are chosen by stratum and tinted by flavor (my default, since this question was left open; easy to change). |
| Worlds | Fixed seed per world, several worlds (save slots) possible. No minimap. |
| Puzzles | Easy mechanical puzzles (levers, plates, rotating statues, light beams). |
| Platforms | Linux, Windows, macOS. Keyboard and mouse. |
| Assets | All textures generated in code. Sound effects generated with the ElevenLabs API (`tools/gen_sfx.mjs`), committed as WAV files. |
| First focus | World generation and lighting (done as prototype, see section 6). |

---

## 2. Design pillars

1. **Exploration first.** Every few minutes there is a new kind of space, landmark or secret.
2. **Scale you can feel.** Giant halls with galleries, colossi and chasms next to 1 m wide
   squeeze tunnels and crawlspaces.
3. **Gloomy look, fun to play.** The game is dark and oppressive to look at, but a power fantasy
   to play, with little frustration.
4. **A progression curve that feels powerful.** New abilities and visibly bigger effects, not
   just bigger numbers.
5. **Retro 3D.** Low internal resolution, 16x16 textures, quantised light, dithering, fog.

---

## 3. The world: height x direction

Every position in the world gets two independent properties:

### 3.1 Stratum: chosen by height. Controls *layout and features*.

| Chunk y (32 m) | Stratum | Character |
|---------------:|---------|-----------|
| 31 and up | **Astral Void** | open space, floating platforms and crystals, stars |
| 21 to 30 | **Heaven** | bright marble, floating islands with little temples |
| 14 to 20 | **Cloud Cloisters** | mostly open air, bridges, pale haze |
| 8 to 13 | **Spires** | towers and open-air bridges, night sky |
| 3 to 7 | **Castle** | built halls, arenas, great halls with galleries |
| -3 to 2 | **Dungeon** | rooms, corridors, cell blocks (start area) |
| -9 to -4 | **Mines** | timbered tunnels, lanterns, ore veins, chasms |
| -16 to -10 | **Roots of the Mountain** | caves, giant caverns, roots, glowing fungus |
| -25 to -17 | **Limbo** (first circle) | grey, bones, cell blocks |
| -39 to -26 | **Inferno** | lava, red light, chasms |
| -40 and below | **World Core** | magenta crystal caves, molten core |

Each stratum has weights for the chunk archetypes and megastructures, plus feature
frequencies (lava, crystals, fungus, roots), fog, ambient light and light colour.
Boundaries are warped with noise so they are irregular.

### 3.2 Flavor: chosen by compass direction. Controls *look*.

| Direction | Tier 1 (from ~4 chunks) | Tier 2 (~10) | Tier 3 (~18) |
|-----------|------------------------|--------------|--------------|
| North (colder) | Frostbitten | Glacial | Everwinter (aurora light) |
| South (hotter) | Sandstone | Sunscorched | Ember-forged (lava) |
| East | Boyar (carved wood, red and gold) | Steppe (felt, earth, bones) | Jade (red lacquer, jade, lanterns) |
| West | Gothic (stained glass) | Druidic (moss, roots, water) | Drowned (bioluminescent, flooded) |

A flavor controls the palette and wall/floor patterns, torch and lantern colours, fog colour,
typical decoration (cobwebs, bones, roots, lanterns, chains) and fluids (ice, water, lava).
On diagonals, the dominant axis wins; near the diagonal it is chosen per 3x3x3 region,
which gives patchy borders.

**Stratum x flavor = theme.** There are 11 x 13 = 143 combinations, for example
*Frostbitten Mines*, *Jade Heaven*, *Drowned Inferno* or *Everwinter Inferno* (a frozen hell,
like Dante's Cocytus). Each theme gets its own generated texture set.

### 3.3 Danger level

`danger = sqrt(dx^2 + dz^2) + 1.5 * |dy|` in chunks from the gate. Today it already reduces
light density (the deeper you go, the darker it gets) and increases crawlspace frequency.
Later it will drive monsters, traps, loot and puzzles.

---

## 4. Generation technique

- **Chunks** are 32^3 cells of 1 m. Every chunk is a pure function of (seed, coordinates), so
  generation needs no stored history, runs in any order, and runs on worker threads.
- **Layers (v0.7):** the world is built like the floors of a building. Each chunk row in y
  is one **layer**, and every layer walks on one main floor, `LAYER_FLOOR` (6) cells above the
  bottom of its chunks, so the ground outside the gate is layer 0. Corridors run flat on that
  floor and are routed with A* (they avoid stairwells and reuse existing passages); caverns
  add slopes of a few cells but never lead to another layer.
- **Staircases:** layers connect only by spiral staircases (5x5 around a pillar, 32 steps).
  Every chunk has one up and one down: the stair on the face between two layers rises from
  the lower chunk's floor through its ceiling and comes out of the upper chunk's floor. The
  two stairs of a chunk use separate zones, so they never collide. A great hall or cavern has
  one up and one down for the whole structure (in opposite corners); a chasm has its own.
  The positions are pure functions too: the map and the Golden Thread know where they are.
- **Portals:** the openings on the face between two chunks are computed from that face's own
  hash, so both neighbours carve them identically, always at the layer's floor. Mostly
  connected: about one chunk side in twelve is a wall. Types: corridor, narrow (1 x 2 m
  squeeze), crawl (1 x 1 m), grand (7 x 10 m arch), gate. Crawl-heavy chunks prefer crawl
  portals, structures prefer grand arches, and a crawl tunnel opening into a great hall is a
  deliberate contrast.
- **Chunk archetypes:** rooms, crawlspace, caves, mine, cell block, arena, open void (islands
  over a glass floor, so nobody falls into the layer below).
- **Megastructures**, at most one per 6x6x6-chunk cell, spanning several chunks:
  - **Great Hall** (64-96 m wide, one layer, 22 m to the vault): pillar rows, vaulted
    ceiling, tall glowing windows, braziers, chandeliers, and its two staircases standing in
    the hall like towers. Variants: *Colossus* (a 21 m statue with glowing eyes and a sword)
    and *Cistern* (flooded floor with walkways).
  - **Cavern** (up to 96 m wide, one layer): noise-carved dome cut by the layer's floor, with
    gentle slopes, columns, stalactites, ponds, crystals and fungus.
  - **Chasm** (rare showpiece, two layers): ledges on both layers' floors, a 32-step straight
    stair along one wall between them, wooden bridges, braziers, and lava or water below.
  - **Grand Vestibule**: the fixed hall behind the gate, on layer 0, with its stairs up and
    down.
- **Outside:** the valley in front of the gate with mountain walls, the castle front (curtain
  wall, gatehouse, towers with lit windows and conical roofs), dead trees, a graveyard, a
  shrine and a brazier-lined path. The castle sits against a sheer cliff that rises into the
  dark.

## 5. Lighting and rendering

- **Coloured flood-fill light** (RGB, 0-15 per channel) computed per chunk with a 15-cell
  border taken from neighbouring chunks, so light crosses chunk borders without seams. Walls
  block it naturally, so there is no light bleeding.
- A separate **fire channel** marks how much light comes from flames; the shader makes exactly
  that part flicker.
- Smooth per-vertex light plus **ambient occlusion**. Emissive blocks (lava, crystals, windows,
  flames) are full-bright and cut through the fog.
- **Moonlight** on open-sky cells outside, with a starfield and a moon in the sky shader.
- **Player torch** as a dynamic point light with flicker.
- **Adaptive fog**: rays measure how open the space is. Fog thins in giant halls so you see
  how far they reach, and thickens in tunnels.
- **Retro pipeline**: render at 480x270 (or 320x180 / 640x360), nearest upscale, light
  quantised into 14 bands, 4x4 Bayer dithering to 20 levels per channel, vignette, optional
  PS1 vertex snapping.

---

## 6. Combat and progression (v0.3)

### 6.1 Combat

- **Weapons** (switch with 1-3 or the mouse wheel; LMB attacks, RMB is the alternate attack):

  | Weapon | Primary | Alternate (needs the tier-4 skill) |
  |--------|---------|------------------------------------|
  | Sword | Hits the nearest foe in an 80° arc, 2.7 m reach, knockback | **Whirlwind**: 360° strike, 12 mana |
  | Bow | Arrow with a slight drop | **Arrow Storm**: 9-arrow volley, 15 mana |
  | Staff | Magic bolt (6 mana), lights up corridors as it flies | **Nova**: ring of 20 fire bolts, 25 mana |

- **Monsters** are coloured primitives with glowing eyes:

  | Monster | Shape and colour | Behaviour |
  |---------|------------------|-----------|
  | Crawler | small ochre box | fast, weak, comes in packs |
  | Brute | tall red humanoid | slow, hard-hitting, barely knocked back |
  | Wisp | floating blue octahedron | keeps its distance and throws fireballs |
  | Horned | green quadruped with horns | charges in a straight line and is stunned if it hits a wall |

- **Elites** have 3x health and a burning crown, and are worth 4x XP.
- **Scaling:** health +18% and damage +10% per danger level.
- **AI:** monsters sleep until they see you or hear their pack. They then follow a flow field
  (a breadth-first search over walkable cells around you), so they come round corners, up
  stairs and down drops. Brutes cannot fit through crawlspaces.
- **Light:** monsters and particles are lit by the same coloured light as the walls. Spell
  bolts and explosions are dynamic lights (up to 8) that light the dungeon around them.
- **Spawning:** chunk by chunk around the player and deterministic from the chunk seed. About
  half the chunks hold a pack; megastructures almost always do. Far-away monsters are put back
  to sleep and their chunk is repopulated when you return. Chunks you cleared stay cleared.
- **Low frustration:** health regenerates after 4 s out of combat and mana regenerates
  constantly. Levelling up fully heals you.
- **Death:** you respawn at the last **shrine** after 3 s with nothing lost. Monsters waiting
  at the shrine are cleared and you get 2 s of invulnerability. Shrines are glowing
  rune stones: one at the start, one in the vestibule, and roughly one room in five or six.
  Walking up to one attunes it.

### 6.2 Classless skill tree (TAB)

| Blade | Bow | Arcane | Survival |
|-------|-----|--------|----------|
| Keen Edge (+30% dmg, 3 ranks) | Steady Aim (+30% dmg, 3) | Focus (+30% dmg, -15% cost, 3) | Toughness (+25 HP, 3) |
| Cleave (hit everything in 150°) | Multishot (3 arrows) | Fireball (bolts explode) | Fleet Foot (+12% speed, 2) |
| Bloodthirst (heal per hit, 2) | Piercing (through 3 foes) | Chain Lightning (arcs to 2 more) | Second Wind (faster regen, 2) |
| Whirlwind (sword RMB) | Arrow Storm (bow RMB) | Nova (staff RMB) | Last Stand (half damage below 30% HP) |

A tier needs one rank in the tier above it. Levels also give +12 health, +6 mana and +7%
damage each, so you clearly outgrow areas you have already cleared.

### 6.3 Map (M)

Every 0.1 s, 720 rays from the eye mark the cells they pass and the wall they stop at. The
map shows a 110 x 110-cell area around you, top-down with north up. It shows the walkable
cells you have seen within a few metres of your height (brighter = your level, darker = lower
down), walls, water and light sources.

### 6.4 Split-screen co-op (v0.4)

- Up to four local players, each a **separate character** (own level, skills, weapon, HP,
  shrine). Player 1 uses keyboard and mouse; any gamepad joins with **START**. **BACK** on an
  idle pad gives it to player 1 instead.
- **Free roam:** nobody is tied to anyone. Chunk streaming keeps the world loaded around every
  player (the chunk map is a pool with a hash index and up to four streaming centres; chunks
  far from all players are evicted).
- Layout: 1 player full screen, 2 side by side, 3 or 4 in quarters (with 3, the spare
  quarter shows a roster). Each view has its own atmosphere, HUD, menus and map; the views
  get a frame in the player's colour, and the HUD becomes compact in small views.
- Monsters chase the nearest living player (with some stickiness) and turn on whoever hits
  them. Every player has their own flow field. Monster HP grows by 50% per extra player.
- The killer gets the full XP; other players within 30 m get half.
- Gamepad players get light aim assist for arrows and bolts (6 degree cone, 40 m, line of
  sight) and rumble when hit.
- Menus only pause the game in single player.
- Other players are shown as figures in their colour with a marker above the head; their
  torches light your view too.
- The explored map is shared and shows everyone's position.

### 6.5 Touch controls (web on phones and tablets)

- Floating move stick on the left (sprint when pushed past the ring), drag to look on the
  right, buttons for attack (hold; dragging from it also aims), alternate attack, jump,
  crouch and weapon, and SKILL / MAP at the top. Menus are tapped directly.
- The low-res target takes the screen's aspect ratio on touch screens (no letterbox), the
  page goes fullscreen/landscape where allowed and asks to rotate in portrait.
- Touch players get the gamepad aim assist; touch devices stream with radius 2.
- The controls drive player 1, so a phone plus Bluetooth gamepads also gives split screen.

### 6.6 Seeds, monsters, loot and spells (v0.5)

- **Start screen:** the world is only generated after the player picks a seed (a number, or
  a word that is hashed). Keyboard typing, a gamepad (D-pad steps through neighbouring
  seeds), or a touch keypad; `--seed` / `?seed=` skip it.
- **Monsters:** 75% of the early chunks and 80% of the deeper ones hold a pack of 3 to 10
  (scaling with danger), structures often a second pack; the cap is now 320. New kinds:
  skeleton archers (keep their distance, shoot), slimes (hop, split in two when killed) and
  bats (fast swarms). Elements leave status effects: burning, chill (slower), poison, stun.
- **Loot:** drops of gold, potions, scrolls and weapons (elites drop much more), chests in
  about a third of the chunks, relics in one in twelve. Taken chests and relics are
  remembered for the session. Pickups and placed loot vanish when nobody is near and come
  back with their chunk.
- **Weapons:** 12 bases in the three classes, each with its own damage, speed, reach, arc,
  knockback, projectile speed, piercing or mana cost. Rarity (common to epic) adds damage
  and up to two affixes; the elemental affixes add that element's effect to every hit.
  Item level follows the danger level where it dropped.
- **Inventory:** three equipped weapons (one per class), an 18-slot bag, potions, gold and
  the relic collection in a tabbed character menu (items, spells, skills).
- **Spells:** 8 elements x 11 forms + 7 utility spells. Damage grows with level, the Focus
  skill, the spell's rank (up to 5) and damage relics. Scroll drops respect a tier per form,
  so meteors, storms and auras only appear deeper in.

### 6.7 Saving (v0.5)

One save per world, keyed by the seed: all characters (stats, skills, inventory, spells,
relics, position, shrine), the taken chests and relics, and up to about 1.2 MB of explored
map (nearest chunks first, stored as a byte mask plus the non-zero bytes). A small versioned
binary format with counted arrays, so later versions can read older saves. Browser:
`localStorage`, base64; desktop: a file in SDL's preference path, written via a temporary
file and a rename. Autosave every 30 s, on shrine attunement, on quit and when the browser
tab is hidden or closed.

### 6.8 Stories (v0.6, steps S1 and S2)

Stories are pure functions of seed and position like the blocks (`src/world/story.c`), so
they cost nothing to keep and any place can refer to any other. World myth per seed; arcs of
three chapters (captain, general, Power) leading farther out in one direction each, with the
Power's death pointing to the next arc; domains with factions, states, eras and lords; traces
(inscriptions, dead adventurers' notes, bounty boards) about once per three chunks. Each
character picks a motive that frames the chapters and decides the reward. Missions (bounty,
retrieve) come from traces, not people. Full design and the next steps (camps and people,
consequences, choices): [STORY.md](STORY.md).

### 6.9 Traps, puzzles and vaults (v0.8)

The generator places them like everything else, from seed and position
(`src/world/gen_mech.c`), and records them in the chunk info; the game builds working
devices for the chunks near the players (`src/game/mech.c`). The rules both sides share are
in `src/world/mech.h`.

- **Vaults** (about one chunk in seven): a 3x3 room cut into solid rock beside a floor, with
  a gilded frame and a portcullis facing the floor, and a hoard inside (gold, potions, a
  rare or epic weapon). Lamps over the gate show the puzzle's progress. Standing at the gate
  shows a one-line hint. Solving the puzzle raises the portcullis for good: solved vaults
  and taken hoards are saved with the world.
- **Puzzles** (one per vault, its parts on the floor reachable from the gate, 3 to 18 steps
  away):
  - *levers* (3 or 4 on walls): each pull flips its own lamp and the next one; light them all.
  - *plates* (3 or 4 marked stones): step on them in the order of their marks. A wrong stone
    puts the lamps out and spits fire.
  - *statues* (3): the watchers must look at the gate; a strike turns one a quarter.
  - *light beam*: a crystal casts light at a mirror (one or two mirrors); a strike turns a
    mirror an eighth, and the light goes where it faces. Light on the crystal lock opens it.
  Levers, statues and mirrors are worked by attacking them (any weapon, any input); a prompt
  appears when one is in reach. The generator only keeps puzzles that can be solved and do
  not start solved, and solid parts (statues, mirrors, the crystal) never cut a floor in two.
  `test_worldgen` checks all of this, and `--demo mechtest` solves a vault through the game
  code (strike, step, open, take the hoard).
- **Traps** (from danger level 2, in about a third of the chunks, one to three): *darts*
  (a loose flagstone fires three darts from a slit in the nearest wall), *spikes* (a 2x2 or
  3x3 patch that rattles, then stabs, every 3.2 s) and *flame vents* (sputter, then a column
  of fire, every 4 s). They hurt monsters too, and their damage grows with the danger level.
  Darts are fired by anyone stepping on the plate, monsters included.
- **Lava** burns anyone standing on it.

## 7. Status

Done:
- Engine: SDL3 window, GL 3.3 core loader, shaders, worker pool, bitmap font, batched
  renderer for entities and UI.
- World generation and lighting (sections 3 to 5): `test_worldgen` reaches 99-100% of portal
  openings from the gate across several seeds.
- First-person controller with automatic crouch and crawl, fly mode.
- Combat, monsters, shrines, death/respawn, XP and levels, skill tree, map (section 6).
- Split-screen co-op for up to four players with gamepads (section 6.4).
- Touch controls for the web version on phones and tablets (section 6.5).
- Start screen with seed choice; loot, weapons, relics, inventory, spells; more monsters (6.6).
- Saving per world (6.7).
- Stories S1 + S2: motives, metaplot chapters, named enemies, domains, traces, missions (6.8).
- Traps, puzzles (levers, plates, statues, light beams) and vaults; lava burns (6.9).
- Screenshot and demo modes (`--shot`, `--demo combat|sword|death|skills|map|coop|menu|
  inventory|spells|loot|magic|bestiary|motive|story|read|named|storytest|vault|levers|plates|
  statues|beam|darts|spikes|flames|mechtest`, `--shot-time`,
  `--players n`, `--test-pad` with a scripted virtual gamepad).

Known gaps:
- Gold has no use yet besides scrapping value (a shrine shop would fit).
- No armour, rings or amulets yet; spells have no visual for every element/form pairing
  beyond colour.
- No summoners yet; named enemies are scaled-up monsters with a name, not bosses with their own attacks.
- Water is still harmless; traps cannot be spotted or disarmed by a skill yet.
- Puzzles are not yet mission steps (S5), and a half-solved puzzle resets when everyone
  leaves.
- Saves do not keep killed monsters, dropped loot lying around or spell effects; a world
  you return to is repopulated.
- A few chunks still contain an opening that is only reachable from outside the chunk.
- No music yet, only sound effects and ambience. Sounds are not muffled behind walls.
- Split screen draws the world once per view, so four views cost about four times the GPU
  and draw-call time of one. Gamepad buttons cannot be remapped yet.
- Touch controls are tested in an emulated phone browser only; feel and real-device speed
  still need checking (the web build generates chunks on the main thread).

## 8. Milestones

| #   | Milestone | Status |
|-----|-----------|--------|
| M0  | Skeleton (CMake, SDL3, GL, loop) | done |
| M1  | Retro renderer (low-res, fog, torch, dithering, font) | done |
| M2  | Player controller (collision, crouch/crawl, step-up) | done (basic) |
| M3  | Chunks and streaming (threads, meshing, culling) | done |
| M4  | World gen v1 (portals, levels, stairwells, castle entrance) | done |
| M5  | World gen v2 (strata x flavors, megastructures) | first version done |
| M6  | Combat and monsters (coloured primitives, AI, melee/magic/bow) | first version done |
| M7  | Progression and loot (XP, skill tree, affixes, inventory, map screen) | first version done |
| M8  | Traps and easy mechanical puzzles, vaults | first version done |
| M9  | Character creation (gameplay), shrines, save/load per world | shrines done |
| M10 | Atmosphere: audio, particles, polish | sound effects and ambience done |
| M11 | Local split-screen co-op (up to 4, gamepads) | first version done |
| M12 | Stories: S1 information, S2 motives, chapters, missions | done |
| M13 | Stories: S3 camps and people, S4 consequences, S5 choices | |

---

## 9. Open questions for the next phase

1. **Loot:** Diablo-style random items with affixes (rarity colours, many drops), or rarer
   hand-crafted uniques with a few clear effects?
2. **Bosses:** a guardian in every megastructure, or rare named elites roaming the deeper
   strata?
3. **Character creation:** with a classless tree, should the start offer an *origin*
   (a starting bonus and starting weapon), or start everyone the same?
4. **Saves:** one save per world that saves automatically at shrines, or manual save slots?
