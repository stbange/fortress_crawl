#include "world/texgen.h"

/* Procedural 16x16 textures. Everything is derived from the theme palette, so a
 * new stratum or flavor gets a full texture set without any art files. */

#define N TEX_SIZE

typedef struct { float r, g, b; } RGB;

static RGB rgb(Col c) { RGB o = { (float)c.r, (float)c.g, (float)c.b }; return o; }
static RGB rgb_scale(RGB c, float f) { RGB o = { c.r * f, c.g * f, c.b * f }; return o; }
static RGB rgb_mix(RGB a, RGB b, float t) {
    RGB o = { lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t) };
    return o;
}

static void put(u8 *p, int x, int y, RGB c, float a) {
    u8 *q = p + ((size_t)y * N + (size_t)x) * 4;
    q[0] = (u8)clampf(c.r, 0, 255);
    q[1] = (u8)clampf(c.g, 0, 255);
    q[2] = (u8)clampf(c.b, 0, 255);
    q[3] = (u8)clampf(a * 255.0f, 0, 255);
}

static float pn(u64 seed, int x, int y) { return hash3f(seed, floormod(x, N), floormod(y, N), 0, 7); }

/* Value noise that tiles over the 16px texture with `cells` lattice cells per side. */
static float tnoise(u64 seed, float x, float y, int cells) {
    float fx = x * cells / (float)N, fy = y * cells / (float)N;
    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    float tx = smoothf(fx - x0), ty = smoothf(fy - y0);
    float a = hash3f(seed, floormod(x0, cells), floormod(y0, cells), cells, 3);
    float b = hash3f(seed, floormod(x0 + 1, cells), floormod(y0, cells), cells, 3);
    float c = hash3f(seed, floormod(x0, cells), floormod(y0 + 1, cells), cells, 3);
    float d = hash3f(seed, floormod(x0 + 1, cells), floormod(y0 + 1, cells), cells, 3);
    return lerpf(lerpf(a, b, tx), lerpf(c, d, tx), ty);
}

/* Quantise brightness to a few steps: gives the hand-pixelled look. */
static float band(float f, float steps) { return floorf(f * steps + 0.5f) / steps; }

static void fill_noise(u8 *p, u64 seed, RGB base, float amount) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float n = 0.6f * tnoise(seed, x, y, 4) + 0.25f * tnoise(seed + 1, x, y, 8) + 0.15f * pn(seed, x, y);
            put(p, x, y, rgb_scale(base, band(1.0f - amount + n * amount * 2.0f, 6)), 1);
        }
}

static void cracks(u8 *p, u64 seed, int count, float dark) {
    Rng r = rng_make(seed ^ 0xC4AC);
    for (int i = 0; i < count; i++) {
        int x = rng_range(&r, 0, N - 1), y = rng_range(&r, 0, N - 1);
        int len = rng_range(&r, 3, 7);
        for (int k = 0; k < len; k++) {
            u8 *q = p + ((size_t)y * N + (size_t)x) * 4;
            q[0] = (u8)(q[0] * dark); q[1] = (u8)(q[1] * dark); q[2] = (u8)(q[2] * dark);
            x = floormod(x + rng_range(&r, -1, 1), N);
            y = floormod(y + 1, N);
        }
    }
}

static void tex_rock(u8 *p, u64 seed, RGB base) {
    fill_noise(p, seed, base, 0.25f);
    cracks(p, seed, 3, 0.55f);
}

/* Generic brick bond: rows of height h, bricks of length l, offset every other row. */
static void tex_bricks(u8 *p, u64 seed, RGB base, RGB mortar, int h, int l, bool irregular) {
    int row_y = 0, row = 0;
    while (row_y < N) {
        int rh = h;
        if (irregular) rh = h + (int)(hash3f(seed, row, 0, 0, 11) * 3) - 1;
        if (row_y + rh > N) rh = N - row_y;
        int off = irregular ? (int)(hash3f(seed, row, 1, 0, 12) * l) : (row % 2) * (l / 2);
        for (int y = row_y; y < row_y + rh; y++)
            for (int x = 0; x < N; x++) {
                int bx = floormod(x + off, l);
                int brick = floordiv(x + off, l);
                bool is_mortar = (y == row_y + rh - 1) || bx == l - 1;
                if (irregular && !is_mortar) {
                    /* wider, uneven stones */
                    int split = 3 + (int)(hash3f(seed, row, brick, 0, 13) * (l - 4));
                    is_mortar = (bx == split) && hash3f(seed, row, brick, 1, 14) < 0.5f;
                }
                if (is_mortar) {
                    put(p, x, y, rgb_scale(mortar, 0.85f + 0.2f * pn(seed, x, y)), 1);
                } else {
                    float bv = 0.82f + 0.3f * hash3f(seed, row, floormod(brick, N / l + 1), 0, 15);
                    float n = 0.9f + 0.2f * tnoise(seed, x, y, 8);
                    /* bevel: lighter top edge, darker bottom */
                    float bev = (y == row_y) ? 1.12f : (y == row_y + rh - 2 ? 0.88f : 1.0f);
                    put(p, x, y, rgb_scale(base, band(bv * n * bev, 7)), 1);
                }
            }
        row_y += rh;
        row++;
    }
}

