#include "game/player.h"

#define HALF_W 0.3f
#define EPS 0.001f

static const float HEIGHTS[3] = { 1.75f, 1.3f, 0.8f };
static const float EYES[3] = { 1.6f, 1.15f, 0.62f };
static const float SPEEDS[3] = { 6.0f, 3.2f, 2.0f };

float player_height(int stance) { return HEIGHTS[stance]; }

void player_init(Player *p, vec3 pos, float yaw) {
    memset(p, 0, sizeof *p);
    p->pos = pos;
    p->yaw = yaw;
}

static bool box_free(vec3 pos, float h, SolidFn solid, void *user) {
    int x0 = (int)floorf(pos.x - HALF_W + EPS), x1 = (int)floorf(pos.x + HALF_W - EPS);
    int y0 = (int)floorf(pos.y + EPS), y1 = (int)floorf(pos.y + h - EPS);
    int z0 = (int)floorf(pos.z - HALF_W + EPS), z1 = (int)floorf(pos.z + HALF_W - EPS);
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                if (solid(user, x, y, z) != 0) return false;
    return true;
}

/* Moves along one axis, stopping at the first obstacle. Returns false if blocked. */
static bool move_axis(Player *p, int axis, float d, float h, SolidFn solid, void *user) {
    if (d == 0) return true;
    const float step = 0.25f;
    float remaining = d;
    while (fabsf(remaining) > 1e-5f) {
        float s = fabsf(remaining) > step ? (remaining > 0 ? step : -step) : remaining;
        vec3 np = p->pos;
        if (axis == 0) np.x += s; else if (axis == 1) np.y += s; else np.z += s;
        if (!box_free(np, h, solid, user)) {
            /* slide flush against the obstacle */
            if (axis == 1) {
                if (s < 0) p->pos.y = floorf(p->pos.y + EPS);
                else p->pos.y = floorf(p->pos.y + h + s) - h - EPS;
            } else {
                float edge = axis == 0 ? p->pos.x : p->pos.z;
                float target = s > 0 ? floorf(edge + HALF_W + s) - HALF_W - EPS : floorf(edge - HALF_W + s) + 1.0f + HALF_W + EPS;
                if (axis == 0) p->pos.x = target; else p->pos.z = target;
            }
            return false;
        }
        p->pos = np;
        remaining -= s;
    }
    return true;
}

/* Lowest stance that fits at pos (standing preferred). */
static int fitting_stance(vec3 pos, int min_stance, SolidFn solid, void *user) {
    for (int s = min_stance; s <= STANCE_CRAWL; s++)
        if (box_free(pos, HEIGHTS[s], solid, user)) return s;
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

    /* stance: crouch on request, otherwise stand up as soon as there is room */
    int want = in->crouch ? STANCE_CROUCH : STANCE_STAND;
    int st = fitting_stance(p->pos, want, solid, user);
    if (st != p->stance) {
        p->eye_offset += EYES[p->stance] - EYES[st];
        p->stance = st;
    }
    float h = HEIGHTS[p->stance];

    float speed = SPEEDS[p->stance] * (in->sprint && p->stance == STANCE_STAND ? 1.6f : 1.0f);
    float accel = p->on_ground ? 14.0f : 3.0f;
    p->vel.x += (wish.x * speed - p->vel.x) * fminf(1.0f, accel * dt);
    p->vel.z += (wish.z * speed - p->vel.z) * fminf(1.0f, accel * dt);
    p->vel.y -= 25.0f * dt;
    if (p->vel.y < -40.0f) p->vel.y = -40.0f;
    if (in->jump && p->on_ground && p->stance == STANCE_STAND) p->vel.y = 8.2f;

    for (int axis = 0; axis < 3; axis += 2) {
        float d = (axis == 0 ? p->vel.x : p->vel.z) * dt;
        vec3 before = p->pos;
        if (move_axis(p, axis, d, h, solid, user)) continue;
        /* blocked: try stepping up one block, then try ducking into low openings */
        vec3 blocked = p->pos;
        p->pos = before;
        bool done = false;
        if (p->on_ground) {
            vec3 up = v3_add(before, v3(0, 1.0f + EPS, 0));
            if (box_free(up, h, solid, user)) {
                Player tmp = *p;
                tmp.pos = up;
                if (move_axis(&tmp, axis, d, h, solid, user) || fabsf((axis == 0 ? tmp.pos.x - up.x : tmp.pos.z - up.z)) > 0.05f) {
                    p->pos = tmp.pos;
                    p->eye_offset -= 1.0f;
                    done = true;
                }
            }
        }
        if (!done && p->stance < STANCE_CRAWL) {
            for (int s = p->stance + 1; s <= STANCE_CRAWL && !done; s++) {
                Player tmp = *p;
                if (move_axis(&tmp, axis, d, HEIGHTS[s], solid, user)) {
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
        }
    }
    bool was_falling = p->vel.y < 0;
    p->on_ground = false;
    if (!move_axis(p, 1, p->vel.y * dt, h, solid, user)) {
        if (was_falling) p->on_ground = true;
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
