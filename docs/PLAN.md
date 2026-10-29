# Fortress Crawl: Development Plan (Draft v0.1)

An endless, procedurally generated 3D dungeon crawler written in C, using SDL and OpenGL.
The player starts in front of a huge fortress built into a mountain. The entrance is
**difficulty 0**. Every direction except back outside leads deeper, and the farther you
get from the entrance, the harder the content becomes.

> Status: draft. Open questions are listed at the end and marked **[Q]** throughout.

---

## 1. Design pillars

1. **Exploration first.** World generation is the core feature. Every few minutes the
   player should find something new: a different space, a landmark, a secret or a puzzle.
2. **Scale you can feel.** Huge halls make the player feel tiny, and crawlspaces make
   them feel trapped. The engine and the generator are built around this contrast.
3. **Gloomy look, fun to play.** The visuals are dark and oppressive, but the game is a
   power fantasy: lots of monsters to kill, fast pacing, and failure that costs little.
4. **A progression curve that feels powerful.** The player clearly outscales earlier zones
   and visibly gains new abilities, not only bigger numbers.
5. **Retro 3D.** Low internal resolution, nearest-neighbour textures, a limited palette,
   heavy fog and flickering light.

---

## 2. Technology stack

No game engine and no framework. We write a small engine of our own and only pull in
focused libraries that come as single headers or small libraries.

| Concern            | Choice                                   | Why |
|--------------------|------------------------------------------|-----|
| Language           | C11 (C99-compatible where practical)     | As requested |
| Window/input/audio | **SDL3** (fallback: SDL2)                | Cross-platform; SDL3 is stable and has a better API |
| Graphics           | **OpenGL 3.3 Core**                      | Runs nearly everywhere and is simple enough for a retro look |
| GL loader          | glad (generated, vendored)               | Standard |
| Math               | cglm                                     | Pure C, SIMD-friendly, header-only option |
| Images             | stb_image, stb_image_write (screenshots) | Single header |
| Noise              | FastNoiseLite (C99 header)               | Fast 2D/3D noise for caverns and the mountain |
| Font/UI            | Custom bitmap-font renderer + small immediate-mode UI | Pixel-perfect retro UI; Nuklear stays as a fallback for debug tools |
| Audio              | SDL audio + miniaudio (or SDL_mixer)     | Positional sounds and reverb per space |
| Build              | CMake, with SDL fetched through FetchContent or found on the system | Simple CI on Linux/Windows/macOS |
| Tests              | Small custom test runner for deterministic world gen | Same seed must give the same world |

**Why not an existing engine like raylib or sokol?** raylib is a good C library, but it
hides the GL pipeline and defaults to GLFW. You asked for SDL and full control, so the
recommendation is a thin engine of our own (roughly 3 to 5k lines) that we fully
understand. **[Q1]**

### Engine modules (`src/engine/`)

- `platform`: SDL window, GL context, timing with a fixed simulation tick (60 Hz) and
  interpolated rendering, input mapping (keyboard/mouse plus gamepad).
- `gfx`: shaders, meshes (VAO/VBO), texture atlas, low-res render target, post-processing,
  debug lines.
- `scene`: camera, frustum culling, chunk mesh registry, dynamic entities.
- `phys`: AABB/capsule collision against the voxel grid, raycasts, simple triggers.
- `audio`: sound bank, 3D panning, reverb zones tied to room size.
- `ui`: bitmap font, panels, menus, HUD.
- `core`: arena allocators, hash maps, PRNG (PCG32 / SplitMix64), job system for
  background chunk generation (SDL threads).

---

## 3. Rendering: "retro, pixelated, gloomy"

- Render into a **low-resolution off-screen framebuffer** (for example 480×270 or 320×180,
  configurable) and upscale with nearest-neighbour filtering to the window.
- **Textures:** 16×16 or 32×32 pixel textures in one atlas, `GL_NEAREST`, no mipmaps
  (or mipmaps with a nearest filter).
- **Optional PS1 effects** (each one can be toggled): vertex snapping, affine texture
  warp, colour quantisation to a palette, ordered (Bayer) dithering.
- **Lighting:** very low ambient light. The player carries a torch or lantern, which is a
  flickering point light. Braziers, glowing runes and monster eyes add a few more lights
  (forward rendering with a small fixed light count per chunk, around 8 to 16). Light is
  sampled per vertex or per pixel and then quantised into bands for the retro look.
- **Fog:** exponential, dark, with a colour that depends on the zone (blue-black crypts,
  red-black forges, green-black sewers). In giant halls the fog thins so the player can
  see how far the space reaches.
- **Scale tools:** FOV changes slightly with the space, the camera lowers when the player
  crouches, footsteps echo, dust particles drift in large halls, and tall vertical
  elements such as pillars, chains and statues give the eye a reference.
- **Monsters (for now):** coloured primitives (cubes, spheres, cones, capsules) with
  emissive eyes, palette-coded by family and tier.

