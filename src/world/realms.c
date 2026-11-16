#include "world/realms.h"

#include "core/noise.h"
#include "game/alchemy.h"
#include "game/food.h"
#include "world/block.h"
#include "world/theme.h"

/* The twenty-one realms (docs/REALMS.md). The numbers: danger (monster level), the land, its
 * rules, its sky. The story lines name the fortress (%F), its builders (%B), what was sealed
 * below (%S) and the night it fell (%N), from the world's history. */

#define SKY(r, g, b) { r, g, b }
#define FOG(r, g, b) { r, g, b }

const RealmDef REALMS[REALM_COUNT] = {
    [RM_PALE_MOON] = { "the Pale Moon", "the Silver Keep", "the Silver King",
        "Low weight, no air outside the domes. Frost lilies hold a breath.",
        "The Silver King swore to watch over %F from his moon. On %N he looked away; the "
        "domes of his keep still face the fortress, and he will not say what he saw.",
        ST_SPIRES, 6, TR_CRATER, FE_DOMES, 0, -40, 10, 0.4f, -8.0f, SKY(10, 11, 13), FOG(0.02f, 0.025f, 0.05f), 0.002f, 1.0f, 0.6f,
        RR_AIRLESS },
    [RM_RED_MOON] = { "the Red Moon", "the Ember Keep", "the Ember King",
        "Rust storms, iron spires, and wyrms that eat the iron.",
        "The Ember King sold %B the iron for the doors of %F. When %S woke he asked for it "
        "back, and took the wrong kind of payment.",
        ST_IRONHELL, 10, TR_CRATER, FE_SPIRES, 0, -40, 14, 0.5f, 22.0f, SKY(12, 6, 4), FOG(0.16f, 0.06f, 0.03f), 0.006f, 0.6f, 0.6f,
        RR_SANDSTORM },
    [RM_BLACK_MOON] = { "the Black Moon", "the Hollow Keep", "the Hollow King",
        "No light but what lives. Fungus forests; the Herald came from here.",
        "The Herald that walked the halls of %F before %N came from the Hollow King's moon. "
        "He sent it, or it left him: the fungus remembers which.",
        ST_LIMBO, 16, TR_CRATER, FE_MUSHROOMS, 0, -40, 8, 0.45f, 4.0f, SKY(0, 0, 0), FOG(0.0f, 0.0f, 0.01f), 0.01f, 0.3f, 0.0f,
        RR_DARK },
    [RM_SUN] = { "the Fortress at the Heart of the Sun", "the Sun Court", "the Sun Regent",
        "It burns without a sun-ward. Rivers of light, flowers of glass.",
        "The Sun Court judged %B once and found them wanting. %F was built in the shade "
        "of that judgement; the Regent still keeps the ledger.",
        ST_SUNCOURT, 26, TR_PLAIN, FE_LIGHTRIVERS, B_LAVA, -4, 12, 1.0f, 55.0f, SKY(15, 13, 9), FOG(0.5f, 0.35f, 0.12f), 0.004f, 0.0f, 0.0f,
        RR_SCORCH },
    [RM_FORGE] = { "the Forge-World", "the Anvil Keep", "the Forgemaster",
        "Magma seas, star-iron and mithril in the rock. The keys were forged here.",
        "Every key of %F was struck on this anvil. The Forgemaster made one more than "
        "%B paid for, and on %N it was used.",
        ST_INFERNO, 20, TR_PEAKS, FE_MAGMA, B_LAVA, -8, 22, 1.0f, 38.0f, SKY(11, 6, 4), FOG(0.22f, 0.08f, 0.03f), 0.007f, 0.0f, 0.0f,
        0 },
    [RM_GLASS_DESERT] = { "the Glass Desert", "the Exile's Tower", "the Exile",
        "Scorching days, freezing nights, sandstorms. Glass cacti hold water.",
        "Who opened the way to %S was sent here, to a desert of glass that remembers every "
        "footstep. The Exile has had a long time to think about %N.",
        ST_MAW, 8, TR_DUNES, FE_SHARDS, 0, -40, 8, 1.0f, 20.0f, SKY(14, 12, 9), FOG(0.45f, 0.36f, 0.22f), 0.004f, 0.4f, 0.3f,
        RR_DAYNIGHT | RR_SANDSTORM },
    [RM_OCEAN_KEEP] = { "the Keep under the Ocean", "the Drowned Keep", "the Drowned Queen",
        "All under water. Air pods give a breath; a gill draught gives more.",
        "The faction that drowned for %F did not die. They wait under the sea for the "
        "debt of %N to be paid, and they count in tides.",
        ST_FUNGAL, 14, TR_DEEP, FE_KELP, B_WATER, 60, 8, 1.0f, 8.0f, SKY(2, 4, 7), FOG(0.02f, 0.08f, 0.14f), 0.02f, 0.0f, 0.0f,
        0 },
    [RM_ICELANDS] = { "the Icelands", "the Frozen Hall", "the Frost Jarl",
        "Bitter cold and wind. The frozen dead remember, if you can warm them.",
        "Those who fled %F on %N walked into the cold and never came out. The Frost Jarl "
        "keeps them frozen as they were, still holding what they took.",
        ST_MINES, 5, TR_PEAKS, FE_ICEBERGS, B_ICE, -4, 18, 1.0f, -22.0f, SKY(11, 12, 14), FOG(0.6f, 0.65f, 0.72f), 0.006f, 0.3f, 0.3f,
        RR_WIND },
    [RM_SEA_OF_NIGHT] = { "the Sea of Night", "the Tide Keep", "the Tide Warden",
        "The tide follows the moons. The causeway to the keep is open at low water.",
        "On %N the three moons stood in a line and the sea came up the stair of %F. The "
        "Tide Warden let it; ask him why at low water.",
        ST_DUNGEON, 9, TR_SEA, FE_CAUSEWAY, B_WATER, -2, 10, 1.0f, 6.0f, SKY(5, 6, 10), FOG(0.03f, 0.04f, 0.09f), 0.004f, 1.0f, 1.0f,
        RR_TIDE },
    [RM_STORM_PEAKS] = { "the Storm Peaks", "the Thunder Eyrie", "the Storm Lord",
        "Lightning out of the sky, wind off the peaks. Lightning trees hold the charge.",
        "The Storm Lord's lightning lit the forges of %B. It struck %F on %N too, and "
        "he swears it struck by itself.",
        ST_STORM, 18, TR_ISLES, FE_CONDUCTORS, 0, -40, 0, 1.0f, 2.0f, SKY(6, 7, 9), FOG(0.12f, 0.13f, 0.17f), 0.006f, 0.0f, 0.0f,
        RR_STORM | RR_WIND },
    [RM_SHINING_COURT] = { "the Shining Court", "the Hall of Names", "the Herald of Names",
        "A cloud world of the dead of the history. The ledger, made visible.",
        "Everyone who died for %F walks the clouds here, and every deed is written on the "
        "air. Your name is there too; read what it says.",
        ST_HEAVEN, 12, TR_ISLES, FE_CLOUDS, 0, -40, 0, 0.8f, 16.0f, SKY(14, 14, 12), FOG(0.75f, 0.74f, 0.66f), 0.003f, 0.0f, 0.0f,
        0 },
    [RM_ASTRAL_SHORE] = { "the Astral Shore", "the Last Lighthouse", "the Keeper of the Shore",
        "No weight at all. Star whales. Open only after the end of the story.",
        "Beyond the last chapter of %F lies a shore where stories wash up. The Keeper "
        "reads them all, and yours is still wet.",
        ST_ASTRAL, 30, TR_ISLES, FE_STARROCKS, 0, -40, 0, 0.12f, -4.0f, SKY(8, 8, 12), FOG(0.03f, 0.02f, 0.06f), 0.0015f, 1.0f, 0.4f,
        RR_FLOAT | RR_STORY },
    [RM_HOLLOW_EARTH] = { "the Hollow Earth", "the Walled Hall", "the First Elder",
        "An inner sun, giant ferns, and the first people, walled in.",
        "Before %B there were others in the deep. They were walled in to make room for "
        "%F, and the wall was never meant to be opened.",
        ST_ROOTS, 11, TR_CAVERN, FE_INNERSUN, B_WATER, -5, 10, 1.0f, 24.0f, SKY(0, 0, 0), FOG(0.14f, 0.12f, 0.06f), 0.006f, 0.0f, 0.0f,
        0 },
    [RM_FUNGAL_DEEP] = { "the Fungal Deep", "the Spore Throne", "the Mushroom Elder",
        "Spores in the air; the mycelium tells the monsters where you step.",
        "The mycelium under %F reaches all the way here. It felt %S wake on %N, and the "
        "Mushroom Elder has been listening ever since.",
        ST_FUNGAL, 7, TR_CAVERN, FE_STALKS, B_WATER, -6, 8, 1.0f, 14.0f, SKY(0, 0, 0), FOG(0.04f, 0.08f, 0.06f), 0.012f, 0.0f, 0.0f,
        RR_SPORES | RR_DARK },
    [RM_BONE_FIELDS] = { "the Bone Fields", "the Ossuary Crown", "the Bone Sovereign",
        "Giant skeletons, and rot that rises from them.",
        "Something enormous died here before %F was a plan. %B carved their first doors "
        "from its ribs; the Bone Sovereign wants them back.",
        ST_OSSUARY, 13, TR_PLAIN, FE_RIBS, 0, -40, 6, 1.0f, 10.0f, SKY(9, 8, 6), FOG(0.22f, 0.2f, 0.14f), 0.007f, 0.2f, 0.6f,
        RR_MIASMA },
    [RM_ORRERY] = { "the Clockwork Orrery", "the Brass Observatory", "the Clockmaker",
        "The moons on their rods. Turn them, and the fortress sees an eclipse.",
        "The Clockmaker set the moons in their courses for %B. On %N someone stopped the "
        "clock for an hour; the Clockmaker kept the hour.",
        ST_ORRERY, 15, TR_ISLES, FE_RINGS, 0, -40, 0, 1.0f, 12.0f, SKY(7, 7, 11), FOG(0.04f, 0.04f, 0.08f), 0.002f, 1.0f, 1.0f,
        0 },
    [RM_LIBRARY] = { "the Library of Ash", "the Burned Archive", "the Archivist",
        "Shelves in the ash. The pages answer questions of the history.",
        "Every account of %F that was burned came here as ash. The Archivist gathers the "
        "pages; some of them are the true account of %N.",
        ST_LIMBO, 10, TR_PLAIN, FE_SHELVES, 0, -40, 4, 1.0f, 18.0f, SKY(8, 6, 5), FOG(0.2f, 0.16f, 0.13f), 0.008f, 0.0f, 0.0f,
        0 },
    [RM_GARDEN] = { "the Garden of the First Seed", "the Gardener's Lodge", "the Gardener",
        "Things grow twenty times as fast. Rare seeds; the refuge of the spared.",
        "The first seed of every garden of %F came from here. Those you spared find their "
        "way here in the end, and the Gardener asks nothing of them.",
        ST_GARDENS, 3, TR_PLAIN, FE_GROVES, B_WATER, -3, 8, 1.0f, 20.0f, SKY(13, 14, 10), FOG(0.4f, 0.5f, 0.35f), 0.003f, 0.0f, 0.0f,
        RR_GROWTH },
    [RM_FALLEN_STAR] = { "the Fallen Star", "the Crater Keep", "the Star-Touched",
        "Star-iron in a crater, and crystal that drinks your mana.",
        "A star fell the year %B laid the first stone of %F. They took its iron for the "
        "seals; the crystal that grew in its place wants it back.",
        ST_CRYSTAL, 17, TR_CRATER, FE_CRYSTALS, 0, -40, 10, 0.8f, 10.0f, SKY(8, 8, 12), FOG(0.05f, 0.05f, 0.12f), 0.004f, 1.0f, 0.3f,
        RR_DRAIN },
    [RM_MIRROR] = { "the Mirror Fortress", "the Mirror Throne", "your Reflection",
        "The fortress, the wrong way round. What you did, undone.",
        "Here %F stands the other way round, and every deed you did is undone: the spared "
        "lie dead, the dead stand. Your reflection keeps the throne.",
        ST_CASTLE, 22, TR_MIRROR, FE_NONE, 0, -40, 0, 1.0f, 12.0f, SKY(0, 0, 0), FOG(0.05f, 0.05f, 0.07f), 0.008f, 0.0f, 0.0f,
        RR_DARK },
    [RM_DREAM] = { "the Dream of the Sleeper", "the Sleeper's Bed", "the Sleeper",
        "Pieces of places, stitched. Gravity shifts. Open after a finale that spared.",
        "What was sealed under %F sleeps, and you spared it. This is its dream: %F as it "
        "remembers it, and you in it.",
        ST_CLOUDS, 28, TR_DREAM, FE_NONE, 0, -40, 0, 1.0f, 12.0f, SKY(10, 8, 12), FOG(0.25f, 0.18f, 0.3f), 0.006f, 0.8f, 1.0f,
        RR_SHIFT | RR_SPARED },
};

