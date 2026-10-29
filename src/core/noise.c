#include "core/noise.h"

static const float GRAD3[12][3] = {
    {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
    {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
    {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1},
};

static inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

static inline float grad3(u64 seed, int x, int y, int z, float dx, float dy, float dz) {
    const float *g = GRAD3[hash3(seed, x, y, z, 0x51) % 12];
    return g[0] * dx + g[1] * dy + g[2] * dz;
}

float noise3(u64 seed, float x, float y, float z) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y), z0 = (int)floorf(z);
    float fx = x - x0, fy = y - y0, fz = z - z0;
    float u = fade(fx), v = fade(fy), w = fade(fz);
    float n000 = grad3(seed, x0, y0, z0, fx, fy, fz);
    float n100 = grad3(seed, x0 + 1, y0, z0, fx - 1, fy, fz);
    float n010 = grad3(seed, x0, y0 + 1, z0, fx, fy - 1, fz);
    float n110 = grad3(seed, x0 + 1, y0 + 1, z0, fx - 1, fy - 1, fz);
    float n001 = grad3(seed, x0, y0, z0 + 1, fx, fy, fz - 1);
    float n101 = grad3(seed, x0 + 1, y0, z0 + 1, fx - 1, fy, fz - 1);
    float n011 = grad3(seed, x0, y0 + 1, z0 + 1, fx, fy - 1, fz - 1);
    float n111 = grad3(seed, x0 + 1, y0 + 1, z0 + 1, fx - 1, fy - 1, fz - 1);
    float nx00 = lerpf(n000, n100, u), nx10 = lerpf(n010, n110, u);
    float nx01 = lerpf(n001, n101, u), nx11 = lerpf(n011, n111, u);
    float nxy0 = lerpf(nx00, nx10, v), nxy1 = lerpf(nx01, nx11, v);
    return lerpf(nxy0, nxy1, w);
}

float noise2(u64 seed, float x, float y) {
    /* A 2D slice of the 3D noise at a seed-dependent depth keeps the code small. */
    return noise3(seed, x, y, 0.5f) * 1.4f;
}

float fbm3(u64 seed, float x, float y, float z, int octaves) {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * noise3(seed + (u64)i * 7919, x, y, z);
        norm += amp;
        amp *= 0.5f;
        x *= 2.03f; y *= 2.03f; z *= 2.03f;
    }
    return sum / norm;
}

float fbm2(u64 seed, float x, float y, int octaves) {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * noise2(seed + (u64)i * 7919, x, y);
        norm += amp;
        amp *= 0.5f;
        x *= 2.03f; y *= 2.03f;
    }
    return sum / norm;
}
