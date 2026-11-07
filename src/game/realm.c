#include "game/game.h"

#include "render/batch.h"

/* The Sanctum behind the Sanctum Door: a home on a floating island that starts as a ruin
 * and is built up plot by plot (gen_pocket.c builds what World.sanctum says; shop.c sells
 * the stages). Here are its doors, its comforts and its trophies:
 *   the bath (bath plot, stage 1+): wounds close, burns and poison wash off, Refreshed;
 *   the feast (dining plot, stage 1+): everything restored and Well Fed;
 *   the bed (the hall, always): everything restored, you wake there, the game is saved;
 *   the armory (stage 1+): this world's legendaries you found on racks, banners of the
 *     named enemies you killed, a statue for every chapter finished.
 * The higher a room's stage, the longer its comfort lasts. The door in the court takes
 * you back to where you opened the Sanctum Door. */

static int fl(float v) { return (int)floorf(v); }

bool realm_has(const vec3 pos) { return world_is_pocket(fl(pos.x / 32.0f), fl(pos.y / 32.0f), fl(pos.z / 32.0f)); }

int sanctum_stage(const Game *g, int plot) { return plot >= 0 && plot < SP_COUNT ? g->world.sanctum[plot] : 0; }

void sanctum_set_stage(Game *g, int plot, int stage) {
    if (plot < 0 || plot >= SP_COUNT) return;
    g->world.sanctum[plot] = (u8)clampf((float)stage, 0, SANCTUM_STAGES);
    g->world.sanctum_gen++; /* the island's chunks are generated again (realm_update) */
}

static vec3 arrive_pos(void) { return v3(POCKET_ARRIVE_X, (float)POCKET_G, POCKET_ARRIVE_Z); }
#define ARRIVE_YAW (-PI_F * 0.5f) /* facing the hall */

static void move_to(Game *g, PlayerSlot *ps, vec3 to, float yaw) {
    particles_burst(g, player_eye(&ps->pl), v3(1.0f, 0.85f, 0.5f), 20, 3.0f, 0.05f, true);
    ps->pl.pos = to;
    ps->pl.vel = v3(0, 0, 0);
    ps->pl.yaw = yaw;
    ps->pl.pitch = 0;
    ps->st.fall_speed = 0;
    ps->st.feast_t = ps->st.rest_t = 0;
    particles_burst(g, player_eye(&ps->pl), v3(1.0f, 0.85f, 0.5f), 20, 3.0f, 0.05f, true);
    flash_add(g, player_eye(&ps->pl), v3(1.2f, 0.9f, 0.5f), 8.0f, 0.4f);
}

void realm_enter(Game *g, PlayerSlot *ps, vec3 from, float yaw) {
    Stats *s = &ps->st;
    if (!realm_has(ps->pl.pos)) {
        s->realm_return = from;
        s->realm_return_yaw = yaw;
        s->realm_return_set = true;
    }
    s->scry_t = 0;
    move_to(g, ps, arrive_pos(), ARRIVE_YAW);
    sfx_at(g, SFX_REALM_DOOR, ps->pl.pos, 1.0f);
    char buf[128];
    snprintf(buf, sizeof buf, "THE SANCTUM - %d KEYSTONES. BUILD AND TRADE IN THE SHOP TAB (%s)", g->sanctum.keystones, game_skills_key(g, ps));
    game_banner(ps, buf);
    g->autosave_timer = 1e9f;
}

static void leave(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    /* back where the door was opened last (kept, so waking in the Sanctum bed still has a
     * way out), else the last shrine, else the gate */
    vec3 to = s->realm_return_set ? s->realm_return : s->respawn;
    float yaw = s->realm_return_set ? s->realm_return_yaw : s->respawn_yaw;
    if (realm_has(to)) to = g->start_pos, yaw = g->start_yaw;
    move_to(g, ps, to, yaw);
    sfx_at(g, SFX_REALM_DOOR, to, 1.0f);
    ps->gate_cd = 1.5f;
    g->autosave_timer = 1e9f;
}

