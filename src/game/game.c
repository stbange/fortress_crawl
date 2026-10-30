#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

static const int RES_MODES[3][2] = { { 320, 180 }, { 480, 270 }, { 640, 360 } };
static const vec3 PLAYER_COLORS[MAX_PLAYERS] = {
    { 0.95f, 0.75f, 0.3f }, { 0.3f, 0.8f, 0.95f }, { 0.45f, 0.95f, 0.4f }, { 0.95f, 0.4f, 0.85f },
};

int game_solid(void *user, int x, int y, int z) {
    Game *g = user;
    bool known;
    Block b = chunkmap_block(&g->map, x, y, z, &known);
    if (!known) return -1;
    return block_solid(b) ? 1 : 0;
}

void game_banner(PlayerSlot *ps, const char *text) {
    snprintf(ps->banner, sizeof ps->banner, "%s", text);
    ps->banner_time = 3.0f;
}

void game_banner_all(Game *g, const char *text) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) game_banner(&g->players[i], text);
}

int game_centres(Game *g, vec3 *out) {
    int n = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) out[n++] = player_eye(&g->players[i].pl);
    return n;
}

PlayerSlot *game_nearest_player(Game *g, vec3 p, float max_dist) {
    PlayerSlot *best = NULL;
    float bd = max_dist;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->st.dead) continue;
        float d = v3_len(v3_sub(player_eye(&ps->pl), p));
        if (d < bd) { bd = d; best = ps; }
    }
    return best;
}

static void stats_init(PlayerSlot *ps) {
    Stats *s = &ps->st;
    memset(s, 0, sizeof *s);
    s->level = 1;
    s->xp_next = 60.0f;
    s->weapon = WPN_SWORD;
    stats_refresh(ps);
    s->hp = s->maxhp;
    s->mana = s->maxmana;
    s->respawn = ps->pl.pos;
    s->respawn_yaw = ps->pl.yaw;
    s->since_hurt = 100.0f;
}

void game_init(Game *g, vec3 start, float yaw) {
    g->start_pos = start;
    g->start_yaw = yaw;
    PlayerSlot *p1 = game_add_player(g);
    p1->keyboard = true;
    g->hud = g->opt.hud;
    batch_init();
}

/* A new character joins next to the first living player (or at the start). */
PlayerSlot *game_add_player(Game *g) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (ps->active) continue;
        memset(ps, 0, sizeof *ps);
        ps->active = true;
        ps->index = i;
        ps->color = PLAYER_COLORS[i];
        vec3 pos = g->start_pos;
        float yaw = g->start_yaw;
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *o = &g->players[k];
            if (k == i || !o->active || o->st.dead) continue;
            pos = o->pl.pos;
            yaw = o->pl.yaw;
            /* step to the side if there is room */
            vec3 side = v3(-sinf(yaw) * 1.2f, 0, cosf(yaw) * 1.2f);
            vec3 p = v3_add(pos, side);
            if (phys_box_free(p, PLAYER_HALF_W, player_height(STANCE_STAND), game_solid, g)) pos = p;
            break;
        }
        player_init(&ps->pl, pos, yaw);
        stats_init(ps);
        ps->fog = v3(0.04f, 0.05f, 0.07f);
        ps->fog_density = 0.02f;
        g->nplayers++;
        return ps;
    }
    return NULL;
}

void game_add_xp(Game *g, PlayerSlot *ps, float xp) {
    Stats *s = &ps->st;
    s->xp += xp;
    while (s->xp >= s->xp_next) {
        s->xp -= s->xp_next;
        s->level++;
        s->points++;
        s->xp_next = 60.0f * powf(1.4f, (float)(s->level - 1));
        stats_refresh(ps);
        s->hp = s->maxhp;
        s->mana = s->maxmana;
        char buf[96];
        snprintf(buf, sizeof buf, "LEVEL %d!  +1 SKILL POINT (%s)", s->level, ps->keyboard ? "TAB" : "Y");
        game_banner(ps, buf);
        flash_add(g, player_eye(&ps->pl), v3(1.2f, 1.0f, 0.6f), 10.0f, 0.6f);
    }
}

void game_hurt_player(Game *g, PlayerSlot *ps, float dmg, vec3 from) {
    Stats *s = &ps->st;
    if (s->dead || ps->pl.fly || s->invuln > 0) return;
    if (s->ranks[SK_LAST_STAND] && s->hp < s->maxhp * 0.3f) dmg *= 0.5f;
    s->hp -= dmg;
    s->hurt = fminf(1.0f, s->hurt + 0.35f + dmg / s->maxhp);
    s->since_hurt = 0;
    vec3 push = v3_sub(ps->pl.pos, from);
    push.y = 0;
    push = v3_norm(push);
    ps->pl.vel = v3_add(ps->pl.vel, v3_scale(push, 4.0f));
    if (ps->pad) SDL_RumbleGamepad(ps->pad, 0x6000, 0x9000, 180);
    if (s->hp <= 0) {
        s->hp = 0;
        s->dead = true;
        s->dead_t = 0;
        game_banner(ps, "YOU HAVE FALLEN");
    }
}

