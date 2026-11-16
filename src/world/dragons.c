#include "world/dragons.h"

#include "world/layout.h"
#include "world/realms.h"

/* The dragons of the fortress (docs/DRAGONS.md). */

#define C3(r, g, b) { r, g, b }
#define B(s) (1u << (s))

const DragonDef DRAGON_KINDS[DK_COUNT] = {
    [DK_RED] = { "red", "the Red", C3(170, 30, 25), C3(220, 150, 60), C3(120, 20, 20), C3(255, 140, 40), BR_FIRE, BR_FIRE, 0,
        B(ST_INFERNO) | B(ST_SUNCOURT) | B(ST_CASTLE), "Proud and greedy; it sets its lair ablaze and fights from the flames." },
    [DK_MAGMA] = { "magma", "the Molten", C3(60, 40, 35), C3(255, 120, 30), C3(80, 30, 20), C3(255, 160, 40), BR_MAGMA, BR_MAGMA, DT_ARMOR,
        B(ST_CORE) | B(ST_MAW) | B(ST_IRONHELL), "Slow and armoured in cooled stone; its breath is molten rock." },
    [DK_WHITE] = { "white", "the White", C3(220, 230, 240), C3(170, 200, 230), C3(190, 210, 230), C3(140, 220, 255), BR_FROST, BR_FROST, 0,
        B(ST_SPIRES) | B(ST_AERIE) | B(ST_MINES), "A patient hunter; it freezes the floor slick and strikes from the snow." },
    [DK_BLUE] = { "blue", "the Thunderer", C3(40, 80, 170), C3(150, 180, 230), C3(30, 60, 130), C3(160, 200, 255), BR_STORM, BR_STORM, 0,
        B(ST_STORM) | B(ST_SPIRES), "Vain and loud; its lightning finds everything in water or iron." },
    [DK_GREEN] = { "green", "the Venomous", C3(50, 120, 50), C3(170, 200, 90), C3(30, 90, 40), C3(160, 255, 90), BR_GAS, BR_GAS, 0,
        B(ST_ROOTS) | B(ST_GARDENS), "Cunning; it fills its lair with poison and waits in it." },
    [DK_BLACK] = { "black", "the Corroder", C3(30, 30, 35), C3(90, 110, 60), C3(20, 20, 25), C3(170, 255, 60), BR_ACID, BR_ACID, 0,
        B(ST_OSSUARY) | B(ST_LIMBO) | B(ST_DUNGEON), "Cruel; its acid melts the pillars you hide behind." },
    [DK_SWAMP] = { "swamp", "the Rotting", C3(80, 90, 50), C3(120, 110, 70), C3(60, 70, 40), C3(200, 220, 90), BR_ROT, BR_ROT, 0,
        B(ST_ROOTS) | B(ST_FUNGAL), "It sinks into the muck and rises behind you." },
    [DK_GOLD] = { "gold", "the Golden", C3(220, 170, 50), C3(255, 230, 150), C3(200, 150, 40), C3(255, 240, 160), BR_HOLY, BR_HOLY, DT_LIGHTHEAL,
        B(ST_HEAVEN) | B(ST_SUNCOURT), "Ancient and terrible in its light; it mends itself when left a moment." },
    [DK_CRYSTAL] = { "crystal", "the Prism", C3(150, 200, 230), C3(220, 240, 255), C3(120, 180, 220), C3(200, 240, 255), BR_LIGHT, BR_LIGHT, 0,
        B(ST_CRYSTAL), "Its beams of light glance off the crystals of its lair." },
    [DK_AMETHYST] = { "amethyst", "the Violet", C3(130, 70, 170), C3(200, 160, 230), C3(100, 50, 140), C3(220, 150, 255), BR_FORCE, BR_FORCE, 0,
        B(ST_CRYSTAL) | B(ST_ORRERY), "Its breath is force: it throws you off ledges and pulls you in." },
    [DK_SHADOW] = { "shadow", "the Unseen", C3(25, 20, 35), C3(60, 50, 80), C3(15, 10, 25), C3(150, 90, 255), BR_SHADOW, BR_SHADOW, DT_STEALTH | DT_DARK,
        B(ST_LIMBO) | B(ST_OSSUARY), "It puts out the lights and strikes from the dark." },
    [DK_BONE] = { "bone", "the Deathless", C3(210, 200, 175), C3(120, 110, 100), C3(90, 80, 70), C3(120, 255, 160), BR_NECRO, BR_NECRO, DT_REVIVE,
        B(ST_OSSUARY) | B(ST_LIMBO), "A dracolich: it rises once more unless its bones are burned, and raises the dead." },
    [DK_IRON] = { "iron", "the Engine", C3(100, 100, 110), C3(200, 150, 70), C3(80, 80, 90), C3(255, 180, 80), BR_STEAM, BR_STEAM, DT_ARMOR,
        B(ST_ORRERY) | B(ST_IRONHELL), "Built, not born: armour plates to break before its heart." },
    [DK_RUST] = { "rust", "the Devourer of Iron", C3(150, 70, 40), C3(190, 120, 70), C3(120, 50, 30), C3(255, 150, 80), BR_RUST, BR_RUST, 0,
        B(ST_IRONHELL) | B(ST_MINES), "It eats metal: its breath rusts your weapons for a while." },
    [DK_SAND] = { "sand", "the Scourer", C3(200, 170, 110), C3(230, 210, 160), C3(170, 140, 90), C3(255, 220, 140), BR_SAND, BR_SAND, 0,
        B(ST_MAW), "It swims in the sand and blinds with its scouring breath." },
    [DK_SEA] = { "sea", "the Drowner", C3(40, 110, 120), C3(150, 210, 200), C3(30, 80, 100), C3(120, 230, 255), BR_WAVE, BR_WAVE, DT_SWIM,
        B(ST_DUNGEON), "It fights from the water and drags you under." },
    [DK_CLOUD] = { "cloud", "the Grey Wind", C3(200, 205, 215), C3(240, 240, 245), C3(170, 175, 190), C3(220, 230, 255), BR_WIND, BR_WIND, DT_STEALTH,
        B(ST_CLOUDS) | B(ST_AERIE), "Invisible in its fog, it blows you off the edge." },
    [DK_VOID] = { "void", "the Hollow Star", C3(30, 20, 60), C3(90, 60, 160), C3(20, 10, 50), C3(180, 120, 255), BR_VOID, BR_VOID, DT_STEALTH,
        B(ST_ASTRAL), "It warps space: it blinks away and comes from any side." },
    [DK_PALE_MOON] = { "pale moon", "of the Pale Moon", C3(210, 215, 230), C3(240, 240, 250), C3(170, 180, 210), C3(220, 230, 255), BR_LIGHT, BR_FROST, 0,
        0, "The Silver King's war-beast; moonlight and cold." },
    [DK_RED_MOON] = { "red moon", "of the Red Moon", C3(150, 50, 30), C3(200, 110, 70), C3(110, 30, 20), C3(255, 120, 60), BR_RUST, BR_FIRE, 0,
        0, "The Ember King's war-beast; rust and flame." },
    [DK_BLACK_MOON] = { "black moon", "of the Black Moon", C3(20, 15, 30), C3(60, 40, 90), C3(10, 10, 20), C3(140, 100, 255), BR_VOID, BR_SHADOW, DT_DARK,
        0, "The Hollow King's war-beast; it brings the dark with it." },
    [DK_HYDRA] = { "hydra", "the Many-Headed", C3(60, 100, 70), C3(150, 170, 110), C3(40, 80, 50), C3(180, 255, 120), BR_GAS, BR_ACID, DT_HEADS | DT_WINGLESS,
        B(ST_ROOTS) | B(ST_FUNGAL) | B(ST_MAW), "Many heads, each with its breath; it grows more as it is hurt." },
    [DK_TWIN] = { "twin-headed", "the Two-Minded", C3(110, 60, 90), C3(170, 130, 140), C3(80, 40, 70), C3(255, 160, 200), BR_FIRE, BR_FROST, DT_TWIN,
        B(ST_CASTLE) | B(ST_DUNGEON) | B(ST_HEAVEN), "Two heads, two elements, two wills: fire and frost at once." },
    [DK_WYRM] = { "wyrm", "the Coiled", C3(120, 90, 60), C3(190, 160, 110), C3(90, 70, 50), C3(255, 190, 90), BR_SAND, BR_SAND, DT_WINGLESS,
        B(ST_MAW) | B(ST_CORE) | B(ST_MINES), "No wings: it burrows and bursts up beneath you." },
    [DK_CHAOS] = { "chaos", "the Unmaking", C3(90, 30, 110), C3(200, 80, 160), C3(60, 20, 80), C3(255, 80, 220), BR_CHAOS, BR_CHAOS, DT_STEALTH,
        B(ST_CORE) | B(ST_ASTRAL), "The hardest of all: its breath is a different element each time." },
};

