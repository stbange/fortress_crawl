# Project Context

## Purpose
Fortress Crawl is an endless, procedurally generated first-person dungeon crawler: a castle
built into a mountain, generated without end in every direction, more dangerous the farther from
the gate. The main storyline is the Old King's; the towns, the Sanctum, the realms behind the
Gate, dragons, crafting and the generated chronicle feed it. How the systems fit together is in
`docs/DESIGN.md`.

## Tech Stack
- C11, SDL3, OpenGL 3.3 core; no other runtime dependencies (own GL loader, math, noise).
- CMake; the browser build with Emscripten (`web/build.sh`).
- Sound effects and music made with ElevenLabs (`tools/gen_sfx.mjs`), committed as WAV.

## Project Conventions

### Code Style
- `src/world/` is pure generation (a function of the seed); `src/game/` is play; `src/render/`
  and `src/engine/` draw and run.
- Comments explain the why in plain words; names in the game's own vocabulary.
- Saves are versioned (`SAVE_VERSION` in `src/game/save.c`); older saves always load.

### Architecture Patterns
- Worlds are chunks of 32 m generated on worker threads; megastructures by super-cell (6 chunks).
- Generated content (histories, the Old King's way, towns, lairs) is a pure function of the seed;
  only what players did is saved.
- Game state lives in one `Game`; systems update it per fixed step (`DT`).

### Testing Strategy
- `test_worldgen SEED` checks generation (reachability, structures, stories, the king's way and
  heirs) on seeds 1337 and 42 at least.
- `--demo NAME` modes drive scripted checks and screenshots (e.g. `kingtest`, `dragonfight`).
- `--bot STYLE` plays hour-long runs; `--bot-react MS` gives a bot a human's reaction; the fun
  audit (`docs/FUN_AUDIT.md`) scores feel, flow, choice, pacing, fairness and loop.
- CI (GitHub Actions) builds natively and for the browser and runs `test_worldgen`; when CI is
  off, the same checks are run locally before every push.

### Git Workflow
- Work lands on `main` in small commits that say what changed and why.

## Domain Context
- Danger rises with distance from the gate; layers are floors with spiral stairs between.
- The Sanctum is the player's home in a pocket realm; the Gate in its court leads to the realms.

## Important Constraints
- Runs on desktop and in the browser (phones included): every feature works with keyboard and
  mouse, gamepad and touch where it can.
- Retro look: low internal resolution, quantised light, fog; the game is dark by design.

## External Dependencies
- SDL3 (fetched by CMake), Emscripten for the browser build, ElevenLabs for generating sounds.

## How this folder was started
OpenSpec was set up in November 2026 by hand in its standard layout. The specs cover the systems
changed since the realms (2026-11-16); earlier systems are described in `docs/` and will get
specs as they are next changed.
