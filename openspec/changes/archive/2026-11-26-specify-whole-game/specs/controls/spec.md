## ADDED Requirements

### Requirement: Keyboard and mouse
Keyboard and mouse SHALL play the whole game: WASD and the mouse to move and look, Space, Shift and Ctrl to jump, sprint and crouch, the mouse buttons to attack and the alternate attack, number keys and the wheel for weapons, and letter keys for spells, potions, food, elixirs, throwing, the Golden Thread, the line at the top, the map and the menu's tabs (see the HELP tab); Esc SHALL release the mouse and a second Esc quit.

#### Scenario: Sprint
- **WHEN** a player holds Shift while walking
- **THEN** they run

### Requirement: Gamepad
A gamepad SHALL play the whole game: sticks to move and look, RT and LT to attack and the alternate attack, LB and RB for weapons, X to cast, the D-pad for spells and potions, A and B to jump and crouch, the left stick click to sprint until stopping, the right stick click for the Golden Thread, Y for the menu and BACK for the map; bows and staves SHALL get a little aim assist.

#### Scenario: Aim assist
- **WHEN** a gamepad player shoots near a monster
- **THEN** the arrow bends a little toward it

### Requirement: Touch
On touch screens the game SHALL show controls: a stick that appears under the thumb on the left (past the ring to sprint), dragging on the right to look, a big attack button (dragging from it aims), the alternate attack, jump, crouch, weapon, cast and next spell, and path, potion, menu and map at the top right; bows and staves SHALL get light aim assist.

#### Scenario: A phone
- **WHEN** the browser version starts on a phone
- **THEN** the touch controls show

### Requirement: Options keys
F2 SHALL cycle the internal resolution, F3 dithering, F4 vertex snapping, F5 full-bright, F6 the player's torch, F7 and F8 invert the mouse, - and = the mouse sensitivity, F11 the debug overlay, F12 save a screenshot (desktop).

#### Scenario: Invert
- **WHEN** a player presses F7
- **THEN** the mouse's vertical look inverts and a banner says so
