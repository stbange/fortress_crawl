# settings Specification

## Purpose
The settings a player can change, where they are changed and how they are kept between sessions.

## Requirements

### Requirement: Settings menu
A SETTINGS screen SHALL be reached from the start screen and the pause menu, with graphics (resolution, window mode, v-sync, frame cap, brightness, dithering, vertex snap, torch), audio (master, effects and music volume, mute), controls (mouse and stick sensitivity, invert X and Y, vibration) and interface (the line at the top) settings, and a reset to defaults that asks first; a change SHALL take effect at once.

#### Scenario: Music down
- **WHEN** a player sets MUSIC to 30% in the pause menu
- **THEN** the music is quieter at once and the effects are not

#### Scenario: Browser
- **WHEN** the settings are opened in the browser
- **THEN** v-sync, frame cap and window mode are greyed out

### Requirement: Saved settings
The settings SHALL be kept between sessions on the device (a file on the desktop, localStorage in the browser) as named values, skipping names it does not know and clamping values out of range; values given on the command line SHALL win for that run; test, screenshot and bot runs SHALL neither read nor write them.

#### Scenario: Next session
- **WHEN** a player sets brightness to 130% and starts the game again
- **THEN** the brightness is 130%

#### Scenario: A newer file
- **WHEN** the file has a setting this build does not know
- **THEN** it is skipped and the rest load
