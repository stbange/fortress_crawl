// The game's music: five instrumental pieces made with the ElevenLabs music API.
// tools/gen_music.mjs turns them into assets/music/<id>.wav (mono 22 kHz) and writes
// src/game/music_ids.h.
//
//   id    - enum name in C (MUS_<ID>) and file name
//   sec   - length in seconds
//   when  - where the game plays it (documentation)

export const MUSIC = [
  {
    id: "gate",
    sec: 150,
    when: "title screen and outside the fortress",
    text:
      "Instrumental cinematic ambient score for a dark fantasy game title. A colossal fortress carved into a mountain at night. " +
      "Opens with a low, slowly swelling synthesizer drone and distant wind-like textures; deep sustained brass chords enter, " +
      "then a slow, noble string theme in a minor modal key with a wordless low male choir. Wide, reverberant, melancholic and " +
      "majestic, building to one restrained epic swell with massive low brass and deep drum hits, then receding to a quiet drone. " +
      "No flutes. Ends softly.",
  },
  {
    id: "deep",
    sec: 160,
    when: "exploring below: dungeon, mines, hells, core",
    text:
      "Instrumental dark ambient orchestral score for exploring endless underground halls. Slow and spacious: sub-bass drones, " +
      "analog synthesizer pads with long reverb tails, distant metallic resonances, low cellos and contrabasses holding long notes, " +
      "occasional deep soft percussion like far-off machinery. Tension without action; a sparse, sorrowful two-note motif on low " +
      "strings and processed piano. Cold, mysterious, heavy and ancient. Very gradual changes, no dominating melody. Ends quietly " +
      "so it can loop.",
  },
  {
    id: "heights",
    sec: 160,
    when: "exploring above: castle, sky, heaven, astral void",
    text:
      "Instrumental ambient cinematic score for climbing into vast skies above an ancient fortress. Shimmering high string " +
      "harmonics and warm analog synthesizer pads, a slow rising chord progression, a soft wordless female choir far away, bowed " +
      "crystal and glass textures, a gentle pulsing arpeggio from a vintage synthesizer. Awe, loneliness and wonder, luminous but " +
      "bittersweet, with a quiet heroic horn line in the middle. Broad and slow. No flutes. Ends softly so it can loop.",
  },
  {
    id: "battle",
    sec: 140,
    when: "when monsters are attacking",
    text:
      "Instrumental cinematic battle score, dark fantasy, driving but not frantic: a relentless string ostinato in a minor key, " +
      "pulsing low synthesizer bass, heavy deep drums and metallic hits, ticking percussion building tension, brass stabs and a " +
      "massive low brass motif with deep braams. Gritty modern hybrid orchestral sound with electronic textures, steady tempo " +
      "around 100 BPM. Builds in waves with brief drops, never cheerful. No flutes. Ends on a sustained low chord.",
  },
  {
    id: "champion",
    sec: 150,
    when: "when a named enemy (story target or domain lord) is close",
    text:
      "Instrumental epic cinematic score for confronting a legendary enemy champion in a dark fantasy world. Starts with an " +
      "ominous low drone and a single tolling bell, then a slow, menacing theme on low brass and a deep male choir chanting " +
      "wordless vowels, huge percussion hits, distorted synthesizer swells and tense high strings. Builds to an overwhelming " +
      "climax with full orchestra and choir, full of dread and grandeur, then collapses into a dark sustained echo.",
  },
];