vec3 game_light_at(Game *g, vec3 p) {
    u8 l[4];
    vec3 out = g->players[0].ambient;
    if (chunkmap_light(&g->map, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z), l)) {
        out.x = fmaxf(out.x, l[0] ? powf(0.85f, 15.0f - l[0]) : 0.0f);
        out.y = fmaxf(out.y, l[1] ? powf(0.85f, 15.0f - l[1]) : 0.0f);
        out.z = fmaxf(out.z, l[2] ? powf(0.85f, 15.0f - l[2]) : 0.0f);
    }
    if (g->r.set.torch)
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!g->players[i].active) continue;
            float d = v3_len(v3_sub(p, player_eye(&g->players[i].pl)));
            float a = clampf(1.0f - d / 11.0f, 0.0f, 1.0f);
            a *= a;
            out = v3_add(out, v3(1.05f * a, 0.72f * a, 0.42f * a));
        }
    for (int i = 0; i < MAX_FLASHES; i++) {
        Flash *f = &g->flashes[i];
        if (f->life <= 0) continue;
        float d = v3_len(v3_sub(p, f->pos));
        float a = clampf(1.0f - d / f->radius, 0.0f, 1.0f) * (f->life / f->max_life);
        out = v3_add(out, v3_scale(f->color, a * a));
    }
    if (g->r.set.fullbright) out = v3(1, 1, 1);
    return v3(fminf(out.x, 1.4f), fminf(out.y, 1.4f), fminf(out.z, 1.4f));
}

/* ---------- atmosphere ---------- */

float game_measure_openness(Game *g, vec3 eye) {
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

void game_update_atmosphere(Game *g, PlayerSlot *ps, float dt) {
    vec3 eye = player_eye(&ps->pl);
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
    ps->openness_timer -= dt;
    if (ps->openness_timer <= 0) {
        ps->openness = game_measure_openness(g, eye);
        ps->openness_timer = 0.2f;
    }
    float open_t = clampf((ps->openness - 4.0f) / 40.0f, 0.0f, 1.0f);
    density *= lerpf(1.5f, 0.45f, open_t);
    float k = 1.0f - expf(-dt * 1.5f);
    ps->fog = v3_lerp(ps->fog, fog, k);
    ps->ambient = v3_lerp(ps->ambient, amb, k);
    ps->fog_density = lerpf(ps->fog_density, density, k);
    ps->stars = lerpf(ps->stars, stars, k);

    char name[128];
    world_place_name(&ci, name, sizeof name);
    if (strcmp(name, ps->place) != 0) {
        snprintf(ps->place, sizeof ps->place, "%s", name);
        if (!ps->st.dead) game_banner(ps, name);
    }
    ps->banner_time -= dt;
}

/* ---------- skills menu (keyboard and gamepad) ---------- */

enum { NAV_LEFT, NAV_RIGHT, NAV_UP, NAV_DOWN, NAV_BUY };

static void skills_nav(PlayerSlot *ps, int nav) {
    int b = ps->skill_sel / 4, t = ps->skill_sel % 4;
    switch (nav) {
    case NAV_LEFT: b = (b + BRANCH_COUNT - 1) % BRANCH_COUNT; break;
    case NAV_RIGHT: b = (b + 1) % BRANCH_COUNT; break;
    case NAV_UP: t = (t + 3) % 4; break;
    case NAV_DOWN: t = (t + 1) % 4; break;
    case NAV_BUY: {
        Stats *s = &ps->st;
        int id = skill_id(b, t);
        if (skill_can_buy(s->ranks, id, s->points)) {
            s->ranks[id]++;
            s->points--;
            float hp_frac = s->hp / s->maxhp;
            stats_refresh(ps);
            s->hp = s->maxhp * hp_frac;
        }
        break;
    }
    }
    ps->skill_sel = skill_id(b, t);
}

/* ---------- input ---------- */

static PlayerSlot *keyboard_player(Game *g) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active && g->players[i].keyboard) return &g->players[i];
    return NULL;
}

static void pad_connected(Game *g, SDL_JoystickID id) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active && g->players[i].pad_id == id) return;
    SDL_Gamepad *pad = SDL_OpenGamepad(id);
    if (!pad) return;
    for (int i = 0; i < ARRAY_LEN(g->idle_pads); i++)
        if (!g->idle_pads[i]) {
            g->idle_pads[i] = pad;
            SDL_Log("Gamepad connected: %s (press START to join, BACK to control player 1)", SDL_GetGamepadName(pad));
            game_banner_all(g, "GAMEPAD FOUND - START TO JOIN");
            return;
        }
    SDL_CloseGamepad(pad);
}

static void pad_disconnected(Game *g, SDL_JoystickID id) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (ps->active && ps->pad_id == id) {
            SDL_CloseGamepad(ps->pad);
            ps->pad = NULL;
            ps->pad_attack = ps->pad_alt = false;
            /* the character stays; a pad pressing START takes it over again */
            game_banner(ps, "GAMEPAD DISCONNECTED");
        }
    }
    for (int i = 0; i < ARRAY_LEN(g->idle_pads); i++)
        if (g->idle_pads[i] && SDL_GetGamepadID(g->idle_pads[i]) == id) {
            SDL_CloseGamepad(g->idle_pads[i]);
            g->idle_pads[i] = NULL;
        }
}

