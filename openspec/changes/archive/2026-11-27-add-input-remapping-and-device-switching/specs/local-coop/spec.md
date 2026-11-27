## MODIFIED Requirements

### Requirement: Characters kept
Each player's character SHALL be saved with the world; a character whose pad disconnects SHALL wait until a pad presses START again, and characters that did not join this time SHALL be kept; alone, a player whose gamepad is unplugged while playing with it SHALL find the game paused until a pad presses a button.

#### Scenario: A lost pad
- **WHEN** player 2's pad disconnects
- **THEN** their character waits until a pad presses START

#### Scenario: Alone
- **WHEN** a solo player's controller is unplugged mid-fight
- **THEN** the game pauses and asks to reconnect it
