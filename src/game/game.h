#ifndef FC_GAME_H
#define FC_GAME_H

#include <SDL3/SDL.h>

#include "core/fmath.h"
#include "game/alchemy.h"
#include "game/food.h"
#include "game/forge.h"
#include "game/items.h"
#include "game/physics.h"
#include "game/player.h"
#include "game/sfx.h"
#include "game/skills.h"
#include "render/renderer.h"
#include "world/chunkmap.h"
#include "world/layout.h"
#include "world/pocket.h"
#include "world/realms.h"
#include "world/towns.h"
#include "world/dragons.h"
#include "world/king.h"
#include "game/tele.h"
/* WBF_* of the weapon landing the current blow (monster_damage reads it: a flail ignores shields) */
extern unsigned g_hit_flags;
#include "world/story.h"

#define MAX_MONSTERS 320
#define MAX_PROJECTILES 512
#define MAX_PARTICLES 2400
#define MAX_FLASHES 16
#define MAX_SHRINES 128
#define MAX_PLAYERS 4

/* Flow field around the player that monsters follow (cells). */
#define FLOW_R 24
#define FLOW_H 8
#define FLOW_W (2 * FLOW_R)
#define FLOW_Y (2 * FLOW_H)

/* 20 kinds, each with its own look and way of fighting (monsters.c, monster_draw.c). */
enum {
    MON_CRAWLER, MON_BRUTE, MON_WISP, MON_HORNED, MON_SKELETON, MON_SLIME, MON_BAT,
    MON_SPIDER,  /* leaps at you */
    MON_WRAITH,  /* vanishes and comes back behind you */
    MON_GOLEM,   /* stomps the ground around it */
    MON_SHAMAN,  /* heals its allies, throws orbs */
    MON_BLOATER, /* walks up and bursts */
    MON_EYE,     /* charges a beam you can step out of */
    MON_KNIGHT,  /* shield in front, two blows */
    MON_WORM,    /* burrows, bursts up under you */
    MON_MIMIC,   /* a chest until you are next to it */
    MON_NECRO,   /* raises the dead */
    MON_HARPY,   /* circles high and dives */
    MON_HOUND,   /* circles in packs, lunges from the side */
    MON_TROLL,   /* throws boulders */
    MON_DRAGON,  /* every dragon, by its kind and size (dragon.c, dragons.h) */
    MON_TYPE_COUNT
};
/* Monster grade: the same kind as a normal monster, an elite or a boss. */
enum { GRADE_NORMAL, GRADE_ELITE, GRADE_BOSS };
enum { MS_IDLE, MS_CHASE, MS_WINDUP, MS_CHARGE, MS_STUN };
enum { WPN_SWORD, WPN_BOW, WPN_STAFF, WPN_COUNT };
enum { PJ_ARROW, PJ_BOLT, PJ_NOVA, PJ_ENEMY_FIRE, PJ_SPELL, PJ_ENEMY_ARROW, PJ_BOULDER };

typedef struct MonsterDef {
    const char *name;
    float hw, h, hp, dmg, speed, reach, attack_time, xp;
    bool flies;
    vec3 color;
    float windup;      /* seconds between raising an attack and landing it */
    int min_level;     /* not met before this danger level */
    const char *boss;  /* title of its bosses */
} MonsterDef;
extern const MonsterDef MONSTERS[MON_TYPE_COUNT];

typedef struct Monster {
    bool alive, elite, awake, on_ground;
    u8 type, state;
    vec3 pos, vel;
    float yaw, scale;
    float hp, maxhp, dmg, xp;
    float timer, cooldown, flash, anim;
    vec3 color, charge_dir;
    u64 chunk_key;
    int target;      /* player slot being chased */
    int last_hitter; /* player slot that hit it last (gets the kill), -1 none */
    /* status effects from elements */
    float burn_t, burn_dps, poison_t, poison_dps, slow_t;
    float level;     /* danger level it was made for (loot quality) */
    /* utility spells on it: charmed (fights for a player), afraid, a portal just taken */
    float charm_t, fear_t, portal_cd;
    /* a target it cannot get to: how long without getting closer, the closest it got, and
     * how long it then leaves you alone */
    float giveup_t, giveup_d, sulk_t;
    float close_t;   /* brute: how long you have stood within its reach */
    float air_used;  /* seconds under water (docs/SIMULATION.md): past MON_BREATH it drowns */
    float wet;       /* 0..1 soaked (fire hurts half, lightning twice, frost freezes it)*/
    float dig_t;     /* the big ones break through what blocks them: time to the next blow */
    /* needs when nobody is about (docs/SIMULATION.md): 1 a drink, 2 a rest by warmth */
    u8 need;
    float need_t;    /* until the next need, or how long it stays */
    vec3 need_at;
    i16 charmer;     /* player slot it fights for */
    u8 alt_kind;     /* this tick's target: 0 a player, 1 a decoy or lure, 2 a monster, 3 follows its charmer */
    i16 alt_idx;     /* that decoy (effect) or monster */
    /* its region: rank = 10-chunk steps from the gate (0..10), element = the region's,
     * carried by its attacks, resisted, and weak to the opposite one */
    u8 rank, element;
    u8 grade;        /* GRADE_* */
    u8 hidden;       /* vanished (wraith) or underground (worm): cannot be hit */
    u8 aux;          /* per-kind state: knight combo, troll throw, bloater burst, summoned */
    float t2;        /* per-kind timer */
    vec3 color2, glow; /* armour / bone colour and the glow of eyes and markings, from the world */
    /* named enemies (chapter targets and domain lords) */
    u64 story_key;
    u8 named_kind;   /* 0 none, else NK_* + 1 */
    u8 orders;       /* a hunters' leader: 1 + the chapter whose target sent it */
    u8 parley;       /* a chapter target that yielded: 1 waiting for a choice, 2 to be killed */
    u8 resident;     /* a spared one living in the Sanctum: harmless and unharmed */
    char name[STORY_NAME];
    u8 ruler;        /* the ruler of a realm behind the Gate: 1 + RM_* */
    u8 townq;        /* a pest a town asked you to kill: 1 + its record (town.c) */
    u8 king;         /* the Old King himself (1), his lieutenant (2), a servant he summoned (3), a raider on a town (4) (king.c) */
    u8 kingstep;     /* a lieutenant: 1 + its step */
    /* a dragon (dragon.c): its kind and size, what it is doing, its fight's phase */
    u8 dkind, dsize, dstate, dphase, heads, revived, grounded;
    float dtimer, dbreath, dwing, dalt, dheal, dstep;
    vec3 dhome, dgoal, dbreath_dir;
    u64 lair_key;
} Monster;

#define INPUT_BUFFER_S 0.12f  /* how long a press waits for its cooldown (openspec player-input) */
#define ENEMY_SHOT_HALF 0.25f /* half the box an enemy shot hits round the player's centre (openspec combat-fairness) */
typedef struct Projectile {
    bool alive;
    u8 kind;
    vec3 pos, vel;
    float dmg, life;
    int pierce;
    bool explode, chain;
    int last_hit;
    int owner; /* player slot that fired it, -1 for monsters */
    u8 element; /* EL_* effect on hit, EL_NONE for none */
    u8 form;    /* SF_* for spells */
    u8 rarity;  /* of the bow or staff that shot it (glow and impact effects) */
    float radius, tick, age;
    u8 wclass;  /* WPN_* of the weapon that shot it (its powers count), 255 for none */
    u8 wbase;   /* WB_* it came from (how it looks and flies) */
    u8 near;    /* an enemy shot: 1 passing close to a player, 2 counted (the fun audit's near-misses) */
    float grav; /* how fast it drops */
    bool back;  /* a chakram on its way back */
} Projectile;

typedef struct Particle {
    vec3 pos, vel, color;
    float life, max_life, size;
    bool emissive, gravity;
} Particle;

