#ifndef FC_GAME_H
#define FC_GAME_H

#include <SDL3/SDL.h>

#include "core/fmath.h"
#include "game/items.h"
#include "game/physics.h"
#include "game/player.h"
#include "game/sfx.h"
#include "game/skills.h"
#include "render/renderer.h"
#include "world/chunkmap.h"
#include "world/layout.h"
#include "world/pocket.h"
#include "game/tele.h"
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
    char name[STORY_NAME];
} Monster;

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
enum { LORE_TRACE = 1, LORE_REVEAL };
#define MAX_LORE 512
typedef struct LoreEntry {
    u8 kind;
    int a, b, c; /* the trace's chunk, or the chapter */
    char *text;  /* generated on first display (not saved) */
} LoreEntry;

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
    float refreshed_t, well_fed_t, rest_t, feast_t;
    u8 sanctum;          /* SANCTUM_*: the rooms bought back to life (shop.c) */
    float library_t;     /* until the library's next scroll */
    /* dying and coming back: the last safe ground, and a short time to return there */
    vec3 safe_pos, fell_pos;
    float fell_t;
    vec3 recall_from;    /* Recall cast twice within 5 s goes back */
    float recall_t;
    float fall_speed; /* fastest downward speed since leaving the ground (fall damage) */
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
    char reading[STORY_TEXT]; /* the trace you stand at */
    u64 reading_key;
    float reading_t;
    char msg[STORY_TEXT], msg_next[STORY_TEXT]; /* story messages (chapters) */
    float msg_t;
    u64 domain_id;
    int domain_cx, domain_cy, domain_cz;
    char domain_name[STORY_NAME];
    int motive_sel, story_scroll;
    int shop_sel;
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
    float fog_density, stars, openness, openness_timer;
    float explore_timer;
    /* flow field toward this player, followed by the monsters chasing them */
    u16 *flow;
    u8 *flow_solid;
    int flow_ox, flow_oy, flow_oz;
    float flow_timer;
    int flow_px, flow_py, flow_pz;
} PlayerSlot;

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
    bool world_started;
    bool freeze_monsters; /* screenshot demos: monsters stand still */
    /* utility spells (utility.c): timed changes to the world, cells only monsters cannot pass */
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
enum { TAB_INVENTORY, TAB_SPELLS, TAB_SKILLS, TAB_STORY, TAB_SHOP, TAB_COUNT };
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
void story_draw_tracker(Game *g, PlayerSlot *ps, int W, int H, int y);
void story_draw_plates(Game *g, PlayerSlot *viewer, int W, int H);
void story_map_markers(Game *g, PlayerSlot *ps, float cx, float cy, int pxw, int pyw, int pzw, int px, float x0, float y0, float x1, float y1);
void story_on_kill(Game *g, Monster *m);
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
/* Where a player's view is (Clairvoyance moves it away from the body). */
vec3 game_view_eye(const PlayerSlot *ps);
/* A monster struck its decoy or charmed foe instead of a player: true if handled. */
bool utility_monster_strike(Game *g, Monster *m, float dmg);
/* spells.c: a lasting spell effect (utility spells use it too) */
Effect *effect_new(Game *g, int type, int element, int owner, vec3 pos, float life, float dmg, float radius);

/* realm.c: the pocket realm behind the Sanctum Door */
bool realm_has(const vec3 pos);
/* shop.c: the SHOP tab */
bool shop_open_here(Game *g, const PlayerSlot *ps);
void shop_nav(Game *g, PlayerSlot *ps, int nav);
void shop_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y);
void shop_draw(Game *g, PlayerSlot *ps, int W, int H, int top);
enum { SH_SCRAP, SH_POTION_HP, SH_POTION_MP, SH_RANK, SH_REROLL, SH_RELIC, SH_BATH, SH_FEAST, SH_LIBRARY, SH_SCROLL };
bool shop_buy_kind(Game *g, PlayerSlot *ps, int kind);
/* game.c: the Golden Thread key (right after a death: back where you fell) */
void game_thread_key(Game *g, PlayerSlot *ps);
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
