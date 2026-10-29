# Fortress Crawl: Development Plan (v0.2)

An endless, procedurally generated 3D dungeon crawler written in C with SDL3 and OpenGL 3.3.
The player starts in front of a giant castle built into a mountain. The gate is
**danger level 0**. Every direction (including up and down) leads deeper, and the farther you
get from the gate, the harder it gets.

v0.2 includes your answers and the first prototype (world generation and lighting).

---

## 1. Decisions

| Topic | Decision |
|-------|----------|
| Engine | Own small engine: SDL3 + OpenGL 3.3 core. No other runtime dependencies (GL loader, math and noise are our own code). |
| Camera | First person. |
| Character | Customisation is gameplay only for now (origin/class, attributes, starting skills). |
| World extent | Endless in all 6 directions. Height and compass direction both drive the look and content (section 3). |
| Death | Respawn at the last shrine, keep levels and gear. |
| Combat | Fast action. Melee, magic and bows, depending on the character's skills. |
| Worlds | Fixed seed per world, several worlds (save slots) possible. No minimap; a map screen in the inventory. |
| Puzzles | Easy mechanical puzzles (levers, plates, rotating statues, light beams). |
| Platforms | Linux, Windows, macOS. Keyboard and mouse. |
| Assets | All textures generated in code. Audio decided later. |
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
- **Floor levels:** each normal chunk has one or two floor levels, from its own hash.
  Corridors run flat on a level and are routed with A* (they avoid stairwells and reuse
  existing passages). Height changes always go through **spiral stairwells**. This keeps
  everything connected and avoids corridors cutting through each other.
- **Portals:** the openings on the face between two chunks are computed from that face's own
  hash, so both neighbours carve them identically. Portal heights are taken from the
  neighbouring chunks' floor levels or from a structure's gallery levels. Types: corridor,
  narrow (1 x 2 m squeeze), crawl (1 x 1 m), grand (7 x 10 m arch), gate. Vertical
  connections are spiral **shafts** through floors and ceilings. Crawl-heavy chunks prefer
  crawl portals, structures prefer grand arches, and a crawl tunnel opening into a great hall
  is a deliberate contrast.
- **Chunk archetypes:** rooms, crawlspace, caves, mine, cell block, arena, open void.
- **Megastructures**, at most one per 6x6x6-chunk cell, spanning several chunks:
  - **Great Hall** (64-96 m wide, 64 m tall): pillar rows, vaulted ceiling, galleries on
    every level linked by stairs, tall glowing windows, braziers, chandeliers. Variants:
    *Colossus* (a 30 m statue with glowing eyes and a sword) and *Cistern* (flooded floor
    with walkways).
  - **Cavern** (up to 96 m): noise-carved, with columns, stalactites, ponds, crystals and fungus.
  - **Chasm** (up to 160 m deep): ledge rings on each level, stairs, wooden bridges,
    braziers and lava or water at the bottom.
  - **Grand Vestibule**: the fixed hall behind the gate, with a spiral stair down into the
    floor and a 60 m free-standing spiral up through the ceiling.
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

## 6. Status (prototype)

Done:
- Engine: SDL3 window, GL 3.3 core loader, shaders, worker pool, bitmap font.
- Everything in sections 3 to 5.
- Chunk streaming around the player (generation and meshing on worker threads).
- First-person walking with step-up, automatic crouch/crawl in low tunnels, fly mode.
- Debug overlay, teleport keys, screenshot mode.
- `test_worldgen`: determinism, portal alignment, reachability from the gate (99-100% of
  openings in a 7x5x7-chunk region), connectivity inside each chunk (about 96%).

Known gaps / next steps for the world:
- A few chunks still have an opening that is only reachable from outside the chunk
  (stairwell placement fails when space is tight).
- Lava and water are not yet dangerous or swimmable.
- Open-air strata can have chunks without a floor, where you can fall a long way.
- More structures: a tower shaft with a wall-hugging spiral, an underground lake, a library,
  a forge, sealed vaults (puzzle rooms), and landmark set pieces at fixed distances.
- Prefabs: hand-authored rooms (shrines, armouries, libraries) mixed into the layouts.
- Transitions: bigger, deliberate scale contrasts (a crawlspace that opens onto a gallery high
  above a hall floor).

---

## 7. Milestones

| #   | Milestone | Status |
|-----|-----------|--------|
| M0  | Skeleton (CMake, SDL3, GL, loop) | done |
| M1  | Retro renderer (low-res, fog, torch, dithering, font) | done |
| M2  | Player controller (collision, crouch/crawl, step-up) | done (basic) |
| M3  | Chunks and streaming (threads, meshing, culling) | done |
| M4  | World gen v1 (portals, levels, stairwells, castle entrance) | done |
| M5  | World gen v2 (strata x flavors, megastructures) | first version done |
| M6  | Combat and monsters (coloured primitives, AI, melee/magic/bow) | next |
| M7  | Progression and loot (XP, skill trees, affixes, inventory, map screen) | |
| M8  | Traps and easy mechanical puzzles, vaults | |
| M9  | Character creation (gameplay), shrines, save/load per world | |
| M10 | Atmosphere: audio, particles, polish | |

---

## 8. Open questions for the next phase

1. **Combat feel:** melee hit-stop and knockback like a modern action game, or more retro
   (Hexen / Heretic style)?
2. **Classes:** fixed classes (Warrior, Ranger, Mage) or a classless skill tree where
   melee, bow and magic are branches?
3. **Map:** should the map in the inventory reveal only explored cells, or whole chunks once
   entered?
4. **Monsters per theme:** should monster families follow the theme (ice creatures up north,
   demons in the Inferno), or be independent of it?