---

## 4. World structure and generation (the core)

### 4.1 Coordinate system and determinism

- The world is a 3D grid of **chunks**. Suggested size: 32×32×32 cells, with one cell
  being 1 m.
- A global `world_seed`. Each chunk gets its own seed from
  `hash(world_seed, cx, cy, cz)`, so generation is deterministic, needs no stored
  history, and any chunk can be generated in any order and on any thread.
- **Entrance = chunk (0, 0, 0).** The castle facade and the mountain lie in the −Z
  half-space. That half is "outside" and is sealed off beyond the courtyard. Everything
  else is endless.
- **Difficulty** `D = f(distance)`. The distance is a weighted 3D distance that may count
  depth extra, for example `sqrt(dx² + dz²) + 1.5·|dy|`. The value is smoothed and
  quantised into **tiers**, and the zone banners make it visible ("Depth 7 – The Drowned
  Archive"). **[Q4]**

### 4.2 Hierarchical generation (coarse to fine)

```
Region (e.g. 8×4×8 chunks)  -> theme/biome, megastructure placement, difficulty band
  Structure (1..N chunks)   -> great hall, cathedral, chasm, cistern, tower shaft, cavern
    Chunk (32³ cells)       -> rooms + corridors from a chunk archetype
      Cell (1 m voxel)      -> solid/air/material + props, lights, spawns, traps
```

1. **Region layer:** a coarse noise field plus hashing picks the theme for each region
   (crypts, barracks, forges, flooded cisterns, libraries, natural caverns inside the
   mountain, collapsed sections, ...). Themes pick palettes, textures, fog, monster
   families, trap and puzzle types.
2. **Megastructure layer:** some regions reserve a multi-chunk volume for a landmark,
   such as a **Great Hall** of 3×2×3 chunks (96 m wide, 64 m tall), a bottomless
   **Chasm** crossed by bridges, a **Vertical Shaft** with spiral stairs, an
   **Underground Lake**, or a **Colossus Chamber**. They are placed with jittered-grid
   sampling so each one is rare and far from the others.
3. **Chunk archetypes:** every chunk not taken by a structure picks an archetype that fits
   its theme and difficulty:
   - *Dungeon cells:* BSP rooms plus corridors.
   - *Crawlspace network:* tunnels 1 m wide and 1.2 to 1.5 m high that force crouching
     or crawling.
   - *Natural cavern:* 3D noise carving (FastNoiseLite) inside the mountain.
   - *Vault / puzzle room:* hand-authored template with random variations.
   - *Arena:* an open room with waves of monsters and a reward.
   - *Stair hub:* vertical connector between layers.
   - *Ruined / collapsed:* partial cave-in, the player has to climb through rubble.
4. **Hand-authored "prefab" templates** (small text or binary files) mixed with procedural
   parts give the world recognisable rooms (shrines, libraries, armouries) while the
   overall layout stays random.

### 4.3 Seamless connectivity in an endless world

The hard problem: neighbouring chunks are generated independently, but their passages
must meet.

- **Portal contract per chunk face:** the openings on a shared face come from
  `hash(world_seed, face_id)`. Both chunks compute the same openings (position, size and
  type: corridor, crawlspace, big arch or stair) without knowing anything about each
  other.
- Inside the chunk, the generator **must connect all its portals** (spanning tree plus
  some extra loops) and then fills the rest with rooms. Multi-chunk structures own their
  internal faces and only expose portals on their outer boundary.
- **Guarantees:**
  - Every face has a non-zero chance of an opening.
  - Every chunk has at least 2 openings.
  - A global "spine" (a coarse lattice of guaranteed main corridors) ensures the world
    has no closed-off pockets near the start.
- **Sealing the outside:** faces toward −Z beyond the courtyard never get portals.
- **Scale transitions:** opening types are chosen so that a crawlspace can lead into a
  great hall. These contrast moments are planned on purpose with a weighted transition
  table (narrow → huge gets a bonus).

### 4.4 Streaming

- Chunks within radius R (for example 3 to 4 horizontally and 2 vertically) are kept
  loaded. Generation and meshing run on background threads, and the mesh upload happens
  on the main thread.
- Meshing uses greedy meshing of the voxel faces plus prop meshes per chunk, and each
  chunk is culled against the view frustum.
- Chunk state the player changes (opened chests, solved puzzles, killed bosses) is stored
  as a small **delta** per chunk in the save file. Everything else is regenerated from
  the seed.

### 4.5 The starting area (hand-designed)

- An outdoor approach: a cliff path, the mountain silhouette, the giant gate, falling ash
  or snow, and the castle half swallowed by rock.
- A courtyard with the first shrine (save/respawn point), a training dummy and the
  character creation moment.
- The gate leads into a **Grand Vestibule** (a megastructure), with exits to the left,
  right, ahead, up and down: the first branching choice.

---

## 5. Gameplay

### 5.1 Camera and controls

- **First person** (recommended for the scale and claustrophobia feeling), with mouse
  look and WASD, jump, crouch, crawl, sprint and dodge. **[Q2]**

### 5.2 Combat (fun, fast, lots of slaying)

- Real-time melee (light and heavy attacks with arcs and hit-stop), ranged attacks
  (bow/crossbow) and spells (bolt, nova, chain).
- Monsters come in packs. Many weak enemies with a few elites keeps the slaughter fun.
- **Monster families** (coloured primitives for now): swarmers (small fast cubes),
  brutes (big slow boxes), casters (floating octahedra), chargers (cones), spawners
  (pillars), and per-tier **elites** with affixes (fiery, shielded, splitting, teleporting).
- **Scaling:** `HP/damage ∝ base · growth^tier`, plus more affixes and new behaviours at
  higher tiers, so difficulty changes the gameplay instead of only the stats.
- Simple AI: a state machine (idle, notice, chase, attack, flee) with pathfinding on the
  chunk's walkable grid (A* or flow fields for packs).

### 5.3 Traps

Pressure plates, swinging blades, dart walls, collapsing floors, flame jets, rolling
boulders in narrow tunnels and gas rooms. Trap density and lethality scale with
difficulty, and traps can hurt monsters too.

### 5.4 Riddles and puzzles

- Lever or rune sequences (the clues are written on walls elsewhere in the chunk),
  rotating statues to face a symbol, light-beam or brazier lighting puzzles, weight
  plates, and a "which door is lying" riddle room.
- Puzzles guard **vaults** with much better loot. They are optional, never block
  progress and scale in complexity with distance.

### 5.5 Progression ("feel powerful")

- **XP and levels:** fast early levelling, and each level gives attribute points.
- **Skill tree or talents:** active skills with visual effects that grow bigger (cleave
  turns into a whirlwind, a firebolt turns into a meteor).
- **Loot:** rarity tiers with random affixes. Item power grows with difficulty, so going
  deeper is the main motivation.
- **Power checks:** the player comes back to a low-tier area and one-shots everything.
  Tiers adjacent to your level are the "sweet spot".
- **Shrines:** sparse checkpoints and respawn points, with optional small permanent buffs.

### 5.6 Death and failure (low frustration)

Recommendation: on death, respawn at the last shrine and keep your levels and gear, but
lose some unbanked gold or XP. No permadeath by default. **[Q5]**

### 5.7 Character customisation

- **Origin / class** sets the starting skills and stats (Warrior, Ranger, Mage, or a
  classless start).
- Attribute allocation, name, and visual options (colours, helmet, cloak; these show up
  as the hands or weapon in first person, and in the inventory doll). **[Q3]**

---

## 6. Milestones

Each milestone ends in something you can play or look at.

| #   | Milestone              | Deliverable |
|-----|------------------------|-------------|
| M0  | Skeleton               | CMake project, SDL window, GL 3.3 context, fixed-tick loop, CI build |
| M1  | Retro renderer         | Low-res framebuffer and upscale, textured meshes, camera, fog, torch light, bitmap font |
| M2  | Player controller      | First-person movement, capsule-vs-voxel collision, crouch/crawl, a small test map |
| M3  | Chunks and streaming   | Seeded chunks, greedy meshing, background generation, debug overlay (chunk coordinates, difficulty) |
| M4  | World gen v1           | Portal contract, BSP rooms and corridors, guaranteed connectivity, the castle entrance |
| M5  | World gen v2: variety  | Region themes, megastructures (great hall, chasm, shaft), crawlspaces, caverns, prefabs |
| M6  | Combat and monsters    | Primitive monsters, AI, melee/ranged/spells, hit feedback, packs and elites |
| M7  | Progression and loot   | XP, levels, skills, items with affixes, inventory and HUD |
| M8  | Traps and riddles      | Trap set, 3 to 4 puzzle types, vaults |
| M9  | Character creation and saves | Creation screen, save/load (seed plus chunk deltas plus player) |
| M10 | Atmosphere pass        | Audio and reverb zones, particles, palette/dither options, lighting polish |

M0–M3 are the engine foundation. M4–M5 are the heart of the project and should take the
most time.

---

## 7. Repository layout (proposed)

```
fortress_crawl/
  CMakeLists.txt
  external/            # glad, cglm, stb, FastNoiseLite, miniaudio (vendored)
  src/
    engine/            # platform, gfx, phys, audio, ui, core
    world/             # chunk, region, structures, portals, meshing, prefabs
    game/              # player, combat, monsters, ai, items, skills, traps, puzzles
    main.c
  assets/
    shaders/ textures/ prefabs/ sounds/ fonts/
  tests/               # determinism and connectivity tests for the generator
  docs/
```

---

## 8. Open questions

See the list in the conversation / PR. Answers will be folded into v0.2 of this plan.
