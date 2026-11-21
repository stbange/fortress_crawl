# old-king Specification

## Purpose
The main storyline: the Old King of each world, the ten steps of the way to him, the throne, the endings and the heirs.

## Requirements

### Requirement: One king per world
Each world SHALL have one Old King of ten kinds (the Tyrant, the Necromancer, the Old Mage, the Dragon-King, the Hollow King, the Clockwork King, the Beast-King, the Priest-King of the Sealed, the Alchemist-King, the King-in-the-Stone), chosen by the seed, whose kind and name are hidden until steps reveal them.

#### Scenario: Hidden
- **WHEN** a world begins
- **THEN** the line at the top calls him the Old King, not by his kind or name

### Requirement: The way
The way SHALL have ten steps: three to find out about him (any order), four to gather the means (any order, open when the first three are done), step 8, step 9, and the throne. It SHALL be a pure function of the seed and the joined realms, and every way SHALL include a dragon step, a town step and a Sanctum step.

#### Scenario: Order
- **WHEN** steps 1-3 are not all done
- **THEN** no step of 4-7 can be completed

### Requirement: The steps
Each step SHALL be completed by its deed: the elder of the nearest town (herald); the witness found (witness); his stele read (true name); a night in the Sanctum's bed (vision); his dragon slain (dragon); a thing taken from a hoard (hoard); a town's friendship (allegiance); a titan heart, 3 essences and 2 steel at the Sanctum's anvil (forge); dragon's blood and an orchid to a healer (brew); a realm's ruler beaten (realm); a relic from its guards (relic); his lieutenant killed (lieutenant); a seal broken (seal); the wyrm nearest his throne slain (wyrm).

#### Scenario: A step done
- **WHEN** a player completes a step
- **THEN** a banner shows the count of ten, the text is told, a lore page is added and the game is saved

### Requirement: The throne hall
Each world SHALL have a throne hall in a fixed cell a few super-cells from the gate, above or below by his kind, with a dais, a throne and decor of his kind; when step 10 opens and a player comes within 70 m, the king SHALL wait on it.

#### Scenario: The king waits
- **WHEN** step 10 is open and a player enters the hall
- **THEN** the king sits on his throne

### Requirement: Settling with the king
The king SHALL not listen standing. If the player knows his true name or holds his artefact, he SHALL falter at half his life and again at a fifth, kneeling for 12 s; while he falters, a strike SHALL speak his name (the word ending), a crouched strike SHALL raise the artefact (the artefact ending); a blow from afar may finish him (the sword). Without name or artefact he does not falter.

#### Scenario: Words before the falter
- **WHEN** a player strikes him on the throne with his name known
- **THEN** he speaks and rises, and the game is not settled

#### Scenario: On his knees
- **WHEN** he falls to half his life and the player knows his name
- **THEN** he kneels for 12 s
- **AND** a strike then gives the word ending

### Requirement: The fight
In the fight, a town's allegiance SHALL bring four of its soldiers; the king SHALL call his servants at two thirds and one third of his life; the Necromancer SHALL take almost no harm without his phylactery; the thing from the forge SHALL make blows hurt him 40% more.

#### Scenario: Allies
- **WHEN** the king rises and the allegiance step is done
- **THEN** four soldiers of that town fight beside the player

### Requirement: Winning
Any ending SHALL win the game: the ending's text, his servants gone, the credits, then the world to explore on; the Astral Shore SHALL open (only a settled king opens it), the Dream of the Sleeper after a merciful ending, and the towns SHALL celebrate.

#### Scenario: Explore on
- **WHEN** the credits have rolled for 6 s and the player strikes
- **THEN** play goes on in the same world

### Requirement: Saved
The steps done, what is known of him, the ending, the heir's generation, the kings settled and whether one was spared SHALL be saved with the world.

#### Scenario: Reloaded
- **WHEN** a world with an heir on the throne is loaded
- **THEN** the heir's way and its steps done are the same

### Requirement: Ending rewards
Each ending SHALL give its own reward: the sword his crown (a relic), his legendary blade, a hoard of gold and rich drops, a titan heart and 3 keystones; the word his knights sworn to every town (two more guards each) and his treasury (gold, 2 keystones); the artefact every town's reputation +2, new towns greeting the player as a guest, a keystone and a rich drop.

#### Scenario: Freed
- **WHEN** the artefact ending is chosen
- **THEN** every known town's reputation rises by 2

### Requirement: The chronicle counts
When the chronicle of the player's motive ends (its finale with the one who opened the seal), step 8 of the way SHALL be completed: the opener served the king.

#### Scenario: Finale first
- **WHEN** the chronicle's finale is done while step 8 is open
- **THEN** step 8 is completed with the opener's papers as the text

### Requirement: Raids
Once steps 1-3 are done, his servants SHALL raid the town a player stands in about once in 15 minutes; driven off, the town SHALL give 1 reputation; if the player leaves, the raiders are gone without thanks.

#### Scenario: Driven off
- **WHEN** the last raider of a raid dies with the player in town
- **THEN** the town's reputation rises by 1

### Requirement: His servants close in
His servants, raiders and lieutenants SHALL move 1.7 times as fast when more than 6 m from the player; his lieutenants SHALL step through the dark to the player's side every 7 s when 9-40 m away.

#### Scenario: An archer at range
- **WHEN** a lieutenant is 20 m from a player
- **THEN** within 7 s it appears beside them

### Requirement: The heir
A quarter of an hour of play after a king is settled, an heir of another kind (never the one before) SHALL take the same throne with a new way of ten steps: places 8 chunks farther out per heir (up to three), any town's elder as herald, the lairs on his way retaken by new dragons, allegiance needing honour (6), the king 6 levels stronger per heir; sworn knights and freed towns stay. Heirs go on to the ninth.

#### Scenario: An heir
- **WHEN** 15 minutes pass after a king is settled
- **THEN** word comes that an heir took the throne and the way begins again
