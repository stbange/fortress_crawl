# Design

## Decisions
- **Shake decays with the player**, at 1.5 a second, the rate the dragons' tick used.
- **No save version change** for the loader fix: the files were always written correctly.
- A third such call in world generation (`gen_chunk.c`, a random draw inside MINI) is left: it is
  deterministic, and changing it would change every world.