#define C3(r, g, b) { r, g, b }
#define V3(a, b, c) { a, b, c }

const FloraDef REALM_FLORA[REALM_FLORA_COUNT] = {
    /* the Pale Moon */
    { "dust clover", V3("grey", "silver", "pale blue"), FS_TUFT, C3(150, 156, 160), { C3(190, 196, 200), C3(220, 224, 232), C3(160, 190, 230) }, FX_FOOD, FD_HERB, -1 },
    { "frost lily", V3("white", "blue", "silver-veined"), FS_BLOOM, C3(110, 140, 150), { C3(240, 246, 255), C3(140, 190, 255), C3(210, 220, 235) }, FX_AIR, -1, RG_DEW },
    { "moonglass bell", V3("milky", "opal", "shining"), FS_ORB, C3(120, 130, 150), { C3(220, 225, 235), C3(200, 230, 220), C3(255, 255, 255) }, FX_MANA, -1, RG_QUICKSILVER },
    /* the Red Moon */
    { "rust lichen", V3("orange", "crimson", "brown"), FS_TUFT, C3(120, 60, 30), { C3(200, 100, 40), C3(170, 40, 30), C3(130, 80, 50) }, FX_FOOD, FD_MOSS, -1 },
    { "blood-reed", V3("dark", "bright", "black-tipped"), FS_REED, C3(110, 30, 25), { C3(150, 20, 25), C3(220, 40, 40), C3(60, 20, 20) }, FX_HEAL, FD_PEPPER, RG_ICHOR },
    { "ember thistle", V3("red", "orange", "white-hot"), FS_SPIKE, C3(90, 40, 30), { C3(240, 70, 30), C3(255, 150, 40), C3(255, 230, 190) }, FX_THORNS, -1, RG_BRIMSTONE },
    /* the Black Moon */
    { "shadow fern", V3("black", "violet", "ink-blue"), FS_FERN, C3(30, 25, 40), { C3(40, 35, 50), C3(80, 40, 110), C3(30, 40, 90) }, FX_FOOD, FD_HERB, -1 },
    { "hollow cap", V3("grey", "violet", "ghost-white"), FS_CAP, C3(70, 65, 80), { C3(110, 100, 120), C3(130, 70, 160), C3(220, 220, 230) }, FX_SPORES, FD_MUSHROOM, -1 },
    { "starlight puffball", V3("blue", "green", "white"), FS_ORB, C3(40, 50, 70), { C3(90, 160, 255), C3(90, 255, 170), C3(240, 240, 255) }, FX_LIGHT, FD_GLOWCAP, -1 },
    /* the Sun */
    { "sun-grass", V3("gold", "amber", "white-gold"), FS_TUFT, C3(200, 150, 40), { C3(255, 210, 70), C3(240, 160, 40), C3(255, 240, 180) }, FX_FOOD, FD_GRAIN, -1 },
    { "glass flower", V3("clear", "rose", "smoky"), FS_BLOOM, C3(200, 180, 120), { C3(220, 240, 255), C3(255, 180, 200), C3(160, 150, 150) }, FX_COOL, -1, RG_AMBER },
    { "flare blossom", V3("yellow", "orange", "blue-white"), FS_ORB, C3(180, 110, 40), { C3(255, 230, 80), C3(255, 140, 40), C3(200, 230, 255) }, FX_MANA, -1, RG_PHOENIX },
    /* the Forge-World */
    { "cinder moss", V3("grey", "glowing", "black"), FS_TUFT, C3(60, 50, 45), { C3(110, 100, 90), C3(200, 90, 40), C3(40, 35, 35) }, FX_WARM, FD_MOSS, -1 },
    { "slag palm", V3("iron", "copper", "tin"), FS_SAPLING, C3(70, 60, 55), { C3(100, 100, 110), C3(180, 100, 50), C3(170, 175, 180) }, FX_FOOD, FD_TUBER, RG_RUST },
    { "forge-bloom", V3("red", "white", "violet"), FS_SPIKE, C3(80, 50, 40), { C3(255, 80, 30), C3(255, 240, 220), C3(190, 90, 255) }, FX_THORNS, -1, RG_BRIMSTONE },
    /* the Glass Desert */
    { "sand sedge", V3("tan", "pale", "rust"), FS_TUFT, C3(170, 140, 90), { C3(200, 170, 110), C3(230, 215, 170), C3(180, 110, 70) }, FX_FOOD, FD_GRAIN, -1 },
    { "glass cactus", V3("green", "clear", "blue"), FS_CACTUS, C3(70, 130, 90), { C3(90, 170, 110), C3(200, 230, 225), C3(100, 160, 210) }, FX_THORNS, FD_MELON, -1 },
    { "mirage lotus", V3("pink", "blue", "shimmering"), FS_BLOOM, C3(110, 140, 100), { C3(255, 160, 210), C3(140, 200, 255), C3(240, 240, 200) }, FX_COOL, -1, RG_ORCHID },
    /* the Keep under the Ocean */
    { "sea-grass", V3("green", "olive", "red"), FS_TUFT, C3(50, 110, 70), { C3(70, 150, 90), C3(110, 120, 60), C3(160, 60, 60) }, FX_FOOD, FD_SEAGRAPE, -1 },
    { "giant kelp", V3("brown", "golden", "green"), FS_KELP, C3(80, 70, 30), { C3(110, 90, 40), C3(190, 160, 60), C3(70, 130, 60) }, FX_FOOD, FD_SEAGRAPE, -1 },
    { "air pod", V3("blue", "white", "green"), FS_POD, C3(60, 90, 80), { C3(120, 200, 255), C3(230, 245, 255), C3(140, 255, 200) }, FX_AIR, -1, RG_DEW },
    /* the Icelands */
    { "snow moss", V3("white", "grey-green", "blue"), FS_TUFT, C3(140, 150, 140), { C3(230, 235, 240), C3(150, 170, 150), C3(170, 200, 240) }, FX_FOOD, FD_MOSS, -1 },
    { "frost-berry bush", V3("red", "blue", "white"), FS_SAPLING, C3(80, 70, 60), { C3(220, 50, 70), C3(80, 110, 230), C3(240, 240, 250) }, FX_FOOD, FD_FROSTBERRY, -1 },
    { "aurora bell", V3("green", "violet", "rose"), FS_ORB, C3(90, 110, 120), { C3(110, 255, 170), C3(180, 110, 255), C3(255, 140, 190) }, FX_WARM, -1, RG_DEW },
    /* the Sea of Night */
    { "tide-weed", V3("black", "green", "blue"), FS_TUFT, C3(30, 50, 50), { C3(30, 40, 45), C3(40, 100, 70), C3(40, 70, 120) }, FX_FOOD, FD_SEAGRAPE, -1 },
    { "moonreed", V3("silver", "grey", "blue"), FS_REED, C3(90, 100, 110), { C3(190, 200, 215), C3(130, 135, 140), C3(110, 140, 200) }, FX_FOOD, FD_TUBER, -1 },
    { "night-pearl lily", V3("pearl", "blue", "violet"), FS_BLOOM, C3(50, 70, 80), { C3(235, 235, 245), C3(110, 160, 255), C3(170, 120, 240) }, FX_MANA, -1, RG_DEW },
    /* the Storm Peaks */
    { "wind-grass", V3("grey", "green", "blue"), FS_TUFT, C3(100, 120, 110), { C3(150, 160, 150), C3(100, 150, 90), C3(130, 150, 190) }, FX_FOOD, FD_HERB, -1 },
    { "lightning tree", V3("white", "blue", "violet"), FS_SAPLING, C3(60, 60, 70), { C3(230, 240, 255), C3(120, 170, 255), C3(180, 120, 255) }, FX_SHOCK, -1, RG_QUICKSILVER },
    { "static thistle", V3("blue", "yellow", "white"), FS_SPIKE, C3(70, 80, 90), { C3(100, 160, 255), C3(255, 240, 100), C3(240, 245, 255) }, FX_THORNS, -1, RG_AMBER },
    /* the Shining Court */
    { "cloud clover", V3("white", "gold", "rose"), FS_TUFT, C3(200, 200, 190), { C3(250, 250, 245), C3(255, 220, 120), C3(255, 200, 210) }, FX_HEAL, FD_HERB, -1 },
    { "ledger lily", V3("ivory", "gilt", "black-inked"), FS_BLOOM, C3(170, 160, 120), { C3(250, 245, 225), C3(240, 200, 90), C3(60, 55, 60) }, FX_MANA, -1, RG_PLUME },
    { "halo orchid", V3("gold", "white", "sky-blue"), FS_ORB, C3(190, 170, 120), { C3(255, 220, 110), C3(255, 255, 250), C3(170, 220, 255) }, FX_HEAL, -1, RG_ORCHID },
    /* the Astral Shore */
    { "star moss", V3("blue", "violet", "silver"), FS_TUFT, C3(50, 50, 90), { C3(80, 110, 220), C3(140, 90, 220), C3(200, 205, 230) }, FX_FOOD, FD_MOSS, -1 },
    { "comet fern", V3("white", "cyan", "rose"), FS_FERN, C3(60, 60, 100), { C3(230, 235, 255), C3(110, 230, 255), C3(255, 160, 210) }, FX_MANA, -1, RG_VOIDSALT },
    { "nebula bloom", V3("violet", "teal", "golden"), FS_ORB, C3(60, 50, 90), { C3(190, 100, 255), C3(80, 230, 210), C3(255, 210, 110) }, FX_LIGHT, -1, RG_LENS },
    /* the Hollow Earth */
    { "sun-fern", V3("green", "yellow", "bronze"), FS_FERN, C3(60, 100, 40), { C3(80, 160, 60), C3(200, 200, 70), C3(170, 120, 60) }, FX_FOOD, FD_HERB, -1 },
    { "giant horsetail", V3("green", "striped", "red"), FS_REED, C3(70, 110, 50), { C3(90, 150, 70), C3(140, 170, 90), C3(170, 80, 60) }, FX_FOOD, FD_BAMBOO, -1 },
    { "first-flame flower", V3("orange", "gold", "red"), FS_BLOOM, C3(90, 90, 40), { C3(255, 150, 40), C3(255, 220, 80), C3(230, 60, 40) }, FX_WARM, FD_STARFRUIT, -1 },
    /* the Fungal Deep */
    { "mycelium mat", V3("white", "yellow", "pink"), FS_TUFT, C3(170, 165, 150), { C3(230, 230, 220), C3(230, 210, 120), C3(230, 170, 190) }, FX_ALERT, FD_MOSS, -1 },
    { "stalk-cap", V3("brown", "red", "blue"), FS_CAP, C3(180, 170, 150), { C3(140, 100, 60), C3(190, 50, 40), C3(70, 100, 190) }, FX_SPORES, FD_MUSHROOM, -1 },
    { "lantern morel", V3("green", "cyan", "amber"), FS_CAP, C3(120, 120, 100), { C3(120, 255, 150), C3(100, 230, 255), C3(255, 190, 80) }, FX_LIGHT, FD_GLOWCAP, -1 },
    /* the Bone Fields */
    { "marrow-grass", V3("yellow", "grey", "red-stemmed"), FS_TUFT, C3(160, 150, 100), { C3(200, 190, 110), C3(150, 150, 140), C3(170, 90, 70) }, FX_FOOD, FD_HERB, -1 },
    { "rib-thorn", V3("bone", "black", "red"), FS_SPIKE, C3(150, 140, 120), { C3(230, 220, 190), C3(60, 55, 50), C3(170, 50, 40) }, FX_THORNS, -1, RG_BONEDUST },
    { "corpse candle", V3("green", "blue", "pale"), FS_ORB, C3(100, 100, 90), { C3(150, 255, 150), C3(120, 170, 255), C3(240, 240, 220) }, FX_LIGHT, -1, RG_WAX },
    /* the Clockwork Orrery */
    { "cog clover", V3("brass", "copper", "steel"), FS_TUFT, C3(130, 110, 60), { C3(210, 180, 90), C3(200, 120, 70), C3(170, 175, 185) }, FX_FOOD, FD_HERB, -1 },
    { "pendulum vine", V3("brass", "green", "silver"), FS_REED, C3(120, 100, 50), { C3(220, 190, 90), C3(90, 150, 80), C3(200, 205, 215) }, FX_SEEDS, FD_TUBER, -1 },
    { "planet bud", V3("blue", "red", "ringed"), FS_ORB, C3(110, 100, 70), { C3(110, 160, 255), C3(240, 110, 80), C3(230, 200, 120) }, FX_MANA, -1, RG_QUICKSILVER },
    /* the Library of Ash */
    { "ash fern", V3("grey", "black", "ember-veined"), FS_FERN, C3(80, 75, 70), { C3(140, 135, 130), C3(50, 45, 45), C3(170, 90, 50) }, FX_FOOD, FD_HERB, -1 },
    { "page tree", V3("parchment", "vellum", "charred"), FS_LEAVES, C3(90, 70, 50), { C3(235, 220, 180), C3(250, 245, 230), C3(110, 90, 70) }, FX_PAGE, -1, RG_WAX },
    { "ember quill", V3("red", "orange", "blue"), FS_SPIKE, C3(70, 60, 55), { C3(240, 80, 40), C3(255, 160, 60), C3(110, 160, 255) }, FX_WARM, -1, RG_PLUME },
    /* the Garden of the First Seed */
    { "first-grass", V3("green", "gold", "silver"), FS_TUFT, C3(70, 140, 50), { C3(90, 190, 70), C3(220, 200, 80), C3(200, 215, 200) }, FX_SEEDS, FD_GRAIN, -1 },
    { "seed tree", V3("apple", "pear", "plum"), FS_SAPLING, C3(100, 70, 40), { C3(220, 50, 40), C3(210, 210, 80), C3(120, 50, 140) }, FX_FOOD, FD_MELON, -1 },
    { "dawn-bloom", V3("rose", "gold", "white"), FS_BLOOM, C3(70, 130, 60), { C3(255, 150, 170), C3(255, 210, 90), C3(255, 255, 250) }, FX_HEAL, FD_STARFRUIT, RG_MANDRAKE },
    /* the Fallen Star */
    { "star-glass grass", V3("clear", "blue", "violet"), FS_TUFT, C3(120, 130, 160), { C3(210, 225, 240), C3(110, 160, 255), C3(170, 120, 240) }, FX_FOOD, FD_HERB, -1 },
    { "shard reed", V3("blue", "white", "black"), FS_SPIKE, C3(90, 100, 130), { C3(110, 170, 255), C3(240, 245, 255), C3(40, 40, 60) }, FX_THORNS, -1, RG_LENS },
    { "mana-drinker", V3("violet", "cyan", "white"), FS_ORB, C3(80, 70, 110), { C3(180, 90, 255), C3(80, 240, 255), C3(250, 250, 255) }, FX_MANA, -1, RG_VOIDSALT },
    /* the Mirror Fortress */
    { "backward grass", V3("green", "grey", "silver"), FS_TUFT, C3(80, 110, 90), { C3(110, 150, 120), C3(140, 140, 140), C3(200, 210, 215) }, FX_FOOD, FD_HERB, -1 },
    { "mirror reed", V3("silver", "black", "gold"), FS_REED, C3(120, 125, 135), { C3(220, 225, 235), C3(40, 40, 45), C3(230, 200, 100) }, FX_FOOD, FD_TUBER, -1 },
    { "reflect-bloom", V3("white", "black", "rainbow"), FS_ORB, C3(100, 100, 110), { C3(250, 250, 255), C3(40, 40, 50), C3(255, 160, 220) }, FX_MANA, -1, RG_QUICKSILVER },
    /* the Dream of the Sleeper */
    { "sleep poppy", V3("red", "violet", "grey"), FS_BLOOM, C3(80, 110, 80), { C3(230, 50, 60), C3(160, 80, 220), C3(170, 170, 180) }, FX_HEAL, FD_HERB, RG_MANDRAKE },
    { "dream willow", V3("blue", "rose", "pale"), FS_LEAVES, C3(90, 80, 110), { C3(120, 150, 255), C3(255, 150, 200), C3(230, 230, 240) }, FX_MANA, -1, RG_ORCHID },
    { "wish-light", V3("gold", "blue", "green"), FS_ORB, C3(90, 90, 110), { C3(255, 220, 110), C3(120, 170, 255), C3(130, 255, 160) }, FX_LIGHT, FD_STARFRUIT, -1 },
};

