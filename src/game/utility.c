#include <ctype.h>
#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* The utility spells beyond the first seven (spells.c casts those): moving around (levitate,
 * wings, feather fall, spider climb, strider's grace, phase step, anchor, twin gates,
 * stairway), safe places (warden's keep, wall, hut, sanctuary, mirror image, stoneskin),
 * knowing (clairvoyance, reveal, map of the mind, dowsing, speak with the dead), bending
 * monsters (invisibility, lure, charm, fear, time slip, gravity well), changing the world
 * (stone shape, bridge of light, quench, steed) and the Sanctum Door (realm.c).
 *
 * Walls, keeps, huts, bridges, tunnels and ice are real blocks for a while: a list of
 * timed edits puts them into loaded chunks (and again when a chunk comes back) and takes
 * them out when their time is up, once nobody stands in the way. */

static float rank_k(int rank) { return 1.0f + 0.2f * (float)(rank - 1); }

/* ---------- collision ---------- */

static PlayerSlot *g_phys_ps;

void utility_physics_player(PlayerSlot *ps) { g_phys_ps = ps; }

int player_solid(void *user, int x, int y, int z) {
    Game *g = user;
    int s = game_solid(g, x, y, z);
    if (s == 0 && g_phys_ps && g_phys_ps->st.stride_t > 0) {
        bool known;
        Block b = chunkmap_block(&g->map, x, y, z, &known);
        if (known && (b == B_WATER || b == B_LAVA)) { /* water and lava hold a strider, at their surface */
            Block above = chunkmap_block(&g->map, x, y + 1, z, &known);
            if (above != B_WATER && above != B_LAVA) return 1;
        }
    }
    return s;
}

bool utility_barrier(const Game *g, int x, int y, int z) {
    for (int i = 0; i < g->nbarriers; i++)
        if (g->barriers[i].x == x && g->barriers[i].y == y && g->barriers[i].z == z && g->barriers[i].until > g->time) return true;
    return false;
}

float utility_time_scale(const Game *g) { return g->timeslip_t > 0 ? 0.25f : 1.0f; }

void utility_player_attacked(PlayerSlot *ps) { ps->st.invis_t = 0; }

vec3 game_view_eye(const PlayerSlot *ps) { return ps->st.scry_t > 0 ? ps->scry_pos : player_eye(&ps->pl); }

void utility_player_physics(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    Player *p = &ps->pl;
    p->move_mode = s->wings_t > 0 ? MOVE_FLY : s->levitate_t > 0 ? MOVE_LEVITATE : MOVE_WALK;
    p->max_fall = s->feather_t > 0 || s->glide_t > 0 ? 3.0f : food_fx(s, ME_GLIDE) > 0 && p->vel.y < 0 && p->vel.y > -9.0f ? 5.0f : 40.0f;
    p->climbing = s->climb_t > 0;
    p->jump_mult = (s->steed_t > 0 ? 1.5f : 1.0f) * (1.0f + food_fx(s, ME_JUMP));
    (void)g;
}

/* ---------- timed world edits ---------- */

static bool cell_occupied(Game *g, int x, int y, int z) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        vec3 f = ps->pl.pos;
        float h = player_height(ps->pl.stance);
        if (f.x + PLAYER_HALF_W > x && f.x - PLAYER_HALF_W < x + 1 && f.z + PLAYER_HALF_W > z && f.z - PLAYER_HALF_W < z + 1 && f.y < y + 1 &&
            f.y + h > y)
            return true;
    }
    for (int i = 0; i < MAX_MONSTERS; i++) {
        const Monster *m = &g->mons[i];
        if (!m->alive) continue;
        float hw = MONSTERS[m->type].hw * m->scale, h = MONSTERS[m->type].h * m->scale;
        if (m->pos.x + hw > x && m->pos.x - hw < x + 1 && m->pos.z + hw > z && m->pos.z - hw < z + 1 && m->pos.y < y + 1 && m->pos.y + h > y)
            return true;
    }
    return false;
}

/* Asks for cell (x,y,z) to be `to` for `life` seconds, if it is `from` now. */
static bool edit_add(Game *g, int x, int y, int z, Block from, Block to, float life) {
    bool known;
    Block b = chunkmap_block(&g->map, x, y, z, &known);
    if (!known || b != from) return false;
    if (block_solid(to) && cell_occupied(g, x, y, z)) return false; /* never shut anyone in */
    for (int i = 0; i < g->nedits; i++)
        if (g->edits[i].x == x && g->edits[i].y == y && g->edits[i].z == z) return false;
    if (g->nedits >= MAX_EDITS) return false;
    g->edits[g->nedits++] = (WorldEdit){ x, y, z, from, to, g->time + life };
    return true;
}

static bool place(Game *g, int x, int y, int z, Block b, float life) { return edit_add(g, x, y, z, B_AIR, b, life); }

static void edits_update(Game *g) {
    for (int i = 0; i < g->nedits;) {
        WorldEdit *e = &g->edits[i];
        bool known;
        Block b = chunkmap_block(&g->map, e->x, e->y, e->z, &known);
        bool remove = false;
        if (g->time < e->until) {
            /* in place (again, if its chunk was made anew) */
            if (known && b == e->from && b != e->to && !(block_solid(e->to) && cell_occupied(g, e->x, e->y, e->z)))
                chunkmap_set_block(&g->map, e->x, e->y, e->z, e->to);
        } else if (!known || b != e->to) {
            remove = true; /* nothing to undo */
        } else if (!(block_solid(e->from) && cell_occupied(g, e->x, e->y, e->z))) {
            remove = chunkmap_set_block(&g->map, e->x, e->y, e->z, e->from);
        }
        if (remove) g->edits[i] = g->edits[--g->nedits];
        else i++;
    }
    for (int i = 0; i < g->nbarriers;) {
        if (g->barriers[i].until <= g->time) g->barriers[i] = g->barriers[--g->nbarriers];
        else i++;
    }
}

/* ---------- helpers ---------- */

static vec3 feet_of(const PlayerSlot *ps) { return ps->pl.pos; }
static vec3 flat_dir(float yaw) { return v3(cosf(yaw), 0, sinf(yaw)); }

/* A spot to stand near p: the cell or a few above or below with ground under it. */
static bool stand_spot(Game *g, vec3 p, vec3 *out) {
    int x = (int)floorf(p.x), z = (int)floorf(p.z), y0 = (int)floorf(p.y);
    for (int k = 0; k < 8; k++) {
        int y = y0 + (k % 2 ? -(k + 1) / 2 : k / 2);
        if (game_solid(g, x, y - 1, z) == 1 && game_solid(g, x, y, z) == 0 && game_solid(g, x, y + 1, z) == 0) {
            *out = v3(x + 0.5f, (float)y, z + 0.5f);
            return true;
        }
    }
    return false;
}