static void assign_pad(PlayerSlot *ps, SDL_Gamepad *pad) {
    ps->pad = pad;
    ps->pad_id = SDL_GetGamepadID(pad);
    ps->pad_prev = 0xFFFFFFFFu; /* ignore the button that did the joining */
}

/* Idle pads: START joins as a new player (or takes over a character whose pad was
 * unplugged), BACK makes the pad drive player 1. */
static void poll_idle_pads(Game *g) {
    for (int i = 0; i < ARRAY_LEN(g->idle_pads); i++) {
        SDL_Gamepad *pad = g->idle_pads[i];
        if (!pad) continue;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START)) {
            PlayerSlot *ps = NULL;
            for (int k = 1; k < MAX_PLAYERS && !ps; k++)
                if (g->players[k].active && !g->players[k].pad && !g->players[k].keyboard) ps = &g->players[k];
            if (!ps) ps = game_add_player(g);
            if (!ps) continue;
            assign_pad(ps, pad);
            g->idle_pads[i] = NULL;
            char buf[64];
            snprintf(buf, sizeof buf, "PLAYER %d JOINED", ps->index + 1);
            game_banner_all(g, buf);
        } else if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK) && !g->players[0].pad) {
            assign_pad(&g->players[0], pad);
            g->idle_pads[i] = NULL;
            game_banner(&g->players[0], "GAMEPAD CONTROLS PLAYER 1");
        }
    }
}

void game_event(Game *g, const SDL_Event *e) {
    if (e->type == SDL_EVENT_GAMEPAD_ADDED) { pad_connected(g, e->gdevice.which); return; }
    if (e->type == SDL_EVENT_GAMEPAD_REMOVED) { pad_disconnected(g, e->gdevice.which); return; }
    PlayerSlot *ps = keyboard_player(g);
    if (!ps) return;
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN || e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        bool down = e->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if (e->button.button == SDL_BUTTON_LEFT) ps->attack_held = down;
        if (e->button.button == SDL_BUTTON_RIGHT) ps->alt_held = down;
    } else if (e->type == SDL_EVENT_MOUSE_WHEEL && !ps->show_skills) {
        int d = e->wheel.y > 0 ? WPN_COUNT - 1 : 1;
        ps->st.weapon = (ps->st.weapon + d) % WPN_COUNT;
    } else if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) {
        SDL_Keycode k = e->key.key;
        if (ps->show_skills && k != SDLK_TAB && k != SDLK_ESCAPE) {
            switch (k) {
            case SDLK_LEFT: case SDLK_A: skills_nav(ps, NAV_LEFT); break;
            case SDLK_RIGHT: case SDLK_D: skills_nav(ps, NAV_RIGHT); break;
            case SDLK_UP: case SDLK_W: skills_nav(ps, NAV_UP); break;
            case SDLK_DOWN: case SDLK_S: skills_nav(ps, NAV_DOWN); break;
            case SDLK_RETURN: case SDLK_SPACE: case SDLK_E: skills_nav(ps, NAV_BUY); break;
            default: break;
            }
            return;
        }
        switch (k) {
        case SDLK_TAB: ps->show_skills = !ps->show_skills; ps->show_map = false; break;
        case SDLK_M: ps->show_map = !ps->show_map; ps->show_skills = false; break;
        case SDLK_1: ps->st.weapon = WPN_SWORD; break;
        case SDLK_2: ps->st.weapon = WPN_BOW; break;
        case SDLK_3: ps->st.weapon = WPN_STAFF; break;
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
        case SDLK_F7:
            g->opt.invert_y = !g->opt.invert_y;
            game_banner(ps, g->opt.invert_y ? "MOUSE Y INVERTED" : "MOUSE Y NORMAL");
            break;
        case SDLK_F8:
            g->opt.invert_x = !g->opt.invert_x;
            game_banner(ps, g->opt.invert_x ? "MOUSE X INVERTED" : "MOUSE X NORMAL");
            break;
        case SDLK_MINUS: g->opt.sens = fmaxf(0.2f, g->opt.sens / 1.15f); break;
        case SDLK_EQUALS: g->opt.sens = fminf(5.0f, g->opt.sens * 1.15f); break;
        case SDLK_F: ps->pl.fly = !ps->pl.fly; ps->pl.vel = v3(0, 0, 0); break;
        case SDLK_T: {
            vec3 d = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
            ps->pl.pos = v3_add(ps->pl.pos, v3_scale(d, 8.0f * CHUNK));
            ps->pl.fly = true;
            break;
        }
        case SDLK_PAGEUP: ps->pl.pos.y += CHUNK; ps->pl.fly = true; break;
        case SDLK_PAGEDOWN: ps->pl.pos.y -= CHUNK; ps->pl.fly = true; break;
        default: break;
        }
    }
}

