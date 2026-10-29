#include "game/physics.h"

#define EPS 0.001f

bool phys_box_free(vec3 pos, float hw, float h, SolidFn solid, void *user) {
    int x0 = (int)floorf(pos.x - hw + EPS), x1 = (int)floorf(pos.x + hw - EPS);
    int y0 = (int)floorf(pos.y + EPS), y1 = (int)floorf(pos.y + h - EPS);
    int z0 = (int)floorf(pos.z - hw + EPS), z1 = (int)floorf(pos.z + hw - EPS);
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                if (solid(user, x, y, z) != 0) return false;
    return true;
}

bool phys_move_axis(vec3 *pos, int axis, float d, float hw, float h, SolidFn solid, void *user) {
    if (d == 0) return true;
    const float step = 0.25f;
    float remaining = d;
    while (fabsf(remaining) > 1e-5f) {
        float s = fabsf(remaining) > step ? (remaining > 0 ? step : -step) : remaining;
        vec3 np = *pos;
        if (axis == 0) np.x += s; else if (axis == 1) np.y += s; else np.z += s;
        if (!phys_box_free(np, hw, h, solid, user)) {
            if (axis == 1) {
                if (s < 0) pos->y = floorf(pos->y + EPS);
                else pos->y = floorf(pos->y + h + s) - h - EPS;
            } else {
                float edge = axis == 0 ? pos->x : pos->z;
                float target = s > 0 ? floorf(edge + hw + s) - hw - EPS : floorf(edge - hw + s) + 1.0f + hw + EPS;
                /* never snap backwards past the start (can happen when already touching) */
                if ((s > 0 && target < edge) || (s < 0 && target > edge)) target = edge;
                if (axis == 0) pos->x = target; else pos->z = target;
            }
            return false;
        }
        *pos = np;
        remaining -= s;
    }
    return true;
}

float phys_raycast(vec3 o, vec3 dir, float max, SolidFn solid, void *user) {
    int x = (int)floorf(o.x), y = (int)floorf(o.y), z = (int)floorf(o.z);
    int sx = dir.x > 0 ? 1 : -1, sy = dir.y > 0 ? 1 : -1, sz = dir.z > 0 ? 1 : -1;
    float tdx = dir.x != 0 ? fabsf(1.0f / dir.x) : 1e30f;
    float tdy = dir.y != 0 ? fabsf(1.0f / dir.y) : 1e30f;
    float tdz = dir.z != 0 ? fabsf(1.0f / dir.z) : 1e30f;
    float tmx = dir.x != 0 ? ((sx > 0 ? (x + 1 - o.x) : (o.x - x)) * tdx) : 1e30f;
    float tmy = dir.y != 0 ? ((sy > 0 ? (y + 1 - o.y) : (o.y - y)) * tdy) : 1e30f;
    float tmz = dir.z != 0 ? ((sz > 0 ? (z + 1 - o.z) : (o.z - z)) * tdz) : 1e30f;
    float t = 0;
    while (t < max) {
        if (solid(user, x, y, z) != 0) return t;
        if (tmx < tmy && tmx < tmz) { x += sx; t = tmx; tmx += tdx; }
        else if (tmy < tmz) { y += sy; t = tmy; tmy += tdy; }
        else { z += sz; t = tmz; tmz += tdz; }
    }
    return max;
}

bool phys_line_of_sight(vec3 a, vec3 b, SolidFn solid, void *user) {
    vec3 d = v3_sub(b, a);
    float len = v3_len(d);
    if (len < 0.01f) return true;
    return phys_raycast(a, v3_scale(d, 1.0f / len), len, solid, user) >= len - 0.01f;
}