static void tex_logs(u8 *p, u64 seed, RGB base, RGB dark) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            int ly = y % 4;
            float shade = (ly == 0) ? 1.15f : (ly == 3 ? 0.55f : 0.95f);
            float grain = 0.85f + 0.3f * tnoise(seed + (u64)(y / 4), x * 0.25f, y, 2);
            RGB c = rgb_scale(base, band(shade * grain, 6));
            if (ly == 3 && pn(seed, x, y) < 0.3f) c = dark;
            put(p, x, y, c, 1);
        }
}

static void tex_plaster(u8 *p, u64 seed, RGB base, RGB accent, int var) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float n = 0.92f + 0.12f * tnoise(seed, x, y, 4) + 0.05f * pn(seed, x, y);
            RGB c = rgb_scale(base, band(n, 8));
            if (var == 0 && (y == 12 || y == 14)) c = rgb_scale(accent, 0.9f);
            if (var == 0 && y == 13 && ((x + 1) % 4 < 2)) c = accent;
            put(p, x, y, c, 1);
        }
}

static void tex_ice(u8 *p, u64 seed, RGB base) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float streak = tnoise(seed, (float)(x + y) * 0.7f, (float)(x - y) * 0.2f, 4);
            float n = 0.8f + 0.35f * streak + 0.1f * pn(seed, x, y);
            RGB c = rgb_scale(base, band(n, 6));
            if (pn(seed + 5, x, y) > 0.96f) c = rgb_mix(c, (RGB){ 255, 255, 255 }, 0.7f);
            put(p, x, y, c, 1);
        }
    cracks(p, seed, 2, 1.25f);
}

static void tex_marble(u8 *p, u64 seed, RGB base) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float v = sinf(x * 0.45f + y * 0.2f + tnoise(seed, x, y, 4) * 6.0f);
            float n = 0.95f + 0.08f * tnoise(seed + 1, x, y, 8);
            RGB c = rgb_scale(base, band(n, 8));
            if (fabsf(v) < 0.15f) c = rgb_scale(base, 0.72f);
            if (x == 0 || y == 0) c = rgb_scale(base, 0.85f);
            put(p, x, y, c, 1);
        }
}

static void tex_flagstone(u8 *p, u64 seed, RGB base, RGB mortar) {
    /* periodic voronoi with 5 stones */
    float sx[5], sy[5], sv[5];
    for (int i = 0; i < 5; i++) {
        sx[i] = hash3f(seed, i, 0, 0, 21) * N;
        sy[i] = hash3f(seed, i, 1, 0, 21) * N;
        sv[i] = 0.8f + 0.35f * hash3f(seed, i, 2, 0, 21);
    }
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float d1 = 1e9f, d2 = 1e9f; int best = 0;
            for (int i = 0; i < 5; i++)
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++) {
                        float dx = x + 0.5f - (sx[i] + ox * N), dy = y + 0.5f - (sy[i] + oy * N);
                        float d = sqrtf(dx * dx + dy * dy);
                        if (d < d1) { d2 = d1; d1 = d; best = i; }
                        else if (d < d2) d2 = d;
                    }
            if (d2 - d1 < 1.1f) put(p, x, y, rgb_scale(mortar, 0.8f + 0.2f * pn(seed, x, y)), 1);
            else put(p, x, y, rgb_scale(base, band(sv[best] * (0.9f + 0.18f * tnoise(seed, x, y, 8)), 7)), 1);
        }
}