typedef struct Flash {
    vec3 pos, color;
    float radius, life, max_life;
} Flash;

/* Mechanisms (mech.c): vaults with their puzzles, and the devices near the players. */
#define MAX_VAULTS 48
#define MAX_DEVICES 320
#define MAX_MECH_CHUNKS 160
#define MECH_CELLS 2048
#define CHEST_VAULT 0x100000 /* a chest's amount: level | CHEST_VAULT for the chest in a vault */

typedef struct MechVault {
    bool alive, solved, beam_on;
    Vault v;          /* as generated (local cells) */
    u64 chunk_key;
    int ox, oy, oz;   /* chunk origin */
    int cx, cz, y;    /* inner centre (world), feet */
    int gx, gz;       /* the gate cell (world) */
    int level;
    u8 lamps, progress;
    u8 facing[VAULT_PARTS]; /* statues and mirrors */
    float open;       /* the gate: 0 shut .. 1 up */
    int beam[(VAULT_PARTS + 2) * 2], nbeam; /* turning points of the light (local x, z) */
} MechVault;

typedef struct Device {
    bool alive;
    u8 kind, part, dir, size, state;
    int vault;        /* index in vaults, -1 for traps */
    int x, y, z;      /* world cell, feet */
    int level;
    float t, cool, anim;
    u8 hit_players;   /* spikes: who was hit this cycle */
    u64 hit_mons;
    u64 chunk_key;
} Device;

/* A task from something found in the dungeon (a bounty board, a dead adventurer's note). */
enum { MI_BOUNTY = 1, MI_RETRIEVE };
#define MAX_MISSIONS 6
typedef struct Mission {
    u8 type;
    u64 key;          /* the lord to kill or the item to bring back */
    int tx, ty, tz;   /* target chunk */
    int sx, sy, sz;   /* the trace it came from */
    int reward;       /* gold */
} Mission;

/* Lore read in this world (shared by all characters). */
enum { LORE_TRACE = 1, LORE_REVEAL, LORE_LEGEND, LORE_ORDERS, LORE_PAGE, LORE_RESIDENT, LORE_REALM, LORE_KING, LORE_KINDS }; /* LORE_REALM: a = realm, b = 0 its story, 1 + q a page of the Library */
#define MAX_LORE 512
typedef struct LoreEntry {
    u8 kind;
    int a, b, c; /* the trace's chunk, or the chapter */
    char *text;  /* generated on first display (not saved) */
} LoreEntry;

#define BREATH_MAX 15.0f
#define MON_BREATH 12.0f
#define MAX_BOMBS 16
typedef struct Bomb {
    bool alive;
    vec3 pos, vel;
    float fuse;
    int owner, level;
    u8 kind;      /* THROW_*: a bomb bounces and goes off; a flask breaks where it first hits */
} Bomb;
/* What X throws (C chooses): a bomb or one of the flasks (docs/SIMULATION.md) */
enum { THROW_BOMB, THROW_OIL, THROW_FROST, THROW_TONIC, THROW_COUNT };

typedef struct Stats {
    float hp, maxhp, mana, maxmana;
    int level, points, kills;
    float xp, xp_next;
    u8 ranks[SKILL_COUNT];
    int weapon;
    float cooldown, swing, swing_len;
    int swing_kind;     /* 0 primary, 1 alternate */
    float hurt, since_hurt, invuln;
    bool dead;
    float dead_t;
    float last_death;   /* g->time of the last death: a second one soon after costs less */
    vec3 respawn;
    float respawn_yaw;
    /* items */
    Item equipped[WPN_COUNT];
    Item bag[BAG_SIZE];
    int gold, potions_hp, potions_mp;
    u8 relics[RELIC_COUNT]; /* how many of each */
    /* spells */
    u8 spell_rank[SPELL_COUNT];
    int spell;      /* selected spell */
    float spell_cd;
    float ward, ward_t, haste_t;
    /* what monsters' elements did to you */
    float burn_t, burn_dps, poison_t, poison_dps, chill_t;
    /* utility spells running on you (utility.c), in seconds left */
    float levitate_t, wings_t, glide_t, feather_t, climb_t, stride_t, stoneskin_t;
    float scry_t, reveal_t, dowse_t, invis_t, steed_t;
    /* the pocket realm: where the door brought you from, and its comforts */
    bool realm_return_set;
    vec3 realm_return;
    float realm_return_yaw;
    /* the realms behind the Gate (gate.c): where you went through from, and what keeps their
     * rules off you (v25) */
    bool gate_from_set;
    vec3 gate_from;
    float gate_from_yaw;
    float sunward_t;  /* a sun-ward: the Sun does not burn */
    float company_t;  /* a night at an inn in good company (town.c): more XP, faster healing (v26) */
    float gills_t;    /* a gill draught: breath lasts five times as long */
    float refreshed_t, well_fed_t, rest_t, feast_t;
    u8 sanctum;          /* SANCTUM_*: the rooms bought back to life (shop.c) */
    float library_t;     /* until the library's next scroll */
    /* dying and coming back: the last safe ground, and a short time to return there */
    vec3 safe_pos, fell_pos;
    float fell_t;
    vec3 recall_from;    /* Recall cast twice within 5 s goes back */
    float recall_t;
    float fall_speed; /* fastest downward speed since leaving the ground (fall damage) */
    /* food (food.c): the larder, cooked dishes (recipe, quality, where cooked), the recipes
     * known, the meal eaten (recipe + 1) and a raw snack (food + 1) */
    u8 food[FD_COUNT];
    u8 dishes[RC_ALL][DISH_QUALITIES][CP_COUNT];
    u32 recipes;
    u8 meal, meal_q, snack;
    u8 last_meal, meal_repeat; /* the dish eaten last (recipe + 1) and how often in a row */
    float meal_t, meal_len, snack_t;
    /* alchemy (alchemy.c): reagents, bottled elixirs, formulae known, the elixir working
     * (elixir + 1; onset: until it takes hold; t: how long it still holds) */
    u8 reag[RG_COUNT];
    u8 elixirs[ELIXIR_COUNT];
    u32 formulae;
    u8 elixir;
    float elixir_onset, elixir_t;
    float elixir_k;      /* how strongly it works (half when drunk too soon after another) */
    float tox_t;         /* after an elixir: the body is still full of it */
    u32 formulae_torn;   /* formulae known from a torn page: the ingredients, not the steps */
    /* the forge (forge.c): materials, smithing skill, blueprints (WB_* bits) and runes known
     * (PT_* and PE_* bits) */
    u8 forge[FG_COUNT];
    int smith_xp;
    u8 armour, armour_el;   /* dragon-scale armour (forge.c): 0 none, 1-3 its make; the element its scales ward off */
    u8 pick;   /* the pickaxe: 1 iron, 2 steel, 3 mithril (docs/DESTRUCTION.md) */
    int bombs;
    /* temperature and wetness (docs/SIMULATION.md) */
    u8 cloak;        /* a fur cloak: the cold reaches you four times slower */
    float cold;      /* 0..1: how far the cold has got into you (COLD from 0.5, CHILLED at 1) */
    float warm_t;    /* warmed (by a fire or a warming draught): the cold does not touch you */
    float wet;       /* 0..1: soaked through (no catching fire; lightning twice as bad; frost freezes) */
    u8 flasks[3];    /* oil, frost salt, growth tonic (THROW_OIL.. - 1) */
    u8 throw_sel;    /* THROW_*: what X throws */
    u8 firewood;     /* bundles: P lights a campfire (docs/SIMULATION.md) */
    u8 seeds;        /* to sow: crouch and strike the earth with the pickaxe */
    u32 blueprints;
    u16 runes_t, runes_e;
    /* story (per character) */
    u8 motive;       /* 0: not chosen yet, else MOTIVE_* + 1 */
    int chapter;
    Mission missions[MAX_MISSIONS];
    int nmissions;
} Stats;

