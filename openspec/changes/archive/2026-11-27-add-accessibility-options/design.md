# Design

## Decisions
- **One knob per effect at its source**: shake scales the camera offset and the landing dip,
  hit pause returns from `game_hitstop`, flashing scales the flash lights where they are summed,
  wind-ups multiply where monsters set them (the drawing's raise uses the same length).
- **Palettes stay recognisable**: an optimiser moved each colour as little as it could (a penalty on
  the distance from the normal colour) until every pair is 20 apart for that deficiency.
- **Assists do not lock anything**: the save's flag is information for the player.
