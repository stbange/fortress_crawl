# Playtest 2: after food and alchemy

November 2026. 20 bot runs of 60 minutes each (20 hours of play): seeds 1337, 2024, 99 and
4242, each with the explorer, delver, archer, caster and quester styles (`--bot`, see
[`PLAYTEST.md`](PLAYTEST.md)). No crashes, no hangs. For comparison: the batches of 16 half-hour
runs before food (`sanct`), with food (`food`) and with alchemy (`alch`).

What the bots do not do as a player would: they never solve vault puzzles, hunt every animal
that crosses their path, eat whenever no meal is working, cook only what a campfire allows
unless they pass the Sanctum kitchen, and drink an elixir only against an elite, a boss or four
awake monsters. Read their numbers as "what the systems allow", not "what players will do".

## 1. The numbers

### Survival

| Batch | Deaths per hour | Close calls per hour | Health potions per hour | Damage taken per hour | Samples under half health |
|---|---|---|---|---|---|
| before food | 0.75 | 3.2 | 18.4 | 5419 | 3.4% |
| with food | 0.25 | 2.8 | 11.8 | 4734 | 2.4% |
| with alchemy | 0.62 | 1.2 | 8.8 | 4042 | 2.0% |
| **now, 60-minute runs** | **0.20** | **1.4** | **8.9** | **4933** | **2.1%** |

4 deaths in 20 hours. Not one 15-second sample in 20 hours shows a character under 30%
health. The archers and casters take 1100-1500 damage an hour and drink no health potion at
all; the melee styles take 5000-15000 and drink 6-27.

### Progress (per run, one hour)

| | Mean | Range |
|---|---|---|
| Level | 13.7 | 10-17 |
| Kills | 477 | 171-642 |
| Farthest tier | 1.7 | 1-3 |
| Farthest from the gate | 486 m | 238-1048 |
| Gold left at the end | 2298 | 5-12336 |
| Chapters finished | 0.7 (questers 3.3, the others 0-1) | |
| Trips home | 4.3 | |
| Sanctum stages built | 5.9 (about 2.4 keystones found per hour, all spent) | |

### Food

| | Per run, per hour |
|---|---|
| Ingredients picked up | 231 |
| Animals hunted | 96 |
| Dishes cooked | 54 |
| Dishes eaten | 5.7 |
| Recipes known | 5.7 of 32 |
| Time with a meal working | **93%** |

Dishes eaten: roast 69, hunter's stew 19, fried eggs 15, glow tea 6, all others 1-2. Cooked:
roast 234, frost sorbet 218, glow tea 170, honey bread 167, fried eggs 147, hunter's stew 109
(at the Sanctum fireplace); four-ingredient dishes: none.

### Alchemy

| | Per run, per hour |
|---|---|
| Reagents picked up | 99 |
| Formulae known at the end | **12.5 of 27** |
| Elixirs brewed | 16 |
| Elixirs drunk | 14 |
| Time with an elixir working | 10% |

Drunk: Draught of Vigor 221, Titan's Brew 26, Venom Draught 17, Swift Tonic 14, Stoneskin 4,
Troll's Blood 4, Vampire's Kiss 1. Vaults opened: 5 in 20 hours (the bots cannot do the
puzzles); chests: 31 per hour.

## 2. What to change, most important first

### 2.1 The game has become too safe, and the meal is why

Deaths fell from 0.75 to 0.2 an hour, and a meal is working **93% of the time**. A buff that is
always on is not a choice, it is a tax: it raises the floor (15-25% more health, regeneration
in fights) and the dungeon was tuned without it. The elixirs add little to this (10% of the
time) but are the strongest moments.

Proposals:
- **A. Make meals situational, not raw power.** Cut the plain stat dishes (roast +15% health,
  root mash +20%) to about half, keep the resistances, mana flow and utility at full strength. The
  value of a meal should come from choosing the right one for where you go (fire broth before
  the hells, frost sorbet before the north), not from always eating.