/* Cells the player has seen, one bit per cell, per chunk. */
typedef struct ExploredChunk {
    int cx, cy, cz;
    u8 bits[CHUNK_VOL / 8];
} ExploredChunk;

typedef struct Explored {
    ExploredChunk **table;
    int cap, count;
} Explored;

typedef struct Options {
    u64 seed;
    bool has_seed;      /* given on the command line: no start screen */
    char seed_text[24]; /* what the player typed (numbers or words) */
    bool has_pos;
    vec3 pos;
    float yaw, pitch;
    bool fly;
    int radius, vradius;
    const char *shot;
    int win_w, win_h;
    int low_w, low_h;
    bool hud;
    bool demo; /* screenshot helper: spawn monsters in view and fire once */
    float shot_time; /* seconds after loading to take the screenshot */
    bool invert_x, invert_y;
    bool grab; /* capture the mouse even in screenshot mode (input tests) */
    int recenter; /* 1 force, -1 never, 0 auto (off under WSL) */
    float sens;
    int players;   /* start with this many characters (split screen) */
    bool test_pad; /* scripted virtual gamepad that joins and plays (test) */
    bool touch;    /* show the touch controls from the start */
    bool fresh;    /* ignore the save (new characters) */
    bool fullbright; /* --fullbright: no lighting (look at the geometry) */
    bool persist;  /* load and save even in screenshot runs (tests) */
    bool mute;     /* --mute: no sound */
    bool sound;    /* --sound: sound even in screenshot runs (audio tests) */
    bool auto_motive; /* screenshot runs: pick a motive without asking */
    int demo_chapter; /* --chapter n: story demos start at chapter n */
    const char *bot;  /* --bot STYLE: a playtest bot plays player 1 (bot.c) */
    const char *bot_log;
    float bot_minutes;
    float bot_react;  /* --bot-react MS: the bot sees and acts this much late, +-35 ms (a human's reaction; fun audit) */
    /* --capture dir: with --bot, plays at full speed until --capture-start seconds of game time,
     * then writes dir/NNNNN.bmp, one frame per 1/--capture-fps seconds of game time (steady,
     * whatever the machine's speed), for --capture-seconds seconds: footage for ffmpeg */
    const char *capture;
    float capture_start, capture_seconds, capture_fps;
    float capture_orbit, capture_drift; /* demos: turn the view this many degrees a second; walk or fly forward (0..1) */
} Options;

/* On-screen touch controls for player 1 (phones and tablets). */
enum { TB_ATTACK, TB_ALT, TB_JUMP, TB_CROUCH, TB_WEAPON, TB_SPELL, TB_NEXT_SPELL, TB_SKILLS, TB_MAP, TB_POTION, TB_THREAD, TB_COUNT };
#define TOUCH_MAX_FINGERS 10

typedef struct TouchCtl {
    bool active;       /* a finger has touched the screen: show the controls */
    bool forced;       /* --touch */
    struct { SDL_FingerID id; int role; } fingers[TOUCH_MAX_FINGERS]; /* role: TOUCH_* or TB_* */
    int nfingers;
    float mx0, my0, mx, my; /* move stick origin and thumb, in low-res pixels */
    bool moving;
    bool held[TB_COUNT];
} TouchCtl;

/* A character from the save, waiting for its slot (save.c). */
typedef struct SavedPlayer {
    bool valid;
    bool moved; /* saved before the layered world: starts again at the gate */
    Stats st;
    Player pl;
} SavedPlayer;

/* What the start screen shows about a world's save. */
typedef struct SaveInfo {
    u64 seed;
    char seed_text[24];
    int level, gold, kills;
    float play_time;
} SaveInfo;

/* Start screen with the seed choice (menu.c). */
typedef struct StartMenu {
    char text[24];
    int row, rolls;
    char peeked[24];  /* the text the save info below belongs to */
    bool has_save, confirm_forget;
    SaveInfo save;
    bool fresh; /* the suggested seed is replaced by typing */
    bool done;
    float t;
    SDL_JoystickID pad; /* the gamepad that used the menu becomes player 1's */
} StartMenu;

/* The golden thread spell: a fading line along a route to a target (quests.c). */
#define THREAD_MAX 600
typedef struct Thread {
    vec3 pts[THREAD_MAX];
    int n;
    float t, life;
    bool reached;
} Thread;

/* One local player: a separate character with its own input device, view and menus. */
typedef struct PlayerSlot {
    bool active;
    int index;
    Player pl;
    Stats st;
    vec3 color;
    /* input */
    bool keyboard;            /* keyboard and mouse (player 1) */
    SDL_Gamepad *pad;
    SDL_JoystickID pad_id;
    u32 pad_prev;             /* buttons held last frame, for presses */
    bool sprint_toggle;
    bool attack_held, alt_held; /* mouse buttons (player 1) */
    bool pad_attack, pad_alt;   /* gamepad triggers */
    bool spell_held, pad_spell; /* cast key (Q) / gamepad X */
    /* ui */
    bool show_map, show_skills; /* show_skills: the character menu */
    int menu_tab, inv_sel, book_sel;
    float potion_cd;
    /* story */
    Named target;           /* the current chapter's target (cache) */
    int target_chapter;     /* chapter the cache belongs to, -1 none */
    float target_retry;
    vec3 scry_pos;          /* Clairvoyance: where your sight is */
    float gate_cd;          /* just went through a gate or door */
    float realm_note_t;     /* the realms: until the next warning of the same kind */
    float shake;            /* the view shakes (a dragon's tread, its roar) */
    float credits_t;        /* the Old King settled: the ending and the credits are showing (king.c) */
    float returned_at;      /* when "back where you fell" was last used (once in 5 minutes) */
    bool waypoint_on;       /* a place the Golden Thread leads to first (the bot's dragon hunt; later the map) */
    vec3 waypoint;
    char waypoint_name[32];
    u8 track;               /* the line at the top: 0 the Old King, 1 the chronicle, 2 a town's task (Y) */
    float king_talk_t;      /* spoken to the Old King: a second blow within this settles it */
    u8 realm_notes;         /* the realms: warnings already given this visit (bits) */
    float ritual_t;         /* opening the Sanctum Door: seconds left of the ritual */
    float story_quiet;      /* seconds without a chapter finished (the story sends hunters) */
    char reading[STORY_TEXT]; /* the trace you stand at */
    u64 reading_key;
    float reading_t;
    char msg[STORY_TEXT], msg_next[STORY_TEXT]; /* story messages (chapters) */
    float msg_t;
    u64 domain_id;
    int domain_cx, domain_cy, domain_cz;
    char domain_name[STORY_NAME];
    int motive_sel, story_scroll;
    int parley, parley_sel; /* 1 + the monster that yielded to this player, the choice under the cursor */
    int parley_vote;        /* co-op: 1 + this player's choice, waiting for the others (quests.c) */
    u8 king_vote;           /* co-op: 1 + the end this player asked for at the faltering king (king.c) */
    bool pick_out;          /* the pickaxe in hand (the fourth slot): attack digs, the alternate throws a bomb */
    float bomb_cd;
    /* input buffers (P1 of the fun audit, openspec player-input): a press while the cooldown runs
     * is kept 120 ms and fires as soon as it may */
    float atk_buf, spell_buf, throw_buf;
    bool atk_prev, spell_prev;
    u8 throw_buf_kind;
    float breath, drown_t;
    float crush_t; bool crush_guard; /* a collapse: the window of one, and whether it may not kill */  /* seconds of air left under water (BREATH_MAX), the next drowning hurt */
    int shop_sel;
    int food_sel;           /* the FOOD tab's row */
    u8 pot[4];              /* ingredients in the pot (food ids) */
    int npot;
    u8 cauldron[FORMULA_STEPS]; /* alchemy: the steps so far, in order */
    int ncauldron, alch_sel, elixir_quick;
    /* the forge: what is planned at the anvil, and the session at the anvil */
    struct { u8 base, metal, ess[2], rune[2], heart, oil; } plan; /* ess: FG_* + 1; rune: trigger << 4 | effect */
    struct { bool on, reforge; float heat, flash, t; int need, done, score, flaws, last; char msg[80]; } fs;
    int forge_sel;
    /* weapon powers (combat.c): hits per weapon class, and each effect's cooldown */
    u16 hits[WPN_COUNT];
    float pow_cd[PE_COUNT];
    Thread thread;
    int thread_goal;
    float thread_cast_time;
    int skill_sel;
    char banner[128];
    float banner_time;
    char place[128];
    float lava_t;
    /* atmosphere of this player's view */
    vec3 fog, ambient;
    float fog_density, stars, moons, openness, openness_timer;
    float explore_timer;
    /* flow field toward this player, followed by the monsters chasing them */
    u16 *flow;
    u8 *flow_solid;
    int flow_ox, flow_oy, flow_oz;
    float flow_timer;
    int flow_px, flow_py, flow_pz;
} PlayerSlot;

