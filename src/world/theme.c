#include "world/theme.h"

/*                 rooms crawl cave mine cells arena void */
const StratumDef STRATA[ST_COUNT] = {
    [ST_CORE] = { "World Core", -100000, -40,
        { 1, 2, 6, 0, 0, 2, 2 }, { 0, 0, 1, 4, 2 },
        { 150, 50, 190 }, 0.40f, 0.85f, 0.0f, { 30, 6, 36 }, 0.6f, 0.030f,
        { 255, 80, 200 }, 0.4f, 2, false, 0.30f, 0.5f, 0.9f, 0.1f, 0.0f },
    [ST_INFERNO] = { "Inferno", -39, -26,
        { 3, 2, 4, 0, 2, 3, 1 }, { 0, 0, 2, 2, 4 },
        { 160, 30, 15 }, 0.45f, 0.75f, 0.0f, { 40, 6, 2 }, 0.6f, 0.030f,
        { 255, 60, 20 }, 0.6f, 2, false, 0.30f, 0.6f, 0.0f, 0.0f, 0.0f },
    [ST_LIMBO] = { "Limbo", -25, -17,
        { 4, 2, 3, 0, 3, 2, 1 }, { 0, 0, 3, 1, 3 },
        { 110, 100, 120 }, 0.40f, 0.80f, 0.6f, { 22, 20, 26 }, 0.5f, 0.035f,
        { 180, 170, 200 }, 0.5f, 1, false, 0.30f, 0.0f, 0.1f, 0.2f, 0.0f },
    [ST_ROOTS] = { "Roots of the Mountain", -16, -10,
        { 1, 3, 7, 1, 0, 1, 0 }, { 0, 0, 0.5f, 5, 2 },
        { 70, 80, 40 }, 0.30f, 0.85f, 0.0f, { 12, 16, 8 }, 0.3f, 0.040f,
        { 0, 0, 0 }, 0.0f, 1, false, 0.30f, 0.0f, 0.3f, 0.8f, 0.9f },
    [ST_MINES] = { "Mines", -9, -4,
        { 2, 3, 2, 8, 0, 1, 0 }, { 0, 0, 1, 3, 3 },
        { 110, 80, 50 }, 0.25f, 0.90f, 0.0f, { 16, 12, 8 }, 0.3f, 0.040f,
        { 0, 0, 0 }, 0.0f, 1, false, 0.35f, 0.0f, 0.15f, 0.0f, 0.1f },
    [ST_DUNGEON] = { "Dungeon", -3, 2,
        { 7, 2, 1, 1, 3, 2, 0 }, { 0, 0, 4, 1, 2 },
        { 0, 0, 0 }, 0.0f, 1.0f, 0.0f, { 0, 0, 0 }, 0.0f, 0.035f,
        { 0, 0, 0 }, 0.0f, 1, false, 0.30f, 0.0f, 0.0f, 0.0f, 0.0f },
    [ST_CASTLE] = { "Castle", 3, 7,
        { 8, 1, 0, 0, 2, 3, 0 }, { 0, 0, 6, 0, 1 },
        { 130, 110, 90 }, 0.10f, 1.10f, 0.0f, { 0, 0, 0 }, 0.0f, 0.030f,
        { 0, 0, 0 }, 0.0f, 1, false, 0.35f, 0.0f, 0.0f, 0.0f, 0.0f },
    [ST_SPIRES] = { "Spires", 8, 13,
        { 5, 1, 0, 0, 1, 2, 4 }, { 0, 0, 4, 0, 2 },
        { 120, 130, 150 }, 0.15f, 1.05f, 0.0f, { 26, 30, 42 }, 0.5f, 0.022f,
        { 0, 0, 0 }, 0.0f, 3, true, 0.35f, 0.0f, 0.0f, 0.0f, 0.0f },
    [ST_CLOUDS] = { "Cloud Cloisters", 14, 20,
        { 3, 0, 0, 0, 0, 2, 7 }, { 0, 0, 3, 0, 1 },
        { 200, 205, 215 }, 0.35f, 1.25f, 0.2f, { 110, 115, 128 }, 0.75f, 0.020f,
        { 220, 230, 255 }, 0.3f, 7, false, 0.35f, 0.0f, 0.0f, 0.0f, 0.0f },
    [ST_HEAVEN] = { "Heaven", 21, 30,
        { 3, 0, 0, 0, 0, 3, 7 }, { 0, 0, 5, 0, 0 },
        { 240, 225, 180 }, 0.45f, 1.40f, 0.2f, { 160, 150, 120 }, 0.85f, 0.016f,
        { 255, 235, 180 }, 0.6f, 9, false, 0.35f, 0.0f, 0.2f, 0.0f, 0.0f },
    [ST_ASTRAL] = { "Astral Void", 31, 100000,
        { 1, 0, 1, 0, 0, 1, 9 }, { 0, 0, 1, 1, 0 },
        { 40, 30, 70 }, 0.50f, 0.70f, 0.3f, { 3, 2, 8 }, 0.8f, 0.010f,
        { 180, 120, 255 }, 0.6f, 2, true, 0.30f, 0.0f, 0.5f, 0.0f, 0.0f },
};

