# Fun audit (November 2026)

An audit with the *game fun evaluator* method: instrument the game, play it with bot personas,
score six pillars (feel, flow, choice, pacing, fairness, loop) and turn every failing number into a
concrete change. The method was written for arcade games; how it was adapted to a first-person,
open-ended dungeon crawler is noted where it matters.

```text
=== GAME FUN AUDIT REPORT ===
Runs Executed: 5 per persona x 30 min (Novice, Human-Proxy @ 200ms, Optimal) + SpamBot; seeds 1337, 42, 777, 2024, 99

Pillar Scores (0.00 - 1.00):
- Kinesthetics & Feel (M_feel):   [0.61] | Latency: 1 frame (17 ms at 60 fps), DropRate: 39%, Channels: 2.5 core / 3.1 high-impact
- Flow & Skill Gap (M_flow):      [0.86] | Win/Surv: Rand=0%, Human=59%, Opt=56%   (sigma Human 0.48)
- Strategic Entropy (M_choice):   [0.84] | H_norm=0.36, max p=0.53, SpamBot Ratio=0.66
- Pacing & Tension (M_pace):      [0.59] | Tension CV=1.21, Near-Misses=0.4/min, no exhaustion
- Fairness & Clarity (M_fair):    [0.55] | Hurtbox Ratio=2.25 (enemy shots), Telegraph: 0 ms for wisps and shamans, Contrast 1.0-1.8:1 (dragon)
- Loop Friction (M_loop):         [0.74] | TTFA=1.5-2.4s (desktop), Retry=3.0s

COMPOSITE FUN SCORE: 63.4 / 100   (functional, lacks spark or balance)
After P1 (section 5): 69.4. Projected after P2 as well: about 83 (engaging, solid loop)

Top Bottlenecks Identified:
1. M_feel  -> no input buffer: 39% of attack presses held for a human's 200 ms are lost to the cooldown
2. M_fair  -> enemy shots hit a box 1.5x the player's body (2.25x its area); wisps and shamans fire with no wind-up
3. M_pace  -> fights last ~1 s and fill 9-19% of the time: long flat troughs, rare spikes, few near-misses
4. M_fair  -> dark creatures on dark stone: a dragon's wings and neck are 1.0:1 against the rock

Planned Code Interventions: see "Improvements" below (P1 first).
```

## 1. How it was measured

### Instrumentation added for the audit

All of it is inactive in normal play.

| What | Where | How |
|---|---|---|
| Novice persona | `--bot novice` (bot.c) | random valid inputs at 2-4 Hz: move, turn, jump, attack (50%), spell (10%); no sense of the room |
| Human proxy | `--bot-react 200` (any style) | the bot sees and acts only every 200 ms +- 35 ms and holds its inputs in between |
| Oracle | `--bot explorer` | the existing bot: no delay, exact positions |
| Spam bot | `--bot spam` | the explorer with nothing but the primary attack (no spells, flasks, elixirs, panic spells) |
| Tension index T(t) | bot.c, once a second | (threats awake within 15 m / 6 + share of the second spent attacking or casting + health missing) / 3 |
| Near-misses | combat.c, enemy shots | a shot that came within 1-1.5x the box it hits, then left it without hitting |
| Dropped inputs | bot.c with `--bot-react` | an attack held for one reaction window that never fired (the weapon still cooling) |
| Action use | `g_tele.uses[8]` | blade, bow, staff, alternate attack, attack spell, utility spell, throw, elixir |
| Time to play, frame times | `FC_TIMING=1` | the first frame, the first frame with the world round the player ready, then 10 s of frame times |

The bot log ends with a `FUN` line (tension mean, sd, CV, longest run above 0.9, near-misses,
intents and drops, uses).

### Adaptations

- **W, the objective**: there is no win in a 30-minute open run, so W = progress x survival.
  Progress is the farthest distance from the gate in the fortress over 400 m (realms excluded);
  survival is 1 - deaths per hour / 4. A bot that walks far and never dies scores 1.