static void tex_tiles(u8 *p, u64 seed, RGB base, RGB mortar) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            bool dark = ((x / 8) + (y / 8)) % 2;
            float n = 0.92f + 0.12f * tnoise(seed, x, y, 8);
            RGB c = rgb_scale(base, band((dark ? 0.62f : 1.0f) * n, 7));
            if (x % 8 == 7 || y % 8 == 7) c = rgb_scale(mortar, 0.9f);
            put(p, x, y, c, 1);
        }
}

static void tex_planks(u8 *p, u64 seed, RGB base, bool vertical) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            int a = vertical ? x : y, b = vertical ? y : x;
            int board = a / 4;
            float bv = 0.8f + 0.3f * hash3f(seed, board, 0, 0, 31);
            float grain = 0.85f + 0.25f * tnoise(seed + (u64)board, vertical ? 0 : b, vertical ? b : 0, 8);
            RGB c = rgb_scale(base, band(bv * grain, 6));
            if (a % 4 == 3) c = rgb_scale(base, 0.45f);
            int nail = floormod((int)(hash3f(seed, board, 1, 0, 32) * 16), 16);
            if (a % 4 == 1 && b == nail) c = rgb_scale(base, 0.35f);
            put(p, vertical ? x : x, y, c, 1);
        }
}

static void tex_dirt(u8 *p, u64 seed, RGB base) {
    fill_noise(p, seed, base, 0.3f);
    for (int i = 0; i < 10; i++) {
        int x = (int)(hash3f(seed, i, 0, 0, 41) * N), y = (int)(hash3f(seed, i, 1, 0, 41) * N);
        put(p, x, y, rgb_scale(base, 1.35f), 1);
        put(p, (x + 1) % N, y, rgb_scale(base, 0.6f), 1);
    }
}

static void tex_pillar(u8 *p, u64 seed, RGB base) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float flute = 0.75f + 0.35f * (0.5f + 0.5f * cosf((x + 0.5f) * 2.0f * PI_F / 4.0f));
            float n = 0.93f + 0.1f * tnoise(seed, x, y, 8);
            put(p, x, y, rgb_scale(base, band(flute * n, 6)), 1);
        }
}

static void tex_metal(u8 *p, u64 seed, RGB base) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float n = 0.85f + 0.2f * tnoise(seed, x, y, 4);
            RGB c = rgb_scale(base, band(n, 6));
            if (x == 0 || y == 0) c = rgb_scale(base, 1.3f);
            if (x == N - 1 || y == N - 1) c = rgb_scale(base, 0.5f);
            if ((x == 2 || x == 13) && (y == 2 || y == 13)) c = rgb_scale(base, 1.6f);
            if (tnoise(seed + 9, x, y, 8) > 0.78f) c = rgb_mix(c, (RGB){ 110, 50, 25 }, 0.5f);
            put(p, x, y, c, 1);
        }
}

static void tex_accent(u8 *p, u64 seed, RGB base) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            int d = abs((x % 8) - 4) + abs((y % 8) - 4);
            float f = (d == 3) ? 0.55f : (d < 2 ? 1.25f : 0.95f);
            f *= 0.92f + 0.1f * pn(seed, x, y);
            put(p, x, y, rgb_scale(base, band(f, 6)), 1);
        }
}

static void tex_lava(u8 *p, u64 seed) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float n = tnoise(seed, x, y, 4) * 0.7f + tnoise(seed + 1, x, y, 8) * 0.3f;
            RGB c = n > 0.62f ? (RGB){ 255, 230, 120 } : (n > 0.4f ? (RGB){ 255, 130, 30 } : (RGB){ 150, 40, 10 });
            put(p, x, y, c, 1);
        }
}

static void tex_water(u8 *p, u64 seed, RGB base) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float w = sinf((x + tnoise(seed, x, y, 4) * 5.0f) * 0.8f + y * 0.3f);
            RGB c = rgb_scale(base, w > 0.85f ? 1.6f : 0.9f + 0.1f * pn(seed, x, y));
            put(p, x, y, c, 1);
        }
}

static void tex_crystal(u8 *p, u64 seed, RGB base) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            int facet = ((x + y) / 5 + (x - y + 32) / 7) % 3;
            float f = facet == 0 ? 1.15f : (facet == 1 ? 0.85f : 0.65f);
            RGB c = rgb_scale(base, f);
            if (pn(seed, x, y) > 0.94f) c = (RGB){ 255, 255, 255 };
            put(p, x, y, c, 1);
        }
}

