# Fortress Crawl: The World Simulation

The world follows a few simple laws of physics, chemistry and biology, cell by cell, so that
things the player does have effects nobody scripted: a blasted lake wall floods the hall
below, water poured on lava leaves rock to walk on. It is built in stages:

1. **Breakable blocks** ([DESTRUCTION.md](DESTRUCTION.md)): 25 cm pieces, pickaxe, bombs. Built.
2. **Water**: flowing, swimming, drowning, lava and fire. Built (this page).
3. **Fire, heat and gases**, and the elements acting on the world. Built (this page).
4. **Living things**: plants, moss, fungus, vines, hives, insects, decay, grazing. Built.

Code: `src/game/sim.c` (water, fire, gases, elements), `src/game/bio.c` (living things), `Chunk.fluid` and `chunkmap_edit` in `src/world/chunkmap.c`, the
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

## Seeping

Water seeps through a broken block into the cell beyond it (below or to the side) where the
block is hollow enough (at least 8 of its 64 pieces gone) and open on both the side the water
touches and the far side: one eighth every other tick, so a cracked floor under a pool drips
into the room below instead of holding or emptying at once.

## Lava

Lava flows too, but thickly: it moves only every third tick, falls into what is below like
water, and runs sideways only where it stands three eighths above its neighbour, so it stops
in a short tongue instead of a film. Where a tongue is two eighths thin or less it crusts over
into obsidian after a while. It burns away grass, flowers, vines, webs, roots, bones and fungus
in its way, sets wood alight beside it and melts ice. Water quenches it: lava touching water
turns to obsidian and the water to steam. A thin flow is only as high as it is: you collide with
its top, and it is drawn at its height. Lava the world was generated with lies still until
something beside it changes (`--demo lava`: 18 cells let loose spread to about 30, set a
wooden post alight and leave 14 cells of obsidian where they cooled or met the pool).

## Swimming and breath

- In water deeper than 0.3 m you wade (75% speed); deeper than 0.9 m you swim: 55% speed, no
  falling, you float up until your eyes are at the surface, jump swims up and crouch dives. At
  an edge you climb out as if stepping up. Falling into water does not hurt.
- With your eyes under water you have 15 seconds of breath, shown as ten bubbles over the
  health bar. Then "OUT OF BREATH - GET TO THE SURFACE", and you lose 8% of your health every
  second. Breath comes back four times as fast as it went.

## Monsters in water, fire and smoke

- Monsters swim like players: slower, floating with the head out, and they climb out at
  edges. Golems, knights, skeletons and mimics are too heavy: they sink and wade along the
  bottom. Those that breathe drown after 12 seconds under water (8% of their health a second);
  skeletons, wraiths, golems, mimics and wisps do not breathe. A monster lured into a lake can
  drown.
- They will not step into fire, lava or a thick cloud of poison or steam: they turn aside, and
  one standing in it gets out.
- In thick smoke or steam you cannot be seen beyond 3.5 metres: smoke is cover.
- And you see little in it either: smoke greys the view, steam whitens it, poison tints it
  green, and under water the light turns green-blue.

## Fire

- A fire the simulation lights keeps its fuel in the cell's level byte: 8 for wood, 2 for webs,
  grass and roots, 1 for air beside something that burns. It loses one about every second and
  goes out at zero; what burned is gone. The fires the world was generated with (campfires,
  braziers) have no fuel count: they burn for ever and never spread.
- A burning cell sets what burns around it alight, upward twice as readily as sideways, so a
  wooden wall goes up in about four seconds and is ash after fifteen (`--demo burn`).
- It melts ice beside it into water (with steam) and gives off smoke.
- Players standing in it burn; monsters standing in it catch fire.
- Water flowing in puts it out (part 2); frost puts it out.

## Gases

Gases are drifting clouds, not cells: up to 96 at a time, each with a kind, an amount and a
radius. They grow, thin out, rise or sink, wander a little and slide along walls. They are
drawn as slow motes.

| Gas | From | Moves | Does |
|-----|------|-------|------|
| steam | water on lava, fire on water or ice | rises fast, gone in seconds | scalds when thick |
| smoke | fires, explosions | rises, lingers | takes your breath (the bubbles drain) |
| poison | poison spells and arrows hitting the world, poison bloaters bursting | sinks a little, lingers | poisons players and monsters |
| swamp gas | still water with roots, fungus or grass at its edge, now and then near you | hangs, lasts long | explodes when fire, lava, a torch or a brazier reaches it, or a blast or a fire spell goes off in it |
| spores | broken fungus | drifts | poisons a little when thick |

## The elements on the world

What a spell, an arrow or a bloater's burst does where it strikes the world (and fire, frost
and lightning stop at water's surface and act on it):

- **Fire** sets what burns alight, melts ice, raises steam from water and sets off swamp gas.
- **Frost** freezes the surface of water within two metres into ice you can walk on, and puts
  out fires.