- **B. Hunger for variety instead of hunger.** The same dish twice in a row works at half
  strength, three times at a quarter ("tired of roast"). It costs nothing to implement and turns
  the 32 recipes from a list into a reason to cook.
- **C. Let danger see the buffs.** The danger of a place comes from its distance only; after an
  hour the bots are level 14 in tier 1-2. Add a soft floor: monsters are at least 60% of the
  player's level wherever they are. Lingering near the gate stops being safe; the far tiers are
  unchanged.
- **D. Campfire smoke.** Cooking at a campfire in the dungeon wakes monsters within 25 m (the
  open question in FOOD.md). The Sanctum kitchen and a Hut of Rest stay safe, which gives them a
  reason.

### 2.2 Food is far too plentiful, so cooking has no economy

231 ingredients and 54 dishes an hour for 6 meals: nine dishes in ten are never eaten, and the
larder is always full of the cheap ones. Roast (any meat + salt) wins because salt lies in
every larder.

Proposals:
- **E. A third of the animals and plants**, larders a half. Keep the regrowth timers.
- **F. A food pouch of 4 dishes** (all kinds together) instead of 250 of each. Cooking ahead
  for a long trip stays possible, hoarding does not.
- **G. Bring the fireplace into the field.** The three-ingredient dishes need the Sanctum, and
  the four-ingredient ones were never cooked. Let the hearth of Warden's Keep and Hut of Rest
  count as a fireplace (3 ingredients), and keep the kitchen (4) as the reason to build the
  dining hall to stage 3.
- **H. Salt rarer, and roast needs fire too** (meat + salty + fire at a hearth), so the first
  recipe a player learns is not the last one they use.

### 2.3 Alchemy: discovery is over too soon, and one elixir is a super potion

12.5 of 27 formulae in an hour means a player knows almost all of them in an evening, which
cuts the mastermind side of alchemy short. The pages come mostly from chests (25% of 31 an
hour). The Draught of Vigor (crawler ichor + herb) is cheap, known from the start and heals
80% of your health over 20 seconds: a sixth health potion that ignores the cap of five.

Proposals:
- **I. Pages much rarer, and torn.** Chests 6%, elites 4%, bosses 25%, vaults always. Half of
  the pages are torn: they show the ingredients but not the steps, or the first half only, so
  the player still has to work the rest out at the cauldron.
- **J. Better murky feedback.** Today it says "3 steps right, in their place". Add the
  mastermind's second number: "and 1 right but in the wrong place". Then finding a formula by
  trying takes 4-6 brews instead of dozens, and it is fun rather than grinding.
- **K. Toxicity.** Every elixir leaves you *poisoned by alchemy* for 90 seconds after it ends; a
  second elixir in that time works at half strength and costs health. Elixirs stay strong and
  rare moments instead of a rotation.
- **L. Vigor down to 2.5% a second**, and the potion cap counts it: it is the alchemist's
  potion, not a sixth one for free.
- **M. A belt of 4 elixirs** carried at once (any kinds); the rest wait in the Sanctum stash.

### 2.4 Ranged play is safe, melee is not

The archers and casters kill as many as the delver (490 and 584 against 507) but take a fifth of
the damage and never drink a potion. Food and elixirs did not change this; they were already
there in the first playtest. Proposals:
- **N. More monsters that close the distance**: hounds and spiders a little more common from
  tier 1, and wraiths that reappear next to an archer rather than behind them.
- **O. Shooters get hurt for standing still**: skeleton archers and wisps lead their shots
  against a target that does not move.

### 2.5 Gold still piles up

2300 gold on average at the end of the hour, 12 000 for one archer, after 6 Sanctum stages.
Keystones (2.4 an hour) hold the building back, so gold has nothing left to buy. Proposals:
- **P. An alchemist and a cook at home**: once the library and the kitchen are built, the Sanctum
  sells a few rare reagents and spices each day (rotating stock), at prices that matter
  (200-800). It gives gold a use and a way past bad luck with drops.
- **Q. Keystones for gold once a day**: the library sells one keystone a day for 2000 gold, so
  building is never fully stuck.