const FaunaDef REALM_FAUNA[REALM_FAUNA_COUNT] = {
    /* the Pale Moon */
    { "dust hare", V3("grey", "silver", "moon-white"), BP_HOPPER, TM_SHY, 1.0f, 7.0f, 1, { C3(150, 150, 155), C3(200, 205, 215), C3(240, 240, 245) }, 0, FD_HARE, -1, NULL },
    { "crater moth", V3("pale", "dusky", "lunar"), BP_FLYER, TM_CALM, 0.8f, 2.0f, 1, { C3(220, 220, 200), C3(120, 110, 120), C3(200, 220, 255) }, 1, -1, RG_WISP, NULL },
    { "regolith crab", V3("grey", "black", "white"), BP_CRAWLER, TM_FIERCE, 1.3f, 2.5f, 4, { C3(140, 140, 145), C3(60, 60, 65), C3(225, 225, 230) }, 0, FD_CRAB, -1, NULL },
    /* the Red Moon */
    { "rust wyrm", V3("rust", "black-iron", "scarlet"), BP_SERPENT, TM_FIERCE, 2.2f, 3.0f, 6, { C3(170, 80, 40), C3(60, 50, 50), C3(200, 40, 30) }, 0, FD_SALAMANDER, RG_RUST, NULL },
    { "iron beetle", V3("iron", "copper", "red"), BP_CRAWLER, TM_CALM, 0.9f, 1.0f, 3, { C3(110, 110, 120), C3(180, 100, 60), C3(160, 50, 40) }, 0, FD_BEETLE_JELLY, RG_RUST, NULL },
    { "red kite", V3("red", "brown", "ember"), BP_BIRD, TM_SHY, 0.9f, 7.0f, 1, { C3(190, 60, 40), C3(120, 80, 50), C3(250, 120, 40) }, 0, FD_FOWL, RG_FEATHER, NULL },
    /* the Black Moon */
    { "glimmer stalker", V3("black", "violet", "eyeless"), BP_STRIDER, TM_FIERCE, 1.8f, 4.0f, 5, { C3(30, 30, 40), C3(70, 40, 100), C3(200, 200, 210) }, 1, -1, RG_LENS, NULL },
    { "lamp moth", V3("green", "blue", "gold"), BP_FLYER, TM_CALM, 0.9f, 1.8f, 1, { C3(100, 255, 160), C3(100, 170, 255), C3(255, 210, 100) }, 1, -1, RG_WISP, NULL },
    { "void hare", V3("black", "grey", "starred"), BP_HOPPER, TM_SHY, 1.0f, 7.0f, 1, { C3(30, 30, 35), C3(90, 90, 100), C3(50, 50, 80) }, 1, FD_HARE, -1, NULL },
    /* the Sun */
    { "phoenix", V3("gold", "crimson", "white-flame"), BP_BIRD, TM_FIERCE, 1.8f, 6.0f, 5, { C3(255, 190, 60), C3(230, 50, 30), C3(255, 245, 220) }, 1, FD_FOWL, RG_PHOENIX, NULL },
    { "corona lizard", V3("orange", "yellow", "white"), BP_SERPENT, TM_SHY, 0.9f, 5.0f, 1, { C3(240, 130, 40), C3(250, 220, 70), C3(250, 245, 230) }, 1, FD_SALAMANDER, -1, NULL },
    { "sunray", V3("gold", "amber", "rose"), BP_FLYER, TM_CALM, 1.6f, 2.0f, 3, { C3(255, 210, 90), C3(230, 150, 50), C3(255, 160, 170) }, 1, -1, RG_PLUME, NULL },
    /* the Forge-World */
    { "anvil tortoise", V3("iron", "bronze", "mithril"), BP_QUAD, TM_CALM, 1.8f, 0.8f, 8, { C3(90, 90, 95), C3(170, 120, 60), C3(200, 220, 235) }, 0, FD_GOAT, RG_HEARTSTONE, NULL },
    { "spark salamander", V3("red", "yellow", "blue"), BP_SERPENT, TM_SHY, 0.8f, 5.0f, 1, { C3(240, 60, 30), C3(255, 210, 50), C3(90, 150, 255) }, 1, FD_SALAMANDER, -1, NULL },
    { "slag hound", V3("black", "glowing", "grey"), BP_QUAD, TM_FIERCE, 1.5f, 5.5f, 5, { C3(40, 35, 35), C3(200, 80, 30), C3(110, 105, 100) }, 1, -1, RG_FANG, NULL },
    /* the Glass Desert */
    { "sand worm", V3("sand", "glass", "rust"), BP_SERPENT, TM_FIERCE, 3.0f, 3.5f, 8, { C3(200, 170, 110), C3(200, 225, 230), C3(170, 90, 60) }, 0, FD_SALAMANDER, RG_TOOTH, NULL },
    { "dune fox", V3("tan", "white", "red"), BP_QUAD, TM_SHY, 0.9f, 7.0f, 1, { C3(210, 170, 110), C3(235, 230, 220), C3(200, 100, 50) }, 0, FD_HARE, -1, NULL },
    { "glass scorpion", V3("clear", "amber", "black"), BP_CRAWLER, TM_FIERCE, 1.1f, 3.0f, 3, { C3(200, 225, 235), C3(220, 160, 60), C3(40, 40, 45) }, 0, -1, RG_VENOM, NULL },
    /* the Keep under the Ocean */
    { "silver shoal", V3("silver", "blue", "gold"), BP_FISH, TM_SHY, 0.8f, 3.5f, 1, { C3(200, 210, 220), C3(80, 130, 220), C3(230, 190, 80) }, 0, FD_CAVE_FISH, -1, NULL },
    { "drowned crab", V3("green", "black", "barnacled"), BP_CRAWLER, TM_FIERCE, 1.4f, 2.0f, 4, { C3(70, 110, 80), C3(40, 40, 45), C3(160, 150, 130) }, 0, FD_CRAB, -1, NULL },
    { "leviathan calf", V3("grey", "blue", "pale"), BP_WHALE, TM_CALM, 4.0f, 1.2f, 12, { C3(100, 110, 120), C3(60, 90, 140), C3(200, 205, 210) }, 0, FD_SKY_CARP, RG_HEARTSTONE, NULL },
    /* the Icelands */
    { "snow goat", V3("white", "grey", "shaggy"), BP_QUAD, TM_SHY, 1.3f, 5.0f, 3, { C3(240, 240, 240), C3(160, 160, 165), C3(220, 210, 190) }, 0, FD_GOAT, RG_HORN, NULL },
    { "ice wolf", V3("white", "blue-grey", "black"), BP_QUAD, TM_FIERCE, 1.4f, 6.5f, 4, { C3(225, 230, 240), C3(130, 150, 180), C3(45, 45, 55) }, 0, -1, RG_FANG, NULL },
    { "frost owl", V3("snowy", "barred", "blue"), BP_BIRD, TM_CALM, 1.0f, 5.0f, 2, { C3(245, 245, 250), C3(180, 175, 165), C3(150, 180, 230) }, 0, FD_GROUSE, RG_FEATHER, NULL },
    /* the Sea of Night */
    { "tide crab", V3("black", "blue", "silver"), BP_CRAWLER, TM_SHY, 0.9f, 2.5f, 2, { C3(40, 40, 50), C3(60, 80, 160), C3(190, 195, 210) }, 0, FD_CRAB, -1, NULL },
    { "night heron", V3("grey", "black", "white"), BP_BIRD, TM_SHY, 1.3f, 5.0f, 2, { C3(120, 125, 140), C3(40, 40, 50), C3(230, 230, 240) }, 0, FD_FOWL, RG_FEATHER, NULL },
    { "moon jelly", V3("blue", "violet", "white"), BP_JELLY, TM_CALM, 1.2f, 0.8f, 1, { C3(120, 170, 255), C3(170, 110, 240), C3(240, 240, 255) }, 1, -1, RG_SLIME, NULL },
    /* the Storm Peaks */
    { "storm ray", V3("grey", "blue", "charged"), BP_FLYER, TM_CALM, 2.0f, 2.5f, 4, { C3(110, 115, 130), C3(60, 90, 160), C3(200, 220, 255) }, 1, -1, RG_PLUME, NULL },
    { "crag ram", V3("grey", "brown", "white"), BP_QUAD, TM_SHY, 1.4f, 4.0f, 3, { C3(140, 140, 135), C3(120, 90, 60), C3(230, 230, 225) }, 0, FD_MUTTON, RG_HORN, NULL },
    { "thunder hawk", V3("slate", "gold", "black"), BP_BIRD, TM_FIERCE, 1.2f, 8.0f, 3, { C3(90, 95, 110), C3(220, 180, 70), C3(35, 35, 40) }, 0, FD_GROUSE, RG_FEATHER, NULL },
    /* the Shining Court */
    { "memory dove", V3("white", "gold", "grey"), BP_BIRD, TM_CALM, 0.8f, 4.0f, 1, { C3(250, 250, 250), C3(250, 220, 120), C3(170, 170, 175) }, 1, -1, RG_PLUME, "remembers" },
    { "cloud lamb", V3("white", "gold-fleeced", "rose"), BP_QUAD, TM_SHY, 1.0f, 3.5f, 2, { C3(250, 250, 248), C3(250, 225, 150), C3(250, 210, 215) }, 0, FD_MUTTON, -1, NULL },
    { "gilded carp", V3("gold", "white", "red"), BP_FLYER, TM_CALM, 1.0f, 1.5f, 2, { C3(255, 200, 80), C3(245, 245, 240), C3(230, 70, 50) }, 0, FD_SKY_CARP, -1, NULL },
    /* the Astral Shore */
    { "star whale", V3("indigo", "silver", "nebular"), BP_WHALE, TM_CALM, 6.0f, 1.0f, 20, { C3(50, 50, 110), C3(190, 195, 215), C3(160, 90, 200) }, 1, -1, RG_VOIDSALT, NULL },
    { "void minnow", V3("blue", "gold", "clear"), BP_FLYER, TM_SHY, 0.5f, 4.0f, 1, { C3(90, 140, 255), C3(255, 210, 90), C3(210, 230, 240) }, 1, FD_SKY_CARP, -1, NULL },
    { "astral strider", V3("violet", "white", "starred"), BP_STRIDER, TM_CALM, 2.0f, 2.5f, 4, { C3(140, 90, 220), C3(235, 235, 245), C3(40, 40, 80) }, 1, -1, RG_LENS, NULL },
    /* the Hollow Earth */
    { "cave ox", V3("brown", "white", "dun"), BP_QUAD, TM_CALM, 2.0f, 2.5f, 6, { C3(110, 80, 55), C3(230, 225, 215), C3(170, 145, 100) }, 0, FD_GOAT, -1, NULL },
    { "terror bird", V3("green", "blue", "brown"), BP_BIRD, TM_FIERCE, 2.0f, 7.5f, 5, { C3(70, 120, 60), C3(60, 90, 160), C3(120, 90, 60) }, 0, FD_FOWL, RG_PLUME, NULL },
    { "burrow lizard", V3("green", "sand", "banded"), BP_SERPENT, TM_SHY, 1.0f, 4.5f, 2, { C3(80, 140, 70), C3(200, 180, 120), C3(120, 100, 60) }, 0, FD_SALAMANDER, -1, NULL },
    /* the Fungal Deep */
    { "spore slug", V3("yellow", "violet", "spotted"), BP_CRAWLER, TM_CALM, 1.0f, 0.4f, 2, { C3(220, 200, 90), C3(150, 90, 200), C3(200, 160, 130) }, 1, FD_SNAIL, RG_SLIME, NULL },
    { "cap hopper", V3("brown", "red", "white"), BP_HOPPER, TM_SHY, 0.8f, 6.0f, 1, { C3(140, 100, 70), C3(190, 60, 50), C3(230, 225, 215) }, 0, FD_HARE, -1, NULL },
    { "puff bat", V3("grey", "green", "pink"), BP_FLYER, TM_FIERCE, 0.8f, 5.0f, 2, { C3(120, 115, 120), C3(110, 170, 110), C3(220, 150, 170) }, 0, -1, RG_BATWING, NULL },
    /* the Bone Fields */
    { "bone vulture", V3("black", "bald", "white-collared"), BP_BIRD, TM_CALM, 1.4f, 5.0f, 2, { C3(40, 35, 35), C3(150, 90, 80), C3(230, 225, 215) }, 0, FD_FOWL, RG_FEATHER, NULL },
    { "carrion beetle", V3("black", "orange", "green"), BP_CRAWLER, TM_CALM, 0.8f, 1.2f, 1, { C3(30, 30, 30), C3(220, 120, 40), C3(60, 140, 70) }, 0, FD_BEETLE_JELLY, -1, NULL },
    { "marrow jackal", V3("tan", "grey", "spotted"), BP_QUAD, TM_FIERCE, 1.2f, 6.5f, 3, { C3(190, 150, 100), C3(130, 125, 120), C3(170, 140, 90) }, 0, FD_HARE, RG_FANG, NULL },
    /* the Clockwork Orrery */
    { "clockwork finch", V3("brass", "silver", "copper"), BP_BIRD, TM_SHY, 0.6f, 6.0f, 1, { C3(220, 180, 80), C3(200, 205, 215), C3(200, 110, 60) }, 0, -1, RG_RUST, NULL },
    { "gear tortoise", V3("brass", "iron", "verdigris"), BP_QUAD, TM_CALM, 1.5f, 0.6f, 6, { C3(200, 160, 70), C3(100, 100, 110), C3(80, 160, 130) }, 0, -1, RG_HEARTSTONE, NULL },
    { "orbit moth", V3("blue", "gold", "red"), BP_FLYER, TM_CALM, 0.8f, 2.0f, 1, { C3(110, 160, 255), C3(255, 210, 90), C3(240, 100, 70) }, 1, -1, RG_WISP, NULL },
    /* the Library of Ash */
    { "book worm", V3("grey", "ink-black", "gilt"), BP_SERPENT, TM_CALM, 1.0f, 1.0f, 2, { C3(150, 145, 140), C3(30, 30, 35), C3(220, 180, 80) }, 0, -1, RG_WAX, "recites" },
    { "ink crow", V3("black", "blue-black", "white"), BP_BIRD, TM_SHY, 0.9f, 6.0f, 1, { C3(25, 25, 30), C3(30, 40, 70), C3(235, 235, 235) }, 0, FD_FOWL, RG_FEATHER, NULL },
    { "cinder cat", V3("grey", "black", "ember"), BP_QUAD, TM_SHY, 0.8f, 6.0f, 2, { C3(120, 115, 110), C3(35, 30, 30), C3(200, 90, 40) }, 1, -1, -1, NULL },
    /* the Garden of the First Seed */
    { "garden deer", V3("fawn", "white", "dappled"), BP_QUAD, TM_SHY, 1.5f, 6.5f, 3, { C3(180, 130, 80), C3(240, 235, 225), C3(150, 110, 70) }, 0, FD_GOAT, -1, NULL },
    { "honey bear", V3("brown", "gold", "black"), BP_QUAD, TM_CALM, 1.8f, 2.5f, 6, { C3(120, 80, 45), C3(210, 160, 70), C3(40, 35, 30) }, 0, FD_HONEY, -1, NULL },
    { "song thrush", V3("brown", "blue", "red-breasted"), BP_BIRD, TM_SHY, 0.6f, 6.0f, 1, { C3(140, 110, 70), C3(80, 120, 200), C3(200, 80, 50) }, 0, FD_QUAIL, -1, NULL },
    /* the Fallen Star */
    { "crystal tick", V3("blue", "violet", "clear"), BP_CRAWLER, TM_FIERCE, 0.8f, 3.5f, 2, { C3(90, 140, 255), C3(160, 90, 230), C3(210, 230, 240) }, 1, -1, RG_VENOM, NULL },
    { "starling", V3("black", "speckled", "white"), BP_BIRD, TM_SHY, 0.6f, 7.0f, 1, { C3(30, 30, 40), C3(80, 80, 100), C3(230, 230, 240) }, 0, FD_QUAIL, -1, NULL },
    { "glass stag", V3("clear", "blue", "violet"), BP_QUAD, TM_SHY, 1.7f, 7.0f, 4, { C3(210, 230, 240), C3(110, 160, 240), C3(170, 110, 230) }, 1, FD_GOAT, RG_HORN, NULL },
    /* the Mirror Fortress */
    { "looking-glass hare", V3("silver", "black", "white"), BP_HOPPER, TM_SHY, 1.0f, 7.0f, 1, { C3(200, 205, 215), C3(35, 35, 40), C3(245, 245, 245) }, 0, FD_HARE, -1, NULL },
    { "twin-headed hen", V3("white", "black", "speckled"), BP_BIRD, TM_SHY, 0.9f, 3.5f, 1, { C3(240, 235, 225), C3(40, 40, 40), C3(160, 150, 140) }, 0, FD_FOWL, -1, NULL },
    { "mirror cat", V3("silver", "black", "gold-eyed"), BP_QUAD, TM_CALM, 0.8f, 5.0f, 2, { C3(190, 195, 205), C3(30, 30, 35), C3(220, 190, 90) }, 0, -1, RG_LENS, "mirrors" },
    /* the Dream of the Sleeper */
    { "dream fish", V3("blue", "rose", "gold"), BP_FLYER, TM_CALM, 1.0f, 1.5f, 1, { C3(110, 150, 255), C3(255, 150, 200), C3(255, 210, 90) }, 1, FD_SKY_CARP, -1, NULL },
    { "patchwork beast", V3("striped", "spotted", "checked"), BP_QUAD, TM_CALM, 1.8f, 2.0f, 5, { C3(200, 140, 80), C3(150, 150, 200), C3(220, 220, 220) }, 0, FD_MUTTON, -1, NULL },
    { "nightmare hound", V3("black", "red-eyed", "smoke"), BP_QUAD, TM_FIERCE, 1.6f, 6.5f, 5, { C3(25, 20, 30), C3(80, 20, 25), C3(110, 105, 120) }, 1, -1, RG_FANG, NULL },
};

