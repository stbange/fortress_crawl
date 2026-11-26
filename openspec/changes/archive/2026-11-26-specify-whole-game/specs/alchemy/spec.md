## ADDED Requirements

### Requirement: Reagents
Every kind of monster SHALL leave its reagent now and then, and eight strange things (quicksilver, brimstone, ghost orchid, starlight dew, void salt, mandrake, amber, phoenix feather) SHALL lie in some places.

#### Scenario: Bone dust
- **WHEN** a player kills skeletons
- **THEN** now and then one leaves bone dust

### Requirement: The cauldron
At any shrine or at home the ALCHEMY tab SHALL be a cauldron: ingredients and steps (grind, heat, stir, chill, distill) put in one by one and bottled; only an exact formula SHALL make an elixir, anything else a murky brew that may boil over and says how many steps were right in their place and how many right but elsewhere.

#### Scenario: Almost
- **WHEN** a player bottles a brew with two steps swapped
- **THEN** it is murky and says how many steps were right

### Requirement: Formulae
Formula pages SHALL be rare (every vault hoard, now and then a chest, an elite or a boss); half SHALL be torn, saying what goes in but not how, and a second half SHALL make one whole.

#### Scenario: Two halves
- **WHEN** a player holds both halves of a torn formula
- **THEN** the formula is whole

### Requirement: Elixirs
There SHALL be 27 elixirs, each taking hold 1 to 5 s after drinking and strong for 20 to 30 s; one SHALL work at a time, one drunk within 90 s of the last SHALL work at half and burn; the belt SHALL hold four.

#### Scenario: Too soon
- **WHEN** a player drinks a second elixir 30 s after the first
- **THEN** it works at half and burns them
