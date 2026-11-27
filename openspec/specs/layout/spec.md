# layout Specification

## Purpose
How the screens fit any window, phone or television: aspect ratios, safe areas, text size and touch targets.

## Requirements

### Requirement: Aspect ratios and text size
The game SHALL fill windows from 4:3 to 21:9 and portrait ones without bars, and SHALL offer a text size of 100, 150 or 200% that makes every screen round the game bigger without text running off it.

#### Scenario: Portrait
- **WHEN** the game runs in a tall window
- **THEN** the view grows in height and nothing is cut off

#### Scenario: Large text
- **WHEN** a player sets the text size to 200%
- **THEN** the menus and the HUD are twice as big and every line fits

### Requirement: Safe areas and touch targets
The screens SHALL keep clear of a phone's notch and rounded corners (the browser's safe areas) and of a margin chosen in the settings, and touch controls SHALL be at least 9 mm across.

#### Scenario: A notch
- **WHEN** the browser version runs on a phone with a notch
- **THEN** no button sits under it
