/* Fortress Crawl - platform layer: window, GL context, main loop.
 *
 * Controls: WASD move, mouse look, LMB attack, RMB alternate attack, 1-3 / wheel weapon,
 * Space jump, Shift sprint, Ctrl crouch, TAB skills, M map, F fly, T teleport ahead,
 * PgUp/PgDn one chunk up/down, F1 debug overlay, F2 resolution, F3 dither,
 * F4 vertex snap, F5 fullbright, F6 torch, F12 screenshot, Esc release mouse (again: quit).
 * Gamepads: START on an idle pad joins as a new split-screen player, BACK gives it to player 1. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "engine/font.h"
#include "engine/gl.h"
#include "engine/jobs.h"
#include "game/game.h"
#include "game/bot.h"
#include "render/batch.h"

static const char *g_demo_kind;
/* --weapon base[:rarity[:affix[:affix]]], e.g. greatsword:epic:fire:keen (screenshots of weapons) */
static const char *g_demo_weapon;
static int g_demo_grade; /* --grade 0|1|2: the bestiary demo shows normal, elite or boss monsters */
static int g_demo_mon = -1; /* --mon n: the portrait demo shows this kind */
static int g_demo_cast = -1; /* --cast n: the cast demo casts this utility spell (SU_*) */

static bool parse_args(Options *o, int argc, char **argv) {
    memset(o, 0, sizeof *o);
    o->seed = 1337;
    o->radius = 3;
    o->vradius = 2;
    o->win_w = 1280;
    o->win_h = 720;
    o->low_w = 480;
    o->low_h = 270;
    o->hud = false;
    o->sens = 1.0f;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--seed") && i + 1 < argc) {
            snprintf(o->seed_text, sizeof o->seed_text, "%s", argv[++i]);
            o->seed = menu_seed(o->seed_text);
            o->has_seed = true;
        }
        else if (!strcmp(a, "--pos") && i + 3 < argc) {
            o->has_pos = true;
            o->pos = v3(strtof(argv[i + 1], NULL), strtof(argv[i + 2], NULL), strtof(argv[i + 3], NULL));
            i += 3;
        } else if (!strcmp(a, "--look") && i + 2 < argc) {
            o->yaw = strtof(argv[i + 1], NULL) * PI_F / 180.0f;
            o->pitch = strtof(argv[i + 2], NULL) * PI_F / 180.0f;
            i += 2;
        } else if (!strcmp(a, "--fly")) o->fly = true;
        else if (!strcmp(a, "--radius") && i + 1 < argc) o->radius = atoi(argv[++i]);
        else if (!strcmp(a, "--vradius") && i + 1 < argc) o->vradius = atoi(argv[++i]);
        else if (!strcmp(a, "--shot") && i + 1 < argc) o->shot = argv[++i];
        else if (!strcmp(a, "--window") && i + 2 < argc) { o->win_w = atoi(argv[i + 1]); o->win_h = atoi(argv[i + 2]); i += 2; }
        else if (!strcmp(a, "--lowres") && i + 2 < argc) { o->low_w = atoi(argv[i + 1]); o->low_h = atoi(argv[i + 2]); i += 2; }
        else if (!strcmp(a, "--debug")) o->hud = true;
        else if (!strcmp(a, "--grab")) o->grab = true;
        else if (!strcmp(a, "--recenter")) o->recenter = 1;
        else if (!strcmp(a, "--no-recenter")) o->recenter = -1;
        else if (!strcmp(a, "--invert-x")) o->invert_x = true;
        else if (!strcmp(a, "--invert-y")) o->invert_y = true;
        else if (!strcmp(a, "--sens") && i + 1 < argc) o->sens = strtof(argv[++i], NULL);
        else if (!strcmp(a, "--shot-time") && i + 1 < argc) o->shot_time = strtof(argv[++i], NULL);
        else if (!strcmp(a, "--demo") && i + 1 < argc) { o->demo = true; g_demo_kind = argv[++i]; }
        else if (!strcmp(a, "--players") && i + 1 < argc) o->players = atoi(argv[++i]);
        else if (!strcmp(a, "--test-pad")) o->test_pad = true;
        else if (!strcmp(a, "--touch")) o->touch = true;
        else if (!strcmp(a, "--fresh")) o->fresh = true;
        else if (!strcmp(a, "--fullbright")) o->fullbright = true;
        else if (!strcmp(a, "--persist")) o->persist = true;
        else if (!strcmp(a, "--mute")) o->mute = true;
        else if (!strcmp(a, "--weapon") && i + 1 < argc) g_demo_weapon = argv[++i];
        else if (!strcmp(a, "--grade") && i + 1 < argc) g_demo_grade = atoi(argv[++i]);
        else if (!strcmp(a, "--mon") && i + 1 < argc) g_demo_mon = atoi(argv[++i]);
        else if (!strcmp(a, "--cast") && i + 1 < argc) g_demo_cast = atoi(argv[++i]);
        else if (!strcmp(a, "--bot") && i + 1 < argc) o->bot = argv[++i];
        else if (!strcmp(a, "--bot-log") && i + 1 < argc) o->bot_log = argv[++i];
        else if (!strcmp(a, "--bot-minutes") && i + 1 < argc) o->bot_minutes = strtof(argv[++i], NULL);
        else if (!strcmp(a, "--sound")) o->sound = true;
        else if (!strcmp(a, "--chapter") && i + 1 < argc) o->demo_chapter = atoi(argv[++i]) - 1;
        else {
            SDL_Log("usage: fortress_crawl [--seed N] [--pos x y z] [--look yaw pitch] [--fly] [--radius n] [--vradius n]\n"
                    "                      [--window w h] [--lowres w h] [--debug] [--invert-x] [--invert-y] [--sens s]\n"
                    "                      [--recenter | --no-recenter]\n"
                    "                      [--shot file.bmp] [--shot-time s] [--demo combat|sword|death|skills|map|coop|menu|inventory|spells|loot|magic|bestiary\n"
                    "                      |motive|story|read|named]\n"
                    "                      [--players n] [--test-pad] [--touch] [--fresh] [--persist] [--mute] [--sound]");
            return false;
        }
    }
    return true;
}

static void save_screenshot(Game *g, const char *path) {
    int w, h;
    unsigned char *px = renderer_read_pixels(&g->r, &w, &h);
    SDL_Surface *s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGB24, px, w * 3);
    if (s) {
        if (!SDL_SaveBMP(s, path)) SDL_Log("screenshot failed: %s", SDL_GetError());
        else SDL_Log("saved %s", path);
        SDL_DestroySurface(s);
    }
    free(px);
}

static int name_index(const char *s, int len, const char *const *names, int n) {
    for (int i = 0; i < n; i++)
        if ((int)strlen(names[i]) == len && !SDL_strncasecmp(s, names[i], (size_t)len)) return i;
    return -1;
}

static bool parse_weapon(const char *spec, Item *out) {
    static const char *BASES[WB_COUNT] = { "dagger", "sword", "axe", "mace", "spear", "greatsword",
                                           "shortbow", "longbow", "crossbow", "wand", "staff", "scepter" };
    static const char *RARS[RAR_COUNT] = { "common", "magic", "rare", "epic", "legendary" };
    static const char *AFF[AF_COUNT] = { "none", "fire", "frost", "shock", "venom", "vampiric", "swift", "keen", "heavy" };
    Item it = { 0 };
    it.used = 1;
    it.level = 8;
    int field = 0, naff = 0;
    for (const char *p = spec; field < 4;) {
        const char *e = strchr(p, ':');
        int len = e ? (int)(e - p) : (int)strlen(p);
        int k = field == 0 ? name_index(p, len, BASES, WB_COUNT) : field == 1 ? name_index(p, len, RARS, RAR_COUNT) : name_index(p, len, AFF, AF_COUNT);
        if (k < 0) return false;
        if (field == 0) it.base = (u8)k;
        else if (field == 1) it.rarity = (u8)k;
        else it.affix[naff++] = (u8)k;
        field++;
        if (!e) break;
        p = e + 1;
    }
    *out = it;
    return true;
}

