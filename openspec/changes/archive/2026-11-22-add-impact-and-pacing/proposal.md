# Change: add-impact-and-pacing

## Why
After P1 the fun audit scored 69: fights ended in about a second (reactions hardly mattered), tension was flat with rare spikes, hits had 2.5 feedback channels and big events 3.1, a few runs died to boss-grade brutes near the gate, and every death cost 3 s before play went on.

## What Changes
- Hitstop: monsters and shots pause 40-60 ms on kills, heavy blows and heavy hurts.
- The camera kicks when the player is hurt hard and on their own heavy blows; the eye dips on a hard landing; dust on jumps and landings.
- The crosshair flashes on a hit and glows gold on a kill.
- Fights last longer: monsters in tiers 0-2 have 40% more health; elites and bosses of the melee kinds sometimes wind up a heavy blow that hits a ring round them, shown on the ground.
- More shots worth dodging: seven packs in ten bring a shooter or healer (was half, and from danger level 1); fire flies faster; enemy shots leave a trail.
- Packs come in waves: beyond the first two, members wait 3-6 s before joining unless struck.
- Steadier outcomes: no boss holds a hall in tier 0 in the first 20 minutes of play; a warning on entering a place 3 levels above the player; one line on what killed them; a hint to drink at low health; bots drink sooner.
- Respawn on a press of attack or jump after 1 s (3 s without a press).

## Impact
- Affected specs: combat-feedback, death-and-respawn, encounters, guidance, playtest-bots
- Affected code and docs: src/game/game.c, game.h, combat.c, monsters.c, monster_draw.c, player.c, sfx.c, dragon.c, bot.c; docs/FUN_AUDIT.md.