- **Hurtboxes**: in first person the player has no sprite; the "visual" body is the 0.3 m
  half-width collider, against the box enemy shots hit.
- **Choices** are read from the bots, which never switch weapons and never use alternate attacks:
  H_norm is a floor for what people will do, not an estimate of it.
- **Frame times** were measured on software rendering (no GPU in the test container): 31.7 ms mean,
  57 ms worst. That says little about players' machines; latency is judged from the code (an
  attack starts the frame its input arrives).

## 2. The numbers

Five runs of 30 minutes per persona:

| Persona | W (sd) | Progress | Survival | Deaths/h | Kills/min | Tension mean / CV | Near-misses/min | Dropped attacks |
|---|---|---|---|---|---|---|---|---|
| Novice | 0.00 (0.00) | 0.10 (39 m) | 0.00 | 6.8 | 0.1 | 0.21 / 0.59 | 0.31 | - |
| Human proxy, 200 ms | 0.59 (0.48) | 0.96 (441 m) | 0.60 | 2.0 | 7.7 | 0.11 / 1.21 | 0.39 | 39% |
| Oracle | 0.56 (0.27) | 0.77 (344 m) | 0.70 | 1.2 | 8.8 | 0.08 / 1.58 | 0.34 | - |
| Spam | 0.39 (0.23) | 0.75 (304 m) | 0.50 | 2.0 | 6.9 | 0.11 / 1.43 | 0.38 | 36% |

Action use (all runs of a persona): human proxy blade 1566, attack spell 1795, utility 12, throw 18,
elixir 4, bow / staff / alternate attack 0. Fights last 0.8-1.3 s on average and fill 9-19% of the
time. Level-ups come about every 3 minutes; the median gap between any two banners (a pickup, a
level, a find) is 1.3 s.

What killed the human proxy, the oracle and the spam bot (13 deaths): skeleton arrows 4 (two
with five potions in the bag), boss-grade brutes in tier 0 near the gate 3, other melee 5 (a troll,
a slime, two horned, a brute), burning 1.

Feedback channels per event (from the code):

| Event | Channels | Count |
|---|---|---|
| swing | weapon animation, sound | 2 |
| jump / land | sound | 1 |
| pickup | sound, sparkle, the counter | 3 |
| hit on a monster | its flash, particles, sound, knockback | 4 |
| kill | sound, two particle bursts (a flash for elites) | 3-4 |
| hurt | sound, red vignette, push (rumble on a gamepad) | 3 |
| level-up | banner, sound, flash | 3 |
| death | sound, banner, red fade | 3 |

Core actions average 2.5 (target 2.5), high-impact events 3.1 (target 3.5). There is no hitstop,
no camera impulse except near dragons, and no number or marker on a hit.

## 3. Pillar by pillar

**Feel (0.61).** Latency is one frame and ground movement eases in (90% speed in 0.16 s), which is
good. But there is no input buffer: an attack pressed while the weapon cools is lost unless held,
and 39% of the human proxy's presses were. Jumping has no coyote time, no buffer (only holding
Space jumps on landing) and no hang at the top. Jumps and landings have one feedback channel.

**Flow (0.86).** The skill gap is there: the novice gets nowhere (0.00) and the others do
(0.56-0.59), so the game neither plays itself nor locks good play out. But reaction speed hardly
matters: the human proxy scores as well as the oracle, because fights end in about a second and
little in them needs a reaction. And outcomes swing too much (sd 0.48 against a target under
0.25): runs either do not die or die three times, mostly to arrows and boss-grade brutes.

**Choice (0.84).** No single action dominates (max share 0.53), and the spam bot reaches only 0.66
of the human proxy's W, so spells and the rest matter. The entropy is low (0.36) because the bots
use only the blade and attack spells; the bow, the staff and every alternate attack were never
used, and the alternate attacks are locked behind skills.

