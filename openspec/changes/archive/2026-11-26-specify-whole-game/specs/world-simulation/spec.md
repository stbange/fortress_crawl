## ADDED Requirements

### Requirement: Breakable blocks
Every block SHALL be made of 4 x 4 x 4 pieces of 25 cm; the pickaxe SHALL chip rock, ore, wood, earth and ice, bombs SHALL blow craters into masonry; blocks left with few pieces SHALL crumble and what is no longer held SHALL fall; vaults, shrines, the Sanctum, stairwells, towns and the outside walls SHALL not break; what is broken SHALL stay broken.

#### Scenario: A tunnel
- **WHEN** a player digs through a rock wall and reloads
- **THEN** the tunnel is still there

### Requirement: Water
Water SHALL flow and find its level, cool lava to rock and put out fires; in deep water one SHALL swim (jump up, crouch down) with 15 s of breath; running water SHALL carry one.

#### Scenario: A broken lake
- **WHEN** a player breaks a lake's wall
- **THEN** it drains into the halls around until level

### Requirement: Fire, smoke and gas
Fire SHALL spread through wood, webs and grass, melt ice and leave ash; thick smoke SHALL hide players from monsters; swamp gas from still water by roots and fungus SHALL explode when fire reaches it; gas SHALL drift out of rooms and up stairwells.

#### Scenario: Gas
- **WHEN** a fire bolt meets swamp gas
- **THEN** it explodes

### Requirement: Elements in the world
Every spell and breath SHALL act on the world by its element: fire sets things alight, frost freezes water into ice one can walk on, lightning runs through water and hurts everyone in it, acid eats stone, oil burns on water.

#### Scenario: A frozen lake
- **WHEN** a frost aura passes over a lake
- **THEN** the water freezes and can be walked on

### Requirement: Heat and cold
Temperature SHALL run through the world: the north is cold (a fire, a fur cloak or a warming draught keep it off), ice melts by fires, water boils by lava, wood catches from heat; being soaked SHALL keep fire off but make lightning twice as bad.

#### Scenario: Soaked
- **WHEN** a soaked player is struck by lightning
- **THEN** it hurts twice as much

### Requirement: Living things
Grass and flowers SHALL spread in light, moss on damp stone, fungus in the dark, vines from wet ceilings; bees SHALL hang hives where flowers bloom; insects SHALL gather where they belong; one SHALL sow seeds and reap what grows.

#### Scenario: Sowing
- **WHEN** a player sows a seed in earth and comes back later
- **THEN** a plant has grown

### Requirement: Weight
Blocks SHALL bear weight and come down when what holds them is cut; big monsters SHALL break through walls to reach a player.

#### Scenario: A cut pillar
- **WHEN** a player digs out the pillar under a ceiling
- **THEN** the ceiling comes down

### Requirement: While away
Areas a player leaves SHALL carry on without them (plants grow, water flows, fires burn out, animals breed and hunt) and be caught up on return; every region SHALL keep its own populations, so hunting a cave empty leaves its hounds hungry.

#### Scenario: Grown over
- **WHEN** a player returns to a hall after an hour away
- **THEN** what grew there has grown on
