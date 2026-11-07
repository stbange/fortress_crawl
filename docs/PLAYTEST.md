# Playtest notes (automated, November 2026)

32 playtests of 30 game-minutes each (16 hours of play) by the playtest bot
(`--bot STYLE`, `src/game/bot.c`): six play styles on three seeds (1337, 42, 777), in
two rounds. The first round found a bug that kept monsters from spawning below the
gate's layer (fixed in 203b85c); the numbers below are from the second round unless
stated otherwise.

**Play styles:** *explorer* (sword, goes where it has not been), *delver* (heads away
from the gate, deeper and farther), *archer* (bow, keeps its distance), *caster*
(staff and attack spells), *quester* (follows the Golden Thread to the chapter target),
*cautious* (heals early, uses Sanctuary, Fear and Mirror Image).

**What the bot cannot do**, so read the numbers with that in mind: it aims perfectly and
turns instantly; it does not solve vault puzzles; it does not circle round shielded
enemies; its pathfinding gets stuck now and then (about 4 times an hour it gives up and
recalls); and the quester still struggles to follow stairs (see 2.6), so its story
numbers say more about the bot than the game.

## 1. The numbers

| Run | Level | Kills | Deaths | Gold | Farthest from gate | Tier | Damage taken | First kill |
|---|---|---|---|---|---|---|---|---|
| 1337 explorer | 12 | 170 | 0 | 2431 | 327 m | 1 | 1405 | 6:00 |
| 1337 delver | 14 | 323 | 0 | 5156 | 552 m | 2 | 6482 | 1:00 |
| 1337 archer | 8 | 91 | 0 | 561 | 174 m | 0 | 104 | 6:00 |
| 1337 caster | 11 | 137 | 0 | 1115 | 234 m | 1 | 240 | 6:00 |
| 1337 cautious | 12 | 193 | 0 | 1912 | 224 m | 1 | 1531 | 6:00 |
| 42 explorer | 10 | 104 | 0 | 1059 | 276 m | 1 | 1304 | 4:30 |
| 42 delver | 18 | 419 | 0 | 15148 | 1002 m | 3 | 15461 | 1:00 |
| 42 archer | 7 | 62 | 0 | 327 | 167 m | 0 | 38 | 4:30 |
| 42 caster | 10 | 121 | 0 | 970 | 313 m | 1 | 188 | 4:30 |
| 42 cautious | 10 | 149 | 0 | 1166 | 221 m | 1 | 806 | 4:30 |
| 777 delver | 12 | 234 | 0 | 2778 | 485 m | 2 | 3936 | 1:00 |
| 777 archer | 11 | 157 | 0 | 1471 | 289 m | 1 | 363 | 2:45 |
| 777 caster | 12 | 203 | 0 | 2598 | 179 m | 1 | 495 | 2:45 |

(Questers left out: 3 to 14 kills, see 2.6.)

- **Deaths: 0 in 8 hours** of play (1 in the first round). Two close calls (below 15%
  health) in all runs.
- **Potions:** 9 of 16 runs drank none at all; most runs end with 9/9, the cap.
- **Gold:** median about 1100 per half hour, up to 15,000; there is nothing to spend it on.
- **Where damage comes from:** skeleton arrows 6820 (the most), knights 4756, spiders 2630,
  slimes 2506; per kill knights deal 87, eyes 54, skeletons 24, wraiths 20; crawlers,
  bats, brutes, horned and trolls each 5 or less.
- **Time in a fight:** 3 to 14% of play for most styles, 18 to 38% for the delver.

## 2. What to improve, most important first

### 2.1 It is too easy to survive
No deaths in 8 hours, potions unused, the Sanctum never needed. The reasons, in order:
- **Heal** is a starting spell, costs 25 mana, and restores a third of your health every 6
  seconds; combined with the passive regeneration it out-heals most packs.
- **Monsters scale slower than the player.** Player damage grows 7% per level plus weapon
  quality (weapon score rises from 1.0 to 4-11 in half an hour), monster health 18% per
  danger level (one level per chunk, 32 m). The bot outlevels the danger at the same pace
  it walks outward; by tier 2-3 the delver is level 14-18 against level 15-30 monsters
  and kills them in about a second.
- **Most monsters do not threaten**: brutes, horned beasts and trolls dealt 4-5 damage per
  kill. Their attacks are slow and readable (good), but nothing punishes standing in
  front of them.

