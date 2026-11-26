# dev-tools Specification

## Purpose
Generator checks, demos, screenshots and map exporters for the people making the game.

## Requirements

### Requirement: Generator checks
test_worldgen SEED SHALL check determinism, portal alignment, reachability, meshing, the stories, the Old King's way and his heirs, and SHALL pass on seeds 1337 and 42 before every push.

#### Scenario: A broken layout
- **WHEN** a change leaves a structure unreachable
- **THEN** test_worldgen fails

### Requirement: Screenshots and demos
--shot FILE SHALL wait until the view has loaded, save one frame and exit; --demo NAME SHALL set up a scene or a scripted check (combat, inventory, story, vaults, traps, the Sanctum, food, alchemy, forge, towns, dragons, realms, the king, the armoury, help, the tutorial and more); tools/screenshots.sh SHALL make the README's screenshots again.

#### Scenario: A screenshot
- **WHEN** the game runs with --demo inventory --shot out.bmp
- **THEN** it writes the inventory screen and exits

### Requirement: Map exporters
worldmap SHALL export a seed's places, vertical sections, block maps, vaults and traps, structures and hidden history.

#### Scenario: History
- **WHEN** worldmap history runs for a seed
- **THEN** it prints that world's hidden history
