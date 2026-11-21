# Design

## Context
The fun audit (docs/FUN_AUDIT.md) found the foundations of feel and fairness below target. These
are small, local changes with large effects on how the game feels and how fair hits look.

## Decisions
- **Buffers are per player and short.** A press is remembered 120 ms; it fires the moment the
  cooldown allows. Holding still repeats as before. Throws buffer through the same timer as their
  key. Spells buffer on their own timer.
- **Jump forgiveness in the player controller.** Coyote time and the buffer live on the Player
  (`coyote`, `jump_buf`, `jump_prev`); the hang reduces gravity to 0.6 while |vy| < 1.5 and jump
  is held. Levitation, vines and swimming are unchanged.
- **Enemy shot box 0.25 m.** 0.25/0.3 squared is 0.69 of the body's area, inside 0.5-0.85. Blasts
  keep their radius. The near-miss band follows (0.25-0.375 m).
- **Wind-ups reuse MS_WINDUP.** Wisps and shamans enter the wind-up state (their eyes turn red and
  sparks gather at the hand) and release the volley in `windup_release`; the shaman's mending of
  others stays instant.
- **Melee release slack 0.25 m**, close to the 0.2 m at which a blow begins, so stepping out during
  the wind-up works.
- **Rim light in the batch, not the shader.** The 3D batch shades by face; a rim term
  (1 - |n.v|)^2 in the creature's glow colour is added per triangle for creatures only
  (`batch3d_set_rim`), and their light floor rises from 0.16 to 0.3. The world's own lighting is
  unchanged, so the rooms stay dark.

## Risks
- Brighter creatures make the game a little easier to read; the gloom of rooms is untouched.
- The buffer could fire an attack the player meant to cancel; 120 ms is short enough not to matter.
