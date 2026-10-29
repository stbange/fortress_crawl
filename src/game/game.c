#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

static const int RES_MODES[3][2] = { { 320, 180 }, { 480, 270 }, { 640, 360 } };

int game_solid(void *user, int x, int y, int z) {
    Game *g = user;
    bool known;
    Block b = chunkmap_block(&g->map, x, y, z, &known);
    if (!known) return -1;
    return block_solid(b) ? 1 : 0;
}

void game_banner(Game *g, const char *text) {
    snprintf(g->banner, sizeof g->banner, "%s", text);
    g->banner_time = 3.0f;
}

void game_init(Game *g) {
    Stats *s = &g->st;
    memset(s, 0, sizeof *s);
    s->level = 1;
    s->xp_next = 60.0f;
    s->weapon = WPN_SWORD;
    stats_refresh(g);
    s->hp = s->maxhp;
    s->mana = s->maxmana;
    s->respawn = g->pl.pos;
    s->respawn_yaw = g->pl.yaw;
    s->since_hurt = 100.0f;
    g->hud = g->opt.hud;
    batch_init();
}

void game_add_xp(Game *g, float xp) {
    Stats *s = &g->st;
    s->xp += xp;
    while (s->xp >= s->xp_next) {
        s->xp -= s->xp_next;
        s->level++;
        s->points++;
        s->xp_next = 60.0f * powf(1.4f, (float)(s->level - 1));
        stats_refresh(g);
        s->hp = s->maxhp;
        s->mana = s->maxmana;
        char buf[96];
        snprintf(buf, sizeof buf, "LEVEL %d!  +1 SKILL POINT (TAB)", s->level);
        game_banner(g, buf);
        flash_add(g, player_eye(&g->pl), v3(1.2f, 1.0f, 0.6f), 10.0f, 0.6f);
    }
}

void game_hurt_player(Game *g, float dmg, vec3 from) {
    Stats *s = &g->st;
    if (s->dead || g->pl.fly || s->invuln > 0) return;
    if (s->ranks[SK_LAST_STAND] && s->hp < s->maxhp * 0.3f) dmg *= 0.5f;
    s->hp -= dmg;
    s->hurt = fminf(1.0f, s->hurt + 0.35f + dmg / s->maxhp);
    s->since_hurt = 0;
    vec3 push = v3_sub(g->pl.pos, from);
    push.y = 0;
    push = v3_norm(push);
    g->pl.vel = v3_add(g->pl.vel, v3_scale(push, 4.0f));
    if (s->hp <= 0) {
        s->hp = 0;
        s->dead = true;
        s->dead_t = 0;
        game_banner(g, "YOU HAVE FALLEN");
    }
}

vec3 game_light_at(Game *g, vec3 p) {
    u8 l[4];
    vec3 out = g->ambient;
    if (chunkmap_light(&g->map, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z), l)) {
        out.x = fmaxf(out.x, l[0] ? powf(0.85f, 15.0f - l[0]) : 0.0f);
        out.y = fmaxf(out.y, l[1] ? powf(0.85f, 15.0f - l[1]) : 0.0f);
        out.z = fmaxf(out.z, l[2] ? powf(0.85f, 15.0f - l[2]) : 0.0f);
    }
    if (g->r.set.torch) {
        float d = v3_len(v3_sub(p, player_eye(&g->pl)));
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

void game_update_atmosphere(Game *g, float dt, vec3 eye) {
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
        g->openness = game_measure_openness(g, eye);
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
        if (!g->st.dead) game_banner(g, name);
    }
}

/* ---------- input ---------- */

static void skills_key(Game *g, SDL_Keycode k) {
    int b = g->skill_sel / 4, t = g->skill_sel % 4;
    switch (k) {
    case SDLK_LEFT: case SDLK_A: b = (b + BRANCH_COUNT - 1) % BRANCH_COUNT; break;
    case SDLK_RIGHT: case SDLK_D: b = (b + 1) % BRANCH_COUNT; break;
    case SDLK_UP: case SDLK_W: t = (t + 3) % 4; break;
    case SDLK_DOWN: case SDLK_S: t = (t + 1) % 4; break;
    case SDLK_RETURN: case SDLK_SPACE: case SDLK_E: {
        int id = skill_id(b, t);
        if (skill_can_buy(g->st.ranks, id, g->st.points)) {
            g->st.ranks[id]++;
            g->st.points--;
            float hp_frac = g->st.hp / g->st.maxhp;
            stats_refresh(g);
            g->st.hp = g->st.maxhp * hp_frac;
        }
        break;
    }
    default: break;
    }
    g->skill_sel = skill_id(b, t);
}

