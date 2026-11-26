# Design

## Decisions
- **Describe, do not design**: every requirement states what the game does on 2026-11-26. Where
  the README and the code differed (four rarities, the alternate attack locked behind skills,
  eight menu tabs), the code's current behaviour was written down and the README corrected.
- **One requirement per rule a player meets**, with one scenario that can be checked in play or
  by a demo; long lists (spells, monsters) stay in one requirement as their inventory.
- **Developer tools apart** (`dev-tools`), so player-facing specs stay about the game.
- **Existing specs untouched** except `saves` and `web-loading`, which gain what they lacked.

## Gaps left on purpose
The audit's missing items (title and pause menus, settings, accessibility, localization, error
messages) are not here: this change only records what exists; they are proposals of their own.
