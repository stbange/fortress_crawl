# The Old King

The main storyline of every fortress. Each world has one Old King, of ten kinds, chosen by the
seed. At first you don't know what he is. Ten steps lead to his throne, and they take you all
over the game: the towns, the Sanctum, the dragons, the forge, alchemy, the realms behind the
Gate and the dungeons. You win the game by settling with him: by the sword, by his true name, or
by the artefact his kind needs, each with its own reward. The credits roll, then you can explore
on, and before long an heir takes the throne and a new way begins.

The other stories (the chronicle, the towns' tasks, the realms) are side stories that feed the
way: the chronicle's end is a step of it, a town's friendship another. How it all fits together is
in [DESIGN.md](DESIGN.md); the concept and its history are in [OLD_KING_PLAN.md](OLD_KING_PLAN.md).

## The ten kinds

| Kind | What he is | Throne | Artefact (from the step) | Where |
|---|---|---|---|---|
| the Tyrant | the old cruel king with an iron crown and an army of knights | the Throne Hall | the Breaking-Stone (forge) | on the gate's layers |
| the Necromancer | a lich-king; his life is kept in a phylactery | the Necropolis | the Phylactery (relic) | deep |
| the Old Mage | an archmage who stopped time for himself | the Tower of Stopped Hours | the Lost Hour (hoard) | high |
| the Dragon-King | a king who became a dragon to live forever | the Dragon's Crown | the Name-Scale (hoard) | high |
| the Hollow King | a ghost bound to the throne by an oath he never kept | the Empty Court | the Oath-Ring (relic) | deep |
| the Clockwork King | an automaton that took the place of the king it served | the Great Engine | the Winding Key (forge) | high |
| the Beast-King | a cursed king, more wolf than man | the Den | the Cure (brew) | deep |
| the Priest-King of the Sealed | he worships what is sealed below | the Black Altar | the Seal-Stone (relic) | deep |
| the Alchemist-King | lives on elixirs, mad and always changing | the Still | the True Elixir (brew) | deep |
| the King-in-the-Stone | the first king, asleep under the fortress | the Sleeping Hall | the Dream-Lantern (hoard) | deep |

His name comes from the history's names with an epithet of his kind ("Sevren the Ninth"). His
throne is a hall megastructure with a carpet, a dais of three steps, the throne (a black altar
for the Priest-King), braziers and decor of his kind, placed in a fixed cell a few super-cells
from the gate, above or below by his kind.

## The ten steps

**Order**: steps 1-3 (finding out) are open together, in any order. Steps 4-7 (the means) open
when those are done, again in any order. Then step 8, step 9, and the throne. The line at the top
shows the nearest open step and its direction; each step done shows a banner
("THE OLD KING - 4 OF 10"), adds a page to the lore and is saved.

| Step | Group | What you do |
|---|---|---|
| the herald | 1-3 (always) | talk to the elder of the nearest town |
| the true name | 1-3 (always) | find the stele with his name on it |
| the witness | 1-3 | find the one who saw him, waiting in the halls with a few of his servants about |
| the vision | 1-3 | sleep a night in the Sanctum's bed and dream of him |
| the dragon | 4-7 (always) | slay a named great dragon in its lair |
| the hoard | 4-7 | take a thing from a dragon's hoard (it lies at the centre of the lair) |
| the allegiance | 4-7 | become a friend of a town (reputation 3: its tasks, its works, driving off a raid): its soldiers fight beside you at the throne |
| the forge | 4-7 | at the Sanctum's anvil (the forge plot built), make the thing from a titan heart (a dragon's), 3 essences of one element and 2 steel |
| the brew | 4-7 | bring dragon's blood (or a heartstone) and an orchid to a town's healer |
| the realm | 4-7 | defeat the ruler of a realm behind the Gate (only once realms are joined) |
| the relic | 4-7 | take a relic from its guards |
| the lieutenant | 8 or 9 | kill his named lieutenant, a boss with guards (step 8 is also done when the chronicle ends) |
| the seal | 8 or 9 | break one of his seals, guarded by five |
| the wyrm | 9 | slay the dragon whose lair lies nearest his throne |
| the throne | 10 | him |