static void tex_brazier(u8 *p, u64 seed, RGB base) {
    tex_metal(p, seed, base);
    for (int x = 0; x < N; x++) {
        for (int y = 0; y < 3; y++) {
            float n = pn(seed, x, y);
            put(p, x, y, n > 0.5f ? (RGB){ 255, 150, 40 } : (RGB){ 200, 60, 20 }, 1);
        }
        put(p, x, 7, rgb_scale(base, 1.4f), 1);
    }
}

static void tex_glass(u8 *p, u64 seed, RGB base, bool stained) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            RGB c;
            if (x % 4 == 0 || y % 5 == 0) c = (RGB){ 25, 22, 22 };
            else {
                int pane = (x / 4) * 7 + (y / 5);
                c = base;
                if (stained) {
                    static const RGB hues[4] = { { 200, 40, 50 }, { 50, 80, 200 }, { 220, 180, 60 }, { 60, 160, 80 } };
                    c = hues[hash3(seed, pane, 0, 0, 51) % 4];
                }
                c = rgb_scale(c, 0.8f + 0.25f * pn(seed, x, y));
            }
            put(p, x, y, c, 1);
        }
}

static void tex_roof(u8 *p, u64 seed, RGB base) {
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            int row = y / 4;
            int off = (row % 2) * 2;
            int sx = (x + off) % 4;
            float dy = (float)(y % 4);
            float edge = sqrtf((sx - 1.5f) * (sx - 1.5f) + (dy - 0.0f) * (dy - 0.0f));
            float f = edge > 2.6f ? 0.5f : (1.1f - dy * 0.1f);
            put(p, x, y, rgb_scale(base, band(f * (0.9f + 0.15f * pn(seed, x, y)), 6)), 1);
        }
}

static void tex_ore(u8 *p, u64 seed, RGB base, RGB vein) {
    tex_rock(p, seed, base);
    for (int i = 0; i < 9; i++) {
        int x = (int)(hash3f(seed, i, 0, 0, 61) * (N - 1)), y = (int)(hash3f(seed, i, 1, 0, 61) * (N - 1));
        put(p, x, y, vein, 1);
        put(p, x + 1, y, rgb_scale(vein, 0.7f), 1);
        put(p, x, y + 1, rgb_scale(vein, 0.8f), 1);
    }
}

/* ---------- sprites (with transparency) ---------- */

static void clear(u8 *p) { memset(p, 0, N * N * 4); }

static void flame(u8 *p, u64 seed, int x0, int x1, int ybase, int maxh) {
    for (int x = x0; x <= x1; x++) {
        float c = 1.0f - fabsf((x - (x0 + x1) * 0.5f) / ((x1 - x0) * 0.5f + 1.0f));
        int h = (int)(maxh * (0.45f + 0.55f * c) * (0.75f + 0.35f * pn(seed, x, 0)));
        for (int k = 0; k < h; k++) {
            int y = ybase - k;
            if (y < 0) break;
            float t = (float)k / (float)(h > 1 ? h : 1);
            RGB col = t < 0.35f ? (RGB){ 255, 240, 160 } : (t < 0.7f ? (RGB){ 255, 150, 40 } : (RGB){ 210, 60, 20 });
            put(p, x, y, col, 1);
        }
    }
}

static void spr_torch(u8 *p, u64 seed, RGB wood) {
    clear(p);
    for (int y = 7; y < N; y++) { put(p, 7, y, wood, 1); put(p, 8, y, rgb_scale(wood, 0.7f), 1); }
    flame(p, seed, 6, 9, 7, 6);
}

static void spr_fire(u8 *p, u64 seed) {
    clear(p);
    flame(p, seed, 1, 14, 15, 13);
}

static void spr_web(u8 *p, u64 seed) {
    clear(p);
    RGB c = { 200, 200, 205 };
    for (int i = 0; i < N; i++) {
        put(p, i, i, c, 0.8f);
        put(p, i, 0, c, 0.6f);
        put(p, 0, i, c, 0.6f);
        if (i < 12) put(p, i, (i * 2) / 3, c, 0.6f);
        if (i < 12) put(p, (i * 2) / 3, i, c, 0.6f);
    }
    for (int r = 4; r < 16; r += 4)
        for (int a = 0; a <= r; a++) put(p, a, r - a, c, 0.7f);
}

