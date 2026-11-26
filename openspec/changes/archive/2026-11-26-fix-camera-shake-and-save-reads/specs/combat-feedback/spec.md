## MODIFIED Requirements

### Requirement: Camera impulse
The view SHALL shake briefly when the player loses more than 10% of their health in one hit (more for bigger hits) and slightly on the player's own kills, and the shake SHALL settle within a second wherever the player is; the eye SHALL dip on landing from a fall faster than 7 m/s.

#### Scenario: A heavy hit
- **WHEN** a brute takes a fifth of the player's health
- **THEN** the view jolts and settles within half a second

#### Scenario: No dragon near
- **WHEN** a player kills a crawler far from any dragon
- **THEN** the view is still again a moment later
