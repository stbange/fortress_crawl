# The living world: implementation plan

Status: **planned** (November 2026). Builds on the simulation that exists
([SIMULATION.md](SIMULATION.md), [DESTRUCTION.md](DESTRUCTION.md)): breakable blocks, water and
lava with levels, fire with fuel, drifting gases, growth by need, insects, decay.

Decisions (November 2026):
1. **Catch-up reaches two hours** of game time; an area left longer looks two hours older.
2. **Cold does not hurt** until there are remedies for it (warmth at fires and braziers, warm
   meals, a fur cloak, a warming draught; see step 2). It slows and chills, nothing more. Heat
   by lava hurts as it does now.
3. **A collapse does heavy damage but never kills from full health** in one go, like falls.
4. **Oil and acid** come as new fluids in step 3.
5. **New sounds are generated** with `tools/gen_sfx.mjs` (ElevenLabs) in step 4.
6. **Populations change hounds and bats too**: more where there is prey, fewer where it is
   hunted out; the dungeon's other monsters keep their danger levels.

The aim is no longer more rules but fewer, shared ones. Today water, fire and moss each have
their own special cases ("fire melts ice", "moss needs water within three cells"). The next
step gives the world a few common quantities that every rule reads (time, temperature,
wetness, material properties, load, populations), so that effects combine in ways nobody wrote
down, and makes every change visible and audible so the player can read cause and effect.

Six steps, in the order they pay off:

| Step | What | Why first |
|------|------|-----------|
| 1 | The world goes on while you are away | cheap; the biggest single effect on "alive" |
| 2 | Temperature and wetness | replaces many special cases with one field |
| 3 | A table of materials | new reactions without new code |
| 4 | Sound and sight | makes steps 1 to 3 readable |
| 5 | Weight and collapse | the destruction feels physical |
| 6 | Populations and the food chain | biology at the scale of a region |

Each step ends playable, with a demo, tests, a bot batch and its section in SIMULATION.md.
Budget for every step: the whole simulation stays under 0.5 ms a frame on the desktop build
(today 0.003 ms on average, 0.12 ms at most) and must run in the single-threaded web build.

## 1. The world goes on while you are away

**What the player sees.** Come back to a hall an hour later and the fight has left its mark:
the fire you started has burnt out to ash and grass has taken it, the bones are under fungus,
the flooded corridor has settled, moss has crept along the wet wall, the hive you robbed has
been rebuilt. A chunk you left a minute ago looks as you left it.

**How.** The simulation only runs near the players, so a chunk out of range is frozen. When it
comes back, it catches up on the time it missed, compressed:

- Keep a **last-seen time** per chunk (the world's `play_time` when it was last within the
  simulation's reach). Chunks with edits keep it in `ChunkEdits`; others in a small table of
  the last 4096 chunks seen (a chunk never visited needs no catch-up: it is as generated).
- On `on_loaded` (already called when a chunk with changes is ready), elapsed time
  `dt = play_time - last_seen`, capped at 2 hours of game time.
- **Fast processes finish at once:** fires with fuel burn out (to ash where they lay on
  something), water and lava settle (the woken cells run to rest, within a per-frame budget),
  thin lava crusts over, clouds that were there are gone.
- **Slow processes run in proportion:** growth gets `dt x rate` samples for that chunk,
  spread over the next frames (at most 2000 cells a frame), so an hour away is about an hour
  of growth. Withering likewise.
- **Populations** (step 6) advance by the same `dt`.
- A banner the first time it is noticeable, in the world's voice: "THE HALL HAS GROWN OVER
  SINCE YOU WERE HERE."

**Code.** `sim.c` (`sim_on_loaded` gets a catch-up queue), `bio.c` (`bio_catch_up(cx, cy, cz,
dt)`, the same rules on cells of one chunk), `chunkmap.c` (last-seen per chunk). Save v17: the
last-seen table.

**Tests.** `test_worldgen` cannot run the game, so a `--demo away`: the burn demo, then the
player is moved 200 m off for a simulated hour, then back; the log counts ash, grass and
fungus before and after. Determinism check: catching up an hour in one go and in two halves
gives similar counts (within 15%).

**Effort:** small to medium (2 to 3 sessions).

## 2. Temperature and wetness

**What the player sees.** Frozen regions are cold: you are chilled out there and warm up by a
fire or a brazier, and a fire you light melts the ice around it into pools. Near lava the air
shimmers, water left beside it boils away and wood smoulders before it bursts into flame. A
soaked character does not catch fire but lightning hurts twice as much; a soaked monster hit by
frost freezes solid for a moment.

**Temperature.** A coarse field: one byte per 2 m cube (16 x 16 x 16 per chunk, 4 KB),
allocated only for chunks within the simulation's reach and not saved (it is rebuilt from what
is there in a few seconds).