static bool in_rect(vec3 p, int x0, int z0, int x1, int z1, float grow) {
    return p.x >= x0 - grow && p.x < x1 + 1 + grow && p.z >= z0 - grow && p.z < z1 + 1 + grow;
}

static void restore(PlayerSlot *ps) {
    Stats *s = &ps->st;
    s->hp = s->maxhp;
    s->mana = s->maxmana;
    s->burn_t = s->poison_t = s->chill_t = 0;
}

static void comforts(Game *g, PlayerSlot *ps, float dt) {
    Stats *s = &ps->st;
    vec3 p = ps->pl.pos;
    /* the bath: feet in the pool; Refreshed for 10, 15 or 25 minutes by its stage */
    int bs = sanctum_stage(g, SP_BATH);
    bool in_bath = bs >= 1 && in_rect(p, POCKET_BATH_X0, POCKET_BATH_Z0, POCKET_BATH_X1, POCKET_BATH_Z1, 0) && p.y < POCKET_G - 0.2f;
    if (in_bath) {
        static const float REFRESH[4] = { 0, 600.0f, 900.0f, 1500.0f };
        s->hp = fminf(s->maxhp, s->hp + s->maxhp * 0.2f * dt);
        s->burn_t = s->poison_t = s->chill_t = 0;
        if (s->refreshed_t < REFRESH[bs] - 60.0f) {
            s->refreshed_t = REFRESH[bs];
            sfx_at(g, SFX_REALM_BATH, p, 1.0f);
            game_banner(ps, bs == 1 ? "A COLD BATH - REFRESHED: MANA FLOWS TWICE AS FAST" : "A WARM BATH - REFRESHED: MANA FLOWS TWICE AS FAST");
        }
        if (bs >= 2 && fmodf(g->time * 8.0f, 1.0f) < dt * 8.0f)
            particles_burst(g, v3_add(p, v3(0, 0.9f, 0)), v3(0.9f, 0.9f, 0.95f), 2, 0.6f, 0.08f, false);
    }
    /* the feast: Well Fed for 10, 20 or 30 minutes */
    int ds = sanctum_stage(g, SP_DINING);
    bool at_table = ds >= 1 && in_rect(p, POCKET_TABLE_X0, ds == 1 ? POCKET_TABLE_Z0_SHORT : POCKET_TABLE_Z0, POCKET_TABLE_X1,
                                       ds == 1 ? POCKET_TABLE_Z1_SHORT : POCKET_TABLE_Z1, 1.8f);
    if (at_table) {
        if (s->feast_t >= 0) {
            s->feast_t += dt;
            if (s->feast_t > 1.0f) {
                static const float FED[4] = { 0, 600.0f, 1200.0f, 1800.0f };
                s->feast_t = -1; /* until you step away */
                bool was_fed = s->well_fed_t > 0;
                s->well_fed_t = FED[ds];
                if (!was_fed) stats_refresh(ps);
                restore(ps);
                sfx_at(g, SFX_REALM_FEAST, p, 1.0f);
                game_banner(ps, ds == 1 ? "A SIMPLE MEAL - WELL FED: MORE HEALTH, AND IT COMES BACK BY ITSELF"
                                        : "A FEAST - WELL FED: MORE HEALTH, AND IT COMES BACK BY ITSELF");
                particles_burst(g, v3_add(p, v3(0, 1.2f, 0)), v3(1.0f, 0.8f, 0.4f), 18, 2.0f, 0.05f, true);
            }
        }
    } else s->feast_t = 0;
    /* the bed */
    if (in_rect(p, POCKET_BED_X0, POCKET_BED_Z0, POCKET_BED_X1, POCKET_BED_Z1, 1.2f)) {
        if (s->rest_t >= 0) {
            s->rest_t += dt;
            if (s->rest_t > 2.0f) {
                s->rest_t = -1;
                restore(ps);
                s->respawn = v3(POCKET_BED_X0 - 1.5f, (float)POCKET_G, (POCKET_BED_Z0 + POCKET_BED_Z1) * 0.5f + 0.5f);
                s->respawn_yaw = 0;
                g->autosave_timer = 1e9f;
                sfx_at(g, SFX_REALM_REST, p, 1.0f);
                game_banner(ps, "YOU REST - YOU WILL WAKE HERE. THE GAME IS SAVED");
            }
        }
    } else s->rest_t = 0;
}

