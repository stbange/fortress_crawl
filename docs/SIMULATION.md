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

## Temperature

One temperature per 2 m cube, for the chunks round the players (`src/game/heat.c`; rebuilt
from what is there, never saved):

- **Base by region:** mild (12 degrees) in most of the world; the frozen north from -6 down to
  about -20 in its deepest tiers; the burning south from 26 up; the deep strata (the Core, the
  Maw, Ironhell, the Inferno) 18 hotter; the high and airy ones 6 cooler. The Sanctum is 18.
- **Sources** pull their cell toward their own temperature: lava 700, a spreading fire 320, a
  hearth fire 160, a brazier 130, a torch 40, a lantern 30; ice keeps its cell at freezing.
  Fire spells, blasts and the elements of spells add bursts of heat; frost adds cold.
- **Spreading:** each cell moves toward its neighbours (a quarter of the way four times a
  second) and slowly toward its region's base. A changed chunk is looked at again at once.

What reads it:

- **Ice** melts above 3 degrees (a ring of ice round a campfire is a puddle in half a minute,
  and the water runs into the fire and puts it out).
- **Still water** that has moved freezes at its surface below -6; the water the world was made
  with keeps its look until something disturbs it.
- **Water boils** away above 100 degrees, faster the hotter, as steam. **Steam** that drifts
  into cool air condenses and drips back down as water.
- **What burns** catches from heat alone: grass, vines, webs, roots and hives at 140, wood at
  220. A log beside lava smoulders, then burns.
- **Plants** grow only between 2 and 45 degrees; frost kills grown grass, flowers and vines.
- **Monsters** keep out of air hotter than 60, as they keep out of fire.

## Cold, warmth and wetness

- **Cold** gets into you slowly where it is below 2 degrees: at -8 it takes about two minutes to
  be COLD (85% speed), twice that to be CHILLED (60%, as frost spells do). Colder and wetter is
  faster. **The cold does not hurt** (decided until there are remedies; there are now, so it
  could, but it does not).
- **Remedies:** standing by a fire or brazier (above 24 degrees) drives the cold out, and from a
  real fire you are WARM (two minutes when the cold does not touch you); a frost-resisting meal;
  a **fur cloak** (shop, 350 gold: the cold reaches you four times slower); a **warming draught**
  (shop, 60 gold: an hour of warmth).
- **Wetness** (WET): in water you are soaked; it dries with warmth (about half a minute in a
  mild hall, much faster by a fire); steam wets you too. Soaked, fire only steams on you (no
  burning, no hurt), lightning hurts you twice, frost chills you for five seconds and drives the
  cold in.
- **Monsters** get wet the same way: fire on a wet monster hurts half and does not catch,
  lightning twice, and frost freezes it solid for a moment and a half (not bosses).
- The fur cloak, warmth and cold are saved with the character (save version 18).

`--demo thaw`: a ring of ice round a hearth fire, a pool with a pit of lava beyond a rock, you
moved into the pool and struck by a 20-point lightning blow while soaked (20 health lost: the
jolt, the half again for being wet, and the shock through the pool). The ice melts
(14 cells to 3 in a minute); the air by the lava rises to 160 and the pool boils down slowly.
`--cast 1` in the far north (`--pos 8 6 -700 --fly`): you stand in -8 degrees and are COLD
after about two minutes.

## The table of materials

Every block has a row in one table (`SUBSTANCES` in `src/world/block.c`) and the rules read it
instead of keeping their own lists: what burns (the degrees it catches at and how long it
burns), what melts and into what, what carries lightning, how heavy it is against water, what
water seeps through when broken, what plants root in. A new block needs a row, not new code;
`test_worldgen` checks the rows are sane (everything that burns has fuel, every liquid flows,
oil is lighter than water, metal carries lightning).

- **Oil** (new liquid): flows as water does, rises through water and floats on it, and burns
  (catching at 180 degrees, for as long as the pool is deep): a burning slick on a pool.
- **Acid** (new liquid): what is left of a slime when it dies (a pool of two to four eighths). It
  flows, eats the stone round it a few pieces at a time (not metal, glass, obsidian or what is
  protected), using itself up as it eats, burns whoever stands in it (3% of their health twice a
  second; slimes are at home in it) and water dilutes it into water.
- **Lightning through metal:** lightning that strikes iron (bars, chains, iron blocks, braziers,
  lanterns) runs along all of it that is joined there (up to 200 cells, 12 m round) and hurts
  whoever touches it, as it runs through water.
- **Floating:** food, reagents, scrolls and formulae dropped in water float up to the surface;
  weapons and gold sink, slowly.
- **Flasks to throw:** C chooses what X throws: a bomb, an **oil flask** (a pool of oil where it
  breaks, on top of water if it lands in it), **frost salt** (frost at five points round where it
  breaks, cold, foes frozen) or a **growth tonic** (grass springs up on earth in some light,
  fungus in the dark, moss on damp stone, then a few seasons of growth at once). They break on
  the first thing they meet. The shop sells them (3 oil flasks 45 gold, 3 frost salts 60, 2
  growth tonics 90); they are saved with the character (save version 19).

