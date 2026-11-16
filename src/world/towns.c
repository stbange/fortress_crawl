#include "world/towns.h"

#include "game/alchemy.h"
#include "game/food.h"
#include "game/forge.h"
#include "world/layout.h"

/* The friendly peoples of the fortress (docs/TOWNS.md). */

#define C3(r, g, b) { r, g, b }
#define S8(a, b, c, d, e, f, g_, h) { a, b, c, d, e, f, g_, h }
#define RG(x) (1000 + (x))

const PeopleDef PEOPLES[PEOPLE_COUNT] = {
    [PE_DWARF] = { "dwarf", "dwarves", "Dwarven", "Hall",
        S8("Khaz", "Dur", "Bar", "Grim", "Thor", "Kar", "Bal", "Dun"), S8("dum", "heim", "rak", "gar", "mor", "zad", "hold", "deep"),
        S8("Thrain", "Dagna", "Borin", "Hilde", "Gimra", "Torvin", "Brokk", "Sigrun"), S8("Ironhand", "Stonebeard", "Deepdelver", "Anvilborn", "Coalbrow", "Goldvein", "Hammerfall", "Rockjaw"),
        0.78f, 1.25f, { C3(220, 170, 140), C3(190, 140, 110), C3(150, 100, 80) }, { C3(170, 60, 30), C3(60, 50, 45), C3(200, 200, 195) },
        { C3(110, 70, 40), C3(70, 90, 110), C3(120, 40, 40) }, 0, 7, 3, false, 0,
        { { WA_FORGE, FG_IRON, 3, 70 }, { WA_FORGE, FG_STEEL, 1, 190 }, { WA_WEAPON, -1, 1, 650 }, { WA_SUPPLY, 100, 3, 110 }, { WA_FOOD, FD_WINE, 2, 40 } },
        { FD_MUSHROOM, FD_SALT, RG(RG_HEARTSTONE) },
        { "The stone remembers who cut it. Ask it about %B some day.", "My grandfather carved the stair you came up. Mind the third step.",
          "Iron sings if you strike it right. Most never hear it.", "%F was built well. Badly kept, but built well.",
          "Ale, a fire and good stone overhead: what more is there?", "They say %S woke because someone dug too greedy and too deep. Not a dwarf. Never a dwarf." },
        "Welcome to %T, stranger. Wipe your boots: the floor was polished by our fathers' fathers." },
    [PE_GOBLIN] = { "goblin", "goblins", "Goblin", "Warren",
        S8("Snag", "Grub", "Nix", "Wort", "Skab", "Fizz", "Gob", "Rott"), S8("warren", "hole", "nook", "bottom", "muck", "gutter", "pit", "stink"),
        S8("Snikkit", "Grimble", "Nogg", "Zazza", "Pikk", "Rutt", "Bizzle", "Mog"), S8("Toecutter", "Mushmouth", "Quickfingers", "Rustpot", "Grinbiter", "Mudwhistle", "Lampsnatcher", "Rattail"),
        0.7f, 0.85f, { C3(110, 150, 70), C3(140, 160, 80), C3(90, 120, 90) }, { C3(40, 35, 30), C3(90, 60, 40), C3(20, 20, 20) },
        { C3(110, 90, 60), C3(80, 70, 50), C3(140, 60, 40) }, 2, 0, 5, false, 1,
        { { WA_REAGENT, RG_SLIME, 2, 45 }, { WA_REAGENT, RG_BATWING, 2, 40 }, { WA_FOOD, FD_MUSHROOM, 4, 20 }, { WA_FOOD, FD_GLOWCAP, 2, 35 }, { WA_SUPPLY, 104, 3, 35 } },
        { RG(RG_SLIME), FD_SNAIL, RG(RG_BATWING) },
        { "Shiny? No? Pity. Everyone's got something shiny.", "We don't steal. We find things early.",
          "The big ones up top call us vermin. Vermin with better tunnels.", "Mushroom stew again. Mushroom stew forever.",
          "%B never looked down. That's how we got in, long ago.", "On %N we hid. Goblins are good at hiding. Live longer." },
        "Oi! A tall one. Welcome to %T. Don't touch the stew, it bites." },
    [PE_GNOME] = { "gnome", "gnomes", "Gnomish", "Clockworks",
        S8("Tink", "Gim", "Bimble", "Fizzle", "Cog", "Wim", "Nim", "Pib"), S8("dell", "wick", "spring", "bolt", "gear", "whistle", "sprocket", "tock"),
        S8("Fennel", "Pip", "Wizzle", "Tilly", "Bodo", "Nim", "Gearie", "Sprocket"), S8("Cogsworth", "Brassbutton", "Tinkertop", "Gearwhistle", "Springheel", "Coppercap", "Fizzlebang", "Ticktock"),
        0.6f, 0.9f, { C3(235, 195, 170), C3(210, 170, 140), C3(190, 150, 120) }, { C3(240, 240, 235), C3(200, 120, 60), C3(120, 90, 160) },
        { C3(60, 110, 160), C3(160, 50, 60), C3(80, 140, 70) }, 3, 4, 1, false, 2,
        { { WA_SUPPLY, 105, 3, 55 }, { WA_SUPPLY, 106, 2, 80 }, { WA_SUPPLY, 100, 3, 100 }, { WA_REAGENT, RG_QUICKSILVER, 1, 90 }, { WA_REAGENT, RG_LENS, 1, 70 } },
        { RG(RG_LENS), RG(RG_RUST), RG(RG_QUICKSILVER) },
        { "Don't touch that. Or do: it's only explosive on Tuesdays.", "Every lock in %F has a gnome's fingerprint inside. Somewhere.",
          "The orrery is fast by four minutes. It's been fast for three hundred years.", "I invented a spoon that stirs itself. It stirs everything now.",
          "Gears don't lie. People do. %B lied a great deal.", "The clocks all stopped on %N. Every one. At the same tick." },
        "Ah! A visitor to %T! Mind the springs, the cogs and the cat. Especially the cat." },
    [PE_ELF] = { "elf", "elves", "Elven", "Grove",
        S8("Ael", "Ith", "Sil", "Lor", "Ela", "Thal", "Cel", "Nim"), S8("indor", "ariel", "anar", "ion", "esse", "alin", "wen", "ore"),
        S8("Aerith", "Thalion", "Sylwen", "Elrandir", "Ilmare", "Caelith", "Nimriel", "Lorien"), S8("Starbough", "Silverleaf", "Moonwhisper", "Dawnsong", "Willowmere", "Brightwater", "Nightbloom", "Featherfall"),
        1.12f, 0.85f, { C3(240, 220, 200), C3(220, 200, 180), C3(170, 140, 120) }, { C3(240, 225, 170), C3(220, 220, 230), C3(60, 50, 70) },
        { C3(70, 130, 80), C3(200, 200, 210), C3(90, 80, 150) }, 1, 0, 4, false, 3,
        { { WA_REAGENT, RG_ORCHID, 2, 70 }, { WA_REAGENT, RG_DEW, 2, 55 }, { WA_FOOD, FD_HERB, 5, 18 }, { WA_POTION, 1, 1, 22 }, { WA_FOOD, FD_STARFRUIT, 1, 60 } },
        { FD_HERB, RG(RG_DEW), RG(RG_ORCHID) },
        { "We were old when %F was young. We will be old when it is dust.", "Listen: the roots are talking. They are always talking.",
          "Patience is a weapon. So is a good bow.", "%B cut down a forest for their beams. We remember every tree.",
          "The light down here is borrowed. We return it each evening.", "What was sealed was not evil. Only hungry. Many things are hungry." },
        "Be welcome under the boughs of %T, wanderer. Walk softly; the moss is listening." },
    [PE_HUMAN] = { "human", "humans", "Human", "Town",
        S8("Ash", "Bright", "Stone", "Mill", "Hollow", "Wolf", "Kings", "Black"), S8("ford", "bury", "wick", "stead", "haven", "gate", "field", "mere"),
        S8("Aldric", "Maren", "Tobias", "Elsa", "Gareth", "Wynn", "Hugh", "Rosa"), S8("Fletcher", "Miller", "Ashdown", "Cooper", "Thatcher", "Wainwright", "Holt", "Brennan"),
        1.0f, 1.0f, { C3(235, 200, 170), C3(200, 150, 110), C3(130, 90, 60) }, { C3(90, 60, 30), C3(30, 25, 20), C3(200, 160, 80) },
        { C3(140, 40, 40), C3(60, 80, 130), C3(100, 90, 60) }, 0, 3, 6, false, 4,
        { { WA_POTION, 0, 1, 30 }, { WA_FOOD, FD_GRAIN, 3, 15 }, { WA_FOOD, FD_SALT, 2, 15 }, { WA_FOOD, FD_SPICES, 2, 35 }, { WA_SUPPLY, 107, 3, 15 } },
        { FD_GRAIN, FD_FOWL, FD_HARE },
        { "We came down for work and stayed for the quiet. Mostly quiet.", "My cousin went to %F's lower halls. Wrote once. Then nothing.",
          "Bread, beer and a roof. The rest is weather.", "They say %B were honest folk. They say a lot of things.",
          "The ones who fled on %N built this town. We're their grandchildren.", "Mind the guards. They're bored, and bored guards are keen." },
        "Welcome to %T, traveller. Inn's by the well, temple's the one with the bell, and the elder is me." },
    [PE_HALFLING] = { "halfling", "halflings", "Halfling", "Burrows",
        S8("Bramble", "Hay", "Thistle", "Apple", "Clover", "Puddle", "Butter", "Honey"), S8("bank", "bottom", "burrow", "hill", "dale", "mead", "brook", "hollow"),
        S8("Pip", "Rosie", "Merry", "Daisy", "Bilbo", "Poppy", "Tom", "Lily"), S8("Underhill", "Goodbarrel", "Proudfoot", "Brandybuck", "Greenhand", "Tealeaf", "Bunce", "Applebloom"),
        0.62f, 1.1f, { C3(240, 205, 175), C3(220, 180, 150), C3(180, 130, 100) }, { C3(150, 90, 40), C3(200, 150, 70), C3(80, 50, 30) },
        { C3(170, 140, 60), C3(80, 120, 60), C3(160, 90, 60) }, 0, 0, 6, false, 5,
        { { WA_FOOD, FD_HONEY, 2, 30 }, { WA_FOOD, FD_MELON, 2, 25 }, { WA_FOOD, FD_EGG, 4, 12 }, { WA_FOOD, FD_MILK, 2, 15 }, { WA_SUPPLY, 108, 5, 25 } },
        { FD_HONEY, FD_EGG, FD_MILK },
        { "Second breakfast is the important one. First breakfast is just practice.", "We grow things. Things grow us. It's a fair trade.",
          "A good pie can mend most quarrels. Two pies the rest.", "Nobody remembers the names of %B, but my gran knew their recipes.",
          "Adventures make you late for supper. Do try to be back by supper.", "On %N the whole burrow hid in the root cellar. Ate all the pickles." },
        "Oh! A guest in %T! Come in, come in, mind your head, there's tea on." },
    [PE_ORC] = { "orc", "orcs", "Orcish", "Clanhold",
        S8("Grush", "Ug", "Mor", "Kra", "Thrak", "Gor", "Azg", "Durb"), S8("nak", "gul", "rak", "goth", "bash", "ur", "mog", "dush"),
        S8("Grukk", "Shara", "Morg", "Ulna", "Thrag", "Bazza", "Dorn", "Krisha"), S8("Skullsplitter", "Ironhide", "Bonechewer", "Ashfang", "Redtusk", "Stormfist", "Grimtooth", "Blackblood"),
        1.15f, 1.3f, { C3(100, 140, 80), C3(120, 120, 90), C3(90, 110, 100) }, { C3(20, 20, 20), C3(60, 40, 30), C3(120, 120, 110) },
        { C3(90, 60, 40), C3(70, 60, 60), C3(130, 40, 30) }, 0, 2, 0, true, 6,
        { { WA_WEAPON, -1, 1, 600 }, { WA_REAGENT, RG_TROLLBLOOD, 1, 110 }, { WA_REAGENT, RG_FANG, 2, 50 }, { WA_FOOD, FD_GOAT, 3, 25 }, { WA_SUPPLY, 103, 1, 55 } },
        { RG(RG_FANG), RG(RG_TROLLBLOOD), FD_GOAT },
        { "Strong arm, strong clan. Weak arm, still clan. Clan is clan.", "The monsters hate us more than they hate you. We take that as a compliment.",
          "We guarded %F's gates once. Nobody paid us. Nobody thanked us.", "Fight well, eat well, sleep well. Then fight again.",
          "%B called us beasts and paid us in copper. We kept the copper.", "On %N we held the lower stair. Nobody sings that song but us." },
        "You walk into %T on your own feet. Good. Respect the fire and the elders, and the clan respects you." },
};

