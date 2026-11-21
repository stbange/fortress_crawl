# Change: improve-feel-and-fairness

## Why
The fun audit scored feel 0.61 and fairness 0.55: 39% of attack presses at a human's reaction were lost to the cooldown, jumps had no coyote time, buffer or hang, enemy shots hit a box 2.25 times the body, wisps and shamans fired with no warning, monster blows reached 0.6 m past their arm, and creatures were 1.0-1.8:1 against the stone.

## What Changes
- Input buffer of 120 ms for attacks, spells and throws.
- Coyote time (100 ms), a jump buffer (120 ms) and a hang at the top of a jump.
- Enemy shots hit within 0.25 m of the player's centre (was 0.45).
- Wisps and shamans wind up 0.35 s before a volley; mimic, spider and the knight's second blow at least 0.35 s.
- Monster blows land within reach + body + 0.25 m (was 0.6).
- Creatures get a floor on their light and a rim in their glow colour; checked at 3.5:1 or better on the dragon fight frame.

## Impact
- Affected specs: combat-fairness, creature-visibility, player-input
- Affected code and docs: src/game/combat.c, player.c, player.h, game.c, game.h, spells.c, dig.c, monsters.c, monster_draw.c, dragon.c, render/batch.c, batch.h; docs/FUN_AUDIT.md; the near-miss band in combat.c follows the new box.
