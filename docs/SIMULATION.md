# Fortress Crawl: The World Simulation

The world follows a few simple laws of physics, chemistry and biology, cell by cell, so that
things the player does have effects nobody scripted: a blasted lake wall floods the hall
below, water poured on lava leaves rock to walk on. It is built in stages:

1. **Breakable blocks** ([DESTRUCTION.md](DESTRUCTION.md)): 25 cm pieces, pickaxe, bombs. Built.
2. **Water**: flowing, swimming, drowning, lava and fire. Built (this page).
3. **Fire, heat and gases**: planned.
4. **Living things** (plants, moss, fungus, insects, decay): planned.

Code: `src/game/sim.c`, `Chunk.fluid` and `chunkmap_edit` in `src/world/chunkmap.c`, the
surfaces in `src/world/mesher.c`, swimming in `src/game/player.c`, breath in `src/game/game.c`.

## Only what changed moves

A world of millions of cells cannot all be simulated, least of all in a browser. Generated
water lies still. Every lasting change to the world (`chunkmap_edit`: a block broken, water
moved) wakes the cell and its six neighbours, and only woken cells are simulated, up to 600
per tick at 8 ticks a second. A cell that moves water wakes its neighbours again; one that
cannot move goes back to sleep. A lake no one touches costs nothing; once its wall is broken
it drains through the hole until it is level again, and then sleeps.

## Water

- Each water cell holds 1 to 8 eighths (`Chunk.fluid`; generated water is full). The surface
  is drawn at that height, and where neighbouring levels differ the step between them shows.
- **Falling:** water moves into the cell below as far as it has room.
- **Spreading:** what is left evens out sideways, one eighth at a time toward the lowest
  neighbour, until neighbours differ by one eighth at most. A film of one eighth only runs
  toward a drop, so spilled water ends as a shallow puddle instead of spreading for ever.
- **Conserved:** water is never made or lost by flowing; a column of 27 full cells let loose
  in a hall stays 216 eighths while it spreads over 89 cells (`--demo flood`).
- **Lava:** water flowing onto or against lava cools it to rock and boils off three eighths,
  with a hiss and steam.
- **Fire:** water flowing into a fire or a torch puts it out.
- Water flows through air, fires and torches only; it is stopped by blocks, broken blocks
  (their pieces), plants and props.
- Changed water is recorded with the world's edits and saved (save version 15 already keeps
  each changed cell's level). After loading, water lies still until something wakes it.
- Nothing flows in the Sanctum.

## Swimming and breath

- In water deeper than 0.3 m you wade (75% speed); deeper than 0.9 m you swim: 55% speed, no
  falling, you float up until your eyes are at the surface, jump swims up and crouch dives. At
  an edge you climb out as if stepping up. Falling into water does not hurt.
- With your eyes under water you have 15 seconds of breath, shown as ten bubbles over the
  health bar. Then "OUT OF BREATH - GET TO THE SURFACE", and you lose 8% of your health every
  second. Breath comes back four times as fast as it went.

## Tests and tools

- `--demo flood`: a block of water let loose in the vestibule; the log follows its cells,
  volume and the cells waiting. `--cast 1` puts lava in its way, `--cast 2` drops you into a
  walled shaft of water (the log follows your height, depth and breath).

## Not done yet

- Lava does not flow; it stays as generated.
- Monsters and critters do not swim or drown, and bots do not yet avoid deep water.
- Water does not seep through the gaps in broken blocks.
