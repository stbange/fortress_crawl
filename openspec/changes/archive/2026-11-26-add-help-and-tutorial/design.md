# Design

## Decisions
- **The tutorial starts at the motive**: `story_pick_motive` is the moment a character is new
  (a loaded one already has a motive), so no save flag is needed; a character saved mid-tutorial
  simply does not see the rest, and the help can run it again.
- **Steps are read from state**, not from input events: distance walked, upward speed, the swing,
  the weapon in hand, the spell cooldown; the Golden Thread and a potion are noted by their code
  paths (`tutorial_note`). So the steps work the same on every device.
- **A moment to read**: a step cannot be done in its first 1.5 s, a done step shows its tick for
  0.9 s, and the first card waits 3 s for the chapter's hook. The story step gives up holding
  after 40 s.
- **Device text** is chosen per player: a gamepad without the keyboard, touch when the touch
  controls are shown, else keyboard and mouse.
