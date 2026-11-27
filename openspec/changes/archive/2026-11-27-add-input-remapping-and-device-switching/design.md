# Design

## Decisions
- **Defaults are the old layout**, so nothing changes until a player changes it; the bots press the
  game's flags directly and play the same.
- **One reading per device**: game.c's keyboard path dispatches the actions bound to a key or mouse
  input (held ones follow it); main.c's movement reads the actions' keys; read_pad builds a mask of
  the actions held on the pad each frame. The pad's context rules stay (with the pickaxe out, the
  spell choices pick what to throw and the mana potion lights a campfire).
- **Capture goes first**: while the remap screen waits for an input, every event goes to it before
  the menus, and the input just taken does not move the menus for 0.3 s.
