# Design

## Context
P1 fixed feel and fairness; the fun audit's P2 items address impact (feedback channels), the
length and rhythm of fights, the swing of outcomes and the time a death costs.

## Decisions
- **Hitstop pauses monsters and shots, not the player.** `g->hitstop` (seconds) is set by
  kills (50 ms), blows that take a quarter of a monster's life (40 ms) and hurts over 15% of the
  player's health (60 ms); while it runs, `monsters_update` and the projectiles get dt = 0. The
  player's own view and inputs keep running, so it never feels like lag. It does not stack past
  80 ms.
- **The camera uses the existing shake** (`ps->shake`, decayed in the dragons' tick): 0.12-0.3
  when hurt by over 10% of health, 0.08 on the player's own kills. The landing dip uses the eye
  offset the controller already smooths.
- **Hit markers on the crosshair**, not floating numbers: a white flash 0.12 s on a hit, gold
  0.25 s on a kill. (Damage numbers stay out: the game has no crits to call out yet.)
- **Fight length by health, not by cutting the player's damage**: +40% health for monsters in
  tiers 0-2. The heavy blow is new for the melee kinds that had no area attack: an elite or boss
  winds up 0.9 s with a ring on the ground (its radius, 2.5 m x its size) and hits everyone in it
  for 1.5x; the brute's slam, the golem's and the wurm's blasts and the troll's sweep stay theirs.
- **Waves by waking, not by spawning**: members of a pack beyond the first two wait 3-6 s
  (`m->wave_t`) when woken by the others; a blow wakes them at once. No new spawns, so the
  world's populations are unchanged.
- **The early boss rule uses play time** (20 minutes) and tier 0, where the playtests' deaths
  to boss-grade brutes happened.
- **Respawn on a press**: attack or jump after 1 s; still 3 s without one, so a bot or someone
  who looks away loses nothing.

## Risks
- More health and heavy blows make the game harder; the bots will show it in deaths per hour.
- Hitstop can feel like stutter if overused; the 80 ms cap and the triggers keep it to big moments.
