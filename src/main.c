/* Fortress Crawl - prototype focused on world generation and lighting.
 *
 * Controls: WASD move, mouse look, Space jump, Shift sprint, Ctrl crouch,
 * F fly/noclip, T teleport ahead (fly), PgUp/PgDn one chunk up/down,
 * F1 debug overlay, F2 resolution, F3 dither, F4 vertex snap, F5 fullbright,
 * F6 torch, F12 screenshot, Esc release mouse (again to quit). */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "core/fmath.h"
#include "engine/font.h"
#include "engine/gl.h"
#include "engine/jobs.h"
#include "game/player.h"
#include "render/renderer.h"
#include "world/chunkmap.h"
#include "world/layout.h"

typedef struct Options {
    u64 seed;
    bool has_pos;
    vec3 pos;
    float yaw, pitch;
    bool fly;
    int radius, vradius;
    const char *shot;
    int win_w, win_h;
    int low_w, low_h;
    bool hud;
} Options;

typedef struct Game {
    SDL_Window *win;
    Renderer r;
    ChunkMap map;
    World world;
    Player pl;
    Options opt;
    bool hud;
    bool mouse;
    float time;
    /* smoothed atmosphere */
    vec3 fog, ambient;
    float fog_density, stars, openness;
    float openness_timer;
    char place[128];
    float banner_time;
    char banner[128];
    float fps;
    int res_mode;
} Game;

static const int RES_MODES[3][2] = { { 320, 180 }, { 480, 270 }, { 640, 360 } };

static int solid_at(void *user, int x, int y, int z) {
    Game *g = user;
    bool known;
    Block b = chunkmap_block(&g->map, x, y, z, &known);
    if (!known) return -1;
    return block_solid(b) ? 1 : 0;
}

static bool parse_args(Options *o, int argc, char **argv) {
    memset(o, 0, sizeof *o);
    o->seed = 1337;
    o->radius = 3;
    o->vradius = 2;
    o->win_w = 1280;
    o->win_h = 720;
    o->low_w = 480;
    o->low_h = 270;
    o->hud = true;
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
        else if (!strcmp(a, "--nohud")) o->hud = false;
        else {
            SDL_Log("usage: fortress_crawl [--seed N] [--pos x y z] [--look yaw pitch] [--fly] [--radius n] [--vradius n]\n"
                    "                      [--window w h] [--lowres w h] [--shot file.bmp] [--nohud]");
            return false;
        }
    }
    return true;
}

/* Average free distance around the eye: big halls get thin fog, tunnels thick. */
static float measure_openness(Game *g, vec3 eye) {
    static const float DIRS[14][3] = {
        { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0.7f, 0, 0.7f }, { -0.7f, 0, 0.7f }, { 0.7f, 0, -0.7f },
        { -0.7f, 0, -0.7f }, { 0, 1, 0 }, { 0, -1, 0 }, { 0.6f, 0.6f, 0 }, { -0.6f, 0.6f, 0 }, { 0, 0.6f, 0.6f }, { 0, 0.6f, -0.6f },
    };
    float sum = 0;
    for (int i = 0; i < 14; i++) {
        float d = 0;
        for (; d < 96.0f; d += 1.0f) {
            vec3 p = v3_add(eye, v3_scale(v3(DIRS[i][0], DIRS[i][1], DIRS[i][2]), d));
            bool known;
            Block b = chunkmap_block(&g->map, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z), &known);
            if (!known || block_opaque(b)) break;
        }
        sum += d;
    }
    return sum / 14.0f;
}

static void update_atmosphere(Game *g, float dt, vec3 eye) {
    int cx = floordiv((int)floorf(eye.x), CHUNK), cy = floordiv((int)floorf(eye.y), CHUNK), cz = floordiv((int)floorf(eye.z), CHUNK);
    Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
    ChunkInfo ci;
    if (c && c->state == CH_READY) ci = c->info;
    else world_chunk_info(&g->world, cx, cy, cz, &ci);
    ThemeInfo ti;
    theme_info(ci.theme, &ti);
    vec3 fog = v3(ti.fog[0], ti.fog[1], ti.fog[2]);
    vec3 amb = v3(ti.ambient[0], ti.ambient[1], ti.ambient[2]);
    float stars = ti.sky ? 0.6f : 0.0f;
    float density = ti.fog_density;
    if (ci.arch == ARCH_EXTERIOR) {
        fog = v3(0.045f, 0.05f, 0.075f);
        amb = v3(0.05f, 0.055f, 0.08f);
        stars = 1.0f;
        density = 0.012f;
    }
    g->openness_timer -= dt;
    if (g->openness_timer <= 0) {
        g->openness = measure_openness(g, eye);
        g->openness_timer = 0.2f;
    }
    float open_t = clampf((g->openness - 4.0f) / 40.0f, 0.0f, 1.0f);
    density *= lerpf(1.5f, 0.45f, open_t);
    float k = 1.0f - expf(-dt * 1.5f);
    g->fog = v3_lerp(g->fog, fog, k);
    g->ambient = v3_lerp(g->ambient, amb, k);
    g->fog_density = lerpf(g->fog_density, density, k);
    g->stars = lerpf(g->stars, stars, k);

    char name[128];
    world_place_name(&ci, name, sizeof name);
    if (strcmp(name, g->place) != 0) {
        snprintf(g->place, sizeof g->place, "%s", name);
        snprintf(g->banner, sizeof g->banner, "%s", name);
        g->banner_time = 3.5f;
    }
}