/* Where the player aims, up to range (on the floor below if to_floor). */
static vec3 aim_at(Game *g, PlayerSlot *ps, float range, bool to_floor) {
    vec3 eye = player_eye(&ps->pl), d = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    float t = phys_raycast(eye, d, range, game_solid, g);
    vec3 p = v3_add(eye, v3_scale(d, fmaxf(0.0f, t - 0.4f)));
    if (to_floor) {
        vec3 f;
        if (stand_spot(g, p, &f)) return f;
    }
    return p;
}

static void teleport(Game *g, PlayerSlot *ps, vec3 to) {
    vec3 c = player_eye(&ps->pl);
    particles_burst(g, c, v3(0.7f, 0.6f, 1.0f), 16, 3.0f, 0.05f, true);
    ps->pl.pos = to;
    ps->pl.vel = v3(0, 0, 0);
    ps->st.fall_speed = 0;
    particles_burst(g, player_eye(&ps->pl), v3(0.7f, 0.6f, 1.0f), 16, 3.0f, 0.05f, true);
    flash_add(g, player_eye(&ps->pl), v3(0.7f, 0.6f, 1.2f), 6.0f, 0.3f);
}

static Effect *owned(Game *g, int type, int owner, int skip) {
    for (int i = 0; i < MAX_EFFECTS; i++)
        if (g->effects[i].alive && g->effects[i].type == type && g->effects[i].owner == owner && i != skip) return &g->effects[i];
    return NULL;
}

/* A building of real blocks round (cx, y0, cz): walls r from the centre, h high, a roof,
 * a doorway on the side facing yaw. Returns the number of blocks placed. */
static int raise_building(Game *g, int cx, int y0, int cz, int r, int h, float yaw, float life, bool roof, bool barrier, Block wall) {
    int n = 0;
    /* the doorway: the wall cell the yaw points at, and its neighbours for a wide one */
    int dx = (int)lroundf(cosf(yaw)), dz = (int)lroundf(sinf(yaw));
    if (dx && dz) dz = 0;
    for (int z = cz - r; z <= cz + r; z++)
        for (int x = cx - r; x <= cx + r; x++) {
            bool ring = abs(x - cx) == r || abs(z - cz) == r;
            if (!ring) continue;
            bool door = (dx && x == cx + dx * r && abs(z - cz) <= (r >= 4 ? 1 : 0)) || (dz && z == cz + dz * r && abs(x - cx) <= (r >= 4 ? 1 : 0));
            for (int y = y0; y < y0 + h; y++) {
                if (door && y < y0 + (r >= 4 ? 3 : 2)) {
                    if (barrier && g->nbarriers < 64) g->barriers[g->nbarriers++].x = x, g->barriers[g->nbarriers - 1].y = y,
                                                       g->barriers[g->nbarriers - 1].z = z, g->barriers[g->nbarriers - 1].until = g->time + life;
                    continue;
                }
                n += place(g, x, y, z, wall, life);
            }
            if (r >= 4 && ((x + z) & 1)) n += place(g, x, y0 + h + 1, z, wall, life); /* crenellations */
        }
    if (roof)
        for (int z = cz - r; z <= cz + r; z++)
            for (int x = cx - r; x <= cx + r; x++) n += place(g, x, y0 + h, z, B_ROOF, life);
    return n;
}

/* ---------- casting ---------- */

static bool fail(PlayerSlot *ps, const char *why) {
    game_banner(ps, why);
    return false;
}

static bool cast_phase(Game *g, PlayerSlot *ps, int blocks) {
    vec3 d = flat_dir(ps->pl.yaw), f = feet_of(ps);
    float h = player_height(STANCE_STAND), wall = -1;
    for (float t = 0.4f; t <= 2.5f; t += 0.2f)
        if (!phys_box_free(v3_add(f, v3_scale(d, t)), PLAYER_HALF_W, h, game_solid, g)) { wall = t; break; }
    if (wall < 0) return fail(ps, "NO WALL TO STEP THROUGH");
    for (float t = wall + 0.5f; t <= wall + blocks + 1.2f; t += 0.25f)
        for (int dy = 0; dy <= 2; dy++) {
            int yy = dy == 2 ? -1 : dy;
            vec3 q = v3_add(v3_add(f, v3_scale(d, t)), v3(0, (float)yy, 0));
            q.y = floorf(q.y + 0.01f);
            if (phys_box_free(q, PLAYER_HALF_W, h, game_solid, g) && game_solid(g, (int)floorf(q.x), (int)q.y - 1, (int)floorf(q.z)) == 1) {
                teleport(g, ps, q);
                return true;
            }
        }
    return fail(ps, "THE WALL IS TOO THICK");
}

static bool cast_stairway(Game *g, PlayerSlot *ps) {
    int dir = ps->pl.pitch > 0.15f ? 1 : ps->pl.pitch < -0.15f ? -1 : 0;
    if (!dir) return fail(ps, "LOOK UP OR DOWN TO CHOOSE THE WAY");
    vec3 f = feet_of(ps);
    int layer = layer_of(f.y) + dir;
    int y0 = layer * CHUNK + 2, y1 = y0 + CHUNK - 3;
    int fx = (int)floorf(f.x), fz = (int)floorf(f.z);
    for (int r = 0; r <= 14; r++)
        for (int dz = -r; dz <= r; dz++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dz) != r) continue;
                int x = fx + dx, z = fz + dz;
                for (int y = y0; y <= y1; y++)
                    if (game_solid(g, x, y - 1, z) == 1 && game_solid(g, x, y, z) == 0 && game_solid(g, x, y + 1, z) == 0 &&
                        game_solid(g, x, y + 2, z) == 0) {
                        /* a column of light marks the way */
                        for (int k = 0; k < 24; k++) particles_burst(g, v3(f.x, f.y + k * 0.5f * dir, f.z), v3(1.0f, 0.95f, 0.7f), 1, 0.4f, 0.06f, true);
                        teleport(g, ps, v3(x + 0.5f, (float)y, z + 0.5f));
                        return true;
                    }
            }
    return fail(ps, dir > 0 ? "NO WAY UP NEAR HERE" : "NO WAY DOWN NEAR HERE");
}

static bool cast_charm(Game *g, PlayerSlot *ps, int rank) {
    vec3 eye = player_eye(&ps->pl), look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    Monster *best = NULL;
    float bc = cosf(8.0f * PI_F / 180.0f);
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || m->hidden) continue;
        vec3 c = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0)), to = v3_sub(c, eye);
        float d = v3_len(to);
        if (d > 16.0f || d < 0.3f) continue;
        float cs = v3_dot(v3_scale(to, 1.0f / d), look);
        if (cs > bc && phys_line_of_sight(eye, c, game_solid, g)) { bc = cs; best = m; }
    }
    if (!best) return fail(ps, "NO MONSTER IN YOUR SIGHT");
    if (best->grade == GRADE_BOSS || best->named_kind) return fail(ps, "TOO STRONG A WILL TO CHARM");
    best->charm_t = 30.0f * rank_k(rank);
    best->charmer = (i16)ps->index;
    best->awake = true;
    best->state = MS_CHASE;
    particles_burst(g, v3_add(best->pos, v3(0, MONSTERS[best->type].h * best->scale, 0)), v3(1.0f, 0.5f, 0.85f), 14, 2.0f, 0.05f, true);
    return true;
}

