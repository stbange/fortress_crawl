#include "game/player.h"

#define HALF_W PLAYER_HALF_W
#define EPS 0.001f

static const float HEIGHTS[3] = { 1.75f, 1.3f, 0.8f };
static const float EYES[3] = { 1.6f, 1.15f, 0.62f };
static const float SPEEDS[3] = { 6.0f, 3.2f, 2.0f };

float player_height(int stance) { return HEIGHTS[stance]; }

void player_init(Player *p, vec3 pos, float yaw) {
    memset(p, 0, sizeof *p);
    p->pos = pos;
    p->yaw = yaw;
    p->speed_mult = 1.0f;
    p->max_fall = 40.0f;
    p->jump_mult = 1.0f;
    p->gravity = 1.0f;
}

/* Lowest stance that fits at pos (standing preferred). */
static int fitting_stance(vec3 pos, int min_stance, SolidFn solid, void *user) {
    for (int s = min_stance; s <= STANCE_CRAWL; s++)
        if (phys_box_free(pos, HALF_W, HEIGHTS[s], solid, user)) return s;
    return STANCE_CRAWL;
}

void player_update(Player *p, const PlayerInput *in, float dt, SolidFn solid, void *user) {
    vec3 fwd = v3(cosf(p->yaw), 0, sinf(p->yaw));
    vec3 right = v3(-sinf(p->yaw), 0, cosf(p->yaw));
    vec3 wish = v3_add(v3_scale(fwd, in->move_z), v3_scale(right, in->move_x));
    float wl = v3_len(wish);
    if (wl > 1.0f) wish = v3_scale(wish, 1.0f / wl);

    if (p->fly) {
        float speed = in->sprint ? 60.0f : 18.0f;
        vec3 look = dir_from_angles(p->yaw, p->pitch);
        vec3 v = v3_add(v3_scale(look, in->move_z), v3_scale(right, in->move_x));
        if (in->up) v.y += 1.0f;
        if (in->down) v.y -= 1.0f;
        p->pos = v3_add(p->pos, v3_scale(v, speed * dt));
        p->vel = v3(0, 0, 0);
        p->stance = STANCE_STAND;
        p->eye_offset = 0;
        return;
    }

    if (p->move_mode == MOVE_FLY) {
        /* wings: fly where you look, with collision; jump climbs, crouch sinks */
        vec3 look = dir_from_angles(p->yaw, p->pitch);
        vec3 w = v3_add(v3_scale(look, in->move_z), v3_scale(right, in->move_x));
        if (in->jump) w.y += 1.0f;
        if (in->crouch) w.y -= 1.0f;
        float wl3 = v3_len(w);
        if (wl3 > 1.0f) w = v3_scale(w, 1.0f / wl3);
        float sp = (in->sprint ? 14.0f : 9.0f) * p->speed_mult;
        p->vel = v3_lerp(p->vel, v3_scale(w, sp), fminf(1.0f, 6.0f * dt));
        int st = fitting_stance(p->pos, STANCE_STAND, solid, user);
        if (st != p->stance) { p->eye_offset += EYES[p->stance] - EYES[st]; p->stance = st; }
        for (int axis = 0; axis < 3; axis++) {
            float d = (axis == 0 ? p->vel.x : axis == 1 ? p->vel.y : p->vel.z) * dt;
            if (!phys_move_axis(&p->pos, axis, d, HALF_W, HEIGHTS[p->stance], solid, user)) {
                if (axis == 0) p->vel.x = 0; else if (axis == 1) p->vel.y = 0; else p->vel.z = 0;
            }
        }
        p->on_ground = !phys_box_free(v3_add(p->pos, v3(0, -0.05f, 0)), HALF_W, 0.05f, solid, user);
        p->eye_offset *= expf(-12.0f * dt);
        return;
    }
    bool lev = p->move_mode == MOVE_LEVITATE;

    /* stance: crouch on request, otherwise stand up as soon as there is room */
    int want = in->crouch && !lev ? STANCE_CROUCH : STANCE_STAND;
    int st = fitting_stance(p->pos, want, solid, user);
    if (st != p->stance) {
        p->eye_offset += EYES[p->stance] - EYES[st];
        p->stance = st;
    }
    float h = HEIGHTS[p->stance];

    /* in water: wading slows you; deeper than the waist you swim */
    bool swim = !lev && p->water > 0.9f;
    float speed = SPEEDS[p->stance] * p->speed_mult * (in->sprint && p->stance == STANCE_STAND ? 1.6f : 1.0f);
    if (swim) speed *= 0.55f;
    else if (p->water > 0.3f) speed *= 0.75f;
    float accel = swim ? 6.0f : p->on_ground ? 14.0f : 3.0f;
    p->vel.x += (wish.x * speed - p->vel.x) * fminf(1.0f, accel * dt);
    p->vel.z += (wish.z * speed - p->vel.z) * fminf(1.0f, accel * dt);
    if (lev) {
        /* levitate: no weight; jump rises, crouch sinks */
        float vy = in->jump ? 3.5f : in->crouch ? -3.5f : 0.0f;
        p->vel.y += (vy - p->vel.y) * fminf(1.0f, 5.0f * dt);
    } else if (p->on_vine && !p->on_ground) {
        /* hanging on a vine: jump climbs, crouch slides down, else you hold on */
        float vy = in->jump ? 3.2f : in->crouch ? -3.0f : 0.0f;
        p->vel.y += (vy - p->vel.y) * fminf(1.0f, 8.0f * dt);
    } else if (p->on_vine && in->jump) {
        p->vel.y = 3.2f; /* from the ground up onto the vine */
    } else if (swim) {
        /* buoyant: float with the eyes at the surface; jump swims up, crouch dives */
        float vy = in->jump ? 3.2f : in->crouch ? -3.0f : p->water > 1.5f ? 0.9f : p->water < 1.3f ? -0.6f : 0.0f;
        p->vel.y += (vy - p->vel.y) * fminf(1.0f, 4.0f * dt);
    } else {
        /* a forgiving jump (openspec player-input): 100 ms of coyote time after leaving the ground,
         * a press up to 120 ms before landing kept, and a hang at the top while jump is held */
        bool pressed = in->jump && !p->jump_prev;
        if (p->on_ground) p->coyote = 0.1f;
        else if (p->coyote > 0) p->coyote -= dt;
        if (pressed) p->jump_buf = 0.12f;
        else if (p->jump_buf > 0) p->jump_buf -= dt;
        float grav = 25.0f * (p->gravity > 0 ? p->gravity : 1.0f);
        if (!p->on_ground && in->jump && fabsf(p->vel.y) < 1.5f) grav *= 0.6f;
        p->vel.y -= grav * dt;
        if (p->vel.y < -p->max_fall) p->vel.y = -p->max_fall;
        bool may = (p->on_ground || p->coyote > 0) && p->stance == STANCE_STAND;
        if (may && ((in->jump && p->on_ground) || p->jump_buf > 0 || (pressed && p->coyote > 0))) {
            p->vel.y = 8.2f * p->jump_mult;
            p->coyote = 0;
            p->jump_buf = 0;
        }
    }

    p->jump_prev = in->jump;
    for (int axis = 0; axis < 3; axis += 2) {
        float d = (axis == 0 ? p->vel.x : p->vel.z) * dt;
        vec3 before = p->pos;
        if (phys_move_axis(&p->pos, axis, d, HALF_W, h, solid, user)) continue;
        /* blocked: try stepping up one block, then try ducking into low openings */
        vec3 blocked = p->pos;
        p->pos = before;
        bool done = false;
        if (p->on_ground || swim) { /* (a swimmer climbs out at the edge) */
            vec3 up = v3_add(before, v3(0, 1.0f + EPS, 0));
            if (phys_box_free(up, HALF_W, h, solid, user)) {
                Player tmp = *p;
                tmp.pos = up;
                if (phys_move_axis(&tmp.pos, axis, d, HALF_W, h, solid, user) || fabsf((axis == 0 ? tmp.pos.x - up.x : tmp.pos.z - up.z)) > 0.05f) {
                    p->pos = tmp.pos;
                    p->eye_offset -= 1.0f;
                    done = true;
                }
            }
        }
        if (!done && p->stance < STANCE_CRAWL) {
            for (int s = p->stance + 1; s <= STANCE_CRAWL && !done; s++) {
                Player tmp = *p;
                if (phys_move_axis(&tmp.pos, axis, d, HALF_W, HEIGHTS[s], solid, user)) {
                    p->eye_offset += EYES[p->stance] - EYES[s];
                    p->stance = s;
                    h = HEIGHTS[s];
                    p->pos = tmp.pos;
                    done = true;
                }
            }
        }
        if (!done) {
            p->pos = blocked;
            if (axis == 0) p->vel.x = 0; else p->vel.z = 0;
            /* spider climb: pushing into a wall walks up it */
            if (p->climbing && (fabsf(in->move_z) > 0.2f || fabsf(in->move_x) > 0.2f)) p->vel.y = fmaxf(p->vel.y, 4.0f);
        }
    }
    bool was_falling = p->vel.y < 0;
    p->on_ground = false;
    if (!phys_move_axis(&p->pos, 1, p->vel.y * dt, HALF_W, h, solid, user)) {
        if (was_falling) p->on_ground = true;
        if (was_falling && p->vel.y < -7.0f) p->eye_offset -= fminf(0.3f, fmaxf(0.05f, -p->vel.y / 40.0f)); /* the eye dips on a hard landing */
        p->vel.y = 0;
    }
    p->eye_offset *= expf(-12.0f * dt);
    float hs = sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z);
    if (p->on_ground) p->bob += hs * dt * 1.6f;
}

vec3 player_eye(const Player *p) {
    float bob = sinf(p->bob * 2.0f) * 0.04f;
    return v3(p->pos.x, p->pos.y + EYES[p->stance] + p->eye_offset + bob, p->pos.z);
}
