# controls Specification

## Purpose
Keyboard and mouse, gamepad and touch.

## Requirements

### Requirement: Keyboard and mouse
Keyboard and mouse SHALL play the whole game: WASD and the mouse to move and look, Space, Shift and Ctrl to jump, sprint and crouch, the mouse buttons to attack and the alternate attack, number keys and the wheel for weapons, and letter keys for spells, potions, food, elixirs, throwing, the Golden Thread, the line at the top, the map and the menu's tabs (see the HELP tab); Esc SHALL close what is open, else open the pause menu and let the mouse go, and SHALL never quit the game by itself.

#### Scenario: Sprint
- **WHEN** a player holds Shift while walking
- **THEN** they run

#### Scenario: Esc
- **WHEN** a player presses Esc with nothing open
- **THEN** the pause menu opens and the cursor is free

### Requirement: Gamepad
A gamepad SHALL play the whole game and every screen round it: by default sticks to move and look, RT and LT to attack and the alternate attack, LB and RB for weapons, X to cast, the D-pad for spells and potions, A and B to jump and crouch, the left stick click to sprint until stopping, the right stick click for the Golden Thread, Y for the menu and BACK for the map; bows and staves SHALL get a little aim assist.

#### Scenario: Aim assist
- **WHEN** a gamepad player shoots near a monster
- **THEN** the arrow bends a little toward it

#### Scenario: Every screen
- **WHEN** a player with only a gamepad starts the game
- **THEN** they can reach and change every setting, start a world and quit with it

### Requirement: Touch
On touch screens the game SHALL show controls: a stick that appears under the thumb on the left (past the ring to sprint), dragging on the right to look, a big attack button (dragging from it aims), the alternate attack, jump, crouch, weapon, cast and next spell, and path, potion, menu and map at the top right; bows and staves SHALL get light aim assist.

#### Scenario: A phone
- **WHEN** the browser version starts on a phone
- **THEN** the touch controls show

### Requirement: Options keys
F2 SHALL cycle the internal resolution, F3 dithering, F4 vertex snapping, F5 full-bright, F6 the player's torch, F7 and F8 invert the mouse, F9 mute, F10 the music, - and = the mouse sensitivity, F11 the debug overlay, F12 save a screenshot (desktop); all but F5, F11 and F12 SHALL change the saved settings.

#### Scenario: Invert
- **WHEN** a player presses F7
- **THEN** the mouse's vertical look inverts, a banner says so, and it is still inverted next session

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
