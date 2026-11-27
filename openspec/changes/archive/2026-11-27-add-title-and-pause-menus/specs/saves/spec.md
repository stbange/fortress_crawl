## MODIFIED Requirements

### Requirement: When it saves
The game SHALL save every 30 seconds, when a shrine is attuned, at an inn or bed, when saving and quitting (to the start screen or out of the game), when quitting, and in the browser when the tab is hidden or closed.

#### Scenario: Tab closed
- **WHEN** a browser player closes the tab
- **THEN** the world is saved

#### Scenario: Save and quit
- **WHEN** a player chooses SAVE AND QUIT TO TITLE
- **THEN** the world is saved before the start screen shows
