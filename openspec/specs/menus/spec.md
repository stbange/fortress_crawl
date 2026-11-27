# menus Specification

## Purpose
The character menu and its tabs, and the inventory.

## Requirements

### Requirement: The character menu
The character menu SHALL have nine tabs: ITEMS, SPELLS, SKILLS, STORY, SHOP, FOOD, ALCHEMY, FORGE and HELP, opened by TAB (the last tab), I, B, L, N, O, U and F1, the Y button on a gamepad and the MENU button on touch; Q/E, [ ] or the shoulder buttons SHALL switch tabs.

#### Scenario: Opening
- **WHEN** a player presses I
- **THEN** the ITEMS tab opens

### Requirement: Inventory
The ITEMS tab SHALL show the three equipped weapons, a bag of 18, the relics, and the selected weapon's name, stats, affixes, powers, material, kills, how it is made and its lore; Enter SHALL equip, Del SHALL scrap.

#### Scenario: Equip
- **WHEN** a player selects a bow in the bag and presses Enter
- **THEN** it is equipped and the old bow goes to the bag

### Requirement: Pause
Esc, START on a gamepad or the II button on touch SHALL open a pause menu with RESUME, SETTINGS, HELP, SAVE AND QUIT TO TITLE and, on the desktop, SAVE AND QUIT GAME; it and every other menu SHALL pause the game when one plays alone and SHALL not in co-op, where the pause menu is that player's own.

#### Scenario: Alone
- **WHEN** a solo player presses Esc during a fight
- **THEN** the pause menu opens and the monsters stand still until RESUME

#### Scenario: Co-op
- **WHEN** one of two players opens the pause menu
- **THEN** it says the world goes on, and the other player keeps playing

#### Scenario: To the title
- **WHEN** a player chooses SAVE AND QUIT TO TITLE
- **THEN** the world is saved, the start screen shows, and CONTINUE brings them back where they were