const DragonSize DRAGON_SIZES[DSIZE_COUNT] = {
    /*                 scale  hp     dmg   speed flies  group   range cone  every */
    [DS_DRAGONET] = { "dragonet", 0.35f, 0.25f, 0.4f, 6.5f, true, 6, 10, 5.0f, 0.35f, 4.0f },
    [DS_DRAKE] = { "drake", 1.0f, 1.0f, 1.0f, 5.5f, false, 3, 5, 7.0f, 0.4f, 6.0f },
    [DS_WYVERN] = { "wyvern", 1.7f, 2.5f, 1.4f, 7.0f, true, 1, 2, 9.0f, 0.4f, 7.0f },
    [DS_DRAGON] = { "dragon", 4.5f, 30.0f, 2.6f, 9.0f, true, 1, 1, 22.0f, 0.45f, 7.0f },
    [DS_ANCIENT] = { "ancient wyrm", 8.0f, 75.0f, 3.6f, 10.0f, true, 1, 1, 34.0f, 0.5f, 6.0f },
};

const char *BREATH_NAMES[BR_COUNT] = { "fire", "molten rock", "frost", "lightning", "poison", "acid", "rot", "holy flame", "light",
                                       "force", "shadow", "death", "steam", "rust", "sand", "a wave", "wind", "the void", "chaos" };

