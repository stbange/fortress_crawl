# Fortress Crawl: Breakable Blocks

Blocks are made of 4x4x4 pieces of 25 cm. A pickaxe, bombs and bursting bloaters break them,
loose pieces fall, and what was broken stays broken. This is the first part of the world
simulation; fluids, fire and gases, and living things build on it (see the plan in
[PLAN.md](PLAN.md)).

Status: built (save version 15).

Code: `src/world/carve.h` (the pieces), `src/world/chunkmap.c` (edits, `chunkmap_edit`),
`src/world/mesher.c` (`emit_pieces`), `src/game/physics.c` (collision by pieces),
`src/game/dig.c` (pickaxe, bombs, explosions, falling islands).

## Blocks and pieces

- A block is still one byte per 1 m cell. The top bit (`BLOCK_CARVED`) marks a block that has
  been broken into; the low bits stay its material.
- A carved block's pieces are a 64-bit mask, bit `x + z*4 + y*16`, kept in a sorted array per
  chunk (`Chunk.carve`). A whole block has no entry.
- A carved block is not opaque: light passes, and the mesher draws its pieces one by one, with
  the matching quarter of the block's texture, so a broken wall still looks like that wall.
- Collision checks pieces, and the player snaps to 25 cm steps, so you can stand on a ledge of
  pieces or climb a half-broken step. Rays (sight, arrows, spells) step through carved cells
  finely.

## What breaks what

| Hardness | Materials | Broken by |
|----------|-----------|-----------|
| soft | wood, earth, ice, glass, webs, roots, fungus, bones, grass, torches, lanterns | anything: a pickaxe, bombs, bloaters |
| rock | rock, ore, crystal | a pickaxe or bombs |
| masonry | walls, floors, ceilings, pillars, ornament, roofs | bombs |
| metal | iron, braziers, chains, bars | nothing the player has |
| never | lava, water, shrines | |

Layer floors are masonry: going down without the stairs takes a bomb. Hitting masonry with the
pickaxe says so ("MASONRY - THIS NEEDS A BOMB").

**Protected:** the Sanctum (the whole pocket realm), the outside of the fortress, a box around
each vault (its puzzle must stay solvable), shrines and the stairwells between layers. Story
sites are not protected.

## Tools

- **The pickaxe** is the fourth slot: key 4, the mouse wheel, the pad's shoulder buttons or the
  touch WPN button. Attack swings it: a monster in front is hit, else plants and webs in the
  way are cut, else the pieces within a small sphere at the point aimed at break (0.5 m in soft
  stuff, 0.3 m in rock, a little more for each pick level). Every character starts with an iron
  pick; the shop upgrades it to steel (400 gold) and mithril (1500).
- **Bombs:** the alternate attack with the pickaxe out, or X at any time. They bounce, burn for
  2.2 s and explode: radius 2.6 m, 50 + 10 per level damage with knockback to monsters and
  players alike, and every piece up to masonry in the sphere breaks. Characters start with two;
  the shop sells three for 90 gold.
- **Bloaters** bursting break soft blocks around them.

## Falling and dropping

- A block left with six pieces or fewer crumbles.
- After every break, the pieces around are flood-filled: an island of up to 48 cells that no
  longer touches anything anchored falls apart (dust and a thud). Bigger structures hold.
- Ore drops what mining at the forge would give (`forge_mine`); fungus, roots and grass drop
  herbs, mushrooms, glowcaps or tubers.
- Plants and props on top of a broken block fall with it.

## Saving

Every lasting change is recorded per chunk (`BlockEdit`: cell, raw block, pieces, fluid level)
and laid over the generated blocks whenever the chunk is generated again, so changes survive
the chunk being unloaded and the game being closed. At most 60000 changed cells are kept; past
that the oldest chunk's changes are forgotten and it comes back as generated. Broken blocks do
not heal.

Save version 15 adds each character's pick level and bombs, and the world's edits after the
v14 deeds. Older saves load with an iron pick, two bombs and an untouched world.

## Tests and tools

- `test_worldgen` section 6b: a body lands on the pieces of a broken block at their height, is
  stopped by them sideways, passes over them where they are gone, and rays hit only the pieces
  that are left.
- `--demo dig` (a tunnel into the mines' rock) and `--demo blast` (a bomb in the vestibule);
  `tools/screenshots.sh dig blast`.
- With `--persist` the log reports the changed blocks saved and restored ("Saved the world 4711
  (59 changed blocks)").

## Not done

- Bots dig and blast only when stuck (docs/SIMULATION.md, Bots), never to take a shortcut.
- Broken metal, and blocks breaking under their own weight beyond the 48-cell islands (no
  structural stress).
- Monsters do not dig to reach you.
