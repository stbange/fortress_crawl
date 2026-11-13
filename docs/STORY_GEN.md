# Fortress Crawl: Generated Stories

A design for stories that differ from world to world in what happens, not only in the names.
Written before any of it is built; nothing here is in the game yet. It extends
[STORY.md](STORY.md), which describes the story system as it is today.

Status: proposal. Effort about 2½-3 weeks, plus about one week if player choices are included
(see "Decisions").

## The problem

The story system already generates a lot: the fortress, its builders, what it seals, domains
with factions and lords, a named captain, general and Power per arc, inscriptions, notes and
bounty boards. But the metaplot is one fixed script. Every world reveals the same secret in
the same order, from the `REVEAL[4][3]` table in `src/world/story.c`:

1. sealed orders bearing the Power's sign; "the gate was not broken, it was opened from
   inside"; the Power, dying: "I only opened the door. Ask {next} who walked through";
2. the builders' brand; "they did not seal it, they fed it"; "we kept it fed";
3. "the body is older than the fortress"; "the fortress is the cage, and we are the bait";
4. "its blood runs upward"; "at the bottom the dead are not dead"; "there is no bottom".

The four motives (revenge, glory, riches, truth) also follow one fixed chain each: the target
always holds the next coin, the next name, the next piece of the map or the next page. The
twenty bot runs in [RUN_STORIES.md](RUN_STORIES.md) and [RUN_STORIES_2.md](RUN_STORIES_2.md)
showed it: Cedmark and Aldburg told the same story with different names. Someone who plays
two worlds will notice in the first hour.

The inscriptions and notes are also independent of each other: each is a template filled with
the local domain's names. Nothing a stele says is ever confirmed or contradicted elsewhere.

## Goals

- **Different events per world.** Who opened the gate, why, what lies below, and who is guilty
  should change between worlds, as should the shape of the mystery itself.
- **Not trivial.** The mystery has a causal chain of several steps, at least one turn the
  player does not expect, and people with reasons rather than villains with titles.
- **Consistent.** Every text in a world describes the same history. A stele far out and a
  confession two chapters later agree, or disagree for a reason (a liar, a partisan, a
  rumour).
- **Discoverable.** The truth can be pieced together from what you find; the game does not
  just announce it at the end.
- **Measurable.** A tool reports the variety over many seeds and finds contradictions.

## Constraints (unchanged from today)

- **Deterministic.** The same seed gives the same story, in every build, offline and on the
  web. No language model at runtime.
- **A function of the seed.** Today nothing is simulated; every piece of story is computed
  from the seed and a position on demand. The new history is simulated, but only once per
  world, at world start, and only from the seed: it is still a pure function of the seed, just
  computed eagerly and cached in `World`. Chunks still derive their traces on demand; they
  look up the cached history instead of an independent template.
- **Cheap.** Generation must take under about 10 ms (native) and the history under about
  64 KB, so world start and the web build are not affected.
- **Saves stay compatible.** A world saved before this change keeps its old story (see
  "Saves").
- **Short, grim fragments.** No long dialogue. The dungeon speaks through its dead.

## The approach in one paragraph

Generate a hidden history first, then let all story text be reports about that history. At
world start a small simulation runs a few eras of the fortress's past: people and factions
with traits, goals and relations take actions with preconditions and effects (found, seal,
make a pact, betray, usurp, steal, flee, open the gate). The result is a graph of events and
the facts they establish. The game then searches that graph for the best mystery, casts its
actors as the arcs' named enemies, and distributes the facts as clues over chapters, traces,
bounties and legendaries. Text comes from small grammars, one per kind of fact, with voices
for the speaker. The player's motive decides which thread of the same history they follow.

## 1. The world's history

### Data model

All of this lives in a new pure module, `src/world/history.c` / `history.h`, in the world
library next to `story.c`, testable without the game.