- **Base temperature** from the region: the frost flavours (`FL_FROST`, `FL_GLACIAL`,
  `FL_EVERWINTER`, ...) are cold, the fire ones (`FL_EMBER`, `FL_SUNSCORCHED`, `FL_SULPHUR`,
  ...) and the deep strata (`ST_INFERNO`, `ST_IRONHELL`, `ST_CORE`) hot, the rest mild.
- **Sources:** lava (very hot), fires by their fuel, braziers, torches (a little), the player's
  flame spells (a burst), frost spells (a burst of cold), ice and snow (cold), deep water
  (cool).
- **Each tick** a cell moves toward the average of its neighbours and its base; solid rock
  conducts slowly, air faster, metal fastest (from the material table, step 3).
- **Rules that read it** instead of their own special cases:
  - ice melts above 2 degrees, still water freezes at the surface below -5 (only water woken
    by a change, so generated lakes in frozen regions keep their look until disturbed);
  - water boils off slowly above 100 degrees (an eighth at a time, as steam);
  - wood and plants ignite above their ignition temperature (fire spreads by heat, not by a
    fixed chance: a log next to lava smoulders, then burns);
  - plants grow faster when warm, stop when cold, and frost kills grown grass and flowers;
  - steam condenses back to water drops when it reaches a cold ceiling (dripping, step 4).
- **On players:** the existing chill and burn states come from the field: standing in cold for
  a while chills (slower), warmth near a fire clears it and gives "WARMED"; close to lava you
  take heat damage. Meals that resist fire or frost (`ME_FIRE_RES`, `ME_FROST_RES`, `ME_LAVA`)
  count.
- **Remedies for cold** (decision 2): warmth at fires and braziers (and a campfire you light
  yourself from wood), the frost-resisting meals that exist, a **fur cloak** (shop and forge:
  keeps you from being chilled) and a **warming draught** (alchemy: an hour of warmth). Cold
  never hurts in this step. Once the remedies are in and tested, whether long exposure should
  start to hurt is your call; the code keeps it as one switch.
- **On monsters:** fire-element monsters seek heat and avoid cold, frost ones the opposite;
  all of them avoid cells above 60 degrees, as they now avoid fire.

**Wetness.** A value 0..1 on players, monsters and critters (not on blocks): set by water depth,
steam and dripping; dries by temperature. Wet: no catching fire, lightning x2, frost freezes
(a short stun), fire hits make steam instead of burning. Wet blocks are derived, not stored:
wood touching water does not ignite.

**Code.** New `src/game/heat.c` (field, sources, diffusion, queries `heat_at(p)`), hooks in
`sim.c` (melt, boil, freeze, ignite), `bio.c` (growth rate, frost), `game.c` (player states),
`monsters.c` (seeking, avoiding). A heat-haze shimmer near hot cells is in step 4.

**Tests.** A `--demo thaw`: ice blocks around a campfire melt into a pool within a minute; a
pool by lava boils down; a soaked player takes double from a lightning trap. Bot batch in a
frozen and a hot region: bots warm up at fires (bot rule: chilled and a fire near, go stand by
it).

**Effort:** medium (3 to 4 sessions). Cost: the field updates a slice of chunks per tick.

## 3. A table of materials

**What the player sees.** The world answers more questions the same way: oil spilled on water
floats and burns; a lightning bolt into iron bars arcs to the monster holding them; a wooden
crate floats up from a flooded cellar; slime eats through stone; a flask thrown at the world does
what is in it.

**The table.** One row per block (`MATERIALS[B_COUNT]` next to `BLOCKS` and `BLOCK_HARDNESS`):

