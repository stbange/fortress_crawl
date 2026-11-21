# Change: add-fun-audit

## Why
To judge and improve how the game feels to play, it was audited with the game fun evaluator: personas, telemetry and six scored pillars.

## What Changes
- Personas: --bot novice, --bot-react MS (a human's reaction for any style), --bot spam.
- Telemetry: the tension index once a second, near-misses of enemy shots, attack presses lost to the cooldown, eight kinds of action use; FC_TIMING for time to play and frame times.
- docs/FUN_AUDIT.md: 63 of 100, and a prioritized list of improvements.

## Impact
- Affected specs: playtest-bots
- Affected code and docs: src/game/bot.c, combat.c, spells.c, dig.c, alchemy.c, game.c, tele.h, main.c; docs/FUN_AUDIT.md.
