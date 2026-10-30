#ifndef FC_GAME_H
#define FC_GAME_H

#include <SDL3/SDL.h>

#include "core/fmath.h"
#include "game/physics.h"
#include "game/player.h"
#include "game/skills.h"
#include "render/renderer.h"
#include "world/chunkmap.h"
#include "world/layout.h"

#define MAX_MONSTERS 160
#define MAX_PROJECTILES 256
#define MAX_PARTICLES 1600
#define MAX_FLASHES 16
#define MAX_SHRINES 128
#define MAX_PLAYERS 4

/* Flow field around the player that monsters follow (cells). */
#define FLOW_R 24
#define FLOW_H 8
#define FLOW_W (2 * FLOW_R)
#define FLOW_Y (2 * FLOW_H)

enum { MON_CRAWLER, MON_BRUTE, MON_WISP, MON_HORNED, MON_TYPE_COUNT };
enum { MS_IDLE, MS_CHASE, MS_WINDUP, MS_CHARGE, MS_STUN };
enum { WPN_SWORD, WPN_BOW, WPN_STAFF, WPN_COUNT };
enum { PJ_ARROW, PJ_BOLT, PJ_NOVA, PJ_ENEMY_FIRE };

typedef struct MonsterDef {
    const char *name;
    float hw, h, hp, dmg, speed, reach, attack_time, xp;
    bool flies;
    vec3 color;
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
    vec3 respawn;
    float respawn_yaw;
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
} Options;

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
    /* ui */
    bool show_map, show_skills;
    int skill_sel;
    char banner[128];
    float banner_time;
    char place[128];
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

    /* chunks whose monsters and shrines have been placed */
    u64 *spawned;
    int spawned_cap, spawned_count;
    int mons_anim_seed;
    vec3 shrines[MAX_SHRINES];
    int nshrines;

    Explored explored;

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
void game_hurt_player(Game *g, PlayerSlot *ps, float dmg, vec3 from);
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
void monster_spawn(Game *g, int type, vec3 pos, int level, bool elite, vec3 tint, u64 key);
Monster *monster_nearest(Game *g, vec3 p, float max_dist, const Monster *exclude);
void flow_update(Game *g, PlayerSlot *ps, float dt);
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

/* explore.c */
void explore_update(Game *g, PlayerSlot *ps, float dt);
bool explore_get(Game *g, int x, int y, int z);
void explore_draw_map(Game *g, PlayerSlot *ps, int w, int h);

#endif
