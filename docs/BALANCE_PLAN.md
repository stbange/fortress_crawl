# Balance and pacing: implementation plan

Status: **done** (November 2026); results in [`PLAYTEST.md`](PLAYTEST.md), section 4.
Where the implementation differs from the plan:
- E1: the stairs already had a solid centre; the falls were through the ring of steps, so
  the spiral got a second, interleaved flight (a missed step drops at most 7 blocks).
- A5: walking up to the shrine after a death does not restore fully (you respawn right next
  to it); instead a second death within two minutes restores fully, against death loops.
- B2: levels need 120 x 1.4^(L-1) XP (1.3 from level 10), monster XP grows 12% per level.
- A4: brutes, horned beasts and trolls also got more health (130, 60, 160), or they died
  before their new attacks came.
- H: going back where you fell uses the Golden Thread key for 15 s after a death; Recall
  goes back when cast again within a minute (its cooldown would not allow 3 s).
- Added from the playtests: monster blows need a clear line; potions end burns and poison;
  respawn clears 30 m and sends a waiting boss away.

Original plan: This answers the problems found by the automated playtests in
[`PLAYTEST.md`](PLAYTEST.md). Each change names where it goes in the code and what number
the playtest bot should show afterwards, so every step can be checked with a batch of runs
(`--bot STYLE --bot-minutes 30`) before and after. Estimates are for one developer.

## 1. Targets

What a 30-minute run of the bot should look like when this plan is done (today in brackets).

| Measure | Target | Today |
|---|---|---|
| Deaths, delver / explorer / cautious | 1-3 / 0-2 / 0-1 per half hour | 0 / 0 / 0 |
| Close calls (below 15% health) | 2-6 per half hour | 0-2 |
| Runs that drink no health potion | under a quarter | 9 of 16 |
| First kill | within 90 s for every style | 4-6 min (delver 1 min) |
| Levels in the busiest 3 minutes | at most 3 | up to 8 |
| Minutes per level from level 6 on | 3-6 | 4-12 |
| Gold left unspent at the end | under 1000 | up to 15,000 |
| Fights with no damage dealt for 20 s (stalemates) | 0-1 per run | up to 9 |
| Fall damage share | under 3% of damage taken | 5-15% |
| Chapters finished by the quester | 1-2 | 0 |
| Damage per kill, brute / troll / horned | 10-25 | 4-5 |

The bot is a strong, perfectly aiming player. A human should die somewhat more often than
the bot, so these targets are on the gentle side.

## 2. Changes

### A. Survival: make danger real (2.1 in the notes)

**A1. Heal becomes a found spell, and slower.**
`spells_give_start` (spells.c) stops giving `SU_HEAL`; new characters start with Flame
Bolt, the Golden Thread and the Sanctum Door. Heal's table entry: cooldown 6 s to 12 s,
the amount from `0.3 + 0.05 * rank` to `0.2 + 0.04 * rank` of max health. Heal scrolls
appear from danger level 3 (`random_scroll` already filters by tier; give Heal tier 1).
Old saves keep the Heal they have.

**A2. No regeneration in a fight.**
`combat_update` (combat.c): health comes back only when `since_hurt > 6 s` (now 4 s) **and**
no awake monster is within 12 m. Well Fed regeneration (the Sanctum feast) keeps working in
fights; that is its point.

**A3. Monsters keep up with the player.**
`monster_spawn` (monsters.c): damage `1 + 0.10 * level` becomes `1 + 0.14 * level`; health
stays at `0.18`. Region tiers (`1 + 0.04 * tier` damage) become `0.08`. Elites deal 1.6
times as much instead of 1.5. Player damage grows 7% per level plus weapons. These numbers
make a monster at the edge of what the bot reaches in half an hour hit about 35% harder than
today.

**A4. Heavy hitters hit.**
New close-range attacks, in `behave` / `windup_release`:
- *Brute*: a ground slam when you stay within 2.5 m for more than 1.5 s (the golem's
  `ground_blast` at 1.2 times its damage, radius 2.5).
- *Troll*: a backhand sweep in a 160° arc after its boulder throw, knockback 6.
- *Horned*: the charge stuns for 0.6 s on a hit and is aimed where you will be (lead by
  your velocity times 0.4 s).

Each keeps a clear windup (the existing `MS_WINDUP` with its red eyes).