| Thing | Count per world | What it has |
|-------|-----------------|-------------|
| **Actor** | 20-40 | name, culture, faction, role (founder, smith, abbot, heir, soldier, merchant, thief, prisoner...), traits (ambitious, devout, greedy, loyal, cowardly, curious, cruel, merciful: 2 each), a goal, alive or dead and how, birth and death era |
| **Faction** | 4-8 | name, culture, creed (keepers, miners, priests, soldiers, traders, heretics), stance to each other faction (-3..3), leader, strength, where it sits (domains) |
| **Relation** | ~60 | between two actors: kin (parent, sibling, spouse), oath, debt, love, rivalry, grudge; with a strength |
| **Artifact** | 6-12 | a key, a seal, a book, a crown, a map, a weapon: who made it, who held it when, where it is now. The world's legendaries become artifacts here |
| **Place** | 8-16 | sites of events, tied to real domains and directions (the chapel, the third gate, the drowned vault) |
| **Event** | 30-80 | type, era, actors in roles (agent, victim, witness, accomplice), place, artifacts, causes (earlier events), effects |
| **Fact** | 100-250 | a proposition the event establishes: `OPENED(gate, Ysolde)`, `KIN(Ysolde, Marrec)`, `PAID(Ulric, fire at Cedbrook)`, `BELIEVES(Grey Abbots, BETRAYED(Marrec))`. Each fact has a truth value: true, false (a lie), or a belief |

Facts are what the player learns. Everything the game says is one or more facts, rendered.

### The simulation

Five to seven eras, matching the strata the world already names (the Sealing, the War of
Fire, the Drowning, the First Hearths, the Deep Delving, the Last Siege, the Fall). Each era:

1. **Needs.** Every living actor with a goal scores the actions open to it (`want`),
   using its traits, relations and its faction's state. Greedy actors want the vault, devout
   ones want the seal kept, ambitious heirs want the crown, aggrieved ones want revenge.
2. **Actions.** A seeded random choice among the best few actions per era (not always the
   best: people make mistakes). An action applies only if its preconditions hold.
3. **Effects.** The action changes the world: relations shift (a betrayal turns kin into a
   grudge), factions gain or lose strength, artifacts change hands, people die, places fall.
   Each effect is recorded as facts, with the event as their cause.
4. **Witnesses.** Some actors see the event (and remember it truly), some hear of it (and may
   remember it wrong), and some factions retell it in their favour (lies, written down).

The event library, about 30 types to start, each with preconditions, effects and the facts
it records:

| Group | Events |
|-------|--------|
| Founding | found the fortress; dig too deep; find what is below; seal it; make the key |
| Power | crown, usurp, abdicate, exile, assassinate, poison, rule as regent |
| Faith | found a cult; declare a heresy; purge; martyr; claim a miracle |
| Bonds | marry, swear an oath, betray an oath, adopt, disown, take a hostage |
| Greed | hide a hoard, steal an artifact, forge a document, bribe, take a tithe |
| War | declare war, besiege, massacre, surrender, sign a truce, break a truce |
| The thing below | make a pact with it; feed it; hear it; become its herald; study it |
| The fall | open the gate; fail to close it; flee; stay behind; burn the evidence |
| The present | burn a village (the player's); put a name on the Roll; send hunters |

**The thing below is part of the history, not a constant.** It can be a sealed god, a
prisoner, a plague, a machine of the builders, an older people, or nothing at all (see
"Tone"). What it is decides which events are possible: one does not make a pact with a
plague, and one does not feed nothing. The world's `sealed` name stays; what it means changes.

**Constraints that make the history a good one.** The simulation is rerun with the next
sub-seed (up to 8 times) if the result is dull: fewer than three events in the chain that
leads to the fall, no betrayal or reversal anywhere, or a cast where nobody important is alive
in the present era. This keeps every world above a floor of interest.

### The present era

The last era is the player's. The survivors of the history, together with their heirs and
followers, are alive in the dungeon now: these become the named enemies. Their goals continue:
someone is still hiding a crime, someone still wants the vault, someone is still feeding the
thing below, someone wants the player dead because of what the player might learn. The
player's village and kin (from `story_personal`) are placed into the history in this era too:
whoever burned the village did it for a reason the history explains.

## 2. Choosing the mystery

The history usually holds several possible stories. A pattern search picks the best one as
the world's metaplot. Each **plot shape** is a pattern over the event graph:

| Shape | Pattern | The turn |
|-------|---------|----------|
| **The betrayal** | A opened the gate; A had an oath or kinship to B; B is blamed | The one everyone blames is the victim |
| **The usurpation** | A killed or exiled the rightful heir B; A rules; B's line survives | The ruler is a usurper, and the heir is someone you met |
| **The pact** | A made a pact with the thing below; the price is due now | The fall was the payment, and there is more to pay |
| **The cage was bait** | The fortress was built to lure something, not to hold it | The builders wanted it to come |
| **The wrong monster** | The thing below is blamed; a person did it | Nothing came up through the gate; someone went down |
| **The mercy** | A did a terrible thing to prevent a worse one | The villain was right, or half right |
| **The cover-up** | The fall is a lie told by the faction that profits | The records are forged; the steles contradict the books |
| **Your village** | The player's village or kin take part in the history | The fire was revenge for something your kin did |
| **The forgotten heir** | An artifact proves someone's right to the fortress | It may be you, or the person you just killed |

Each candidate is scored on:

- **chain length** (the number of causal steps from the deep past to the present, 3-7 best);
- **surprise** (how far the truth is from the first belief a player meets: a betrayal by kin
  scores higher than one by a rival; a sympathetic villain higher than a cruel one);
- **cast** (how many of its actors are alive in the present and can be met);
- **fit** (whether the world can place its sites: directions, depth and height that the world
  generator actually provides).

The best shape becomes the **main mystery**, spread over the first three or four arcs. The next
best one or two become **secondary threads** that the motives and the traces carry. After the
mystery is solved, later arcs continue as today: new Powers, with smaller stories drawn from
the rest of the history.

## 3. Casting: who the named enemies are

Today a chapter target is a generated name with a rank. In the new version, each arc's
captain, general and Power are **actors from the mystery**, chosen by their role in it:

- The **captain** is someone at the edge of the crime: a soldier who held the door, a clerk who
  forged the ledger, a witness who sold their silence.
- The **general** is closer: the accomplice, the hand that did it, the one who paid.
- The **Power** is the one who decided, or the one the mystery turns on. Sometimes the Power is
  not the culprit, and its death is the turn: "I kept the gate shut for forty years. Who do you
  think opened it?"

Their culture, home direction and monster family still come from the arc's direction, as
today, so the world generator does not change; the history is generated with those
constraints, so a northern Power is an actor of the northern culture. Their titles come from
their role in the history ("the Oathbreaker", "Keeper of the Third Key", "the Last Heir").

**Domain lords** come from the history's factions and their surviving members, so a lord's
faction matches what the steles in its domain say about it.

**Legendaries become artifacts.** The world's unique weapons (`arms_unique`) get their maker,
owners and deeds from the history: the blade that cut the bar of the chapel door, the bow the
usurper's brother carried. Their lore lines become clues. This also fixes the duplicate
legendary drops seen in the runs, because each artifact has exactly one place in the present.

## 4. Motives: four ways into one history

The four motives stay, but each follows a thread of the same history instead of a fixed chain:

- **Revenge** follows the burning of your village back through the history: the one who held
  the bar, the one who paid, and why: a debt, an old grudge against your kin, or an order to
  silence what your kin knew.
- **Glory** follows the Roll of the Dead, which is now a real list in the history: who earned
  each name, and what they did to earn it. Climbing it reveals that the Roll is also a record
  of the crime.
- **Riches** follows a hoard someone hid in the history: who hid it, from whom, who stole it
  since. The map's pieces are held by the people who touched it.
- **Truth** follows the books: the faction chronicles, a forged one among them, and the real
  account. Truth hears the most lies and has to tell them apart.

All four arrive at the main mystery by different roads, so two characters with different
motives in the same world, or two players in co-op, tell each other different halves.

## 5. Clues: how the truth is found

Every fact of the mystery gets a **clue budget**: the important facts (the culprit, the turn)
at least three clues each, minor ones one or two. The clues are spread over:

| Source | Carries | Reliable? |
|--------|---------|-----------|
| Chapter targets: notes on the body, dying words | the fact the arc is about | yes, but dying people lie sometimes (by trait) |
| Steles (inscriptions) | what a faction wanted remembered | partisan: a faction's steles praise it and blame its enemies |
| Corpses' notes | what an adventurer saw | true, but partial |
| Bounty boards | who someone wants dead, and why | the reason is true; the accusation may not be |
| Legendaries' lore | who made and carried it | true |
| Sanctum library (books bought or found) | chronicles of factions | one in each world is forged |
| Hunters' patrols (from the story's target) | orders found on the leader | true; also, as the runs suggested, the target's whereabouts |

**Contradiction is a feature.** When two sources disagree, at least one more source can settle
it: a stele says the Grey Abbots sealed the gate in the Drowning; a corpse's note says the
abbots' seal was broken from inside; a legendary's lore says who forged the abbots' key. The
validator (below) checks that every contradiction has a resolution the player can find.

