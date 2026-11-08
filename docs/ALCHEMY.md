# Alchemy

Status: **implemented** (November 2026): `src/game/alchemy.c`, `src/game/alchemy.h`, the
ALCHEMY tab (O), save version 9.

Alchemy sits beside cooking ([`FOOD.md`](FOOD.md)) but works the other way round:

| | Cooking | Alchemy |
|---|---|---|
| Ingredients | wildlife, plants, larders | monster parts, strange finds, a few larder goods |
| Recipes | tags in any order ("meat + sharp") | exact formulae: these ingredients, these steps, in this order |
| Effect | a meal, 10-20 minutes, modest | an elixir: takes hold after 1-5 s, then 20-30 s, strong |
| Where | any fire; the Sanctum fireplace and kitchen | any shrine; at home (two bottles with the library's alembic, stage 2) |
| One at a time | yes | yes, and a meal and an elixir work together |

## Reagents

Every kind of monster leaves one (18% of normal ones, 60% of elites, bosses always two;
small split-off slimes rarely):

| Monster | Reagent |
|---|---|
| crawler | crawler ichor |
| brute | brute's gall |
| wisp | wisp ember |
| horned beast | horn shavings |
| skeleton | bone dust |
| slime | slime core |
| bat | bat membrane |
| spider | spider venom |
| wraith | shroud ash |
| golem | golem heartstone |
| shaman | spirit feather |
| bloater | bloater bile |
| floating eye | gazing lens |
| knight | knight's rust |
| worm | worm tooth |
| mimic | mimic tongue |
| necromancer | grave wax |
| harpy | harpy plume |
| hound | hound fang |
| troll | troll blood |

Strange finds lie about by region, like plants (one chunk in three has one, back every 20
minutes): quicksilver (mines, iron hells, brass), brimstone (the hells, sulphur, obsidian,
ember), ghost orchid (limbo, ossuary, barrow, hollow), starlight dew (heaven, astral, orrery,
starfrost, aurora), void salt (crystal abyss, abyssal, eldritch, leviathan), mandrake root (roots,
fungal deep, druidic), amber (bamboo, jade, terracotta, steppe) and phoenix feather (rare: sun
court, phoenix, sunblast). Some formulae also want larder goods: wild herb, salt, ember pepper,
honey, wine, frost berry, glow cap, beetle jelly, barrow root, royal jelly, star fruit.

## Brewing

At a shrine or anywhere at home, the ALCHEMY tab shows the cauldron. Each choice adds one
step: an ingredient, or something done to the brew (grind, heat, stir, chill, distill). X takes
the last step back. **Bottle** compares the steps with the formulae, exactly: the same
ingredients, the same steps, the same order, the same count. Anything else is a **murky brew**:
the ingredients are lost, it may boil over and scald you, and it tells you how close you came
("3 steps were right, in their place, of 5", "and too many"). So formulae can be found out by
trying, mastermind fashion. A brew that matches a formula you did not know teaches it.

Formulae are also found as **pages**: one in every vault hoard, 6% of chests, 4% of elites, a
quarter of bosses (a page you know is worth 25 gold). Half of the pages (all but the vaults')
are **torn**: they tell what goes in (the ingredients and how many steps), not how; a second
torn page of the same formula makes it whole. The murky brew tells both numbers of a
mastermind: steps right in their place, and steps right but elsewhere. Every character knows the first
two (Draught of Vigor and Swift Tonic).

## Drinking

From the tab, or V (the last brewed or drunk). The elixir bubbles in you for its onset, then
takes hold with a flash for its full time; the HUD shows BREWING IN YOU, then ELIXIR. Only one
works at a time; a second waits until the first is over. Dying ends it. For 90 seconds after
one ends you are TOXIC: an elixir drunk then costs 15% of your health and works at half, for
half as long. The belt holds four elixirs; the Draught of Vigor counts as a health potion
instead (potions and Vigor together, five at most). At home, with the library, an alchemist
sells three reagents a day (200 gold, 400 for strange finds, 800 for a phoenix feather).

## The formulae

| # | Elixir | Formula | Onset | Lasts | Effect |
|---|---|---|---|---|---|
| 1 | draught of vigor | crawler ichor > wild herb > heat > stir | 2 s | 20 s | 2.5% of your health back every second (counts as a health potion) |
| 2 | swift tonic | bat membrane > grind > salt > stir | 2 s | 25 s | 60% faster, attacks 25% faster |
| 3 | berserker's draught | brute's gall > ember pepper > heat > heat > stir | 3 s | 25 s | 80% more weapon damage, 25% more taken |
| 4 | stoneskin philtre | golem heartstone > grind > bone dust > chill > stir | 4 s | 25 s | 60% less damage taken, blows do not push you |
| 5 | troll's blood | troll blood > crawler ichor > heat > distill | 3 s | 20 s | 8% of your health back every second |
| 6 | ghost draught | shroud ash > ghost orchid > chill > distill | 3 s | 25 s | unseen until you strike |
| 7 | elixir of wings | harpy plume > harpy plume > starlight dew > stir > distill | 2 s | 25 s | wings: fly where you look |
| 8 | phoenix elixir | phoenix feather > brimstone > heat > heat > distill > royal jelly | 5 s | 30 s | a killing blow lifts you again; fire and lava cannot hurt you |
| 9 | arcane surge | wisp ember > glow cap > starlight dew > distill | 3 s | 20 s | spells cost nothing, cool half as long, 30% stronger |
| 10 | storm in a bottle | gazing lens > quicksilver > grind > heat > stir | 3 s | 25 s | blows arc to the next foe |
| 11 | dragonfire draught | brimstone > ember pepper > ember pepper > heat | 3 s | 25 s | every blow sets the foe alight; fire cannot hurt you |
| 12 | frost heart | frost berry > void salt > chill > chill > stir | 3 s | 25 s | foes close by freeze slow; frost cannot hurt you |
| 13 | vampire's kiss | hound fang > wine > grave wax > stir | 3 s | 25 s | a quarter of your weapon damage heals you |
| 14 | eagle eye | gazing lens > harpy plume > chill > distill | 2 s | 25 s | half your hits are critical |
| 15 | midas brew | mimic tongue > salt > honeycomb > heat > stir | 3 s | 30 s | what you kill drops far more |
| 16 | draught of dread | grave wax > ghost orchid > bone dust > grind > chill | 4 s | 20 s | foes close by run from you |
| 17 | titan's brew | horn shavings > brute's gall > grind > heat > stir | 4 s | 25 s | 40% more damage, blows throw three times as far, 30% more health |
| 18 | time syrup | quicksilver > amber > starlight dew > stir > distill | 3 s | 20 s | the world slows round you |
| 19 | golem's ward | golem heartstone > knight's rust > grind > heat | 2 s | 25 s | a ward of 60% of your health |
| 20 | spirit mirror | void salt > gazing lens > distill > chill | 3 s | 20 s | arrows, bolts and fire pass through you |
| 21 | mana font | glow cap > beetle jelly > slime core > distill | 2 s | 25 s | mana flows ten times as fast |
| 22 | tunneler's sight | worm tooth > mandrake root > grind > stir | 2 s | 30 s | the land round you is mapped; hidden things show |
| 23 | purity elixir | mandrake root > barrow root > salt > chill | 2 s | 30 s | fire, frost, poison and lightning cannot touch you |
| 24 | volatile draught | bloater bile > brimstone > stir > heat | 3 s | 20 s | blows burst round the foe |
| 25 | venom draught | spider venom > spider venom > grind > stir | 1 s | 30 s | hits poison hard |
| 26 | star elixir | star fruit > starlight dew > royal jelly > distill > distill | 5 s | 30 s | 30% more of everything, mana three times as fast |
| 27 | slime draught | slime core > slime core > stir > chill | 2 s | 25 s | jump twice as high, falls do not hurt |

## Code

- Formula steps are bytes: larder goods 0-63 (`FD_*`), reagents 64-127 (`RG_*`), actions
  128+ (`AS_*`). `alchemy_match` compares exactly; `alchemy_closest` counts steps in place.
- Elixir effects reuse the meal effects (`ME_*`, food.h): `food_fx` adds `alchemy_fx`, so
  every hook that reads a meal reads an elixir. New ones: lifesteal, crits, revive (in
  `game_hurt_player`), luck (more loot drops), chained lightning, bursts and poison on weapon
  hits (combat.c), frost and fear auras (`alchemy_update`), lightning immunity. Some act once
  as they take hold: invisibility, wings, Time Slip, a ward, the map round you.
- `--demo alchtest` brews every formula, checks a murky brew, the onset, the Phoenix and (with
  `--persist`) the save; `--demo alchemy` shows the tab.