/* The island's chunks generated from an older state of the Sanctum are made again. */
static void regenerate(Game *g) {
    for (int cy = POCKET_CY - 3; cy <= POCKET_CY + 1; cy++)
        for (int cz = 0; cz <= 3; cz++)
            for (int cx = 0; cx <= 3; cx++) {
                Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
                if (c && c->state == CH_READY && c->gen_id != g->world.sanctum_gen) chunkmap_regenerate(&g->map, cx, cy, cz);
            }
}

void realm_update(Game *g, float dt) {
    if (g->sanctum.library_t > 0) g->sanctum.library_t -= dt;
    bool any = false;
    for (int i = 0; i < MAX_PLAYERS; i++) any |= g->players[i].active && realm_has(g->players[i].pl.pos);
    if (any) regenerate(g);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->st.dead || !realm_has(ps->pl.pos)) continue;
        vec3 p = ps->pl.pos;
        float dx = p.x - POCKET_EXIT_X, dz = p.z - POCKET_EXIT_Z;
        if (ps->gate_cd <= 0 && dx * dx + dz * dz < 0.8f * 0.8f && fabsf(p.y - POCKET_G) < 1.6f) {
            leave(g, ps);
            continue;
        }
        if (p.y < POCKET_Y0 - 30) { /* fell off the island */
            move_to(g, ps, arrive_pos(), ARRIVE_YAW);
            continue;
        }
        /* something was just built where you stand: out to the court */
        if (!ps->pl.fly && game_solid(g, fl(p.x), fl(p.y + 0.5f), fl(p.z)) == 1 && game_solid(g, fl(p.x), fl(p.y + 1.5f), fl(p.z)) == 1) {
            move_to(g, ps, arrive_pos(), ARRIVE_YAW);
            continue;
        }
        comforts(g, ps, dt);
    }
}

/* Trophies: a legendary found, a named enemy killed. */
void sanctum_trophy_unique(Game *g, int id) {
    if (id >= 0 && id < UNIQUE_COUNT) g->sanctum.trophy[id] = 1;
}

void sanctum_trophy_named(Game *g, u64 key) {
    for (int i = 0; i < g->sanctum.nnamed; i++)
        if (g->sanctum.named[i] == key) return;
    if (g->sanctum.nnamed < MAX_TROPHY_NAMED) g->sanctum.named[g->sanctum.nnamed++] = key;
}

/* ---------- drawing ---------- */

static void plate(vec3 c, vec3 food, int kind, float t) {
    batch3d_box(c, v3(0.22f, 0.015f, 0.22f), 0, v3(0.85f, 0.83f, 0.78f), false);
    vec3 top = v3_add(c, v3(0, 0.02f, 0));
    switch (kind) {
    case 0: /* a roast */
        batch3d_box(v3_add(top, v3(0, 0.08f, 0)), v3(0.14f, 0.08f, 0.1f), 0.3f, food, false);
        batch3d_box(v3_add(top, v3(0.16f, 0.1f, 0)), v3(0.06f, 0.02f, 0.02f), 0.3f, v3(0.95f, 0.9f, 0.8f), false);
        break;
    case 1: /* bread */
        batch3d_box(v3_add(top, v3(0, 0.06f, 0)), v3(0.15f, 0.06f, 0.08f), 0.8f, v3(0.78f, 0.55f, 0.28f), false);
        break;
    case 2: /* fruit */
        for (int k = 0; k < 5; k++) {
            float a = k * 1.26f;
            vec3 col = k % 3 == 0 ? v3(0.8f, 0.15f, 0.12f) : k % 3 == 1 ? v3(0.95f, 0.75f, 0.2f) : v3(0.45f, 0.2f, 0.55f);
            batch3d_octa(v3_add(top, v3(cosf(a) * 0.09f, 0.05f, sinf(a) * 0.09f)), 0.05f, 0.05f, a, col, false);
        }
        break;
    default: /* a pie, steaming */
        batch3d_box(v3_add(top, v3(0, 0.04f, 0)), v3(0.16f, 0.04f, 0.16f), 0.0f, v3(0.8f, 0.6f, 0.3f), false);
        batch3d_box(v3_add(top, v3(0, 0.25f + fmodf(t * 0.3f, 0.3f), 0)), v3(0.03f, 0.03f, 0.03f), t, v3(0.9f, 0.9f, 0.9f), true);
        break;
    }
}