**Pacing (0.59).** No exhaustion (T never stays above 0.9), but the tension is a flat line with
rare spikes (CV 1.2-1.6, target 0.25-0.85): long walks, one-second fights. Near-misses are rare
(0.4 a minute against 2-12), because most enemies fight in melee and shots are slow and few.

**Fairness (0.55).**
- Enemy shots hit within 0.45 m of the player's centre; the body is 0.3 m (2.25x the area,
  target 0.5-0.85). Shots that look like misses land.
- Monster blows reach 0.6 m beyond reach plus body.
- Player shots hit within the monster's body plus 0.15 m: generous (about 2x the area for a
  small monster; target 1.05-1.35), which helps the player but makes aiming matter little.
- Telegraphs: most wind-ups are 0.35-1.1 s (good). Wisps and shamans fire their volleys with no
  wind-up at all, only the sound at the same moment; the mimic's 0.25 s and the spider's 0.30 s
  are at or under the 0.3 s floor.
- Contrast: the game is meant to be dark, but its creatures share the dark: a dragon's wings and
  neck measure 1.0:1 against the cave rock, its pale belly 1.8:1 (target 3.5:1).
- Screen shake (dragons only) moves the camera at most 0.18 m: fine.

**Loop (0.74).** On the desktop the world is playable 1.5-2.4 s after launch (the browser version
first loads 55 MB). Death always lasts 3.0 s before the respawn, with nothing to skip it (target
1 s). Micro-rewards come every second or two; level-ups every few minutes.

## 4. Improvements

Ordered by what they do for the score and for players. Each names the place in the code, the
change and the number it should move. Effort: S (hours), M (a day), L (days).

### P1: feel and fairness (the foundations) - done 2026-11-21, see section 5

1. **An attack input buffer** (S). `combat.c`, `update_player`: remember a press for 120 ms;
   when the cooldown ends inside that window, attack. The same for spells (`spell_cast`) and
   throws. Target: dropped presses from 39% to under 5% (M_feel +0.25).
2. **Coyote time and a jump buffer** (S). `player.c`: allow a jump up to 100 ms after leaving the
   ground; a press up to 120 ms before landing jumps on landing. Track `ground_t` and
   `jump_press_t` on the Player.
3. **A hang at the top of a jump** (S). `player.c`: gravity x0.6 while |vy| < 1.5 and jump is held.
   With 2 and 3, motion scores 1.0 (M_feel +0.12).