static void draw_hud(Game *g, vec3 eye) {
    Renderer *r = &g->r;
    font_begin(r->fbo_w, r->fbo_h);
    /* crosshair */
    font_text(r->fbo_w / 2 - 3, r->fbo_h / 2 - 4, "+", 0.8f, 0.75f, 0.6f);
    if (g->banner_time > 0) {
        float a = clampf(g->banner_time, 0, 1);
        int w = font_text_width(g->banner);
        font_text((r->fbo_w - w) / 2, r->fbo_h / 4, g->banner, 0.9f * a, 0.8f * a, 0.55f * a);
    }
    if (g->hud) {
        char buf[256];
        int y = 4;
        int cx = floordiv((int)floorf(eye.x), CHUNK), cy = floordiv((int)floorf(eye.y), CHUNK), cz = floordiv((int)floorf(eye.z), CHUNK);
        Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
        snprintf(buf, sizeof buf, "FORTRESS CRAWL  seed %llu  %.0f fps", (unsigned long long)g->world.seed, g->fps);
        font_text(4, y, buf, 0.85f, 0.7f, 0.45f); y += FONT_LINE;
        font_text(4, y, g->place, 0.9f, 0.85f, 0.75f); y += FONT_LINE;
        snprintf(buf, sizeof buf, "pos %.1f %.1f %.1f  chunk %d %d %d", eye.x, eye.y, eye.z, cx, cy, cz);
        font_text(4, y, buf, 0.7f, 0.7f, 0.7f); y += FONT_LINE;
        if (c && c->state == CH_READY) {
            snprintf(buf, sizeof buf, "danger level %d  %s / %s", c->info.level, STRATA[c->info.stratum].name, FLAVORS[c->info.flavor].name);
            font_text(4, y, buf, 0.7f, 0.7f, 0.7f); y += FONT_LINE;
        }
        snprintf(buf, sizeof buf, "chunks %d loaded %d meshed %d drawn  jobs %d", g->map.stats.loaded, g->map.stats.meshed,
                 g->map.stats.drawn, g->map.stats.jobs);
        font_text(4, y, buf, 0.6f, 0.6f, 0.6f); y += FONT_LINE;
        static const char *STANCES[3] = { "standing", "crouching", "crawling" };
        snprintf(buf, sizeof buf, "%s  %s  open %.0fm  %dx%d%s%s", g->pl.fly ? "FLY" : "WALK", STANCES[g->pl.stance], g->openness,
                 r->fbo_w, r->fbo_h, r->set.dither ? " dither" : "", r->set.vertex_snap ? " snap" : "");
        font_text(4, y, buf, 0.6f, 0.6f, 0.6f);
        font_text(4, r->fbo_h - 12, "WASD SPACE SHIFT CTRL  F FLY  T JUMP AHEAD  PGUP/DN  F1-F6 F12", 0.5f, 0.45f, 0.4f);
    }
    font_end();
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

static void render_frame(Game *g) {
    vec3 eye = player_eye(&g->pl);
    SceneParams sp;
    float aspect = (float)g->r.fbo_w / (float)g->r.fbo_h;
    sp.proj = m4_perspective(g->r.set.fov_deg * PI_F / 180.0f, aspect, 0.05f, 400.0f);
    sp.view = m4_look_dir(eye, dir_from_angles(g->pl.yaw, g->pl.pitch), v3(0, 1, 0));
    sp.cam = eye;
    sp.fog = g->fog;
    sp.fog_density = g->fog_density;
    sp.ambient = g->ambient;
    sp.stars = g->stars;
    sp.time = g->time;
    renderer_begin(&g->r, &sp);
    chunkmap_draw(&g->map);
    renderer_end_world(&g->r);
    draw_hud(g, eye);
    renderer_present(&g->r, g->time);
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
    g->win = SDL_CreateWindow("Fortress Crawl", g->opt.win_w, g->opt.win_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!g->win) {
        SDL_Log("window failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(g->win);
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
    SDL_GetWindowSizeInPixels(g->win, &ww, &wh);
    if (!renderer_init(&g->r, ww, wh)) return 1;
    font_init();
    chunkmap_init(&g->map, &g->world, &g->r, g->opt.radius, g->opt.vradius);
    g->hud = g->opt.hud;

    vec3 start = g->opt.has_pos ? g->opt.pos : v3(-70.5f, 6.0f, 0.5f);
    player_init(&g->pl, start, g->opt.yaw);
    g->pl.pitch = g->opt.pitch;
    g->pl.fly = g->opt.fly;
    g->fog = v3(0.04f, 0.05f, 0.07f);
    g->fog_density = 0.02f;

    if (!g->opt.shot) {
        SDL_SetWindowRelativeMouseMode(g->win, true);
        g->mouse = true;
    }

    u64 last = SDL_GetTicksNS();
    float acc = 0, fps_acc = 0;
    int fps_frames = 0;
    bool running = true;
    float shot_wait = 0;
    const float DT = 1.0f / 60.0f;
    while (running) {
        u64 now = SDL_GetTicksNS();
        float frame_dt = (float)((now - last) / 1e9);
        last = now;
        if (frame_dt > 0.25f) frame_dt = 0.25f;
        fps_acc += frame_dt;
        fps_frames++;
        if (fps_acc > 0.5f) { g->fps = fps_frames / fps_acc; fps_acc = 0; fps_frames = 0; }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) renderer_resize(&g->r, e.window.data1, e.window.data2);
            else if (e.type == SDL_EVENT_MOUSE_MOTION && g->mouse) {
                g->pl.yaw += e.motion.xrel * 0.0025f;
                g->pl.pitch = clampf(g->pl.pitch - e.motion.yrel * 0.0025f, -1.55f, 1.55f);
            } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !g->mouse) {
                SDL_SetWindowRelativeMouseMode(g->win, true);
                g->mouse = true;
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                switch (e.key.key) {
                case SDLK_ESCAPE:
                    if (g->mouse) { SDL_SetWindowRelativeMouseMode(g->win, false); g->mouse = false; }
                    else running = false;
                    break;
                case SDLK_F1: g->hud = !g->hud; break;
                case SDLK_F2:
                    g->res_mode = (g->res_mode + 1) % 3;
                    g->r.set.low_w = RES_MODES[g->res_mode][0];
                    g->r.set.low_h = RES_MODES[g->res_mode][1];
                    break;
                case SDLK_F3: g->r.set.dither = !g->r.set.dither; break;
                case SDLK_F4: g->r.set.vertex_snap = !g->r.set.vertex_snap; break;
                case SDLK_F5: g->r.set.fullbright = !g->r.set.fullbright; break;
                case SDLK_F6: g->r.set.torch = !g->r.set.torch; break;
                case SDLK_F12: save_screenshot(g, "screenshot.bmp"); break;
                case SDLK_F: g->pl.fly = !g->pl.fly; g->pl.vel = v3(0, 0, 0); break;
                case SDLK_T: {
                    vec3 d = dir_from_angles(g->pl.yaw, g->pl.pitch);
                    g->pl.pos = v3_add(g->pl.pos, v3_scale(d, 8.0f * CHUNK));
                    g->pl.fly = true;
                    break;
                }
                case SDLK_PAGEUP: g->pl.pos.y += CHUNK; g->pl.fly = true; break;
                case SDLK_PAGEDOWN: g->pl.pos.y -= CHUNK; g->pl.fly = true; break;
                default: break;
                }
            }
        }

        const bool *keys = SDL_GetKeyboardState(NULL);
        PlayerInput in = { 0 };
        if (g->mouse || g->opt.shot) {
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
        bool ground_known = solid_at(g, (int)floorf(g->pl.pos.x), (int)floorf(g->pl.pos.y) - 1, (int)floorf(g->pl.pos.z)) >= 0;
        acc += frame_dt;
        while (acc >= DT) {
            if (ground_known || g->pl.fly) player_update(&g->pl, &in, DT, solid_at, g);
            acc -= DT;
            g->time += DT;
        }
        g->banner_time -= frame_dt;
        update_atmosphere(g, frame_dt, player_eye(&g->pl));

        render_frame(g);
        if (g->opt.shot) {
            if (chunkmap_settled(&g->map, player_eye(&g->pl))) shot_wait += frame_dt;
            if (shot_wait > 1.0f || g->time > 120.0f) {
                /* atmosphere snaps to its target for a deterministic screenshot */
                g->openness = measure_openness(g, player_eye(&g->pl));
                for (int i = 0; i < 60; i++) update_atmosphere(g, 0.5f, player_eye(&g->pl));
                g->banner_time = 0;
                render_frame(g);
                save_screenshot(g, g->opt.shot);
                running = false;
            }
        }
        SDL_GL_SwapWindow(g->win);
    }

    chunkmap_shutdown(&g->map);
    jobs_shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(g->win);
    SDL_Quit();
    free(g);
    return 0;
}