/* The Sanctum's contents, one per world (its build stages are World.sanctum). */
#define STASH_SIZE 30
#define MAX_TROPHY_NAMED 48
typedef struct Sanctum {
    int keystones;
    Item stash[STASH_SIZE];
    u8 trophy[UNIQUE_COUNT];            /* this world's legendaries found */
    u64 named[MAX_TROPHY_NAMED];        /* named enemies killed (their story keys) */
    int nnamed;
    float library_t;                    /* until the library's next scroll */
    int keys_bought;                    /* keystones bought (each costs more) */
} Sanctum;

/* Neutral wildlife (wildlife.c): never attacks, monsters ignore it, gives food. */
enum { CR_HARE, CR_HEN, CR_GOAT, CR_RAM, CR_FISH, CR_CARP, CR_CRAB, CR_SALAMANDER, CR_GROUSE, CR_BEETLE, CR_SNAIL, CR_HIVE, CR_QUAIL,
       CR_REALM, /* an animal of a realm behind the Gate: its species in REALM_FAUNA (realms.h) */
       CR_COUNT };
enum { CS_GRAZE, CS_WALK, CS_FLEE, CS_HIDE };
typedef struct Critter {
    bool alive, on_ground;
    u8 kind, state, hp;
    vec3 pos, vel, home, color;
    float yaw, timer, anim, flash;
    float ready_t;  /* milk, honey or eggs: seconds until there is some again */
    float calm_t;   /* a hive: smoked (fire) */
    float angry_t;  /* a hive: its bees sting */
    float sound_t;
    float air_used;  /* seconds with its head under water (docs/SIMULATION.md) */
    u64 key, chunk_key;
    u8 species, variant, level; /* CR_REALM: REALM_FAUNA index, its variant, the realm's danger */
    float bite_t;    /* a fierce one: until its next bite */
} Critter;
#define MAX_CRITTERS 128

/* A temporary block change by a spell: to while it lasts, back to from afterwards. */
typedef struct WorldEdit {
    int x, y, z;
    u8 from, to;
    float until;
} WorldEdit;
#define MAX_EDITS 2048

typedef struct Game {
    Options opt;
    World world;
    Renderer r;
    ChunkMap map;
    PlayerSlot players[MAX_PLAYERS];
    int nplayers;
    vec3 start_pos;
    float start_yaw;
    SDL_Gamepad *idle_pads[8]; /* connected pads not yet driving a player */

    Monster mons[MAX_MONSTERS];
    Projectile proj[MAX_PROJECTILES];
    Particle parts[MAX_PARTICLES];
    int nparts;
    Flash flashes[MAX_FLASHES];
    Pickup pickups[MAX_PICKUPS];
    Effect effects[MAX_EFFECTS];
    Critter critters[MAX_CRITTERS];
    /* the people of the towns (town.c): townsfolk about, near the players */
    struct Folk {
        bool alive;
        u8 people, role, state, look;
        vec3 pos, home, goal;
        float yaw, timer, anim, act_t, talk_t;
        int rec;                   /* its town's record */
        u64 key;
        char name[32];
    } folk[48];
    /* the realms behind the Gate (gate.c): those joined to this world (from its seed), the one
     * the Gate in the Sanctum shows, rifts seen in the fortress, the orrery's eclipse */
    u32 realms_joined;
    int gate_sel;
    struct Rift { vec3 pos; u8 realm; bool alive; u64 chunk_key; float yaw; } rifts[24];
    int nrifts;
    float eclipse_t, realm_t, storm_t, tide;
    /* chests and relics already taken (keys) */
    u64 *looted;
    int looted_cap, looted_count;
    /* mechanisms near the players */
    MechVault vaults[MAX_VAULTS];
    Device devs[MAX_DEVICES];
    u64 mech_chunks[MAX_MECH_CHUNKS];
    int nmech_chunks;
    u64 mech_cells[MECH_CELLS]; /* cells made solid by devices (closed gates, statues) */
    int mech_ncells;
    bool mech_dirty;

    /* chunks whose monsters and shrines have been placed */
    u64 *spawned;
    int spawned_cap, spawned_count;
    int mons_anim_seed;
    vec3 shrines[MAX_SHRINES];
    int nshrines;

    Explored explored;
    TouchCtl touch;
    SavedPlayer saved[MAX_PLAYERS];
    StoryWorld story;
    LoreEntry lore[MAX_LORE];
    int nlore;
    u32 story_answers; /* the questions of the world's history answered so far (history.h) */
    bool parley_hold;  /* someone is choosing: monsters wait (co-op; alone, the game pauses) */
    Bomb bombs[MAX_BOMBS];
    bool world_started;
    bool freeze_monsters; /* screenshot demos: monsters stand still */
    /* utility spells (utility.c): timed changes to the world, cells only monsters cannot pass */
    Sanctum sanctum; /* the world's home (realm.c, shop.c); its build stages are in world.sanctum */
    WorldEdit edits[MAX_EDITS];
    int nedits;
    struct { int x, y, z; float until; } barriers[64];
    int nbarriers;
    float timeslip_t;
    PlayerSlot *draw_viewer; /* the view being drawn (Reveal the Hidden) */
    float play_time, autosave_timer;

    bool mouse; /* player 1's mouse is captured */
    bool hud;
    float fps;
    int res_mode;
    float time;
} Game;

/* game.c */
void game_init(Game *g, vec3 start, float yaw);
PlayerSlot *game_add_player(Game *g);
void game_event(Game *g, const SDL_Event *e);
/* Mouse look for player 1 from a motion in window pixels. */
void game_look(Game *g, float dx, float dy);
/* kb: keyboard input for player 1; gamepads are read inside. */
void game_update(Game *g, const PlayerInput *kb, float dt);
void game_render(Game *g);
/* Eye positions of the active players (chunk streaming centres); returns the count. */
int game_centres(Game *g, vec3 *out);
int game_solid(void *user, int x, int y, int z);
void game_banner(PlayerSlot *ps, const char *text);
void game_banner_all(Game *g, const char *text);
void game_add_xp(Game *g, PlayerSlot *ps, float xp);
float xp_for_level(int level); /* XP from this level to the next */
void game_hurt_player(Game *g, PlayerSlot *ps, float dmg, vec3 from);
/* Damage without a blow (burning, poison): no push. */
void game_dot_player(Game *g, PlayerSlot *ps, float dmg);
/* A monster's element on a player it hit: burn, chill, a jolt, poison, a smite or a drain. */
void player_element_hit(Game *g, PlayerSlot *ps, int element, float dmg);
float game_measure_openness(Game *g, vec3 eye);
void game_update_atmosphere(Game *g, PlayerSlot *ps, float dt);
/* Light at a world position as linear brightness, including ambient, torches and flashes. */
vec3 game_light_at(Game *g, vec3 p);
/* Nearest living player to p within max_dist, or NULL. */
PlayerSlot *game_nearest_player(Game *g, vec3 p, float max_dist);