int dragon_kind_for(int stratum, u64 seed) {
    int cand[DK_COUNT], n = 0;
    for (int k = 0; k < DK_COUNT; k++)
        if (DRAGON_KINDS[k].strata & B(stratum)) cand[n++] = k;
    if (!n) return -1;
    return cand[mix64(seed ^ 0xD2A6) % (u64)n];
}

int dragon_kind_of_realm(int r) {
    static const i8 K[REALM_COUNT] = {
        [RM_PALE_MOON] = DK_PALE_MOON, [RM_RED_MOON] = DK_RED_MOON, [RM_BLACK_MOON] = DK_BLACK_MOON, [RM_SUN] = DK_RED,
        [RM_FORGE] = DK_MAGMA, [RM_GLASS_DESERT] = DK_SAND, [RM_OCEAN_KEEP] = DK_SEA, [RM_ICELANDS] = DK_WHITE,
        [RM_SEA_OF_NIGHT] = DK_SEA, [RM_STORM_PEAKS] = DK_BLUE, [RM_SHINING_COURT] = DK_GOLD, [RM_ASTRAL_SHORE] = DK_VOID,
        [RM_HOLLOW_EARTH] = DK_GREEN, [RM_FUNGAL_DEEP] = DK_SWAMP, [RM_BONE_FIELDS] = DK_BONE, [RM_ORRERY] = DK_IRON,
        [RM_LIBRARY] = DK_SHADOW, [RM_GARDEN] = -1, [RM_FALLEN_STAR] = DK_CRYSTAL, [RM_MIRROR] = DK_TWIN, [RM_DREAM] = DK_CHAOS,
    };
    return r >= 0 && r < REALM_COUNT ? K[r] : -1;
}

void dragon_name(int kind, u64 key, char *out, int n) {
    static const char *A[12] = { "Vyr", "Kal", "Thar", "Mor", "Sael", "Ix", "Draug", "Naz", "Ur", "Vel", "Zor", "Ash" };
    static const char *M[10] = { "ma", "go", "ra", "the", "vo", "dra", "ske", "la", "", "ze" };
    static const char *E[12] = { "thax", "rion", "goth", "zar", "mir", "drax", "lith", "vorn", "kesh", "nyr", "gaul", "thrax" };
    u64 h = mix64(key ^ 0xD4A90);
    snprintf(out, (size_t)n, "%s%s%s %s", A[h % 12], M[(h >> 8) % 10], E[(h >> 16) % 12], DRAGON_KINDS[kind].title);
}

bool lair_from_structure(const struct Structure *s, Lair *L) {
    if (s->type != STRUCT_HALL || s->variant != HALL_LAIR) return false;
    memset(L, 0, sizeof *L);
    L->key = s->seed;
    L->kind = s->people < DK_COUNT ? s->people : DK_RED;
    L->size = (mix64(s->seed ^ 0xA9C1) % 100) < 22 ? DS_ANCIENT : DS_DRAGON;
    dragon_name(L->kind, s->seed, L->name, sizeof L->name);
    int X0 = s->cx0 * 32, Z0 = s->cz0 * 32;
    L->x0 = X0 + 3; L->z0 = Z0 + 3; L->x1 = X0 + s->sx * 32 - 4; L->z1 = Z0 + s->sz * 32 - 4;
    L->floor = s->floor_y;
    L->cx = X0 + s->sx * 16; L->cz = Z0 + s->sz * 16;
    for (int k = 0; k < 4; k++) {
        u64 h = mix64(s->seed ^ (u64)(k + 1) * 0x7E1);
        float a = k * 1.5708f + 0.5f + (float)(h % 100) / 200.0f;
        float r = 26.0f + (float)((h >> 8) % 8);
        L->perch_x[k] = L->cx + (int)(cosf(a) * r);
        L->perch_z[k] = L->cz + (int)(sinf(a) * r);
        L->perch_h[k] = 8 + (int)((h >> 16) % 6);
    }
    return true;
}