#define C(r, g, b) { r, g, b }
const FlavorDef FLAVORS[FL_COUNT] = {
    [FL_OLD] = { "Old", "Ancient", 0, 0,
        C(70, 66, 62), C(92, 86, 80), C(40, 38, 36), C(80, 76, 70), C(100, 95, 88), C(80, 58, 38),
        C(60, 60, 66), C(130, 110, 70), C(64, 54, 44), C(150, 170, 200), C(70, 60, 56),
        PAT_BRICK, FP_FLAGSTONE, C(255, 150, 70), C(14, 12, 14), B_WEB, 0 },
    [FL_FROST] = { "Frost", "Frostbitten", 1, 1,
        C(78, 88, 100), C(110, 122, 138), C(170, 190, 210), C(96, 106, 120), C(130, 142, 158), C(70, 62, 58),
        C(90, 100, 115), C(170, 200, 230), C(120, 128, 138), C(170, 210, 255), C(80, 90, 110),
        PAT_BIGBRICK, FP_FLAGSTONE, C(170, 200, 255), C(16, 20, 30), B_WEB, B_ICE },
    [FL_GLACIAL] = { "Glacial", "Glacial", 1, 2,
        C(110, 140, 170), C(150, 190, 220), C(210, 230, 250), C(130, 160, 190), C(170, 200, 225), C(80, 70, 66),
        C(100, 115, 130), C(200, 230, 255), C(170, 180, 195), C(180, 220, 255), C(120, 150, 180),
        PAT_ICE, FP_FLAGSTONE, C(140, 190, 255), C(20, 30, 44), B_WEB, B_ICE },
    [FL_EVERWINTER] = { "Everwinter", "Everwinter", 1, 3,
        C(150, 180, 205), C(200, 225, 245), C(240, 250, 255), C(170, 200, 225), C(210, 235, 250), C(90, 90, 100),
        C(120, 140, 160), C(120, 255, 220), C(200, 215, 230), C(150, 255, 220), C(160, 190, 220),
        PAT_ICE, FP_TILES, C(120, 255, 200), C(20, 34, 40), B_WEB, B_ICE },
    [FL_SANDSTONE] = { "Sandstone", "Sandstone", 2, 1,
        C(150, 112, 70), C(176, 136, 88), C(110, 80, 50), C(160, 124, 80), C(190, 150, 100), C(110, 76, 44),
        C(120, 90, 50), C(210, 170, 60), C(150, 120, 80), C(255, 200, 120), C(150, 90, 50),
        PAT_BIGBRICK, FP_TILES, C(255, 190, 90), C(30, 22, 14), B_BONES, 0 },
    [FL_SUNSCORCHED] = { "Sunscorched", "Sunscorched", 2, 2,
        C(160, 100, 60), C(190, 120, 70), C(120, 70, 40), C(180, 90, 50), C(210, 160, 100), C(120, 70, 40),
        C(150, 110, 50), C(240, 200, 70), C(170, 120, 70), C(255, 220, 140), C(170, 70, 40),
        PAT_PLASTER, FP_TILES, C(255, 210, 120), C(36, 22, 12), B_BONES, 0 },
    [FL_EMBER] = { "Ember", "Ember-forged", 2, 3,
        C(40, 30, 30), C(56, 40, 38), C(200, 70, 20), C(50, 36, 34), C(70, 50, 46), C(60, 36, 26),
        C(70, 56, 50), C(255, 120, 30), C(60, 40, 34), C(255, 140, 60), C(50, 30, 28),
        PAT_BIGBRICK, FP_FLAGSTONE, C(255, 90, 40), C(34, 10, 6), B_BONES, B_LAVA },
    [FL_BOYAR] = { "Boyar", "Boyar", 3, 1,
        C(80, 70, 60), C(110, 70, 40), C(50, 30, 20), C(120, 84, 50), C(150, 40, 40), C(120, 80, 46),
        C(70, 70, 60), C(220, 170, 50), C(80, 64, 46), C(255, 200, 120), C(40, 90, 60),
        PAT_LOGS, FP_PLANKS, C(255, 170, 90), C(20, 14, 10), B_CHAIN, 0 },
    [FL_STEPPE] = { "Steppe", "Steppe", 3, 2,
        C(110, 96, 72), C(140, 110, 80), C(80, 60, 40), C(110, 90, 60), C(120, 90, 60), C(120, 90, 60),
        C(90, 80, 60), C(180, 60, 40), C(110, 90, 60), C(255, 190, 120), C(130, 60, 40),
        PAT_PLASTER, FP_DIRT, C(255, 160, 80), C(24, 18, 12), B_BONES, 0 },
    [FL_JADE] = { "Jade", "Jade", 3, 3,
        C(60, 66, 60), C(150, 40, 36), C(30, 20, 20), C(50, 60, 55), C(170, 40, 36), C(120, 40, 30),
        C(60, 56, 50), C(60, 170, 120), C(70, 70, 60), C(255, 120, 90), C(40, 40, 44),
        PAT_PLASTER, FP_TILES, C(255, 90, 70), C(20, 10, 12), B_LANTERN, 0 },
    [FL_GOTHIC] = { "Gothic", "Gothic", 4, 1,
        C(50, 50, 56), C(72, 72, 80), C(30, 30, 34), C(60, 60, 66), C(90, 90, 100), C(60, 44, 36),
        C(56, 56, 62), C(140, 130, 150), C(54, 50, 50), C(200, 60, 90), C(40, 40, 50),
        PAT_BRICK, FP_TILES, C(255, 200, 120), C(10, 10, 16), B_WEB, 0 },
    [FL_DRUIDIC] = { "Druidic", "Druidic", 4, 2,
        C(60, 70, 52), C(80, 92, 66), C(40, 60, 30), C(70, 74, 52), C(100, 104, 88), C(70, 56, 36),
        C(60, 64, 56), C(120, 200, 90), C(60, 56, 40), C(140, 255, 120), C(50, 70, 40),
        PAT_ASHLAR, FP_DIRT, C(140, 255, 120), C(10, 18, 12), B_ROOTS, B_WATER },
    [FL_DROWNED] = { "Drowned", "Drowned", 4, 3,
        C(40, 70, 74), C(60, 100, 104), C(30, 50, 50), C(50, 84, 88), C(80, 120, 120), C(50, 60, 50),
        C(60, 80, 80), C(220, 110, 100), C(50, 70, 66), C(90, 240, 220), C(40, 80, 80),
        PAT_ASHLAR, FP_FLAGSTONE, C(90, 240, 220), C(6, 22, 26), B_ROOTS, B_WATER },
};
#undef C