int realm_flavor(int r) { return FL_R_FIRST + r; }
int realm_of_flavor(int flavor) { return flavor >= FL_R_FIRST && flavor < FL_R_FIRST + REALM_COUNT ? flavor - FL_R_FIRST : -1; }

/* Six to eight of the twenty-one, by the world's seed. */
u32 realm_pick(u64 seed) {
    Rng r = rng_make(seed ^ 0x5EA1A5ULL);
    int n = rng_range(&r, 6, 8);
    int order[REALM_COUNT];
    for (int i = 0; i < REALM_COUNT; i++) order[i] = i;
    for (int i = REALM_COUNT - 1; i > 0; i--) {
        int j = rng_range(&r, 0, i);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }
    u32 bits = 0;
    for (int i = 0; i < n; i++) bits |= 1u << order[i];
    return bits;
}

/* ---------- the land ---------- */

#define ARRIVE_X REALM_ARRIVE_X
#define ARRIVE_Z REALM_ARRIVE_Z
#define KEEP_X REALM_KEEP_X
#define KEEP_Z REALM_KEEP_Z

static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

/* A plaza around the arrival and the keep: the ground is flat there. */
static float plaza(int wx, int wz, int px, int pz, float r) {
    float d = sqrtf((float)((wx - px) * (wx - px) + (wz - pz) * (wz - pz)));
    return 1.0f - smooth01((d - r) / 10.0f);
}