**A5. Death has a cost.**
On death (`update_player`, game.c): lose 20% of the gold you carry, and the shrine's
restoration gives back only half health and mana (walking up to the shrine afterwards
still restores fully). Losing equipment is too harsh for a game without a stash. A banner
says what was lost. The Sanctum bed as a respawn point restores fully (its
reward for going home).

**A6. Potions matter.**
With A1-A3 potions become the main heal. Lower the cap for health potions from 9 to 5
(fewer, so each counts) and halve their drop chance below danger level 3
(`loot_drop`: `0.08` to `0.04` there). Mana potions unchanged.

Check: deaths and close calls in the table; potion use; the Sanctum is visited by the
cautious bot (it goes home when out of potions).

### B. Pacing: no empty start, no level burst (2.2)

**B1. A first encounter.**
`spawn_chunk` (monsters.c): the chunks of danger level 0 and 1 always get 2-3 crawlers
(today 75% chance of 1-3), and the vestibule's exit chunk gets a pack of 3 placed in sight
of the gate (spawned when the world starts, like the shrine).

**B2. No multi-level jumps, shorter late gaps.**
The burst comes from a level-1 character killing its first pack of level 5-8 monsters
after the empty start: one kill is worth more than a whole level. In `game_add_xp`
(game.c), a single kill gives at most 30% of the XP needed for the next level, so three
or more kills are needed per level. B1 also brings the first fights closer to the
character's level. Later, the curve `xp_next = 60 * 1.4^(L-1)` grows faster than monster
XP (`1 + 0.15 * level`), which makes the 4-12 minute gaps; from level 10 the factor
becomes 1.3 per level instead of 1.4 (level 15 then needs about 40% less XP for its last
level).

Check: first kill under 90 s; no more than 3 levels in any 3 minutes; 3-6 minutes per level
later.

### C. Gold and loot with a purpose (2.3)

