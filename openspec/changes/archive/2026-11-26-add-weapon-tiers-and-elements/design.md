# Design

## Context
The models were already built from shaped parts (make-weapons-realistic); the rarity only picked
colours, and elements only a glow and motes.

## Decisions
- **One style struct** (`WStyle`) derived by `weapon_style(item)`: per-tier tables of four
  choices for guard, pommel and blade form, indexed by bits of the look's hash; the element maps
  to an overlay; swift, heavy and keen set bulk and point.
- **The look is a byte on the item**, rolled at generation (`item_roll_look`), so a drop, a forged
  piece and a world's legendary each get their own; 0 (older saves) falls back to a hash of the
  item's other fields, so a weapon looks the same each time it is loaded.
- **Overlays are drawn along a span** each model names (its blade or head), from the edges on a
  blade or all round anything round, scaled to the span so a dagger carries less than a
  greatsword. Frost also lifts the steel's brightness: ice is bright even on blackened steel.
- **Steel is capped** before lighting so white starmetal does not burn out under a torch.

## Risks
- More triangles on magical weapons (crystals, coils); measured in the armoury pages only.
