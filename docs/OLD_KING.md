# The Old King

The main storyline of every fortress. Each world has one Old King, of ten kinds, chosen by the
seed. At first you don't know what he is. Ten steps lead to his throne, and they take you all
over the game: the towns, the Sanctum, the dragons, the forge, alchemy, the realms behind the
Gate and the dungeons. You win the game by settling with him: by the sword, by his true name, or
by the artefact his kind needs. The credits roll, then you can explore on.

The other stories (the chronicles, the towns' tasks, the realms) stay as they are, as side
stories. The concept and its history are in [OLD_KING_PLAN.md](OLD_KING_PLAN.md).

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
when those are done, again in any order. Then step 8, step 9, and the throne. The tracker at the
top left shows the nearest open step and its direction; each step done shows a banner
("THE OLD KING - 4 OF 10"), adds a page to the lore and is saved.

| Step | Group | What you do |
|---|---|---|
| the herald | 1-3 (always) | talk to the elder of the nearest town |
| the true name | 1-3 (always) | find the stele with his name on it |
| the witness | 1-3 | find the one who saw him, waiting in the halls with a few of his servants about |
| the vision | 1-3 | sleep a night in the Sanctum's bed and dream of him |
| the dragon | 4-7 (always) | slay a named great dragon in its lair |
| the hoard | 4-7 | take a thing from a dragon's hoard (it lies at the centre of the lair) |
| the allegiance | 4-7 | become a friend of a town (reputation 3): its soldiers fight beside you at the throne |
| the forge | 4-7 | at the Sanctum's anvil (the forge plot built), make the thing from a titan heart (a dragon's), 3 essences of one element and 2 steel |
| the brew | 4-7 | bring dragon's blood (or a heartstone) and an orchid to a town's healer |
| the realm | 4-7 | defeat the ruler of a realm behind the Gate (only once realms are joined) |
| the relic | 4-7 | take a relic from its guards |
| the lieutenant | 8 or 9 | kill his named lieutenant, a boss with guards |
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

## At the throne

When step 10 opens and you come within 70 m, the king sits on his throne, asleep or waiting: a
boss half again as large as others and three times as hard, of his kind's form (a knight, a
necromancer, a shaman, a wraith, a golem, a troll, a bloater; the Dragon-King is an ancient gold
dragon). Strike at him to speak:
- with his true name (from the true-name step), strike again: **the word ending**
- crouched, with his artefact, strike again: **the artefact ending**
- with neither, or by striking from afar: **the fight**

In the fight:
- if you won a town's allegiance, four of its soldiers come and fight beside you
- he calls his servants at two thirds and at one third of his life
- the Necromancer takes almost no harm until his phylactery is in your hands
- the thing from the forge breaks him: blows hurt him 40% more

## After

Any ending wins the game:
- the ending's text is told
- his servants scatter
- the credits roll; strike after a few seconds to explore on
- the Astral Shore opens behind the Gate; after a merciful ending (the word or the artefact),
  so does the Dream of the Sleeper
- the towns, the chronicles, the realms and the dragons go on as before

## Testing

`--demo kingtest` walks the seed's way: it talks to the elder, steps onto the markers, meets the
witness and kills the lieutenant through the real handlers. The steps that take hours (a dragon
slain, a town's friendship, a realm's ruler) are done by hand. Then it settles at the throne.
`KINGTEST_END=fight` or `art` picks the ending.

```bash
build/fortress_crawl --seed 1337 --fresh --mute --demo kingtest --shot /tmp/k.bmp --shot-time 150
```

## Code

- `src/world/king.h`, `king.c`: the kinds, the texts, the way generator, the throne cell
- `src/world/gen_struct.c`: `throne_dais`; `layout.c` forces the throne hall into its cell
- `src/game/king.c`: the steps in play, markers, the witness and the lieutenant, the throne, the
  endings, the tracker and the credits
- save version 28: the steps done, what is known of him, the ending

Not done yet (from the plan): separate throne megastructures per kind, a victory hall in the
Sanctum, the bot following the main way, and the "back to the start screen" choice after the
credits.