static void spr_chain(u8 *p, u64 seed, RGB metal) {
    clear(p);
    for (int y = 0; y < N; y++) {
        int link = y / 4, ly = y % 4;
        if (link % 2 == 0) {
            if (ly == 0 || ly == 3) put(p, 7, y, metal, 1), put(p, 8, y, metal, 1);
            else put(p, 6, y, metal, 1), put(p, 9, y, rgb_scale(metal, 0.7f), 1);
        } else {
            put(p, 7, y, rgb_scale(metal, 1.2f), 1);
            put(p, 8, y, rgb_scale(metal, 0.8f), 1);
        }
    }
}

static void spr_bars(u8 *p, u64 seed, RGB metal) {
    clear(p);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            bool bar = (x % 5 == 1) || (x % 5 == 2);
            bool band_ = (y == 1 || y == 2 || y == 13 || y == 14);
            if (bar || band_) put(p, x, y, rgb_scale(metal, (x % 5 == 1) ? 1.2f : 0.8f), 1);
        }
}

static void spr_roots(u8 *p, u64 seed, RGB wood) {
    clear(p);
    for (int i = 0; i < 5; i++) {
        float x = 1.0f + hash3f(seed, i, 0, 0, 71) * 13.0f;
        int len = 6 + (int)(hash3f(seed, i, 1, 0, 71) * 10);
        for (int y = 0; y < len; y++) {
            x += (hash3f(seed, i, y, 0, 72) - 0.5f) * 1.2f;
            int xi = CLAMPI((int)x, 0, N - 1);
            put(p, xi, y, rgb_scale(wood, 0.8f + 0.3f * pn(seed, xi, y)), 1);
        }
    }
}

static void spr_fungus(u8 *p, u64 seed, RGB glow) {
    clear(p);
    for (int i = 0; i < 3; i++) {
        int cx = 3 + i * 5, h = 4 + (int)(hash3f(seed, i, 0, 0, 81) * 6), r = 2 + (i == 1);
        for (int y = N - h; y < N; y++) put(p, cx, y, (RGB){ 200, 200, 180 }, 1);
        for (int dx = -r; dx <= r; dx++)
            for (int dy = 0; dy < 2; dy++)
                put(p, CLAMPI(cx + dx, 0, N - 1), N - h - dy, rgb_scale(glow, dy ? 0.8f : 1.1f), 1);
    }
}

static void spr_lantern(u8 *p, u64 seed, RGB glow, RGB metal) {
    clear(p);
    for (int y = 0; y < 4; y++) put(p, 8, y, metal, 1);
    for (int y = 4; y < 13; y++) {
        float dy = (y - 8.5f) / 4.5f;
        int w = (int)(4.0f * sqrtf(clampf(1.0f - dy * dy, 0, 1))) + 1;
        for (int x = 8 - w; x <= 7 + w; x++) {
            RGB c = (y % 3 == 0) ? rgb_scale(glow, 0.6f) : glow;
            put(p, x, y, c, 1);
        }
    }
    for (int x = 6; x < 10; x++) { put(p, x, 4, metal, 1); put(p, x, 13, metal, 1); }
}

static void spr_bones(u8 *p, u64 seed) {
    clear(p);
    RGB bone = { 210, 200, 170 }, dark = { 60, 50, 40 };
    for (int y = 9; y < 14; y++)
        for (int x = 3; x < 9; x++) put(p, x, y, bone, 1);
    put(p, 4, 11, dark, 1); put(p, 7, 11, dark, 1); put(p, 5, 13, dark, 1);
    for (int x = 8; x < 15; x++) put(p, x, 14 - (x - 8) / 3, bone, 1);
    for (int x = 1; x < 6; x++) put(p, x, 15, bone, 1);
}

static void spr_grass(u8 *p, u64 seed, RGB base) {
    clear(p);
    for (int i = 0; i < 8; i++) {
        int x = (int)(hash3f(seed, i, 0, 0, 91) * N);
        int h = 3 + (int)(hash3f(seed, i, 1, 0, 91) * 7);
        for (int k = 0; k < h; k++) put(p, CLAMPI(x + (k > h / 2 ? 1 : 0), 0, N - 1), N - 1 - k, rgb_scale(base, 0.8f + 0.04f * k), 1);
    }
}