`--demo matter`: oil thrown onto a pool and lit burns on the water; a pool of acid against a
rock wall eats 10 of its pieces and spreads out thin; lightning into iron bars hurts the crawler
behind them (28 health to 12); a growth tonic on bare earth leaves 12 tufts of grass and three
flowers; a fish dropped into the pool floats at the surface.

## Sound and sight

Every process shows and sounds as it goes, and changes gently.

**Marks on cells.** Each cell can carry marks besides its block (two bits each, kept in a layer
per chunk and saved with the edits, save version 20): wet, soot, scorch and, for a plant, its
age. The mesher draws them:

- **Wet stone** next to water, or where water drained away, is drawn darker; it dries in a few
  minutes once the water is gone.
- **Soot:** the smoke of a fire blackens the first ceiling above it (up to 12 m up); it fades
  over hours. **Scorch:** a blast blackens the walls round it.
- **Plants grow:** what grows starts as a shoot (half height), then young, then grown; vines
  grow down from their top.
- **Fires** are drawn as big as their fuel: a fresh one fills its cell, a dying one is a few
  low flames.
- **Lava** in a thin flow is drawn darker as it thins; fresh obsidian smokes.
- **Flowing liquids:** the surface texture of water, oil and acid slides the way it runs
  (toward the steepest drop, over edges), and down the sides where it falls; falling water
  throws spray.
- **Heat shimmer:** pale motes rise from air hotter than 150 degrees. **Cold breath:** your
  breath shows as a white puff every few seconds below 2 degrees.

**Sound.** New sounds made with the ElevenLabs sound effects API (`tools/sfx_list.mjs`): the
fire loop grows louder the more burns near you and a roaring **blaze** joins in from four
burning cells on; a **stream** loop where water is moving near you (not only where it lies);
**steam hissing**, **ice cracking** as it melts, **drips** where water seeps through, a
**gurgle** when you drown, oil **catching fire**, acid **sizzling** on stone, **flasks
shattering**, and the **creak** and **rumble** of a collapse (step 5).

## Weight and collapse

Blocks bear weight (`src/world/support.c`). A block is held if it rests on a held block, or
reaches one sideways or hangs from one above within its material's **span** (in the table of
materials): earth 1, glass and ice 3, wood 4, masonry and obsidian 5, rock and ore 6, metal 8.
Resting on something held is always held; each step sideways or down costs one.

- After a break (a pickaxe, a blast, a burning beam, acid), the box round it (up to 8 m each
  way) is checked twice: as it was and as it is. **Only what was held before and is not now
  falls**, so the world as it was made stands even where it spans further than these rules
  allow (wide halls do not fall from ordinary fighting), and only what relied on what was broken
  comes down. A broken block with half its pieces or more still bears load. Protected places
  never fall.
- What loses its support **creaks** and trickles dust for one to two seconds (a moment to run),
  the lowest first, then drops as **falling debris** that lands as **rubble** (stone; wood and
  metal break up). The **rumble** of the landing carries.
- Debris hurts what it lands on: monsters by its speed, players for 30% of their health a
  block; a player at nine-tenths of their health or more is never killed by one collapse
  (decided).
- **Rubble** (new block) is loose: it falls into what is empty below and slides off a pile onto
  a lower side, like coarse sand. It breaks easily.
- `test_worldgen` checks the rules on built structures: a masonry beam cut at one end loses the
  three cells beyond its span, a wooden one four; a 5 x 5 slab on four corner pillars stands,
  loses the one cell beyond masonry's reach with the pillars of one side cut, and falls whole
  with all four cut.

`--demo collapse`: a 5 x 5 slab of stone on four pillars, bombs at the pillars one at a time: the
two on the east side cut, one cell of the slab falls; the third, five more; the last, the other
19, which land as a mound of rubble.

## Populations and the food chain

Each region (chunk) a player has been near keeps numbers, not animals (`src/world/eco.c`,
kept with the last-seen times in `src/game/away.c`, saved in version 21): its plants and
flowers (counted in the world), grazers (hares, goats, rams, grouse, quail, hens), predators
(hounds, bats), insects and carrion. Only the animals near the players are real, and they are
made from these numbers. Ten minutes of play are an hour for them.

- **Grazers** grow toward what the plants feed (one for every 25 plant cells, and a few wander
  in) and are eaten by predators. Too many of them eat the grass and flowers down faster than
  they grow: those cells go from the world.
- **Predators** live on the grazers (bats on insects too) and starve without them; a few always
  wander in. **Insects** follow the flowers and the dead; **carrion** (what died there) rots.
- **Neighbouring regions** even out: animals wander to where there is more to eat.
- **They are the world's animals:** grazers spawn by the numbers (none where they are hunted
  out, up to twice as many where they thrive); hunting one, or a hound catching one, takes it
  from the numbers; a monster's death adds carrion.
