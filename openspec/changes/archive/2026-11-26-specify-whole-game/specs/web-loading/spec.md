## ADDED Requirements

### Requirement: The browser version
The same game SHALL run in the browser (WebGL 2, one thread, chunk generation in time slices between frames); a click SHALL lock the mouse; on touch devices it SHALL try fullscreen and landscape and use a smaller view distance; ?touch, ?seed= and ?args= SHALL show the touch controls, skip the start screen and pass command line options.

#### Scenario: A seed in the link
- **WHEN** the page is opened with ?seed=dragon
- **THEN** the world dragon starts without the start screen
