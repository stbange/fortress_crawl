## ADDED Requirements

### Requirement: Saving indicator
The game SHALL show SAVING in a corner while it saves.

#### Scenario: Autosave
- **WHEN** the 30-second autosave runs
- **THEN** SAVING shows in the top left for a moment

### Requirement: Export and import
A world's save SHALL be exported to a .fcsave file (a file dialog on the desktop, a download in the browser) and imported from one; an import SHALL be checked before it is stored, and replacing a world SHALL ask first.

#### Scenario: To another computer
- **WHEN** a player exports a world and imports the file on another computer
- **THEN** the world is listed there with the same level and continues where it was

#### Scenario: Not a save
- **WHEN** a player imports a file that is not a save
- **THEN** they are told and nothing changes

## MODIFIED Requirements

### Requirement: Where and which
The browser SHALL keep saves in its local storage and the desktop in the user data folder; saves SHALL be versioned and checked (a CRC-32 from version 31), older ones SHALL always load, and a damaged save or one from a newer version SHALL never be written over: it SHALL be kept aside, shown in the list of worlds to export or delete, and the world SHALL start with new characters after asking; --fresh SHALL start without loading.

#### Scenario: An old save
- **WHEN** a save written two versions ago is loaded
- **THEN** it loads

#### Scenario: Damaged
- **WHEN** a save's bytes were damaged and its world is started
- **THEN** the player is asked first, the save is kept aside and nothing of it is used
