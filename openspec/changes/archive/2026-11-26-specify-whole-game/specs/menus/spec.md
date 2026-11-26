## ADDED Requirements

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
Menus SHALL pause the game when one plays alone and SHALL not in co-op.

#### Scenario: Alone
- **WHEN** a solo player opens the menu during a fight
- **THEN** the monsters stand still
