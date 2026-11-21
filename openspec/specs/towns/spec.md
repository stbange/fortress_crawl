# towns Specification

## Purpose
Towns of the friendly peoples in the fortress: safe ground, trade, healing, rest, company, tasks, reputation and what a town gives back.

## Requirements

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
The healer SHALL cure wounds, burns, poison, cold and the after-effects of elixirs for 10 gold plus 1 per danger level, half that for friends, free for the honoured, who also get a blessing that takes the next blows.

#### Scenario: A friend healed
- **WHEN** a friend of the town is healed
- **THEN** they pay half the price

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

### Requirement: Special wares by trust
The market SHALL sell its people's own wares at their price, and three special wares: one for friends, one for the honoured and one from the caravan the player paid for (moonsilver, epic weapons, keystones, formula pages, titan hearts...), each once a day.

#### Scenario: Honoured
- **WHEN** an honoured player opens the market
- **THEN** the friends' and the honoured's special wares are listed

### Requirement: The town's works
Crouching before the elder SHALL give gold to the town's next work: rebuild the burned houses (400 gold and more by depth), arm the guards (900 and more), send a caravan (1600 and more); each SHALL give 2 reputation and be seen (a new family, two more guards, the caravan's ware).

#### Scenario: Arms for the guards
- **WHEN** a player pays for the guards' arms
- **THEN** two more guards walk the walls

### Requirement: Kin
A town that calls the player kin (10+) SHALL send one of its people to live in the Sanctum, who SHALL give a gift from home once a day.

#### Scenario: A gift from home
- **WHEN** a player talks to their kin in the Sanctum for the first time in a day
- **THEN** they receive the people's gift

### Requirement: After the king
After the Old King is settled, the towns SHALL celebrate (lights over the plazas, talk of the king's end); after the word ending every town SHALL have two more guards; after the artefact ending new towns start at guest.

#### Scenario: Celebration
- **WHEN** a player enters a town after the king is settled
- **THEN** fireworks burst over the plaza