static void feast(Game *g) {
    float y = POCKET_G + 1.0f;
    float cx = (POCKET_TABLE_X0 + POCKET_TABLE_X1 + 1) * 0.5f;
    bool small = sanctum_stage(g, SP_DINING) == 1;
    int z0 = small ? POCKET_TABLE_Z0_SHORT : POCKET_TABLE_Z0, z1 = small ? POCKET_TABLE_Z1_SHORT : POCKET_TABLE_Z1;
    int n = 0;
    for (int z = z0 + 1; z <= z1 - 1; z += 2, n++) {
        vec3 l = game_light_at(g, v3(cx, y + 0.5f, z + 0.5f));
        l = v3(fmaxf(l.x, 0.5f), fmaxf(l.y, 0.45f), fmaxf(l.z, 0.4f));
        for (int side = -1; side <= 1; side += 2) {
            vec3 c = v3(cx + side * 1.6f, y + 0.02f, z + 0.5f);
            plate(c, v3(0.55f * l.x, 0.3f * l.y, 0.15f * l.z), (n + (side > 0)) % 4, g->time + z);
            /* a goblet */
            vec3 gb = v3_add(c, v3(-side * 0.4f, 0, 0.3f));
            batch3d_box(v3_add(gb, v3(0, 0.08f, 0)), v3(0.02f, 0.08f, 0.02f), 0, v3(0.9f, 0.75f, 0.35f), false);
            batch3d_box(v3_add(gb, v3(0, 0.2f, 0)), v3(0.06f, 0.05f, 0.06f), 0, v3(0.9f, 0.75f, 0.35f), false);
            batch3d_box(v3_add(gb, v3(0, 0.24f, 0)), v3(0.05f, 0.01f, 0.05f), 0, v3(0.5f, 0.05f, 0.1f), false);
        }
        /* candles down the middle */
        if (n % 3 == 0) {
            vec3 cd = v3(cx, y, z + 0.5f);
            batch3d_box(v3_add(cd, v3(0, 0.15f, 0)), v3(0.03f, 0.15f, 0.03f), 0, v3(0.95f, 0.92f, 0.85f), false);
            float fl = 0.04f + 0.01f * sinf(g->time * 13.0f + z);
            batch3d_octa(v3_add(cd, v3(0, 0.35f, 0)), fl, fl * 1.8f, g->time, v3(1.4f, 0.9f, 0.4f), true);
        }
    }
}

static vec3 lit(Game *g, vec3 p, vec3 col) {
    vec3 l = game_light_at(g, p);
    l = v3(fmaxf(l.x, 0.35f), fmaxf(l.y, 0.33f), fmaxf(l.z, 0.35f));
    return v3(col.x * l.x, col.y * l.y, col.z * l.z);
}

