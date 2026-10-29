# Fortress Crawl: Development Plan (v0.3)

An endless, procedurally generated 3D dungeon crawler written in C with SDL3 and OpenGL 3.3.
The player starts in front of a giant castle built into a mountain. The gate is
**danger level 0**. Every direction (including up and down) leads deeper, and the farther you
get from the gate, the harder it gets.

v0.3 adds the combat milestone: Hexen-style combat, a classless skill tree, monsters, shrines
and a map of explored cells.

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

## 7. Status

Done:
- Engine: SDL3 window, GL 3.3 core loader, shaders, worker pool, bitmap font, batched
  renderer for entities and UI.
- World generation and lighting (sections 3 to 5): `test_worldgen` reaches 99-100% of portal
  openings from the gate across several seeds.
- First-person controller with automatic crouch and crawl, fly mode.
- Combat, monsters, shrines, death/respawn, XP and levels, skill tree, map (section 6).
- Screenshot and demo modes (`--shot`, `--demo combat|sword|death|skills|map`, `--shot-time`).

Known gaps:
- No loot yet. Weapons only improve through skills and levels.
- Monsters only melee and shoot; there are no ranged brutes, summoners or bosses yet.
- Lava and water are still harmless.
- No saving yet: the explored map, cleared chunks and the character are lost on exit.
- A few chunks still contain an opening that is only reachable from outside the chunk.
- No audio yet.

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
| M7  | Progression and loot (XP, skill tree, affixes, inventory, map screen) | XP, skill tree and map done; loot next |
| M8  | Traps and easy mechanical puzzles, vaults | |
| M9  | Character creation (gameplay), shrines, save/load per world | shrines done |
| M10 | Atmosphere: audio, particles, polish | |

---

## 9. Open questions for the next phase

1. **Loot:** Diablo-style random items with affixes (rarity colours, many drops), or rarer
   hand-crafted uniques with a few clear effects?
2. **Bosses:** a guardian in every megastructure, or rare named elites roaming the deeper
   strata?
3. **Character creation:** with a classless tree, should the start offer an *origin*
   (a starting bonus and starting weapon), or start everyone the same?
4. **Saves:** one save per world that saves automatically at shrines, or manual save slots?