4. **Shrink the box enemy shots hit** (S). `combat.c`, the enemy shot against player test: 0.45 m
   to 0.25 m half-width (0.69x the body's area). Keep the blast radius of explosions as it is.
   Expected: fewer arrow deaths (4 of 13 now), lower outcome variance.
5. **Give wisps and shamans a wind-up** (S). `monsters.c` (wisp ~line 1495, shaman ~1165): enter
   `MS_WINDUP` for 0.35 s with a growing glow at the hand or orb before `monster_volley`, as the
   skeleton's bow already does. Raise the mimic to 0.35 s (its lid lifting is the tell) and the
   spider and the knight's second blow to 0.35 s. Telegraph score 0.85 to 1.0.
6. **Trim the monsters' extra reach** (S). `monsters.c`, `windup_release` and the attack start:
   +0.6 m to +0.25 m. A blow you stepped out of no longer lands.
7. **Make creatures stand out of the dark** (M). In the model shader or `batch3d`: a rim light (an
   edge term on the creatures only, in the colour of their glow) and a floor on their lighting of
   about 0.25; dragons' wings and necks get a lighter membrane or edge tint. Target: 3.5:1 against
   the stone round them, checked with the luminance script used here on `--demo dragonfight` and
   `--demo combat`. The gloom stays in the rooms, not on the enemies.

### P2: impact, pacing and flow - done 2026-11-22, see section 6

8. **Hitstop on heavy blows** (S). A short pause of the simulation (40-60 ms) on a kill, a
   critical hit, a boss's phase and the player being hit hard; `game_update` skips monster and
   projectile updates while `g->hitstop > 0`. High-impact channels 3.1 to 4.
9. **A camera impulse when hurt and on heavy hits** (S). Reuse `ps->shake` (now dragons only):
   0.15-0.3 on taking damage over 10% of health, 0.1 on the player's own heavy blows. A landing
   dip of the eye after falls (`eye_offset`). Jump and landing get dust particles.
10. **A hit marker** (S). The crosshair flashes on a hit (white) and a kill (gold); optional damage
    numbers for crits. Gives swings and shots a third channel (core 2.5 to 3+).
11. **Longer, readable fights** (M). Fights end in about a second, so reactions hardly matter
    (human proxy = oracle). Raise monster health about 40% in tiers 0-2 and lower the player's
    early damage slightly, and give each melee family one telegraphed area attack (a brute's slam,
    a horned charge across the room) that must be stepped out of. Target: average fight 3-5 s,
    tension CV under 0.85, and the oracle clearly ahead of the human proxy (a skill gap in play,
    not only between novice and expert).
12. **More shots worth dodging** (M). More ranged enemies in mixed packs, shots a little faster
    and visible (a trail), so dodging is part of fighting: near-misses from 0.4 to 2+ a minute.
    Pair with 4 and 5 so it stays fair.
13. **A wave rhythm** (M). Encounters in a chunk come in waves: a pack, a breather of 5-8 s with
    loot to pick up, then the elite or the rest. In the spawn of packs (`monsters.c`, the pack
    placement) stagger the members' waking by 3-6 s. Target: tension CV 0.4-0.8.
14. **Steadier outcomes** (M). Boss-grade brutes in tier 0 killed three times: keep
    bosses out of tier 0 for the first 20 minutes; warn when entering a chunk whose danger is
    3+ above your level (the tracker line turns red, a banner); after a death, one line on what
    killed you. Bots should also drink potions sooner (two arrow deaths with five in the bag)
    and a player-facing hint on low health ("H: drink") helps people the same way. Target: sd of
    W under 0.3.
15. **A shorter death** (S). `game.c`, `update_player`: allow respawn on a key press after 1.0 s
    (the fade can go on behind it); keep 3 s as the default without a press. Retry 3.0 s to 1.0 s
    (M_loop +0.2).

### P3: choice and polish

16. **Weapons that want switching** (M). Make the bow and staff the better answer to something:
    flying and ranged enemies take more from arrows; armoured ones (knights, golems) from blunt
    blades or spells; a quick-swap key that keeps the swing timer. Unlock one alternate attack per
    weapon at the start (a weaker whirl, a power shot, a small nova) and let skills improve it,
    rather than hiding them behind skills. Target: H_norm 0.36 to 0.6+, every weapon used.
17. **Teach the bots what players do** (S). The bot never switches weapons nor uses alternate
    attacks, so the choice numbers undercount; give styles a weapon-switch and alternate-attack
    policy so the next audit measures the game, not the bot.
18. **Less text at the top of the screen** (S). Two tracker lines plus a hint line in the middle
    sit over the view; hide "(Y: ANOTHER LINE)" after the first few minutes and fade hints after
    reading.
19. **Browser time to play** (M). 55 MB of sound and music load before play; load the music after
    the first frame and compress sounds (the open known gap): time to first action on a normal
    connection under 3 s.
20. **Measure on a real GPU** (S). Run `FC_TIMING=1` on a machine with a GPU (and the browser
    build) to confirm frame times under 16.6 ms; the container here renders in software.

### Expected result

With P1 and P2: feel ~0.92, flow ~0.92, fairness ~0.82, choice ~0.88, pacing ~0.74, loop ~0.9,
a composite of about 83 (from 63). P3 adds choice and polish on top.

## 5. After P1 (openspec change improve-feel-and-fairness)

All seven P1 items are in (2026-11-21). The same personas and seeds, 30 minutes each, after:

| | Before | After |
|---|---|---|
| Deaths (human proxy, oracle, spam: 15 runs) | 13 | 8 |
| ... by skeleton arrows | 4 | 0 |
| Human proxy: W (sd) | 0.59 (0.48) | 0.54 (0.29) |
| Oracle: W (sd) | 0.56 (0.27) | 0.62 (0.24) |
| Spam: W | 0.39 | 0.56 |
| Near-misses a minute (human proxy) | 0.39 | 0.53 |
| Attack presses lost, any time in the cooldown | 39% | 12% (20-minute runs, 4 seeds) |
| Attack presses lost within 100 ms of readiness | - | 0.5% (1 of 189) |
| Jump: coyote time, buffer, hang | none | checked in test_worldgen (40 frames tapped, 44 held) |
| Dragon against the cave rock (median of its pixels) | 1.0-1.8:1 | 3.7:1 (darkest tenth 2.9:1) |
| Slowest wind-up | 0 ms (wisps, shamans) | 350 ms |

The first after-run counted 31% lost presses: the counter credited an attack to the previous
window's waiting press. It was fixed, and the press counted near readiness was added (the
skill's own measure); the 12% are presses made more than about 320 ms before the weapon is ready,
which a 120 ms buffer is not meant to save.

```text
=== GAME FUN AUDIT REPORT (after P1) ===
- Kinesthetics & Feel (M_feel):   [0.99] | Latency: 1 frame, DropRate: 0.5% near readiness (12% any time), Channels: 2.5 / 3.1
- Flow & Skill Gap (M_flow):      [0.95] | Win/Surv: Rand=0%, Human=54%, Opt=62%   (sigma Human 0.29)
- Strategic Entropy (M_choice):   [0.50] | H_norm=0.35, max p=0.57, SpamBot Ratio=1.04
- Pacing & Tension (M_pace):      [0.42] | Tension CV=2.03 (human), Near-Misses=0.5/min
- Fairness & Clarity (M_fair):    [0.90] | Hurtbox Ratio=0.69, Telegraph>=350ms, Contrast 3.7:1 (dragon)
- Loop Friction (M_loop):         [0.74] | TTFA=1.5-2.4s, Retry=3.0s

COMPOSITE FUN SCORE: 69.4 / 100   (from 63.4)
```

Two pillars fell, and both follow from fairer defence:
- **The spam bot now does as well as the human proxy** (ratio 1.04 against 0.66). Arrows that
  miss and blows one can step out of help a bot that only swings most. Fights are still won by
  hitting; using spells and the rest buys little. P2 11 (longer fights with telegraphed area
  attacks) and P3 16 (weapons with roles, alternate attacks from the start) are the remedy.
- **Tension is flatter** (CV 2.03 for the human proxy): fewer hits taken means more calm. P2 11-13
  (longer fights, more shots worth dodging, waves) give it its peaks back.

The five-run batches are small: W moves by 0.05-0.1 between batches from chance alone (the human
proxy's W fell from 0.59 to 0.54 while it died less), so the trends above, not the second
decimal, are the finding.

## 6. After P2 (openspec change add-impact-and-pacing)

All eight P2 items are in (2026-11-22). The same personas and seeds, 30 minutes each, after P1
and after P2:

| | After P1 | After P2 |
|---|---|---|
| Human proxy: W (sd) | 0.54 (0.29) | 0.63 (0.36) |
| Oracle: W (sd) | 0.62 (0.24) | 0.72 (0.20) |
| Spam: W | 0.56 | 0.25 |
| Novice: W, deaths/h | 0.00, 6.8 | 0.00, 6.0 |
| Deaths/h: human proxy, oracle, spam | 1.6, 0.8, 0.8 | 1.6, 0.4, 4.4 |
| Average fight (human proxy) | 1.0 s | 1.1 s |
| Share of time in fights (human proxy, oracle) | 16%, 11% | 17%, 23% |
| Tension CV (human proxy) | 2.03 | 1.68 |
| Near-misses a minute (human proxy, oracle) | 0.53, 0.27 | 0.38, 0.51 |
| Attack presses lost within 100 ms of readiness | 0.5% | 0% (0 of 647) |
| Feedback channels: core, high-impact | 2.5, 3.1 | 3.0, 4.6 |
| Retry after death | 3.0 s | 1.0 s on a press |

Channels now: swing 3 (animation, sound, a marker on a hit), jump and landing 3 (sound, dust, the
eye's dip), hit on a monster 5 (adds the marker, hitstop on big blows), kill 6 (adds hitstop, a
camera kick, the gold marker), hurt 5 (adds the camera and hitstop on big hits).

What killed the bots (16 deaths, 11 of them the spam bot's): boss-grade brutes 9 (grade 2, deeper
than tier 0), projectiles 3, a slime, a knight, a brute, a blast.

```text
=== GAME FUN AUDIT REPORT (after P2) ===
- Kinesthetics & Feel (M_feel):   [1.00] | Latency: 1 frame, DropRate: 0% near readiness, Channels: 3.0 / 4.6
- Flow & Skill Gap (M_flow):      [0.92] | Win/Surv: Rand=0%, Human=63%, Opt=72%   (sigma Human 0.36)
- Strategic Entropy (M_choice):   [0.85] | H_norm=0.35, max p=0.58, SpamBot Ratio=0.40
- Pacing & Tension (M_pace):      [0.48] | Tension CV=1.68 (human), Near-Misses=0.4/min
- Fairness & Clarity (M_fair):    [0.90] | Hurtbox Ratio=0.69, Telegraph>=350ms (heavy blow 900ms + ring), Contrast 3.7:1
- Loop Friction (M_loop):         [0.94] | TTFA=1.5-2.4s, Retry=1.0s

COMPOSITE FUN SCORE: 81.5 / 100   (from 69.4; 63.4 at the start)
```

What worked:
- **Impact.** Every high-impact event has four channels or more, and the hitstop never stops the
  player's own view or shots.
- **Choice is back.** The spam bot, which only swings, now loses to the human proxy by more than
  half (ratio 0.40): heavy blows and the shooters in packs punish standing in place.
- **The expert pulls ahead.** The oracle beats the human proxy by 0.09 and dies a quarter as often.
- **Loop.** Retry after a death is 1 s on a press.

What did not, honestly:
- **Fights are not longer.** The average fight is still about 1.1 s (target 3-5 s), although
  monsters in tiers 0-2 have 40% more health. The fight counter measures stretches of continuous
  combat, and these break up as packs come in waves, so part of this may be the metric; the time
  spent fighting did rise (the oracle's from 11% to 23%). Either way pacing is still the weakest
  pillar: tension CV 1.68 against the 0.25-0.85 target, and near-misses 0.4-0.5 a minute
  against 2+.
- **Outcomes still swing** (sd of the human proxy's W 0.36, target under 0.3). Boss-grade
  brutes are now the main killer; the early-boss rule covers tier 0 only, and these died deeper.
- **The spam bot now dies 4.4 times an hour.** That is the intended punishment for button
  mashing, but a new player who mashes will feel it too; the heavy-blow ring and the danger
  warning are what tells them why.

Next: make brute bosses' slam read like the heavy blow (a ring), measure fight length per
encounter rather than per combat stretch, and P3 16 (weapons with roles) for pacing and choice.

## 7. Running it again

```bash
# one persona run (30 minutes of play, about 3 minutes of wall time)
build/fortress_crawl --seed 1337 --fresh --mute --bot explorer --bot-react 200 --bot-minutes 30 \
  --bot-log human_1337.log --lowres 320 180 --window 320 180
```

```bash
# time to play and frame times
FC_TIMING=1 build/fortress_crawl --seed 1337 --fresh --mute --shot /tmp/x.bmp --shot-time 16
```

Personas: `--bot novice`, `--bot explorer --bot-react 200` (the human proxy), `--bot explorer`
(the oracle), `--bot spam --bot-react 200`. The `FUN` and `SUMMARY` lines at the end of each log
hold the numbers; W and the pillar scores are computed as described in section 1.
