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
#endif

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
    o->sens = 1.0f;
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
        else {
            SDL_Log("usage: fortress_crawl [--seed N] [--pos x y z] [--look yaw pitch] [--fly] [--radius n] [--vradius n]\n"
                    "                      [--window w h] [--lowres w h] [--debug] [--invert-x] [--invert-y] [--sens s]\n"
                    "                      [--recenter | --no-recenter]\n"
                    "                      [--shot file.bmp] [--shot-time s] [--demo combat|sword|death|skills|map|coop]\n"
                    "                      [--players n] [--test-pad]");
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
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !g->mouse) {
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
    if (g->opt.demo && !a->demo_done && a->shot_wait > 0.5f) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            PlayerSlot *ps = &g->players[i];
            if (!ps->active) continue;
            ps->openness = game_measure_openness(g, player_eye(&ps->pl));
            for (int k = 0; k < 60; k++) game_update_atmosphere(g, ps, 0.5f);
        }
        demo_setup(g);
        a->demo_done = true;
    }
    const char *kind = g_demo_kind ? g_demo_kind : "";
    bool fight = !strcmp(kind, "combat") || !strcmp(kind, "sword") || !strcmp(kind, "death") || !strcmp(kind, "coop");
    if (g->opt.demo && a->demo_done && fight) {
        p1->attack_held = a->shot_wait > 0.9f && a->shot_wait < 0.95f;
        p1->alt_held = false;
    }
    float capture = g->opt.shot_time > 0 ? g->opt.shot_time : 1.0f;
    if (g->opt.demo && fight && g->opt.shot_time <= 0) capture = !strcmp(kind, "sword") ? 1.0f : 1.08f;
    if (a->shot_wait > capture || (g->time > 120.0f && g->opt.shot_time <= 0)) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            PlayerSlot *ps = &g->players[i];
            if (!ps->active) continue;
            if (!g->opt.demo) {
                ps->openness = game_measure_openness(g, player_eye(&ps->pl));
                for (int k = 0; k < 60; k++) game_update_atmosphere(g, ps, 0.5f);
                ps->banner_time = 0;
            }
            SDL_Log("P%d time %.1f  hp %.0f/%.0f  level %d  kills %d  dead %d  pad %d  pos %.1f %.1f %.1f  yaw %.1f pitch %.1f",
                    i + 1, g->time, ps->st.hp, ps->st.maxhp, ps->st.level, ps->st.kills, ps->st.dead, ps->pad != NULL,
                    ps->pl.pos.x, ps->pl.pos.y, ps->pl.pos.z, ps->pl.yaw * 180.0f / PI_F, ps->pl.pitch * 180.0f / PI_F);
        }
        game_render(g);
        save_screenshot(g, g->opt.shot);
        a->running = false;
    }
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
    a->acc += frame_dt;
    while (a->acc >= DT) {
        game_update(g, &in, DT);
        a->acc -= DT;
    }
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) game_update_atmosphere(g, &g->players[i], frame_dt);
    game_render(g);
    if (g->opt.shot) screenshot_step(a, frame_dt);
    SDL_GL_SwapWindow(a->win);
#ifdef __EMSCRIPTEN__
    if (!a->running) emscripten_cancel_main_loop();
#endif
}

int main(int argc, char **argv) {
    static App app;
    App *a = &app;
    Game *g = calloc(1, sizeof(Game));
    a->g = g;
    if (!parse_args(&g->opt, argc, argv)) return 1;
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
    world_init(&g->world, g->opt.seed);
    g->r.set.low_w = g->opt.low_w;
    g->r.set.low_h = g->opt.low_h;
    g->r.set.dither = true;
    g->r.set.torch = true;
    g->r.set.fov_deg = 72.0f;
    g->res_mode = 1;
    int ww, wh;
    SDL_GetWindowSizeInPixels(a->win, &ww, &wh);
    if (!renderer_init(&g->r, ww, wh)) return 1;
    font_init();
    chunkmap_init(&g->map, &g->world, &g->r, g->opt.radius, g->opt.vradius);

    vec3 start = g->opt.has_pos ? g->opt.pos : v3(-70.5f, 6.0f, 0.5f);
    game_init(g, start, g->opt.yaw);
    g->players[0].pl.pitch = g->opt.pitch;
    g->players[0].pl.fly = g->opt.fly;
    /* --players n: extra characters without a device yet (a pad takes one over with START) */
    for (int i = 1; i < g->opt.players && i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = game_add_player(g);
        if (ps) { ps->pl.pitch = g->opt.pitch; ps->pl.fly = g->opt.fly; }
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

#ifndef __EMSCRIPTEN__
    /* in the browser, pointer lock needs a click first */
    if (!g->opt.shot || g->opt.grab) mouse_capture(a, true);
#endif

    a->running = true;
    a->last = SDL_GetTicksNS();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(frame, a, 0, true);
#else
    while (a->running) frame(a);
    chunkmap_shutdown(&g->map);
    jobs_shutdown();
    SDL_GL_DestroyContext(a->ctx);
    SDL_DestroyWindow(a->win);
    SDL_Quit();
    free(g);
#endif
    return 0;
}