static bool cast_speakdead(Game *g, PlayerSlot *ps) {
    const Pickup *tr = NULL;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        const Pickup *p = &g->pickups[i];
        if (p->alive && p->kind == PK_TRACE && p->amount != TR_BOUNTY && v3_len(v3_sub(p->pos, ps->pl.pos)) < 5.0f) tr = p;
    }
    if (!tr) return fail(ps, "NO DEAD REST NEAR HERE");
    char dir[64], buf[160];
    if (ps->st.motive && ps->target_chapter == ps->st.chapter + 1) {
        story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, ps->target.cx, ps->target.cy, ps->target.cz, dir, sizeof dir);
        snprintf(buf, sizeof buf, "THE DEAD WHISPER: %s IS %s", ps->target.name, dir);
    } else {
        /* no quarry yet: the nearest sealed vault */
        const MechVault *best = NULL;
        float bd = 1e9f;
        for (int i = 0; i < MAX_VAULTS; i++) {
            const MechVault *v = &g->vaults[i];
            float d = v3_len(v3_sub(v3((float)v->gx, (float)v->y, (float)v->gz), ps->pl.pos));
            if (v->alive && !v->solved && d < bd) { bd = d; best = v; }
        }
        if (!best) return fail(ps, "THE DEAD HAVE NOTHING TO TELL");
        story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, floordiv(best->gx, CHUNK), floordiv(best->y, CHUNK),
                        floordiv(best->gz, CHUNK), dir, sizeof dir);
        snprintf(buf, sizeof buf, "THE DEAD WHISPER: A SEALED HOARD %s", dir);
    }
    for (char *c = buf; *c; c++) *c = (char)toupper((unsigned char)*c);
    game_banner(ps, buf);
    ps->banner_time = 7.0f;
    particles_burst(g, v3_add(tr->pos, v3(0, 0.6f, 0)), v3(0.6f, 0.9f, 0.8f), 18, 1.5f, 0.05f, true);
    return true;
}

