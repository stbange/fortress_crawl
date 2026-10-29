#ifndef FC_COMMON_H
#define FC_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MINI(a, b) ((a) < (b) ? (a) : (b))
#define MAXI(a, b) ((a) > (b) ? (a) : (b))
#define CLAMPI(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#define PI_F 3.14159265358979f

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothf(float t) { return t * t * (3.0f - 2.0f * t); }

/* Floor division / modulo that behave for negative numbers (world coordinates). */
static inline int floordiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
static inline int floormod(int a, int b) { int m = a % b; return m < 0 ? m + b : m; }

/* ---------- Hashing: everything in world generation derives from these ---------- */

static inline u64 mix64(u64 x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static inline u64 hash_combine(u64 h, u64 v) {
    return mix64(h ^ (v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2)));
}

/* Hash of a 3D integer position with a seed and a salt (salt separates independent decisions). */
static inline u64 hash3(u64 seed, int x, int y, int z, u32 salt) {
    u64 h = mix64(seed ^ ((u64)salt * 0x9E3779B97F4A7C15ULL));
    h = hash_combine(h, (u32)x);
    h = hash_combine(h, (u32)y);
    h = hash_combine(h, (u32)z);
    return h;
}

static inline float hash_unit(u64 h) { return (float)(h >> 40) * (1.0f / 16777216.0f); }
static inline float hash3f(u64 seed, int x, int y, int z, u32 salt) { return hash_unit(hash3(seed, x, y, z, salt)); }

/* ---------- Small deterministic RNG (SplitMix64) ---------- */

typedef struct Rng { u64 s; } Rng;

static inline Rng rng_make(u64 seed) { Rng r = { mix64(seed ^ 0xD1B54A32D192ED03ULL) }; return r; }
static inline u64 rng_next(Rng *r) {
    u64 z = (r->s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static inline float rng_float(Rng *r) { return hash_unit(rng_next(r)); }
/* Inclusive range [lo, hi]. */
static inline int rng_range(Rng *r, int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rng_next(r) % (u64)(hi - lo + 1));
}
static inline bool rng_chance(Rng *r, float p) { return rng_float(r) < p; }

/* Pick an index from a weight table; returns -1 if all weights are zero. */
static inline int pick_weighted(float u, const float *w, int n) {
    float total = 0;
    for (int i = 0; i < n; i++) total += w[i];
    if (total <= 0) return -1;
    float t = u * total;
    for (int i = 0; i < n; i++) {
        if (t < w[i]) return i;
        t -= w[i];
    }
    for (int i = n - 1; i >= 0; i--) if (w[i] > 0) return i;
    return -1;
}

#endif
