# Fortress Crawl: the design

This is the source of truth for how the game fits together. The other documents go into one
system each, and the older plans (`PLAN.md`, `BALANCE_PLAN.md`, `SANCTUM_PLAN.md`,
`SIMULATION_PLAN.md`, `OLD_KING_PLAN.md`) record how things were planned. Where they disagree
with this page, this page is right.

## The game in one paragraph

You come to a castle built into a mountain. Behind its gate the fortress goes on without end, in
every direction, and the farther you go the more dangerous it is. Somewhere in it sits **the Old
King**, and reaching him is the main story: ten steps that take you to the towns of the friendly
peoples, home to your Sanctum, into dragons' lairs, through the Gate to other realms and back.
Everything else (the chronicle of your motive, the towns' tasks, the realms, crafting) is there to
make you stronger, richer in what you know, and to give the way to him its texture. Settle with
him and the game is won. Then an heir takes the throne, and the way begins again, farther out.

## The pillars

1. **Exploration first.** Every few minutes a new kind of place, landmark or secret.
2. **Scale you can feel.** Huge halls next to crawlspaces; towns, lairs, thrones, realms.
3. **Gloomy to look at, a power fantasy to play.** Little frustration, big effects.
4. **Progression you can see.** New abilities and visibly bigger effects, not only numbers.
5. **Retro 3D.** Low resolution, small textures, quantised light, fog.

## The core loop

Out, back, and on:

1. **Out**: into the fortress toward the line at the top of the screen (the Old King's next
   step, unless Y picked another). Fight, loot, mine, gather, read.
2. **Back**: to a town (rest, heal, trade, its tasks, its works) or home to the Sanctum (bed,
   bath, table, forge, cauldron, building) through the Sanctum Door.
3. **On**: stronger, with what you made and learned, to the next step and farther out.

Danger rises with distance from the gate, and never falls below about 60% of your level, so
no place stays safe.

## What feeds what

| System | Gives | Takes |
|---|---|---|
| Fighting | XP, gold, weapons, reagents, essences, titan hearts, keystones | health, potions |
| Dragons | the richest loot: gold, legendaries, titan hearts, scales (essences), teeth, blood | the hardest fights |
| Towns | rest, healing, company (XP), the people's wares, tasks, kin | gold (the inn, the healer, the market, the town's works) |
| The Sanctum | the bed (saved, respawn), bath, table, library, garden, forge, alembic | gold and keystones to build |
| The forge | weapons above what drops; dragon-scale armour | ores, ingots, essences, titan hearts, teeth |
| Alchemy | elixirs for the hard fights | reagents, formulae |
| Food | a meal fitted to where you go | ingredients, a fire |
| The realms | rulers' keystones, rare plants and animals, a step of the way | a rift or the Gate |
| The chronicle | the history's truth; step 8 of the way, when it ends | following its chapters |

Gold has more to buy than it gives: the town's works (400 to 1600 and more each), sealed weapons,
the forge's supplies, keystones, decorations. Discounts are gone: trust opens wares instead.

## The main story: the Old King

See [OLD_KING.md](OLD_KING.md). In short:

- one king of ten kinds per world, hidden at first; ten steps in three groups (finding out, the
  means, the approach), then the throne
- each step teaches a system the first time the line at the top follows it
- once you know what he is, his servants raid the towns you stand in; drive them off for the
  town's thanks
- at the throne he will not listen standing: bring him to his knees (at half his life, and a
  fifth), then speak his true name, raise the artefact crouched, or finish him
- each ending has its own reward: his crown, blade and hoard (the sword); his knights sworn to the
  towns and his treasury (the word); the bound freed and every town grateful (the artefact)
- any ending wins: the credits, then the world to explore on, the Astral Shore open; a
  merciful ending opens the Dream of the Sleeper
- a quarter of an hour later an heir of another kind takes the throne, and a new, longer way
  begins: an endgame that does not end

## The other stories

- **The chronicle** (your motive's chapters, [STORY.md](STORY.md), [STORY_GEN.md](STORY_GEN.md),
  [STORY_CHOICES.md](STORY_CHOICES.md)): the history of the world, its mystery and the choices at
  those who yield. Its end (the one who opened the seal) also completes step 8 of the king's way:
  the opener served him.
- **The towns** ([TOWNS.md](TOWNS.md)): tasks, reputation, the town's works, kin.
- **The realms** ([REALMS.md](REALMS.md)): twenty-one worlds behind the Gate, each with a ruler.

The line at the top shows one of them at a time: the Old King's way by default; **Y** moves it to
the chronicle or a town's task and back. The Golden Thread (G) leads to the king's step, or to
the chronicle's target when the line follows the chronicle.

## Co-op

Up to four players share a world. Choices are made together:
- at a target that yields, everyone near chooses; the most decide (a tie spares; after 25 s
  those who chose decide)
- at the faltering king, more than half of those at the throne must strike for the same ending;
  the sword needs no vote

## Death

You wake at your last shrine or bed with a fifth of your gold gone and half your health (in full
at home, or a second death within two minutes). For 15 s you can pay another fifth to go back
where you fell, but only once in five minutes: no walking back into the same pack again and again.

## Where the numbers come from

The bots (`--bot STYLE`, [PLAYTEST.md](PLAYTEST.md), [PLAYTEST_2.md](PLAYTEST_2.md)) play hour-long
runs and log what they do; `--bot king` follows the Old King's way. `--demo kingtest` walks a
seed's way to each ending; `test_worldgen` checks the way and two heirs on hundreds of seeds.

[FUN_AUDIT.md](FUN_AUDIT.md) scores the game on feel, flow, choice, pacing, fairness and loop friction
with four bot personas (`--bot novice`, `--bot-react 200` for a human's reaction, the plain bot as
the expert, `--bot spam`) and lists what to improve, most important first.