/* monsters.c */
void monsters_spawn_for_chunks(Game *g);
void monsters_update(Game *g, float dt);
void monsters_draw(Game *g, vec3 eye);
void monster_damage(Game *g, Monster *m, float dmg, vec3 from, float knock, int attacker);
Monster *monster_spawn(Game *g, int type, vec3 pos, int level, bool elite, vec3 tint, u64 key);
/* Makes a monster its kind's boss: bigger, much tougher, a title and a health bar. */
void monster_make_boss(Game *g, Monster *m);
Monster *monster_nearest(Game *g, vec3 p, float max_dist, const Monster *exclude);
/* The element of a region (REG_*): frost north, fire south, lightning east, poison west,
 * holy up, shadow down. */
int region_element(int region);
/* The element that hurts a monster of element e most (fire-frost, lightning-earth,
 * poison-arcane, holy-shadow). */
int element_opposite(int e);
void flow_update(Game *g, PlayerSlot *ps, float dt);
/* Forget that a chunk was populated: monsters and loot are placed again on return. */
void chunk_forget(Game *g, u64 chunk_key);
u64 chunk_spawn_key(int cx, int cy, int cz);
/* Other players as figures in the world, seen from viewer's view. */
void players_draw(Game *g, const PlayerSlot *viewer);

/* combat.c */
void combat_update(Game *g, float dt);
void combat_draw_world(Game *g, vec3 eye);
void combat_draw_viewmodel(Game *g, PlayerSlot *ps);
void combat_collect_lights(Game *g, PlayerSlot *viewer, SceneParams *sp);
void particles_burst(Game *g, vec3 p, vec3 color, int n, float speed, float size, bool emissive);
void flash_add(Game *g, vec3 p, vec3 color, float radius, float life);
float stats_max_hp(const Stats *s);
const char *combat_weapon_name(int w);
float stats_max_mana(const Stats *s);
void stats_refresh(PlayerSlot *ps);

/* Skill tree helpers for other input methods (game.c). */
enum { NAV_LEFT, NAV_RIGHT, NAV_UP, NAV_DOWN, NAV_BUY, NAV_DROP };
void game_skills_nav(PlayerSlot *ps, int nav);
/* Skill under a point of the skill screen (view coordinates): a skill id, -2 / -3 for the
 * previous / next branch arrows of the compact layout, or -1. */
int game_skills_hit(PlayerSlot *ps, int W, int H, float x, float y);
void game_view_rect(const Game *g, const PlayerSlot *ps, int *x, int *y, int *w, int *h);
void game_menu_tap(Game *g, PlayerSlot *ps, int W, int H, float x, float y);
/* Name of the control that opens the skill tree for this player (TAB, Y or SKILLS). */
const char *game_skills_key(const Game *g, const PlayerSlot *ps);

/* menu.c */
void menu_init(StartMenu *m);
void menu_event(StartMenu *m, Game *g, const SDL_Event *e);
void menu_draw(StartMenu *m, Game *g, float dt);
/* Seed from the text: numbers as they are, words hashed (case-insensitive). */
u64 menu_seed(const char *text);
/* Gives an idle gamepad to player 1. */
void game_give_pad_to_p1(Game *g, SDL_JoystickID id);

/* save.c */
bool game_save(Game *g);
bool game_load(Game *g);
bool save_peek(u64 seed, SaveInfo *info);
void save_delete(u64 seed);
bool save_last_seed(char *out, int n);
void save_apply_player(Game *g, PlayerSlot *ps);
void looted_mark(Game *g, u64 key);
void explore_import(Game *g, int cx, int cy, int cz, const u8 *bits);

/* touch.c */
void touch_to_low(Game *g, float nx, float ny, float *x, float *y);
void touch_event(Game *g, const SDL_Event *e);
void touch_input(Game *g, PlayerInput *in);
void touch_draw(Game *g);
/* Low-res width that fills a touch screen of this aspect (no letterbox). */
int touch_low_width(Game *g, int win_w, int win_h);

/* Character menu tabs. */
enum { TAB_INVENTORY, TAB_SPELLS, TAB_SKILLS, TAB_STORY, TAB_SHOP, TAB_FOOD, TAB_ALCHEMY, TAB_FORGE, TAB_COUNT };
enum { SANCTUM_BATH = 1, SANCTUM_FEAST = 2, SANCTUM_LIBRARY = 4 };

/* quests.c: motives, chapters, missions, lore, named enemies */
void story_game_init(Game *g);
void story_update(Game *g, float dt);
bool story_motive_pending(const Game *g, const PlayerSlot *ps);
void story_pick_motive(Game *g, PlayerSlot *ps, int motive);
void story_picker_nav(Game *g, PlayerSlot *ps, int nav);
void story_picker_tap(Game *g, PlayerSlot *ps, int W, int H, float x, float y);
void story_draw_picker(Game *g, PlayerSlot *ps, int W, int H);
void story_tab_nav(Game *g, PlayerSlot *ps, int nav);
void story_draw_tab(Game *g, PlayerSlot *ps, int W, int H, int top);
void story_draw_panel(Game *g, PlayerSlot *ps, int W, int H);
int story_draw_tracker(Game *g, PlayerSlot *ps, int W, int H, int y); /* H 0: only whether it has a line */
void story_draw_plates(Game *g, PlayerSlot *viewer, int W, int H);
void story_map_markers(Game *g, PlayerSlot *ps, float cx, float cy, int pxw, int pyw, int pzw, int px, float x0, float y0, float x1, float y1);
void story_on_kill(Game *g, Monster *m);
void story_orders_found(Game *g, Monster *m);
/* Choices (docs/STORY_CHOICES.md): a chapter target near death yields instead of dying. */
bool story_parley_begin(Game *g, Monster *m);
bool story_parley_open(const PlayerSlot *ps);
void story_parley_nav(Game *g, PlayerSlot *ps, int nav);
void story_parley_tap(Game *g, PlayerSlot *ps, int W, int H, float x, float y);
void story_parley_choose(Game *g, PlayerSlot *ps, int choice);
void story_draw_parley(Game *g, PlayerSlot *ps, int W, int H);
void story_legend_found(Game *g, int id);
bool story_collect_quest(Game *g, PlayerSlot *ps, Pickup *p);
/* A domain lord lives in this chunk: spawn it (from spawn_chunk). */
void story_spawn_lord(Game *g, Chunk *c);
void lore_add(Game *g, int kind, int a, int b, int c);
Monster *story_spawn_named_at(Game *g, const Named *n, vec3 pos);
bool story_cast_thread(Game *g, PlayerSlot *ps);
void story_update_threads(Game *g, float dt);
void story_draw_threads(Game *g, vec3 eye);
void story_thread_lights(Game *g, vec3 eye, SceneParams *sp);

/* mech.c: vaults, puzzles, traps, lava */
void mech_update(Game *g, float dt);
void mech_draw(Game *g, vec3 eye);
void mech_collect_lights(Game *g, vec3 eye, SceneParams *sp);
void mech_draw_hud(Game *g, PlayerSlot *ps, int W, int H);
/* Works the lever, statue or mirror the player faces; false if there is none. */
bool mech_use(Game *g, PlayerSlot *ps);
bool mech_solid(const Game *g, int x, int y, int z);
/* Reveal the Hidden: traps and puzzle parts glow. */
void mech_draw_revealed(Game *g);
/* Flame vents burning right now (centres of the flames); returns the count (sfx.c). */
int mech_burning_flames(Game *g, vec3 *out, int max);
/* Screenshot demos and the scripted test (--demo levers|plates|statues|beam|spikes|flames|darts|mechtest). */
bool mech_demo_view(Game *g, PlayerSlot *ps, int puzzle);
bool mech_demo_trap(Game *g, PlayerSlot *ps, int trap);
bool mech_test_solve(Game *g, PlayerSlot *ps);