const char *ARCH_NAMES[ARCH_COUNT] = { "rooms", "crawlspace", "caves", "mine", "cell block", "arena", "void" };
const char *STRUCT_NAMES[STRUCT_TYPE_COUNT] = { "none", "Grand Vestibule", "Great Hall", "Cavern", "Chasm" };

static Col col_transform(Col c, const StratumDef *s) {
    float r = c.r * s->bright, g = c.g * s->bright, b = c.b * s->bright;
    r = lerpf(r, s->tint.r, s->tint_amt);
    g = lerpf(g, s->tint.g, s->tint_amt);
    b = lerpf(b, s->tint.b, s->tint_amt);
    float l = 0.3f * r + 0.59f * g + 0.11f * b;
    r = lerpf(r, l, s->desat); g = lerpf(g, l, s->desat); b = lerpf(b, l, s->desat);
    Col o = { (u8)clampf(r, 0, 255), (u8)clampf(g, 0, 255), (u8)clampf(b, 0, 255) };
    return o;
}

static Col col_lerp(Col a, Col b, float t) {
    Col o = { (u8)lerpf(a.r, b.r, t), (u8)lerpf(a.g, b.g, t), (u8)lerpf(a.b, b.b, t) };
    return o;
}

static void light_levels(Col c, u8 out[3]) {
    /* Normalise so the brightest channel is 15: the block's light value scales it later. */
    int m = MAXI(c.r, MAXI(c.g, c.b));
    if (m == 0) m = 1;
    out[0] = (u8)((c.r * 15 + m / 2) / m);
    out[1] = (u8)((c.g * 15 + m / 2) / m);
    out[2] = (u8)((c.b * 15 + m / 2) / m);
}

