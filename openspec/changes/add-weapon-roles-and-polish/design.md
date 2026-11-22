# Design

## Context
The audit's P3 items: weapons that want switching (16), bots that do what players do (17), less
text over the view (18), the browser's time to play (19) and a measurement on a real GPU (20).

## Decisions
- **Roles as multipliers in the one place every weapon hit passes** (`weapon_hit_ex`), after
  crits and silver, so the existing powers and materials keep working. Attack spells get the
  armour bonus in `spell_projectile_hit`. Dragons are left out: their fight has its own rules.
- **Blunt is by base** (mace, flail, sling, cannon), because the sword slot holds several kinds.
- **Alternate attacks from the start, weaker**: the small whirl (0.9x, +0.3 m, 10 mana), the
  power shot (2.2x, pierces 3, 10 mana), the small nova (12 bolts at 18, short range, 16 mana).
  The nova's bolts now count as staff hits, so they take the staff's role and powers.
- **The swap key is Z** (free on the keyboard); the gamepad keeps its shoulder buttons, which
  already cycle without touching the swing timer.
- **The tracker dims by its text's hash**, so each of the three trackers needs only to draw through
  `game_tracker_text`.
- **Music by `emscripten_async_wget`** after the first `music_update`, one piece after another,
  the gate's first (the title screen's). Saved without a .wav name so the browser does not try to
  decode it itself. ADPCM by ffmpeg in `web/build.sh`; without ffmpeg the files go in as they are.
- **The real-GPU measurement cannot be made in the build container** (software rendering only):
  the browser build now always logs TIMING to its console and the desktop keeps FC_TIMING, so it
  is one run on any machine with a GPU.

## Risks
- ADPCM is lossy (4 bits a sample). Measured against the originals: the music keeps 27-46 dB
  signal to noise, the sounds 14-35 dB, worst on sharp hits and zaps. Nobody has listened yet;
  FC_WEB_RAW_SFX=1 builds with the sounds as they are (index.data 24 MB) if it shows.
- Weapon roles make the blade worse against knights; the banner says what to use instead.