bool utility_cast(Game *g, PlayerSlot *ps, int u, int rank) {
    Stats *s = &ps->st;
    float k = rank_k(rank);
    vec3 f = feet_of(ps), eye = player_eye(&ps->pl), fwd = flat_dir(ps->pl.yaw);
    int fx = (int)floorf(f.x), fy = (int)floorf(f.y + 0.01f), fz = (int)floorf(f.z);
    switch (u) {
    /* ----- moving around ----- */
    case SU_LEVITATE:
        s->levitate_t = 15.0f * k;
        s->wings_t = 0;
        ps->pl.vel.y = fmaxf(ps->pl.vel.y, 2.0f);
        particles_burst(g, f, v3(0.8f, 0.9f, 1.0f), 14, 2.0f, 0.04f, true);
        return true;
    case SU_WINGS:
        s->wings_t = 20.0f * k;
        s->levitate_t = 0;
        particles_burst(g, eye, v3(0.9f, 0.95f, 1.0f), 24, 3.0f, 0.05f, true);
        return true;
    case SU_FEATHER: s->feather_t = 30.0f * k; return true;
    case SU_CLIMB: s->climb_t = 20.0f * k; return true;
    case SU_STRIDE: s->stride_t = 30.0f * k; return true;
    case SU_PHASE: return cast_phase(g, ps, rank >= 3 ? 4 : 3);
    case SU_ANCHOR: {
        Effect *a = owned(g, EF_ANCHOR, ps->index, -1);
        if (a) {
            if (v3_len(v3_sub(a->pos, f)) > 400.0f) return fail(ps, "THE ANCHOR IS TOO FAR AWAY");
            teleport(g, ps, a->pos);
            a->alive = false;
            return true;
        }
        a = effect_new(g, EF_ANCHOR, EL_ARCANE, ps->index, f, 600.0f * k, 0, 0.6f);
        if (!a) return false;
        game_banner(ps, "ANCHOR SET - CAST AGAIN TO RETURN HERE");
        return true;
    }
    case SU_GATES: {
        Effect *a = owned(g, EF_GATE, ps->index, -1);
        Effect *b = a ? owned(g, EF_GATE, ps->index, (int)(a - g->effects)) : NULL;
        vec3 at;
        if (!stand_spot(g, v3_add(f, v3_scale(fwd, 2.0f)), &at)) at = f;
        if (a && !b && a->link < 0) {
            Effect *nb = effect_new(g, EF_GATE, EL_ARCANE, ps->index, at, 60.0f * k, 0, 0.8f);
            if (!nb) return false;
            nb->yaw = ps->pl.yaw + PI_F;
            nb->link = (int)(a - g->effects);
            a->link = (int)(nb - g->effects);
            a->t = 0;
            a->life = nb->life;
            game_banner(ps, "THE GATES ARE LINKED");
            return true;
        }
        if (a) a->alive = false;
        if (b) b->alive = false;
        Effect *na = effect_new(g, EF_GATE, EL_ARCANE, ps->index, at, 120.0f, 0, 0.8f);
        if (!na) return false;
        na->yaw = ps->pl.yaw + PI_F;
        game_banner(ps, "FIRST GATE - CAST AGAIN FOR THE SECOND");
        return true;
    }
    case SU_STAIRWAY: return cast_stairway(g, ps);

    /* ----- defence and safe places ----- */
    case SU_KEEP: {
        if (realm_has(f)) return fail(ps, "YOU ARE ALREADY HOME");
        float life = 180.0f * k;
        int n = raise_building(g, fx, fy, fz, 5, 4, ps->pl.yaw, life, true, true, B_WALL);
        /* a hearth at the back, lamps in the corners */
        vec3 back = v3_sub(f, v3_scale(fwd, 3.0f));
        int hx = (int)floorf(back.x), hz = (int)floorf(back.z);
        place(g, hx, fy, hz, B_BRAZIER, life);
        place(g, hx, fy + 1, hz, B_FIRE, life);
        for (int a = -1; a <= 1; a += 2)
            for (int b = -1; b <= 1; b += 2) place(g, fx + a * 4, fy + 3, fz + b * 4, B_LANTERN, life);
        effect_new(g, EF_HEARTH, EL_FIRE, ps->index, v3(fx + 0.5f, (float)fy, fz + 0.5f), life, 1, 4.6f);
        s->respawn = f;
        s->respawn_yaw = ps->pl.yaw;
        g->autosave_timer = 1e9f;
        game_banner(ps, n > 20 ? "WARDEN'S KEEP - MONSTERS STAY OUT, YOUR SHRINE FOR NOW" : "NO ROOM FOR A KEEP - BUT THE HEARTH BURNS");
        sfx_at(g, SFX_GOLEM_STOMP, f, 0.6f);
        return true;
    }
    case SU_WALL: {
        vec3 c = v3_add(f, v3_scale(fwd, 2.6f));
        vec3 side = v3(-fwd.z, 0, fwd.x);
        int n = 0;
        for (int i = -2; i <= 2; i++)
            for (int y = 0; y < 3; y++) {
                vec3 q = v3_add(c, v3_scale(side, (float)i));
                n += place(g, (int)floorf(q.x), fy + y, (int)floorf(q.z), B_WALL, 30.0f * k);
            }
        if (!n) return fail(ps, "NO ROOM FOR A WALL");
        sfx_at(g, SFX_GOLEM_STOMP, c, 0.5f);
        return true;
    }
    case SU_HUT: {
        float life = 60.0f * k;
        raise_building(g, fx, fy, fz, 2, 3, ps->pl.yaw, life, true, false, B_WOOD);
        place(g, fx - (int)lroundf(fwd.x), fy, fz - (int)lroundf(fwd.z), B_FIRE, life);
        effect_new(g, EF_HEARTH, EL_FIRE, ps->index, v3(fx + 0.5f, (float)fy, fz + 0.5f), life, 0, 2.6f);
        return true;
    }
    case SU_SANCTUARY: return effect_new(g, EF_SANCTUARY, EL_HOLY, ps->index, f, 20.0f * k, 0, 3.5f + 0.5f * rank) != NULL;
    case SU_MIRROR: {
        int n = 2 + rank / 2, made = 0;
        for (int i = 0; i < n; i++) {
            float a = ps->pl.yaw + (i + 0.5f) * 2.0f * PI_F / n;
            vec3 q = v3_add(f, v3(cosf(a) * 2.2f, 0, sinf(a) * 2.2f));
            if (!phys_box_free(q, PLAYER_HALF_W, 1.75f, game_solid, g)) q = v3_add(f, v3(cosf(a) * 0.9f, 0, sinf(a) * 0.9f));
            Effect *e = effect_new(g, EF_DECOY, EL_ARCANE, ps->index, q, 20.0f * k, 1, 0.5f);
            if (e) { e->yaw = ps->pl.yaw; made++; }
        }
        particles_burst(g, eye, v3(0.8f, 0.8f, 1.0f), 20, 3.0f, 0.05f, true);
        return made > 0;
    }
    case SU_STONESKIN: s->stoneskin_t = 20.0f * k; return true;

    /* ----- seeing and knowing ----- */
    case SU_SCRY:
        if (s->scry_t > 0) { s->scry_t = 0; return false; } /* casting again ends it */
        s->scry_t = 20.0f * k;
        ps->scry_pos = eye;
        return true;
    case SU_REVEAL: s->reveal_t = 30.0f * k; return true;
    case SU_MAPMIND:
        explore_reveal_area(g, f, 2 + rank / 2);
        game_banner(ps, "THE PLACE UNFOLDS IN YOUR MIND - SEE THE MAP");
        return true;
    case SU_DOWSE: s->dowse_t = 20.0f * k; return true;
    case SU_SPEAKDEAD: return cast_speakdead(g, ps);

    /* ----- bending monsters ----- */
    case SU_INVIS:
        s->invis_t = 15.0f * k;
        for (int i = 0; i < MAX_MONSTERS; i++)
            if (g->mons[i].alive && g->mons[i].target == ps->index && v3_len(v3_sub(g->mons[i].pos, f)) > 2.0f) g->mons[i].awake = false;
        return true;
    case SU_LURE: return effect_new(g, EF_LURE, EL_HOLY, ps->index, aim_at(g, ps, 12.0f, true), 10.0f * k, 0, 18.0f) != NULL;
    case SU_CHARM: return cast_charm(g, ps, rank);
    case SU_FEAR: {
        int n = 0;
        for (int i = 0; i < MAX_MONSTERS; i++) {
            Monster *m = &g->mons[i];
            if (!m->alive || v3_len(v3_sub(m->pos, f)) > 9.0f) continue;
            m->fear_t = 6.0f * k * (m->grade == GRADE_BOSS ? 0.5f : 1.0f);
            m->awake = true;
            n++;
        }
        flash_add(g, eye, v3(0.6f, 0.2f, 0.8f), 9.0f, 0.4f);
        particles_burst(g, eye, v3(0.5f, 0.2f, 0.7f), 26, 6.0f, 0.06f, true);
        return n > 0 || true;
    }
    case SU_TIMESLIP:
        g->timeslip_t = 4.0f * k;
        flash_add(g, eye, v3(0.5f, 0.7f, 1.3f), 12.0f, 0.6f);
        return true;
    case SU_GRAVWELL: return effect_new(g, EF_GRAVWELL, EL_ARCANE, ps->index, aim_at(g, ps, 14.0f, false), 5.0f * k, 0, 7.0f) != NULL;

    /* ----- changing the world ----- */
    case SU_STONESHAPE: {
        int n = 0, seen = 0;
        for (float t = 0.8f; t < 9.0f && seen < 5; t += 0.5f) {
            vec3 q = v3_add(f, v3_scale(fwd, t));
            int x = (int)floorf(q.x), z = (int)floorf(q.z);
            bool any = false;
            for (int y = fy; y <= fy + 1; y++) {
                bool known;
                Block b = chunkmap_block(&g->map, x, y, z, &known);
                if (!known || !block_solid(b) || b == B_SHRINE || b == B_LAVA || b == B_WATER || mech_solid(g, x, y, z)) continue;
                n += edit_add(g, x, y, z, b, B_AIR, 180.0f * k);
                any = true;
            }
            if (any) seen++;
        }
        if (!n) return fail(ps, "NO STONE AHEAD TO SHAPE");
        particles_burst(g, v3_add(eye, v3_scale(fwd, 1.5f)), v3(0.6f, 0.55f, 0.45f), 24, 3.0f, 0.08f, false);
        return true;
    }
    case SU_BRIDGE: {
        int n = 0;
        vec3 side = v3(-fwd.z, 0, fwd.x);
        for (float t = 0.8f; t < 16.0f + 4.0f * rank; t += 0.5f) {
            vec3 q = v3_add(f, v3_scale(fwd, t));
            if (game_solid(g, (int)floorf(q.x), fy, (int)floorf(q.z)) == 1) break; /* a wall ends it */
            for (int w = 0; w < 2; w++) {
                vec3 c = v3_add(q, v3_scale(side, w ? 0.6f : -0.4f));
                n += place(g, (int)floorf(c.x), fy - 1, (int)floorf(c.z), B_GLASS, 60.0f * k);
            }
        }
        if (!n) return fail(ps, "NOTHING TO BRIDGE HERE");
        return true;
    }
    case SU_QUENCH: {
        int n = 0, r = 5 + rank;
        for (int z = fz - r; z <= fz + r; z++)
            for (int y = fy - 3; y <= fy + 1; y++)
                for (int x = fx - r; x <= fx + r; x++) {
                    if ((x - fx) * (x - fx) + (z - fz) * (z - fz) > r * r) continue;
                    n += edit_add(g, x, y, z, B_WATER, B_ICE, 60.0f * k);
                    n += edit_add(g, x, y, z, B_LAVA, B_ROCK, 60.0f * k);
                }
        if (!n) return fail(ps, "NO WATER OR LAVA NEAR");
        particles_burst(g, eye, v3(0.7f, 0.9f, 1.0f), 30, 5.0f, 0.06f, true);
        return true;
    }
    case SU_STEED: s->steed_t = 60.0f * k; return true;

    case SU_SANCTUM: {
        if (realm_has(f)) return fail(ps, "YOU ARE ALREADY HOME");
        if (ps->ritual_t > 0) return false;
        /* the ritual: three seconds in a circle of light; a hit breaks it (game_hurt_player) */
        ps->ritual_t = 3.0f;
        game_banner(ps, "THE RITUAL BEGINS - THREE SECONDS, AND DO NOT GET HIT");
        return true;
    }
    default: return false;
    }
}