void game_look(Game *g, float dx, float dy) {
    PlayerSlot *ps = keyboard_player(g);
    if (!ps || ps->show_skills || ps->show_map) return;
    float k = 0.0025f * (g->opt.sens > 0 ? g->opt.sens : 1.0f);
    if (g->opt.invert_x) dx = -dx;
    if (g->opt.invert_y) dy = -dy;
    ps->pl.yaw += dx * k;
    ps->pl.pitch = clampf(ps->pl.pitch - dy * k, -1.55f, 1.55f);
}

static float stick(SDL_Gamepad *pad, SDL_GamepadAxis axis) { return SDL_GetGamepadAxis(pad, axis) / 32767.0f; }

/* Radial dead zone, then a curve that keeps small deflections precise. */
static void deadzone(float *x, float *y, float dz) {
    float m = sqrtf(*x * *x + *y * *y);
    if (m < dz) { *x = *y = 0; return; }
    float k = fminf(1.0f, (m - dz) / (1.0f - dz)) / m;
    *x *= k;
    *y *= k;
}

/* Gamepad layout: left stick move, right stick look, RT attack, LT alternate attack,
 * A jump, B crouch, L3 sprint, LB/RB weapon, Y skills, BACK map. */
static void read_pad(Game *g, PlayerSlot *ps, PlayerInput *in, float dt) {
    SDL_Gamepad *pad = ps->pad;
    u32 now = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && b < 32; b++)
        if (SDL_GetGamepadButton(pad, (SDL_GamepadButton)b)) now |= 1u << b;
    u32 pressed = now & ~ps->pad_prev;
    ps->pad_prev = now;