void game_event(Game *g, const SDL_Event *e) {
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN || e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        bool down = e->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if (e->button.button == SDL_BUTTON_LEFT) g->attack_held = down;
        if (e->button.button == SDL_BUTTON_RIGHT) g->alt_held = down;
    } else if (e->type == SDL_EVENT_MOUSE_WHEEL && !g->show_skills) {
        int d = e->wheel.y > 0 ? WPN_COUNT - 1 : 1;
        g->st.weapon = (g->st.weapon + d) % WPN_COUNT;
    } else if (e->type == SDL_EVENT_MOUSE_MOTION && g->mouse && !g->show_skills && !g->show_map) {
        g->pl.yaw += e->motion.xrel * 0.0025f;
        g->pl.pitch = clampf(g->pl.pitch - e->motion.yrel * 0.0025f, -1.55f, 1.55f);
    } else if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) {
        SDL_Keycode k = e->key.key;
        if (g->show_skills && k != SDLK_TAB && k != SDLK_ESCAPE) { skills_key(g, k); return; }
        switch (k) {
        case SDLK_TAB: g->show_skills = !g->show_skills; g->show_map = false; break;
        case SDLK_M: g->show_map = !g->show_map; g->show_skills = false; break;
        case SDLK_1: g->st.weapon = WPN_SWORD; break;
        case SDLK_2: g->st.weapon = WPN_BOW; break;
        case SDLK_3: g->st.weapon = WPN_STAFF; break;
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

/* ---------- update ---------- */

static void update_shrines(Game *g) {
    Stats *s = &g->st;
    for (int i = 0; i < g->nshrines; i++) {
        vec3 d = v3_sub(g->shrines[i], v3_add(g->pl.pos, v3(0, 1, 0)));
        if (v3_len(d) > 2.8f) continue;
        if (v3_len(v3_sub(s->respawn, g->shrines[i])) > 5.0f && g->pl.on_ground) {
            s->respawn = g->pl.pos;
            s->respawn_yaw = g->pl.yaw;
            s->hp = s->maxhp;
            s->mana = s->maxmana;
            game_banner(g, "SHRINE ATTUNED - YOU WILL RETURN HERE");
            particles_burst(g, g->shrines[i], v3(1.0f, 0.85f, 0.45f), 30, 4.0f, 0.06f, true);
            flash_add(g, g->shrines[i], v3(1.3f, 1.1f, 0.6f), 10.0f, 0.8f);
        }
    }
}

void game_update(Game *g, const PlayerInput *in, float dt) {
    Stats *s = &g->st;
    bool paused = g->show_skills || g->show_map;
    if (!paused) {
        if (s->dead) {
            s->dead_t += dt;
            if (s->dead_t > 3.0f) {
                s->dead = false;
                g->pl.pos = s->respawn;
                g->pl.yaw = s->respawn_yaw;
                g->pl.vel = v3(0, 0, 0);
                s->hp = s->maxhp;
                s->mana = s->maxmana;
                s->hurt = 0;
                for (int i = 0; i < MAX_PROJECTILES; i++)
                    if (g->proj[i].kind == PJ_ENEMY_FIRE) g->proj[i].alive = false;
                /* a safe return: whatever was waiting at the shrine is gone, and a short grace period */
                for (int i = 0; i < MAX_MONSTERS; i++)
                    if (g->mons[i].alive && v3_len(v3_sub(g->mons[i].pos, s->respawn)) < 16.0f) g->mons[i].alive = false;
                s->invuln = 2.0f;
                game_banner(g, "THE SHRINE RESTORES YOU");
            }
        } else {
            s->invuln = fmaxf(0.0f, s->invuln - dt);
            PlayerInput pin = *in;
            player_update(&g->pl, &pin, dt, game_solid, g);
        }
        monsters_spawn_for_chunks(g);
        flow_update(g, dt);
        monsters_update(g, dt);
        combat_update(g, dt);
        update_shrines(g);
    }
    explore_update(g, dt);
    g->time += dt;
}

/* ---------- drawing ---------- */

static void bar(float x, float y, float w, float h, float frac, float r, float gg, float b) {
    batch2d_rect(x - 1, y - 1, w + 2, h + 2, 0, 0, 0, 0.8f);
    batch2d_rect(x, y, w, h, r * 0.25f, gg * 0.25f, b * 0.25f, 1.0f);
    batch2d_rect(x, y, w * clampf(frac, 0, 1), h, r, gg, b, 1.0f);
}

static void draw_skills(Game *g) {
    Renderer *r = &g->r;
    Stats *s = &g->st;
    int bw = 104, bh = 34, gap = 8;
    int total_w = BRANCH_COUNT * bw + (BRANCH_COUNT - 1) * gap;
    int x0 = (r->fbo_w - total_w) / 2, y0 = 44;
    batch2d_begin(r->fbo_w, r->fbo_h);
    batch2d_rect(0, 0, (float)r->fbo_w, (float)r->fbo_h, 0.02f, 0.01f, 0.01f, 0.8f);
    for (int b = 0; b < BRANCH_COUNT; b++)
        for (int t = 0; t < 4; t++) {
            int id = skill_id(b, t);
            float x = (float)(x0 + b * (bw + gap)), y = (float)(y0 + t * (bh + gap));
            bool sel = g->skill_sel == id, owned = s->ranks[id] > 0, can = skill_can_buy(s->ranks, id, s->points);
            if (t > 0) batch2d_rect(x + bw / 2.0f - 1, y - gap, 2, (float)gap, 0.3f, 0.25f, 0.2f, 1.0f);
            if (sel) batch2d_rect(x - 2, y - 2, bw + 4.0f, bh + 4.0f, 0.95f, 0.75f, 0.35f, 1.0f);
            float k = owned ? 0.32f : (can ? 0.2f : 0.1f);
            batch2d_rect(x, y, (float)bw, (float)bh, k * 1.2f, k, k * 0.8f, 1.0f);
        }
    batch2d_flush();
    font_begin(r->fbo_w, r->fbo_h);
    char buf[160];
    snprintf(buf, sizeof buf, "SKILLS - LEVEL %d - %d POINT%s TO SPEND", s->level, s->points, s->points == 1 ? "" : "S");
    font_text((r->fbo_w - font_text_width(buf)) / 2, 12, buf, 0.95f, 0.8f, 0.5f);
    for (int b = 0; b < BRANCH_COUNT; b++)
        font_text(x0 + b * (bw + gap) + (bw - font_text_width(BRANCH_NAMES[b])) / 2, y0 - 12, BRANCH_NAMES[b], 0.8f, 0.7f, 0.55f);
    for (int b = 0; b < BRANCH_COUNT; b++)
        for (int t = 0; t < 4; t++) {
            int id = skill_id(b, t);
            int x = x0 + b * (bw + gap) + 4, y = y0 + t * (bh + gap) + 5;
            bool owned = s->ranks[id] > 0, can = skill_can_buy(s->ranks, id, s->points);
            float c = owned ? 1.0f : (can ? 0.75f : 0.45f);
            font_text(x, y, SKILLS[id].name, c, c * 0.9f, c * 0.7f);
            snprintf(buf, sizeof buf, "%d/%d", s->ranks[id], SKILLS[id].max_rank);
            font_text(x, y + 12, buf, 0.6f * c, 0.6f * c, 0.6f * c);
        }
    const SkillDef *sd = &SKILLS[g->skill_sel];
    snprintf(buf, sizeof buf, "%s: %s", sd->name, sd->desc);
    int ty = y0 + 4 * (bh + gap) + 4;
    font_text((r->fbo_w - font_text_width(buf)) / 2, ty, buf, 0.9f, 0.85f, 0.75f);
    font_text((r->fbo_w - font_text_width("ARROWS/WASD SELECT  ENTER LEARN  TAB CLOSE")) / 2, ty + 14,
              "ARROWS/WASD SELECT  ENTER LEARN  TAB CLOSE", 0.55f, 0.5f, 0.45f);
    snprintf(buf, sizeof buf, "HEALTH %.0f  MANA %.0f  SPEED +%d%%  KILLS %d", s->maxhp, s->maxmana, s->ranks[SK_FLEET_FOOT] * 12, s->kills);
    font_text((r->fbo_w - font_text_width(buf)) / 2, ty + 28, buf, 0.6f, 0.6f, 0.6f);
    font_end();
}

static void draw_hud(Game *g) {
    Renderer *r = &g->r;
    Stats *s = &g->st;
    vec3 eye = player_eye(&g->pl);
    int W = r->fbo_w, H = r->fbo_h;
    batch2d_begin(W, H);
    if (s->hurt > 0) batch2d_rect(0, 0, (float)W, (float)H, 0.6f, 0.0f, 0.0f, s->hurt * 0.35f);
    if (s->dead) batch2d_rect(0, 0, (float)W, (float)H, 0.25f, 0.0f, 0.0f, fminf(0.75f, s->dead_t * 0.4f));
    bar(8, H - 28.0f, 110, 6, s->hp / s->maxhp, 0.85f, 0.15f, 0.12f);
    bar(8, H - 18.0f, 110, 4, s->mana / s->maxmana, 0.25f, 0.4f, 0.95f);
    bar(0, H - 3.0f, (float)W, 2, s->xp / s->xp_next, 0.9f, 0.7f, 0.25f);
    batch2d_flush();

    font_begin(W, H);
    font_text(W / 2 - 3, H / 2 - 4, "+", 0.8f, 0.75f, 0.6f);
    char buf[256];
    snprintf(buf, sizeof buf, "LV %d", s->level);
    font_text(124, H - 30, buf, 0.95f, 0.8f, 0.5f);
    snprintf(buf, sizeof buf, "%.0f", s->hp);
    font_text(124, H - 20, buf, 0.85f, 0.4f, 0.35f);
    for (int w = 0; w < WPN_COUNT; w++) {
        snprintf(buf, sizeof buf, "%d %s", w + 1, combat_weapon_name(w));
        float c = w == s->weapon ? 1.0f : 0.45f;
        font_text(W - 190 + w * 62, H - 16, buf, c, c * 0.85f, c * 0.6f);
    }
    if (s->points > 0) {
        snprintf(buf, sizeof buf, "%d SKILL POINT%s - TAB", s->points, s->points == 1 ? "" : "S");
        font_text((W - font_text_width(buf)) / 2, H - 16, buf, 1.0f, 0.85f, 0.4f);
    }
    if (g->banner_time > 0) {
        float a = clampf(g->banner_time, 0, 1);
        font_text((W - font_text_width(g->banner)) / 2, H / 4, g->banner, 0.95f * a, 0.82f * a, 0.55f * a);
    }
    if (g->hud) {
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
        int alive = 0;
        for (int i = 0; i < MAX_MONSTERS; i++) alive += g->mons[i].alive;
        snprintf(buf, sizeof buf, "chunks %d meshed %d drawn  jobs %d  monsters %d", g->map.stats.meshed, g->map.stats.drawn,
                 g->map.stats.jobs, alive);
        font_text(4, y, buf, 0.6f, 0.6f, 0.6f); y += FONT_LINE;
        static const char *STANCES[3] = { "standing", "crouching", "crawling" };
        snprintf(buf, sizeof buf, "%s  %s  open %.0fm  %dx%d", g->pl.fly ? "FLY" : "WALK", STANCES[g->pl.stance], g->openness, W, H);
        font_text(4, y, buf, 0.6f, 0.6f, 0.6f); y += FONT_LINE;
        font_text(4, y, "LMB/RMB ATTACK  1-3 WEAPON  TAB SKILLS  M MAP  F FLY  T AHEAD  F1-F6", 0.5f, 0.45f, 0.4f);
    }
    font_end();
}

void game_render(Game *g) {
    vec3 eye = player_eye(&g->pl);
    SceneParams sp;
    memset(&sp, 0, sizeof sp);
    float aspect = (float)g->r.set.low_w / (float)g->r.set.low_h;
    sp.proj = m4_perspective(g->r.set.fov_deg * PI_F / 180.0f, aspect, 0.05f, 400.0f);
    sp.view = m4_look_dir(eye, dir_from_angles(g->pl.yaw, g->pl.pitch), v3(0, 1, 0));
    sp.cam = eye;
    sp.fog = g->fog;
    sp.fog_density = g->fog_density;
    sp.ambient = g->ambient;
    sp.stars = g->stars;
    sp.time = g->time;
    combat_collect_lights(g, &sp);
    renderer_begin(&g->r, &sp);
    chunkmap_draw(&g->map);

    BatchFog fog = { eye, g->fog, g->fog_density };
    batch3d_begin();
    monsters_draw(g);
    combat_draw_world(g);
    batch3d_flush(&g->r.vp, &fog);

    /* the weapon in view gets its own depth range */
    glClear(GL_DEPTH_BUFFER_BIT);
    mat4 vm_proj = m4_perspective(60.0f * PI_F / 180.0f, aspect, 0.01f, 10.0f);
    BatchFog none = { v3(0, 0, 0), g->fog, 0.0f };
    batch3d_begin();
    combat_draw_viewmodel(g);
    batch3d_flush(&vm_proj, &none);
    renderer_end_world(&g->r);

    if (g->show_map) explore_draw_map(g);
    else if (g->show_skills) draw_skills(g);
    else draw_hud(g);
    renderer_present(&g->r, g->time);
}