/* ---------- running ---------- */

static void tick(float *t, float dt) { if (*t > 0) *t = fmaxf(0.0f, *t - dt); }

/* The ritual is done: the door stands before you for 30 seconds. */
static void open_door(Game *g, PlayerSlot *ps) {
    for (int i = 0; i < MAX_EFFECTS; i++)
        if (g->effects[i].alive && g->effects[i].type == EF_DOOR && g->effects[i].owner == ps->index) g->effects[i].alive = false;
    vec3 f = feet_of(ps), fwd = flat_dir(ps->pl.yaw), at;
    if (!stand_spot(g, v3_add(f, v3_scale(fwd, 2.2f)), &at)) at = v3_add(f, v3_scale(fwd, 1.2f));
    Effect *e = effect_new(g, EF_DOOR, EL_HOLY, ps->index, at, 30.0f, 0, 0.9f);
    if (!e) return;
    e->yaw = ps->pl.yaw + PI_F; /* facing you */
    sfx_at(g, SFX_REALM_DOOR, at, 1.0f);
    flash_add(g, v3_add(at, v3(0, 1.2f, 0)), v3(1.3f, 1.0f, 0.6f), 10.0f, 0.6f);
    game_banner(ps, "THE SANCTUM DOOR OPENS - STEP THROUGH");
}

void utility_ritual_break(Game *g, PlayerSlot *ps) {
    if (ps->ritual_t <= 0) return;
    ps->ritual_t = 0;
    ps->st.spell_cd = fmaxf(ps->st.spell_cd, 5.0f);
    particles_burst(g, v3_add(ps->pl.pos, v3(0, 0.3f, 0)), v3(1.0f, 0.85f, 0.5f), 20, 4.0f, 0.05f, true);
    sfx_player(g, ps, SFX_SPELL_FIZZLE, 1.0f);
    game_banner(ps, "THE RITUAL BREAKS");
}

static void player_timers(Game *g, PlayerSlot *ps, float dt) {
    if (ps->ritual_t > 0) {
        if (ps->st.dead || realm_has(ps->pl.pos)) ps->ritual_t = 0;
        else if ((ps->ritual_t -= dt) <= 0) { ps->ritual_t = 0; open_door(g, ps); }
        else if (fmodf(ps->ritual_t * 10.0f, 1.0f) < dt * 10.0f) {
            float a = ps->ritual_t * 4.0f;
            particles_burst(g, v3_add(ps->pl.pos, v3(cosf(a) * 1.2f, 0.1f, sinf(a) * 1.2f)), v3(1.0f, 0.85f, 0.5f), 2, 1.5f, 0.05f, true);
        }
    }
    Stats *s = &ps->st;
    bool had_wings = s->wings_t > 0, had_lev = s->levitate_t > 0;
    tick(&s->levitate_t, dt); tick(&s->wings_t, dt); tick(&s->glide_t, dt); tick(&s->feather_t, dt); tick(&s->climb_t, dt);
    tick(&s->stride_t, dt); tick(&s->stoneskin_t, dt); tick(&s->scry_t, dt); tick(&s->reveal_t, dt); tick(&s->dowse_t, dt);
    tick(&s->invis_t, dt); tick(&s->steed_t, dt); tick(&ps->gate_cd, dt);
    if (had_wings && s->wings_t <= 0) s->glide_t = 4.0f; /* glide down when the wings fade */
    if (had_lev && s->levitate_t <= 0) s->glide_t = fmaxf(s->glide_t, 2.0f);
    bool fed = s->well_fed_t > 0;
    tick(&s->refreshed_t, dt); tick(&s->well_fed_t, dt);
    if (fed && s->well_fed_t <= 0) stats_refresh(ps);
    if (s->steed_t > 0 && ps->pl.on_ground && sqrtf(ps->pl.vel.x * ps->pl.vel.x + ps->pl.vel.z * ps->pl.vel.z) > 3.0f &&
        fmodf(g->time * 6.0f, 1.0f) < dt * 6.0f)
        particles_burst(g, ps->pl.pos, v3(0.6f, 0.8f, 1.0f), 2, 1.0f, 0.04f, true);
}

/* Something (a player or a monster) steps into a gate: out of the other one. */
static bool gate_target(Game *g, const Effect *e, vec3 *out) {
    if (e->link < 0 || !g->effects[e->link].alive) return false;
    const Effect *o = &g->effects[e->link];
    vec3 ofw = flat_dir(o->yaw);
    *out = v3_add(o->pos, v3_scale(ofw, 1.1f));
    return true;
}

static bool near_effect(vec3 p, const Effect *e, float r) {
    float dx = p.x - e->pos.x, dz = p.z - e->pos.z;
    return dx * dx + dz * dz < r * r && fabsf(p.y - e->pos.y) < 1.6f;
}

