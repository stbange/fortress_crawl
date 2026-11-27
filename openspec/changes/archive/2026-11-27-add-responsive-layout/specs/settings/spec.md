## MODIFIED Requirements

### Requirement: Settings menu
A SETTINGS screen SHALL be reached from the start screen and the pause menu, with graphics (resolution, window mode, v-sync, frame cap, brightness, dithering, vertex snap, torch), audio (master, effects and music volume, mute), controls (the keys and buttons of every action, hold or toggle for sprint and crouch, mouse and stick sensitivity, invert X and Y, vibration), interface (text size, screen margin, the line at the top) and accessibility settings, and a reset to defaults that asks first; a change SHALL take effect at once.

#### Scenario: Music down
- **WHEN** a player sets MUSIC to 30% in the pause menu
- **THEN** the music is quieter at once and the effects are not

#### Scenario: Browser
- **WHEN** the settings are opened in the browser
- **THEN** v-sync, frame cap and window mode are greyed out
