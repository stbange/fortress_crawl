## ADDED Requirements

### Requirement: Start screen
The game SHALL start on a start screen to type a seed (a number or any word) or roll a random one; it SHALL suggest the world played last and show what is saved; New characters (pressed twice) SHALL delete that world's save; --seed SHALL skip it.

#### Scenario: Last world
- **WHEN** a player who played seed 42 starts the game
- **THEN** the start screen suggests 42 and shows its save
