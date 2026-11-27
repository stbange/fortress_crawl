## ADDED Requirements

### Requirement: Remapping
Every action of play SHALL be given any key or mouse button (two each) and any gamepad button or trigger in the settings; an input another action has SHALL be offered as a swap; the keys and the buttons SHALL reset apart; sprint and crouch SHALL be held or toggled by choice; menus SHALL keep fixed keys so a binding can always be undone.

#### Scenario: Jump on another key
- **WHEN** a player gives jump keypad 0
- **THEN** keypad 0 jumps and the help says so

#### Scenario: A clash
- **WHEN** a player gives jump the health potion's key
- **THEN** they are asked whether to swap the two

### Requirement: Prompts follow the device
The help, the tutorial and the hints SHALL name the inputs of the device the player used last (keyboard and mouse, gamepad or touch), changing as soon as they use another.

#### Scenario: Picking up a pad
- **WHEN** a keyboard player presses a gamepad button and opens the help
- **THEN** its controls page names the gamepad's buttons

## MODIFIED Requirements

### Requirement: Gamepad
A gamepad SHALL play the whole game and every screen round it: by default sticks to move and look, RT and LT to attack and the alternate attack, LB and RB for weapons, X to cast, the D-pad for spells and potions, A and B to jump and crouch, the left stick click to sprint until stopping, the right stick click for the Golden Thread, Y for the menu and BACK for the map; bows and staves SHALL get a little aim assist.

#### Scenario: Aim assist
- **WHEN** a gamepad player shoots near a monster
- **THEN** the arrow bends a little toward it

#### Scenario: Every screen
- **WHEN** a player with only a gamepad starts the game
- **THEN** they can reach and change every setting, start a world and quit with it
