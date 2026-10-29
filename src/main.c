/* Fortress Crawl - platform layer: window, GL context, main loop.
 *
 * Controls: WASD move, mouse look, LMB attack, RMB alternate attack, 1-3 / wheel weapon,
 * Space jump, Shift sprint, Ctrl crouch, TAB skills, M map, F fly, T teleport ahead,
 * PgUp/PgDn one chunk up/down, F1 debug overlay, F2 resolution, F3 dither,
 * F4 vertex snap, F5 fullbright, F6 torch, F12 screenshot, Esc release mouse (again: quit). */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "engine/font.h"
#include "engine/gl.h"
#include "engine/jobs.h"
#include "game/game.h"

static const char *g_demo_kind;

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
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--seed") && i + 1 < argc) o->seed = strtoull(argv[++i], NULL, 10);
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
        else if (!strcmp(a, "--shot-time") && i + 1 < argc) o->shot_time = strtof(argv[++i], NULL);
        else if (!strcmp(a, "--demo") && i + 1 < argc) { o->demo = true; g_demo_kind = argv[++i]; }
        else {
            SDL_Log("usage: fortress_crawl [--seed N] [--pos x y z] [--look yaw pitch] [--fly] [--radius n] [--vradius n]\n"
                    "                      [--window w h] [--lowres w h] [--debug] [--shot file.bmp [--demo combat|map|skills]]");
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

/* Scripted setups for screenshots of the combat systems. */
static void demo_setup(Game *g) {
    Stats *s = &g->st;
    game_add_xp(g, 1500.0f);
    s->ranks[SK_KEEN_EDGE] = 2; s->ranks[SK_CLEAVE] = 1;
    s->ranks[SK_FOCUS] = 2; s->ranks[SK_FIREBALL] = 1;
    s->ranks[SK_TOUGHNESS] = 1;
    s->points -= 7;
    if (s->points < 0) s->points = 1;
    stats_refresh(g);
    s->hp = s->maxhp * 0.8f;
    s->mana = s->maxmana;
    g->banner_time = 0;
    if (!strcmp(g_demo_kind, "skills")) { g->show_skills = true; g->skill_sel = skill_id(BR_ARCANE, 2); return; }
    if (!strcmp(g_demo_kind, "map")) { g->show_map = true; return; }
    /* combat: a pack in front of the player */
    for (int i = 0; i < MAX_MONSTERS; i++) g->mons[i].alive = false;
    vec3 eye = player_eye(&g->pl);
    static const struct { int type; float dist, side; bool elite; } PACK[] = {
        { MON_CRAWLER, 8.0f, -1.5f, false }, { MON_CRAWLER, 9.0f, 1.2f, false }, { MON_HORNED, 11.0f, 2.6f, false },
        { MON_WISP, 10.0f, -3.0f, false }, { MON_BRUTE, 14.0f, 0.3f, true },
    };
    vec3 f = v3(cosf(g->pl.yaw), 0, sinf(g->pl.yaw)), r = v3(-sinf(g->pl.yaw), 0, cosf(g->pl.yaw));
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
        if (g->mons[i].alive) g->mons[i].yaw = g->pl.yaw + PI_F;
    s->weapon = !strcmp(g_demo_kind, "sword") ? WPN_SWORD : WPN_STAFF;
    if (!strcmp(g_demo_kind, "death")) s->hp = 1.0f; /* exercises death and shrine respawn */
}

int main(int argc, char **argv) {
    Game *g = calloc(1, sizeof(Game));
    if (!parse_args(&g->opt, argc, argv)) return 1;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window *win = SDL_CreateWindow("Fortress Crawl", g->opt.win_w, g->opt.win_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!win) {
        SDL_Log("window failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx || !gl_load()) {
        SDL_Log("OpenGL 3.3 context failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(g->opt.shot ? 0 : 1);
    SDL_Log("GL %s / %s", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));

    int cores = SDL_GetNumLogicalCPUCores();
    jobs_init(cores > 2 ? cores - 1 : 2);
    world_init(&g->world, g->opt.seed);
    g->r.set.low_w = g->opt.low_w;
    g->r.set.low_h = g->opt.low_h;
    g->r.set.dither = true;
    g->r.set.torch = true;
    g->r.set.fov_deg = 72.0f;
    g->res_mode = 1;
    int ww, wh;
    SDL_GetWindowSizeInPixels(win, &ww, &wh);
    if (!renderer_init(&g->r, ww, wh)) return 1;
    font_init();
    chunkmap_init(&g->map, &g->world, &g->r, g->opt.radius, g->opt.vradius);

    vec3 start = g->opt.has_pos ? g->opt.pos : v3(-70.5f, 6.0f, 0.5f);
    player_init(&g->pl, start, g->opt.yaw);
    g->pl.pitch = g->opt.pitch;
    g->pl.fly = g->opt.fly;
    g->fog = v3(0.04f, 0.05f, 0.07f);
    g->fog_density = 0.02f;
    game_init(g);

    if (!g->opt.shot) {
        SDL_SetWindowRelativeMouseMode(win, true);
        g->mouse = true;
    }

    u64 last = SDL_GetTicksNS();
    float acc = 0, fps_acc = 0, shot_wait = 0;
    int fps_frames = 0;
    bool running = true, demo_done = false;
    const float DT = 1.0f / 60.0f;
    while (running) {
        u64 now = SDL_GetTicksNS();
        float frame_dt = (float)((now - last) / 1e9);
        last = now;
        if (frame_dt > 0.25f) frame_dt = 0.25f;
        if (g->opt.shot) frame_dt = DT; /* deterministic stepping for screenshots */
        fps_acc += frame_dt;
        fps_frames++;
        if (fps_acc > 0.5f) { g->fps = fps_frames / fps_acc; fps_acc = 0; fps_frames = 0; }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) renderer_resize(&g->r, e.window.data1, e.window.data2);
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !g->mouse) {
                SDL_SetWindowRelativeMouseMode(win, true);
                g->mouse = true;
            } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE && !e.key.repeat) {
                if (g->show_skills || g->show_map) { g->show_skills = g->show_map = false; }
                else if (g->mouse) { SDL_SetWindowRelativeMouseMode(win, false); g->mouse = false; g->attack_held = g->alt_held = false; }
                else running = false;
            } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F12 && !e.key.repeat) {
                save_screenshot(g, "screenshot.bmp");
            } else {
                game_event(g, &e);
            }
        }

        const bool *keys = SDL_GetKeyboardState(NULL);
        PlayerInput in = { 0 };
        if (g->mouse && !g->show_skills && !g->show_map) {
            in.move_z = (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
            in.move_x = (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
            in.jump = keys[SDL_SCANCODE_SPACE];
            in.up = keys[SDL_SCANCODE_SPACE];
            in.crouch = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_C];
            in.down = in.crouch;
            in.sprint = keys[SDL_SCANCODE_LSHIFT];
        }
        vec3 eye = player_eye(&g->pl);
        chunkmap_update(&g->map, eye);
        /* do not simulate until the ground under the player exists */
        bool ground_known = game_solid(g, (int)floorf(g->pl.pos.x), (int)floorf(g->pl.pos.y) - 1, (int)floorf(g->pl.pos.z)) >= 0;
        acc += frame_dt;
        while (acc >= DT) {
            if (ground_known || g->pl.fly) game_update(g, &in, DT);
            acc -= DT;
        }
        g->banner_time -= frame_dt;
        game_update_atmosphere(g, frame_dt, player_eye(&g->pl));
        game_render(g);

        if (g->opt.shot) {
            if (chunkmap_settled(&g->map, player_eye(&g->pl))) shot_wait += frame_dt;
            if (g->opt.demo && !demo_done && shot_wait > 0.5f) {
                g->openness = game_measure_openness(g, player_eye(&g->pl));
                for (int i = 0; i < 60; i++) game_update_atmosphere(g, 0.5f, player_eye(&g->pl));
                demo_setup(g);
                demo_done = true;
            }
            bool fight = !strcmp(g_demo_kind ? g_demo_kind : "", "combat") || !strcmp(g_demo_kind ? g_demo_kind : "", "sword") ||
                         !strcmp(g_demo_kind ? g_demo_kind : "", "death");
            if (g->opt.demo && demo_done && fight) {
                g->attack_held = shot_wait > 0.9f && shot_wait < 0.95f;
                g->alt_held = false;
            }
            float capture = g->opt.shot_time > 0 ? g->opt.shot_time : 1.0f;
            if (g->opt.demo && fight && g->opt.shot_time <= 0) capture = !strcmp(g_demo_kind, "sword") ? 1.0f : 1.08f;
            bool ready = shot_wait > capture;
            if (ready || (g->time > 120.0f && g->opt.shot_time <= 0)) {
                if (!g->opt.demo) {
                    g->openness = game_measure_openness(g, player_eye(&g->pl));
                    for (int i = 0; i < 60; i++) game_update_atmosphere(g, 0.5f, player_eye(&g->pl));
                    g->banner_time = 0;
                }
                game_render(g);
                SDL_Log("time %.1f  hp %.0f/%.0f  level %d  kills %d  dead %d  pos %.1f %.1f %.1f", g->time, g->st.hp, g->st.maxhp,
                        g->st.level, g->st.kills, g->st.dead, g->pl.pos.x, g->pl.pos.y, g->pl.pos.z);
                save_screenshot(g, g->opt.shot);
                running = false;
            }
        }
        SDL_GL_SwapWindow(win);
    }

    chunkmap_shutdown(&g->map);
    jobs_shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    free(g);
    return 0;
}
