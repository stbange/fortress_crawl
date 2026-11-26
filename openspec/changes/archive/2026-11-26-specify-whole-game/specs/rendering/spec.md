## ADDED Requirements

### Requirement: Retro look
The game SHALL render at a low internal resolution (480 x 270, or 320 x 180 or 640 x 360) scaled up without smoothing, with 16 x 16 textures, light quantised into bands, ordered dithering, a vignette and optional vertex snapping.

#### Scenario: Low resolution
- **WHEN** the window is 1920 x 1080
- **THEN** the picture is 480 x 270 scaled up in sharp pixels

### Requirement: Fog and light
Fog SHALL thin in open spaces so giant halls can be seen across and close in in tunnels; torches, braziers, lava and crystals SHALL light the world and emissive things cut through the fog; the game SHALL be dark by design.

#### Scenario: A great hall
- **WHEN** a player enters a great hall
- **THEN** its far end can be seen