/* spells.c */
void spells_give_start(Stats *s);
/* Casts the selected spell; returns false if it could not (mana, cooldown). */
bool spell_cast(Game *g, PlayerSlot *ps);
/* Casts a given known spell (the golden thread has its own key). */
bool spell_cast_id(Game *g, PlayerSlot *ps, int id);
void spell_next(PlayerSlot *ps, int dir);
void spells_update(Game *g, float dt);
void spells_draw_world(Game *g);
void spell_projectile_hit(Game *g, Projectile *p, Monster *m);
void spell_collect_lights(Game *g, vec3 eye, SceneParams *sp);
/* Damage with an element: the hit plus its effect (burn, chill, shock...). */
void element_hit(Game *g, Monster *m, int element, float dmg, vec3 from, float knock, int attacker);
/* Learn a spell or raise its rank; returns the new rank. */
int spell_learn(PlayerSlot *ps, int id);

/* weapons.c: weapon models, their glow and their impact effects */
vec3 weapon_glow_color(const Item *it);
void weapon_draw_view(Game *g, PlayerSlot *ps, vec3 light);
void weapon_draw_held(const Item *it, vec3 hand, vec3 dir, vec3 side, vec3 light, float time, float seed);
void weapon_draw_ground(const Item *it, vec3 c, float yaw, vec3 light, float time, float seed);
/* Sparks, frost, arcs, venom or a blood mist where a magical weapon hits (rarity > common or an element). */
void weapon_impact_fx(Game *g, int element, int rarity, bool vampiric, vec3 at, int owner);

/* utility.c: the utility spells beyond the first seven */
bool utility_cast(Game *g, PlayerSlot *ps, int u, int rank);
void utility_update(Game *g, float dt);
void utility_draw_world(Game *g, PlayerSlot *viewer);
void utility_lights(Game *g, vec3 eye, SceneParams *sp);
void utility_draw_hud(Game *g, PlayerSlot *ps, int W, int H);
/* Sets the player's movement from the spells on them, before player_update. */
void utility_player_physics(Game *g, PlayerSlot *ps);
/* Collision for a player's movement (water is ground for a strider). Call utility_physics_player first. */
int player_solid(void *user, int x, int y, int z);
void utility_physics_player(PlayerSlot *ps);
/* Cells only monsters cannot pass (the gate of a Warden's Keep). */
bool utility_barrier(const Game *g, int x, int y, int z);
/* How fast the world runs for monsters and their shots (Time Slip). */
float utility_time_scale(const Game *g);
/* A player attacked: invisibility ends. */
void utility_player_attacked(PlayerSlot *ps);
void utility_ritual_break(Game *g, PlayerSlot *ps); /* a hit ends the Sanctum Door's ritual */
/* Where a player's view is (Clairvoyance moves it away from the body). */
vec3 game_view_eye(const PlayerSlot *ps);
/* A monster struck its decoy or charmed foe instead of a player: true if handled. */
bool utility_monster_strike(Game *g, Monster *m, float dmg);
/* spells.c: a lasting spell effect (utility spells use it too) */
Effect *effect_new(Game *g, int type, int element, int owner, vec3 pos, float life, float dmg, float radius);

/* realm.c: the pocket realm behind the Sanctum Door */
bool realm_has(const vec3 pos);
int sanctum_stage(const Game *g, int plot);          /* SP_* stage of the world's Sanctum */
void sanctum_set_stage(Game *g, int plot, int stage);
void sanctum_add_decor(Game *g, int d);
void sanctum_trophy_unique(Game *g, int id);
void sanctum_trophy_named(Game *g, u64 key);
/* shop.c: the SHOP tab */
bool shop_open_here(Game *g, const PlayerSlot *ps);
void shop_nav(Game *g, PlayerSlot *ps, int nav);
void shop_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y);
void shop_draw(Game *g, PlayerSlot *ps, int W, int H, int top);
enum { SH_SCRAP, SH_POTION_HP, SH_POTION_MP, SH_RANK, SH_REROLL, SH_RELIC, SH_BUILD, SH_SCROLL, SH_STORE, SH_TAKE, SH_MERCHANT, SH_KEYSTONE, SH_PAGE, SH_COMPLETE, SH_RECIPE, SH_SEALED, SH_DECOR, SH_SUPPLY, SH_TOWN };
bool shop_buy_reagent(Game *g, PlayerSlot *ps, int rg);
bool shop_buy_kind(Game *g, PlayerSlot *ps, int kind);
bool shop_buy_arg(Game *g, PlayerSlot *ps, int kind, int arg);
/* game.c: the Golden Thread key (right after a death: back where you fell) */
void game_thread_key(Game *g, PlayerSlot *ps);

/* gate.c: the realms behind the Gate (realms.h, docs/REALMS.md) */
void monsters_alert(Game *g, vec3 p, float radius);
/* king.c: the Old King and the way to him (king.h, docs/OLD_KING.md) */
void king_update(Game *g, float dt);
bool king_use(Game *g, PlayerSlot *ps);         /* the throne: speak to him */
bool king_town_talk(Game *g, PlayerSlot *ps, struct Folk *f, u64 town_key); /* the herald, the brew */
bool king_folk_talk(Game *g, PlayerSlot *ps, struct Folk *f); /* the witness, the allies */
void king_rest(Game *g, PlayerSlot *ps);        /* the Sanctum's bed: the vision */
void king_on_kill(Game *g, Monster *m);
float king_damage(Game *g, Monster *m, float dmg);
void king_provoked(Game *g, Monster *m);
void king_draw(Game *g, PlayerSlot *viewer);
int king_draw_tracker(Game *g, PlayerSlot *ps, int W, int H, int y); /* lines drawn */
void king_draw_credits(Game *g, PlayerSlot *ps, int W, int H);
void king_heir(Game *g);
bool king_raids_on(Game *g);
bool king_bot_target(Game *g, PlayerSlot *ps, vec3 *at, bool *talk);
bool king_bot_home(Game *g);
enum { KN_DRAGON = 1, KN_STEEL = 2, KN_FORGE = 4, KN_ORCHID = 8 }; /* what the bot lacks for the open steps */
int king_bot_needs(Game *g, PlayerSlot *ps);
bool king_thread_goal(Game *g, PlayerSlot *ps, vec3 *at, char *name, int n);
int king_raid(Game *g, vec3 at, int level, int n, u64 key); /* raiders (king 4) */
bool king_faltering(const Monster *m);
void king_lore_text(Game *g, int a, char *out, int n);
bool king_won(const Game *g);
/* dragon.c: dragons (dragons.h, docs/DRAGONS.md) */
Monster *dragon_spawn(Game *g, int kind, int size, vec3 pos, int level, u64 key);
bool dragon_spawn_chunk(Game *g, Chunk *c);     /* a lair's dragon and hoard; true: nothing else spawns here */
bool dragon_pack(Game *g, Chunk *c, u32 seed);  /* now and then a pack of drakes, dragonets or wyverns */
void dragon_update(Game *g, Monster *m, float dt);
float dragon_damage(Game *g, Monster *m, float dmg, vec3 from, int attacker);
bool dragon_rises(Game *g, Monster *m);         /* a dracolich at no health: it rises again */
void dragon_slain(Game *g, Monster *m);
bool dragon_lair_near(Game *g, vec3 from, int max_level, vec3 *at, int *level, u64 *key); /* a living great dragon's lair */
void dragon_draw(Game *g, const Monster *m, vec3 light);
void dragons_shake(Game *g, vec3 p, float amount, float radius);
/* town.c: the towns of the friendly peoples (towns.h, docs/TOWNS.md) */
#define MAX_FOLK 48
typedef struct Folk Folk;
void town_spawn_chunk(Game *g, Chunk *c);       /* its people, when the plaza's chunk comes */
bool town_chunk(const Chunk *c);                /* a chunk of a town: no monsters, no digging */
void town_update(Game *g, float dt);
void town_draw(Game *g, PlayerSlot *viewer);
bool town_use(Game *g, PlayerSlot *ps);         /* talk to whoever is in front of you */
void town_on_kill(Game *g, Monster *m);
int town_draw_tracker(Game *g, PlayerSlot *ps, int W, int H, int y);
void game_next_track(Game *g, PlayerSlot *ps);
int town_market(Game *g, PlayerSlot *ps, int *special); /* the people whose market you stand in, or -1; which of its special wares are open to you (bits) */
u64 town_special_key(Game *g, PlayerSlot *ps, int ware);
bool town_buy(Game *g, PlayerSlot *ps, int ware);
void gate_init(Game *g);                       /* the realms joined to this world */
void gate_update(Game *g, float dt);           /* travel, rifts, each realm's rules */
void gate_draw(Game *g, PlayerSlot *viewer);   /* the Gate in the Sanctum, the arches home, rifts */
bool gate_use(Game *g, PlayerSlot *ps);        /* the Gate's dial; the orrery's moons */
bool gate_go(Game *g, PlayerSlot *ps, int realm, vec3 from, float yaw);
vec3 gate_ring_pos(void);
void gate_go_home(Game *g, PlayerSlot *ps); /* out of a realm as through its arch (the bot) */
int gate_dialed(const Game *g);
void gate_spawn_chunk(Game *g, Chunk *c);      /* rifts in the fortress, rulers in their keeps */
void gate_ruler_slain(Game *g, Monster *m);
void gate_flora_broken(Game *g, PlayerSlot *ps, Block b, int x, int y, int z, u64 h);
void gate_fauna_speaks(Game *g, PlayerSlot *ps, int species, u64 key);
void gate_note_fauna(Game *g, int species);
bool gate_no_air(Game *g, PlayerSlot *ps);     /* airless here: breath runs out as under water */
float gate_gravity(Game *g, PlayerSlot *ps);
float gate_temperature(int realm);
bool gate_open(const Game *g, int realm, const char **why); /* not sealed by the story */
void gate_lore_text(Game *g, int a, int b, char *out, int n);
void realm_enter(Game *g, PlayerSlot *ps, vec3 from, float yaw);
void realm_update(Game *g, float dt);
void realm_draw(Game *g);
/* utility.c: a door frame with a glowing way through, facing yaw (home: the way back) */
void utility_draw_door(Game *g, vec3 p, float yaw, float open, bool home);

