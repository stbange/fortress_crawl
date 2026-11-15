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

## Swimming and breath

- In water deeper than 0.3 m you wade (75% speed); deeper than 0.9 m you swim: 55% speed, no
  falling, you float up until your eyes are at the surface, jump swims up and crouch dives. At
  an edge you climb out as if stepping up. Falling into water does not hurt.
- With your eyes under water you have 15 seconds of breath, shown as ten bubbles over the
  health bar. Then "OUT OF BREATH - GET TO THE SURFACE", and you lose 8% of your health every
  second. Breath comes back four times as fast as it went.

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

## Cost

The whole simulation (water, fire, gases, growth, insects) costs 0.003 ms a frame on average
and 0.12 ms at most in a four-minute explorer run on the desktop build; what it changes costs
the remeshing of the chunks it touches, as any other edit.

## Tests and tools

- `--demo flood`: a block of water let loose in the vestibule; the log follows its cells,
  volume and the cells waiting. `--cast 1` puts lava in its way, `--cast 2` drops you into a
  walled shaft of water (the log follows your height, depth and breath), `--cast 3` freezes
  the pool and then strikes it with lightning.
- `--demo burn`: a wooden wall with grass in front, set alight; the log counts wood, fire and
  smoke. `--cast 1` hangs a cloud of swamp gas beside it, which explodes when the fire reaches
  it ("swamp gas explodes at ...").
- `--demo grow` (`--cast n`: n growth ticks a frame, default 40): the garden above; the log
  counts what grew.

## Bots

Bots do not path through water over their heads, through fire or onto lava; in deep water or on
a vine they swim or climb up and out.

## In play

Four bot runs of 15 minutes after all four stages (Edhold 1337 quester, Gerhold 2025 explorer,
Aldburg 8080 caster, Ulwall 9001 archer): no crashes, no drownings, one death in all (the
quester, at level 10 in chapter 3). Between 31 and 95 cells of each world changed (bones
where monsters fell, grass and moss spreading, fungus on bones), far below the 60000 kept.
Bots do not dig, blast or harvest, so the pickaxe, bombs, hives and swimming were checked with
the demos rather than in these runs. The simulation's cost stays under a tenth of a millisecond
a frame at its worst.

## Not done yet

- Lava does not flow; it stays as generated.
- Monsters and critters do not swim or drown.
- Water does not seep through the gaps in broken blocks.
- Gases and swarms are not saved.
- Plants do not die back in the dark or for want of water; nothing withers.
- Smoke does not hide anything yet, and monsters do not avoid fire or clouds.
