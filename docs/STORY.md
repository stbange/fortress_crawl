# Fortress Crawl: Stories

The dungeon is endless, so its stories have to be too. They are not written in advance and
not simulated: like the blocks, every piece of story is a pure function of the world seed and
a position. Any chunk can work out who rules its domain, what happened there and which named
enemy lives where, so a note on a corpse can point at a place that has not been generated yet,
and it will be there when you arrive. Only what players change or learn is saved.

Tone: short, grim fragments. No long dialogue; the dungeon speaks through its dead.

Status: steps S1 (information) and S2 (motive, chapters, named enemies, missions) are in the
game. S3 to S5 are planned (see the end).

## Layers

| Layer | One per | What it is |
|-------|---------|------------|
| World myth | seed | the fortress and its name, the builders, what it seals, the night it fell |
| Arcs (metaplot) | arc index | one Power with a captain and a general under it: three chapters |
| Domains | ~4x2x4 chunks | an irregular region with a name, a ruling faction, a state, a lord and an era |
| Traces | chunk (about 1 in 3) | an inscription, a dead adventurer with a note, or a bounty board |

Code: `src/world/story.c` (pure, in the world library, testable without the game) and
`src/game/quests.c` (motives, missions, lore, spawning, screens).
`./build/worldmap story <chapters> <radius>` prints a seed's myth, metaplot and the domains
and traces around the gate.

## Directions mean something

- **Depth is time.** Each stratum downward is an older age: the dungeon remembers the Last
  Siege, the mines the Deep Delving, the roots the First Hearths, then the Drowning, the War
  of Fire, and at the core the Sealing. Domains date their history by their stratum.
- **Height is power.** The castle is the reign of the builders, then the Ascent, the Long
  Prayer, the Exaltation, and the time before stars.
- **The compass gives the cultures.** Names, factions and the Powers' titles come from the
  culture of the direction: northern (frost), southern (ember), eastern (boyar, steppe,
  jade), western (gothic, druidic, drowned), and the old culture of the gate.
- **Distance gives rank.** Chapter targets sit farther out with every chapter; arc *k* reaches
  danger level 12 + 16k. Domain lords are everywhere, scaled by their domain's level.

## The metaplot: arcs and chapters

Chapter *c* belongs to arc *c / 3*. Each arc is one direction (down, up, or a compass
direction, never the same twice in a row) and one Power:

1. **The captain** (about 40% of the way out) - chapter 3k+1
2. **The general**, the Power's hand or marshal (about 70%) - chapter 3k+2
3. **The Power** itself, with a title from its culture ("Queen under the Ice", "Herald of
   the Pale Hunger") - chapter 3k+3

Every kill reveals one piece of the metaplot (the *revelation*). The Power's dying words name
the next arc's Power, which is how the arcs connect. The revelations of the first four arcs
build one secret, getting darker the deeper you go: the gate was opened from inside; the
builders did not seal the thing below, they fed it; the thing was there first and the
fortress is a cage with the players as bait; there is no bottom. After that the pattern
continues with new Powers.

Named enemies are placed in the best chunk near their spot (great halls and arenas first) and
are big: captains 1.5x, generals 1.7x, Powers 2x. If one does not fit, the game shrinks it
until it does. `test_worldgen` checks that the first nine chapter targets have room.

## The motive: one per character

Every character picks why they came (in co-op each player chooses their own):

| Motive | The thread | Chapter reward |
|--------|------------|----------------|
| Revenge | they burned your village; your kin was in the chapel. Each target paid or ordered it | +1 skill point |
| Glory | the Roll of the Dead at the gate: the next name above yours | a relic |
| Riches | each target holds a piece of a map to a vault | gold and a rare or epic weapon |
| Truth | each target holds a page of a book about what lies below | two spells |

The motive does not change the world's story, only what it means to you. Everyone in a world
hunts the same captains and Powers, so co-op players with different motives meet at the same
targets. If someone else killed your target, your chapter still closes ("Someone was faster").

## Traces and missions

Quests do not only come from people. In S2 they all come from things in the dungeon:

- **Inscriptions** (standing stones with a faint line of script): lore from the domain's
  state and era and the world myth.
- **Dead adventurers** (bones and a scrap of paper): a last note. About half of them ask you
  to take something home: a **retrieve** mission. The item lies a few chunks away and only
  appears for a player who has the mission.
- **Bounty boards**: a **bounty** on the lord of this or a neighbouring domain, with the
  reward and where to find them.

Standing next to a trace shows its text; the first reading adds it to the world's lore
(shared by all characters) and adds the mission to the reader (at most six). Taken chests,
relics, quest items and dead named enemies are remembered for the world, so a bounty on a lord
someone already killed says so.

## On screen

- **Entering a domain:** a banner, e.g. "THE CELLS OF MORWEN - SEALED SINCE THE LAST SIEGE".
- **Reading:** a box above the HUD with the text of the trace you stand at; chapter messages
  (what the dying said, the revelation, the next hook) use the same box.
- **Tracker:** the current target and where it is, e.g. "> ZARUKET LANTERN-JAW - 250 M SOUTH,
  30 M BELOW", at the top of the view.
- **Name plates:** named enemies in view show their name and health.
- **Map:** the chapter target (red) and missions (gold), on the edge when off the map.
- **Directions** are given in layers: "140 M NORTH-EAST, 2 LAYERS DOWN".
- **Golden Thread** (a spell everyone has; G, right stick click, PATH on touch): when the
  target is on another layer, it first leads to the best staircase of your layer in that
  direction (stair positions are known in advance). Otherwise a weighted A*
  search over the walkable cells of the loaded chunks (level moves, one-cell steps up, drops
  of up to six cells, crawlspaces, no corner cutting) toward the target, laid out as a thin
  golden line with a light pulse running along it toward the goal; it dissolves after about
  20 seconds. If the target is beyond the loaded area, the line ends at the reachable spot
  closest to it. Casting again within eight seconds switches to the next mission.
- **STORY tab** (L, or the menu): motive, chapter, arc, target, the hook, missions with
  directions, and everything read so far ("What the dead say"), newest first.

## Saved

Per character: motive, chapter, missions. Per world: the lore read (as positions and chapter
numbers; the text is regenerated) and, as before, the taken chests and relics, which now also
cover quest items and dead named enemies. Save format version 2; version 1 saves still load
and their characters are asked for a motive.

## Next steps

- **S3 - camps and people:** survivors gather at shrines (already respawn points): merchants
  (a use for gold), quest givers, the occasional ghost. Short conversations with a few
  choices. Notes and boards stay as quest sources.
- **S4 - consequences:** domain states change with what players do (a lord killed, a lair
  cleared: the faction weakens, a camp appears, a rival expands), saved as small per-domain
  changes; monster families become factions with allies and enemies; wars on the borders.
- **S5 - choices and endings:** taking sides, mechanical puzzles as mission steps, and a
  closing scene for each chapter of a motive.
- **Generated stories:** a simulated history per world, from which the mystery, the named
  enemies and the clues are drawn, so that worlds differ in what happens and not only in their
  names. Proposed in [STORY_GEN.md](STORY_GEN.md).
