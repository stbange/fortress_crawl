## ADDED Requirements

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
The king SHALL be settled by the sword, by his true name, or by the artefact his kind needs: striking him speaks; striking again with his name known gives the word ending, crouched with the artefact the artefact ending; otherwise he rises and fights.

#### Scenario: The word
- **WHEN** a player who read his stele strikes him twice
- **THEN** the word ending is told

### Requirement: The fight
In the fight, a town's allegiance SHALL bring four of its soldiers; the king SHALL call his servants at two thirds and one third of his life; the Necromancer SHALL take almost no harm without his phylactery; the thing from the forge SHALL make blows hurt him 40% more.

#### Scenario: Allies
- **WHEN** the king rises and the allegiance step is done
- **THEN** four soldiers of that town fight beside the player

### Requirement: Winning
Any ending SHALL win the game: the ending's text, his servants gone, the credits, then the world to explore on; the Astral Shore SHALL open, and after a merciful ending the Dream of the Sleeper.

#### Scenario: Explore on
- **WHEN** the credits have rolled for 6 s and the player strikes
- **THEN** play goes on in the same world

### Requirement: Saved
The steps done, what is known of him and the ending SHALL be saved with the world.

#### Scenario: Reloaded
- **WHEN** a world with four steps done is loaded
- **THEN** the same four steps are done