void texgen_theme(int theme, u8 *out) {
    ThemeInfo t;
    theme_info(theme, &t);
    const RGB mortar = rgb(t.mortar);
    memset(out, 0, (size_t)TILES_PER_THEME * N * N * 4);
    for (int b = 1; b < B_COUNT; b++) {
        for (int v = 0; v < TEX_VARIANTS; v++) {
            u8 *p = out + ((size_t)(b * TEX_VARIANTS + v)) * N * N * 4;
            u64 seed = hash3(0xFC7E7, theme, b, v, 1);
            RGB base = rgb(t.mat[b]);
            switch (b) {
            case B_ROCK: tex_rock(p, seed, base); break;
            case B_WALL:
            case B_CEIL:
                switch (t.wall_pat) {
                case PAT_BRICK: tex_bricks(p, seed, base, mortar, 4, 8, false); break;
                case PAT_BIGBRICK: tex_bricks(p, seed, base, mortar, 8, 16, false); break;
                case PAT_ASHLAR: tex_bricks(p, seed, base, mortar, 5, 12, true); break;
                case PAT_LOGS: tex_logs(p, seed, base, mortar); break;
                case PAT_PLASTER: tex_plaster(p, seed, base, rgb(t.mat[B_ACCENT]), b == B_WALL ? v : 1); break;
                case PAT_ICE: tex_ice(p, seed, base); break;
                case PAT_MARBLE: tex_marble(p, seed, base); break;
                }
                break;
            case B_FLOOR:
                switch (t.floor_pat) {
                case FP_FLAGSTONE: tex_flagstone(p, seed, base, mortar); break;
                case FP_TILES: tex_tiles(p, seed, base, mortar); break;
                case FP_PLANKS: tex_planks(p, seed, base, true); break;
                case FP_DIRT: tex_dirt(p, seed, base); break;
                }
                break;
            case B_PILLAR: tex_pillar(p, seed, base); break;
            case B_WOOD: tex_planks(p, seed, base, false); break;
            case B_METAL: tex_metal(p, seed, base); break;
            case B_ACCENT: tex_accent(p, seed, base); break;
            case B_DIRT: tex_dirt(p, seed, base); break;
            case B_LAVA: tex_lava(p, seed); break;
            case B_WATER: tex_water(p, seed, base); break;
            case B_CRYSTAL: tex_crystal(p, seed, base); break;
            case B_ICE: tex_ice(p, seed, base); break;
            case B_BRAZIER: tex_brazier(p, seed, base); break;
            case B_GLASS: tex_glass(p, seed, base, theme_flavor(theme) == FL_GOTHIC || theme_stratum(theme) == ST_HEAVEN); break;
            case B_ROOF: tex_roof(p, seed, base); break;
            case B_ORE: tex_ore(p, seed, rgb(t.mat[B_ROCK]), rgb(t.mat[B_ACCENT])); break;
            case B_TORCH: spr_torch(p, seed, rgb(t.mat[B_WOOD])); break;
            case B_FIRE: spr_fire(p, seed); break;
            case B_WEB: spr_web(p, seed); break;
            case B_CHAIN: spr_chain(p, seed, rgb_scale(rgb(t.mat[B_METAL]), 1.3f)); break;
            case B_BARS: spr_bars(p, seed, rgb(t.mat[B_METAL])); break;
            case B_ROOTS: spr_roots(p, seed, rgb(t.mat[B_WOOD])); break;
            case B_FUNGUS: {
                RGB g = { t.light[LIGHT_FUNGUS][0] * 17.0f, t.light[LIGHT_FUNGUS][1] * 17.0f, t.light[LIGHT_FUNGUS][2] * 17.0f };
                spr_fungus(p, seed, g);
                break;
            }
            case B_LANTERN: {
                RGB g = { t.light[LIGHT_TORCH][0] * 17.0f, t.light[LIGHT_TORCH][1] * 17.0f, t.light[LIGHT_TORCH][2] * 17.0f };
                spr_lantern(p, seed, g, rgb(t.mat[B_METAL]));
                break;
            }
            case B_BONES: spr_bones(p, seed); break;
            case B_GRASS: spr_grass(p, seed, rgb_scale(rgb(t.mat[B_DIRT]), 1.2f)); break;
            default: fill_noise(p, seed, base, 0.2f); break;
            }
        }
    }
}