- **Lightning** runs through the whole body of water it strikes (up to 400 cells, 10 m around)
  and hurts everyone standing in it, the caster too, a little.
- **Poison** leaves a cloud.
- **Bombs** set what burns alight, set off swamp gas around them and leave smoke.

### Every spell, every blow

All of it, for every way an element reaches the world:

| Spell or blow | Where it acts on the world |
|---------------|----------------------------|
| bolt, barrage, orb, elemental arrows | where they hit a wall, the floor or water |
| breath, nova | where they hit, and where they fade out (half strength) |
| a hit on a monster (spells, arrows, weapons with fire, frost or lightning on them, chain lightning at every jump) | at the monster's feet: the grass it stands in catches, the water round it freezes, lightning runs through the water it wades in |
| a fire, frost or lightning blow on a player (monsters' attacks, traps) | at the player's feet in the same way |
| meteor | breaks what is soft where it lands (an earth meteor rock too), its element at the centre and at five points round it, and smoke |
| storm | at every strike |
| rune | where it goes off |
| orbit | where the orbs pass (one of them each tick) |
| aura | two points of its edge every pulse; a frost aura also freezes the water under your feet, so you can walk across a pool |

Poison leaves its clouds only where spells land and from bloaters, not on every hit (a poisoned
blade would otherwise poison its wielder). Frost never freezes a cell someone stands in: it
does not shut monsters or players into ice. Small hurts (burning, steam, gas) do not act on the
world again, so nothing feeds on itself.

The utility spells join in: **Quench** also puts out fires and raises steam where it cools lava;
the cells **Stoneshape**, **Quench** and the other temporary spells change wake the simulation
when they appear and when they go, so water runs into a shaped tunnel and thawed water flows
again. The hearths of **Warden's Keep** and **Hut** stay hearths: they are lit for warmth and do
not spread (a hut would burn down around you).

`--demo elements`: a meadow with a crawler in it, a wooden post and a pool with a crawler wading.
A fire meteor and a fire bolt at the crawler in the grass burn the meadow to ash (48 tufts to
15 cells of ash in ten seconds); a lightning storm over the pool kills the wading crawler
through the water; a frost aura at the pool's edge freezes all 25 cells of it.

## Living things

Six new blocks: **moss** (stone grown over), **vines**, **flowers**, **beehives**, **ash** and
**obsidian**. The block atlas grew to 2048 px to hold 48 block types per theme.

Growth looks at random cells around each player (48 per player, 8 times a second, within 16 m
and 6 m up and down), and each cell grows by what it needs:

| Grows | Needs | Spreads |
|-------|-------|---------|
| grass | earth below (or ash), light 6 or more (ash: 4) | from grass beside it; ash is taken over first |
| flowers | grass in bright light (9 or more), few flowers beside it | on grass |
| moss | bare rock, damp (water within 2 to 3 cells), shade (light 8 or less) | from moss beside it |
| fungus | the dark (light 4 or less), stone, earth or ash below | from fungus or bones beside it; bones turn to fungus |
| vines | a mossy ceiling, or a ceiling over damp ground, some light | grow down, up to six long |
| beehives | a ceiling, three or more flowers below, no hive within five cells | rare |

- **Fire** leaves ash where it burned on something (half the time); grass takes ash over first.
  Vines, flowers and hives burn.
- **Lava** cooled by water turns to obsidian (black glass, masonry-hard).
- **The dead:** a monster that dies leaves bones a third of the time, where it fell. Flies find
  them; fungus eats them in time.
- **Grazing:** goats, rams and hares crop the grass and flowers they stand in.
- **Harvest:** flowers and vines give herbs, the first pickaxe blow on moss scrapes it off as
  moss for the pot, and a hive gives honey (sometimes royal jelly). Breaking a hive sets its
  bees on you: they follow and sting until you dive into water, stand in smoke or outrun them.
- **Climbing:** a vine can be climbed: jump climbs, crouch slides down, otherwise you hold on.
- **Insects** are swarms of motes near what draws them, looked for among cells near you: flies
  over bones and ash, bees at flowers and hives, moths at torches and lanterns, glowworms
  under dark moss.
- Growth is a lasting change like any other (saved; it grows back after you cut it, slowly).
  Nothing grows in protected places (the Sanctum has its own garden, vaults, stairwells, the
  outside).

Growth is slow on purpose: a patch of grass by a torch spreads about a metre a minute. With
`--demo grow` it runs 300 times as fast over a small garden (a bed of earth with two tufts of
grass, a channel of water along a rock wall under an overhang, bones and two torches): in ten
seconds, about twenty minutes of growth, 48 grass, 17 flowers, 38 moss, 28 vines and 2 hives.

## Withering

Plants that grew in play wither when what they need goes away; the plants the world was
generated with stay as made. Grass in the dark (light 2 or less) dies, flowers in dim light
close back to grass, moss whose water is gone dries back to stone, fungus brought into bright
light dries up, and a vine cut above falls. Moss grows and dries by the same test of dampness,
so it does not flicker. (`--demo grow` puts the torches out and drains the channel after six
seconds: the 18 cells of moss dry back to stone; the grass stays, because the vestibule's
braziers still light it.)

## Saving and waking

Every changed cell is saved (save version 15), and the clouds of gas too (version 16). Water,
lava and fires that were moving when the game was saved, or when their chunk was unloaded,
move on as soon as their chunk is loaded again: a fire saved half-way burns on after loading
and goes out by itself. Insect swarms are not saved; they gather again around what draws them.

## Time away

The simulation runs only near the players, so the world would freeze wherever nobody is. It
does not: every chunk a player has been near remembers when it was last seen (the world's play
time; time with the game closed does not count). Coming near it again after more than a minute,
it catches up on what it missed, up to two hours (`src/game/away.c`):

- Fires that were still burning spread once more to what burns beside them and have burnt out
  since, to ash where they lay on something.
- Growth and withering run for the time away, compressed: the cells where anything can happen
  (plants and what is beside them, bones, ash, damp stone by water; at most 2000) get as many
  looks as they would have had in play (one every 37 seconds or so), at most 60000 in all,
  3000 a frame. What grows opens new ground around it for the next looks.
- If it shows (a dozen cells or more grew next to you), the world says so: "THIS PLACE HAS
  GROWN OVER SINCE YOU WERE HERE" (at most every ten minutes).
- An area never visited needs nothing: it is as the world made it. The last 4096 chunks seen
  are remembered (save version 17); beyond that the longest unseen are forgotten.

`--demo away` (`--cast n`: n minutes): a bed of earth with two tufts of grass by a channel of
water under a rock wall, a torch, bones and a burning wooden post; you walk 80 m off, an hour
passes, you come back. Before: 1 tuft of grass, 5 fires. After: 31 grass, 20 flowers, 18 moss
on the wall, the bones turned to fungus, the fire out.

## Cost

The whole simulation (water, fire, gases, growth, insects) costs 0.003 ms a frame on average
and 0.12 ms at most in a four-minute explorer run on the desktop build; what it changes costs
the remeshing of the chunks it touches, as any other edit.

## Tests and tools

- `--demo flood`: a block of water let loose in the vestibule; the log follows its cells,
  volume and the cells waiting. `--cast 1` puts lava in its way, `--cast 2` drops you into a
  walled shaft of water (the log follows your height, depth and breath), `--cast 3` freezes
  the pool and then strikes it with lightning, `--cast 4` breaks the floor under it through
  its core above a hollow (the water seeps through), `--cast 5` shuts a crawler and a golem in
  a lidded shaft of water.
- `--demo lava`: a block of lava let loose beside a wooden post and a pool; the log counts lava,
  obsidian, fire and water.
- `--demo burn`: a wooden wall with grass in front, set alight; the log counts wood, fire and
  smoke. `--cast 1` hangs a cloud of swamp gas beside it, which explodes when the fire reaches
  it ("swamp gas explodes at ...").
- `--demo grow` (`--cast n`: n growth ticks a frame, default 40): the garden above; the log
  counts what grew.

## Bots

Bots do not path through water over their heads, through fire or onto lava; in deep water or on
a vine they swim or climb up and out. When they are stuck they dig toward their goal with the
pickaxe for a few seconds, and throw a bomb at masonry in the way if they carry one.

## In play

Four bot runs of 15 minutes after all four stages (Edhold 1337 quester, Gerhold 2025 explorer,
Aldburg 8080 caster, Ulwall 9001 archer): no crashes, no drownings, one death in all (the
quester, at level 10 in chapter 3). Between 31 and 95 cells of each world changed (bones
where monsters fell, grass and moss spreading, fungus on bones), far below the 60000 kept.
The simulation's cost stays under a tenth of a millisecond a frame at its worst.

The same four runs again after lava, seeping, monsters in water, withering and digging bots:
no crashes, three deaths in all (the quester twice, in melee). The bots dug toward their goal
23 times and threw 5 bombs at masonry when stuck; they had to give up a goal and go home 4 times
instead of 10. Between 90 and 187 cells of each world changed. Bots still do not harvest, so
hives were checked with the demos.

In `--demo flood --cast 5` a crawler and a golem are shut in a lidded shaft of water: the
crawler floats up against the lid, runs out of air after 12 seconds and drowns at 24; the golem
sinks, walks the bottom and is unharmed (it does not breathe).

## Not done yet

- Critters (hares, goats, fish) do not swim or drown; monsters do.
- Water inside a broken block (in among its pieces) is not kept: it passes through or not.
- Swarms are not saved (they gather again).
- Monsters do not dig.
