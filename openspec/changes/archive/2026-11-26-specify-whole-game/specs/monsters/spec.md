## ADDED Requirements

### Requirement: Twenty kinds
There SHALL be twenty kinds of monster, each with its own look and way of fighting: crawlers swarm, brutes wind up heavy blows, wisps and skeletons shoot, horned beasts charge, slimes hop and split, bats dive, spiders leap, wraiths vanish and reappear behind, golems stomp, shamans heal their allies, bloaters burst, eyes charge a beam one can step out of, knights block from the front, worms burrow and burst up, mimics are chests until one is next to them, necromancers raise the dead, harpies circle and dive, hounds circle and lunge from the side, trolls throw boulders. Which kinds live where SHALL depend on the stratum, the tougher ones only farther out.

#### Scenario: A knight
- **WHEN** a player strikes a knight from the front
- **THEN** the shield takes the blow, and from the side it lands

### Requirement: Packs
Most chunks SHALL hold a pack, structures two, and bats SHALL come in swarms.

#### Scenario: A chunk
- **WHEN** a player enters an ordinary chunk
- **THEN** a pack sleeps or roams somewhere in it

### Requirement: Grades
Every kind SHALL come as a normal monster, an elite (bigger, armoured shoulders, spines, burning eyes, tougher, better loot) or a boss (twice the size, a crown, runes, a title and a health bar, harder below half health, always a good weapon); bosses SHALL hold great halls and caverns now and then, more often farther out.

#### Scenario: A boss
- **WHEN** a player meets a boss
- **THEN** its title and health bar show

### Requirement: Look of the place
A monster's colours SHALL come from where it lives: its body from the local rock, armour and bone from the local metal, eyes and markings from the region's element or the torch light.

#### Scenario: In the north
- **WHEN** a crawler lives in the glacial dungeon
- **THEN** its markings are frost-coloured

### Requirement: Noticing
A sleeping monster SHALL wake when a player comes within 7 m, or within 24 m in its line of sight (a mimic only right beside it); thick smoke or steam SHALL hide a player beyond 3.5 m and invisibility unless bumped into; cooking in the dungeon SHALL wake what lives within 25 m; a woken monster SHALL wake its pack, and SHALL give up and sleep again beyond 60 m. (Dragons wake by their own rule: dragons spec.)

#### Scenario: Out of sight
- **WHEN** a player passes a sleeping pack 15 m away behind a wall
- **THEN** it stays asleep

#### Scenario: In view
- **WHEN** the same pack can see the player 20 m away
- **THEN** it wakes and hunts