/* items.c */
void items_give_start(Stats *s);
void pickups_update(Game *g, float dt);
void pickups_draw(Game *g, vec3 eye);
void pickup_spawn(Game *g, int kind, vec3 pos, int amount, const Item *item, u64 key, u64 chunk_key);
Pickup *pickup_find_key(Game *g, u64 key); /* a placed pickup still lying there */
void loot_drop(Game *g, vec3 pos, float level, bool elite, float luck);
void loot_place_in_chunk(Game *g, Chunk *c);
void pickups_despawn_chunk(Game *g, u64 chunk_key);
bool looted_has(Game *g, u64 key);
void inventory_tap(Game *g, PlayerSlot *ps, int cell);
void spellbook_tap(Game *g, PlayerSlot *ps, int id);
Item item_roll(u64 seed, float level, int min_rarity);
const WeaponBase *equipped_base(const Stats *s, int wclass);
void drink_potion(Game *g, PlayerSlot *ps, bool mana);
void inventory_draw(Game *g, PlayerSlot *ps, int W, int H, int top);
void spellbook_draw(Game *g, PlayerSlot *ps, int W, int H, int top);
/* Menu input for the inventory and spell tabs. */
void inventory_nav(Game *g, PlayerSlot *ps, int nav);
void spellbook_nav(Game *g, PlayerSlot *ps, int nav);
int inventory_hit(PlayerSlot *ps, int W, int H, int top, float x, float y);
int spellbook_hit(PlayerSlot *ps, int W, int H, int top, float x, float y);
float relic_bonus(const Stats *s, int bonus);

/* food.c: the larder, cooking, meals */
float food_fx(const Stats *s, int effect);          /* ME_* from the meal, a snack and the feast */
int food_cook_place(Game *g, const PlayerSlot *ps); /* CP_* where the player stands */
void food_give(Game *g, PlayerSlot *ps, int food, int n);
bool food_cook(Game *g, PlayerSlot *ps);            /* cooks what is in the pot */
bool food_pot_add(Game *g, PlayerSlot *ps, int food);
bool food_eat_dish(Game *g, PlayerSlot *ps, int recipe, int q, int place);
bool food_eat_best(Game *g, PlayerSlot *ps);        /* the quick key: the best dish held */
bool food_eat_raw(Game *g, PlayerSlot *ps, int food);
void food_feast(PlayerSlot *ps);                    /* the Sanctum's feast replaces a meal */
void food_update(Game *g, float dt);
void food_nav(Game *g, PlayerSlot *ps, int nav);
void food_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y);
void food_draw(Game *g, PlayerSlot *ps, int W, int H, int top);
void food_draw_model(int food, vec3 c, float yaw, float s, vec3 light);
int food_held(const Stats *s);                      /* ingredients in the larder */
int food_dishes_held(const Stats *s);

/* alchemy.c: reagents, formulae, elixirs */
float alchemy_fx(const Stats *s, int effect);       /* ME_* from the elixir working (food_fx adds it) */
int alchemy_station(Game *g, const PlayerSlot *ps); /* 0 none, 1 a shrine or home, 2 home with the library's alembic */
void alchemy_give(Game *g, PlayerSlot *ps, int rg, int n);
bool alchemy_learn(Game *g, PlayerSlot *ps, int r, bool found);
bool alchemy_learn_torn(Game *g, PlayerSlot *ps, int r);
int healing_held(const Stats *s); /* health potions and Draughts of Vigor together (one cap) */
int alchemy_belt(const Stats *s);  /* elixirs carried, Vigor aside (ELIXIR_BELT at most) */
int alchemy_random_formula(const Stats *s, u64 seed);
bool alchemy_add(Game *g, PlayerSlot *ps, u8 step);
bool alchemy_bottle(Game *g, PlayerSlot *ps);
bool alchemy_drink(Game *g, PlayerSlot *ps, int r);
bool alchemy_drink_quick(Game *g, PlayerSlot *ps);
bool alchemy_revive(Game *g, PlayerSlot *ps);
void alchemy_update(Game *g, float dt);
void alchemy_monster_drop(Game *g, const Monster *m);
void alchemy_world_weights(const ChunkInfo *ci, float *w);
void alchemy_nav(Game *g, PlayerSlot *ps, int nav);
void alchemy_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y);
void alchemy_draw(Game *g, PlayerSlot *ps, int W, int H, int top);
void alchemy_draw_reagent(int rg, vec3 c, float yaw, float s, vec3 light);
void alchemy_draw_formula(vec3 c, float yaw, vec3 light);

/* forge.c: smithing */
bool forge_here(Game *g, const PlayerSlot *ps);        /* at the anvil in the Sanctum's forge */
void forge_give(Game *g, PlayerSlot *ps, int m, int n);
void forge_tick(Game *g, float dt);                    /* the session at the anvil (runs while the menu is open) */
void forge_nav(Game *g, PlayerSlot *ps, int nav);
void forge_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y);
void forge_draw(Game *g, PlayerSlot *ps, int W, int H, int top);
void forge_draw_mat(int m, vec3 c, float yaw, vec3 light);
void forge_monster_drop(Game *g, const Monster *m);
bool forge_mine(Game *g, PlayerSlot *ps, int x, int y, int z); /* a blow on an ore vein */