int realm_height(u64 seed, int r, int wx, int wz) {
    const RealmDef *d = &REALMS[r];
    int g0 = realm_ground_y(r);
    u64 s = seed ^ ((u64)r * 0x9E3779B97F4A7C15ULL);
    float x = (float)wx, z = (float)wz, h = 0;
    switch (d->terrain) {
    case TR_PLAIN: h = fbm2(s, x * 0.012f, z * 0.012f, 4) * d->amp; break;
    case TR_CRATER: {
        h = fbm2(s, x * 0.01f, z * 0.01f, 4) * d->amp;
        /* craters on a 48 m grid, one in two cells */
        int gx = (int)floorf(x / 48.0f), gz = (int)floorf(z / 48.0f);
        for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++) {
                int cx = gx + dx, cz = gz + dz;
                if (hash3f(s, cx, 0, cz, 0xC7A) < 0.5f) continue;
                float ox = (cx + 0.2f + 0.6f * hash3f(s, cx, 1, cz, 0xC7A)) * 48.0f;
                float oz = (cz + 0.2f + 0.6f * hash3f(s, cx, 2, cz, 0xC7A)) * 48.0f;
                float rad = 8.0f + 12.0f * hash3f(s, cx, 3, cz, 0xC7A);
                float dd = sqrtf((x - ox) * (x - ox) + (z - oz) * (z - oz)) / rad;
                if (dd < 1.0f) h -= (1.0f - dd * dd) * rad * 0.45f;
                else if (dd < 1.4f) h += (1.0f - fabsf(dd - 1.15f) / 0.25f) * rad * 0.12f; /* the rim */
            }
        break;
    }
    case TR_DUNES:
        h = sinf(x * 0.05f + fbm2(s, x * 0.008f, z * 0.008f, 2) * 4.0f) * d->amp + fbm2(s + 1, x * 0.02f, z * 0.02f, 3) * 3.0f;
        break;
    case TR_PEAKS: {
        float n = fbm2(s, x * 0.01f, z * 0.01f, 5);
        float ridge = 1.0f - fabsf(fbm2(s + 7, x * 0.008f, z * 0.008f, 3));
        h = n * d->amp * 0.5f + powf(ridge, 3.0f) * d->amp * 1.6f - d->amp * 0.4f;
        break;
    }
    case TR_SEA: case TR_DEEP:
        h = fbm2(s, x * 0.012f, z * 0.012f, 4) * d->amp * 1.4f - (d->terrain == TR_SEA ? 3.0f : 0.0f);
        break;
    case TR_CAVERN: h = fbm2(s, x * 0.015f, z * 0.015f, 4) * d->amp; break;
    default: h = 0; break;
    }
    /* flat ground to arrive on and to build the keep on */
    float pa = plaza(wx, wz, ARRIVE_X, ARRIVE_Z, 9.0f), pk = plaza(wx, wz, KEEP_X, KEEP_Z, 22.0f);
    float flat = d->terrain == TR_SEA ? 2.0f : 0.0f; /* (above the sea) */
    h = lerpf(h, flat, fmaxf(pa, pk));
    /* the land ends in walls of rock all round */
    float edge = fminf(fminf((float)wx, (float)wz), fminf((float)(REALM_W - 1 - wx), (float)(REALM_W - 1 - wz)));
    if (edge < 12.0f) h += (12.0f - edge) * (12.0f - edge) * 0.6f;
    return g0 + (int)floorf(fminf(h, 130.0f));
}

void realm_arrival(int r, int *x, int *y, int *z) {
    *x = ARRIVE_X; *z = ARRIVE_Z;
    const RealmDef *d = &REALMS[r];
    if (d->terrain == TR_ISLES) *y = realm_ground_y(r);
    else if (d->terrain == TR_MIRROR || d->terrain == TR_DREAM) *y = (REALM_CY(r) + 1) * 32 + 6; /* (the layer's floor) */
    else *y = realm_height(0, r, ARRIVE_X, ARRIVE_Z);
}

void realm_keep(int r, int *x, int *y, int *z) {
    *x = KEEP_X; *z = KEEP_Z;
    const RealmDef *d = &REALMS[r];
    if (d->terrain == TR_ISLES) *y = realm_ground_y(r);
    else if (d->terrain == TR_MIRROR || d->terrain == TR_DREAM) *y = (REALM_CY(r) + 1) * 32 + 6;
    else *y = realm_height(0, r, KEEP_X, KEEP_Z);
}