#define PRESSED(btn) (pressed & (1u << (btn)))
#define HELD(btn) (now & (1u << (btn)))
    float lx = stick(pad, SDL_GAMEPAD_AXIS_LEFTX), ly = stick(pad, SDL_GAMEPAD_AXIS_LEFTY);
    float rx = stick(pad, SDL_GAMEPAD_AXIS_RIGHTX), ry = stick(pad, SDL_GAMEPAD_AXIS_RIGHTY);
    deadzone(&lx, &ly, 0.2f);
    deadzone(&rx, &ry, 0.15f);

    if (ps->show_skills) {
        static float nav_cool[MAX_PLAYERS];
        float *cool = &nav_cool[ps->index];
        *cool -= dt;
        if (PRESSED(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || (lx < -0.6f && *cool <= 0)) { skills_nav(ps, NAV_LEFT); *cool = 0.25f; }
        if (PRESSED(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || (lx > 0.6f && *cool <= 0)) { skills_nav(ps, NAV_RIGHT); *cool = 0.25f; }
        if (PRESSED(SDL_GAMEPAD_BUTTON_DPAD_UP) || (ly < -0.6f && *cool <= 0)) { skills_nav(ps, NAV_UP); *cool = 0.25f; }
        if (PRESSED(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || (ly > 0.6f && *cool <= 0)) { skills_nav(ps, NAV_DOWN); *cool = 0.25f; }
        if (fabsf(lx) < 0.3f && fabsf(ly) < 0.3f) *cool = 0;
        if (PRESSED(SDL_GAMEPAD_BUTTON_SOUTH)) skills_nav(ps, NAV_BUY);
        if (PRESSED(SDL_GAMEPAD_BUTTON_EAST) || PRESSED(SDL_GAMEPAD_BUTTON_NORTH)) ps->show_skills = false;
        ps->attack_held = ps->alt_held = ps->pad_attack = ps->pad_alt = false;
        return;
    }
    if (ps->show_map) {
        if (PRESSED(SDL_GAMEPAD_BUTTON_EAST) || PRESSED(SDL_GAMEPAD_BUTTON_BACK)) ps->show_map = false;
        ps->attack_held = ps->alt_held = ps->pad_attack = ps->pad_alt = false;
        return;
    }
    if (PRESSED(SDL_GAMEPAD_BUTTON_NORTH)) { ps->show_skills = true; ps->show_map = false; }
    if (PRESSED(SDL_GAMEPAD_BUTTON_BACK)) { ps->show_map = true; ps->show_skills = false; }
    if (PRESSED(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) ps->st.weapon = (ps->st.weapon + 1) % WPN_COUNT;
    if (PRESSED(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) ps->st.weapon = (ps->st.weapon + WPN_COUNT - 1) % WPN_COUNT;
    if (PRESSED(SDL_GAMEPAD_BUTTON_LEFT_STICK)) ps->sprint_toggle = !ps->sprint_toggle;
    if (fabsf(lx) < 0.1f && fabsf(ly) < 0.1f) ps->sprint_toggle = false;

    if (fabsf(lx) + fabsf(ly) > 0) { in->move_x = lx; in->move_z = -ly; }
    in->jump |= HELD(SDL_GAMEPAD_BUTTON_SOUTH) != 0;
    in->up |= in->jump;
    in->crouch |= HELD(SDL_GAMEPAD_BUTTON_EAST) != 0;
    in->down |= in->crouch;
    in->sprint |= ps->sprint_toggle;

    /* look speed in radians per second, with a cubic response for fine aim */
    float sens = g->opt.sens > 0 ? g->opt.sens : 1.0f;
    float ax = rx * rx * rx * 0.6f + rx * 0.4f, ay = ry * ry * ry * 0.6f + ry * 0.4f;
    if (g->opt.invert_x) ax = -ax;
    if (g->opt.invert_y) ay = -ay;
    ps->pl.yaw += ax * 3.4f * sens * dt;
    ps->pl.pitch = clampf(ps->pl.pitch - ay * 2.4f * sens * dt, -1.55f, 1.55f);

    float rt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    float lt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    ps->pad_attack = rt > 0.4f;
    ps->pad_alt = lt > 0.4f;
#undef PRESSED
#undef HELD
}

/* ---------- update ---------- */

static void update_shrines(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    for (int i = 0; i < g->nshrines; i++) {
        vec3 d = v3_sub(g->shrines[i], v3_add(ps->pl.pos, v3(0, 1, 0)));
        if (v3_len(d) > 2.8f) continue;
        if (v3_len(v3_sub(s->respawn, g->shrines[i])) > 5.0f && ps->pl.on_ground) {
            s->respawn = ps->pl.pos;
            s->respawn_yaw = ps->pl.yaw;
            s->hp = s->maxhp;
            s->mana = s->maxmana;
            game_banner(ps, "SHRINE ATTUNED - YOU WILL RETURN HERE");
            particles_burst(g, g->shrines[i], v3(1.0f, 0.85f, 0.45f), 30, 4.0f, 0.06f, true);
            flash_add(g, g->shrines[i], v3(1.3f, 1.1f, 0.6f), 10.0f, 0.8f);
        }
    }
}

static void update_player(Game *g, PlayerSlot *ps, const PlayerInput *in, float dt) {
    Stats *s = &ps->st;
    if (s->dead) {
        s->dead_t += dt;
        if (s->dead_t > 3.0f) {
            s->dead = false;
            ps->pl.pos = s->respawn;
            ps->pl.yaw = s->respawn_yaw;
            ps->pl.vel = v3(0, 0, 0);
            s->hp = s->maxhp;
            s->mana = s->maxmana;
            s->hurt = 0;
            /* a safe return: whatever was waiting at the shrine is gone, and a short grace period */
            for (int i = 0; i < MAX_MONSTERS; i++)
                if (g->mons[i].alive && v3_len(v3_sub(g->mons[i].pos, s->respawn)) < 16.0f) g->mons[i].alive = false;
            s->invuln = 2.0f;
            game_banner(ps, "THE SHRINE RESTORES YOU");
        }
        return;
    }
    s->invuln = fmaxf(0.0f, s->invuln - dt);
    /* do not simulate until the ground under the player exists */
    bool ground_known = game_solid(g, (int)floorf(ps->pl.pos.x), (int)floorf(ps->pl.pos.y) - 1, (int)floorf(ps->pl.pos.z)) >= 0;
    if (ground_known || ps->pl.fly) {
        PlayerInput pin = *in;
        player_update(&ps->pl, &pin, dt, game_solid, g);
    }
}

void game_update(Game *g, const PlayerInput *kb, float dt) {
    poll_idle_pads(g);
    bool paused = false;
    PlayerInput inputs[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        memset(&inputs[i], 0, sizeof inputs[i]);
        if (!ps->active) continue;
        if (ps->keyboard && kb) inputs[i] = *kb;
        if (ps->pad) read_pad(g, ps, &inputs[i], dt);
        if (ps->show_skills || ps->show_map) memset(&inputs[i], 0, sizeof inputs[i]);
        /* menus pause the game only when playing alone */
        if (g->nplayers == 1 && (ps->show_skills || ps->show_map)) paused = true;
    }
    if (!paused) {
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (g->players[i].active) update_player(g, &g->players[i], &inputs[i], dt);
        monsters_spawn_for_chunks(g);
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (g->players[i].active) flow_update(g, &g->players[i], dt);
        monsters_update(g, dt);
        combat_update(g, dt);
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (g->players[i].active) update_shrines(g, &g->players[i]);
    }
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) explore_update(g, &g->players[i], dt);
    g->time += dt;
}

/* ---------- drawing ---------- */

static void bar(float x, float y, float w, float h, float frac, float r, float gg, float b) {
    batch2d_rect(x - 1, y - 1, w + 2, h + 2, 0, 0, 0, 0.8f);
    batch2d_rect(x, y, w, h, r * 0.25f, gg * 0.25f, b * 0.25f, 1.0f);
    batch2d_rect(x, y, w * clampf(frac, 0, 1), h, r, gg, b, 1.0f);
}

static void draw_skills(Game *g, PlayerSlot *ps, int W, int H) {
    Stats *s = &ps->st;
    bool compact = W < 440; /* split screen: one branch at a time */
    int nb = compact ? 1 : BRANCH_COUNT;
    int bw = compact ? MINI(150, W - 40) : 104, bh = compact ? 22 : 34, gap = compact ? 5 : 8;
    int total_w = nb * bw + (nb - 1) * gap;
    int x0 = (W - total_w) / 2, y0 = compact ? 30 : 44;
    int sel_branch = ps->skill_sel / 4;
    batch2d_begin(W, H);
    batch2d_rect(0, 0, (float)W, (float)H, 0.02f, 0.01f, 0.01f, 0.8f);
    for (int bi = 0; bi < nb; bi++)
        for (int t = 0; t < 4; t++) {
            int b = compact ? sel_branch : bi;
            int id = skill_id(b, t);
            float x = (float)(x0 + bi * (bw + gap)), y = (float)(y0 + t * (bh + gap));
            bool sel = ps->skill_sel == id, owned = s->ranks[id] > 0, can = skill_can_buy(s->ranks, id, s->points);
            if (t > 0) batch2d_rect(x + bw / 2.0f - 1, y - gap, 2, (float)gap, 0.3f, 0.25f, 0.2f, 1.0f);
            if (sel) batch2d_rect(x - 2, y - 2, bw + 4.0f, bh + 4.0f, 0.95f, 0.75f, 0.35f, 1.0f);
            float k = owned ? 0.32f : (can ? 0.2f : 0.1f);
            batch2d_rect(x, y, (float)bw, (float)bh, k * 1.2f, k, k * 0.8f, 1.0f);
        }
    batch2d_flush();
    font_begin(W, H);
    char buf[160];
    snprintf(buf, sizeof buf, compact ? "LV %d - %d POINT%s" : "SKILLS - LEVEL %d - %d POINT%s TO SPEND", s->level, s->points,
             s->points == 1 ? "" : "S");
    font_text((W - font_text_width(buf)) / 2, compact ? 4 : 12, buf, 0.95f, 0.8f, 0.5f);
    for (int bi = 0; bi < nb; bi++) {
        int b = compact ? sel_branch : bi;
        if (compact) snprintf(buf, sizeof buf, "< %s >", BRANCH_NAMES[b]);
        else snprintf(buf, sizeof buf, "%s", BRANCH_NAMES[b]);
        font_text(x0 + bi * (bw + gap) + (bw - font_text_width(buf)) / 2, y0 - (compact ? 11 : 12), buf, 0.8f, 0.7f, 0.55f);
        for (int t = 0; t < 4; t++) {
            int id = skill_id(b, t);
            int x = x0 + bi * (bw + gap) + 4, y = y0 + t * (bh + gap) + (compact ? 2 : 5);
            bool owned = s->ranks[id] > 0, can = skill_can_buy(s->ranks, id, s->points);
            float c = owned ? 1.0f : (can ? 0.75f : 0.45f);
            font_text(x, y, SKILLS[id].name, c, c * 0.9f, c * 0.7f);
            snprintf(buf, sizeof buf, "%d/%d", s->ranks[id], SKILLS[id].max_rank);
            font_text(compact ? x + bw - 34 : x, compact ? y + 9 : y + 12, buf, 0.6f * c, 0.6f * c, 0.6f * c);
        }
    }
    const SkillDef *sd = &SKILLS[ps->skill_sel];
    int ty = y0 + 4 * (bh + gap) + 4;
    if (compact) {
        font_text((W - font_text_width(sd->name)) / 2, ty, sd->name, 0.9f, 0.85f, 0.75f);
        /* wrap the description over two lines */
        char line[64];
        int len = (int)strlen(sd->desc), maxc = MAXI(10, (W - 8) / 6), pos = 0, row = 1;
        while (pos < len && row <= 3) {
            int n = MINI(maxc, len - pos);
            if (pos + n < len)
                while (n > 0 && sd->desc[pos + n] != ' ') n--;
            if (n == 0) n = MINI(maxc, len - pos);
            snprintf(line, sizeof line, "%.*s", n, sd->desc + pos);
            font_text((W - font_text_width(line)) / 2, ty + row * 9, line, 0.7f, 0.68f, 0.6f);
            pos += n;
            while (pos < len && sd->desc[pos] == ' ') pos++;
            row++;
        }
        const char *help = ps->keyboard && !ps->pad ? "ENTER LEARN  TAB CLOSE" : "A LEARN  B CLOSE";
        font_text((W - font_text_width(help)) / 2, H - 10, help, 0.55f, 0.5f, 0.45f);
    } else {
        snprintf(buf, sizeof buf, "%s: %s", sd->name, sd->desc);
        font_text((W - font_text_width(buf)) / 2, ty, buf, 0.9f, 0.85f, 0.75f);
        const char *help = ps->keyboard && !ps->pad ? "ARROWS/WASD SELECT  ENTER LEARN  TAB CLOSE" : "D-PAD SELECT  A LEARN  B CLOSE";
        font_text((W - font_text_width(help)) / 2, ty + 14, help, 0.55f, 0.5f, 0.45f);
        snprintf(buf, sizeof buf, "HEALTH %.0f  MANA %.0f  SPEED +%d%%  KILLS %d", s->maxhp, s->maxmana, s->ranks[SK_FLEET_FOOT] * 12,
                 s->kills);
        font_text((W - font_text_width(buf)) / 2, ty + 28, buf, 0.6f, 0.6f, 0.6f);
    }
    font_end();
}

static void draw_hud(Game *g, PlayerSlot *ps, int W, int H) {
    Stats *s = &ps->st;
    vec3 eye = player_eye(&ps->pl);
    bool small = W < 400;
    float bw = small ? fminf(80.0f, W * 0.35f) : 110.0f;
    batch2d_begin(W, H);
    if (s->hurt > 0) batch2d_rect(0, 0, (float)W, (float)H, 0.6f, 0.0f, 0.0f, s->hurt * 0.35f);
    if (s->dead) batch2d_rect(0, 0, (float)W, (float)H, 0.25f, 0.0f, 0.0f, fminf(0.75f, s->dead_t * 0.4f));
    bar(6, H - 24.0f, bw, 5, s->hp / s->maxhp, 0.85f, 0.15f, 0.12f);
    bar(6, H - 15.0f, bw, 3, s->mana / s->maxmana, 0.25f, 0.4f, 0.95f);
    bar(0, H - 3.0f, (float)W, 2, s->xp / s->xp_next, 0.9f, 0.7f, 0.25f);
    if (g->nplayers > 1) {
        /* a thin frame in the player's colour tells the views apart */
        vec3 c = ps->color;
        batch2d_rect(0, 0, (float)W, 1, c.x, c.y, c.z, 0.9f);
        batch2d_rect(0, 0, 1, (float)H, c.x, c.y, c.z, 0.9f);
        batch2d_rect(W - 1.0f, 0, 1, (float)H, c.x, c.y, c.z, 0.9f);
    }
    batch2d_flush();

    font_begin(W, H);
    font_text(W / 2 - 3, H / 2 - 4, "+", 0.8f, 0.75f, 0.6f);
    char buf[256];
    if (g->nplayers > 1) snprintf(buf, sizeof buf, "P%d LV %d", ps->index + 1, s->level);
    else snprintf(buf, sizeof buf, "LV %d", s->level);
    font_text((int)bw + 12, H - 26, buf, ps->color.x, ps->color.y, ps->color.z);
    snprintf(buf, sizeof buf, "%.0f", s->hp);
    font_text((int)bw + 12, H - 17, buf, 0.85f, 0.4f, 0.35f);
    if (small) {
        const char *wn = combat_weapon_name(s->weapon);
        font_text(W - font_text_width(wn) - 5, H - 14, wn, 1.0f, 0.85f, 0.6f);
    } else {
        for (int w = 0; w < WPN_COUNT; w++) {
            snprintf(buf, sizeof buf, "%d %s", w + 1, combat_weapon_name(w));
            float c = w == s->weapon ? 1.0f : 0.45f;
            font_text(W - 190 + w * 62, H - 14, buf, c, c * 0.85f, c * 0.6f);
        }
    }
    if (s->points > 0) {
        snprintf(buf, sizeof buf, small ? "%d PT - %s" : "%d SKILL POINT%s - %s", s->points,
                 small ? (ps->pad ? "Y" : "TAB") : (s->points == 1 ? "" : "S"), ps->pad ? "Y" : "TAB");
        if (small) snprintf(buf, sizeof buf, "%d PT (%s)", s->points, ps->pad ? "Y" : "TAB");
        font_text((W - font_text_width(buf)) / 2, H - 36, buf, 1.0f, 0.85f, 0.4f);
    }
    if (ps->banner_time > 0) {
        float a = clampf(ps->banner_time, 0, 1);
        const char *b = ps->banner;
        font_text((W - font_text_width(b)) / 2, H / 4, b, 0.95f * a, 0.82f * a, 0.55f * a);
    }
    bool idle_pad = false;
    for (int i = 0; i < ARRAY_LEN(g->idle_pads); i++) idle_pad |= g->idle_pads[i] != NULL;
    if (idle_pad && ps->index == 0 && g->nplayers < MAX_PLAYERS) {
        const char *t = "GAMEPAD READY: PRESS START TO JOIN";
        font_text((W - font_text_width(t)) / 2, 6, t, 0.7f, 0.85f, 0.95f);
    }
    if (g->hud && ps->index == 0) {
        int y = 4 + (idle_pad ? 10 : 0);
        int cx = floordiv((int)floorf(eye.x), CHUNK), cy = floordiv((int)floorf(eye.y), CHUNK), cz = floordiv((int)floorf(eye.z), CHUNK);
        Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
        snprintf(buf, sizeof buf, "FORTRESS CRAWL  seed %llu  %.0f fps", (unsigned long long)g->world.seed, g->fps);
        font_text(4, y, buf, 0.85f, 0.7f, 0.45f); y += FONT_LINE;
        font_text(4, y, ps->place, 0.9f, 0.85f, 0.75f); y += FONT_LINE;
        snprintf(buf, sizeof buf, "pos %.1f %.1f %.1f  chunk %d %d %d  yaw %.0f pitch %.0f", eye.x, eye.y, eye.z, cx, cy, cz,
                 ps->pl.yaw * 180.0f / PI_F, ps->pl.pitch * 180.0f / PI_F);
        font_text(4, y, buf, 0.7f, 0.7f, 0.7f); y += FONT_LINE;
        if (c && c->state == CH_READY) {
            snprintf(buf, sizeof buf, "danger level %d  %s / %s", c->info.level, STRATA[c->info.stratum].name, FLAVORS[c->info.flavor].name);
            font_text(4, y, buf, 0.7f, 0.7f, 0.7f); y += FONT_LINE;
        }
        int alive = 0;
        for (int i = 0; i < MAX_MONSTERS; i++) alive += g->mons[i].alive;
        snprintf(buf, sizeof buf, "chunks %d meshed %d drawn  jobs %d  monsters %d", g->map.stats.meshed, g->map.stats.drawn,
                 g->map.stats.jobs, alive);
        font_text(4, y, buf, 0.6f, 0.6f, 0.6f); y += FONT_LINE;
        static const char *STANCES[3] = { "standing", "crouching", "crawling" };
        snprintf(buf, sizeof buf, "%s  %s  open %.0fm  %dx%d", ps->pl.fly ? "FLY" : "WALK", STANCES[ps->pl.stance], ps->openness, W, H);
        font_text(4, y, buf, 0.6f, 0.6f, 0.6f); y += FONT_LINE;
        if (!small) font_text(4, y, "LMB/RMB ATTACK  1-3 WEAPON  TAB SKILLS  M MAP  F FLY  T AHEAD  F1-F6", 0.5f, 0.45f, 0.4f);
    }
    font_end();
}

/* View rectangles in the low-res target: 1 full, 2 side by side, 3-4 quarters. */
static void view_rect(const Game *g, int slot_order, int n, int *x, int *y, int *w, int *h) {
    int W = g->r.fbo_w, H = g->r.fbo_h;
    if (n <= 1) { *x = 0; *y = 0; *w = W; *h = H; return; }
    if (n == 2) { *w = W / 2; *h = H; *x = slot_order * (W / 2); *y = 0; return; }
    *w = W / 2; *h = H / 2;
    *x = (slot_order % 2) * (W / 2);
    *y = (slot_order / 2) * (H / 2);
}

static void render_view(Game *g, PlayerSlot *ps, int vx, int vy, int vw, int vh) {
    vec3 eye = player_eye(&ps->pl);
    SceneParams sp;
    memset(&sp, 0, sizeof sp);
    float aspect = (float)vw / (float)vh;
    /* keep the horizontal field of view sensible in tall split-screen views */
    float fov = g->r.set.fov_deg * PI_F / 180.0f;
    if (aspect < 1.2f) fov = 2.0f * atanf(tanf(fov * 0.5f) * 1.2f / aspect) * 0.85f;
    sp.proj = m4_perspective(fov, aspect, 0.05f, 400.0f);
    sp.view = m4_look_dir(eye, dir_from_angles(ps->pl.yaw, ps->pl.pitch), v3(0, 1, 0));
    sp.cam = eye;
    sp.fog = ps->fog;
    sp.fog_density = ps->fog_density;
    sp.ambient = ps->ambient;
    sp.stars = ps->stars;
    sp.time = g->time;
    combat_collect_lights(g, ps, &sp);
    renderer_set_view(&g->r, vx, vy, vw, vh);
    renderer_begin(&g->r, &sp);
    chunkmap_draw(&g->map, eye);

    BatchFog fog = { eye, ps->fog, ps->fog_density };
    batch3d_begin();
    monsters_draw(g, eye);
    players_draw(g, ps);
    combat_draw_world(g, eye);
    batch3d_flush(&g->r.vp, &fog);

    /* the weapon in view gets its own depth range */
    glClear(GL_DEPTH_BUFFER_BIT);
    mat4 vm_proj = m4_perspective(60.0f * PI_F / 180.0f, aspect, 0.01f, 10.0f);
    BatchFog none = { v3(0, 0, 0), ps->fog, 0.0f };
    batch3d_begin();
    combat_draw_viewmodel(g, ps);
    batch3d_flush(&vm_proj, &none);
    renderer_end_world(&g->r);

    if (ps->show_map) explore_draw_map(g, ps, vw, vh);
    else if (ps->show_skills) draw_skills(g, ps, vw, vh);
    else draw_hud(g, ps, vw, vh);
}

void game_render(Game *g) {
    renderer_frame_begin(&g->r);
    int order = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        int x, y, w, h;
        view_rect(g, order++, g->nplayers, &x, &y, &w, &h);
        render_view(g, ps, x, y, w, h);
    }
    if (g->nplayers == 3) {
        /* the spare quarter shows who is where */
        int x, y, w, h;
        view_rect(g, 3, 4, &x, &y, &w, &h);
        renderer_set_view(&g->r, x, y, w, h);
        font_begin(w, h);
        int ty = h / 2 - 20;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            PlayerSlot *ps = &g->players[i];
            if (!ps->active) continue;
            char buf[160];
            snprintf(buf, sizeof buf, "P%d LV %d  %s", i + 1, ps->st.level, ps->place);
            font_text(6, ty, buf, ps->color.x, ps->color.y, ps->color.z);
            ty += 12;
        }
        font_end();
    }
    renderer_present(&g->r, g->time);
}