void theme_info(int theme, ThemeInfo *t) {
    const StratumDef *s = &STRATA[theme_stratum(theme)];
    const FlavorDef *f = &FLAVORS[theme_flavor(theme)];
    memset(t, 0, sizeof *t);
    Col base[BLOCK_TEX_SLOTS] = { 0 };
    base[B_ROCK] = f->rock;
    base[B_WALL] = f->wall;
    base[B_FLOOR] = f->floor;
    base[B_CEIL] = col_lerp(f->wall, (Col){ 0, 0, 0 }, 0.25f);
    base[B_PILLAR] = f->pillar;
    base[B_WOOD] = f->wood;
    base[B_METAL] = f->metal;
    base[B_ACCENT] = f->accent;
    base[B_DIRT] = f->dirt;
    base[B_ROOF] = f->roof;
    base[B_ORE] = f->rock;
    for (int i = 0; i < BLOCK_TEX_SLOTS; i++) t->mat[i] = col_transform(base[i], s);
    /* Glowing and liquid things keep their own colour more strongly. */
    t->mat[B_LAVA] = (Col){ 255, 120, 30 };
    t->mat[B_WATER] = col_lerp((Col){ 20, 50, 70 }, f->glass, 0.15f);
    t->mat[B_CRYSTAL] = theme_stratum(theme) == ST_CORE ? (Col){ 255, 90, 220 } : col_lerp((Col){ 90, 200, 255 }, f->accent, 0.3f);
    t->mat[B_ICE] = (Col){ 150, 200, 235 };
    t->mat[B_BRAZIER] = col_transform(f->metal, s);
    t->mat[B_GLASS] = f->glass;
    t->mortar = col_transform(f->mortar, s);
    t->glass = f->glass;
    t->roof = col_transform(f->roof, s);
    t->wall_pat = f->wall_pat;
    t->floor_pat = f->floor_pat;
    if (theme_stratum(theme) == ST_HEAVEN || theme_stratum(theme) == ST_CLOUDS) t->wall_pat = PAT_MARBLE;

    Col torch = col_lerp(f->torch, s->light, s->light_mix);
    light_levels(torch, t->light[LIGHT_TORCH]);
    light_levels((Col){ 255, 110, 30 }, t->light[LIGHT_LAVA]);
    light_levels(t->mat[B_CRYSTAL], t->light[LIGHT_CRYSTAL]);
    light_levels(col_lerp(f->glass, (Col){ 150, 170, 220 }, 0.4f), t->light[LIGHT_GLASS]);
    light_levels(col_lerp((Col){ 80, 255, 170 }, f->accent, 0.25f), t->light[LIGHT_FUNGUS]);

    Col fog = col_lerp(f->fog, s->fog, s->fog_mix);
    t->fog[0] = fog.r / 255.0f; t->fog[1] = fog.g / 255.0f; t->fog[2] = fog.b / 255.0f;
    t->fog_density = s->fog_density;
    /* low levels stay pitch dark, high levels (heaven, clouds) are bright and tinted by the haze */
    float amb = powf(s->ambient / 15.0f, 1.5f) * 0.6f;
    t->ambient[0] = amb * (0.6f + t->fog[0]);
    t->ambient[1] = amb * (0.6f + t->fog[1]);
    t->ambient[2] = amb * (0.6f + t->fog[2]);
    t->sky = s->sky;
}

void theme_name(int theme, char *buf, int n) {
    snprintf(buf, (size_t)n, "%s %s", FLAVORS[theme_flavor(theme)].adj, STRATA[theme_stratum(theme)].name);
}
