# Fortress Crawl: Generated Stories

Stories that differ from world to world in what happens, not only in the names. This page
began as the design written before the work; it now describes what was built, with real
output of the generator. It extends [STORY.md](STORY.md), which describes the rest of the
story system (domains, traces, missions, the Golden Thread).

Status: built. New worlds (save version 13) use it; worlds begun before keep their old story.
Player choices that change the history are not part of it (see "Decisions").

Code: `src/world/history.c` (simulation, mystery, casting, facts and clues),
`src/world/history_text.c` (the grammars and voices), the branches in `src/world/story.c`, and
the STORY tab in `src/game/quests.c`. `worldmap history` prints a seed's hidden history
(`FC_SEED=n`), `worldmap storystats 1000` measures the variety, and `test_worldgen` validates
200 histories on every run.

## The problem it solves

The story system already generated a lot: the fortress, its builders, what it seals, domains
with factions and lords, a named captain, general and Power per arc, inscriptions, notes and
bounty boards. But the metaplot was one fixed script: every world revealed the same secret in
the same order (the gate was opened from inside; the builders fed the thing below; the
fortress is a cage and we are the bait; there is no bottom), and each motive followed one
fixed chain (the next coin, the next name on the Roll, the next piece of the map, the next
page). Twenty bot runs in [RUN_STORIES.md](RUN_STORIES.md) and [RUN_STORIES_2.md](RUN_STORIES_2.md)
showed two worlds telling the same story with different names. The inscriptions were also
independent of each other: nothing a stele said was confirmed or contradicted anywhere.

## The approach

Generate a hidden history first, then let every story text be a report about it. At world
start a small simulation runs the fortress's past from the seed alone: people and factions
with traits, kin and drives act through events with causes and effects. The result is a graph
of events and the facts they establish. The game searches that graph for the best mystery,
casts its survivors as the named enemies of the first three arcs, and spreads the facts as
clues over chapter targets, inscriptions, notes, bounty boards, legendaries, hunters' orders
and the truth motive's pages. Text comes from small grammars with voices. The player's motive
decides which thread of the same history they follow.

Constraints kept: the same seed gives the same story in every build, offline and on the web,
with no language model at runtime. The history is computed once per world (0.3-0.4 ms) and
cached; chunks still derive their traces on demand and look the clue up in it. A history
takes about 60 KB.

## 1. The simulation

Six eras. Their names come from the world: the Sealing (or the Founding), two of the old ages
the domains already date themselves by (the War of Fire, the Drowning, the Deep Delving...),
the last years of the fortress, the night it fell (the world's own name for it, "the Night of
the Open Gate", "the Quiet Year"), and now.

| Era | What happens |
|-----|--------------|
| The founding | The builders raise the fortress over what lies below and make three keys for three houses. What lies below depends on the world's tone (see "Tone"): they seal a hunger (and perhaps feed it a child every winter), build a lure for a sleeper, wall in the first people of the mountain, chain a king who was there first, or seal a shaft on living miners or the plague-sick and invent a monster to keep the diggers out. |
| Two old eras | Two or three events each: wars between houses (and massacres), a heresy that prays to what lies below (and its purge), a bargain with it made by the loser of a war (the price: one night with the gate open), the hoard hidden from an enemy, the official chronicle, a smith's blade, a house taking the fortress from its builders, the Roll of the Dead. |
| The last years | The generation of the mystery: a steward, three keepers with the keys, a captain sworn to the steward, a scribe, a priest, a merchant, the heir of a bargain's line. Siblings, a marriage, children. Then four to six personal events, each chosen from who could do it: a child lost (to the fever, to a shaft, to the winter tithe, to the dark), a dead child's voice heard below the seal, someone reading what the founders hid, the steward having them killed for it, a rival for the steward's seat, an exile, a keeper's debt, a conversion to the heresy, the bargain falling due. Each gives someone a drive, and remembers why. |
| The fall | The strongest drive among the living opens the seal: grief answering a voice, a bargain paid, revenge on the steward, faith, the truth, power, gold, or, in a world built as a lure, the last of the builders finishing what the fortress was built for. If the opener has no key, they steal one (often from their own kin) or are given one. Then the consequences: the opener goes down and comes back changed, hides, seizes the fortress, flees with the gold, or dies; the steward and keepers may die. Whoever is left tells a lie: they blame a keeper, an agitator, or the thing below itself. The one they blame is walled in, hanged or driven into the deep, and their kin want revenge. A scribe writes the false chronicle; someone else writes down the truth and hides the pages. |
| Now | The survivors and their heirs, and those sworn to them. Someone burned the player's village, on someone's order, for a reason the history explains: the true pages were hidden there, the blamed one's child hid there, the bargain asked for it, or as a lesson to those who ask. In some worlds the one who wrote the true account was the player's own grandparent, and died in that fire. |

A history that is dull is thrown away and simulated again from the next sub-seed (up to
eight times): when the chain of causes behind the fall has fewer than three steps, or no plot
shape scores high enough.

## 2. The mystery

Ten plot shapes are patterns over the events. Each is scored; the best becomes the main
mystery, the next two (if strong enough) colour it.

| Shape | Pattern | Main shape in |
|-------|---------|---------------|
| Betrayal | the one blamed is the opener's sibling, spouse, parent or child (or sworn) | 15% |
| The voice below | the opener heard a dead child calling from beneath the seal | 21% |
| The wrong monster | the lie blames the thing below; a hand drew the bolts | 14% |
| The cage was bait | the fortress was built to call it up, and the last of the builders opened it | 13% |
| Pact | an old bargain fell due, and someone of the line paid it | 12% |
| Your village | the true pages were hidden in the player's village | 11% |
| Cover-up | nothing came up: the horror was people, and the chronicle is forged | 9% |
| Mercy | the opener freed the ones below, who were wronged | 4% |
| Usurpation | the opener took the fortress in the chaos | 3% |
| The hidden heir | an heir of the wronged is alive | possible in 9%, never the main shape yet |

The revelations of the nine chapters follow the shape's order: the lie (orders under the
liars' seal), how the blamed one was punished, the crack in the lie (dying words of the one
who told it), the forgery or the village's reason, the key and the kinship, the turn (who
opened it), the root of the drive, the deepest cause (the founding's secret or the old
bargain), and last the opener's own words, or the words of their heir or herald.