### 2.6 The story waits for those who look for it

Only the questers finish chapters (3.3 an hour); the other styles finish 0-1, because nothing
in the dungeon draws them to the story target. This is fine for a player who wants to wander,
but a new player may never notice the story. Proposal:
- **R. The thread finds you.** After 10 minutes without chapter progress, the next named enemy
  sends its patrol toward the player (an elite and two followers), with a line of text. The
  story comes to those who do not go to it.

### 2.7 Smaller notes

- Bots hunt about 100 animals an hour. With E this halves; for players it is optional anyway.
- The Sanctum's four-ingredient dishes and most elixirs need home or rare parts; that is
  intended, but the bot shows that without a reason to go home (only gold or low health), the
  kitchen is never used. G and P give reasons.
- Vaults: the bots open none, so keystone and formula supply in real play will be higher than
  measured here (one of each per vault).

## 3. Suggested order

1. **A, B, E, F** (meals situational, variety, fewer ingredients, the pouch): a day; then a batch.
2. **I, J, K, L** (alchemy pacing): a day; then a batch.
3. **C, D** (danger floor, smoke): half a day; check deaths per hour lands near 0.6-1.0.
4. **G, H, M, N, O** (field hearths, roast, belt, ranged pressure): two days.
5. **P, Q, R** (Sanctum merchants, daily keystone, the story comes to you): two days.

Target after all of it: 0.6-1.0 deaths an hour for the melee styles, a meal working about half
the time and chosen for the place, 3-6 formulae found per hour, an elixir in every hard fight
rather than every fight, and gold under 1000 at the end of an hour.

## 4. After the changes (A-R)

All eighteen proposals are in (commit 8e25e80). The same 20 runs of 60 minutes again, on the
new build:

| Per hour | Before | After | Target |
|---|---|---|---|
| Deaths | 0.20 | **0.65** (delver 1.75, quester 1.25, explorer 0.25, archer and caster 0) | 0.6-1.0 for melee |
| Close calls | 1.4 | 1.9 | |
| Health potions drunk | 8.9 | 12.9 | |
| Ingredients picked up | 231 | 120 | |
| Animals hunted | 96 | 39 | |
| Dishes cooked / eaten | 54 / 5.7 | 7.8 / 4.2 | |
| Time with a meal working | 93% | 72% | about half |
| Formulae known after an hour | 12.5 | **3.95** | 3-6 |
| Elixirs drunk | 14 (Vigor 221 of 287) | 1.75 (Swift Tonic most) | one per hard fight |
| Gold left after an hour | 2298 | 2686 | under 1000 |
| Chapters (non-questers) | 0-1 | 0-1, four runs with one | |

What worked: deaths are in the target range for the melee styles; dishes are cooked to be eaten
and the bots now vary them (fried eggs 30, glow tea 28, honey bread 12, roast only at a hearth);
formulae come at the intended pace; the Draught of Vigor is no longer a sixth potion.

What did not, and what to do next:
- **Gold still piles up.** The merchants and the daily keystone need the library, which needs
  the island's second stage and a keystone: the bots built the library once in 20 hours, so
  none of the new sinks was ever offered. Next: open the alchemist and the keystone with the
  hall's second stage instead.
- **Elixirs are now rare in the bots' hands** (under two an hour): rarer pages, the belt and
  toxicity together. The bots only brew what they know and drink only against elites and packs;
  a player who experiments at the cauldron will brew more. Worth watching in real play before
  changing it; if it stays low, double the reagent drops of normal monsters.
- **Ranged play still never dies.** Hounds, spiders, wraiths at your side and leading shots
  raised the archers' damage taken only a little. Next: fewer arrows' worth of distance, by
  letting more monsters rush (crawlers and horned beasts sprint when shot at).
- **The meal is still working 72% of the time** for the bots, which eat whenever none is; real
  players will eat less often. Leave it.
- **Bot**: one caster fell into a deep shaft at minute 10 and never climbed out (stuck 52 times,
  one recall); the bot should recall sooner when it has not moved for a minute.
