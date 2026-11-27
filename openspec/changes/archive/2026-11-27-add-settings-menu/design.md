# Design

## Decisions
- **One set, kept as text**: `Settings g_set` is the truth; `settings_apply` pushes it to the
  renderer, the sound and the options, `settings_from_game` pulls the F-keys' changes back. Text
  lines keep the file readable and tolerant across builds.
- **Brightness as gamma** after the vignette and before dithering, so dark caves lift without
  washing out the fog colour.
- **Effects volume** is applied where sfx.c starts and moves voices (a macro over `audio_play` and
  `audio_set` in the effects section), music by its own gain, master on the mixer.
