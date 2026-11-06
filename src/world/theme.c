#include "world/theme.h"

/* One stratum per 10 chunks of height (the dungeon around the gate is a little thinner).
 * Fields: name, chunk-y band,
 *   archetype weights { rooms crawl cave mine cells arena void },
 *   megastructure weights { none vestibule hall cavern chasm },
 *   tint, tint amount, brightness, desaturation, fog, fog mix, fog density,
 *   light colour, light mix, ambient (0..15), stars, shaft chance,
 *   lava, crystal, fungus, roots,
 *   isles, wall pattern, ore, bony, fiery, stained glass, crystal colour */
#define P(x) ((x) + 1)
const StratumDef STRATA[ST_COUNT] = {
    /* ---------- down ---------- */
    [ST_CORE] = { "World Core", -100000, -90,
        { 1, 2, 6, 0, 0, 2, 2 }, { 0, 0, 1, 4, 2 },
        { 150, 50, 190 }, 0.40f, 0.85f, 0.0f, { 30, 6, 36 }, 0.6f, 0.030f,
        { 255, 80, 200 }, 0.4f, 2, false, 0.30f, 0.5f, 0.9f, 0.1f, 0.0f,
        ISLE_CRYSTAL, 0, false, false, true, false, { 255, 90, 220 } },
    [ST_MAW] = { "the Maw", -89, -80,
        { 1, 3, 7, 0, 0, 1, 0 }, { 0, 0, 0, 4, 2 },
        { 140, 40, 50 }, 0.50f, 0.75f, 0.0f, { 40, 4, 8 }, 0.7f, 0.040f,
        { 255, 60, 80 }, 0.5f, 2, false, 0.30f, 0.2f, 0.0f, 0.2f, 0.7f,
        ISLE_PLAIN, 0, false, true, true, false, { 0, 0, 0 } },
    [ST_IRONHELL] = { "Iron Hells", -79, -70,
        { 5, 1, 0, 2, 4, 2, 0 }, { 0, 0, 4, 0, 3 },
        { 90, 60, 50 }, 0.35f, 0.70f, 0.1f, { 30, 8, 4 }, 0.6f, 0.030f,
        { 255, 120, 40 }, 0.5f, 1, false, 0.35f, 0.3f, 0.0f, 0.0f, 0.0f,
        ISLE_PLAIN, 0, true, true, true, false, { 0, 0, 0 } },
    [ST_INFERNO] = { "Inferno", -69, -60,
        { 3, 2, 4, 0, 2, 3, 1 }, { 0, 0, 2, 2, 4 },
        { 160, 30, 15 }, 0.45f, 0.75f, 0.0f, { 40, 6, 2 }, 0.6f, 0.030f,
        { 255, 60, 20 }, 0.6f, 2, false, 0.30f, 0.6f, 0.0f, 0.0f, 0.0f,
        ISLE_PLAIN, 0, false, true, true, false, { 0, 0, 0 } },
    [ST_CRYSTAL] = { "Crystal Abyss", -59, -50,
        { 1, 1, 5, 0, 0, 2, 1 }, { 0, 0, 0, 4, 3 },
        { 60, 90, 140 }, 0.30f, 0.80f, 0.0f, { 6, 12, 24 }, 0.5f, 0.032f,
        { 120, 200, 255 }, 0.5f, 2, false, 0.30f, 0.0f, 1.0f, 0.0f, 0.0f,
        ISLE_CRYSTAL, 0, false, false, false, false, { 110, 220, 255 } },
    [ST_LIMBO] = { "Limbo", -49, -40,
        { 4, 2, 3, 0, 3, 2, 1 }, { 0, 0, 3, 1, 3 },
        { 110, 100, 120 }, 0.40f, 0.80f, 0.6f, { 22, 20, 26 }, 0.5f, 0.035f,
        { 180, 170, 200 }, 0.5f, 1, false, 0.30f, 0.0f, 0.1f, 0.2f, 0.0f,
        ISLE_PLAIN, 0, false, true, false, false, { 0, 0, 0 } },
    [ST_FUNGAL] = { "Fungal Deep", -39, -30,
        { 1, 2, 7, 0, 0, 1, 0 }, { 0, 0, 0, 5, 1 },
        { 60, 40, 90 }, 0.25f, 0.85f, 0.0f, { 20, 10, 30 }, 0.5f, 0.040f,
        { 120, 255, 180 }, 0.4f, 2, false, 0.30f, 0.0f, 0.1f, 1.0f, 0.3f,
        ISLE_PLAIN, 0, false, false, false, false, { 0, 0, 0 } },
    [ST_OSSUARY] = { "Ossuary", -29, -20,
        { 4, 3, 1, 0, 4, 1, 0 }, { 0, 0, 3, 1, 0 },
        { 190, 180, 150 }, 0.20f, 0.85f, 0.3f, { 20, 18, 14 }, 0.4f, 0.038f,
        { 230, 210, 160 }, 0.3f, 1, false, 0.30f, 0.0f, 0.0f, 0.0f, 0.0f,
        ISLE_PLAIN, 0, false, true, false, false, { 0, 0, 0 } },
    [ST_ROOTS] = { "Roots of the Mountain", -19, -10,
        { 1, 3, 7, 1, 0, 1, 0 }, { 0, 0, 0.5f, 5, 2 },
        { 70, 80, 40 }, 0.30f, 0.85f, 0.0f, { 12, 16, 8 }, 0.3f, 0.040f,
        { 0, 0, 0 }, 0.0f, 1, false, 0.30f, 0.0f, 0.3f, 0.8f, 0.9f,
        ISLE_PLAIN, 0, false, false, false, false, { 0, 0, 0 } },
    [ST_MINES] = { "Mines", -9, -4,
        { 2, 3, 2, 8, 0, 1, 0 }, { 0, 0, 1, 3, 3 },
        { 110, 80, 50 }, 0.25f, 0.90f, 0.0f, { 16, 12, 8 }, 0.3f, 0.040f,
        { 0, 0, 0 }, 0.0f, 1, false, 0.35f, 0.0f, 0.15f, 0.0f, 0.1f,
        ISLE_PLAIN, 0, true, false, false, false, { 0, 0, 0 } },
    /* ---------- the start ---------- */
    [ST_DUNGEON] = { "Dungeon", -3, 2,
        { 7, 2, 1, 1, 3, 2, 0 }, { 0, 0, 4, 1, 2 },
        { 0, 0, 0 }, 0.0f, 1.0f, 0.0f, { 0, 0, 0 }, 0.0f, 0.035f,
        { 0, 0, 0 }, 0.0f, 1, false, 0.30f, 0.0f, 0.0f, 0.0f, 0.0f,
        ISLE_PLAIN, 0, false, false, false, false, { 0, 0, 0 } },
    /* ---------- up ---------- */
    [ST_CASTLE] = { "Castle", 3, 9,
        { 8, 1, 0, 0, 2, 3, 0 }, { 0, 0, 6, 0, 1 },
        { 130, 110, 90 }, 0.10f, 1.10f, 0.0f, { 0, 0, 0 }, 0.0f, 0.030f,
        { 0, 0, 0 }, 0.0f, 1, false, 0.35f, 0.0f, 0.0f, 0.0f, 0.0f,
        ISLE_PLAIN, 0, false, false, false, false, { 0, 0, 0 } },
    [ST_SPIRES] = { "Spires", 10, 19,
        { 5, 1, 0, 0, 1, 2, 4 }, { 0, 0, 4, 0, 2 },
        { 120, 130, 150 }, 0.15f, 1.05f, 0.0f, { 26, 30, 42 }, 0.5f, 0.022f,
        { 0, 0, 0 }, 0.0f, 3, true, 0.35f, 0.0f, 0.0f, 0.0f, 0.0f,
        ISLE_PLAIN, 0, false, false, false, false, { 0, 0, 0 } },
    [ST_AERIE] = { "Eagle Aeries", 20, 29,
        { 3, 0, 1, 0, 0, 2, 5 }, { 0, 0, 2, 0, 2 },
        { 170, 160, 140 }, 0.15f, 1.10f, 0.1f, { 60, 66, 80 }, 0.5f, 0.022f,
        { 255, 230, 190 }, 0.2f, 4, true, 0.35f, 0.0f, 0.0f, 0.0f, 0.1f,
        ISLE_NEST, 0, false, true, false, false, { 0, 0, 0 } },
    [ST_CLOUDS] = { "Cloud Cloisters", 30, 39,
        { 3, 0, 0, 0, 0, 2, 7 }, { 0, 0, 3, 0, 1 },
        { 200, 205, 215 }, 0.35f, 1.25f, 0.2f, { 110, 115, 128 }, 0.75f, 0.020f,
        { 220, 230, 255 }, 0.3f, 7, false, 0.35f, 0.0f, 0.0f, 0.0f, 0.0f,
        ISLE_TEMPLE, P(PAT_MARBLE), false, false, false, false, { 0, 0, 0 } },
    [ST_GARDENS] = { "Hanging Gardens", 40, 49,
        { 3, 0, 1, 0, 0, 1, 6 }, { 0, 0, 2, 1, 1 },
        { 120, 180, 110 }, 0.25f, 1.20f, 0.0f, { 90, 120, 90 }, 0.6f, 0.020f,
        { 220, 255, 200 }, 0.3f, 7, false, 0.35f, 0.0f, 0.0f, 0.2f, 0.8f,
        ISLE_GARDEN, 0, false, false, false, false, { 0, 0, 0 } },
    [ST_HEAVEN] = { "Heaven", 50, 59,
        { 3, 0, 0, 0, 0, 3, 7 }, { 0, 0, 5, 0, 0 },
        { 240, 225, 180 }, 0.45f, 1.40f, 0.2f, { 160, 150, 120 }, 0.85f, 0.016f,
        { 255, 235, 180 }, 0.6f, 9, false, 0.35f, 0.0f, 0.2f, 0.0f, 0.0f,
        ISLE_TEMPLE, P(PAT_MARBLE), false, false, false, true, { 0, 0, 0 } },
    [ST_SUNCOURT] = { "Sun Court", 60, 69,
        { 4, 0, 0, 0, 0, 4, 3 }, { 0, 0, 5, 0, 0 },
        { 255, 210, 120 }, 0.40f, 1.45f, 0.1f, { 200, 160, 90 }, 0.8f, 0.014f,
        { 255, 220, 140 }, 0.6f, 11, false, 0.35f, 0.0f, 0.0f, 0.0f, 0.0f,
        ISLE_TEMPLE, P(PAT_MARBLE), false, false, true, true, { 0, 0, 0 } },
    [ST_STORM] = { "Stormcrown", 70, 79,
        { 2, 0, 0, 0, 0, 2, 7 }, { 0, 0, 2, 0, 2 },
        { 80, 90, 120 }, 0.40f, 0.90f, 0.2f, { 40, 46, 64 }, 0.7f, 0.025f,
        { 170, 200, 255 }, 0.5f, 4, true, 0.35f, 0.0f, 0.3f, 0.0f, 0.0f,
        ISLE_CRYSTAL, 0, false, false, false, false, { 170, 210, 255 } },
    [ST_ORRERY] = { "Celestial Orrery", 80, 89,
        { 2, 0, 0, 0, 0, 3, 6 }, { 0, 0, 3, 0, 1 },
        { 60, 50, 110 }, 0.45f, 0.90f, 0.0f, { 10, 8, 30 }, 0.8f, 0.012f,
        { 200, 170, 255 }, 0.6f, 3, true, 0.35f, 0.0f, 0.4f, 0.0f, 0.0f,
        ISLE_ORRERY, 0, false, false, false, false, { 190, 150, 255 } },
    [ST_ASTRAL] = { "Astral Void", 90, 100000,
        { 1, 0, 1, 0, 0, 1, 9 }, { 0, 0, 1, 1, 0 },
        { 40, 30, 70 }, 0.50f, 0.70f, 0.3f, { 3, 2, 8 }, 0.8f, 0.010f,
        { 180, 120, 255 }, 0.6f, 2, true, 0.30f, 0.0f, 0.5f, 0.0f, 0.0f,
        ISLE_CRYSTAL, 0, false, false, false, false, { 0, 0, 0 } },
};
#undef P

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
        PAT_BRICK, FP_TILES, C(255, 200, 120), C(10, 10, 16), B_WEB, 0, true },
    [FL_DRUIDIC] = { "Druidic", "Druidic", 4, 2,
        C(60, 70, 52), C(80, 92, 66), C(40, 60, 30), C(70, 74, 52), C(100, 104, 88), C(70, 56, 36),
        C(60, 64, 56), C(120, 200, 90), C(60, 56, 40), C(140, 255, 120), C(50, 70, 40),
        PAT_ASHLAR, FP_DIRT, C(140, 255, 120), C(10, 18, 12), B_ROOTS, B_WATER },
    [FL_DROWNED] = { "Drowned", "Drowned", 4, 3,
        C(40, 70, 74), C(60, 100, 104), C(30, 50, 50), C(50, 84, 88), C(80, 120, 120), C(50, 60, 50),
        C(60, 80, 80), C(220, 110, 100), C(50, 70, 66), C(90, 240, 220), C(40, 80, 80),
        PAT_ASHLAR, FP_FLAGSTONE, C(90, 240, 220), C(6, 22, 26), B_ROOTS, B_WATER },
    /* ---------- north, tiers 4-10: past the ice, into the cold that ends things ---------- */
    [FL_RIMEIRON] = { "Rime-iron", "Rime-iron", 1, 4,
        C(56, 62, 72), C(70, 76, 88), C(200, 220, 240), C(60, 66, 76), C(90, 98, 112), C(60, 56, 56),
        C(110, 120, 135), C(180, 220, 255), C(90, 96, 106), C(160, 210, 255), C(60, 70, 86),
        PAT_BIGBRICK, FP_GRATE, C(160, 200, 255), C(14, 18, 26), B_CHAIN, B_ICE },
    [FL_BARROW] = { "Barrow", "Barrow-cold", 1, 5,
        C(86, 92, 86), C(110, 116, 108), C(60, 66, 60), C(90, 96, 88), C(130, 136, 126), C(66, 60, 52),
        C(80, 86, 84), C(190, 220, 190), C(96, 100, 90), C(170, 230, 200), C(70, 80, 72),
        PAT_ASHLAR, FP_DIRT, C(180, 240, 210), C(12, 18, 16), B_BONES, B_ICE },
    [FL_TUSK] = { "Tusk", "Tusk-built", 1, 6,
        C(120, 110, 96), C(220, 210, 186), C(110, 80, 56), C(130, 104, 80), C(236, 226, 200), C(120, 84, 56),
        C(100, 90, 80), C(160, 60, 40), C(140, 120, 96), C(255, 210, 150), C(110, 76, 50),
        PAT_LOGS, FP_PLANKS, C(255, 180, 100), C(22, 18, 16), B_BONES, B_ICE },
    [FL_AURORA] = { "Aurora", "Aurora-lit", 1, 7,
        C(30, 40, 64), C(40, 52, 86), C(80, 255, 180), C(34, 44, 72), C(60, 76, 120), C(50, 46, 60),
        C(70, 80, 110), C(120, 255, 200), C(50, 60, 80), C(200, 110, 255), C(30, 50, 80),
        PAT_RUNES, FP_MOSAIC, C(110, 255, 190), C(8, 20, 30), B_LANTERN, B_ICE },
    [FL_STARFROST] = { "Starfrost", "Starfrost", 1, 8,
        C(20, 26, 40), C(30, 40, 60), C(140, 180, 230), C(26, 34, 52), C(80, 110, 150), C(40, 40, 56),
        C(90, 110, 140), C(220, 240, 255), C(40, 50, 70), C(220, 240, 255), C(30, 40, 60),
        PAT_ICE, FP_MOSAIC, C(220, 235, 255), C(4, 6, 14), B_WEB, B_ICE },
    [FL_HOLLOW] = { "Hollow Winter", "Hollow-wintered", 1, 9,
        C(200, 204, 210), C(226, 230, 236), C(170, 176, 186), C(210, 214, 222), C(240, 242, 246), C(150, 150, 156),
        C(170, 176, 186), C(120, 130, 150), C(196, 200, 208), C(230, 240, 255), C(190, 196, 206),
        PAT_PLASTER, FP_FLAGSTONE, C(235, 240, 255), C(40, 42, 48), B_WEB, B_ICE },
    [FL_LASTCOLD] = { "Last Cold", "Lifeless", 1, 10,
        C(24, 22, 36), C(34, 30, 52), C(150, 130, 220), C(28, 26, 42), C(50, 46, 74), C(40, 36, 50),
        C(60, 56, 80), C(170, 150, 255), C(30, 28, 40), C(170, 150, 255), C(30, 28, 44),
        PAT_BASALT, FP_FLAGSTONE, C(170, 150, 255), C(4, 3, 10), 0, B_ICE },

    /* ---------- south, tiers 4-10: from hot stone to the heart of the sun ---------- */
    [FL_OBSIDIAN] = { "Obsidian", "Obsidian", 2, 4,
        C(24, 20, 26), C(34, 28, 38), C(255, 110, 30), C(30, 26, 32), C(50, 42, 54), C(50, 30, 24),
        C(60, 50, 50), C(255, 130, 40), C(40, 30, 30), C(255, 120, 50), C(30, 24, 30),
        PAT_BASALT, FP_FLAGSTONE, C(255, 110, 40), C(30, 8, 4), B_BONES, B_LAVA },
    [FL_BRASS] = { "Brass", "Brass-wrought", 2, 5,
        C(90, 72, 50), C(170, 130, 60), C(80, 56, 30), C(120, 96, 60), C(200, 160, 80), C(100, 70, 40),
        C(190, 150, 70), C(240, 200, 100), C(110, 86, 56), C(255, 220, 140), C(150, 110, 50),
        PAT_BIGBRICK, FP_GRATE, C(255, 210, 110), C(30, 22, 12), B_CHAIN, 0 },
    [FL_SALT] = { "Salt", "Salt-crusted", 2, 6,
        C(210, 190, 186), C(236, 220, 214), C(200, 170, 166), C(226, 210, 204), C(244, 234, 230), C(140, 110, 90),
        C(160, 150, 150), C(230, 140, 150), C(214, 196, 190), C(255, 210, 220), C(220, 190, 186),
        PAT_PLASTER, FP_TILES, C(255, 240, 230), C(44, 36, 34), B_BONES, 0 },
    [FL_SULPHUR] = { "Sulphur", "Sulphurous", 2, 7,
        C(130, 120, 50), C(160, 150, 60), C(90, 80, 30), C(140, 130, 56), C(180, 170, 80), C(90, 70, 40),
        C(110, 100, 60), C(220, 230, 70), C(150, 140, 60), C(220, 255, 80), C(120, 110, 40),
        PAT_ASHLAR, FP_DIRT, C(220, 255, 100), C(30, 32, 6), B_BONES, B_WATER },
    [FL_GLASSBLOWN] = { "Glassblown", "Glass-blown", 2, 8,
        C(150, 110, 60), C(200, 140, 60), C(120, 70, 30), C(190, 150, 90), C(230, 170, 80), C(120, 80, 40),
        C(150, 110, 60), C(255, 170, 60), C(200, 160, 100), C(255, 180, 80), C(170, 110, 50),
        PAT_ICE, FP_TILES, C(255, 190, 90), C(36, 22, 8), B_LANTERN, 0, true },
    [FL_PHOENIX] = { "Phoenix", "Phoenix", 2, 9,
        C(120, 40, 30), C(190, 60, 30), C(255, 190, 60), C(150, 50, 30), C(230, 150, 50), C(120, 50, 30),
        C(200, 150, 60), C(255, 210, 80), C(140, 60, 40), C(255, 160, 60), C(200, 70, 30),
        PAT_SCALES, FP_MOSAIC, C(255, 170, 60), C(40, 12, 4), B_LANTERN, B_LAVA },
    [FL_SUNBLAST] = { "Sunblast", "Sun-blasted", 2, 10,
        C(230, 200, 140), C(255, 236, 180), C(255, 200, 90), C(240, 214, 150), C(255, 246, 210), C(200, 150, 90),
        C(240, 210, 140), C(255, 255, 200), C(230, 200, 150), C(255, 250, 200), C(250, 220, 150),
        PAT_MARBLE, FP_MOSAIC, C(255, 250, 210), C(60, 50, 30), 0, B_LAVA },

    /* ---------- east, tiers 4-10: further into the empire of the dawn ---------- */
    [FL_PORCELAIN] = { "Porcelain", "Porcelain", 3, 4,
        C(170, 176, 186), C(226, 230, 238), C(60, 90, 170), C(200, 206, 216), C(240, 242, 248), C(110, 80, 60),
        C(120, 130, 150), C(50, 90, 200), C(160, 160, 166), C(180, 210, 255), C(50, 70, 140),
        PAT_PLASTER, FP_MOSAIC, C(200, 220, 255), C(16, 18, 26), B_LANTERN, 0 },
    [FL_BAMBOO] = { "Bamboo", "Bamboo", 3, 5,
        C(90, 100, 60), C(170, 180, 90), C(80, 90, 40), C(150, 140, 80), C(140, 170, 70), C(160, 150, 80),
        C(90, 90, 60), C(200, 60, 40), C(100, 90, 60), C(255, 220, 160), C(80, 110, 50),
        PAT_LOGS, FP_PLANKS, C(255, 200, 130), C(14, 20, 10), B_LANTERN, B_WATER },
    [FL_TERRACOTTA] = { "Terracotta", "Terracotta", 3, 6,
        C(130, 80, 56), C(170, 96, 64), C(110, 64, 44), C(150, 90, 60), C(190, 120, 80), C(110, 70, 46),
        C(90, 70, 56), C(90, 150, 130), C(140, 96, 66), C(255, 180, 120), C(150, 70, 46),
        PAT_BIGBRICK, FP_DIRT, C(255, 170, 100), C(26, 14, 10), B_CHAIN, 0 },
    [FL_SILK] = { "Silk", "Silk-hung", 3, 7,
        C(90, 60, 80), C(150, 70, 120), C(230, 180, 90), C(120, 70, 100), C(200, 150, 80), C(110, 60, 70),
        C(140, 110, 80), C(240, 190, 90), C(110, 80, 90), C(255, 150, 210), C(130, 40, 90),
        PAT_PLASTER, FP_TILES, C(255, 150, 200), C(24, 10, 20), B_LANTERN, 0 },
    [FL_DRAGON] = { "Dragon", "Dragon-scaled", 3, 8,
        C(30, 70, 70), C(40, 120, 110), C(220, 180, 60), C(40, 90, 86), C(60, 150, 140), C(90, 50, 30),
        C(170, 140, 60), C(240, 200, 70), C(50, 80, 70), C(140, 230, 255), C(30, 100, 90),
        PAT_SCALES, FP_MOSAIC, C(150, 220, 255), C(6, 20, 22), B_LANTERN, B_WATER },
    [FL_MOONLIT] = { "Moonlit", "Moonlit", 3, 9,
        C(40, 44, 76), C(56, 60, 110), C(190, 200, 230), C(50, 54, 90), C(160, 170, 210), C(60, 50, 70),
        C(170, 180, 210), C(210, 220, 255), C(50, 50, 80), C(200, 220, 255), C(40, 46, 96),
        PAT_PLASTER, FP_TILES, C(200, 215, 255), C(8, 10, 24), B_LANTERN, B_WATER },
    [FL_IMPERIAL] = { "Imperial", "Imperial", 3, 10,
        C(110, 40, 30), C(190, 40, 30), C(255, 200, 70), C(150, 40, 30), C(230, 180, 60), C(130, 40, 30),
        C(220, 180, 70), C(255, 220, 90), C(120, 60, 40), C(255, 210, 120), C(220, 170, 50),
        PAT_RUNES, FP_MOSAIC, C(255, 200, 90), C(30, 8, 6), B_LANTERN, 0 },

    /* ---------- west, tiers 4-10: down under the sea, to what sleeps there ---------- */
    [FL_CORAL] = { "Coral", "Coral-grown", 4, 4,
        C(170, 100, 90), C(230, 130, 110), C(250, 200, 170), C(200, 180, 140), C(250, 160, 130), C(120, 80, 70),
        C(110, 130, 130), C(255, 120, 150), C(210, 190, 150), C(110, 240, 230), C(220, 110, 100),
        PAT_SCALES, FP_DIRT, C(120, 240, 230), C(10, 30, 36), B_ROOTS, B_WATER },
    [FL_ABYSSAL] = { "Abyssal", "Abyssal", 4, 5,
        C(16, 22, 40), C(22, 30, 56), C(150, 90, 255), C(20, 26, 46), C(36, 46, 80), C(30, 30, 40),
        C(50, 60, 90), C(170, 110, 255), C(24, 30, 46), C(130, 120, 255), C(20, 26, 50),
        PAT_BASALT, FP_FLAGSTONE, C(150, 120, 255), C(2, 6, 16), B_ROOTS, B_WATER },
    [FL_VERDIGRIS] = { "Verdigris", "Verdigris", 4, 6,
        C(60, 90, 80), C(80, 140, 120), C(40, 70, 60), C(70, 110, 96), C(110, 170, 150), C(70, 60, 50),
        C(90, 160, 140), C(180, 220, 160), C(70, 90, 80), C(90, 255, 200), C(60, 120, 100),
        PAT_BRICK, FP_GRATE, C(120, 255, 210), C(6, 22, 20), B_CHAIN, B_WATER, true },
    [FL_PEARL] = { "Pearl", "Pearlescent", 4, 7,
        C(180, 180, 196), C(230, 224, 236), C(200, 190, 220), C(210, 206, 220), C(246, 240, 250), C(150, 140, 150),
        C(190, 190, 210), C(255, 200, 230), C(190, 186, 200), C(220, 240, 255), C(210, 200, 226),
        PAT_MARBLE, FP_TILES, C(230, 230, 255), C(20, 24, 34), B_LANTERN, B_WATER },
    [FL_LEVIATHAN] = { "Leviathan", "Leviathan", 4, 8,
        C(90, 60, 60), C(210, 196, 170), C(130, 40, 40), C(120, 70, 66), C(230, 220, 196), C(150, 70, 60),
        C(110, 90, 80), C(200, 60, 60), C(110, 70, 60), C(120, 220, 210), C(160, 60, 60),
        PAT_LOGS, FP_DIRT, C(120, 230, 220), C(16, 10, 12), B_BONES, B_WATER },
    [FL_BRINE] = { "Brine", "Brine-rotted", 4, 9,
        C(70, 60, 40), C(110, 80, 50), C(60, 80, 40), C(90, 76, 50), C(130, 100, 60), C(80, 60, 40),
        C(130, 70, 40), C(150, 200, 80), C(80, 70, 50), C(160, 230, 100), C(100, 70, 40),
        PAT_ASHLAR, FP_DIRT, C(170, 240, 110), C(14, 20, 8), B_ROOTS, B_WATER },
    [FL_ELDRITCH] = { "Eldritch", "Eldritch", 4, 10,
        C(36, 30, 50), C(50, 40, 70), C(110, 255, 140), C(40, 34, 56), C(70, 56, 96), C(40, 50, 40),
        C(60, 80, 70), C(120, 255, 150), C(40, 40, 50), C(120, 255, 160), C(40, 30, 60),
        PAT_RUNES, FP_FLAGSTONE, C(120, 255, 160), C(4, 12, 8), B_FUNGUS, B_WATER },
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
    bool own_crystal = s->crystal_col.r || s->crystal_col.g || s->crystal_col.b;
    t->mat[B_CRYSTAL] = own_crystal ? s->crystal_col : col_lerp((Col){ 90, 200, 255 }, f->accent, 0.3f);
    t->mat[B_ICE] = (Col){ 150, 200, 235 };
    t->mat[B_BRAZIER] = col_transform(f->metal, s);
    t->mat[B_GLASS] = f->glass;
    t->mortar = col_transform(f->mortar, s);
    t->glass = f->glass;
    t->roof = col_transform(f->roof, s);
    t->wall_pat = f->wall_pat;
    t->floor_pat = f->floor_pat;
    if (s->wall_pat) t->wall_pat = (u8)(s->wall_pat - 1);

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
