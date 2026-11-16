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
**dragons**, of every kind, from the tiny faerie dragon to the ancient wyrm that may be the Old
King himself.

## 1. The Old King

### Who he is

One per fortress, chosen by the seed. **What he is stays hidden at first.** The first steps of
the way are about finding out. The ten kinds:

| # | Kind | What he is | His throne | How it can end |
|---|---|---|---|---|
| 1 | **The Tyrant** | the old cruel king who never died: iron crown, an army of knights | the Throne Hall, the highest hall of the castle | kill him; turn his knights against him (allegiances won); take the crown yourself |
| 2 | **The Necromancer** | a lich-king; his life is in a phylactery hidden elsewhere | the Necropolis in the ossuary | break the phylactery, then kill him; or bind him back into it |
| 3 | **The Old Mage** | an archmage who stopped time for himself; wards and duels of spells | the Tower of Stopped Hours above the clouds | break his four wards; out-duel him; or give him back his lost hour (an artefact) and let him age and rest |
| 4 | **The Dragon-King** | a king who became a dragon to live forever | the Dragon's Crown, a peak in the stormcrown | slay the dragon; or name him (his true name) so he remembers he was a man |
| 5 | **The Hollow King** | a ghost bound to the throne by an unfinished oath | the Empty Court in limbo | defeat his court and him; or bring his crown and his oath-ring and release him |
| 6 | **The Clockwork King** | an automaton that took the place of the king it served | the Great Engine in the orrery | stop the heart-engine (gnomes' help); or rewind it to the day it was made |
| 7 | **The Beast-King** | a cursed king who is more wolf than man | the Den under the roots | kill the beast; or cure him (elven and halfling remedies, an elixir) |
| 8 | **The Priest-King of the Sealed** | he worships what is sealed below and wants it out | the Black Altar at the deepest seal | kill him at the altar; or seal him in with what he worships; or let it out (the dark ending) |
| 9 | **The Alchemist-King** | lives on elixirs, mad and changing | the Still in the inferno | poison his elixir; or brew him the true one |
| 10 | **The King-in-the-Stone** | the first king, asleep under the fortress; maybe not evil at all | the Sleeping Hall, the deepest layer | wake him (he thanks you, the fortress ends its long night); keep him asleep (seal him for good); or take his place in the stone |

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
- at least **three endings**:
  - a fight: always open
  - a word: needs knowledge from the steps
  - an artefact: needs a thing from the steps
  - some kinds have a dark ending: take his place
- ties to the history:
  - he is who the builders served, who ordered the seal, or who profited from the night of the
    fall
  - the history generator gets one more actor, the king, and a few facts and clues about him
  - existing clues can point at him

### Winning

Settling with the Old King, by any ending:
1. shows the **ending**: a page of text that sums up the way, the choices on it, the ending
   chosen and what became of the fortress, the towns, the realms and the peoples
2. records the **victory** in the save and on a hall of victories in the Sanctum
3. lets you **play on** in a changed world:
   - the king's minions scatter: fewer named enemies, and the throne is empty or yours
   - the towns celebrate
   - the Astral Shore opens (instead of after the motive finale)
   - after a merciful ending, the Dream of the Sleeper opens
   - "a new fortress" from the start screen is the way to a new game

## 2. The way to him: ten steps

The way is generated from the seed like the history: a pure function, checked by a validator
and printed by the world map tool. **Ten steps**:

- **Steps 1-3: the call and who he is.** You learn there is a king, what kind he is and his name.
- **Steps 4-7: the means.** Allies, a weapon or artefact, knowledge, the way in.
- **Steps 8-9: the approach.** His lieutenants, his last guardians, the gate of his throne.
- **Step 10: the throne.** Him.

Each step:
- lies in a place that is farther out and more dangerous than the step before
- has a goal and a reward that the later steps or the ending need
- has a text in the voice of the world: who sent you, what is told, what you find

Some steps (4-7 mostly) can be done **in any order**. The tracker shows the open ones.

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
| **Dragon** | slay, outwit or befriend a dragon: its scale, its hoard, its word | dragons | a material, or a dragon ally |
| **Trial** | survive an arena of waves in a sealed hall | arenas | a key |
| **Seal** | open one of his seals: a gate at the end of a dungeon | portals, vaults | the way on |
| **Rescue** | bring back someone he holds: lost, guarded | towns' lost folk | a witness, an ally |
| **War** | choose a side between two factions in a domain, and win their war | domains, choices | a faction's road |
| **Descent** | reach a place: the deepest layer, the highest spire | strata | the way on, and a sight |

### How a way is made

1. The kind of king picks **required steps**:
   - the Necromancer: a *Relic* step for the phylactery
   - the Dragon-King: two *Dragon* steps
   - the Beast-King: a *Brew* step for the cure
2. The rest are filled from the pool by weight, with these rules:
   - no kind twice in a row and at most two of a kind
   - the first step is always a *Herald*
   - step 9 is always a *Lieutenant* or a *Trial*
   - step 10 is the throne
3. Places are picked like the chapter targets today: rings of distance and danger outward, and
   for kings of the depths downward, or of the heights upward. Each place is a real
   chunk with room for what happens there.
4. The texts come from grammars with the history's names, the king's name and the peoples.
5. The validator throws a way away and makes another if it fails any of these:
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

A new family of monsters, the largest in the game, in four sizes.

| Size | What | Where | How |
|---|---|---|---|
| **Faerie dragons** | tiny, harmless, bright | gardens, groves, the Garden realm | wildlife: they flit about, steal a coin and drop a gem |
| **Drakes** | dog-sized, no wings, in packs | anywhere past the first rings | monsters: bite, a short breath of their element |
| **Wyverns** | two legs, wings, the size of a horse | spires, aeries, storm, peaks | flying monsters: dive, sting with poison, carry off |
| **Dragons** | great, winged, intelligent | in **lairs** with hoards, a few per world | bosses: flight, breath, tail and wings |
| **Ancient wyrms** | huge and named, one or two per world | the deepest or highest lair, the realms | world bosses; one may be the Dragon-King |

### Kinds

Each kind has a colour, a breath tied to the simulation, a home and a temper. These were built
for the elements and they work for breath:
- fire sets things burning
- frost freezes water
- lightning arcs along metal and water
- poison gas drifts
- acid eats stone

| Kind | Breath | Home | Temper |
|---|---|---|---|
| **Red** (fire) | a cone of fire: burns, sets wood and grass alight | inferno, the Sun, the Forge-World | proud, hoards gold |
| **White** (frost) | frost: freezes water to ice, chills | the north, the Icelands | patient hunter |
| **Blue** (storm) | lightning: arcs along metal and water | stormcrown, the Storm Peaks | vain, can be flattered |
| **Green** (venom) | a cloud of poison gas | roots, fungal deep | cunning, bargains |
| **Black** (acid) | acid that pools and eats stone | the drowned west, the Bone Fields | cruel |
| **Gold** | a holy flame that heals its allies | heaven, the Shining Court | wise: can be talked to, gives quests |
| **Crystal** | beams of light that bounce | crystal abyss, the Fallen Star | curious, trades riddles |
| **Shadow** | darkness; it vanishes and comes back | limbo, the Black Moon | silent |
| **Bone** (dracolich) | a necrotic breath, raises the dead | ossuary, the Necropolis | undead; serves the Necromancer |
| **Iron** (clockwork) | steam and sparks | the orrery, gnomish halls | built, not born; can be stopped |
| **Sea dragon** | a wave that pushes and drowns | the Keep under the Ocean, the Sea of Night | swims |
| **Moon dragons** (pale, red, black) | moonlight, rust, void | the three moons | the moon-kings' mounts |
| **Hydra** | many heads, each a breath; a cut head grows back unless burned | swamps of the roots | a fight of its own |
| **Wyrm** | no wings; burrows, bursts from the ground | the Glass Desert, the deep | ambush |

### How they fight

- A **flight state machine**: perched, take off, circle, strafe (a breath run along a line), land,
  and fight on the ground (bite, claw, tail sweep, wing buffet that throws you back).
- Wounded, it flees to its lair and comes back.
- **Breath** is shown with a telegraph (the throat glows, the head rears) and goes through the
  simulation, so the world remembers it: scorch, ice, acid pools, burned forests.
- **Weak points**: the belly for a shot from below, the wings to ground it (crippled wings make
  it walk). Frost on a fire dragon hurts twice, and the reverse.
- **Talk**: gold, crystal, green and blue dragons can be spoken to before the fight:
  - a riddle, a bargain or a tribute of gold can avoid the fight
  - they can give a step of the way or the Dragon-King's true name
  - with a *Dragon* step: an ally at the throne

### Lairs and hoards

- A new megastructure, the **dragon's lair**: a huge cavern with a perch, bones of its prey and
  a **hoard** (piles of gold, chests and the world's legendaries).
- The dragon sleeps on the hoard. Sneak in (crouched, unseen) to steal a little, or wake it.
- **Eggs** in some lairs. One brought to the Sanctum hatches a **drake companion** that grows
  over time. This is later and optional, and would need the companion work.

### Model

All dragons are drawn by one generic, data-driven drawer:
- a body
- a neck and a tail as chains of segments that follow the motion
- a head with a moving jaw
- four or two legs
- wings with an animated stroke
- horns, frills and spines by kind

Sizes go from 0.3 to 12 m by a scale, and colours come from the kind. A dragon in flight also
casts a shadow on the ground and a dark shape against the sky.

### Sounds

New ElevenLabs sound effects through the sound tools, generated only with your go-ahead:
- roars (small, great, ancient)
- wing beats
- a breath for each element
- the faerie dragons' chirp

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
- The ending page, the victory record, the changed world after, the realm gates rewired, and a
  hall of victories in the Sanctum.

### Phase 4: dragons

- The monster family:
  - species and sizes in a table
  - flight and ground AI
  - breath through the simulation
  - weak points
  - drawing
- Drakes and wyverns in the families of the strata; faerie dragons in the wildlife.
- Lairs and hoards (megastructure, loot); dragons of the realms (one per realm where it fits).
- Talking dragons; *Dragon* steps; the Dragon-King's throne fight.
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

## 5. Open questions

1. **After winning**: play on in the same world (planned), or should the run end with credits?
2. **Order of the steps**: strictly one after the other, or the middle steps (4-7) in any order
   (planned)?
3. **His identity**: hidden until found out (planned), or known from the start?
4. **Length**: how long should the way take? Steps scaled for roughly 6-10 hours seems right for
   the current pacing.
5. **Dragon companions** (hatched eggs, perhaps a mount): in scope now, or later?
6. **The dark endings** (taking his place, letting the sealed out): in, or only the heroic ones?
