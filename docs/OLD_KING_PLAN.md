# The Old King and the dragons: concept and plan

Today each fortress has stories but no end the whole game leads to. The nine chapters of a
motive end with the opener of the seal, and that ending feels like a side road. This plan adds
**one main storyline per fortress: the way to the Old King**. Ten generated steps, like chapters,
lead to him; reaching him and settling with him **wins the game**. Everything that exists stays:
- the motive chapters
- the history's mystery
- domains, lords, bounties
- towns and realms

They become what gives the world its feeling, and the side quests. The plan also adds
**dragons**: hard enemies and bosses of every kind, huge and terrifying, with hoards of loot, up
to the ancient wyrm that may be the Old King himself.

**Decided** (answers to the first version's questions):
- **Winning** shows the credits, then you choose to explore on in the same world.
- **The first steps are not in order**: steps 1-3 can be done in any order, and so can steps 4-7.
  Steps 8-10 come one after the other.
- **His identity is hidden** until the steps reveal it.
- **Length**: 6-10 hours to the throne.
- **Dragons are enemies only**: no companions, eggs or mounts, and no dragon allies. They are
  bosses, huge and terrifying, with tons of loot, of all kinds.
- **Heroic endings only** for now: no taking his place, no letting the sealed thing out.

## 1. The Old King

### Who he is

One per fortress, chosen by the seed. **What he is stays hidden at first.** The first steps of
the way are about finding out. The ten kinds:

| # | Kind | What he is | His throne | How it can end |
|---|---|---|---|---|
| 1 | **The Tyrant** | the old cruel king who never died: iron crown, an army of knights | the Throne Hall, the highest hall of the castle | kill him; turn his knights against him (allegiances won); break the iron crown (an artefact) and free the bound |
| 2 | **The Necromancer** | a lich-king; his life is in a phylactery hidden elsewhere | the Necropolis in the ossuary | break the phylactery, then kill him; or bind him back into it |
| 3 | **The Old Mage** | an archmage who stopped time for himself; wards and duels of spells | the Tower of Stopped Hours above the clouds | break his four wards; out-duel him; or give him back his lost hour (an artefact) and let him age and rest |
| 4 | **The Dragon-King** | a king who became a dragon to live forever | the Dragon's Crown, a peak in the stormcrown | slay the dragon; or name him (his true name) so he remembers he was a man |
| 5 | **The Hollow King** | a ghost bound to the throne by an unfinished oath | the Empty Court in limbo | defeat his court and him; or bring his crown and his oath-ring and release him |
| 6 | **The Clockwork King** | an automaton that took the place of the king it served | the Great Engine in the orrery | stop the heart-engine (gnomes' help); or rewind it to the day it was made |
| 7 | **The Beast-King** | a cursed king who is more wolf than man | the Den under the roots | kill the beast; or cure him (elven and halfling remedies, an elixir) |
| 8 | **The Priest-King of the Sealed** | he worships what is sealed below and wants it out | the Black Altar at the deepest seal | kill him at the altar; or seal him in with what he worships; or turn his own faithful from him (his true name spoken at the altar) |
| 9 | **The Alchemist-King** | lives on elixirs, mad and changing | the Still in the inferno | poison his elixir; or brew him the true one |
| 10 | **The King-in-the-Stone** | the first king, asleep under the fortress; maybe not evil at all | the Sleeping Hall, the deepest layer | wake him (he thanks you, the fortress ends its long night); keep him asleep (seal him for good); or defeat the dream-guardians that hold him and let him rest |

Each kind has:
- a generated name and title from the history: "Aldric the Ninth, the Iron Crown of Ostgard"
- a look:
  - a big humanoid boss with crown, robe or armour by kind
  - the Dragon-King is a dragon (section 2)
  - the Clockwork King is brass and gears
- two or three fight phases with mechanics of their own, such as:
  - wards to break
  - minions on the throne steps
  - a phylactery shield
  - a gas phase
  - a summoned dragon
- at least **three endings**, all of them heroic:
  - a fight: always open
  - a word: needs knowledge from the steps
  - an artefact: needs a thing from the steps
- ties to the history:
  - he is who the builders served, who ordered the seal, or who profited from the night of the
    fall
  - the history generator gets one more actor, the king, and a few facts and clues about him
  - existing clues can point at him

### Winning

Settling with the Old King, by any ending:
1. shows the **ending**: a page of text that sums up the way, the choices on it, the ending
   chosen and what became of the fortress, the towns, the realms and the peoples
2. rolls the **credits**, then asks: *explore on* (the default) or *back to the start screen*
3. records the **victory** in the save and on a hall of victories in the Sanctum
4. if you explore on, the world has changed:
   - the king's minions scatter: fewer named enemies, and the throne is empty or yours
   - the towns celebrate
   - the Astral Shore opens (instead of after the motive finale)
   - after a merciful ending (a word or an artefact), the Dream of the Sleeper opens
   - the chronicles, towns, realms and dragons go on as before
   - "a new fortress" from the start screen is the way to a new game

## 2. The way to him: ten steps

The way is generated from the seed like the history: a pure function, checked by a validator
and printed by the world map tool. **Ten steps**:

- **Steps 1-3: the call and who he is.** You learn there is a king, what kind he is and his name.
- **Steps 4-7: the means.** Allies, a weapon or artefact, knowledge, the way in.
- **Steps 8-9: the approach.** His lieutenants, his last guardians, the gate of his throne.
- **Step 10: the throne.** Him.

**Order**: steps 1-3 are open together and can be done in any order, and so are steps 4-7; each
group opens when the one before it is done. Steps 8, 9 and 10 follow one after the other.

Each step:
- lies in a place that is farther out and more dangerous than the steps before it
- has a goal and a reward that the later steps or the ending need
- has a text in the voice of the world: who sent you, what is told, what you find

The tracker shows the nearest open step; the STORY tab shows all the open ones.

### Kinds of steps (the pool)

| Step | What you do | Built on | Gives |
|---|---|---|---|
| **Herald** | find someone who knows: a town's elder, a dying knight, a resident of the Sanctum | towns, traces | the next places |
| **True name** | read pages in a library, a stele, the Library of Ash | traces, realms | the word ending; a weakness |
| **Lieutenant** | kill (or spare) one of his named servants | named enemies, parley | a key, his orders |
| **Relic** | take an artefact piece from a vault behind a puzzle | vaults, mechanisms | the artefact ending |
| **Allegiance** | win a people's help: friend of a town, their task chain | towns, reputation | allies at the throne: soldiers fight beside you |
| **Forge** | make the king-breaker at the Sanctum's forge from rare metal | forge | a weapon that ignores his ward |
| **Brew** | brew an elixir or antidote (the cure, the poison) | alchemy | the cure or poison ending |
| **Realm** | go through the Gate and bring something back: a ruler's crown, a moon flower | realms | a key or a material |
| **Dragon** | slay a dragon in its lair | dragons | its heart, scales or a key from its hoard |
| **Trial** | survive an arena of waves in a sealed hall | arenas | a key |
| **Seal** | open one of his seals: a gate at the end of a dungeon | portals, vaults | the way on |
| **Rescue** | bring back someone he holds: lost, guarded | towns' lost folk | a witness, an ally |
| **War** | choose a side between two factions in a domain, and win their war | domains, choices | a faction's road |
| **Descent** | reach a place: the deepest layer, the highest spire | strata | the way on, and a sight |

### How a way is made

1. The kind of king picks **required steps**:
   - the Necromancer: a *Relic* step for the phylactery
   - the Dragon-King: two *Dragon* steps (slaying his brood)
   - the Beast-King: a *Brew* step for the cure
2. The rest are filled from the pool by weight, with these rules:
   - no kind twice in a row and at most two of a kind
   - one of the first three steps is always a *Herald*
   - step 9 is always a *Lieutenant* or a *Trial*
   - step 10 is the throne
3. Places are picked like the chapter targets today: rings of distance and danger outward, and
   for kings of the depths downward, or of the heights upward. Each place is a real
   chunk with room for what happens there.
4. Steps 1-3 lie at about the same danger, and so do steps 4-7; each group is a ring farther out.
5. The texts come from grammars with the history's names, the king's name and the peoples.
6. The validator throws a way away and makes another if it fails any of these:
   - every place exists and can be reached
   - every ending's needs are met by some step
   - the danger rises
   - no two places coincide

Like the history, `worldmap king` prints the way of a seed and `test_worldgen` checks hundreds
of seeds.

### The existing stories

- **The motive chapters stay.** They are renamed *chronicles*, side quests with their own
  rewards. Their finale (the opener) stays, but it no longer ends anything.
- They can **meet the main way**. A chronicle target can be one of the king's lieutenants, and
  the history's opener can serve the king. Doing one then counts for both.
- The **STORY tab** gets a first page, *The Old King*: the steps done and open, what you know of
  him, the endings within reach. The chronicles, lore and deeds follow.
- The **tracker** shows the main step in gold on top and the chronicle's target under it, as
  today. A town task stays on the third line.

## 3. Dragons

**Enemies only, and the hardest in the game**: huge, terrifying bosses of every kind, with tons
of loot. No companions, no eggs, no mounts, no allies, no bargains.

### Sizes

| Size | What | Where | How they fight |
|---|---|---|---|
| **Dragonets** | cat-sized, in swarms of 6-12 | gardens, groves, the Garden realm, crystal halls | a swarm that bites and spits sparks of its element; quick and hard to hit |
| **Drakes** | horse-sized, no wings, in packs of 3-5 | anywhere past the first rings | charge, bite, short breath of their element |
| **Wyverns** | two legs, wings, 6-8 m across | spires, aeries, storm, peaks, realms | dive from the air, poison sting, snatch and drop you from a height |
| **Dragons** | 15-25 m, winged | lairs with hoards, several per world | bosses: flight, breath, claws, tail, wings; three phases |
| **Ancient wyrms** | 40 m and more, named | the deepest and highest lairs, one in some realms | world bosses: the world shakes; one may be the Dragon-King |

Size is the point: a dragon fills the cavern it lives in. Its tread shakes the screen and drops
dust and loose stone (the collapse rules). Its roar is heard chunks away, and every monster near
wakes. Its shadow passes over you before you see it.

### Kinds

Each kind has a colour, a breath tied to the simulation, a home and a way of fighting. These were
built for the elements and they work for breath:
- fire sets things burning
- frost freezes water
- lightning arcs along metal and water
- poison gas drifts
- acid eats stone

| Kind | Breath | Home | How it fights |
|---|---|---|---|
| **Red** (fire) | a cone of fire: burns, sets wood and grass alight | inferno, the Sun, the Forge-World | sets its lair ablaze, fights from the flames |
| **Magma** | molten rock that cools to stone and walls you in | the core, volcanic halls | slow, armoured; breaks the floor into lava |
| **White** (frost) | frost: freezes water to ice, chills | the north, the Icelands | freezes the floor slick, attacks from snowstorms |
| **Blue** (storm) | lightning: arcs along metal and water | stormcrown, the Storm Peaks | strikes everything in water or armour; calls lightning |
| **Green** (venom) | a cloud of poison gas | roots, fungal deep | fills its lair with gas, waits in it |
| **Black** (acid) | acid that pools and eats stone | the drowned west, the Bone Fields | melts the pillars you hide behind |
| **Swamp** | rot and flies | the roots' marshes | sinks into the muck and rises behind you |
| **Gold** | a holy flame | heaven, the Shining Court | heals itself in the light; its scales turn spells |
| **Crystal** | beams of light that bounce off its lair's crystals | crystal abyss, the Fallen Star | beams that ricochet; immune where the crystal shines |
| **Amethyst** | force: a blast that throws you | crystal halls, the orrery | gravity: pulls you in, throws you off ledges |
| **Shadow** | darkness; it vanishes and comes back | limbo, the Black Moon | puts out the lights, strikes from the dark |
| **Bone** (dracolich) | a necrotic breath that raises the dead | ossuary, the Necropolis | rises again once unless its bones are burned |
| **Iron** (clockwork) | steam and sparks | the orrery, gnomish halls | built, not born: armour plates to break off |
| **Rust** | rust: corrodes your armour and weapons for a while | the Red Moon, iron hells | eats metal; best fought with bare magic |
| **Sand** | a scouring blast that blinds | the Glass Desert, dunes | swims in the sand, a sandstorm round it |
| **Sea dragon** | a wave that pushes and drowns | the Keep under the Ocean, the Sea of Night | fights from the water, drags you under |
| **Cloud** | wind | the clouds, the Shining Court | invisible in its fog; blows you off the islands |
| **Void** (astral) | a breath that pulls you toward it | the Astral Shore | warps space: blinks and comes from any side |
| **Moon dragons** (pale, red, black) | moonlight, rust, void | the three moons | the moon-kings' war-beasts |
| **Hydra** | many heads, each a breath; a cut head grows back unless burned | swamps of the roots | grows heads as it is hurt |
| **Twin-headed** | two breaths of different kinds | deep halls | each head its own element and will |
| **Wyrm** | no wings; burrows, bursts from the ground | the Glass Desert, the deep | ambush from below, coils |
| **Chaos dragon** | a breath that changes its element each time | the farthest lairs, the Dream | the hardest of all |

### How they fight

- A **flight state machine**: perched, take off, circle, strafe (a breath run along a line), land,
  and fight on the ground:
  - bite and claw
  - a tail sweep behind it
  - a wing buffet that throws you back
  - a stomp that shakes loose stone from the ceiling
- **Three phases** for great dragons and wyrms:
  1. in the air
  2. grounded and furious (more breath, faster)
  3. desperate (the lair comes down: collapse, fire everywhere, the hoard melting)
- **Breath** is shown with a telegraph (the throat glows, the head rears) and goes through the
  simulation, so the world remembers it: scorch, ice, acid pools, burned forests.
- **Weak points**: the belly for a shot from below, the wings to ground it (crippled wings make
  it walk). Frost on a fire dragon hurts twice, and the reverse.
- **They hunt**: a wounded dragon does not flee for good. It comes back healed, and a woken one
  can leave its lair and find you in the halls nearby.
- **Their roar** wakes and frightens lesser monsters, and its brood (drakes, dragonets) answer it.

### Lairs and hoards: tons of loot

- A new megastructure, the **dragon's lair**: a huge cavern of several chunks with a perch, the
  bones of its prey, scorched or frozen walls by kind, and the **hoard**.
- **The hoard** is the best loot in the game:
  - piles of gold that you walk through to gather (thousands of gold)
  - many chests by the size of the dragon
  - always at least one of the world's legendaries; an ancient's hoard has two or three
  - keystones
  - **dragon materials** for the forge and alchemy: scales (armour runes), a heart (the strongest
    essence), teeth and claws (weapon bases), blood (an elixir of the dragon's element)
  - new legendary weapons of dragon-slayers past, found only in hoards
- **Sneaking**: the dragon sleeps on its hoard. Crouched and unseen, you can take a little before
  it wakes; every step on the gold may wake it.
- **Dragonslayer**: each slain dragon is a trophy in the Sanctum's armory (its skull) and a
  title on the Roll.

### Model

All dragons are drawn by one generic, data-driven drawer:
- a body
- a neck and a tail as chains of segments that follow the motion
- a head with a moving jaw
- four or two legs
- wings with an animated stroke
- horns, frills, spines and scales by kind
- the many heads of hydras

Sizes go from 0.3 to 40 m by a scale, and colours come from the kind. A dragon in flight also
casts a shadow on the ground and a dark shape against the sky.

### Sounds

New ElevenLabs sound effects through the sound tools, generated only with your go-ahead:
- roars (dragonet, great, ancient)
- wing beats
- a breath for each element
- the tread of a great dragon
- the dragonets' screech

## 4. Implementation plan

In five phases, each one playable on its own, with tests and a bot batch after each.

### Phase 1: the king and his way (world side)

- `src/world/king.h`, `king.c`: the ten kinds (names, titles, looks, thrones, endings, needs),
  the step pool, the generator of a way, the validator, the texts and grammars.
- The history gets the king as an actor, with facts and clues about him.
- `worldmap king`; `test_worldgen` over hundreds of seeds: every way is valid, every step
  reachable, the endings satisfiable, no duplicates.

### Phase 2: the steps in play

- `src/game/king.c`:
  - progress per world (save version 27)
  - the handler of each step kind: what spawns, what counts as done, what it gives
  - the tracker and the STORY tab page
- The motive chapters renamed chronicles; the links between chronicle targets and lieutenants.
- Demos: `--demo kingtest` walks a seed's way with the debug moves and logs each step.

### Phase 3: the throne and the endings

- A **throne** megastructure per kind:
  - hall
  - necropolis
  - tower
  - peak
  - court
  - engine
  - den
  - altar
  - still
  - sleeping hall
- The ten Old King bosses: model, phases, mechanics, the talk at the throne (parley as today,
  with the endings open to you).
- The ending page and the credits, then *explore on* or *to the start screen*; the victory record,
  the changed world after, the realm gates rewired, and a hall of victories in the Sanctum.

### Phase 4: dragons

- The monster family:
  - species and sizes in a table
  - flight and ground AI
  - breath through the simulation
  - weak points
  - drawing
- Dragonets, drakes and wyverns in the families of the strata and the realms.
- Lairs and hoards (megastructure, loot, dragon materials for the forge and alchemy, dragon-slayer
  legendaries); the great dragons and ancient wyrms; dragons of the realms where they fit.
- Screen shake, roars that wake, collapse in the third phase; trophies and titles.
- *Dragon* steps; the Dragon-King's throne fight.
- Sounds (with your go-ahead).

### Phase 5: polish and balance

- The bot follows the main way (a policy per step kind) to test that a way can be done end to
  end.
- Batches on many seeds: time to the throne, deaths per step, difficulty by step.
- Screenshots, docs (`OLD_KING.md`, `DRAGONS.md`), README.

### Effort

The phases in order of size: 3 and 4 are the largest (new bosses, a flying AI, a new model),
2 is next (fourteen step handlers), 1 and 5 are smaller. The order above is the order of
dependence; dragons could also come first, since phase 4 does not depend on 1-3 except for the
*Dragon* steps.

## 5. Still open

- **Sounds**: the dragon sounds and the Old King's voice need your go-ahead before they are generated.
- **Where to start**: phase 1 (the king and his way) is planned first; the dragons (phase 4) could
  come first instead, as they do not depend on the story except for the *Dragon* steps.