/* A rack of every weapon on the floor ahead, in different rarities and affixes. */
static void demo_weapons(Game *g, PlayerSlot *p1) {
    static const u8 RAR[WB_COUNT] = { RAR_COMMON, RAR_MAGIC, RAR_RARE, RAR_EPIC, RAR_MAGIC, RAR_EPIC,
                                      RAR_COMMON, RAR_RARE, RAR_EPIC, RAR_MAGIC, RAR_RARE, RAR_EPIC };
    static const u8 AFX[WB_COUNT] = { AF_NONE, AF_FIRE, AF_FROST, AF_SHOCK, AF_VENOM, AF_VAMPIRIC,
                                      AF_NONE, AF_FIRE, AF_SHOCK, AF_FROST, AF_VENOM, AF_KEEN };
    vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw)), r = v3(-sinf(p1->pl.yaw), 0, cosf(p1->pl.yaw));
    for (int i = 0; i < WB_COUNT; i++) {
        Item it = { 1, (u8)i, RAR[i], 8, { AFX[i], 0 } };
        int row = i / 6, col = i % 6;
        vec3 p = v3_add(v3_add(p1->pl.pos, v3_scale(f, 2.0f + row * 1.3f)), v3_scale(r, (col - 2.5f) * (0.75f + row * 0.25f)));
        p.y += 1.0f + row * 0.35f; /* at eye height, so each can be seen */
        pickup_spawn(g, PK_WEAPON, p, 0, &it, 880 + (u64)i, 0);
        for (int k = MAX_PICKUPS - 1; k >= 0; k--)
            if (g->pickups[k].alive && g->pickups[k].key == 880 + (u64)i) g->pickups[k].t = 1.0f + i * 0.6f; /* turned differently */
    }
    for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
}

/* Moves player 1 to the most open floor spot of the chunk it is in and looks down the
 * longest view (screenshots of far places: --pos lands anywhere, often in rock). */
static void demo_place(Game *g, PlayerSlot *p1) {
    int cx = floordiv((int)floorf(p1->pl.pos.x), CHUNK), cy = floordiv((int)floorf(p1->pl.pos.y), CHUNK),
        cz = floordiv((int)floorf(p1->pl.pos.z), CHUNK);
    Rng r = rng_make(0x91ACE ^ (u64)(cx * 73 + cz * 31 + cy));
    float best = -1.0f;
    vec3 best_p = p1->pl.pos;
    for (int tries = 0; tries < 2000; tries++) {
        int x = cx * CHUNK + rng_range(&r, 1, CHUNK - 2), y = cy * CHUNK + rng_range(&r, 1, CHUNK - 4),
            z = cz * CHUNK + rng_range(&r, 1, CHUNK - 2);
        if (game_solid(g, x, y - 1, z) != 1 || game_solid(g, x, y, z) != 0 || game_solid(g, x, y + 1, z) != 0 ||
            game_solid(g, x, y + 2, z) != 0)
            continue;
        bool known;
        Block under = chunkmap_block(&g->map, x, y - 1, z, &known);
        if (under == B_LAVA || under == B_GLASS) continue;
        float open = game_measure_openness(g, v3(x + 0.5f, y + 1.6f, z + 0.5f));
        if (open > best) { best = open; best_p = v3(x + 0.5f, (float)y, z + 0.5f); }
    }
    if (best < 0) { SDL_Log("place demo: no open floor in chunk %d %d %d", cx, cy, cz); return; }
    p1->pl.pos = best_p;
    p1->pl.vel = v3(0, 0, 0);
    p1->pl.fly = true; /* hold still, no weapon in view */
    /* look down the longest free line */
    vec3 eye = player_eye(&p1->pl);
    float far = 0;
    for (int a = 0; a < 32; a++) {
        float yaw = a * 2.0f * PI_F / 32.0f;
        vec3 d = v3(cosf(yaw), 0, sinf(yaw));
        float t = phys_raycast(eye, d, 60.0f, game_solid, g);
        if (t > far) { far = t; p1->pl.yaw = yaw; }
    }
    p1->pl.pitch = -0.08f;
    for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
    SDL_Log("place demo: chunk %d %d %d, openness %.1f, view %.1f m", cx, cy, cz, best, far);
}