**How a way is made** (`src/world/king.c`, a pure function of the seed and the realms joined):
- Steps 1-3: the herald, the true name, and the witness or the vision.
- Steps 4-7: always the step that gives his kind's artefact, always a dragon, the forge when the
  vision is not on the way (so every way has a Sanctum step), the rest from the pool.
- Step 8: the lieutenant or the seal. Step 9: the wyrm or the other one of those two (the
  Dragon-King always has the wyrm).
- Places: the herald in the nearest town; towns and lairs in rings of distance (steps 1-3 at
  4-14 chunks, 4-7 at 10-26, 8-9 at 18-34); other steps in ordinary chunks with room.
- `test_worldgen` checks every seed: the way is valid, the throne is in its hall, and every way
  has a dragon, a town and a Sanctum step.

## On the way

- **The line at the top** follows the nearest open step (Y moves it to the chronicle or a town's
  task), and the Golden Thread (G) leads to it. The first time it follows a step, a line says how that step is done: the way teaches
  the game (the Sanctum Door, the STORY tab, crouching past a sleeping dragon, the forge, the
  Gate, a town's works).
- **The chronicle counts**: when the chronicle of your motive ends (the one who opened the seal),
  step 8 is done too, whichever it was: the opener served the Old King.
- **Raids**: once you know what he is (steps 1-3 done), his servants raid the town you stand in,
  about once in a quarter of an hour. They run at you; drive them off and the town thanks you
  (+1 reputation, toward the allegiance). Leave, and they are gone without thanks.
- **His own close in**: his servants, raiders and lieutenants run when you are more than a few
  metres off, and his lieutenants step through the dark to your side every few seconds. Keeping
  them at bow's length does not work.

## At the throne

When step 10 opens and you come within 70 m, the king sits on his throne: a boss half again as
large as others and three times as hard, of his kind's form (a knight, a necromancer, a shaman, a
wraith, a golem, a troll, a bloater; the Dragon-King is an ancient gold dragon). Strike at him and
he speaks, and rises: **he will not listen standing.**

- **He falters** at half his life, and again at a fifth, if you know his true name or hold his
  artefact. For 12 seconds he kneels on the steps of his throne and listens:
  - strike: speak his true name, **the word ending**
  - crouch and strike: raise the artefact, **the artefact ending**
  - or finish him from afar (a bow, a spell): **the fight**
- Then he rises again. Without his name or his artefact he does not falter: it is the sword.
- If you won a town's allegiance, four of its soldiers fight beside you.
- He calls his servants at two thirds and at one third of his life.
- The Necromancer takes almost no harm until his phylactery is in your hands.
- The thing from the forge breaks him: blows hurt him 40% more.
- In co-op, more than half of those at the throne must strike for the same ending; the sword
  needs no vote.

## The endings and what they give

| Ending | What it gives |
|---|---|
| the sword | his crown (a relic), his own legendary blade, the throne's hoard (gold, four rich drops), a titan heart, 3 keystones; his skull in the Sanctum's armory |
| the word | his knights swear to the towns: every town has two more guards from then on; his treasury (gold, 2 keystones) |
| the artefact | the bound are freed: every town's reputation +2, and new towns greet you as a guest; a keystone and a rich drop |

Any ending wins the game:
- the ending's text is told and his servants scatter
- the credits roll; strike after a few seconds to explore on
- the towns celebrate: lights over the plazas and nothing else talked about
- the Astral Shore opens behind the Gate (only the king opens it); after a merciful ending (the
  word or the artefact) so does the Dream of the Sleeper

## The heir

