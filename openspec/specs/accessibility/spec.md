# accessibility Specification

## Purpose
Options so more people can play: less motion and flashing, colours told apart with a colour-vision deficiency, and assists.

## Requirements

### Requirement: Reduced motion and flashing
The settings SHALL offer screen shake off, half or full (the landing's dip included), the hit pause on or off, and reduced flashing, which SHALL dim flashes of light, the red of being hurt and monsters' white flash.

#### Scenario: No shake
- **WHEN** a player with screen shake off takes a heavy blow
- **THEN** the view does not move

### Requirement: Not by colour alone
The settings SHALL offer palettes for protanopia, deuteranopia and tritanopia in which every two elements and every two tiers of loot are at least 20 apart in CIELAB as seen with that deficiency (checked by tools/check_palettes.py); finds SHALL name their tier and element, and a named foe's plate its element, friends and those that yielded marked in words.

#### Scenario: Frost and lightning
- **WHEN** a player with deuteranopia picks the deuteranopia palette
- **THEN** frost and lightning bolts are told apart

#### Scenario: A find
- **WHEN** a magic bow of frost is picked up
- **THEN** the banner says MAGIC and FROST

### Requirement: Assists
The settings SHALL offer damage taken at 75 or 50%, enemy wind-ups (and a dragon's breath) half again as long, and no gold lost on death or on going back where one fell; a world played with any of them SHALL be marked so in its save and the list of worlds, and nothing else SHALL change.

#### Scenario: Half damage
- **WHEN** a player with damage taken at 50% is struck for 56
- **THEN** they lose 28

#### Scenario: Marked
- **WHEN** a world was played with an assist
- **THEN** the list of worlds shows ASSIST beside it
