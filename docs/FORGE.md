# The forge

Status: **implemented** (November 2026): `src/game/forge.c`, `src/game/forge.h`, the forge plot
in `gen_pocket.c`, the FORGE tab (U), save version 12.

Smithing is optional, and it is the way to the strongest weapons in the game. A novice makes
plain iron blades; a master working starmetal with two essences, two runes and a titan heart
makes masterworks that nothing in the dungeon matches. Getting there takes the forge built up,
the skill learned, the metals found or bought, and a steady hand at the anvil.

## The forge (a Sanctum plot)

On the east lawn, from the island's second stage, built like the other plots:

| Stage | Cost | What it brings |
|---|---|---|
| 1 | 800 gold | an anvil, a hearth and a quench trough; iron, steel and silver; the smith, who sells supplies |
| 2 | 2500 gold, 1 keystone | a blast furnace: mithril, adamant and moonsilver; one rune |
| 3 | 6000 gold, 2 keystones | a runic forge: starmetal; two runes |

## Smithing

A skill from 1 to 10, raised only by working at the forge (forging, reforging, smelting,
breaking weapons up). Each level opens something and steadies the hand: the heat window of
every metal is a little wider.

| Level | XP | Opens |
|---|---|---|
| 1 | 0 | iron |
| 2 | 80 | steel |
| 3 | 200 | silver |
| 4 | 380 | a second essence, one rune (with forge stage 2) |
| 5 | 620 | mithril |
| 6 | 950 | moonsilver |
| 7 | 1400 | adamant |
| 8 | 2000 | two runes (with forge stage 3), masterworks |
| 9 | 2800 | starmetal |
| 10 | 3800 | a master |

## Materials

| Kind | What | Where from |
|---|---|---|
| Ore | iron, silver, mithril, adamant, star ore | ore veins in the rock (strike them with a blade; each vein once), loose ore in one chunk in six, golems. What a region holds: iron near the gate, silver in the north and the ossuary, mithril in the deep and four steps out, adamant in the hells and six steps out, star ore in the orrery, the astral void, the starfrost and nine steps out |
| Ingots | iron, silver, mithril, adamant, starmetal | smelted at the forge: two ore, charcoal, and flux for the hard metals (mithril and adamant need the furnace, starmetal the runic forge); breaking weapons up |
| Ingots, bought | steel (200), moonsilver (1500, one a day, forge stage 2) | the smith |
| Essences | ember, frost, storm, venom, holy, shadow, earth, arcane | monsters of a region's element: 4% of normal ones, 35% of elites, every boss two |
| Titan heart | | 30% of bosses |
| Supplies, only bought | charcoal (5 for 100), flux (80), quenching oil (150), rune ink (300) | the smith |

## Blueprints and runes

Every character knows the dagger, the sword, the shortbow and the staff. Every other weapon,
the lost arts too, is learned by **breaking one up** at the forge: it gives its metal back as
ingots (more for higher levels), teaches its blueprint, and teaches the **runes** of its powers
(the trigger and the effect separately: any known trigger can be inscribed with any known
effect). Legendaries are not broken up.

## Forging

At the anvil, the FORGE tab plans the weapon: a blueprint, a metal, essences (each becomes an
affix: ember fire, frost frost, storm shock, venom venom, holy keen, shadow vampiric, earth
heavy, arcane swift), runes (each a power), a titan heart (+10% quality, +2 levels) and
quenching oil (+6% quality). It shows the cost and what is missing.

Then the work itself: the metal cools all the time; the **bellows** (X or LEFT) heat it; each
**strike** (ENTER or UP) counts by where the heat is:

| Heat | Points |
|---|---|
| in the middle half of the metal's window | 3, perfect |
| in the window | 2, good |
| just outside it | 1, close |
| far outside | 0, a flaw (too cold: it cracks; too hot: it burns) |

Each metal has its own window and cools at its own speed: iron is forgiving (0.40-0.72), silver
wants it low (0.35-0.58), adamant and starmetal want it high and narrow (0.70-0.85, 0.80-0.92)
and cool fast. A weapon takes 5, 7 or 9 strikes by its size. Walking away loses the work.

The **quality** is the points over the most possible, plus the heart and the oil, less 10% for
three flaws or more. It makes the weapon:
- **Level**: your level, plus the metal's (steel +3, silver +2, mithril +6, adamant and
  moonsilver +9, starmetal +13), plus up to 6 for quality, plus half your smithing, plus 2 for a
  heart; 3 less if flawed.
- **Rarity**: common under 35%, magic, rare, epic from 75%. At 92% and more, with smithing 8 and
  a metal of the fourth tier or better, a **masterwork**: legendary, and its hidden affix awake
  at once.
- **Material**: iron and steel iron, silver and moonsilver silver (bane of the undead), mithril
  crystal (critical hits), adamant obsidian (20% more damage), starmetal (15% faster).

A master's starmetal greatsword at player level 6 is level 32 and hits about four times as hard
as a rare found at that level; later it stays well ahead.

**Reforging** the weapon in hand, with ingots of its own metal and charcoal, brings it up to
your level and above it (half the metal's bonus, quality, a third of your smithing), keeping all
it is; a perfect reforge also makes it a step rarer, up to epic.

## Code

- `forge.c`: materials, metals, smithing levels, the plan, the session (`forge_start`,
  `forge_bellows`, `forge_strike`, `forge_tick`, which runs even while the menu pauses the game),
  smelting, salvage, drops (`forge_monster_drop`), ore veins (`forge_mine`, from a blow that
  hits a wall), the FORGE tab.
- The shop sells the smith's supplies (`SH_SUPPLY`); the bot breaks up, smelts and forges for the
  weapon in its hand on each trip home.
- `--demo forgetest` (salvage, smelting, a novice's cold strikes, a master's session, a reforge,
  and with `--persist` the save), `--demo forge` (the tab), `--demo forgesession` (the anvil).