- **Hounds and bats hunt:** with nobody to fight, they run down the grazing animals near them.
  They come in packs as big as their prey allows: from half to one and a half times the usual
  (decided).
- **Catching up** (time away) moves the numbers on for the time away too: come back to a cave
  you hunted empty and the grass has grown, and the hares are coming back from next door.
- `test_worldgen` checks the numbers alone: without predators the grazers settle at what the
  plants feed (4.4 of 4.5); with predators both live on and stay within bounds; too many grazers
  eat the plants down and fall back. The bot log prints the numbers of the region the bot is in
  (`ECO`).

`--demo hunt`: hares on a patch of grass and a hound, you out of sight: the hound runs them down,
four hares in three seconds.

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

## Filling the gaps

What the first pass left undone:

- **Heat by material:** each 2 m cell passes heat on by what it is made of: air quickly, stone
  and earth slowly, wood more slowly still, metal fastest.
- **Swarms are saved** (save version 22), angry bees still after you included.
- **Throwing with a gamepad or a touch screen:** with the pickaxe out, the d-pad left and right
  (or the > button on a touch screen) choose what to throw, and the pickaxe's alternate attack
  throws it (on a keyboard too, besides C and X).
- **Animals swim:** hares, goats and the rest swim with their heads out, climb out at the edge,
  and drown when held under for eight seconds; crabs and salamanders are at home where they are.
- **Monsters dig:** a brute or troll chasing you breaks through rock in its way, a golem through
  masonry too (never what is protected); worms burrow as before.
- **Water in broken blocks:** a block broken open enough (8 of its 64 pieces or more, of stone,
  earth or wood) holds water in among its pieces: water flows in and out of it, is swum in and
  drawn as a surface inside it, and stays when the block is broken further. A cracked floor
  under a pool fills and drains on through.
- **Campfires:** P (or the d-pad down with the pickaxe out) lights a campfire from a bundle of
  firewood on the ground in front of you; it burns about two minutes, warms you and cooks. Wood
  and roots give firewood when chopped; the shop sells it; you start with two (save version 23).
- **Bots:** when COLD they go to a fire in sight, or light their own campfire when there is none
  they can reach; at a shop they buy firewood, a warming draught in the cold, a fur cloak once
  they can afford it and frost salt and oil flasks; in a fight they throw frost salt into a crowd
  and oil before a fire spell; and they harvest flowers, vines, fungus and moss on their way. (In
  the frozen north a bot that could not reach a fire lit its own and was warm two seconds later.)

## Air, currents, needs and farming

The ideas left for later in the plan:

- **Draughts:** gas clouds drift toward where the way is most open (out of rooms, up
  stairwells), looked at twice a second; heavy gas is slow to climb.
- **A fire needs air:** shut in (fewer than five open cells round it) it smoulders: it does not
  spread, it smokes and it burns slowly; with air all round it burns fiercer and spreads twice as
  fast, so a door opened on a smouldering room lets it flare.
- **Currents:** water runs toward lower water and over edges and falls where nothing holds it;
  that current carries players, monsters, animals and floating things. Running water wears the
  earth under it away, a few pieces at a time.
- **Monsters' needs:** with nobody to fight, those that breathe now and then go and drink at water
  or rest where it is warm (by a brazier or a fire), stay a while and come back. Whatever walks a
  way often tramples a path through the grass. Fungus that takes a body in the dark sometimes
  gets up: a **fungal slime** rises from the bones (never next to you).
- **Farming:** cut flowers for seeds (the shop sells them too; you start with three). With the
  pickaxe out, crouch and strike the earth (or moss, or ash) to sow: a **crop** (new block) grows
  from a shoot to young to ripe where it has light, water within a few cells and mild air (in the
  Sanctum's garden it is watered for you; frost kills it). Strike a ripe crop for grain, peppers,
  onions or melons, and two seeds back; flowers or a hive near it mean the bees reached it, and the
  harvest is doubled. Dig a channel from water to your field and the water runs to it (save
  version 24).

`--demo farm`: six crops by a channel of water with flowers near, growth sped up: all ripe in a
moment, reaped for 12 food (the bees doubled it) and ten more seeds.

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

After the living world (time away, temperature, materials, sound and sight, collapse, food
chain), the same four worlds for 12 minutes each: no crashes and no deaths. Areas the bots came
back to caught up 45 times (24 for the quester, who doubles back most); one collapse in all (a
bot's blast), so ordinary fighting does not bring the fortress down; the bots dug 21 times when
stuck. Between 109 and 440 cells of each world changed. The regions' numbers in the log look
like the places: a grassy cave by Ulwall feeds a grazer and a bit (42 plant cells), the
quester's corridors full of the dead it left have carrion 8.7 and insects to match.

## Not done yet

- Bots do not farm.
- Touch screens have no button for a campfire (keyboard P, gamepad d-pad down with the pickaxe
  out).
- Currents move water only; oil, acid and lava are not carried, and draughts move gas clouds,
  not heat.
- An emptied lair grows over only where plants can grow (earth, damp stone); bare masonry stays
  bare.
