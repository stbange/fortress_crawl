## ADDED Requirements

### Requirement: Towns in the fortress
A town SHALL always stand about 200 m east of the gate, and more SHALL stand farther out, each people at its own depths.

#### Scenario: The first town
- **WHEN** a new world begins
- **THEN** a town lies about 200 m east of the gate on the gate's layer

### Requirement: Safe ground
No monster SHALL be born in a town, nothing there SHALL be dug or blasted, and its guards SHALL shoot hostile monsters that come in.

#### Scenario: Followed in
- **WHEN** a monster chases a player into a town
- **THEN** the guards shoot it

### Requirement: Talking
Striking at someone of a town face to face SHALL talk to them instead of hitting them.

#### Scenario: The elder
- **WHEN** a player strikes facing the elder
- **THEN** the elder greets them or speaks of the town's task

### Requirement: The inn
The innkeeper SHALL give a night's rest for 15 gold plus 2 per danger level (free when honoured): everything healed, burns, poison and cold gone, well fed, the respawn set there, the game saved, and good company (15% more XP and twice the healing for 15 minutes).

#### Scenario: A night at the inn
- **WHEN** a player with enough gold talks to the innkeeper
- **THEN** they are healed, the game is saved and they will wake there

### Requirement: The healer
The healer SHALL cure wounds, burns, poison, cold and the after-effects of elixirs for 10 gold plus 1 per danger level, free for friends.

#### Scenario: Healed
- **WHEN** a hurt player with the price talks to the healer
- **THEN** they are whole and the price is paid

### Requirement: Market discounts
The market SHALL sell the people's own wares, 20% cheaper for friends and 35% for the honoured.

#### Scenario: A friend's price
- **WHEN** a friend of the town buys at its market
- **THEN** the price is 20% lower

### Requirement: Tasks
The elder SHALL ask a task (drive off 5 to 8 beasts, bring 2 to 4 goods the people use, find someone lost, or kill a beast in its lair); done, it SHALL pay gold, a gift of the people's craft and reputation (1, or 2 for the lost and the beast).

#### Scenario: A task done
- **WHEN** a player returns to the elder with the task done
- **THEN** they receive gold, the gift and reputation

### Requirement: Reputation
Each town SHALL remember the player's standing: stranger (0), guest (1-2), friend (3-5), honoured (6-9), kin (10+), saved with the world.

#### Scenario: Standing rises
- **WHEN** a task raises a town's reputation past 3
- **THEN** the town calls the player friend