void people_name(int people, u64 key, char *out, int n) {
    const PeopleDef *p = &PEOPLES[people];
    u64 h = mix64(key);
    snprintf(out, (size_t)n, "%s %s", p->first[h % 8], p->last[(h >> 8) % 8]);
}

/* ---------- the plan of a town ---------- */

int town_people(int st, u64 seed) {
    float wt[PEOPLE_COUNT] = { 1, 1, 1, 1, 1, 1, 1 };
    switch (st) {
    case ST_MINES: case ST_DUNGEON: wt[PE_DWARF] += 4; wt[PE_HUMAN] += 4; wt[PE_HALFLING] += 2; break;
    case ST_ROOTS: case ST_FUNGAL: wt[PE_GOBLIN] += 5; wt[PE_ELF] += 2; break;
    case ST_OSSUARY: case ST_LIMBO: wt[PE_GOBLIN] += 3; wt[PE_ORC] += 3; break;
    case ST_CRYSTAL: wt[PE_GNOME] += 5; wt[PE_DWARF] += 3; break;
    case ST_INFERNO: case ST_IRONHELL: case ST_MAW: case ST_CORE: wt[PE_ORC] += 5; wt[PE_DWARF] += 2; break;
    case ST_CASTLE: wt[PE_HUMAN] += 4; wt[PE_HALFLING] += 2; break;
    case ST_SPIRES: case ST_ORRERY: wt[PE_GNOME] += 4; wt[PE_HUMAN] += 2; break;
    case ST_GARDENS: case ST_HEAVEN: case ST_AERIE: case ST_CLOUDS: wt[PE_ELF] += 5; wt[PE_HALFLING] += 3; break;
    default: wt[PE_ORC] += 2; wt[PE_ELF] += 2; break;
    }
    float sum = 0;
    for (int i = 0; i < PEOPLE_COUNT; i++) sum += wt[i];
    float r = hash_unit(mix64(seed ^ 0x9E09)) * sum;
    for (int i = 0; i < PEOPLE_COUNT; i++) {
        if (r < wt[i]) return i;
        r -= wt[i];
    }
    return PEOPLE_COUNT - 1;
}