void utility_update(Game *g, float dt) {
    tick(&g->timeslip_t, dt);
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) player_timers(g, &g->players[i], dt);
    for (int i = 0; i < MAX_EFFECTS; i++) {
        Effect *e = &g->effects[i];
        if (!e->alive || e->type < EF_DECOY) continue;
        e->t += dt;
        if (e->t >= e->life) { e->alive = false; continue; }
        switch (e->type) {
        case EF_SANCTUARY:
            /* monsters are pushed out of the ring */
            for (int k = 0; k < MAX_MONSTERS; k++) {
                Monster *m = &g->mons[k];
                if (!m->alive) continue;
                vec3 d = v3_sub(m->pos, e->pos);
                d.y = 0;
                float dd = v3_len(d), r = e->radius + MONSTERS[m->type].hw * m->scale;
                if (dd >= r || fabsf(m->pos.y - e->pos.y) > 3.0f) continue;
                vec3 n = dd > 0.01f ? v3_scale(d, 1.0f / dd) : v3(1, 0, 0);
                vec3 to = v3_add(e->pos, v3_scale(n, r));
                to.y = m->pos.y;
                if (phys_box_free(to, MONSTERS[m->type].hw * m->scale, MONSTERS[m->type].h * m->scale, game_solid, g)) m->pos = to;
                m->vel.x = n.x * 2.0f;
                m->vel.z = n.z * 2.0f;
            }
            break;
        case EF_GRAVWELL:
            for (int k = 0; k < MAX_MONSTERS; k++) {
                Monster *m = &g->mons[k];
                if (!m->alive) continue;
                vec3 d = v3_sub(e->pos, m->pos);
                float dd = v3_len(d);
                if (dd > e->radius || dd < 0.6f) continue;
                m->vel = v3_add(m->vel, v3_scale(d, (m->grade == GRADE_BOSS ? 4.0f : 12.0f) * dt / dd));
            }
            for (int k = 0; k < MAX_PICKUPS; k++) {
                Pickup *p = &g->pickups[k];
                if (!p->alive || p->key) continue;
                vec3 d = v3_sub(e->pos, p->pos);
                if (v3_len(d) < e->radius && v3_len(d) > 0.4f) p->pos = v3_add(p->pos, v3_scale(v3_norm(d), 4.0f * dt));
            }
            if (fmodf(e->t, 0.1f) < dt) particles_burst(g, e->pos, v3(0.6f, 0.4f, 1.0f), 2, 4.0f, 0.05f, true);
            break;
        case EF_GATE: {
            vec3 out;
            if (!gate_target(g, e, &out)) break;
            for (int k = 0; k < MAX_PLAYERS; k++) {
                PlayerSlot *ps = &g->players[k];
                if (ps->active && !ps->st.dead && ps->gate_cd <= 0 && near_effect(ps->pl.pos, e, 0.75f)) {
                    teleport(g, ps, out);
                    ps->gate_cd = 1.2f;
                    sfx_at(g, SFX_SPELL_BLINK, out, 0.8f);
                }
            }
            for (int k = 0; k < MAX_MONSTERS; k++) {
                Monster *m = &g->mons[k];
                if (m->alive && m->portal_cd <= 0 && near_effect(m->pos, e, 0.75f + MONSTERS[m->type].hw)) {
                    m->pos = out;
                    m->portal_cd = 1.5f;
                    particles_burst(g, out, v3(0.7f, 0.6f, 1.0f), 10, 2.0f, 0.05f, true);
                }
            }
            break;
        }
        case EF_DOOR:
            for (int k = 0; k < MAX_PLAYERS; k++) {
                PlayerSlot *ps = &g->players[k];
                if (ps->active && !ps->st.dead && ps->gate_cd <= 0 && near_effect(ps->pl.pos, e, 0.8f)) {
                    realm_enter(g, ps, e->pos, e->yaw + PI_F);
                    ps->gate_cd = 1.5f;
                }
            }
            break;
        case EF_HEARTH:
            for (int k = 0; k < MAX_PLAYERS; k++) {
                PlayerSlot *ps = &g->players[k];
                if (!ps->active || ps->st.dead || !near_effect(ps->pl.pos, e, e->radius)) continue;
                ps->st.hp = fminf(ps->st.maxhp, ps->st.hp + ps->st.maxhp * 0.03f * dt);
                ps->st.mana = fminf(ps->st.maxmana, ps->st.mana + ps->st.maxmana * 0.04f * dt);
            }
            break;
        default: break;
        }
    }
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (m->alive && m->portal_cd > 0) m->portal_cd -= dt;
    }
    edits_update(g);
    realm_update(g, dt);
}

/* A monster's blow lands on a decoy or the monster it was charmed to fight. */
bool utility_monster_strike(Game *g, Monster *m, float dmg) {
    if (m->alt_kind == 1) {
        Effect *e = &g->effects[m->alt_idx];
        if (e->alive && e->type == EF_DECOY) {
            e->alive = false;
            particles_burst(g, v3_add(e->pos, v3(0, 1.0f, 0)), v3(0.8f, 0.8f, 1.0f), 24, 3.0f, 0.05f, true);
            sfx_at(g, SFX_WARD_ABSORB, e->pos, 1.0f);
        }
        return true;
    }
    if (m->alt_kind == 2) {
        Monster *o = &g->mons[m->alt_idx];
        if (o->alive) monster_damage(g, o, dmg, m->pos, 3.0f, m->charmer);
        return true;
    }
    return m->alt_kind == 3; /* following its charmer: no harm meant */
}

/* ---------- drawing ---------- */

/* Another player's figure, standing still: a mirror image. */
static void draw_figure(Game *g, vec3 p, float yaw, vec3 col, float shimmer) {
    vec3 f = flat_dir(yaw), r = v3(-sinf(yaw), 0, cosf(yaw));
    vec3 c = v3_scale(col, 0.7f + 0.3f * shimmer);
    for (int k = -1; k <= 1; k += 2) batch3d_box(v3_add(v3_add(p, v3_scale(r, 0.12f * k)), v3(0, 0.38f, 0)), v3(0.09f, 0.38f, 0.09f), yaw, v3_scale(c, 0.6f), true);
    vec3 torso = v3_add(p, v3(0, 1.05f, 0));
    batch3d_box(torso, v3(0.16f, 0.32f, 0.26f), yaw, c, true);
    batch3d_box(v3_add(torso, v3(0, 0.48f, 0)), v3(0.15f, 0.15f, 0.15f), yaw, v3_scale(c, 0.9f), true);
    batch3d_spike(v3_add(v3_add(torso, v3_scale(r, 0.3f)), v3_scale(f, 0.25f)), v3_add(f, v3(0, 1.2f, 0)), 0.04f, 0.7f, v3_scale(c, 1.2f), true);
    (void)g;
}

static void ring(vec3 c, float r, int n, float t, vec3 col, float size) {
    for (int k = 0; k < n; k++) {
        float a = k * 2.0f * PI_F / n + t;
        batch3d_box(v3_add(c, v3(cosf(a) * r, 0.04f, sinf(a) * r)), v3(size, 0.02f, size), a, col, true);
    }
}

