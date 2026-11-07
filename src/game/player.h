#ifndef FC_PLAYER_H
#define FC_PLAYER_H

#include "core/fmath.h"
#include "game/physics.h"

enum { STANCE_STAND, STANCE_CROUCH, STANCE_CRAWL };

typedef struct PlayerInput {
    float move_x, move_z; /* strafe / forward, -1..1 */
    bool jump, crouch, sprint, up, down;
} PlayerInput;

/* How the player moves (utility spells): walking, levitating, flying. */
enum { MOVE_WALK, MOVE_LEVITATE, MOVE_FLY };

typedef struct Player {
    vec3 pos; /* feet, centre */
    vec3 vel;
    float yaw, pitch;
    bool fly, on_ground;
    int stance;
    float eye_offset; /* smoothing for step-ups and stance changes */
    float bob;
    float speed_mult; /* from skills */
    /* set every tick by the game from the spells on the player */
    int move_mode;    /* MOVE_* */
    float max_fall;   /* fastest fall (feather fall: slow) */
    float jump_mult;  /* a steed jumps higher */
    bool climbing;    /* spider climb: pushing into a wall climbs it */
} Player;

void player_init(Player *p, vec3 pos, float yaw);
void player_update(Player *p, const PlayerInput *in, float dt, SolidFn solid, void *user);
vec3 player_eye(const Player *p);
float player_height(int stance);
#define PLAYER_HALF_W 0.3f

#endif