/* Scripted setups for screenshots of the combat systems. */
static void demo_setup(Game *g) {
    PlayerSlot *p1 = &g->players[0];
    Stats *s = &p1->st;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) {
            game_add_xp(g, &g->players[i], 1500.0f);
            g->players[i].banner_time = 0;
        }
    s->ranks[SK_KEEN_EDGE] = 2; s->ranks[SK_CLEAVE] = 1;
    s->ranks[SK_FOCUS] = 2; s->ranks[SK_FIREBALL] = 1;
    s->ranks[SK_TOUGHNESS] = 1;
    s->points -= 7;
    if (s->points < 0) s->points = 1;
    stats_refresh(p1);
    s->hp = s->maxhp * 0.8f;
    s->mana = s->maxmana;
    if (!strcmp(g_demo_kind, "skills")) { p1->show_skills = true; p1->skill_sel = skill_id(BR_ARCANE, 2); return; }
    if (!strcmp(g_demo_kind, "map")) { p1->show_map = true; return; }
    if (!strcmp(g_demo_kind, "inventory") || !strcmp(g_demo_kind, "spells") || !strcmp(g_demo_kind, "loot") ||
        !strcmp(g_demo_kind, "magic")) {
        /* a character some way into the game */
        for (int i = 0; i < BAG_SIZE - 5; i++) s->bag[i] = item_roll(1000 + (u64)i * 7, 6.0f + i * 0.5f, i % 4);
        s->equipped[WPN_SWORD] = item_roll(77, 9, RAR_RARE);
        s->equipped[WPN_SWORD].base = WB_AXE;
        s->gold = 342;
        s->potions_hp = 4;
        s->potions_mp = 3;
        s->relics[0] = 1; s->relics[3] = 2; s->relics[7] = 1;
        for (int i = 0; i < 30; i++) {
            int id = (int)(mix64(500 + (u64)i) % SPELL_COUNT);
            s->spell_rank[id] = (u8)(1 + mix64(900 + (u64)i) % 4);
        }
        s->spell_rank[spell_attack_id(EL_FIRE, SF_STORM)] = 2;
        s->spell = spell_attack_id(EL_FIRE, SF_STORM);
        p1->inv_sel = 4;
        p1->book_sel = s->spell;
        stats_refresh(p1);
        s->mana = s->maxmana;
        if (!strcmp(g_demo_kind, "inventory")) { p1->show_skills = true; p1->menu_tab = TAB_INVENTORY; return; }
        if (!strcmp(g_demo_kind, "spells")) { p1->show_skills = true; p1->menu_tab = TAB_SPELLS; return; }
    }
    if (!strcmp(g_demo_kind, "weapons")) { demo_weapons(g, p1); return; }
    if (!strcmp(g_demo_kind, "cast") && g_demo_cast >= 0 && g_demo_cast < SU_COUNT) {
        /* a utility spell cast in an open place, then a step back to see it */
        demo_place(g, p1);
        p1->pl.fly = false;
        vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw));
        int u = g_demo_cast;
        bool two = u == SU_GATES || u == SU_ANCHOR;
        bool building = u == SU_KEEP || u == SU_HUT || u == SU_WALL || u == SU_SANCTUARY || u == SU_GRAVWELL || u == SU_QUENCH;
        p1->pl.pitch = u == SU_BRIDGE || u == SU_STONESHAPE ? -0.1f : -0.2f;
        if (!utility_cast(g, p1, u, 3)) SDL_Log("cast demo: spell %d failed", u);
        if (two) {
            p1->pl.pos = v3_add(p1->pl.pos, v3_scale(f, 5.0f));
            p1->pl.yaw += PI_F * 0.5f;
            utility_cast(g, p1, u, 3);
            p1->pl.yaw -= PI_F * 0.5f;
            p1->pl.pos = v3_sub(p1->pl.pos, v3_scale(f, 8.0f));
        }
        if (building) {
            p1->pl.pos = v3_add(p1->pl.pos, v3_add(v3_scale(f, u == SU_KEEP ? -9.0f : -5.0f), v3(0, u == SU_KEEP ? 4.0f : 2.0f, 0)));
            p1->pl.pitch = -0.35f;
            p1->pl.fly = true;
        }
        if (u == SU_MIRROR || u == SU_SANCTUM) { /* step back to see the images round you, or the door */
            vec3 back = v3_sub(p1->pl.pos, v3_scale(f, u == SU_MIRROR ? 4.5f : 1.5f));
            if (phys_box_free(back, PLAYER_HALF_W, 1.75f, game_solid, g)) p1->pl.pos = back;
            p1->pl.pitch = -0.15f;
        }
        p1->banner_time = 0;
        return;
    }
    if (!strcmp(g_demo_kind, "realmtest")) {
        /* the Sanctum's doors and comforts, step by step (log only) */
        vec3 home = p1->pl.pos;
        s->sanctum = SANCTUM_BATH | SANCTUM_FEAST | SANCTUM_LIBRARY;
        realm_enter(g, p1, home, 1.0f);
        SDL_Log("realmtest: inside %d at %.1f %.1f %.1f, return set %d", realm_has(p1->pl.pos), p1->pl.pos.x, p1->pl.pos.y, p1->pl.pos.z,
                s->realm_return_set);
        s->hp = 10; s->burn_t = 5;
        p1->pl.pos = v3(28.5f, POCKET_G - 1.0f, 60.5f); /* in the bath */
        for (int k = 0; k < 30; k++) realm_update(g, 1.0f / 30);
        SDL_Log("realmtest: bath hp %.0f/%.0f burn %.1f refreshed %.0f", s->hp, s->maxhp, s->burn_t, s->refreshed_t);
        float before = s->maxhp;
        p1->pl.pos = v3(POCKET_TABLE_X0 - 1.0f, (float)POCKET_G, 60.5f); /* at the table */
        for (int k = 0; k < 60; k++) realm_update(g, 1.0f / 30);
        SDL_Log("realmtest: feast well fed %.0f maxhp %.0f -> %.0f hp %.0f", s->well_fed_t, before, s->maxhp, s->hp);
        p1->pl.pos = v3(POCKET_BED_X0 - 1.0f, (float)POCKET_G, 106.5f); /* by the bed */
        for (int k = 0; k < 90; k++) realm_update(g, 1.0f / 30);
        SDL_Log("realmtest: bed respawn %.1f %.1f %.1f (in realm %d)", s->respawn.x, s->respawn.y, s->respawn.z, realm_has(s->respawn));
        p1->gate_cd = 0;
        p1->pl.pos = v3(POCKET_EXIT_X, (float)POCKET_G, POCKET_EXIT_Z); /* through the door home */
        realm_update(g, 1.0f / 30);
        SDL_Log("realmtest: out at %.1f %.1f %.1f (home %.1f %.1f %.1f) inside %d", p1->pl.pos.x, p1->pl.pos.y, p1->pl.pos.z, home.x, home.y,
                home.z, realm_has(p1->pl.pos));
        return;
    }
    if (!strcmp(g_demo_kind, "shop")) {
        /* the SHOP tab at a shrine (use --pos next to one, e.g. the Sanctum library's) */
        s->gold = 2600;
        s->relics[2] = 1;
        s->relics[5] = 2;
        s->spell_rank[spell_attack_id(EL_FROST, SF_CHAIN)] = 2;
        s->spell = spell_attack_id(EL_FROST, SF_CHAIN);
        s->equipped[WPN_SWORD] = item_roll(91, 8, RAR_RARE);
        s->weapon = WPN_SWORD;
        for (int i = 0; i < 6; i++) s->bag[i] = item_roll(300 + (u64)i, 2, RAR_COMMON);
        s->sanctum = SANCTUM_BATH;
        p1->show_skills = true;
        p1->menu_tab = TAB_SHOP;
        p1->shop_sel = 3;
        return;
    }
    if (!strcmp(g_demo_kind, "realm")) {
        /* through the Sanctum Door: --pos inside the realm picks the spot */
        p1->pl.fly = true;
        p1->banner_time = 0;
        return;
    }
    if (!strcmp(g_demo_kind, "place") || !strcmp(g_demo_kind, "placefight")) {
        demo_place(g, p1);
        if (!strcmp(g_demo_kind, "placefight")) {
            /* one of each kind down the view, to see the region's monsters */
            vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw)), r = v3(-sinf(p1->pl.yaw), 0, cosf(p1->pl.yaw));
            for (int t = 0; t < MON_TYPE_COUNT; t++) {
                vec3 at = v3_add(v3_add(p1->pl.pos, v3_scale(f, 4.0f + t * 1.3f)), v3_scale(r, (t % 2 ? -1.0f : 1.0f) * (0.6f + (t % 3) * 0.5f)));
                at.y += MONSTERS[t].flies ? 1.2f : 0.0f;
                if (!phys_box_free(at, MONSTERS[t].hw, MONSTERS[t].h, game_solid, g)) at = v3_add(p1->pl.pos, v3_scale(f, 3.0f + t * 0.8f));
                Monster *m = monster_spawn(g, t, at, 40, t == MON_BRUTE, v3(0.5f, 0.45f, 0.4f), 9000 + (u64)t);
                if (m) { m->yaw = p1->pl.yaw + PI_F; m->awake = false; }
            }
        }
        return;
    }
    if (!strcmp(g_demo_kind, "impact")) {
        /* one tough monster right ahead, for the hit effects of the weapon in hand */
        for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
        vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw));
        Monster *m = monster_spawn(g, MON_BRUTE, v3_add(p1->pl.pos, v3_scale(f, 2.2f)), 30, false, v3(0.5f, 0.45f, 0.4f), 4242);
        if (m) { m->yaw = p1->pl.yaw + PI_F; m->hp = m->maxhp = 1e6f; m->cooldown = 99.0f; }
        return;
    }
    if (!strcmp(g_demo_kind, "loot")) {
        /* a chest and some loot on the floor ahead */
        vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw)), r = v3(-sinf(p1->pl.yaw), 0, cosf(p1->pl.yaw));
        vec3 base = v3_add(p1->pl.pos, v3_scale(f, 4.0f));
        Item it = item_roll(4242, 10, RAR_EPIC);
        pickup_spawn(g, PK_CHEST, v3_add(base, v3_scale(f, 2.0f)), 5, NULL, 99, 0);
        pickup_spawn(g, PK_WEAPON, v3_add(base, v3_scale(r, -1.5f)), 0, &it, 98, 0);
        pickup_spawn(g, PK_SCROLL, v3_add(base, v3_scale(r, 1.5f)), spell_attack_id(EL_FROST, SF_NOVA), NULL, 97, 0);
        pickup_spawn(g, PK_RELIC, v3_add(v3_add(base, v3_scale(r, -0.5f)), v3(0, 0.6f, 0)), 4, NULL, 96, 0);
        pickup_spawn(g, PK_GOLD, v3_add(base, v3_scale(r, 0.6f)), 40, NULL, 95, 0);
        pickup_spawn(g, PK_POTION_HP, v3_add(v3_add(base, v3_scale(r, 2.6f)), v3_scale(f, 1.0f)), 1, NULL, 94, 0);
        pickup_spawn(g, PK_POTION_MP, v3_add(v3_add(base, v3_scale(r, -2.6f)), v3_scale(f, 1.0f)), 1, NULL, 93, 0);
        if (g->opt.shot_time > 0) {
            /* test: things at your feet are picked up (a chest too) */
            pickup_spawn(g, PK_GOLD, v3_add(p1->pl.pos, v3(0.3f, 0.3f, 0)), 25, NULL, 0, 0);
            pickup_spawn(g, PK_SCROLL, v3_add(p1->pl.pos, v3(-0.3f, 0.3f, 0)), spell_attack_id(EL_POISON, SF_ORB), NULL, 0, 0);
            pickup_spawn(g, PK_CHEST, v3_add(p1->pl.pos, v3(0.8f, 0.0f, 0.5f)), 8, NULL, 92, 0);
        }
        for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
        return;
    }
    {
        static const char *PUZ[] = { "vault", "levers", "plates", "statues", "beam" };
        static const char *TRAPS[] = { "", "darts", "spikes", "flames" };
        for (int i = 0; i < 5; i++)
            if (!strcmp(g_demo_kind, PUZ[i])) {
                if (!mech_demo_view(g, p1, i)) SDL_Log("mech demo: no such vault nearby");
                for (int k = 0; k < MAX_MONSTERS; k++) g->mons[k].alive = false;
                return;
            }
        for (int i = 1; i < 4; i++)
            if (!strcmp(g_demo_kind, TRAPS[i])) {
                if (!mech_demo_trap(g, p1, i)) SDL_Log("mech demo: no such trap nearby");
                for (int k = 0; k < MAX_MONSTERS; k++) g->mons[k].alive = false;
                return;
            }
        if (!strcmp(g_demo_kind, "mechtest")) {
            for (int k = 0; k < MAX_MONSTERS; k++) g->mons[k].alive = false;
            SDL_Log("mechtest: %s", mech_test_solve(g, p1) ? "passed" : "FAILED");
            return;
        }
    }
    if (!strcmp(g_demo_kind, "thread")) {
        /* the golden thread to the first chapter target, from wherever --pos put us */
        s->motive = MOTIVE_REVENGE + 1;
        s->chapter = g->opt.demo_chapter;
        p1->pl.fly = false;
        u64 t0 = SDL_GetTicksNS();
        bool ok = story_cast_thread(g, p1);
        Named t;
        story_chapter_target(&g->world, s->chapter, &t);
        SDL_Log("thread: %s, %d points, reached %d, %.1f ms; target %s at chunk %d %d %d; banner \"%s\"", ok ? "ok" : "none",
                p1->thread.n, p1->thread.reached, (SDL_GetTicksNS() - t0) / 1e6, t.name, t.cx, t.cy, t.cz, p1->banner);
        return;
    }
    if (!strcmp(g_demo_kind, "story") || !strcmp(g_demo_kind, "read") || !strcmp(g_demo_kind, "named")) {
        /* a revenge story in its second chapter, with some lore read and two missions */
        s->motive = MOTIVE_REVENGE + 1;
        s->chapter = 1;
        lore_add(g, LORE_REVEAL, 0, 0, 0);
        int found_bounty = 0, found_retrieve = 0, nread = 0;
        for (int cz = -5; cz <= 5; cz++)
            for (int cx = -5; cx <= 5; cx++) {
                Trace t;
                if (!story_trace(&g->world, cx, 0, cz, &t)) continue;
                if (nread < 6) { lore_add(g, LORE_TRACE, cx, 0, cz); nread++; }
                Mission mi = { 0 };
                mi.sx = cx; mi.sy = 0; mi.sz = cz;
                if (t.kind == TR_BOUNTY && !found_bounty) {
                    Named l;
                    int gold;
                    story_bounty(&g->world, cx, 0, cz, &l, &gold);
                    mi.type = MI_BOUNTY; mi.key = l.key; mi.tx = l.cx; mi.ty = l.cy; mi.tz = l.cz; mi.reward = gold;
                    s->missions[s->nmissions++] = mi;
                    found_bounty = 1;
                } else if (t.kind == TR_CORPSE && t.mission && !found_retrieve) {
                    Retrieve rt;
                    story_retrieve(&g->world, cx, 0, cz, &rt);
                    mi.type = MI_RETRIEVE; mi.key = rt.key; mi.tx = rt.cx; mi.ty = rt.cy; mi.tz = rt.cz; mi.reward = 60;
                    s->missions[s->nmissions++] = mi;
                    found_retrieve = 1;
                }
                if (!strcmp(g_demo_kind, "read") && t.kind == TR_STELE && !p1->reading[0]) {
                    story_trace_text(&g->world, cx, 0, cz, p1->reading, sizeof p1->reading);
                    p1->reading_t = 100.0f;
                    p1->reading_key = 1;
                }
            }
        if (!strcmp(g_demo_kind, "story")) { p1->show_skills = true; p1->menu_tab = TAB_STORY; }
        if (!strcmp(g_demo_kind, "named")) {
            for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
            Named t;
            story_chapter_target(&g->world, s->chapter, &t);
            vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw));
            Monster *m = story_spawn_named_at(g, &t, v3_add(p1->pl.pos, v3_scale(f, 7.0f)));
            if (m) { m->yaw = p1->pl.yaw + PI_F; m->awake = false; }
        }
        return;
    }
    if (!strcmp(g_demo_kind, "portrait") || !strcmp(g_demo_kind, "duel")) {
        /* one monster (--mon n, --grade g) standing in front, at a distance that fits its size */
        for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
        int t = g_demo_mon >= 0 && g_demo_mon < MON_TYPE_COUNT ? g_demo_mon : 0;
        vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw));
        float size = MONSTERS[t].h * (g_demo_grade == 2 ? 1.9f : g_demo_grade == 1 ? 1.35f : 1.0f);
        vec3 at = v3_add(p1->pl.pos, v3_scale(f, 1.6f + size * 1.25f));
        if (MONSTERS[t].flies) at.y += 0.6f;
        Monster *m = monster_spawn(g, t, at, 3, g_demo_grade >= 1, v3(0.5f, 0.45f, 0.4f), 777);
        if (m && g_demo_grade == 2) monster_make_boss(g, m);
        if (m) {
            m->yaw = p1->pl.yaw + PI_F + 0.5f; /* a little turned, to show its side */
            m->awake = t == MON_MIMIC && g_demo_grade > 0; /* show the mimic both ways */
        }
        p1->pl.pitch = -atanf((1.6f - size * 0.45f) / (1.6f + size * 1.25f));
        bool duel = !strcmp(g_demo_kind, "duel");
        p1->pl.fly = !duel;
        g->freeze_monsters = !duel;
        if (duel && m) { m->awake = t != MON_MIMIC; m->state = MS_CHASE; p1->st.hp = p1->st.maxhp = 5000; } /* a fight to watch */
        return;
    }
    if (!strcmp(g_demo_kind, "bestiary")) {
        /* every kind in two rows, asleep (--grade 1 elites, 2 bosses) */
        for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
        vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw)), r = v3(-sinf(p1->pl.yaw), 0, cosf(p1->pl.yaw));
        float gap = g_demo_grade == 2 ? 3.0f : 1.8f;
        for (int t = 0; t < MON_TYPE_COUNT; t++) {
            int row = t / 10, col = t % 10;
            vec3 at = v3_add(v3_add(p1->pl.pos, v3_scale(f, 7.0f + row * gap * 1.8f)), v3_scale(r, (col - 4.5f) * gap));
            if (MONSTERS[t].flies) at.y += 1.2f;
            Monster *m = monster_spawn(g, t, at, 3, g_demo_grade >= 1, v3(0.5f, 0.45f, 0.4f), 555 + (u64)t);
            if (m && g_demo_grade == 2) monster_make_boss(g, m);
        }
        g->freeze_monsters = true;
        for (int i = 0; i < MAX_MONSTERS; i++)
            if (g->mons[i].alive) { g->mons[i].yaw = p1->pl.yaw + PI_F; g->mons[i].awake = false; }
        return;
    }
    /* combat: a pack in front of the player */
    for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
    vec3 eye = player_eye(&p1->pl);
    static const struct { int type; float dist, side; bool elite; } PACK[] = {
        { MON_CRAWLER, 8.0f, -1.5f, false }, { MON_CRAWLER, 9.0f, 1.2f, false }, { MON_HORNED, 11.0f, 2.6f, false },
        { MON_WISP, 10.0f, -3.0f, false }, { MON_BRUTE, 14.0f, 0.3f, true },
    };
    vec3 f = v3(cosf(p1->pl.yaw), 0, sinf(p1->pl.yaw)), r = v3(-sinf(p1->pl.yaw), 0, cosf(p1->pl.yaw));
    for (int i = 0; i < ARRAY_LEN(PACK); i++) {
        vec3 p = v3_add(v3_add(eye, v3_scale(f, PACK[i].dist)), v3_scale(r, PACK[i].side));
        int x = (int)floorf(p.x), z = (int)floorf(p.z);
        for (int y = (int)floorf(eye.y) + 2; y > (int)floorf(eye.y) - 8; y--)
            if (game_solid(g, x, y, z) == 0 && game_solid(g, x, y - 1, z) == 1) {
                vec3 at = v3(x + 0.5f, (float)y + (PACK[i].type == MON_WISP ? 1.2f : 0.0f), z + 0.5f);
                monster_spawn(g, PACK[i].type, at, 4, PACK[i].elite, v3(0.5f, 0.45f, 0.4f), 777 + (u64)i);
                g->mons[0].yaw = 0;
                break;
            }
    }
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (g->mons[i].alive) g->mons[i].yaw = p1->pl.yaw + PI_F;
    s->weapon = !strcmp(g_demo_kind, "sword") ? WPN_SWORD : WPN_STAFF;
    if (!strcmp(g_demo_kind, "magic")) {
        /* spells at the pack: a firestorm, a fireball and a frost rune */
        static const int CAST[3] = { 0, 1, 2 };
        int ids[3] = { spell_attack_id(EL_FIRE, SF_STORM), spell_attack_id(EL_FIRE, SF_ORB), spell_attack_id(EL_LIGHTNING, SF_CHAIN) };
        for (int k = 0; k < 3; k++) {
            s->spell_rank[ids[CAST[k]]] = 3;
            s->spell = ids[CAST[k]];
            s->spell_cd = 0;
            s->mana = s->maxmana;
            p1->pl.pitch = k == 0 ? -0.15f : 0.0f;
            spell_cast(g, p1);
        }
        p1->pl.pitch = 0;
        s->spell = ids[0];
    }
    /* co-op: the other players stand beside player 1 and look at the pack */
    for (int i = 1; i < MAX_PLAYERS; i++) {
        PlayerSlot *o = &g->players[i];
        if (!o->active) continue;
        o->st.weapon = i % 2 ? WPN_BOW : WPN_SWORD;
        vec3 at = v3_add(v3_add(p1->pl.pos, v3_scale(f, 3.0f)), v3_scale(r, (i % 2 ? -1.3f : 1.3f) * (float)((i + 1) / 2)));
        if (phys_box_free(at, PLAYER_HALF_W, player_height(STANCE_STAND), game_solid, g)) o->pl.pos = at;
        o->pl.yaw = p1->pl.yaw + (i % 2 ? 0.35f : -0.35f);
    }
    if (!strcmp(g_demo_kind, "death")) s->hp = 1.0f; /* exercises death and shrine respawn */
}