**C1. A shrine merchant.** Walking up to an attuned shrine opens a small shop tab
(the character menu's fifth tab, only at a shrine):
- *Rank up a known spell*: 100 x rank^2 gold.
- *Reroll a weapon's affix*: 50 + 20 x item level.
- *Potions*: health 40, mana 30 (within the cap).
- *A relic of your choice* among those found before: 500 x (count + 1).

**C2. Sanctum upgrades** (realm.c, gen_pocket.c, needs the food plan's kitchen later):
pay gold to unlock the bath (500), the feast (1000), the library's scroll of the day
(1500). The realm starts with the bed only. This also makes the Sanctum something to
return to.

**C3. Less clutter.**
- "Scrap all common" and "scrap all worse than equipped" in the inventory (Del held).
- Spell scrolls for spells you know rank them up (as now), and once you know 12 spells,
  new scrolls are three times as likely to be one you know (`random_scroll`).
- Scroll drop chance `0.035` to `0.025`.

Check: unspent gold under 1000; spells known under 20 at the end of a delver run.

### D. Melee across height differences (2.4)

**D1. Reach from the chest, with more height.**
`sword_strike` (combat.c): measure `dy` from the chest (eye - 0.5) to the target's centre,
allow `|dy| < 2.8` when the target is below; line of sight to the target's top, not its
centre (`phys_line_of_sight(eye, top)`), so a slime below a ledge edge can be struck.
The same for the alternate attacks.

**D2. Monsters that cannot reach you give up.**
`monsters_update`: a monster whose target stays out of reach for 6 s with less than 1 m of
progress turns away (`MS_IDLE`, sleeps 10 s) unless it is ranged; a slime or spider one
block below that *can* hit you keeps doing so, but D1 lets you answer.

Check: stalemates 0-1 per run (the bot's STALEMATE events).

### E. Falls (2.5)

Most falls were 13-45 m, the height of one or more layers: that matches the open
middle of the spiral staircases between layers (`carve_shafts` and `carve_helix_w`,
gen_common.c). First confirm it with screenshots at the logged FALL coordinates; some
falls are knockback off ledges.

**E1. A newel post.** The helix gets a solid centre column (1x1 pillar) from bottom to top,
so the only way down is the steps. Check the generator test still connects every portal
(`test_worldgen`).

**E2. A rim.** The top opening of each staircase gets a knee-high rim block on the open
sides (not the entry side) and a different floor block, so it is visible from a distance.

**E3. A cap.** Fall damage is capped at 35% of max health (game.c), so a misstep hurts
but does not kill a healthy character.

Check: fall share under 3%; `test_worldgen` passes.

### F. The story moves (2.6)

**F1. The first chapter is near.**
`story_chapter_target` (story.c): chapter 1 (arc 0, the captain) lies at
`d = 12 x 0.42 = 5` chunks today, and up to 3 layers up or down. Make chapter 1 always on
the gate's layer or one below, at 3-4 chunks, and keep later chapters as they are.

**F2. The thread goes down the stairs.**
`story_cast_thread` (quests.c): when the goal is on another layer, the thread leads to the
staircase, **down its steps** (the helix positions are known from the portal's phase), and
on to the next layer's exit node, instead of ending at the top.

**F3. A layer hint on the compass line.** The tracker shows an arrow (up or down) and the
count, and the map (M) marks the staircase the thread uses.

Check: the quester (after the bot follows stairs reliably, see section 3) finishes chapter
1 in every seed; the explorer sometimes.

### G. Combat variety (2.7)

**G1. Mixed packs early.** From danger level 2, a pack has a 50% chance of one support
monster (shaman, wisp or skeleton archer) mixed into a melee pack (`spawn_chunk`).
**G2. Elements from tier 1.** `monster_spawn`: monsters carry the region's element from
tier 1 (now from the rank where `m->rank >= 3`), at half strength until tier 3.
**G3. Readable knights.** The knight's shield glints and makes a sound when it blocks (it
does), and its back takes double damage, so circling is rewarded, not only required.
**G4. Skeleton arrows.** They are the top damage source because they never miss a
standing target: give them a 0.25 s draw sound and a small spread (2°), so moving sideways
avoids most of them.

Check: damage share of skeletons under 15%; at least 6 kinds above 5% of damage taken.

### H. Smaller changes (2.8)

- **Return to where you fell.** A respawn choice for 10 s: the shrine (free) or the place of
  death (costs the 20% gold of A5 again). Recall gains a second use: cast twice within 3 s
  to go back to the last Recall's starting point.
- **Vault hoards** roll one rarity higher and always hold a relic, so a solved puzzle beats
  ten chests.
- **Hard-stuck spots**: look at the logged coordinates with `--pos ... --fly --shot`; iron bars
  and decorations that look passable get collision that matches.

## 3. The playtest bot needs these too

To measure the targets above the bot has to play like a person in a few more ways
(bot.c):
1. Strafe round shielded knights (circle when the target is a knight facing it).
2. Use the shrine shop (C1) by a simple rule: rank up the best attack spell, buy potions up
   to the cap.
3. Follow staircases reliably: with F2 the thread itself leads down, so the quester follows
   it to the end.
4. Report potion count at death, gold lost, and the respawn choice (H).

## 4. Order and effort

| Step | Changes | Time | Batch of runs after |
|---|---|---|---|
| 1 | D1, D2, E1-E3 (fixes, no balance risk) | 2 days | yes |
| 2 | A1-A6, B1-B2 (the core balance pass) | 3-4 days | yes, then tune the numbers in A3 and B2 |
| 3 | F1-F3 and bot stair following | 3 days | yes |
| 4 | G1-G4 | 2-3 days | yes |
| 5 | C1-C3, H | 4-5 days | yes |
| 6 | Bot updates (section 3), final tuning | 2 days | 32 runs, update PLAYTEST.md |
| | **Total** | **about 3-4 weeks** | |

Every step runs the same 16-run batch (six styles, seeds 1337, 42, 777) and compares the
summary table with the previous one. Steps 2 and 4 change the feel of combat the most and
need a human playtest too: the bot does not mind being hit, a person does.

## 5. Risks and open questions

- **Too hard after A1-A6.** The changes stack; apply A1-A3 first, measure, then A4-A6.
- **Death cost** (A5) may feel bad without a way to get the gold back; H's "return to where
  you fell" softens it. Alternative: no gold loss, only the half restore.
- **The merchant** (C1) needs UI on every input (mouse, pad, touch); it reuses the
  inventory grid and its navigation.
- **Old saves**: characters keep Heal and their gold; the new numbers apply from loading.
- **Co-op**: monster health already scales with players; damage should not (one player
  drawing aggro should not take more).