/* dig.c: breaking the world (docs/DESTRUCTION.md) */
bool dig_protected(Game *g, int x, int y, int z);
/* Breaks the pieces within r of p that yield to power (HARD_*); returns how many. */
int world_break_sphere(Game *g, PlayerSlot *ps, vec3 p, float r, int power);
void world_explode(Game *g, vec3 p, float radius, int power, float dmg, int owner);
void bomb_throw(Game *g, PlayerSlot *ps);
void throw_buffer_update(Game *g, PlayerSlot *ps, float dt); /* a throw pressed a moment early */
void throw_selected(Game *g, PlayerSlot *ps);     /* X: the bomb or flask chosen with C */
void throw_cycle(PlayerSlot *ps);                 /* C */
bool camp_light(Game *g, PlayerSlot *ps);         /* P: a campfire from firewood, in front of you */
const char *throw_name(int kind);
void bio_tonic(Game *g, vec3 p, float r);         /* a growth tonic breaks: plants spring up */
void bombs_update(Game *g, float dt);
void bombs_draw(Game *g);
/* weight and collapse (dig.c, support.c): cells that stopped bearing load are noted, then checked */
void collapse_note(int x, int y, int z, int mat);
void collapse_check(Game *g, int x, int y, int z, int r);
void collapse_update(Game *g, float dt);
void collapse_draw(Game *g);
void collapse_reset(void);
int collapse_pending(void);
void dig_swing(Game *g, PlayerSlot *ps);
void dig_update(Game *g, PlayerSlot *ps, bool attack, bool alt);
const char *dig_pick_name(const Stats *s);
/* The next or previous of the four slots: blade, bow, staff, pickaxe. */
void game_cycle_weapon(PlayerSlot *ps, int dir);

/* sim.c: water that flows (docs/SIMULATION.md) */
void sim_init(Game *g);
void sim_reset(void);
void sim_wake(int x, int y, int z);
void sim_update(Game *g, float dt);
int sim_fluid(Game *g, int x, int y, int z);
int sim_lava(Game *g, int x, int y, int z); /* lava level 1..8, 0 if none */
float sim_water_depth(Game *g, vec3 feet);
int sim_pending(void);
int sim_flow_near(Game *g, vec3 p, float r); /* cells of water that moved in the last 2 s near p */
vec3 sim_current(Game *g, vec3 p);           /* the pull of running water at p (m/s) */
int sim_gas_list(float *out, int max); /* 6 floats each: x, y, z, amount, radius, kind */
void sim_gas_restore(const float *in);
enum { GAS_STEAM, GAS_SMOKE, GAS_POISON, GAS_SWAMP, GAS_SPORES, GAS_COUNT };
void sim_gas(Game *g, int kind, vec3 p, float amount);
float sim_gas_at(vec3 p, int kind);
void sim_gas_fire(Game *g, vec3 p, float r); /* fire reaches p: swamp gas there explodes */
/* An element strikes the world at p: fire lights and melts, frost freezes water, lightning runs
 * through water, poison leaves a cloud. */
void sim_element_hit(Game *g, vec3 p, int el, int owner, float dmg);
bool cell_occupied(Game *g, int x, int y, int z); /* a player or monster in the cell (utility.c) */
bool sim_ignite(Game *g, int x, int y, int z);    /* sets the cell alight if it burns */

/* heat.c: temperature (docs/SIMULATION.md) */
void heat_reset(void);
void heat_update(Game *g, float dt);
float heat_cell(Game *g, int x, int y, int z);
float heat_at(Game *g, vec3 p);
void heat_add(Game *g, vec3 p, float degrees);
float heat_base_theme(int theme);
int heat_chunks(void);
void heat_mark(int x, int y, int z);

/* bio.c: living things (docs/SIMULATION.md) */
void bio_update(Game *g, float dt);
void bio_reset(void);
void bio_on_death(Game *g, vec3 p, u64 h);   /* bones where something died */
void bio_hive_broken(Game *g, vec3 p);       /* angry bees */
int bio_grown(int block);                    /* cells grown since start (demo, tests) */
int bio_withered(void);
int bio_swarm_list(float *out, int max);
void bio_swarm_restore(const float *in);
void bio_catch_up_begin(Game *g, int cx, int cy, int cz, float dt);
bool bio_catch_up_run(Game *g, int budget);
bool sim_catch_up_fires(Game *g, int cx, int cy, int cz, float dt);

/* away.c: the world goes on while you are away (docs/SIMULATION.md) */
void away_reset(void);
void away_update(Game *g, float dt);
int away_pending(void);
struct Eco;
int away_list(int *xyz, float *t, struct Eco *eco, int max);
void away_restore(int cx, int cy, int cz, float t, const struct Eco *eco);
/* populations (away.c, eco.c; docs/SIMULATION.md) */
enum { ECO_GRAZER_TAKEN, ECO_BODY };
struct Eco *eco_at(int cx, int cy, int cz);
void eco_note(vec3 p, int what);
float eco_prey_factor(int cx, int cy, int cz);
float eco_grazer_factor(int cx, int cy, int cz);
bool monster_hunts_here(int stratum);        /* hounds or bats live in this stratum */
bool critter_grazes(int kind);
void wildlife_taken(Game *g, Critter *c);    /* a hound or bat caught it */
void away_pretend(float seconds);
bool forge_start(Game *g, PlayerSlot *ps, bool reforge);
void forge_strike(Game *g, PlayerSlot *ps);
void forge_bellows(Game *g, PlayerSlot *ps);
bool forge_smelt(Game *g, PlayerSlot *ps, int ore);
bool forge_salvage(Game *g, PlayerSlot *ps, int bag_slot);
bool forge_armour(Game *g, PlayerSlot *ps);       /* make dragon-scale armour at the anvil */
float forge_armour_taken(const Stats *s);          /* the share of a blow it takes */
void forge_armour_name(const Stats *s, char *out, int n);
void forge_window(const PlayerSlot *ps, float *lo, float *hi);
int forge_region_ore(Game *g, int x, int y, int z, u64 h); /* FG_* ore of the region round a cell */

/* wildlife.c: animals that do not fight */
void wildlife_spawn_chunk(Game *g, Chunk *c);
void wildlife_spawn_home(Game *g);
Critter *wildlife_spawn(Game *g, int kind, vec3 pos); /* demos */                  /* the Sanctum garden's hens, hives and fish */
void wildlife_update(Game *g, float dt);
void wildlife_draw(Game *g, PlayerSlot *viewer);
void wildlife_despawn_far(Game *g);
/* A blow from a player: true if it struck an animal. */
bool wildlife_strike(Game *g, PlayerSlot *ps, vec3 eye, vec3 fwd, float range, float arc, float dmg);
/* A projectile or blast at p: true if it hit an animal (radius 0: a point). */
bool wildlife_hit(Game *g, vec3 p, float radius, float dmg, int element, int owner);
/* The attack key next to a goat with milk: milks it. */
bool wildlife_use(Game *g, PlayerSlot *ps);
Critter *wildlife_nearest(Game *g, vec3 p, float max_dist, bool huntable);

/* explore.c */
void explore_update(Game *g, PlayerSlot *ps, float dt);
bool explore_get(Game *g, int x, int y, int z);
/* Map of the Mind: everything open in the loaded chunks within r chunks of p counts as seen. */
void explore_reveal_area(Game *g, vec3 p, int r);
/* The layer a feet height is on, and its name ("LAYER 3 BELOW"). */
int layer_of(float feet_y);
void layer_name(int layer, char *out, int n);
void explore_draw_map(Game *g, PlayerSlot *ps, int w, int h);

#endif