/* ---------- mouse capture ----------
 * Preferred: SDL relative mouse mode. On X11 it needs SDL built with XInput2, and
 * some setups (WSLg, remote desktops) have none. Then we fall back to confining the
 * cursor to the window, measuring movement between absolute positions and
 * re-centring the cursor ourselves. If re-centring does not work either (the next
 * position is not the centre), we stop trusting warps instead of reading the jump
 * back as a huge motion in the opposite direction. */

typedef struct App {
    SDL_Window *win;
    SDL_GLContext ctx;
    Game *g;
    bool running, demo_done;
    u64 last;
    float acc, fps_acc, shot_wait;
    int fps_frames;
    /* fallback mouse look */
    bool manual;
    bool warp_ok, warp_pending;
    float last_x, last_y;
    int suspicious; /* motion events that show relative mode is not really active */
    SDL_Joystick *vpad; /* --test-pad */
    int touch_low_w;    /* low-res width before the touch controls widened it */
    bool in_menu;
    StartMenu menu;
} App;

static void mouse_capture(App *a, bool on);

static void enable_fallback(App *a, const char *why) {
    SDL_Log("Relative mouse mode is not available (%s). Using the cursor-grab fallback.", why);
    SDL_Log("On Linux, install libxi-dev (XInput2) before building SDL3 for proper mouse capture.");
    a->manual = true;
    a->warp_ok = true;
    a->warp_pending = false;
}

