# Change: add-king-bot

## Why
To measure the time to the throne, a bot has to follow the way; doing so found that the Golden Thread did not lead to the king's steps, and that markers could be placed out of reach.

## What Changes
- --bot king: follows the thread to the step it can do, goes home for the vision, builds the forge first, buys steel and orchids, wins friendship with gold, hunts dragons, takes the Gate or the Vestibule rift for the realm step.
- The Golden Thread leads to the step the line follows; a player waypoint leads it first.
- The king's markers lie on the layer's floor.
- Batches show the way's first act takes 10-40 minutes; the bot still stalls at stairs.

## Impact
- Affected specs: guidance, playtest-bots
- Affected code and docs: src/game/bot.c, king.c, quests.c, dragon.c, gate.c; docs/OLD_KING.md.
