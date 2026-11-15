#include "game/physics.h"

#define EPS 0.001f

u64 (*phys_pieces)(void *user, int x, int y, int z);

/* Collisions work on a grid of 25 cm: whole blocks are 4x4x4 of it. */
static inline float floor4(float v) { return floorf(v * 4.0f) * 0.25f; }

/* Does the box [lo, hi] touch one of the pieces left of the broken block at (x, y, z)? */
static bool pieces_hit(void *user, int x, int y, int z, float lx, float ly, float lz, float hx, float hy, float hz) {
    u64 m = phys_pieces ? phys_pieces(user, x, y, z) : ~0ULL;
    if (!m) return false;
    int sx0 = CLAMPI((int)floorf((lx - x) * 4.0f), 0, 3), sx1 = CLAMPI((int)floorf((hx - x) * 4.0f), 0, 3);
    int sy0 = CLAMPI((int)floorf((ly - y) * 4.0f), 0, 3), sy1 = CLAMPI((int)floorf((hy - y) * 4.0f), 0, 3);
    int sz0 = CLAMPI((int)floorf((lz - z) * 4.0f), 0, 3), sz1 = CLAMPI((int)floorf((hz - z) * 4.0f), 0, 3);
    for (int sy = sy0; sy <= sy1; sy++)
        for (int sz = sz0; sz <= sz1; sz++)
            for (int sx = sx0; sx <= sx1; sx++)
                if (m >> (sx + sz * 4 + sy * 16) & 1) return true;
    return false;
}

bool phys_box_free(vec3 pos, float hw, float h, SolidFn solid, void *user) {
    float lx = pos.x - hw + EPS, hx = pos.x + hw - EPS, ly = pos.y + EPS, hy = pos.y + h - EPS, lz = pos.z - hw + EPS, hz = pos.z + hw - EPS;
    int x0 = (int)floorf(lx), x1 = (int)floorf(hx);
    int y0 = (int)floorf(ly), y1 = (int)floorf(hy);
    int z0 = (int)floorf(lz), z1 = (int)floorf(hz);
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) {
                int v = solid(user, x, y, z);
                if (v == 2) {
                    if (pieces_hit(user, x, y, z, lx, ly, lz, hx, hy, hz)) return false;
                } else if (v != 0) return false;
            }
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
            /* flush against the obstacle: faces lie on the 25 cm grid (whole blocks too) */
            if (axis == 1) {
                if (s < 0) pos->y = fmaxf(floor4(pos->y + EPS), np.y);
                else pos->y = floor4(pos->y + h + s) - h - EPS;
            } else {
                float edge = axis == 0 ? pos->x : pos->z;
                float target = s > 0 ? floor4(edge + hw + s) - hw - EPS : floor4(edge - hw + s) + 0.25f + hw + EPS;
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
        int v = solid(user, x, y, z);
        if (v == 2) {
            /* a broken block: march through it in small steps against its pieces */
            u64 m = phys_pieces ? phys_pieces(user, x, y, z) : ~0ULL;
            float tend = fminf(fminf(tmx, tmy), fminf(tmz, max));
            for (float tt = t; tt <= tend; tt += 0.05f) {
                vec3 p = v3_add(o, v3_scale(dir, tt));
                int sx = CLAMPI((int)floorf((p.x - x) * 4.0f), 0, 3), sy = CLAMPI((int)floorf((p.y - y) * 4.0f), 0, 3),
                    sz = CLAMPI((int)floorf((p.z - z) * 4.0f), 0, 3);
                if (m >> (sx + sz * 4 + sy * 16) & 1) return tt;
            }
        } else if (v != 0) return t;
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