static void mouse_capture(App *a, bool on) {
    Game *g = a->g;
    if (on == g->mouse) return;
    if (!on) {
        if (a->manual) {
            SDL_SetWindowMouseGrab(a->win, false);
            SDL_ShowCursor();
        } else {
            SDL_SetWindowRelativeMouseMode(a->win, false);
        }
        g->mouse = false;
        g->players[0].attack_held = g->players[0].alt_held = false;
        return;
    }
    if (!a->manual && SDL_SetWindowRelativeMouseMode(a->win, true)) {
        g->mouse = true;
        return;
    }
    if (!a->manual) enable_fallback(a, SDL_GetError());
    SDL_SetWindowMouseGrab(a->win, true);
    SDL_HideCursor();
    SDL_GetMouseState(&a->last_x, &a->last_y);
    g->mouse = true;
}

/* Turns a motion event into a look delta in pixels; returns false if there is none. */
static bool mouse_delta(App *a, const SDL_MouseMotionEvent *m, float *dx, float *dy) {
#ifdef __EMSCRIPTEN__
    *dx = m->xrel;
    *dy = m->yrel;
    return true; /* the browser's pointer lock always delivers real movement */
#endif
    if (!a->manual) {
        /* SDL accepts relative mode even when the platform cannot do it, and then only
         * delivers zero-delta events (thanks to the warp-motion hint) while the cursor
         * moves freely. A run of those means: switch to the fallback. */
        bool moved = m->x != a->last_x || m->y != a->last_y;
        a->last_x = m->x;
        a->last_y = m->y;
        if (m->xrel == 0 && m->yrel == 0) {
            if (moved && ++a->suspicious >= 6) {
                SDL_SetWindowRelativeMouseMode(a->win, false);
                a->g->mouse = false;
                enable_fallback(a, "no relative motion events");
                mouse_capture(a, true);
            }
            return false;
        }
        a->suspicious = 0;
        *dx = m->xrel;
        *dy = m->yrel;
        return true;
    }
    int w, h;
    SDL_GetWindowSize(a->win, &w, &h);
    float cx = w * 0.5f, cy = h * 0.5f;
    if (a->warp_pending) {
        a->warp_pending = false;
        bool at_centre = fabsf(m->x - cx) < 2.0f && fabsf(m->y - cy) < 2.0f;
        if (!at_centre) {
            /* the warp did not happen: never read this jump as movement */
            a->warp_ok = false;
            SDL_Log("Cursor warping does not work here; mouse look stops at the window edge.");
        }
        a->last_x = m->x;
        a->last_y = m->y;
        if (at_centre) return false;
    }
    *dx = m->x - a->last_x;
    *dy = m->y - a->last_y;
    a->last_x = m->x;
    a->last_y = m->y;
    /* re-centre before the cursor reaches the window border */
    if (a->warp_ok && (m->x < w * 0.25f || m->x > w * 0.75f || m->y < h * 0.25f || m->y > h * 0.75f)) {
        SDL_WarpMouseInWindow(a->win, cx, cy);
        a->warp_pending = true;
    }
    return true;
}