## 3. Casting

The first three arcs are the mystery, nine chapters:

- **Arc I, the liars:** a sworn blade (who held the bar of the chapel door in the player's
  village), the one who punished the blamed (gaoler, or the steward's hand), and the one who
  told the lie (the steward, the captain, or their heir).
- **Arc II, those who know:** a captain, the merchant who sold silence (or the blamed one's
  kin), and the one who knows the turn (the blamed one, alive in exile; the keeper whose key it
  was; the witness).
- **Arc III, the opener's side:** a follower, the opener's hand (the one who gave the key, or
  kin), and the opener (returned, hidden, ruling) or the one who speaks for them.

Their cultures follow the arcs' directions as before, so names are made last, once the cast
is known. Titles come from their part: "Steward of Edhold", "gaoler of the Last Host", "the
Exile", "the One Who Came Back", "Herald of the Red Silence", "keeper of the Bone Key". After
the third arc the old pattern continues (new Powers farther out), and their revelations
carry the facts no chapter has told yet.

**Legendaries:** each of the nine targets carries a different world legendary, chosen by the
history, and its lore tells its bearer's deed ("Carried by Ishusut. Ishusut bricked Gereos into
the wall at the Salt Door."). Domain lords never drop those nine; the old blades of the history
get the lore of their making.

**The motives** follow four threads through the same history: revenge goes from the one who
held the bar to the one who gave the order and why; glory reads the Roll of the Dead, where
each name stands beside its deed, and finds at the turn that it is the list of those who kept
the secret; riches follows the hoard hidden in an old war and what was carried off on the
night of the fall; truth collects the pages of the true account, and each page is a clue
nobody else gets.

## 4. Clues

Every fact gets clues by its weight: the key facts (who opened it, why, the crack in the lie,
the founding's secret, the lie itself) at least three, important ones two, colour one. They
are spread over:

| Source | Voice | Reliable? |
|--------|-------|-----------|
| Chapter targets | orders and papers on the body, dying words (first person when it was their deed) | true; the first is the lie itself, under the liars' seal |
| Inscriptions | a faction's stone, old letters, something cut in a hurry, a grave ("Here lies Raeim, daughter of Amiim") | the liars' stones repeat the lie |
| Notes of the dead | a diary, a letter never sent, a rumour ("They say...") | true, sometimes second hand |
| Bounty boards | the lord's bounty, and what the poster wants known | the lie |
| Legendaries | who carried it and what they did | true |
| Hunters' orders | the leader of a band of hunters carries orders from the one who sent them | true |
| Pages (truth motive) | a page of the true account, in the witness's hand | true |

Traces carry clues by distance from the gate: the old ages and the lie are close, the turn
and the reasons farther out (never nearer than the fourth chapter's target lies), and the
frontier is favoured. Near the gate a little under half of the traces keep the old
atmosphere lines ("Do not answer if it calls you by name"), farther out about a third. The same
clue on two traces is worded differently.

## 5. Text

Every event type has a grammar in three forms with alternatives: told about the doer ("Ishusut
bricked Gereos into the wall at the Salt Door"), told by the doer ("I walled Gereos in. Gereos
did not scream") and told without knowing who ("Gereos was walled in alive for the Night of
the Open Gate"). Some forms depend on details (hanged, walled in or driven into the deep; a
king or a people; miners or the sick). The reasons for the fall and for the fire have their
own grammars per drive, also in the first person for the opener's last words. The voices
above wrap them. About 300 short templates in all. Everything is deterministic, under 320
characters, and checked by the validator.

## 6. The STORY tab: what you know

A section between the missions and the lore: "WHAT YOU KNOW - N FACTS OF THE HISTORY OF
EDHOLD", then the world's questions, open (?) or answered (!) with the answer under them:

- Who opened the seal on the night of the fall?
- Did the blamed one open it, as the liars say? (or: did the thing break out by itself?)
- Why was the seal opened?
- What lies beneath the fortress?
- Why did your village burn?
- Where is the opener now? (shown once you know who it was)

A question is answered once a clue of its fact has been read: on a body, a stone, a note, a
legendary, a page or the hunters' orders. A banner announces it ("A QUESTION ANSWERED: WHY
DID VARFORD BURN?"). What is known is worked out from the lore read in the world, which is
already saved, so nothing new needs saving for it.

## 7. Tone

| Tone | Share | What lies below |
|------|-------|-----------------|
| Cosmic horror | 59% | a hunger (fed, in two thirds of them) or a sleeper the fortress was built to lure |
| Human drama | 22% | nothing: miners sealed in alive, or the plague-sick walled in, and a monster invented |
| Tragedy | 19% | the first people of the mountain, or a king who was there first, walled in and named a monster |

The monsters are the same in every world; what changes is what the dead say about them.

## 8. Saves

The world save has a byte for its story generator (version 13): 0 for worlds begun before,
which keep the old story exactly, and 1 for new ones. Lore entries gained three kinds (a
legendary's lore, hunters' orders, a page); everything else (the questions, what is known) is
worked out from the lore.

## Measurements

`worldmap storystats 1000`:

| | |
|---|---|
| Time | 0.3-0.4 ms per world |
| Validation | 0 of 1000 failed (key facts with three clues, every lie with a crack, no one acting after their death, cast distinct and alive, every question answerable, distinct names, every text fits) |
| Distinct plots | 379 combinations of shape, drive, the blamed one's role, what lies below, the opener's fate and the village's reason |
| The opener's drive | grief 30%, duty 17%, revenge 16%, pact 14%, truth 12%, faith 7%, power 4%, greed 1% |
| The opener | keeper 34%, scribe 18%, heir 17%, priest 11%, merchant 8%, captain 7%, steward 4% |
| The blamed one is the opener | 0% (betrayed by their own kin: 19%) |
| Causal chain behind the fall | 3 steps 70%, 4 steps 18%, 5 steps 10% |
| The village burned for | the pages 60%, a lesson 17%, the traitor's child 14%, the bargain 9% |
| The player's grandparent in it | 15% |
| Facts, clues | 34 facts and 89 clues per world |
| Thrown away as dull | none in 66% of worlds, one in 17% |

## Three worlds, as generated

Real output of `worldmap history` (village and kin as for player 1 of the dump: "Aldmere").

### Morwall (seed 9): the cage was bait, with a betrayal

Cosmic horror. Halowen of the Ninefold Kings built Morwall as a lure for the Sleeper, not a
cage. In the Deep Delving the Salt Choir massacred the Lantern Host, whose Tameim bargained
with the Sleeper for a hundred years without war. In the last years of Morwall, Ilezad and
Amiim both lost a child "to the dark below the seal"; Ilezad began to hear the dead son
calling. But it was Branewen, the last of the builders' line, who read the old books and
learned what the fortress was for. On the Quiet Year Branewen stole the Amber Key from Amiim,
Branewen's own brother, and opened the Ninth Door. The Grey Court named Amiim the traitor and
drove him into the deep.

1. *Orders on the body, under the seal of the Grey Court: 'Amiim opened the seal and let the
   Sleeper into Morwall. Let no one say otherwise.'*
2. *'Radawei drove Amiim down into the deep for opening the seal.'*
3. *Radawei, dying: 'I named Amiim. But Amiim's key was gone the day before. Amiim told me so.
   I did not listen.'*
4. *'Mirorin wrote the chronicle of the Quiet Year for the Grey Court, and wrote it false.'*
5. *'Branewen stole the Amber Key from Amiim, Branewen's own brother.'*
6. *Amiim (the Exile), dying: 'Branewen opened the Ninth Door with the Amber Key.'*
7. *'Branewen found out what lies beneath Morwall.'*
8. *Cedimund, dying: 'It is in the oldest books: Halowen never meant Morwall to hold the
   Sleeper. It was built to call it up.'*
9. *Branewen, dying: 'I opened it because that is what Morwall was built for. I learned what
   lies below. I wish I had not. I only finished it.'*

### Gerhold (seed 2025): the voice below, and the wrong monster

Tragedy. The First Masons walled the first people of the mountain under Gerhold and called
them the Red Silence. Ulwen, a keeper, lost a daughter, Cedohold, to the dark below the seal,
and then heard the ones below singing, and Cedohold's voice among them. On the Last Siege
Ulwen turned the Glass Key and went down to them. The Ninefold Host said no hand opened it.

1. *'No one opened the Salt Door. The Red Silence broke it from below. Let no one say
   otherwise.'*
3. *Branaric, dying: 'I said the Red Silence broke it from below. But the bolts were drawn back
   from our side. Nothing below draws bolts.'*
4. *'Aldmere burned as a lesson to those who ask about the Last Siege. Gerwyn gave the order.'*
6. *Gwyiys, dying: 'Ulwen turned the Glass Key in the Salt Door on the night of the Last Siege.
   I was there.'*
7. *'Ulwen heard the ones below singing, and Cedohold's voice among them.'*
9. *Aldowen, Herald of the Red Silence, dying: 'Ulwen opened it for Cedohold, the daughter Ulwen
   had lost: Cedohold's voice called from below, and Ulwen went to answer.'*

### Edhold (seed 1337, the README's screenshots): betrayal and cover-up

Human drama. The Grey Abbots shut the lower shaft on living miners and told the world of the
Thing That Counts. Thyvik, a scribe, found out; on the Night of the Open Gate Thyvik opened the
Salt Door with the Bone Key so the world would see the bones. The Grey Abbots blamed Thyvik's
sister Gereos, whose key had been stolen the day before, and bricked her into the wall. The
village burned because Gereos's child hid there; a gravestone near the gate reads "Here lies
Gereos, mother of Ulawyn", and Ulawyn is the second arc's Power.

2. *'Ishusut bricked Gereos into the wall at the Salt Door.'*
5. *Gerahold, dying: 'Gereos and Thyvik: one blood. Thyvik was Gereos's brother.'*
8. *'The Grey Abbots shut the lower shaft on living miners and told the world of the Thing That
   Counts.'*
9. *Thyvik, the Hidden, dying: 'I opened it so the world would see the bones. No one came to
   look.'*

## In play

Ten quester bots in ten new worlds, 90 minutes each: [RUN_STORIES_3.md](RUN_STORIES_3.md).
Every world told a different story; the lie and its crack land in the first arc; questions
get answered from stones, notes, pages and hunters' orders even by bots stuck on their first
target. Two fixes came from it: a target without a deed of its own no longer borrows a
revelation for the Roll, and the founding's secret no longer lies by the gate.

## What changed from the proposal

- Known facts are worked out from the lore already saved rather than kept as a bitset per
  character; so they are per world, like the lore, and shared in co-op.
- Domain lords and domain factions are as before; the history's factions have names of their
  own. Lords do not come from the history.
- The Sanctum library does not sell chronicles; the false chronicle and the true account reach
  the player through clues, pages and dying words instead.
- The duplicate legendary drops of the earlier runs turned out to be legendaries left lying
  (a full bag) and dropped again later; the nine targets now carry nine different ones.
- Hunters' orders are found on the band's leader when it dies.

## Decisions

1. **Player choices:** not in this version, as proposed. The story is a mystery to uncover;
   what the player does decides only what they learn. Sparing targets, siding with a faction
   and a closing decision would come next, about a week of work.
2. **Tone:** mostly cosmic horror, with human dramas and tragedies mixed in, as proposed.
