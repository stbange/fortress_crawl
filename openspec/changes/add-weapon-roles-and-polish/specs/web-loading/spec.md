## ADDED Requirements

### Requirement: Packed sounds
The browser build SHALL pack its sounds as IMA ADPCM WAV when ffmpeg is present at build time (unless FC_WEB_RAW_SFX=1 asks for them as they are), and SHALL not pack the music, which it serves as ADPCM WAV in music/.

#### Scenario: The package
- **WHEN** web/build.sh runs with ffmpeg
- **THEN** index.data holds about 6 MB of sounds and music/ holds the pieces

### Requirement: Music after the first frame
The browser SHALL fetch the music pieces after the first frame, one after another and the gate's first, and play each once it has arrived.

#### Scenario: Starting
- **WHEN** the page opens
- **THEN** the world can be played before the music has arrived

### Requirement: Browser timing
The browser build SHALL log its TIMING lines (first frame, playable, ten seconds of frame times) to the console without being asked.

#### Scenario: A GPU check
- **WHEN** the page has been played for ten seconds
- **THEN** the console holds the mean and worst frame times