/* WSLg forwards the Windows cursor as absolute positions and cannot lock it, so every
 * time SDL re-centres the hidden cursor, the jump back comes in as motion: the view
 * moves against the hand. Without re-centring, deltas are right (the cursor just is
 * not confined). */
static bool running_under_wsl(void) {
#ifdef __linux__
    if (getenv("WSL_DISTRO_NAME") || getenv("WSL_INTEROP")) return true;
    FILE *f = fopen("/proc/version", "r");
    if (f) {
        char buf[256] = { 0 };
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = 0;
        if (strstr(buf, "icrosoft") || strstr(buf, "WSL")) return true;
    }
#endif
    return false;
}

static void handle_events(App *a) {
    Game *g = a->g;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) a->running = false;
        else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || e.type == SDL_EVENT_WINDOW_RESIZED) {
            int w, h;
            SDL_GetWindowSizeInPixels(a->win, &w, &h);
            renderer_resize(&g->r, w, h);
        }
        else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) mouse_capture(a, false);
        else if (e.type == SDL_EVENT_MOUSE_MOTION) {
            float dx, dy;
            if (g->mouse && mouse_delta(a, &e.motion, &dx, &dy)) game_look(g, dx, dy);
        } else if ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) && e.button.which == SDL_TOUCH_MOUSEID) {
            /* mouse events made up from touches: the touch controls handle those */
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !g->mouse) {
            if (!g->touch.forced) g->touch.active = false; /* a real mouse: back to mouse controls */
            mouse_capture(a, true); /* the click that captures the mouse does not attack */
        } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE && !e.key.repeat) {
            PlayerSlot *p1 = &g->players[0];
            if (p1->show_skills || p1->show_map) p1->show_skills = p1->show_map = false;
            else if (g->mouse) mouse_capture(a, false);
#ifndef __EMSCRIPTEN__
            else a->running = false;
#endif
        } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F12 && !e.key.repeat) {
#ifndef __EMSCRIPTEN__
            save_screenshot(g, "screenshot.bmp");
#endif
        } else {
            game_event(g, &e);
        }
    }
#ifdef __EMSCRIPTEN__
    /* the browser releases pointer lock on its own (Esc, tab switch) */
    if (g->mouse && !a->manual && !SDL_GetWindowRelativeMouseMode(a->win)) {
        g->mouse = false;
        g->players[0].attack_held = g->players[0].alt_held = false;
    }
#endif
}

/* --test-pad: a virtual gamepad joins with START, walks forward, turns and attacks. */
/* --demo storytest: reads a real note or bounty board, kills the first chapter target,
 * and carries a quest item home, logging each step (used as an end-to-end test). */
static void story_test_step(App *a) {
    Game *g = a->g;
    static int phase;
    static u64 quest_key;
    if (!g_demo_kind || strcmp(g_demo_kind, "storytest") || !a->demo_done) return;
    PlayerSlot *p1 = &g->players[0];
    float t = a->shot_wait;
    if (phase == 0 && t > 0.8f) {
        /* walk up to a mission trace (prefer a corpse note) */
        Pickup *best = NULL;
        for (int pass = 0; pass < 2 && !best; pass++)
            for (int i = 0; i < MAX_PICKUPS; i++) {
                Pickup *pk = &g->pickups[i];
                if (!pk->alive || pk->kind != PK_TRACE) continue;
                int cx = floordiv((int)floorf(pk->pos.x), CHUNK), cy = floordiv((int)floorf(pk->pos.y), CHUNK), cz = floordiv((int)floorf(pk->pos.z), CHUNK);
                Trace tr;
                if (story_trace(&g->world, cx, cy, cz, &tr) && tr.mission && (pass || tr.kind == TR_CORPSE)) { best = pk; break; }
            }
        if (best) {
            p1->pl.pos = v3_add(best->pos, v3(0.8f, 0, 0));
            SDL_Log("storytest: walked to a %s", best->amount == TR_CORPSE ? "corpse note" : "bounty board");
        } else {
            SDL_Log("storytest: no mission trace loaded nearby");
        }
        phase = 1;
    } else if (phase == 1 && t > 1.2f) {
        SDL_Log("storytest: read \"%s\"", p1->reading);
        SDL_Log("storytest: missions %d, lore %d", p1->st.nmissions, g->nlore);
        for (int i = 0; i < p1->st.nmissions; i++)
            if (p1->st.missions[i].type == MI_RETRIEVE) quest_key = p1->st.missions[i].key;
        /* the chapter target, here and now */
        Named n;
        story_chapter_target(&g->world, p1->st.chapter, &n);
        Monster *m = story_spawn_named_at(g, &n, v3_add(p1->pl.pos, v3(2.0f, 0, 0)));
        int before = p1->st.chapter;
        if (m) monster_damage(g, m, 1e9f, p1->pl.pos, 0, 0);
        SDL_Log("storytest: killed %s: chapter %d -> %d, lore %d, message \"%s\"", n.name, before + 1, p1->st.chapter + 1, g->nlore, p1->msg);
        phase = 2;
        if (quest_key) {
            for (int i = 0; i < p1->st.nmissions; i++)
                if (p1->st.missions[i].key == quest_key) {
                    Mission *mi = &p1->st.missions[i];
                    p1->pl.pos = v3(mi->tx * CHUNK + 16.0f, mi->ty * CHUNK + 20.0f, mi->tz * CHUNK + 16.0f);
                    p1->pl.fly = true;
                    SDL_Log("storytest: flying to the lost item at chunk %d %d %d", mi->tx, mi->ty, mi->tz);
                }
        }
    } else if (phase == 2 && quest_key) {
        for (int i = 0; i < MAX_PICKUPS; i++) {
            Pickup *pk = &g->pickups[i];
            if (pk->alive && pk->kind == PK_QUEST && pk->key == quest_key) {
                int gold = p1->st.gold, nm = p1->st.nmissions;
                p1->pl.pos = pk->pos;
                p1->pl.fly = false;
                SDL_Log("storytest: found the item at %.0f %.0f %.0f (gold %d, missions %d)", pk->pos.x, pk->pos.y, pk->pos.z, gold, nm);
                phase = 3;
                break;
            }
        }
    } else if (phase == 3 && t > 0) {
        static float wait;
        wait += 1.0f / 60.0f;
        if (wait > 0.5f) {
            SDL_Log("storytest: after pick-up: gold %d, missions %d, item taken %d", p1->st.gold, p1->st.nmissions, looted_has(g, quest_key));
            phase = 4;
        }
    }
}