/* A door frame of stone with a glowing way through, facing yaw. */
void utility_draw_door(Game *g, vec3 p, float yaw, float open, bool home) {
    vec3 f = flat_dir(yaw), r = v3(-sinf(yaw), 0, cosf(yaw));
    vec3 l = game_light_at(g, v3_add(p, v3(0, 1.2f, 0)));
    l = v3(fmaxf(l.x, 0.35f), fmaxf(l.y, 0.32f), fmaxf(l.z, 0.3f));
    vec3 stone = v3(0.62f * l.x, 0.58f * l.y, 0.52f * l.z), gold = v3(1.0f, 0.8f, 0.35f);
    for (int k = -1; k <= 1; k += 2) batch3d_obox(v3_add(p, v3_add(v3_scale(r, 0.8f * k), v3(0, 1.3f, 0))), v3_scale(r, 0.14f), v3(0, 1.3f, 0), v3_scale(f, 0.18f), stone, false);
    batch3d_obox(v3_add(p, v3(0, 2.7f, 0)), v3_scale(r, 0.95f), v3(0, 0.12f, 0), v3_scale(f, 0.2f), stone, false);
    batch3d_obox(v3_add(p, v3(0, 2.85f, 0)), v3_scale(r, 0.3f), v3(0, 0.08f, 0), v3_scale(f, 0.22f), gold, true);
    /* the way through: warm light from the castle, a slow swirl */
    float a = 0.55f + 0.25f * sinf(g->time * 2.0f);
    vec3 glow = home ? v3(1.0f * a + 0.3f, 0.8f * a + 0.2f, 0.5f * a) : v3(1.0f * a + 0.3f, 0.75f * a + 0.25f, 0.45f * a + 0.1f);
    batch3d_obox(v3_add(p, v3(0, 1.25f, 0)), v3_scale(r, 0.66f * open), v3(0, 1.25f * open, 0), v3_scale(f, 0.02f), glow, true);
    for (int k = 0; k < 6; k++) {
        float b = g->time * 1.5f + k * 1.05f;
        vec3 q = v3_add(p, v3_add(v3_scale(r, cosf(b) * 0.45f), v3(0, 1.25f + sinf(b) * 0.9f, 0)));
        batch3d_box(v3_add(q, v3_scale(f, 0.05f)), v3(0.04f, 0.04f, 0.04f), b, v3(1.0f, 0.95f, 0.7f), true);
    }
}

void utility_draw_world(Game *g, PlayerSlot *viewer) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->ritual_t <= 0) continue;
        float k = 1.0f - ps->ritual_t / 3.0f; /* the circle closes as the ritual goes on */
        ring(ps->pl.pos, 1.3f, 24, g->time * 1.5f, v3(1.0f, 0.85f, 0.45f), 0.06f);
        ring(ps->pl.pos, 1.3f * (1.0f - k) + 0.3f, 12, -g->time * 3.0f, v3(1.2f, 1.0f, 0.6f), 0.05f);
    }
    for (int i = 0; i < MAX_EFFECTS; i++) {
        Effect *e = &g->effects[i];
        if (!e->alive || e->type < EF_DECOY) continue;
        float fade = fminf(1.0f, (e->life - e->t) / 1.5f);
        switch (e->type) {
        case EF_DECOY: {
            vec3 col = e->owner >= 0 && e->owner < MAX_PLAYERS ? g->players[e->owner].color : v3(0.7f, 0.7f, 1.0f);
            draw_figure(g, e->pos, e->yaw + sinf(g->time + i) * 0.3f, col, 0.5f + 0.5f * sinf(g->time * 9.0f + i));
            break;
        }
        case EF_LURE: {
            vec3 c = v3_add(e->pos, v3(0, 1.0f + 0.2f * sinf(e->t * 3.0f), 0));
            batch3d_octa(c, 0.2f, 0.3f, e->t * 3.0f, v3(1.0f, 0.95f, 0.6f), true);
            ring(e->pos, 0.8f + 0.3f * sinf(e->t * 4.0f), 8, e->t, v3(1.0f, 0.9f, 0.5f), 0.05f);
            break;
        }
        case EF_SANCTUARY:
            ring(e->pos, e->radius, 36, e->t * 0.3f, v3_scale(v3(1.0f, 0.95f, 0.6f), 0.6f * fade + 0.2f), 0.08f);
            ring(e->pos, e->radius - 0.25f, 18, -e->t * 0.5f, v3_scale(v3(1.0f, 0.85f, 0.4f), 0.4f * fade), 0.05f);
            break;
        case EF_GRAVWELL:
            batch3d_octa(e->pos, 0.35f, 0.35f, e->t * 6.0f, v3(0.15f, 0.05f, 0.3f), true);
            for (int k = 0; k < 3; k++) ring(e->pos, fmodf(e->radius * (1.0f - fmodf(e->t * 0.8f + k / 3.0f, 1.0f)), e->radius), 16, e->t * 2.0f + k, v3(0.6f, 0.4f, 1.0f), 0.05f);
            break;
        case EF_GATE: {
            bool linked = e->link >= 0 && g->effects[e->link].alive;
            vec3 f = flat_dir(e->yaw), r = v3(-sinf(e->yaw), 0, cosf(e->yaw));
            vec3 col = linked ? v3(0.6f, 0.45f, 1.0f) : v3(0.4f, 0.35f, 0.6f);
            for (int k = 0; k < 16; k++) {
                float a = k * 2.0f * PI_F / 16.0f + e->t * (linked ? 2.0f : 0.5f);
                vec3 q = v3_add(e->pos, v3_add(v3_scale(r, cosf(a) * 0.7f), v3(0, 1.1f + sinf(a) * 1.0f, 0)));
                batch3d_box(q, v3(0.06f, 0.06f, 0.06f), a, col, true);
            }
            if (linked) batch3d_obox(v3_add(e->pos, v3(0, 1.1f, 0)), v3_scale(r, 0.6f), v3(0, 0.95f, 0), v3_scale(f, 0.02f), v3_scale(col, 0.5f + 0.2f * sinf(e->t * 5.0f)), true);
            break;
        }
        case EF_DOOR: utility_draw_door(g, e->pos, e->yaw, fminf(1.0f, e->t * 2.0f) * fade, false); break;
        case EF_ANCHOR:
            if (e->owner == (viewer ? viewer->index : -1) || g->nplayers == 1) {
                ring(e->pos, 0.6f, 8, e->t, v3(0.7f, 0.5f, 1.0f), 0.05f);
                batch3d_octa(v3_add(e->pos, v3(0, 0.4f + 0.1f * sinf(e->t * 2.0f), 0)), 0.1f, 0.18f, e->t, v3(0.8f, 0.6f, 1.0f), true);
            }
            break;
        case EF_HEARTH:
            if (fmodf(e->t, 0.4f) < 0.05f) particles_burst(g, v3_add(e->pos, v3(0, 0.3f, 0)), v3(1.0f, 0.8f, 0.4f), 1, 0.6f, 0.04f, true);
            break;
        default: break;
        }
    }
    /* the viewer's dowsing: a beam of light over the treasure */
    if (viewer && viewer->st.dowse_t > 0) {
        vec3 best = v3(0, 0, 0);
        float bd = 160.0f;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            const Pickup *p = &g->pickups[i];
            if (!p->alive || (p->kind != PK_CHEST && p->kind != PK_RELIC && p->kind != PK_QUEST)) continue;
            float d = v3_len(v3_sub(p->pos, viewer->pl.pos));
            if (d < bd) { bd = d; best = p->pos; }
        }
        if (bd < 160.0f)
            for (int k = 0; k < 20; k++)
                batch3d_box(v3_add(best, v3(0, 0.8f + k * 0.6f, 0)), v3(0.06f, 0.3f, 0.06f), g->time + k, v3(1.0f, 0.85f, 0.35f), true);
    }
    if (viewer && viewer->st.reveal_t > 0) mech_draw_revealed(g);
    realm_draw(g);
}

