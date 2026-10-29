#ifndef FC_PLAYER_H
#define FC_PLAYER_H

#include "core/fmath.h"

/* Returns 1 for solid, 0 for free, -1 for not loaded yet (treated as solid). */
typedef int (*SolidFn)(void *user, int x, int y, int z);

enum { STANCE_STAND, STANCE_CROUCH, STANCE_CRAWL };

typedef struct PlayerInput {
    float move_x, move_z; /* strafe / forward, -1..1 */
    bool jump, crouch, sprint, up, down;
} PlayerInput;

typedef struct Player {
    vec3 pos; /* feet, centre */
    vec3 vel;
    float yaw, pitch;
    bool fly, on_ground;
    int stance;
    float eye_offset; /* smoothing for step-ups and stance changes */
    float bob;
} Player;

void player_init(Player *p, vec3 pos, float yaw);
void player_update(Player *p, const PlayerInput *in, float dt, SolidFn solid, void *user);
vec3 player_eye(const Player *p);
float player_height(int stance);

#endif