static void test_pad_step(App *a) {
    if (!a->vpad) return;
    float t = a->shot_wait;
    bool start = t > 0.1f && t < 0.2f;
    SDL_SetJoystickVirtualButton(a->vpad, SDL_GAMEPAD_BUTTON_START, start);
    bool go = t > 0.4f;
    SDL_SetJoystickVirtualAxis(a->vpad, SDL_GAMEPAD_AXIS_LEFTY, go ? -32767 : 0);
    SDL_SetJoystickVirtualAxis(a->vpad, SDL_GAMEPAD_AXIS_RIGHTX, go && t < 0.7f ? 20000 : 0);
    SDL_SetJoystickVirtualAxis(a->vpad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, go && fmodf(t, 0.5f) < 0.25f ? 32767 : 0);
}

static void screenshot_step(App *a, float frame_dt) {
    Game *g = a->g;
    PlayerSlot *p1 = &g->players[0];
    if (chunkmap_settled(&g->map, player_eye(&p1->pl))) a->shot_wait += frame_dt;
    test_pad_step(a);
    story_test_step(a);
    if (g->opt.demo && !a->demo_done && a->shot_wait > 0.5f) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            PlayerSlot *ps = &g->players[i];
            if (!ps->active) continue;
            ps->openness = game_measure_openness(g, player_eye(&ps->pl));
            for (int k = 0; k < 60; k++) game_update_atmosphere(g, ps, 0.5f);
        }
        demo_setup(g);
        a->demo_done = true;
        Item it;
        if (g_demo_weapon && parse_weapon(g_demo_weapon, &it)) {
            int wc = WEAPON_BASES[it.base].wclass;
            p1->st.equipped[wc] = it;
            p1->st.weapon = wc;
        } else if (g_demo_weapon) SDL_Log("--weapon: not understood: %s", g_demo_weapon);
    }
    const char *kind = g_demo_kind ? g_demo_kind : "";
    bool fight = !strcmp(kind, "combat") || !strcmp(kind, "sword") || !strcmp(kind, "death") || !strcmp(kind, "coop") ||
                 !strcmp(kind, "impact");
    bool magic = !strcmp(kind, "magic");
    if (g->opt.demo && a->demo_done && fight) {
        p1->attack_held = a->shot_wait > 0.9f && a->shot_wait < 0.95f;
        p1->alt_held = false;
    }
    float capture = g->opt.shot_time > 0 ? g->opt.shot_time : 1.0f;
    if (g->opt.demo && fight && g->opt.shot_time <= 0) capture = !strcmp(kind, "sword") ? 1.0f : 1.08f;
    if (g->opt.demo && magic && g->opt.shot_time <= 0) capture = 1.15f;
    if (a->shot_wait > capture || (g->time > 120.0f && g->opt.shot_time <= 0)) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            PlayerSlot *ps = &g->players[i];
            if (!ps->active) continue;
            if (!g->opt.demo) {
                ps->openness = game_measure_openness(g, player_eye(&ps->pl));
                for (int k = 0; k < 60; k++) game_update_atmosphere(g, ps, 0.5f);
                ps->banner_time = 0;
            }
            int known = 0, bag = 0;
            for (int k = 0; k < SPELL_COUNT; k++) known += ps->st.spell_rank[k] > 0;
            for (int k = 0; k < BAG_SIZE; k++) bag += ps->st.bag[k].used;
            SDL_Log("P%d gold %d  potions %d/%d  spells %d  bag %d  motive %d  chapter %d  missions %d  lore %d", i + 1, ps->st.gold,
                    ps->st.potions_hp, ps->st.potions_mp, known, bag, ps->st.motive, ps->st.chapter + 1, ps->st.nmissions, g->nlore);
            SDL_Log("P%d time %.1f  hp %.0f/%.0f  level %d  kills %d  dead %d  pad %d  pos %.1f %.1f %.1f  yaw %.1f pitch %.1f",
                    i + 1, g->time, ps->st.hp, ps->st.maxhp, ps->st.level, ps->st.kills, ps->st.dead, ps->pad != NULL,
                    ps->pl.pos.x, ps->pl.pos.y, ps->pl.pos.z, ps->pl.yaw * 180.0f / PI_F, ps->pl.pitch * 180.0f / PI_F);
        }
        game_render(g);
        save_screenshot(g, g->opt.shot);
        a->running = false;
    }
}

/* Generates the world for the chosen seed and puts the players in front of the gate. */
static void start_world(App *a) {
    Game *g = a->g;
    if (!g->opt.seed_text[0]) snprintf(g->opt.seed_text, sizeof g->opt.seed_text, "%llu", (unsigned long long)g->opt.seed);
    SDL_Log("World seed: %s (%llu)", g->opt.seed_text, (unsigned long long)g->opt.seed);
    world_init(&g->world, g->opt.seed);
    story_game_init(g);
    chunkmap_init(&g->map, &g->world, &g->r, g->opt.radius, g->opt.vradius);
    /* the saved characters of this world (screenshot and test runs start fresh) */
    if ((!g->opt.shot || g->opt.persist) && !g->opt.fresh) game_load(g);
    vec3 start = g->opt.has_pos ? g->opt.pos : v3(-70.5f, 6.0f, 0.5f);
    bool touch = g->touch.active;
    game_init(g, start, g->opt.yaw);
    g->touch.active = touch || g->opt.touch;
    g->touch.forced = g->opt.touch;
    g->players[0].pl.pitch = g->opt.pitch;
    g->players[0].pl.fly = g->opt.fly;
    /* --players n: extra characters without a device yet (a pad takes one over with START) */
    for (int i = 1; i < g->opt.players && i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = game_add_player(g);
        if (ps) { ps->pl.pitch = g->opt.pitch; ps->pl.fly = g->opt.fly; }
    }
    a->in_menu = false;
    g->world_started = true;
#ifndef __EMSCRIPTEN__
    /* in the browser, pointer lock needs a click first */
    if ((!g->opt.shot || g->opt.grab) && !g->touch.active) mouse_capture(a, true);
#endif
}

static void menu_frame(App *a, float frame_dt) {
    Game *g = a->g;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) a->running = false;
        else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || e.type == SDL_EVENT_WINDOW_RESIZED) {
            int w, h;
            SDL_GetWindowSizeInPixels(a->win, &w, &h);
            renderer_resize(&g->r, w, h);
        }
#ifndef __EMSCRIPTEN__
        else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) a->running = false;
#endif
        else menu_event(&a->menu, g, &e);
    }
    menu_draw(&a->menu, g, frame_dt);
    music_update(g, frame_dt, true);
    if (g->opt.shot) {
        /* screenshot of the start screen */
        save_screenshot(g, g->opt.shot);
        a->running = false;
    }
    SDL_GL_SwapWindow(a->win);
    if (a->menu.done) {
        snprintf(g->opt.seed_text, sizeof g->opt.seed_text, "%s", a->menu.text);
        g->opt.seed = menu_seed(a->menu.text);
        SDL_JoystickID pad = a->menu.pad;
        start_world(a);
        game_give_pad_to_p1(g, pad);
    }
#ifdef __EMSCRIPTEN__
    if (!a->running) emscripten_cancel_main_loop();