void utility_lights(Game *g, vec3 eye, SceneParams *sp) {
    for (int i = 0; i < MAX_EFFECTS && sp->ndl < MAX_DLIGHTS; i++) {
        Effect *e = &g->effects[i];
        if (!e->alive || v3_len(v3_sub(e->pos, eye)) > 50.0f) continue;
        vec3 col;
        float r;
        if (e->type == EF_LURE) col = v3(1.2f, 1.1f, 0.7f), r = 10.0f;
        else if (e->type == EF_DOOR) col = v3(1.3f, 1.0f, 0.6f), r = 8.0f;
        else if (e->type == EF_GATE) col = v3(0.7f, 0.5f, 1.2f), r = 5.0f;
        else if (e->type == EF_SANCTUARY) col = v3(0.9f, 0.85f, 0.5f), r = e->radius + 2.0f;
        else continue;
        sp->dl_pos[sp->ndl] = v3_add(e->pos, v3(0, 1.2f, 0));
        sp->dl_col[sp->ndl] = col;
        sp->dl_rad[sp->ndl] = r;
        sp->ndl++;
    }
    if (realm_has(eye) && sp->ndl < MAX_DLIGHTS) { /* the door home glows in the garden */
        sp->dl_pos[sp->ndl] = v3(POCKET_EXIT_X, POCKET_G + 1.3f, POCKET_EXIT_Z + 0.4f);
        sp->dl_col[sp->ndl] = v3(1.0f, 0.8f, 0.45f);
        sp->dl_rad[sp->ndl] = 6.0f;
        sp->ndl++;
    }
}

/* What is on you, and where the treasure is. */
void utility_draw_hud(Game *g, PlayerSlot *ps, int W, int H) {
    const Stats *s = &ps->st;
    struct { float t; const char *name; vec3 c; } ON[] = {
        { s->levitate_t, "LEVITATE", { 0.8f, 0.9f, 1.0f } },  { s->wings_t, "WINGS", { 0.9f, 0.95f, 1.0f } },
        { s->feather_t, "FEATHER", { 0.85f, 0.85f, 1.0f } },  { s->climb_t, "CLIMB", { 0.7f, 0.9f, 0.6f } },
        { s->stride_t, "STRIDE", { 0.5f, 0.85f, 1.0f } },     { s->stoneskin_t, "STONESKIN", { 0.75f, 0.7f, 0.6f } },
        { s->reveal_t, "REVEAL", { 1.0f, 0.8f, 0.4f } },      { s->dowse_t, "DOWSING", { 1.0f, 0.85f, 0.35f } },
        { s->invis_t, "INVISIBLE", { 0.7f, 0.7f, 0.9f } },    { s->steed_t, "STEED", { 0.6f, 0.8f, 1.0f } },
        { s->scry_t, "SIGHT", { 0.8f, 0.6f, 1.0f } },         { s->well_fed_t, "WELL FED", { 1.0f, 0.75f, 0.4f } },
        { s->refreshed_t, "REFRESHED", { 0.6f, 0.9f, 1.0f } }, { g->timeslip_t, "TIME SLIP", { 0.5f, 0.7f, 1.0f } },
        { s->elixir && s->elixir_onset <= 0 ? s->elixir_t : 0, "ELIXIR", { 0.6f, 1.0f, 0.6f } },
        { s->elixir ? s->elixir_onset : 0, "BREWING IN YOU", { 0.8f, 1.0f, 0.5f } },
        { s->meal ? s->meal_t : 0, "MEAL", { 1.0f, 0.8f, 0.5f } },
    };
    font_begin(W, H);
    char buf[64];
    int x = 6, y = H - 66;
    for (int i = 0; i < (int)(sizeof ON / sizeof ON[0]); i++) {
        if (ON[i].t <= 0) continue;
        if (ON[i].t > 99.0f) snprintf(buf, sizeof buf, "%s %dM", ON[i].name, (int)(ON[i].t / 60.0f) + 1);
        else snprintf(buf, sizeof buf, "%s %d", ON[i].name, (int)ceilf(ON[i].t));
        int w = font_text_width(buf);
        if (x + w > W - 6) { x = 6; y -= 9; }
        font_text(x, y, buf, ON[i].c.x, ON[i].c.y, ON[i].c.z);
        x += w + 8;
    }
    if (s->dowse_t > 0) {
        const Pickup *best = NULL;
        float bd = 160.0f;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            const Pickup *p = &g->pickups[i];
            if (!p->alive || (p->kind != PK_CHEST && p->kind != PK_RELIC && p->kind != PK_QUEST)) continue;
            float d = v3_len(v3_sub(p->pos, ps->pl.pos));
            if (d < bd) { bd = d; best = p; }
        }
        if (best) {
            vec3 to = v3_sub(best->pos, ps->pl.pos);
            float rel = atan2f(to.z, to.x) - ps->pl.yaw;
            while (rel > PI_F) rel -= 2.0f * PI_F;
            while (rel < -PI_F) rel += 2.0f * PI_F;
            const char *side = fabsf(rel) < 0.4f ? "AHEAD" : fabsf(rel) > 2.7f ? "BEHIND" : rel > 0 ? "TO THE RIGHT" : "TO THE LEFT";
            int dl = (int)lroundf(to.y / 3.0f);
            snprintf(buf, sizeof buf, "%s %.0f M %s%s", best->kind == PK_RELIC ? "A RELIC" : best->kind == PK_QUEST ? "THE QUEST ITEM" : "A CHEST",
                     v3_len(to), side, dl > 0 ? ", ABOVE" : dl < 0 ? ", BELOW" : "");
        } else snprintf(buf, sizeof buf, "THE ROD DOES NOT STIR");
        font_text((W - font_text_width(buf)) / 2, 30, buf, 1.0f, 0.85f, 0.4f);
    }
    if (s->scry_t > 0) {
        const char *t = "CLAIRVOYANCE - CAST AGAIN TO RETURN";
        font_text((W - font_text_width(t)) / 2, 40, t, 0.8f, 0.6f, 1.0f);
    }
    font_end();
    if (g->timeslip_t > 0 || s->scry_t > 0 || s->invis_t > 0) {
        batch2d_begin(W, H);
        if (g->timeslip_t > 0) batch2d_rect(0, 0, (float)W, (float)H, 0.3f, 0.45f, 0.9f, 0.12f);
        if (s->scry_t > 0) batch2d_rect(0, 0, (float)W, (float)H, 0.45f, 0.25f, 0.7f, 0.1f);
        if (s->invis_t > 0) batch2d_rect(0, 0, (float)W, (float)H, 0.6f, 0.6f, 0.75f, 0.08f);
        batch2d_flush();
    }
}
