# hud Specification

## Purpose
What the screen shows during play.

## Requirements

### Requirement: The HUD
During play the screen SHALL show health and mana bars, level, potions and gold, the weapon and spell in hand, the line at the top, banners, the crosshair, the meal, elixir and bath working with their time left, name plates of named enemies and a boss's health bar; menus and the map SHALL cover it.

#### Scenario: A boss
- **WHEN** a boss is near
- **THEN** its health bar and title show

### Requirement: Hurt
Being hit SHALL show a red vignette and play a sound (and rumble a gamepad).

#### Scenario: A blow
- **WHEN** a player is struck
- **THEN** the screen's edges flash red
