// Every sound in the game, as prompts for the ElevenLabs sound effects API.
// tools/gen_sfx.mjs generates them into assets/sfx/ and writes src/game/sfx_ids.h from this list,
// so the game and the files always agree.
//
//   id      - enum name in C (SFX_<ID>) and file name (assets/sfx/<id>.wav, or <id>_1.wav... with variants)
//   text    - the prompt
//   dur     - seconds (0.5 .. 30)
//   loop    - generated as a seamless loop (ambience)
//   v       - variants: several takes, the game picks one at random each time
//
// The style line is added to every prompt so the whole set sounds like one game.

export const STYLE = "dark fantasy dungeon crawler game sound effect, gritty, atmospheric";

export const SOUNDS = [
  // ---------------- monsters ----------------
  { id: "crawler_alert", dur: 1.2, text: "a large insect-like crawling creature hisses and clicks its mandibles as it notices you" },
  { id: "crawler_attack", dur: 0.8, text: "fast chitinous insect bite, snapping mandibles, quick lunge" },
  { id: "crawler_hurt", dur: 0.6, text: "giant bug squeals in pain, wet crunch" },
  { id: "crawler_death", dur: 1.2, text: "giant insect dies with a crunching splat and a fading chittering screech" },

  { id: "brute_alert", dur: 1.8, text: "huge ogre brute lets out a deep furious bellowing roar" },
  { id: "brute_attack", dur: 1.0, text: "massive ogre grunts and swings a heavy club, deep whoosh" },
  { id: "brute_hurt", dur: 0.8, text: "huge ogre grunts in pain, deep and angry" },
  { id: "brute_death", dur: 2.0, text: "giant ogre groans and collapses heavily onto a stone floor, body thud" },

  { id: "wisp_alert", dur: 1.5, text: "ghostly will-o-the-wisp, eerie ethereal shimmering whisper chime rising" },
  { id: "wisp_attack", dur: 1.0, text: "magical spirit fireball launched, airy whoosh with crackling flame" },
  { id: "wisp_hurt", dur: 0.6, text: "ghostly spirit flickers and distorts, short ethereal wail" },
  { id: "wisp_death", dur: 1.5, text: "spirit dissipates into the air, ethereal wail fading out with shimmering sparkle" },

  { id: "horned_alert", dur: 1.4, text: "horned demon beast snorts and roars aggressively, like an angry bull" },
  { id: "horned_attack", dur: 0.8, text: "horned beast gores with its horns, heavy impact and snarl" },
  { id: "horned_charge", dur: 1.4, text: "horned beast bellows and charges, heavy galloping hooves on stone" },
  { id: "horned_hurt", dur: 0.6, text: "horned beast grunts and snorts in pain" },
  { id: "horned_death", dur: 1.6, text: "horned demon beast lets out a dying bellow and crashes to the ground" },

  { id: "skeleton_alert", dur: 1.2, text: "skeleton warrior rises, dry bones rattling and clattering" },
  { id: "skeleton_attack", dur: 0.7, text: "skeleton archer draws an old creaking bow and looses an arrow, twang" },
  { id: "skeleton_hurt", dur: 0.6, text: "dry bones cracking and rattling from a hit" },
  { id: "skeleton_death", dur: 1.4, text: "skeleton collapses into a pile of clattering bones on stone" },

  { id: "slime_alert", dur: 1.0, text: "gelatinous slime gurgles and bubbles" },
  { id: "slime_attack", dur: 0.7, text: "slime monster lunges with a wet sticky splat" },
  { id: "slime_hop", dur: 0.6, text: "small wet squelching hop of a jelly blob landing" },
  { id: "slime_hurt", dur: 0.5, text: "wet squishy jelly impact, gloopy" },
  { id: "slime_death", dur: 1.2, text: "slime bursts with a big wet splatter and gurgle" },
  { id: "slime_split", dur: 1.0, text: "slime splits apart into two smaller blobs, wet stretching squelch" },

  { id: "bat_alert", dur: 1.0, text: "cave bat high-pitched screech with rapid wing flaps" },
  { id: "bat_attack", dur: 0.6, text: "bat swoops in and bites, quick flutter and squeak" },
  { id: "bat_hurt", dur: 0.5, text: "bat squeals in pain" },
  { id: "bat_death", dur: 0.9, text: "bat shrieks and falls, last weak wing flutters" },

  { id: "monster_stun", dur: 1.0, text: "heavy creature slams head-first into a stone wall, dull thud and dazed groan" },
  { id: "named_appear", dur: 3.0, text: "ominous deep horn blast and rising dark choir, a powerful enemy champion appears" },
  { id: "named_death", dur: 3.0, text: "powerful villain dies, booming dying roar followed by a dark resonant impact and echo" },
  { id: "enemy_fire_hit", dur: 0.8, text: "magic fireball explodes against stone, fiery whoomp" },

  // ---------------- weapons ----------------
  { id: "swing_dagger", dur: 0.5, v: 2, text: "quick light dagger slash through the air, sharp swish" },
  { id: "swing_sword", dur: 0.6, v: 2, text: "steel sword swing through the air, metallic whoosh" },
  { id: "swing_axe", dur: 0.7, v: 2, text: "heavy battle axe swing, deep whoosh" },
  { id: "swing_mace", dur: 0.7, v: 2, text: "heavy iron mace swung through the air, low heavy whoosh" },
  { id: "swing_spear", dur: 0.6, v: 2, text: "spear thrust, fast piercing whoosh" },
  { id: "swing_greatsword", dur: 0.9, v: 2, text: "huge two-handed greatsword swing, long heavy metallic whoosh" },
  { id: "whirlwind", dur: 1.2, text: "warrior spins with a sword in a whirlwind attack, rapid circling whooshes of steel" },
  { id: "shoot_shortbow", dur: 0.6, v: 2, text: "short bow string release, light twang and arrow whoosh" },
  { id: "shoot_longbow", dur: 0.8, v: 2, text: "longbow draw and powerful release, deep bowstring twang and arrow whoosh" },
  { id: "shoot_crossbow", dur: 0.7, v: 2, text: "crossbow fired, mechanical clack and thwack of the bolt" },
  { id: "arrow_storm", dur: 1.4, text: "volley of many arrows released at once, a swarm of whooshing arrows" },
  { id: "cast_wand", dur: 0.6, v: 2, text: "magic wand zap, small sparkling arcane bolt" },
  { id: "cast_staff", dur: 0.8, v: 2, text: "wizard staff launches a magic bolt, humming energy blast" },
  { id: "cast_scepter", dur: 0.8, v: 2, text: "royal scepter fires a resonant glowing magic orb, deep chime and whoosh" },
  { id: "staff_nova", dur: 1.5, text: "ring of fire bursts outward from a mage, roaring expanding flame wave" },
  { id: "hit_blade", dur: 0.5, v: 3, text: "sword slices into a monster, fleshy cut" },
  { id: "hit_blunt", dur: 0.5, v: 2, text: "heavy blunt weapon smashes into a creature, bone crunching thud" },
  { id: "hit_arrow", dur: 0.5, v: 2, text: "arrow thunks into flesh" },
  { id: "hit_wall", dur: 0.5, v: 2, text: "sword blade strikes a stone wall, metallic clang with sparks" },
  { id: "arrow_wall", dur: 0.5, v: 2, text: "arrow hits a stone wall and clatters" },
  { id: "bolt_hit", dur: 0.6, text: "magic energy bolt impacts, crackling arcane burst" },
  { id: "explosion", dur: 1.5, text: "fireball explosion in a stone dungeon, deep boom with echo and debris" },
  { id: "chain_zap", dur: 0.8, text: "electric chain lightning arcs between targets, sharp crackling zaps" },

  // ---------------- spells ----------------
  { id: "cast_fire", dur: 1.0, text: "fire spell cast, roaring flame whoosh igniting" },
  { id: "cast_frost", dur: 1.0, text: "ice spell cast, crystalline freezing shimmer and frosty wind" },
  { id: "cast_lightning", dur: 1.0, text: "lightning spell cast, sharp electric crackle and buzz" },
  { id: "cast_poison", dur: 1.0, text: "poison spell cast, bubbling acidic hiss and gurgle" },
  { id: "cast_arcane", dur: 1.0, text: "arcane magic spell cast, mystical shimmering hum and whoosh" },
  { id: "cast_holy", dur: 1.0, text: "holy light spell cast, radiant choir shimmer and bright chime" },
  { id: "cast_shadow", dur: 1.0, text: "shadow magic spell cast, dark whispering whoosh, sinister" },
  { id: "cast_earth", dur: 1.0, text: "earth magic spell cast, grinding rocks and deep rumble" },
  { id: "impact_fire", dur: 0.8, text: "fire spell impact, burst of flames and crackle" },
  { id: "impact_frost", dur: 0.8, text: "ice spell impact, shattering ice and freezing crack" },
  { id: "impact_lightning", dur: 0.8, text: "lightning strike impact, loud electric crack and zap" },
  { id: "impact_poison", dur: 0.8, text: "poison splash impact, acidic sizzling splat" },
  { id: "impact_arcane", dur: 0.8, text: "arcane magic impact, shimmering energy burst" },
  { id: "impact_holy", dur: 0.8, text: "holy light impact, radiant bright burst with ringing chime" },
  { id: "impact_shadow", dur: 0.8, text: "shadow magic impact, dark muffled thump with ghostly whisper" },
  { id: "impact_earth", dur: 0.8, text: "earth magic impact, rocks smashing and crumbling" },
  { id: "meteor_fall", dur: 1.2, text: "meteor falling from the sky, rising roaring whistle" },
  { id: "meteor_impact", dur: 2.0, text: "massive meteor impact explosion, ground shaking boom and rumbling debris" },
  { id: "rune_place", dur: 0.8, text: "magic rune inscribed on the floor, glowing hum and soft chime" },
  { id: "rune_trigger", dur: 1.2, text: "magic trap rune detonates, sharp magical blast" },
  { id: "storm_strike", dur: 0.8, v: 2, text: "magical storm bolt strikes the ground from above, crack and rumble" },
  { id: "aura_pulse", dur: 0.8, text: "pulsing magical aura around a hero, low resonant throb" },
  { id: "orbit_hum", dur: 1.0, text: "spinning magical orbs circling around, whirring energy hum" },
  { id: "breath", dur: 0.8, v: 2, text: "magical breath stream sprayed from hands, roaring rushing gust" },
  { id: "spell_heal", dur: 1.4, text: "healing spell, warm gentle chimes and soothing shimmer" },
  { id: "spell_blink", dur: 0.8, text: "teleport blink, quick magical whoosh and pop" },
  { id: "spell_ward", dur: 1.2, text: "magic shield barrier raised, resonant glassy hum forming" },
  { id: "ward_absorb", dur: 0.5, v: 2, text: "attack deflected by a magic shield, glassy energy impact" },
  { id: "spell_haste", dur: 1.0, text: "haste spell, quick rising whoosh and ticking speed shimmer" },
  { id: "spell_flare", dur: 1.2, text: "magic flare launched, bright fizzing ignite then steady sizzle" },
  { id: "spell_recall", dur: 1.6, text: "recall teleport spell, swirling magical vortex and deep whoosh" },
  { id: "spell_thread", dur: 1.6, text: "golden thread spell, delicate magical harp shimmer leading the way" },
  { id: "spell_fizzle", dur: 0.6, text: "spell fails for lack of mana, weak magical fizzle and puff" },

  // ---------------- the player ----------------
  { id: "step_stone", dur: 0.5, v: 3, text: "single boot footstep on a stone dungeon floor" },
  { id: "step_dirt", dur: 0.5, v: 3, text: "single boot footstep on dirt and gravel" },
  { id: "step_wood", dur: 0.5, v: 3, text: "single boot footstep on creaky wooden planks" },
  { id: "step_water", dur: 0.5, v: 3, text: "single footstep splashing in shallow water" },
  { id: "step_snow", dur: 0.5, v: 3, text: "single footstep crunching on ice and frozen snow" },
  { id: "jump", dur: 0.5, text: "adventurer jumps, short grunt and cloth rustle" },
  { id: "land", dur: 0.5, text: "adventurer lands on stone after a jump, heavy boot thud" },
  { id: "player_hurt", dur: 0.6, v: 3, text: "male adventurer grunts in pain from a hit" },
  { id: "player_death", dur: 2.0, text: "adventurer dying groan and body collapsing with armor clatter" },
  { id: "respawn", dur: 2.0, text: "hero is revived at a holy shrine, rising angelic shimmer and deep breath" },
  { id: "level_up", dur: 2.0, text: "level up, triumphant magical fanfare with bright chimes" },
  { id: "potion_drink", dur: 1.0, text: "drinking a potion from a glass bottle, gulp and refreshing sparkle" },

  // ---------------- items ----------------
  { id: "pickup_gold", dur: 0.6, v: 2, text: "picking up gold coins, jingling clink" },
  { id: "pickup_potion", dur: 0.6, text: "picking up a glass potion bottle, liquid slosh and glass clink" },
  { id: "pickup_scroll", dur: 0.8, text: "picking up an old magic scroll, parchment rustle with a faint mystical chime" },
  { id: "pickup_weapon", dur: 0.7, text: "picking up a metal weapon, steel scrape and clank" },
  { id: "pickup_relic", dur: 1.6, text: "ancient relic found, resonant mystical chord and glittering shimmer" },
  { id: "pickup_quest", dur: 1.0, text: "important quest item picked up, soft magical chime and cloth rustle" },
  { id: "chest_open", dur: 1.5, text: "old wooden treasure chest creaks open, coins and treasure inside" },
  { id: "hoard_open", dur: 2.5, text: "sealed vault hoard revealed, heavy chest opens with a cascade of gold coins and a triumphant shimmer" },
  { id: "scrap_item", dur: 0.8, text: "breaking down a weapon for scrap, metal clanks and coins" },
  { id: "equip", dur: 0.6, text: "equipping a weapon, leather and steel sliding into hand" },
  { id: "bag_full", dur: 0.5, text: "error buzz, dull wooden knock, inventory full" },

  // ---------------- story and menus ----------------
  { id: "read_note", dur: 1.0, text: "unfolding old parchment, paper rustle" },
  { id: "chapter_done", dur: 3.0, text: "dark epic fantasy victory sting, low brass and timpani swell" },
  { id: "mission_done", dur: 1.6, text: "quest completed, coins and a short heroic chime" },
  { id: "shrine", dur: 2.5, text: "holy shrine attuned, warm resonating bell and angelic choir swell" },
  { id: "menu_open", dur: 0.5, text: "opening a leather-bound book, page flip" },
  { id: "menu_close", dur: 0.5, text: "closing a leather-bound book, soft thump" },
  { id: "menu_move", dur: 0.5, text: "short soft wooden click, menu cursor" },
  { id: "skill_buy", dur: 1.0, text: "learning a new skill, magical power-up chime" },
  { id: "map_open", dur: 0.6, text: "unrolling an old parchment map, paper rustle" },
  { id: "place_enter", dur: 2.5, text: "entering a new region of a dungeon, deep ominous reverberating gong" },

  // ---------------- mechanisms and traps ----------------
  { id: "lever", dur: 1.0, text: "pulling an old iron lever, heavy rusty clunk and mechanism" },
  { id: "statue_turn", dur: 1.2, text: "heavy stone statue rotating on its base, grinding stone scrape" },
  { id: "mirror_turn", dur: 0.9, text: "turning a bronze mirror on a pivot, metal creak and glassy ring" },
  { id: "plate_ok", dur: 0.8, text: "stepping on a pressure plate, stone click and a soft magical chime" },
  { id: "plate_wrong", dur: 1.2, text: "wrong pressure plate, stone click then a burst of fire from the floor" },
  { id: "gate_open", dur: 4.0, text: "huge stone vault gate grinding open slowly, heavy chains and rumbling" },
  { id: "puzzle_solved", dur: 2.0, text: "ancient puzzle solved, mystical resonant chime and distant mechanism unlocking" },
  { id: "darts", dur: 0.7, text: "dart trap fires, quick mechanical click and whooshing darts from the wall" },
  { id: "spikes", dur: 0.7, text: "spike trap shoots up from the floor, sharp metallic shing" },
  { id: "spikes_rattle", dur: 0.5, text: "metal spikes rattling under a floor grate, warning" },
  { id: "flames", dur: 1.4, text: "fire trap vent bursts into a roaring jet of flame" },
  { id: "lava_burn", dur: 0.7, text: "flesh sizzling on hot lava, burning hiss" },

  // ---------------- ambient one-shots (played now and then around the player) ----------------
  { id: "amb_drip", dur: 1.5, v: 2, text: "single water droplet falling into a puddle in a cave, echoing" },
  { id: "amb_rumble", dur: 4.0, text: "distant deep rumble of stone shifting somewhere in a vast dungeon" },
  { id: "amb_chains", dur: 2.5, text: "distant chains rattling and swaying in a dark dungeon, echoing" },
  { id: "amb_scream", dur: 2.5, text: "faint distant agonized scream echoing through dungeon halls" },
  { id: "amb_whisper", dur: 3.0, text: "eerie ghostly whispers, unintelligible, close and unsettling" },
  { id: "amb_door", dur: 2.5, text: "distant heavy wooden door creaking and slamming shut, echo" },
  { id: "amb_growl", dur: 2.5, text: "distant monster growl echoing from deep within a cave" },
  { id: "amb_rocks", dur: 2.0, text: "small rocks and pebbles falling and tumbling in a cave" },
  { id: "amb_bell", dur: 4.0, text: "distant lonely church bell tolling once, long reverb" },

  // ---------------- ambience loops: one per stratum, plus the outside ----------------
  { id: "loop_exterior", dur: 14, loop: true, text: "night wind howling around a mountain fortress, distant crickets, cold and lonely" },
  { id: "loop_dungeon", dur: 14, loop: true, text: "dark stone dungeon ambience, low drone, occasional water drips, faint distant echoes" },
  { id: "loop_castle", dur: 14, loop: true, text: "cold empty castle hall ambience, wind moaning through stone corridors, faint flag flapping" },
  { id: "loop_mines", dur: 14, loop: true, text: "old mine tunnel ambience, creaking timber supports, distant pickaxe echoes, dripping water" },
  { id: "loop_roots", dur: 14, loop: true, text: "underground cavern full of giant roots, organic creaking, insects skittering, damp earth" },
  { id: "loop_limbo", dur: 14, loop: true, text: "limbo underworld ambience, hollow ghostly wind, faint mournful voices, unsettling drone" },
  { id: "loop_inferno", dur: 14, loop: true, text: "hellish inferno ambience, rumbling fire, bubbling lava, distant tormented moans" },
  { id: "loop_core", dur: 14, loop: true, text: "deep inside the earth's molten core, enormous low magma roar, pressure rumbling" },
  { id: "loop_spires", dur: 14, loop: true, text: "high stone tower spires, strong whistling wind, distant crows, open height" },
  { id: "loop_clouds", dur: 14, loop: true, text: "floating above the clouds, soft airy wind, vast open sky, gentle breeze" },
  { id: "loop_heaven", dur: 14, loop: true, text: "celestial heaven ambience, soft angelic choir pad, gentle shimmering chimes, serene" },
  { id: "loop_astral", dur: 14, loop: true, text: "astral void in outer space, deep cosmic drone, strange twinkling tones, otherworldly" },

  // ---------------- loops near things in the world ----------------
  { id: "loop_fire", dur: 8, loop: true, text: "torch fire crackling and burning steadily, close up" },
  { id: "loop_lava", dur: 8, loop: true, text: "molten lava bubbling and gurgling, thick blobs popping, hot hiss" },
  { id: "loop_water", dur: 8, loop: true, text: "underground water flowing and lapping gently against stone" },
  { id: "loop_flames", dur: 6, loop: true, text: "roaring jet of fire from a trap vent, steady loud flame" },
];
