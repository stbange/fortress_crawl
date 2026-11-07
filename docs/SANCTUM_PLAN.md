# The Sanctum: implementation plan

Status: **implemented** (steps 1-6 of section 5, November 2026). The Sanctum is one per
world, starts as a ruin and is built up plot by plot in the SHOP tab at home; keystones drop
from vault hoards, named enemies and bosses; the door is a ritual. Still open: the drafting
table as a thing to use (building is in the SHOP tab, anywhere in the Sanctum), the shop
counter and stash as places (both work anywhere at home), and the questions in section 6.

Decisions (November 2026):
1. **One Sanctum per world**, shared by everyone playing it (split screen now, online later).
2. **The full shop moves to the Sanctum.** Shrines in the fortress keep potions and scrapping.
3. **Building costs gold and keystones**, a rare find.
4. **Opening the door is a 3-second ritual**; a hit breaks it. Monsters nearby do not matter.

Already done: no more trapping in the fountain; a night sky with three moons and falling
stars; empty sky all round the island.

## 1. What the Sanctum is for

The place to return to between expeditions, and a record of the run:

| At home | Why come back |
|---|---|
| The bed | where you wake after a death; restores fully, saves |
| The stash | 30 places for weapons the bag has no room for (shared by the party) |
| The full shop | spell ranks, new affixes, relics; later the forge |
| The comforts | one buff at a time: Refreshed (bath), Well Fed (feast) |
| The trophy hall | every legendary found and every named enemy killed, on show |
| The drafting table | where the Sanctum grows |

The door home still returns you exactly where you left, so a trip costs time, not
progress.

**The ritual**: casting the Sanctum Door starts a 3-second circle of light round you
(you can move slowly inside it). Any hit breaks it (mana spent, a 5-second cooldown). When it
completes, the door stands for 30 seconds, as now. Spell code: `utility_cast(SU_SANCTUM)`
starts `ps->ritual_t`; `game_hurt_player` clears it; `utility_update` opens the door.

## 2. One Sanctum per world

- A new `Sanctum` record in `Game` (not `Stats`): the stage of every plot, the stash, the
  trophies (unique ids found, named enemies killed), the library's timer, keystones held.
  Saved with the world (save version 7).
- Old saves: the bath and feast flags of any character mark those plots as built; the
  island starts at the size their rooms need, so nobody loses anything.
- In split screen everyone uses the same rooms, stash and keystones; gold stays per
  character (each pays for what they build).

## 3. Growing it

### The island and its plots

The island is a grid of 4 x 4 plots of 32 x 32 m (one chunk each). The first state uses the
middle four; each ring of growth adds plots round them.

```
  . . . .        . G G .        T G G T
  . H B .   ->   L H B K   ->   L H B K
  . F D .        . F D A        W F D A
  . . . .        . . . .        T W W T
 start: hall,    ring 1         ring 2: towers (T), walls (W)
 bed, fire,      garden (G), library (L),
 door (D)        kitchen (K), armory (A)
```

(H hall with bed and fire, B bath, F forecourt with the door home, D dining.)

### Plots and their stages

| Plot | Stage 1 | Stage 2 | Stage 3 | Function |
|---|---|---|---|---|
| Island | the four middle plots, bare rock | ring 1, cliffs with ivy | ring 2, waterfalls off the edge | room for plots |
| Hall | a ruined room, bed, campfire, the drafting table | walls whole, banners, carpet | throne, chandeliers, stained glass | bed, stash, shop counter |
| Bath | a cold pool | heated, steam | a marble hall with columns | Refreshed |
| Dining hall | a table and a fire | the long table, the feast | a kitchen beside it | Well Fed (cooking later) |
| Library | empty shelves | full shelves, a shrine | a domed reading room | the daily scroll, spell ranks |
| Garden | wild grass | paths, trees, a fountain | a walled garden with a pond and flower beds | none yet (food later) |
| Armory / trophy hall | an empty wall | racks with your legendaries | banners of named enemies, a statue for every chapter finished | trophies |
| Towers and walls | ruins | standing | battlements, beacons | none (silhouette) |
| Later | forge, observatory, greenhouse | | | with their features |

Cost: stage 1 of a plot 300-800 gold; stage 2 1000-2500 gold and 1 keystone; stage 3
3000-6000 gold and 2 keystones; the island rings 1500 gold + 1 keystone and 5000 + 2.
The whole Sanctum is about 40,000 gold and 20 keystones: a long-term goal.

**Keystones** (a new pickup): every vault hoard holds one, every named enemy from danger
level 6 drops one, a boss has a 25% chance. A world gives about one every 10-15 minutes of
play.

### The drafting table

A table in the hall. Using it (the attack key in reach, like a lever) opens a list in the
SHOP tab: each plot, its next stage, its cost, a line of what changes ("Bath, stage 2: the
water steams; Refreshed lasts 20 minutes"), and what is missing. Building takes effect at
once: the chunk of that plot is generated again and the new rooms are there.

## 4. How it is built (code)

- **gen_pocket.c** reads the stages: `world->sanctum_stage[plot]` (a small array in `World`,
  set from `Game.sanctum` at load and when a stage is built). Each plot is generated by its
  own function per stage (`plot_hall(g, stage)`, ...), clipped to its chunk; the island shape
  comes from the ring. The generator stays deterministic: the same stages give the same
  rooms.
- **Regeneration**: a new `chunkmap_regenerate(map, cx, cy, cz)` sends a loaded chunk back
  to the generator and mesher (the chunk store generates and meshes on worker threads
  already; this adds a way back into that queue for a ready chunk). Edits by utility spells
  are re-applied as for any reloaded chunk. Building a stage regenerates its
  plot's chunks and their neighbours.
- **realm.c**: the bath, feast, bed, stash, shop counter, drafting table and trophy displays
  look up where their plot is and what stage it has.
- **Trophies** are drawn, not built: the legendaries in their racks with
  `weapon_draw_ground`, banners in the colours of the named enemies' cults.
- **Shop**: `shop.c` offers its full list only in the Sanctum (at the shop counter in the
  hall); a shrine in the fortress offers potions and the scrap button.

## 5. Order and effort

| Step | What | Time |
|---|---|---|
| 1 | `Game.sanctum` per world, save v7, migration; the ritual | 2 days |
| 2 | Plot grid, the generator by stages, chunk regeneration; start state (ruined hall, bed, fire, door) | 3-4 days |
| 3 | Drafting table, costs, keystones (pickup, drops, HUD) | 2 days |
| 4 | Plots: hall, bath, dining, library (3 stages each), the shop counter and stash | 3-4 days |
| 5 | Garden, armory and trophy hall, island rings, towers and walls | 3-4 days |
| 6 | Shop moves home; playtest bot learns to build and use the stash; batch of runs | 2 days |
| | **Total** | **about 3 weeks** |

## 6. Open questions

- **Gold per character, building for all**: fine in split screen; online it may need a
  shared treasury. Decide when multiplayer comes.
- **Should the trophy hall give anything?** Purely cosmetic is the plan; a small bonus per
  ten trophies (like relics) would be possible.
- **Time of day**: the Sanctum is always night now. A slow day and night cycle could come
  later with the garden and food.