| Property | Used by |
|----------|---------|
| hardness (exists) | digging, blasts |
| ignition temperature, fuel | fire (step 2's heat) |
| melts at, becomes | ice to water, metal glowing, glass softening near lava |
| conducts heat | the temperature field |
| conducts lightning | lightning through water and metal (bars, chains, iron blocks) |
| density | floating and sinking: items, wood debris, fluids layering |
| porosity | seeping (today a fixed test on broken blocks) |
| nutrient | what plants grow on (earth, ash, moss, bones) |
| absorbs water | soaked wood and earth, dried by heat |

The rules of steps 1 and 2 and the existing ones (`flammable`, `ignite`, `porous`, `earth`,
`stone`) are rewritten to read the table, so a new block needs a row, not new code.

**New fluids**, using the existing levels: **oil** (from oil flasks, food `FD_OIL` already
exists, and from broken oil jars placed in some storerooms): lighter than water, floats on it,
burns hot and fast. **Acid** (a slime's death, an alchemy elixir): eats one piece of a broken
block per tick, hurts what stands in it, neutralised by water.

**Thrown flasks:** alchemy gains three flasks to throw (today's elixirs act only on the one who
drinks them): oil (a burning pool), frost salt (freezes the water it lands in) and a growth
tonic (plants spring up around it, by what the ground allows).

**Floating:** pickups and critter bodies of low density rise to the water's surface and drift a
little with a falling level.

**Code.** `block.c` (the table), `sim.c` and `bio.c` (rules through the table), `items.c` /
`alchemy.c` (thrown elixirs), `combat.c` (pickups float). Two new block types (oil and acid as
liquids) fit the 48 texture slots.

**Tests.** `test_worldgen` gets a table test (every block has sane values: anything that burns
has fuel, every liquid a density). `--demo oil`: oil poured on a pool, lit, burns off the
surface; `--demo arc`: lightning into bars hurts the monster behind them.

**Effort:** medium (3 sessions).

## 4. Sound and sight

**What the player sees and hears.** Every process announces itself and changes gently instead
of popping:

- **Sound.** Dripping where water seeps or steam condenses; a crackle that grows with a fire's
  fuel and dies to embers; the hiss of steam; bees you can hear coming from one side; water
  that sounds louder where it is moving (the water loop scaled by moving cells near, today it
  only knows standing water); a creak and a rumble before a collapse (step 5); the gurgle of
  something drowning.
- **Water** shows where it flows: the surface texture slides toward the lower neighbour (the
  mesher knows the levels around each cell), with a line of foam where it falls over an edge.
- **Wet stone** next to water is drawn darker; it dries back as the wetness goes.
- **Soot and scorch:** ceilings above a fire blacken, walls beside an explosion are scorched;
  both fade over a long time.
- **Lava cools visibly:** a thin flow is drawn from yellow through red to black as it cools, and
  fresh obsidian smokes for a while.
- **Fire** is drawn as big as its fuel: a fresh fire fills its cell, a dying one is a few low
  flames.
- **Growing things grow:** a plant's age is kept in its level byte and picks the texture
  variant (shoot, young, grown), so grass spreads as a sprout first and flowers bud before they
  open.
- **Heat shimmer** over lava and big fires (a small screen-space wobble where the field is hot).
- **Cold breath:** the player's breath shows as small white puffs in cold regions.

**Code.** A per-chunk **stain** layer (one byte per cell, allocated on demand: wet, soot,
scorch, age bits), saved with the edits (save v18); `mesher.c` reads it for tints, water flow
direction and plant age; a new vertex flag for flowing water (the shader slides its texture);
`sfx.c` new point loops (drips, fire by size) and one-shots.

**New sounds** (drip, fire sizes, steam hiss, collapse creak and rumble, gurgle, ice cracking,
bees from one side) come from `tools/gen_sfx.mjs` with the ElevenLabs sound API (decision 5):
the new entries go into `tools/sfx_list.mjs`, the key is checked with `elevenlabs onboard
check` (the key file itself is never opened), and only the missing files are generated. Until
they exist the code falls back to existing sounds (the salamander hiss for steam, the bees
loop, the water step for drips), so nothing waits on them.

**Effort:** medium (3 sessions), plus the sound generation if wanted.

## 5. Weight and collapse

**What the player sees.** Cut through the pillars of a hall and the ceiling holds for a moment
(dust trickles, a creak), then comes down in stages around the gap, burying what is under it.
A bridge with its middle blown out falls into the chasm. A wooden scaffold burns through and
collapses. Masonry arches stand longer than they should; loose earth barely spans at all.

**How.** Today only islands of up to 48 cells that touch nothing fall. Instead, after a
break, the cells within 8 m are checked for support:

- A cell is **supported** if it rests on a supported cell, or reaches one sideways within its
  material's **span** (earth 1, wood 4, masonry 5, rock 6, metal 8: arches and beams), counted
  through solid cells. The ground (cells generated as solid below the floor level of their
  layer) is always supported.
- Unsupported cells **fall**, but not at once: they are marked, dust falls from them and a
  creak sounds for 1 to 2 seconds (a warning you can run from), then they drop as falling
  debris, an entity with the material's colour, and land as **rubble** (a new block: loose
  stone, soft, diggable, which spills sideways like coarse sand when piled higher than two).
- Falling debris hurts what it lands on by its weight. A player caught under a ceiling takes
  heavy damage but is never killed from full health in one go (decision 3; as falls); monsters
  have no such mercy.
- Protected places never fall (the Sanctum, vaults, stairwells, the outside).
- The check is bounded (8 m, at most 4000 cells) and spread over frames; the web build gets
  the same with a smaller radius if needed.

**Code.** `dig.c` (`support_check` replaces `crumble_islands`), a small falling-debris list
like the bombs, a new block type (rubble), `sim.c` (rubble spills like a thick fluid with
levels, reusing the lava rules without the heat).

**Tests.** `test_worldgen`: a beam of each material over a gap, cut at one end, stands or falls
by its span; a pillar hall with all pillars cut comes down, one with half of them stands.
`--demo collapse`: bombs at the pillars of a hall.

**Effort:** medium to large (4 sessions). The span rules need tuning in play so the fortress
does not fall apart from ordinary fighting.

## 6. Populations and the food chain

**What the player sees.** Regions have a life of their own that you change: hunt the hares of
a cave and its grass grows tall and flowers, bees come, then the hounds that lived on the
hares move on or turn on you. Leave it and the hares come back from the next cave. A
battlefield draws flies, then bats that eat the flies. Overgrazed meadows go bare and the goats
leave or starve (their bones turn up).

**How.** Not every animal is simulated; each region keeps **numbers** that follow simple
predator and prey rules, and only the animals near the players are real:

- Per chunk (in its ecology record): plants (grass, flowers, moss, fungus: counted by the
  growth sampling that already runs), grazers (hares, goats, ...), insects (flies, bees, moths:
  from flowers, carrion), predators (hounds, bats, spiders as hunters), carrion (deaths).
- Every 10 seconds near the players, and in catch-up (step 1), the numbers change: grazers grow
  with plants and shrink with predators; predators grow with grazers and starve without;
  insects follow flowers and carrion; carrion decays; animals wander to neighbouring chunks
  where there is more to eat.
- **Spawning reads the numbers:** `wildlife_spawn_chunk` spawns as many critters as the record
  says (today a fixed roll per chunk); killing them lowers it. Hounds and bats near critters hunt
  them (a monster's target can already be something other than a player).
- **Hounds and bats follow their food** (decision 6): where prey is plentiful more of them are
  spawned (up to half again as many), where it is hunted out fewer (down to half). Other
  monsters keep the danger levels of their regions.
- **Grazing writes the numbers back:** what critters crop and what they leave as bones.
- The bot log and `worldmap` print the ecology of a region, so tuning can be checked.

**Code.** `wildlife.c` (records, spawning from them, hunting), `bio.c` (plant counts),
`monsters.c` (predators target critters), save v19 (ecology records of visited chunks).

**Tests.** A headless test of the numbers alone: a region with no predators fills to its plant
limit and stays; one with predators settles into a cycle without dying out; overgrazing then
recovery. Bot batch: hunting a cave empty is followed by its grass growing over (seen through
step 1).

**Effort:** large (4 to 5 sessions).

## Later

Ideas that build on these and can be taken up afterwards:

- **Air and draughts:** gases drift toward openings and up stairwells; fire needs air and chokes
  in a sealed room, then flares when the door opens.
- **Currents:** flowing water pushes players, items and monsters; waterfalls with spray;
  running water wears earth away.
- **Monsters with needs:** they drink, sleep by warmth, leave tracks; an emptied lair grows over;
  fungus on a corpse can rise as a fungus monster.
- **Farming:** dig channels that carry water, plant seeds, let bees pollinate the Sanctum garden.
- **Monsters that dig** (worms through earth, trolls through walls); **critters that swim**.

## Order and effort

| Step | Sessions | Save | Depends on |
|------|----------|------|------------|
| 1. Time away | 2 to 3 | v17 last-seen times | - |
| 2. Temperature, wetness | 3 to 4 | - (rebuilt) | - |
| 3. Materials | 3 | - | 2 (ignition, conduction) |
| 4. Sound and sight | 3 | v18 stains, plant ages | 2, 3 for the most effects |
| 5. Weight, collapse | 4 | rubble as edits | 3 (spans) |
| 6. Food chain | 4 to 5 | v19 ecology | 1 (catch-up) |

After each step: `test_worldgen`, its demo and screenshots, a four-bot batch of 15 minutes, the
cost measured, SIMULATION.md updated, commit and push.

## Open questions

None at the moment; the six questions of the first draft are decided above. Still to settle:
when spells act on the world beyond what they do now (meteors, storms, runes, auras, hits on
monsters, the utility fire and ice spells; see SIMULATION.md), before step 1 or as part of
step 2.