/* the armory's racks, banners and statues */
static void trophies(Game *g) {
    int as = sanctum_stage(g, SP_ARMORY);
    if (as < 1 || sanctum_stage(g, SP_ISLAND) < 2) return;
    int x0 = plot_x(SP_ARMORY), z0 = plot_z(SP_ARMORY);
    int racks = as >= 2 ? 20 : 8, k = 0;
    for (int id = 0; id < UNIQUE_COUNT && k < racks; id++) {
        if (!g->sanctum.trophy[id]) continue;
        int slot = k++, wall = slot % 2, i = slot / 2;
        float x = x0 + 4 + i * 2 + (i >= 5 ? 4 : 0) + 0.5f;
        float z = wall ? z0 + 28.4f : z0 + 3.6f;
        Item it = arms_unique(id, 10);
        vec3 hand = v3(x, (float)POCKET_G + 0.5f, z);
        weapon_draw_held(&it, hand, v3(0, 1, 0), v3(1, 0, 0), game_light_at(g, v3_add(hand, v3(0, 1, 0))), g->time, (float)id);
    }
    if (as >= 2) /* the banners of the named enemies killed, on the east wall */
        for (int i = 0; i < g->sanctum.nnamed && i < 12; i++) {
            u64 h = mix64(g->sanctum.named[i]);
            vec3 col = v3(0.3f + (h & 255) / 400.0f, 0.15f + ((h >> 8) & 255) / 600.0f, 0.15f + ((h >> 16) & 255) / 500.0f);
            float z = z0 + 4.5f + i * 2.0f, x = x0 + 28.6f;
            vec3 top = v3(x, (float)POCKET_G + 5.5f, z);
            batch3d_box(top, v3(0.05f, 0.05f, 0.7f), 0, lit(g, top, v3(0.4f, 0.3f, 0.2f)), false);
            batch3d_box(v3_add(top, v3(0, -1.3f, 0)), v3(0.03f, 1.25f, 0.6f), 0, lit(g, top, col), false);
            batch3d_box(v3_add(top, v3(-0.04f, -1.2f, 0)), v3(0.01f, 0.25f, 0.25f), 0.785f, lit(g, top, v3(0.9f, 0.8f, 0.4f)), false);
        }
    if (as >= 3) { /* a statue for every chapter finished */
        int ch = 0;
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (g->players[i].active) ch = MAXI(ch, g->players[i].st.chapter);
        for (int i = 0; i < ch && i < 6; i++) {
            vec3 p = v3(x0 + 26.0f, (float)POCKET_G + 1.0f, z0 + 7.0f + i * 4.0f);
            vec3 stone = lit(g, v3_add(p, v3(0, 1, 0)), v3(0.75f, 0.73f, 0.7f));
            batch3d_box(v3_add(p, v3(0, 0.45f, 0)), v3(0.22f, 0.45f, 0.18f), 0, stone, false);
            batch3d_box(v3_add(p, v3(0, 1.15f, 0)), v3(0.26f, 0.3f, 0.2f), 0, stone, false);
            batch3d_box(v3_add(p, v3(0, 1.6f, 0)), v3(0.13f, 0.14f, 0.13f), 0, stone, false);
            batch3d_spike(v3_add(p, v3(-0.3f, 1.2f, 0)), v3(0, 1, 0), 0.04f, 1.0f, stone, false);
        }
    }
}

void realm_draw(Game *g) {
    PlayerSlot *v = g->draw_viewer;
    if (v && !realm_has(game_view_eye(v))) return;
    utility_draw_door(g, v3(POCKET_EXIT_X, (float)POCKET_G, POCKET_EXIT_Z), ARRIVE_YAW, 1.0f, true);
    if (sanctum_stage(g, SP_DINING) >= 1) feast(g);
    trophies(g);
    /* the drafting table: plans and an inkwell */
    vec3 d = v3(POCKET_DESK_X + 1.0f, (float)POCKET_G + 1.02f, POCKET_DESK_Z + 1.0f);
    batch3d_box(d, v3(0.6f, 0.01f, 0.45f), 0.2f, lit(g, d, v3(0.85f, 0.8f, 0.65f)), false);
    batch3d_box(v3_add(d, v3(0.3f, 0.02f, 0.2f)), v3(0.25f, 0.005f, 0.004f), 0.9f, lit(g, d, v3(0.3f, 0.25f, 0.4f)), false);
    batch3d_box(v3_add(d, v3(-0.45f, 0.06f, -0.3f)), v3(0.05f, 0.06f, 0.05f), 0, lit(g, d, v3(0.15f, 0.15f, 0.2f)), false);
}