bool town_from_structure(const struct Structure *s, Town *t) {
    if (s->type != STRUCT_HALL || s->variant != HALL_TOWN) return false;
    memset(t, 0, sizeof *t);
    t->key = s->seed;
    t->people = s->people < PEOPLE_COUNT ? s->people : PE_HUMAN;
    const PeopleDef *p = &PEOPLES[t->people];
    u64 h = mix64(s->seed ^ 0x7A3E);
    snprintf(t->name, sizeof t->name, "the %s %s of %s%s", p->adj, p->place, p->pre[h % 8], p->suf[(h >> 8) % 8]);
    t->cx0 = s->cx0; t->cy = s->cy0; t->cz0 = s->cz0;
    int X0 = s->cx0 * CHUNK, Z0 = s->cz0 * CHUNK;
    t->x0 = X0 + 3; t->z0 = Z0 + 3; t->x1 = X0 + s->sx * CHUNK - 4; t->z1 = Z0 + s->sz * CHUNK - 4;
    t->floor = s->floor_y;
    t->px = X0 + s->sx * CHUNK / 2; t->pz = Z0 + s->sz * CHUNK / 2;
    /* the lots: one per chunk; the middle one the plaza, the corners (where the stairs come
     * up and go down) the market and a garden, the rest buildings in an order by the seed */
    u8 roles[6] = { TB_INN, TB_TEMPLE, TB_ELDER, TB_WORKSHOP, TB_HOUSE, TB_HOUSE };
    for (int i = 5; i > 0; i--) {
        int j = (int)(mix64(s->seed ^ (u64)i * 0x51) % (u64)(i + 1));
        u8 tmp = roles[i]; roles[i] = roles[j]; roles[j] = tmp;
    }
    int ri = 0;
    for (int lj = 0; lj < 3; lj++)
        for (int li = 0; li < 3; li++) {
            TownLot *L = &t->lot[lj * 3 + li];
            int lx0 = X0 + li * CHUNK, lz0 = Z0 + lj * CHUNK;
            if (li == 1 && lj == 1) L->role = TB_PLAZA;
            else if (li == 0 && lj == 0) L->role = TB_MARKET;
            else if (li == 2 && lj == 2) L->role = TB_GARDEN;
            else L->role = roles[ri++];
            L->x0 = lx0; L->z0 = lz0; L->x1 = lx0 + CHUNK - 1; L->z1 = lz0 + CHUNK - 1;
            if (L->role == TB_PLAZA || L->role == TB_MARKET || L->role == TB_GARDEN) continue;
            /* a building in the middle of its lot, its door toward the plaza */
            bool big = L->role == TB_ELDER || L->role == TB_INN;
            int hw = big ? 9 : 7, hd = big ? 7 : 6;
            int cx = lx0 + 16, cz = lz0 + 16;
            bool along_x = li != 1; /* the door faces the plaza: on an x wall unless in the middle column */
            int wx = along_x ? hd : hw, wz = along_x ? hw : hd;
            L->x0 = cx - wx; L->x1 = cx + wx - 1; L->z0 = cz - wz; L->z1 = cz + wz - 1;
            if (along_x) {
                L->door_x = li == 0 ? L->x1 : L->x0;
                L->door_z = cz;
                L->spot_x = L->door_x + (li == 0 ? -4 : 4);
                L->spot_z = cz;
            } else {
                L->door_x = cx;
                L->door_z = lj == 0 ? L->z1 : L->z0;
                L->spot_x = cx;
                L->spot_z = L->door_z + (lj == 0 ? -4 : 4);
            }
        }
    /* the market's stalls: round the rim of its lot, clear of the stair in its middle */
    const TownLot *m = &t->lot[0];
    static const int SX[6] = { 5, 5, 26, 26, 16, 5 }, SZ[6] = { 5, 26, 5, 26, 27, 16 };
    for (int k = 0; k < 6; k++) { t->stall_x[k] = m->x0 + SX[k]; t->stall_z[k] = m->z0 + SZ[k]; }
    t->nstalls = 6;
    return true;
}

bool world_town_at(const struct World *w, int cx, int cy, int cz, Town *out) {
    Structure s;
    if (!world_structure_at(w, cx, cy, cz, &s)) return false;
    return town_from_structure(&s, out);
}
