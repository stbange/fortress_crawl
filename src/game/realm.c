#include "game/game.h"

#include "render/batch.h"

/* The pocket realm behind the Sanctum Door: a castle on a floating island (gen_pocket.c
 * builds it). Here are its doors and its comforts:
 *   the bath (stand in the pool): wounds close, burns and poison wash off, and you come
 *     out Refreshed (mana flows twice as fast for 15 minutes);
 *   the feast (stand by the long table): everything restored and Well Fed (more health,
 *     health comes back by itself, for 20 minutes);
 *   the bed (stand by it for a moment): everything restored, you will wake here when you
 *     fall, and the game is saved.
 * The door in the garden takes you back to where you opened the Sanctum Door. */

#define BATH_REFRESH 900.0f
#define FEAST_FED 1200.0f

static int fl(float v) { return (int)floorf(v); }

bool realm_has(const vec3 pos) { return world_is_pocket(fl(pos.x / 32.0f), fl(pos.y / 32.0f), fl(pos.z / 32.0f)); }

static vec3 arrive_pos(void) { return v3(POCKET_ARRIVE_X, (float)POCKET_G, POCKET_ARRIVE_Z); }

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
    move_to(g, ps, arrive_pos(), PI_F * 0.5f);
    sfx_at(g, SFX_REALM_DOOR, ps->pl.pos, 1.0f);
    if ((s->sanctum & (SANCTUM_BATH | SANCTUM_FEAST)) == (SANCTUM_BATH | SANCTUM_FEAST))
        game_banner(ps, "THE SANCTUM: BATH TO THE WEST, FEAST TO THE EAST, BED UP NORTH-EAST");
    else
        game_banner(ps, "THE SANCTUM: YOUR BED IS MADE. THE LIBRARY'S SHRINE RESTORES THE REST, FOR GOLD");
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
    /* the bath: feet in the pool */
    bool in_bath = in_rect(p, POCKET_BATH_X0, POCKET_BATH_Z0, POCKET_BATH_X1, POCKET_BATH_Z1, 0) && p.y < POCKET_G - 0.2f;
    if (in_bath && !(s->sanctum & SANCTUM_BATH) && ps->banner_time <= 0)
        game_banner(ps, "THE WATER IS COLD AND STILL - RESTORE THE BATH AT THE LIBRARY SHRINE");
    if (in_bath && (s->sanctum & SANCTUM_BATH)) {
        s->hp = fminf(s->maxhp, s->hp + s->maxhp * 0.2f * dt);
        s->burn_t = s->poison_t = s->chill_t = 0;
        if (s->refreshed_t < BATH_REFRESH - 60.0f) {
            s->refreshed_t = BATH_REFRESH;
            sfx_at(g, SFX_REALM_BATH, p, 1.0f);
            game_banner(ps, "A WARM BATH - REFRESHED: MANA FLOWS TWICE AS FAST");
        }
        if (fmodf(g->time * 8.0f, 1.0f) < dt * 8.0f)
            particles_burst(g, v3_add(p, v3(0, 0.9f, 0)), v3(0.9f, 0.9f, 0.95f), 2, 0.6f, 0.08f, false);
    }
    /* the feast */
    bool at_table = in_rect(p, POCKET_TABLE_X0, POCKET_TABLE_Z0, POCKET_TABLE_X1, POCKET_TABLE_Z1, 1.8f);
    if (at_table && !(s->sanctum & SANCTUM_FEAST) && ps->banner_time <= 0)
        game_banner(ps, "AN EMPTY TABLE - STOCK THE DINING HALL AT THE LIBRARY SHRINE");
    if (at_table && (s->sanctum & SANCTUM_FEAST)) {
        if (s->feast_t >= 0) {
            s->feast_t += dt;
            if (s->feast_t > 1.0f) {
                s->feast_t = -1; /* until you step away */
                bool was_fed = s->well_fed_t > 0;
                s->well_fed_t = FEAST_FED;
                if (!was_fed) stats_refresh(ps);
                restore(ps);
                sfx_at(g, SFX_REALM_FEAST, p, 1.0f);
                game_banner(ps, "A FEAST - WELL FED: MORE HEALTH, AND IT COMES BACK BY ITSELF");
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

void realm_update(Game *g, float dt) {
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
            move_to(g, ps, arrive_pos(), PI_F * 0.5f);
            continue;
        }
        comforts(g, ps, dt);
    }
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
    int n = 0;
    for (int z = POCKET_TABLE_Z0 + 1; z <= POCKET_TABLE_Z1 - 1; z += 2, n++) {
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

void realm_draw(Game *g) {
    PlayerSlot *v = g->draw_viewer;
    if (v && !realm_has(game_view_eye(v))) return;
    utility_draw_door(g, v3(POCKET_EXIT_X, (float)POCKET_G, POCKET_EXIT_Z), PI_F * 0.5f, 1.0f, true);
    if (!v || (v->st.sanctum & SANCTUM_FEAST)) feast(g);
}