Suggestions: make Heal a found spell (or a long cooldown), let health regeneration stop
for a few seconds after any hit, raise monster damage with danger level faster (10% now,
health 18%), give the heavy hitters an attack that hurts if you stay close (a stomp, a
sweep), and give death a cost (some gold, or the run's unbanked loot) so that caution
matters.

### 2.2 The first minutes are empty, then everything comes at once
- First kill after 4-6 minutes for every style except the delver (1 minute): the two chunks
  round the gate hold 0-3 crawlers each, and the dungeon there is a maze.
- Then a burst: the caster went from level 2 to 10 in 2.4 minutes, the cautious bot from 2
  to 7 in 3 minutes; afterwards a level every 4-12 minutes (the XP curve grows 1.4 times
  per level, monster XP 15% per danger level).

Suggestions: a small first encounter within a minute of the gate (a few crawlers in
sight), lower monster XP at low levels or a flatter early curve (1.25 instead of 1.4 up
to level 8), so the first hour gives a level every few minutes instead of eight at once.

### 2.3 Gold and loot pile up with no use
Gold up to 15,000, bags full (15/15) in every run after 10-15 minutes, most finds are
common or magic. 10-34 spell scrolls per run; the delver knew 34 spells.

Suggestions: a sink for gold (a shrine shop, a smith that rerolls affixes, buying
ranks of known spells, or upgrades for the Sanctum), auto-scrap of worse commons (or a
"scrap all below" button), fewer but stronger scrolls (rank-ups rather than new spells
once you know 10-15).

### 2.4 Small monsters below a ledge can hit you, you cannot hit them
Stalemates (20 s of fighting without denting the monster) happened 30 times, nearly
all with the monster 1-2 blocks lower and 1-2 m away: slimes and spiders one block down
(they still hit you: monster reach is measured from the feet, the sword from the eye with
a 2.2 m height limit and a clear line), brutes and trolls two blocks down in a pit (neither
side can hit). In the first round slimes dealt 426 damage to the delver this way.

Suggestions: let the melee check measure from the chest and allow 2.5-3 m of height
difference when the target is below; let melee strike over a ledge edge (line of sight
to the top of the target, not its centre); and give monsters that cannot reach you a
way out (climb, or give up the chase).

### 2.5 Shafts between layers and the new fall damage
53 falls in the second round, mostly 13-45 m down shafts between layers (one 81 damage),
5-15% of all damage taken. The shafts are easy to walk into and hard to see.

Suggestions: mark shaft edges (a rim, a different floor block, a faint light from below),
put water at the bottom of long drops, or cap fall damage at a third of max health so a
misstep does not kill.

### 2.6 The story moves very slowly
One chapter completed in 16 runs (8 hours). Chapter targets were 90-830 m and
1-4 layers away. The Golden Thread only leads to the top of the staircase on the current
layer; on the stairs (often spiral, one block per step) it points back at their top. Even
with a bot that follows the thread, the way down is not obvious.

Suggestions: let the thread continue down the stairs to the next layer's exit, put the
first chapter target within about 100 m and one layer, and show "x layers down" as an
arrow on the compass line.

### 2.7 Combat variety
Skeleton archers deal a quarter of all damage; knights are the only real melee threat
(their shield takes 75% from the front, which a player has to learn to circle). The many
other kinds die before they matter. Hound packs and spiders are the next most dangerous.
Elements matter only far out: burn and poison were 18% of the damage taken at tier 3,
nearly nothing before.

Suggestions: more ranged and support enemies early (a shaman or wisp in most packs),
elemental effects from tier 1, and telegraph knight shields more clearly.

### 2.8 Smaller notes
- **Recall** (and dying) puts you at the shrine, often far behind where you were, in an
  area you already cleared: one bot spent 10 minutes there without a fight. A "return to
  where I fell" option (the Sanctum door already does this) would help.
- **The Sanctum** was never visited by the bot: with full potions and Heal there is no
  need. Its value depends on 2.1.
- **Vaults:** none opened (the bot cannot solve puzzles); chests are plentiful (8-25 per
  run), so the vault hoards should be clearly better to be worth the puzzle.
- **Hard-stuck spots** (the bot gave up after 24 s): about half are bot pathfinding, a few
  were in front of iron bars and decoration a path seemed to lead through. Worth a look
  with `--pos` at the coordinates in the logs.

## 3. Running the playtests yourself

```
SDL_AUDIO_DRIVER=dummy xvfb-run -a ./build/fortress_crawl --seed 42 --fresh --mute \
    --bot delver --bot-minutes 30 --bot-log delver.log --lowres 320 180 --window 320 180
```

A 30-minute run takes 2-5 minutes. The log has a status line every 15 s, events (DEATH
with its cause, LEVEL, TIER, FALL, CLOSECALL, STALEMATE, HARDSTUCK, REALM) and a summary:
damage taken by source and by monster, kills by kind and grade, loot, skills and spells.