**Placement.** Clues are put in traces by distance and direction: early facts close to the
gate, later ones out along the arcs. A trace in a domain prefers facts about that domain's
faction. About a third of traces still carry pure atmosphere ("Do not answer if it calls you by
name"); the rest carry facts.

**Order.** The chapter revelations follow the shape's order: first the false belief, then the
crack in it, then the turn, then the cost. This replaces `REVEAL[4][3]`.

## 6. Text: grammars, not fixed lines

Each fact type has a small grammar (the existing `expand()` with `{key}` and `#sym#` is enough
to start, extended with conditions) with several phrasings and **voices**:

- **Witness**, first person: "I held the lamp while she turned the key. She was crying."
- **Partisan**, a faction's stone: "Here the Grey Abbots held the seal for nine generations.
  The traitor Marrec broke it."
- **Record**, a ledger or an order: "Paid to Ulric: forty crowns, for the matter of
  Cedbrook."
- **Rumour**, uncertain: "They say the heir did not die in the flood."
- **Dying words**, short and personal: "Tell Ysolde I kept the key. Tell her I am sorry."

The voice is chosen from the source and the speaker's traits; the tone from the era and the
culture (northern stones are terse, southern ones speak of fire, the old culture is formal).
Each grammar has 4-8 alternatives per voice so that two worlds with the same shape still read
differently. About 150 grammar entries in all to begin with; the existing steles and notes
stay as atmosphere entries.

The capitalisation fixes in `expand()` stay. Text stays within `STORY_TEXT` (320 bytes).

## 7. The STORY tab: what you know

The tab gets a section **"What you know"**: the mystery's facts the player has found, each as
one line with its sources, grouped by person. Contradictions are shown as such ("The Grey
Abbots say Marrec broke the seal. A dead delver says it was broken from inside."). Open
questions are listed: "Who opened the gate?", "Where is the heir?" Each question closes when
enough clues have been found, with a short banner. The arcs and the current target stay as
they are.

There are no menus for deduction and no accusations to make (unless player choices are
included, see "Decisions"); the satisfaction is in the pieces fitting together.

## 8. Tone

Today every world is a cosmic horror: something below, older than the fortress. The history
model allows more, chosen per world:

| Tone | Share | What lies below |
|------|-------|-----------------|
| Cosmic horror | ~55% | something old and hungry, as today |
| Human drama | ~25% | nothing, or something harmless: the horror was people (a mine of prisoners, a plague hidden for profit, a war covered up) |
| Tragedy | ~20% | something that was wronged: the thing below was the first people, imprisoned by the builders |

The monsters are the same in every world; what changes is what the dead say about them.

## 9. Saves

The new story replaces the old only in **new worlds**. The world save gets a `story_gen` byte:
0 for the existing generator, 1 for the new one. Old worlds keep the old story exactly, and the
old code stays in `story.c` behind that byte. Per character, the save adds the fact ids the
character has found (a bitset of 256 bits: 32 bytes) and the closed questions. Save format
version 13.

## 10. Tools and measurements

- `worldmap story` prints the world's history (eras, events, actors and relations), the
  candidate shapes with their scores, the chosen mystery, the casting and the clue
  placement, so a person can read the hidden truth of any seed.
- `worldmap storystats N` generates N worlds (1,000 by default) and reports: the distribution
  of shapes, tones and chain lengths; how many distinct culprits by role; the share of worlds
  where the culprit is the first suspect (should be low); generation time; and the
  validator's failures.
- **The validator** (also in `test_worldgen`, over 200 seeds): every main fact has at least its
  budget of clues; every contradiction has a resolving clue; no actor acts after their death;
  each artifact is in one place; every cast actor has a reachable site; no text overflows.
- **Bot story batches**: ten quester runs on new worlds, written up as in RUN_STORIES.md, to
  read how the stories feel in play.

## Example histories

These are sketches written by hand of what the generator should produce, to make the target
concrete. They are not output from a prototype; the first step of the plan builds that
prototype, and its real output will replace these examples here.

### Seed A: the betrayal (cosmic horror)

*The history.* In the Sealing, the Grey Abbots built Cedmark over the Pale Hunger and made
three keys. In the Drowning, Abbot Marrec and his sister Ysolde held two of them. Ysolde, devout
and afraid, heard the Hunger speak her dead child's name. She came to believe the child was
below, alive. In the Last Siege she stole Marrec's key, opened the seal and went down. The
abbots blamed Marrec, who had argued for opening the vaults, and walled him in. Ysolde is the
Power of the second arc: what came back up wearing her.

*What the player meets.* Chapter 1: an abbot captain, carrying an order to keep Marrec's tomb
shut. The steles near the gate, the abbots' own, all curse the traitor Marrec. Chapter 3: a
corpse's note: "Marrec's key was still on his belt when they walled him in. So whose key turned
in the seal?" Chapter 5, the turn, a legendary's lore: Ysolde's ring, "given by her brother,
on the day she lost her child". Chapter 6, the Power, dying: "Marrec? He held my hand. I told
him I would only look."

*Motives.* Revenge: the village burned because it sheltered the last abbot who knew. Riches:
the abbots' tithe hoard, which Marrec had wanted opened to feed the town.

### Seed B: the cover-up (human drama)

*The history.* Aldburg's founders, the Salt Covenant, dug for silver with convicts. In the
Deep Delving a collapse buried two hundred of them; the Covenant sealed the shaft with the men
still alive and invented the Thing Below to keep anyone from digging them out. In the War of
Fire the convicts' families rose; the Covenant crushed them and wrote the chronicle that the
steles now repeat. The Fall was the families' heirs, opening the gate to let the truth out.

*What the player meets.* Steles warn of the Thing Below and praise the Covenant's sacrifice. A
miner's note far down: "The knocking stops after three days. It always stopped after three
days." A bounty board from the Covenant offers gold for "the agitator Varard", the Power of the
first arc, who dies saying: "Go down. Count the bones. Then decide which of us is the monster."
The Covenant's own chronicle, sold in the Sanctum library, is the forged source.

*Motives.* Glory: the Roll of the Dead is the Covenant's honour roll; the player's name will be
carved next to the men who sealed the shaft. Truth: the real account is a convict's ledger of
names.

### Seed C: your village (the pact)

*The history.* The player's grandmother was the Steward of Cedstead, a village that paid the
fortress's tithe. In the Last Siege she made the pact with what lay below: the village would
be spared if the gate stayed open one night every hundred years. The night came due this year.
The ones who burned Cedstead were not raiders: they were the Steward's own order, trying to
break the pact by destroying the village it protects.

*What the player meets.* Revenge leads to the captain who held the chapel bar, who dies saying
"We did it to save you." Then the Steward's seal on the general's orders. The turn: the
player's kin is in the pact's text. The last arc of the mystery asks what the player will do
about the night the gate opens, which is where player choices would come in.

## Plan

| Step | Work | Days |
|------|------|------|
| 1 | `history.c`: data model, event library (30 types), simulation, dullness reruns; `worldmap story` prints it. **Checkpoint: real example histories for ten seeds go into this document for review.** | 4 |
| 2 | Plot shapes as patterns, scoring, choosing the main mystery and secondary threads; tones | 3 |
| 3 | Casting: chapter targets, domain lords, legendaries and the player's village from the history; motives as threads | 4 |
| 4 | Clue budget and placement, grammars and voices (~150 entries), the validator | 4 |
| 5 | The STORY tab's "What you know", save v13, `worldmap storystats`, a bot story batch and its write-up | 3 |
| | **Total** | **~18 days** |

Optional, after step 5: **player choices** (about 5 days, see below).

Step 1 ends with a checkpoint: the generated histories are shown here before anything else is
built on them, so the variety can be judged early and cheaply.

## Risks

- **Generated histories read like lists.** The simulation can produce events no one cares
  about. Mitigated by the shape search (only the best chain is shown as the mystery), the
  dullness reruns, and the checkpoint after step 1.
- **Clues the player never finds.** Clues sit in traces that may not be visited. Mitigated by
  the clue budget (three sources for each important fact), clues on chapter targets (which
  every quester meets) and on hunters' patrols (which everyone meets).
- **Contradictions that are just bugs.** The validator checks every contradiction for a
  resolution; unintended ones are failures.
- **Text that reads generated.** Short fragments help; so do voices tied to the speaker, and
  a writing pass on the grammars after the first batch of real output.
- **The generator grows too big.** The event library starts at 30 types; new types are added
  only when the measurements show that a shape is missing.

## Decisions

Two questions are open; the plan above assumes the defaults.

1. **Player choices.** Default: *not in the first version.* The story is a mystery to uncover;
   what the player does decides only what they learn. With choices (about 5 more days): a few
   targets can be spared instead of killed (and tell you something, or betray you later), the
   player can side with one faction in a war (which changes which lords are hostile and which
   steles are true), and the main mystery ends with a decision (the pact in seed C: keep it,
   break it, or take it over). Choices would be saved per world, and the history's present era
   continues from them.
2. **Tone.** Default: *mostly cosmic horror, with human dramas and tragedies mixed in*, in the
   shares given under "Tone". The alternative is cosmic horror in every world, which keeps the
   current mood but reduces the variety of the turns.