#endif
}

static void frame(void *user) {
    App *a = user;
    Game *g = a->g;
    const float DT = 1.0f / 60.0f;
    u64 now = SDL_GetTicksNS();
    float frame_dt = (float)((now - a->last) / 1e9);
    a->last = now;
    if (frame_dt > 0.25f) frame_dt = 0.25f;
    if (g->opt.shot) frame_dt = DT; /* deterministic stepping for screenshots */
    a->fps_acc += frame_dt;
    a->fps_frames++;
    if (a->fps_acc > 0.5f) { g->fps = a->fps_frames / a->fps_acc; a->fps_acc = 0; a->fps_frames = 0; }

    /* touch screens get no letterbox: the low-res target takes the screen's aspect */
    if (g->touch.active) {
        if (!a->touch_low_w) a->touch_low_w = g->r.set.low_w; /* to restore when touch ends */
        g->r.set.low_w = touch_low_width(g, g->r.win_w, g->r.win_h);
    } else if (a->touch_low_w) {
        g->r.set.low_w = a->touch_low_w;
        a->touch_low_w = 0;
    }

    if (a->in_menu) {
        menu_frame(a, frame_dt);
        return;
    }

    handle_events(a);

    const bool *keys = SDL_GetKeyboardState(NULL);
    PlayerInput in = { 0 };
    if (g->mouse && !g->players[0].show_skills && !g->players[0].show_map) {
        in.move_z = (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
        in.move_x = (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
        in.jump = keys[SDL_SCANCODE_SPACE];
        in.up = keys[SDL_SCANCODE_SPACE];
        in.crouch = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_C];
        in.down = in.crouch;
        in.sprint = keys[SDL_SCANCODE_LSHIFT];
    }
    vec3 centres[MAX_PLAYERS];
    int ncentres = game_centres(g, centres);
    chunkmap_update(&g->map, centres, ncentres);
    if (g->opt.bot) {
        /* playtest: as fast as the world loads, drawing now and then */
        static int frames;
        static bool started;
        if (!started) { bot_init(g, g->opt.bot, g->opt.bot_log, g->opt.bot_minutes); started = true; }
        u64 t0 = SDL_GetTicksNS();
        while (SDL_GetTicksNS() - t0 < 30000000ull && chunkmap_settled(&g->map, player_eye(&g->players[0].pl))) {
            if (!bot_step(g, &in, DT)) { a->running = false; break; }
            game_update(g, &in, DT);
        }
        if (++frames % 20 == 0) {
            for (int i = 0; i < MAX_PLAYERS; i++)
                if (g->players[i].active) game_update_atmosphere(g, &g->players[i], frame_dt);
            game_render(g);
            SDL_GL_SwapWindow(a->win);
        }
        return;
    }
    a->acc += frame_dt;
    while (a->acc >= DT) {
        game_update(g, &in, DT);
        a->acc -= DT;
    }
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) game_update_atmosphere(g, &g->players[i], frame_dt);
    sfx_update(g, frame_dt);
    music_update(g, frame_dt, false);
    game_render(g);
    if (g->opt.shot) screenshot_step(a, frame_dt);
    SDL_GL_SwapWindow(a->win);
#ifdef __EMSCRIPTEN__
    if (!a->running) emscripten_cancel_main_loop();
#endif
}

#ifdef __EMSCRIPTEN__
/* The browser may close or freeze the tab at any time: save when it is hidden or left. */
static App *g_app;
static EM_BOOL on_visibility(int type, const EmscriptenVisibilityChangeEvent *e, void *user) {
    if (e->hidden && g_app && !g_app->in_menu && !g_app->g->opt.shot) game_save(g_app->g);
    return 0;
}
static const char *on_unload(int type, const void *reserved, void *user) {
    if (g_app && !g_app->in_menu && !g_app->g->opt.shot) game_save(g_app->g);
    return NULL;
}
#endif

int main(int argc, char **argv) {
    static App app;
    App *a = &app;
    Game *g = calloc(1, sizeof(Game));
    a->g = g;
    if (!parse_args(&g->opt, argc, argv)) return 1;
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0"); /* touches drive the on-screen controls */
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_WARP_MOTION, "1"); /* lets us detect broken relative mode */
    bool recenter = g->opt.recenter ? g->opt.recenter > 0 : !running_under_wsl();
    if (!recenter) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, "0");
        SDL_Log("Mouse: not re-centring the cursor (WSL detected or --no-recenter).");
    }
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
#ifdef __EMSCRIPTEN__
    /* WebGL 2 = OpenGL ES 3.0 */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    a->win = SDL_CreateWindow("Fortress Crawl", g->opt.win_w, g->opt.win_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!a->win) {
        SDL_Log("window failed: %s", SDL_GetError());
        return 1;
    }
    a->ctx = SDL_GL_CreateContext(a->win);
    if (!a->ctx || !gl_load()) {
        SDL_Log("OpenGL context failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(g->opt.shot ? 0 : 1);
    SDL_Log("GL %s / %s", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));

#ifdef __EMSCRIPTEN__
    jobs_init(0); /* no threads in the browser build: jobs run in time slices on the main thread */
#else
    int cores = SDL_GetNumLogicalCPUCores();
    jobs_init(cores > 2 ? cores - 1 : 2);
#endif
    g->r.set.low_w = g->opt.low_w;
    g->r.set.low_h = g->opt.low_h;
    g->r.set.dither = true;
    g->r.set.torch = true;
    g->r.set.fullbright = g->opt.fullbright;
    g->r.set.fov_deg = 72.0f;
    g->res_mode = 1;
    int ww, wh;
    SDL_GetWindowSizeInPixels(a->win, &ww, &wh);
    if (!renderer_init(&g->r, ww, wh)) return 1;
    font_init();
    batch_init();
    sfx_init(g);
    g->touch.active = g->touch.forced = g->opt.touch;
    /* the start screen picks the seed, unless it was given (or this is a screenshot run) */
    bool menu_shot = g->opt.shot && g_demo_kind && !strcmp(g_demo_kind, "menu");
    /* screenshot runs pick a motive without asking, except the one that shows the question */
    g->opt.auto_motive = g->opt.shot && !(g_demo_kind && !strcmp(g_demo_kind, "motive"));
    if ((!g->opt.has_seed && !g->opt.shot) || menu_shot) {
        menu_init(&a->menu);
        a->in_menu = true;
    } else {
        start_world(a);
    }
    if (g->opt.test_pad) {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "Fortress Crawl test pad";
        SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
        a->vpad = id ? SDL_OpenJoystick(id) : NULL;
        if (!a->vpad) SDL_Log("virtual gamepad failed: %s", SDL_GetError());
    }

    a->running = true;
    a->last = SDL_GetTicksNS();
#ifdef __EMSCRIPTEN__
    g_app = a;
    emscripten_set_visibilitychange_callback(NULL, 0, on_visibility);
    emscripten_set_beforeunload_callback(NULL, on_unload);
    emscripten_set_main_loop_arg(frame, a, 0, true);
#else
    while (a->running) frame(a);
    if (!a->in_menu && (!g->opt.shot || g->opt.persist)) {
        bool ok = game_save(g);
        SDL_Log("%s the world %s", ok ? "Saved" : "Could not save", g->opt.seed_text);
    }
    if (!a->in_menu) chunkmap_shutdown(&g->map);
    jobs_shutdown();
    sfx_shutdown();
    SDL_GL_DestroyContext(a->ctx);
    SDL_DestroyWindow(a->win);
    SDL_Quit();
    free(g);
#endif
    return 0;
}