A quarter of an hour of play after a king is settled, word comes from the towns: the throne did
not stay empty. An **heir** of another kind (never the one before) takes it, and a new way of ten
steps begins:
- the same throne hall, but the steps lie 8 chunks farther out per heir (up to three)
- the herald is any town's elder, not the nearest
- the dragons' lairs on his way are taken again by new dragons
- his allegiance step asks the town to honour you (reputation 6), not only to be your friend
- he is 6 levels stronger per heir; his true name, his artefact and his endings are those of
  his kind
- what the last ending changed (sworn knights, freed towns) stays

Heirs go on up to the ninth; the way never has to end.

## Testing

`--demo kingtest` walks the seed's way: it talks to the elder, steps onto the markers, meets the
witness and kills the lieutenant through the real handlers. The steps that take hours (a dragon
slain, a town's friendship, a realm's ruler) are done by hand. At the throne it checks that a word
before he falters is not heard, brings him to his knees and settles it. `KINGTEST_END=fight` or
`art` picks the ending, and `KINGTEST_HEIR=1` then starts the heir and walks his way too.

```bash
build/fortress_crawl --seed 1337 --fresh --mute --demo kingtest --shot /tmp/k.bmp --shot-time 150
```

`--bot king` plays by itself toward the nearest step it can do (talking to the elder, the
witness, the healer; the markers, the dragons, the lieutenant, the throne) and logs a `KING` line
at the end. `test_worldgen` checks the way of every seed and two heirs each.

## How long the way takes (bots, November 2026)

`--bot king`, four seeds (1337, 42, 777, 2024), three hours of play each. The bot follows the
Golden Thread to the nearest step it can do (its own waypoint, not the line at the top); goes
home for the vision as soon as nothing else can be done; builds the island and the forge first
and buys steel for the forge step; buys an orchid where one is sold; hunts the nearest great
dragon within two levels of its own when the way needs a dragon's heart or blood; and for the
realm step walks through the Grand Vestibule's rift (it always leads to the gentlest joined
realm, the one the way asks for) or the Gate, to the ruler, and out through the arch (if it
cannot find the arch in a minute it is put through it, and counted).

**No bot has reached the throne in six batches.** The best runs:

| Seed | Steps 1-3 | Further | Dragons slain | What held it |
|---|---|---|---|---|
| 1337 | 11-16 min | 3 of 4 means by 50 min (one batch) | 1 | stairs: stuck 900 times on the way down to a hoard |
| 42 | 9-24 min | 2 of 4 means (two batches) | up to 2 | the thread's end at a staircase, 187 times in one run |
| 777 | 31-72 min | 2 of 4 means by 88 min | 0 | navigation between layers |
| 2024 | 21-39 min | 0-2 of 4 | 0-1 | stairs; once lost in a realm for 1.7 hours (fixed) |

What it shows:
- **Finding out (steps 1-3) takes 10-40 minutes.**
- **The means (steps 4-7) are the long part.** The forge and the brew need a dragon's heart and
  blood, so a dragon comes first; the great dragons a character can face are often 200-400 m off
  and some layers away.
- **What stops the bot now is changing layers**: it reaches the staircase the thread leads to and
  does not take it. This is the bot's own old weakness (its stair following), not the way's; a
  player takes the stairs. Until it is fixed the bot cannot measure the time to the throne.
- The realm step's route (the Vestibule rift, the ruler, the arch) has not been reached by any
  bot yet in these batches, so it is in place but not yet proven in play.

## Code

- `src/world/king.h`, `king.c`: the kinds, the texts, the way generator (and the heirs'), the
  throne cell
- `src/world/gen_struct.c`: `throne_dais`; `layout.c` forces the throne hall into its cell
- `src/game/king.c`: the steps in play, hints, markers, the witness and the lieutenant, raids,
  the throne, the falter, the endings and their rewards, the heir, the tracker and the credits
- save version 29: the steps done, what is known of him, the ending; the heir's generation, the
  kings settled, whether one was spared

Not done yet: separate throne megastructures per kind (all are a throne hall with decor of the
kind), a hall of victories in the Sanctum, and a "back to the start screen" choice after the
credits.
