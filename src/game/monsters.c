#include "game/game.h"

#include "render/batch.h"

/* For now monsters are simple shapes in strong colours. The family mix comes from the
 * stratum, the tint from the flavor, and toughness from the danger level. */

const MonsterDef MONSTERS[MON_TYPE_COUNT] = {
    /*            hw     h     hp    dmg  speed reach atk   xp  flies  colour */
    { "Crawler", 0.32f, 0.6f, 18, 5, 5.2f, 0.9f, 0.9f, 8, false, { 0.85f, 0.6f, 0.25f } },
    { "Brute", 0.55f, 2.0f, 90, 16, 2.8f, 1.5f, 1.6f, 30, false, { 0.85f, 0.2f, 0.15f } },
    { "Wisp", 0.35f, 0.9f, 30, 9, 3.4f, 0.0f, 2.2f, 18, true, { 0.3f, 0.55f, 1.0f } },
    { "Horned", 0.45f, 1.2f, 45, 13, 3.6f, 1.2f, 1.4f, 22, false, { 0.5f, 0.85f, 0.3f } },
};

/* family weights per stratum: crawler, brute, wisp, horned */
static const float FAMILY[ST_COUNT][MON_TYPE_COUNT] = {
    [ST_CORE] = { 2, 2, 3, 2 }, [ST_INFERNO] = { 3, 2, 2, 3 }, [ST_LIMBO] = { 3, 1, 3, 1 },
    [ST_ROOTS] = { 4, 1, 2, 1 }, [ST_MINES] = { 3, 2, 1, 3 }, [ST_DUNGEON] = { 4, 2, 2, 1 },
    [ST_CASTLE] = { 2, 3, 2, 2 }, [ST_SPIRES] = { 1, 2, 4, 2 }, [ST_CLOUDS] = { 1, 1, 5, 1 },
    [ST_HEAVEN] = { 1, 2, 4, 1 }, [ST_ASTRAL] = { 1, 1, 5, 2 },
};

/* ---------- spawned-chunk set ---------- */

static u64 chunk_key(int cx, int cy, int cz) {
    return (((u64)(u32)cx & 0x1FFFFF) << 42) | (((u64)(u32)cy & 0x1FFFFF) << 21) | ((u64)(u32)cz & 0x1FFFFF) | (1ULL << 63);
}

#define TOMBSTONE 1ULL

static bool spawned_has(Game *g, u64 k) {
    if (!g->spawned_cap) return false;
    for (int i = (int)(mix64(k) & (u64)(g->spawned_cap - 1));; i = (i + 1) & (g->spawned_cap - 1)) {
        if (g->spawned[i] == 0) return false;
        if (g->spawned[i] == k) return true;
    }
}

static void spawned_remove(Game *g, u64 k) {
    if (!g->spawned_cap) return;
    for (int i = (int)(mix64(k) & (u64)(g->spawned_cap - 1));; i = (i + 1) & (g->spawned_cap - 1)) {
        if (g->spawned[i] == 0) return;
        if (g->spawned[i] == k) { g->spawned[i] = TOMBSTONE; return; }
    }
}

static void spawned_add(Game *g, u64 k) {
    if ((g->spawned_count + 1) * 2 > g->spawned_cap) {
        int old_cap = g->spawned_cap;
        u64 *old = g->spawned;
        g->spawned_cap = old_cap ? old_cap * 2 : 1024;
        g->spawned = calloc((size_t)g->spawned_cap, sizeof(u64));
        g->spawned_count = 0;
        for (int i = 0; i < old_cap; i++)
            if (old[i] > TOMBSTONE) spawned_add(g, old[i]);
        free(old);
    }
    for (int i = (int)(mix64(k) & (u64)(g->spawned_cap - 1));; i = (i + 1) & (g->spawned_cap - 1)) {
        if (g->spawned[i] == k) return;
        if (g->spawned[i] <= TOMBSTONE) {
            g->spawned[i] = k;
            g->spawned_count++;
            return;
        }
    }
}

/* ---------- spawning ---------- */

void monster_spawn(Game *g, int type, vec3 pos, int level, bool elite, vec3 tint, u64 key) {
    Monster *m = NULL;
    for (int i = 0; i < MAX_MONSTERS && !m; i++)
        if (!g->mons[i].alive) m = &g->mons[i];
    if (!m) return;
    const MonsterDef *d = &MONSTERS[type];
    memset(m, 0, sizeof *m);
    m->alive = true;
    m->type = (u8)type;
    m->elite = elite;
    m->pos = pos;
    m->scale = elite ? 1.35f : 1.0f;
    /* more players, tougher monsters (but not proportionally: slaying should stay quick) */
    int np = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) np += g->players[i].active;
    float party = 1.0f + 0.5f * (float)MAXI(0, np - 1);
    m->maxhp = m->hp = d->hp * (1.0f + 0.18f * level) * (elite ? 3.0f : 1.0f) * party;
    m->dmg = d->dmg * (1.0f + 0.10f * level) * (elite ? 1.5f : 1.0f);
    m->xp = d->xp * (1.0f + 0.15f * level) * (elite ? 4.0f : 1.0f);
    m->color = v3_lerp(d->color, tint, 0.2f);
    m->state = MS_IDLE;
    m->chunk_key = key;
    m->target = 0;
    m->last_hitter = -1;
    m->yaw = (float)((key + (u64)g->mons_anim_seed * 97) % 628) * 0.01f;
    m->anim = (float)((key + (u64)g->mons_anim_seed * 31) % 100) * 0.1f;
}

static bool standable_cell(Game *g, int x, int y, int z, int headroom) {
    if (game_solid(g, x, y - 1, z) != 1) return false;
    for (int k = 0; k < headroom; k++)
        if (game_solid(g, x, y + k, z) != 0) return false;
    return true;
}

static void spawn_chunk(Game *g, Chunk *c) {
    const ChunkInfo *ci = &c->info;
    u64 key = chunk_key(c->cx, c->cy, c->cz);
    spawned_add(g, key);
    /* shrines */
    for (int i = 0; i < CHUNK_VOL && g->nshrines < MAX_SHRINES; i++)
        if (c->blocks[i] == B_SHRINE) {
            int x = i % CHUNK, z = (i / CHUNK) % CHUNK, y = i / (CHUNK * CHUNK);
            vec3 sp = v3(c->cx * CHUNK + x + 0.5f, c->cy * CHUNK + y + 0.5f, c->cz * CHUNK + z + 0.5f);
            bool known = false;
            for (int k = 0; k < g->nshrines && !known; k++) known = v3_len(v3_sub(g->shrines[k], sp)) < 0.5f;
            if (!known) g->shrines[g->nshrines++] = sp;
        }
    if (ci->arch == ARCH_EXTERIOR) return;

    Rng r = rng_make(ci->seed ^ 0x5EED);
    int level = ci->level;
    /* about half the chunks hold a pack; packs grow with danger */
    int n = 0;
    if (level <= 1) n = rng_chance(&r, 0.5f) ? 1 : 0;
    else if (rng_chance(&r, ci->has_struct ? 0.9f : 0.5f)) n = MINI(6, 2 + (int)(level * 0.08f + rng_float(&r) * 2.0f));
    const float *w = FAMILY[ci->stratum];
    int main_type = level <= 1 ? MON_CRAWLER : pick_weighted(rng_float(&r), w, MON_TYPE_COUNT);
    ThemeInfo ti;
    theme_info(ci->theme, &ti);
    vec3 tint = v3(ti.mat[B_ACCENT].r / 255.0f, ti.mat[B_ACCENT].g / 255.0f, ti.mat[B_ACCENT].b / 255.0f);
    for (int i = 0; i < n; i++) {
        int type = (level > 1 && rng_chance(&r, 0.3f)) ? pick_weighted(rng_float(&r), w, MON_TYPE_COUNT) : main_type;
        bool elite = level > 2 && rng_chance(&r, 0.04f + 0.004f * level);
        int head = (int)ceilf(MONSTERS[type].h * (elite ? 1.35f : 1.0f)) + (MONSTERS[type].flies ? 1 : 0);
        for (int attempt = 0; attempt < 40; attempt++) {
            int x = rng_range(&r, 1, 30), z = rng_range(&r, 1, 30);
            int wx = c->cx * CHUNK + x, wz = c->cz * CHUNK + z;
            int found = -1;
            for (int y = 30; y >= 1; y--)
                if (standable_cell(g, wx, c->cy * CHUNK + y, wz, head)) { found = c->cy * CHUNK + y; break; }
            if (found < 0) continue;
            vec3 p = v3(wx + 0.5f, (float)found + (MONSTERS[type].flies ? 1.0f : 0.0f), wz + 0.5f);
            bool near_player = false;
            for (int k = 0; k < MAX_PLAYERS; k++)
                if (g->players[k].active && v3_len(v3_sub(p, player_eye(&g->players[k].pl))) < 12.0f) near_player = true;
            if (near_player) continue;
            monster_spawn(g, type, p, level, elite, tint, key);
            g->mons_anim_seed++;
            break;
        }
    }
}

#define SPAWN_RADIUS_H 2
#define SPAWN_RADIUS_V 1
#define DESPAWN_DIST 110.0f

/* Distance from p to the nearest active player's eye. */
static float player_dist(Game *g, vec3 p) {
    float best = 1e9f;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) best = fminf(best, v3_len(v3_sub(p, player_eye(&g->players[i].pl))));
    return best;
}

void monsters_spawn_for_chunks(Game *g) {
    ChunkMap *m = &g->map;
    /* monsters far from everyone go back to sleep in their chunk: it will be repopulated on return */
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *mo = &g->mons[i];
        if (!mo->alive || player_dist(g, mo->pos) < DESPAWN_DIST) continue;
        mo->alive = false;
        spawned_remove(g, mo->chunk_key);
    }
    for (int i = 0; i < m->pool_cap; i++) {
        Chunk *c = &m->pool[i];
        if (!c->used || c->state != CH_READY || !c->has_mesh) continue;
        vec3 centre = v3(c->cx * CHUNK + 16.0f, c->cy * CHUNK + 16.0f, c->cz * CHUNK + 16.0f);
        bool in_range = false, too_close = false;
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *ps = &g->players[k];
            if (!ps->active) continue;
            vec3 eye = player_eye(&ps->pl);
            int pcx = floordiv((int)floorf(eye.x), CHUNK), pcy = floordiv((int)floorf(eye.y), CHUNK), pcz = floordiv((int)floorf(eye.z), CHUNK);
            float d = v3_len(v3_sub(centre, eye));
            if (abs(c->cx - pcx) <= SPAWN_RADIUS_H && abs(c->cz - pcz) <= SPAWN_RADIUS_H && abs(c->cy - pcy) <= SPAWN_RADIUS_V && d <= 70.0f)
                in_range = true;
            /* never spawn into the chunk a player is standing in */
            if (d < 20.0f && c->info.arch != ARCH_EXTERIOR) too_close = true;
        }
        if (!in_range || too_close) continue;
        u64 key = chunk_key(c->cx, c->cy, c->cz);
        if (spawned_has(g, key)) continue;
        spawn_chunk(g, c);
    }
}

/* ---------- flow field ---------- */

static inline int fidx(int x, int y, int z) { return (y * FLOW_W + z) * FLOW_W + x; }
#define SY0 4 /* solid cache has 4 extra layers below and above */
static inline bool fsolid(const PlayerSlot *g, int x, int y, int z) {
    if ((unsigned)x >= FLOW_W || (unsigned)z >= FLOW_W || y < -SY0 || y >= FLOW_Y + SY0) return true;
    return g->flow_solid[((y + SY0) * FLOW_W + z) * FLOW_W + x];
}
static inline bool fstand(const PlayerSlot *g, int x, int y, int z) { return !fsolid(g, x, y, z) && fsolid(g, x, y - 1, z); }

void flow_update(Game *game, PlayerSlot *g, float dt) {
    if (!g->flow) {
        g->flow = malloc(sizeof(u16) * FLOW_W * FLOW_W * FLOW_Y);
        g->flow_solid = malloc((size_t)FLOW_W * FLOW_W * (FLOW_Y + 2 * SY0));
    }
    int px = (int)floorf(g->pl.pos.x), py = (int)floorf(g->pl.pos.y + 0.01f), pz = (int)floorf(g->pl.pos.z);
    g->flow_timer -= dt;
    if (g->flow_timer > 0 && px == g->flow_px && py == g->flow_py && pz == g->flow_pz) return;
    g->flow_timer = 0.35f;
    g->flow_px = px; g->flow_py = py; g->flow_pz = pz;
    g->flow_ox = px - FLOW_R;
    g->flow_oy = py - FLOW_H;
    g->flow_oz = pz - FLOW_R;
    for (int y = -SY0; y < FLOW_Y + SY0; y++)
        for (int z = 0; z < FLOW_W; z++)
            for (int x = 0; x < FLOW_W; x++)
                g->flow_solid[((y + SY0) * FLOW_W + z) * FLOW_W + x] =
                    game_solid(game, g->flow_ox + x, g->flow_oy + y, g->flow_oz + z) != 0;
    memset(g->flow, 0xFF, sizeof(u16) * FLOW_W * FLOW_W * FLOW_Y);
    int sx = FLOW_R, sy = FLOW_H, sz = FLOW_R;
    for (int k = 0; k < 4 && !fstand(g, sx, sy, sz); k++) sy--;
    if (!fstand(g, sx, sy, sz)) return;
    static int queue[FLOW_W * FLOW_W * FLOW_Y];
    int head = 0, tail = 0;
    g->flow[fidx(sx, sy, sz)] = 0;
    queue[tail++] = fidx(sx, sy, sz);
    static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
    while (head < tail) {
        int u = queue[head++];
        int ux = u % FLOW_W, uz = (u / FLOW_W) % FLOW_W, uy = u / (FLOW_W * FLOW_W);
        u16 du = g->flow[u];
        for (int d = 0; d < 4; d++) {
            int vx = ux + DX[d], vz = uz + DZ[d];
            if ((unsigned)vx >= FLOW_W || (unsigned)vz >= FLOW_W) continue;
            /* cells v from which a monster can move into u */
            int cand[5], nc = 0;
            if (fstand(g, vx, uy, vz)) cand[nc++] = uy;
            if (fstand(g, vx, uy - 1, vz) && !fsolid(g, vx, uy, vz)) cand[nc++] = uy - 1;
            for (int k = 1; k <= 3; k++) {
                if (fsolid(g, ux, uy + k, uz)) break;
                if (fstand(g, vx, uy + k, vz)) { cand[nc++] = uy + k; break; }
            }
            for (int c = 0; c < nc; c++) {
                int vy = cand[c];
                if (vy < 0 || vy >= FLOW_Y) continue;
                int v = fidx(vx, vy, vz);
                if (g->flow[v] != 0xFFFF) continue;
                g->flow[v] = du + 1;
                queue[tail++] = v;
            }
        }
    }
}

/* Next cell centre on the way to the player, or false if the field does not cover it. */
static bool flow_next(const PlayerSlot *g, vec3 pos, vec3 *target) {
    if (!g->flow || g->flow_timer <= -1.0f) return false;
    int x = (int)floorf(pos.x) - g->flow_ox, y = (int)floorf(pos.y + 0.01f) - g->flow_oy, z = (int)floorf(pos.z) - g->flow_oz;
    if ((unsigned)x >= FLOW_W || (unsigned)z >= FLOW_W || y < 0 || y >= FLOW_Y) return false;
    if (g->flow[fidx(x, y, z)] == 0xFFFF && y > 0 && g->flow[fidx(x, y - 1, z)] != 0xFFFF) y--;
    u16 here = g->flow[fidx(x, y, z)];
    if (here == 0xFFFF || here == 0) return false;
    static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
    u16 best = here;
    int bx = -1, by = 0, bz = 0;
    for (int d = 0; d < 4; d++) {
        int nx = x + DX[d], nz = z + DZ[d];
        if ((unsigned)nx >= FLOW_W || (unsigned)nz >= FLOW_W) continue;
        int cand[5], nc = 0;
        if (fstand(g, nx, y, nz)) cand[nc++] = y;
        if (fstand(g, nx, y + 1, nz) && !fsolid(g, x, y + 1, z)) cand[nc++] = y + 1;
        for (int k = 1; k <= 3; k++) {
            if (fsolid(g, nx, y - k + 1, nz)) break;
            if (fstand(g, nx, y - k, nz)) { cand[nc++] = y - k; break; }
        }
        for (int c = 0; c < nc; c++) {
            int ny = cand[c];
            if (ny < 0 || ny >= FLOW_Y) continue;
            u16 dn = g->flow[fidx(nx, ny, nz)];
            if (dn < best) { best = dn; bx = nx; by = ny; bz = nz; }
        }
    }
    if (bx < 0) return false;
    *target = v3(g->flow_ox + bx + 0.5f, (float)(g->flow_oy + by), g->flow_oz + bz + 0.5f);
    return true;
}

/* ---------- behaviour ---------- */

Monster *monster_nearest(Game *g, vec3 p, float max_dist, const Monster *exclude) {
    Monster *best = NULL;
    float bd = max_dist;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || m == exclude) continue;
        float d = v3_len(v3_sub(v3_add(m->pos, v3(0, MONSTERS[m->type].h * 0.5f, 0)), p));
        if (d < bd) { bd = d; best = m; }
    }
    return best;
}

static vec3 monster_centre(const Monster *m) {
    return v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
}

static void wake_neighbours(Game *g, Monster *m) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *o = &g->mons[i];
        if (o->alive && !o->awake && v3_len(v3_sub(o->pos, m->pos)) < 10.0f) {
            o->awake = true;
            o->state = MS_CHASE;
        }
    }
}

void monster_damage(Game *g, Monster *m, float dmg, vec3 from, float knock, int attacker) {
    if (!m->alive) return;
    m->hp -= dmg;
    m->flash = 0.15f;
    if (attacker >= 0 && attacker < MAX_PLAYERS && g->players[attacker].active) {
        m->last_hitter = attacker;
        if (!g->players[attacker].st.dead) m->target = attacker; /* turn on whoever hurts you */
    }
    if (!m->awake) { m->awake = true; m->state = MS_CHASE; wake_neighbours(g, m); }
    vec3 dir = v3_sub(m->pos, from);
    dir.y = 0;
    dir = v3_norm(dir);
    float resist = m->type == MON_BRUTE ? 0.3f : 1.0f;
    m->vel = v3_add(m->vel, v3_scale(dir, knock * resist / m->scale));
    if (m->type != MON_WISP && knock > 0) m->vel.y += 2.0f * resist;
    vec3 c = monster_centre(m);
    particles_burst(g, c, v3_scale(m->color, 0.5f), 6, 3.0f, 0.06f, false);
    if (m->hp <= 0) {
        m->alive = false;
        particles_burst(g, c, m->color, 18, 5.0f, 0.12f, false);
        particles_burst(g, c, v3_scale(m->color, 0.4f), 10, 4.0f, 0.08f, false);
        if (m->elite) flash_add(g, c, v3(1.0f, 0.6f, 0.3f), 8.0f, 0.4f);
        /* the killer gets the full XP, everyone else nearby half of it */
        int killer = m->last_hitter;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            PlayerSlot *ps = &g->players[i];
            if (!ps->active) continue;
            if (i == killer) {
                ps->st.kills++;
                game_add_xp(g, ps, m->xp);
            } else if (v3_len(v3_sub(ps->pl.pos, m->pos)) < 30.0f) {
                game_add_xp(g, ps, m->xp * 0.5f);
            }
        }
    }
}

static void enemy_fire(Game *g, Monster *m, vec3 target) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (p->alive) continue;
        memset(p, 0, sizeof *p);
        p->alive = true;
        p->kind = PJ_ENEMY_FIRE;
        p->pos = monster_centre(m);
        p->vel = v3_scale(v3_norm(v3_sub(target, p->pos)), 11.0f);
        p->dmg = m->dmg;
        p->life = 4.0f;
        p->last_hit = -1;
        return;
    }
}

static void monster_physics(Game *g, Monster *m, vec3 want, float dt) {
    const MonsterDef *d = &MONSTERS[m->type];
    float hw = d->hw * m->scale, h = d->h * m->scale;
    if (d->flies) {
        m->vel = v3_lerp(m->vel, want, fminf(1.0f, 4.0f * dt));
        for (int a = 0; a < 3; a++) {
            float v = a == 0 ? m->vel.x : (a == 1 ? m->vel.y : m->vel.z);
            if (!phys_move_axis(&m->pos, a, v * dt, hw, h, game_solid, g)) {
                if (a == 0) m->vel.x = 0; else if (a == 1) m->vel.y = 0; else m->vel.z = 0;
            }
        }
        return;
    }
    float accel = m->on_ground ? 10.0f : 2.0f;
    m->vel.x += (want.x - m->vel.x) * fminf(1.0f, accel * dt);
    m->vel.z += (want.z - m->vel.z) * fminf(1.0f, accel * dt);
    m->vel.y -= 25.0f * dt;
    if (m->vel.y < -30.0f) m->vel.y = -30.0f;
    bool blocked = false;
    for (int a = 0; a < 3; a += 2) {
        float v = a == 0 ? m->vel.x : m->vel.z;
        vec3 before = m->pos;
        if (phys_move_axis(&m->pos, a, v * dt, hw, h, game_solid, g)) continue;
        vec3 up = v3_add(before, v3(0, 1.01f, 0));
        if (m->on_ground && phys_box_free(up, hw, h, game_solid, g)) {
            m->pos = up;
            phys_move_axis(&m->pos, a, v * dt, hw, h, game_solid, g);
        } else {
            blocked = true;
            if (a == 0) m->vel.x = 0; else m->vel.z = 0;
        }
    }
    bool falling = m->vel.y < 0;
    m->on_ground = false;
    if (!phys_move_axis(&m->pos, 1, m->vel.y * dt, hw, h, game_solid, g)) {
        if (falling) m->on_ground = true;
        m->vel.y = 0;
    }
    if (blocked && m->state == MS_CHARGE) {
        m->state = MS_STUN;
        m->timer = 1.0f;
        particles_burst(g, v3_add(m->pos, v3(0, h, 0)), v3(1.0f, 0.9f, 0.5f), 8, 3.0f, 0.05f, true);
    }
}

/* Keep chasing the current target unless another living player is clearly closer. */
static PlayerSlot *monster_target(Game *g, Monster *m, vec3 c) {
    PlayerSlot *cur = NULL;
    if (m->target >= 0 && m->target < MAX_PLAYERS && g->players[m->target].active && !g->players[m->target].st.dead)
        cur = &g->players[m->target];
    PlayerSlot *near = game_nearest_player(g, c, 1e9f);
    if (!near) return NULL;
    if (!cur || v3_len(v3_sub(player_eye(&near->pl), c)) + 4.0f < v3_len(v3_sub(player_eye(&cur->pl), c)))
        cur = near;
    m->target = cur->index;
    return cur;
}

void monsters_update(Game *g, float dt) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        const MonsterDef *d = &MONSTERS[m->type];
        vec3 c = monster_centre(m);
        PlayerSlot *tp = monster_target(g, m, c);
        bool all_dead = tp == NULL;
        if (!tp) tp = &g->players[0];
        vec3 eye = player_eye(&tp->pl);
        vec3 feet = tp->pl.pos;
        vec3 to = v3_sub(eye, c);
        float dist = v3_len(to);
        if (dist > 80.0f) continue; /* frozen far away (their chunk may be unloaded) */
        /* stay put while the ground under them is not loaded */
        if (game_solid(g, (int)floorf(m->pos.x), (int)floorf(m->pos.y) - 1, (int)floorf(m->pos.z)) < 0) continue;
        m->flash = fmaxf(0.0f, m->flash - dt);
        m->cooldown -= dt;
        m->anim += dt * (1.0f + v3_len(m->vel));
        float hdist = sqrtf((feet.x - m->pos.x) * (feet.x - m->pos.x) + (feet.z - m->pos.z) * (feet.z - m->pos.z));
        float dy = feet.y - m->pos.y;
        bool los = dist < 40.0f && phys_line_of_sight(c, eye, game_solid, g);
        if (all_dead) { m->awake = false; m->state = MS_IDLE; }
        if (!m->awake && !all_dead) {
            if (dist < 7.0f || (dist < 24.0f && los)) {
                m->awake = true;
                m->state = MS_CHASE;
                wake_neighbours(g, m);
            }
        } else if (dist > 60.0f) {
            m->awake = false;
            m->state = MS_IDLE;
        }
        vec3 want = v3(0, 0, 0);
        float speed = d->speed * (m->elite ? 1.1f : 1.0f);
        vec3 hdir = v3_norm(v3(feet.x - m->pos.x, 0, feet.z - m->pos.z));
        if (m->awake) {
            switch (m->state) {
            case MS_STUN:
                m->timer -= dt;
                if (m->timer <= 0) m->state = MS_CHASE;
                break;
            case MS_WINDUP:
                m->timer -= dt;
                m->yaw = atan2f(hdir.z, hdir.x);
                if (m->timer <= 0) {
                    if (hdist < d->reach + d->hw * m->scale + 0.6f && fabsf(dy) < 2.0f) game_hurt_player(g, tp, m->dmg, c);
                    m->cooldown = d->attack_time;
                    m->state = MS_CHASE;
                }
                break;
            case MS_CHARGE:
                m->timer -= dt;
                want = v3_scale(m->charge_dir, 12.0f);
                if (hdist < d->hw * m->scale + 0.7f && fabsf(dy) < 1.5f) {
                    game_hurt_player(g, tp, m->dmg * 1.3f, c);
                    m->state = MS_STUN;
                    m->timer = 0.6f;
                } else if (m->timer <= 0) {
                    m->state = MS_CHASE;
                }
                break;
            default: {
                m->state = MS_CHASE;
                bool moved = false;
                if (m->type == MON_WISP) {
                    if (los && dist < 18.0f && m->cooldown <= 0) {
                        enemy_fire(g, m, eye);
                        m->cooldown = d->attack_time;
                    }
                    if (los) {
                        vec3 dir3 = v3_norm(to);
                        vec3 side = v3(-dir3.z, 0, dir3.x);
                        if (dist < 7.0f) want = v3_scale(dir3, -speed);
                        else if (dist > 11.0f) want = v3_scale(dir3, speed);
                        else want = v3_scale(side, speed * sinf(m->anim * 0.5f));
                        want.y = (eye.y + 0.5f - c.y) * 1.5f;
                        moved = true;
                    }
                } else {
                    if (m->type == MON_HORNED && los && hdist > 4.0f && hdist < 14.0f && fabsf(dy) < 1.5f && m->cooldown <= 0) {
                        m->state = MS_CHARGE;
                        m->charge_dir = hdir;
                        m->timer = 1.3f;
                        m->cooldown = 3.0f;
                        m->yaw = atan2f(hdir.z, hdir.x);
                        break;
                    }
                    if (hdist < d->reach + d->hw * m->scale + 0.2f && fabsf(dy) < 1.8f && m->cooldown <= 0) {
                        m->state = MS_WINDUP;
                        m->timer = m->type == MON_BRUTE ? 0.6f : 0.35f;
                        break;
                    }
                    if (hdist < d->reach + d->hw * m->scale) { moved = true; } /* close enough: hold */
                }
                if (!moved) {
                    vec3 target;
                    if (los && dist < 5.0f) want = v3_scale(hdir, speed);
                    else if (flow_next(tp, m->pos, &target)) {
                        vec3 dir = v3_norm(v3(target.x - m->pos.x, 0, target.z - m->pos.z));
                        want = v3_scale(dir, speed);
                        if (d->flies) want.y = (target.y + 1.2f - m->pos.y) * 2.0f;
                    } else if (los) want = v3_scale(hdir, speed);
                }
                if (v3_len(want) > 0.1f) m->yaw = atan2f(want.z, want.x);
                break;
            }
            }
        }
        monster_physics(g, m, want, dt);
    }
    /* keep monsters from stacking inside each other */
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *a = &g->mons[i];
        if (!a->alive) continue;
        for (int j = i + 1; j < MAX_MONSTERS; j++) {
            Monster *b = &g->mons[j];
            if (!b->alive || fabsf(a->pos.y - b->pos.y) > 1.5f) continue;
            float dx = b->pos.x - a->pos.x, dz = b->pos.z - a->pos.z;
            float min = (MONSTERS[a->type].hw * a->scale + MONSTERS[b->type].hw * b->scale) * 0.9f;
            float d2 = dx * dx + dz * dz;
            if (d2 >= min * min || d2 < 1e-6f) continue;
            float dd = sqrtf(d2), push = (min - dd) * 0.5f;
            vec3 n = v3(dx / dd * push, 0, dz / dd * push);
            vec3 pa = v3_sub(a->pos, n), pb = v3_add(b->pos, n);
            if (phys_box_free(pa, MONSTERS[a->type].hw * a->scale, MONSTERS[a->type].h * a->scale, game_solid, g)) a->pos = pa;
            if (phys_box_free(pb, MONSTERS[b->type].hw * b->scale, MONSTERS[b->type].h * b->scale, game_solid, g)) b->pos = pb;
        }
    }
}

/* ---------- drawing ---------- */

void monsters_draw(Game *g, vec3 eye) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        if (v3_len(v3_sub(m->pos, eye)) > 90.0f) continue;
        const MonsterDef *d = &MONSTERS[m->type];
        float s = m->scale;
        vec3 light = game_light_at(g, monster_centre(m));
        /* a faint glow keeps silhouettes readable in pitch darkness */
        light = v3(fmaxf(light.x, 0.16f), fmaxf(light.y, 0.14f), fmaxf(light.z, 0.14f));
        vec3 base = m->flash > 0 ? v3(1.6f, 1.6f, 1.6f) : m->color;
        vec3 col = v3(base.x * light.x, base.y * light.y, base.z * light.z);
        vec3 dark = v3_scale(col, 0.6f);
        vec3 eyes = m->elite ? v3(1.0f, 0.25f, 0.15f) : v3(1.0f, 0.85f, 0.4f);
        if (m->state == MS_WINDUP || m->state == MS_CHARGE) eyes = v3(1.0f, 0.15f, 0.1f);
        float y = m->yaw;
        vec3 f = v3(cosf(y), 0, sinf(y)), r = v3(-sinf(y), 0, cosf(y));
        vec3 p = m->pos;
        float bob = sinf(m->anim * 6.0f) * 0.04f;
        switch (m->type) {
        case MON_CRAWLER: {
            vec3 body = v3_add(p, v3(0, 0.28f * s + bob, 0));
            batch3d_box(body, v3(0.34f * s, 0.24f * s, 0.26f * s), y, col, false);
            for (int k = -1; k <= 1; k += 2) {
                vec3 e = v3_add(v3_add(body, v3_scale(f, 0.34f * s)), v3_add(v3_scale(r, 0.12f * k * s), v3(0, 0.08f * s, 0)));
                batch3d_box(e, v3(0.035f * s, 0.035f * s, 0.035f * s), y, eyes, true);
                for (int leg = -1; leg <= 1; leg++) {
                    float ph = sinf(m->anim * 8.0f + leg + k) * 0.08f;
                    vec3 lp = v3_add(v3_add(p, v3_scale(f, (0.18f * leg + ph) * s)), v3_add(v3_scale(r, 0.3f * k * s), v3(0, 0.12f * s, 0)));
                    batch3d_box(lp, v3(0.04f * s, 0.12f * s, 0.04f * s), y, dark, false);
                }
            }
            break;
        }
        case MON_BRUTE: {
            float sw = sinf(m->anim * 3.0f) * 0.35f;
            for (int k = -1; k <= 1; k += 2) {
                vec3 leg = v3_add(v3_add(p, v3_scale(r, 0.22f * k * s)), v3_add(v3(0, 0.4f * s, 0), v3_scale(f, sw * k * 0.3f)));
                batch3d_box(leg, v3(0.14f * s, 0.4f * s, 0.14f * s), y, dark, false);
            }
            vec3 torso = v3_add(p, v3(0, 1.2f * s + bob, 0));
            batch3d_box(torso, v3(0.3f * s, 0.42f * s, 0.5f * s), y, col, false);
            float lift = m->state == MS_WINDUP ? 0.5f : 0.0f;
            for (int k = -1; k <= 1; k += 2) {
                vec3 arm = v3_add(v3_add(torso, v3_scale(r, 0.62f * k * s)), v3_add(v3(0, (-0.15f + lift) * s, 0), v3_scale(f, -sw * k * 0.3f + lift * 0.4f)));
                batch3d_box(arm, v3(0.13f * s, 0.45f * s, 0.13f * s), y, dark, false);
            }
            vec3 head = v3_add(torso, v3(0, 0.62f * s, 0));
            batch3d_box(head, v3(0.2f * s, 0.2f * s, 0.2f * s), y, col, false);
            for (int k = -1; k <= 1; k += 2)
                batch3d_box(v3_add(v3_add(head, v3_scale(f, 0.2f * s)), v3_scale(r, 0.08f * k * s)), v3(0.035f * s, 0.035f * s, 0.035f * s), y, eyes, true);
            break;
        }
        case MON_WISP: {
            vec3 cpos = v3_add(p, v3(0, 0.45f * s + sinf(m->anim * 2.0f) * 0.12f, 0));
            batch3d_octa(cpos, 0.38f * s, 0.5f * s, m->anim * 1.5f, col, false);
            batch3d_octa(cpos, 0.16f * s, 0.2f * s, -m->anim * 3.0f, v3_lerp(eyes, v3(0.6f, 0.7f, 1.0f), 0.5f), true);
            break;
        }
        case MON_HORNED: {
            vec3 body = v3_add(p, v3(0, 0.6f * s + bob, 0));
            batch3d_box(body, v3(0.55f * s, 0.32f * s, 0.32f * s), y, col, false);
            vec3 head = v3_add(body, v3_add(v3_scale(f, 0.62f * s), v3(0, 0.12f * s, 0)));
            batch3d_box(head, v3(0.2f * s, 0.22f * s, 0.22f * s), y, dark, false);
            for (int k = -1; k <= 1; k += 2) {
                vec3 base_h = v3_add(head, v3_add(v3_scale(r, 0.2f * k * s), v3(0, 0.15f * s, 0)));
                batch3d_spike(base_h, v3_add(f, v3(0, 0.6f, 0)), 0.06f * s, 0.45f * s, v3(0.85f * light.x, 0.8f * light.y, 0.7f * light.z), false);
                batch3d_box(v3_add(v3_add(head, v3_scale(f, 0.2f * s)), v3_scale(r, 0.09f * k * s)), v3(0.03f * s, 0.03f * s, 0.03f * s), y, eyes, true);
            }
            for (int k = -1; k <= 1; k += 2)
                for (int fb = -1; fb <= 1; fb += 2) {
                    float ph = sinf(m->anim * 8.0f + k * fb) * 0.1f;
                    vec3 lp = v3_add(v3_add(p, v3_scale(f, (0.35f * fb + ph) * s)), v3_add(v3_scale(r, 0.24f * k * s), v3(0, 0.15f * s, 0)));
                    batch3d_box(lp, v3(0.07f * s, 0.15f * s, 0.07f * s), y, dark, false);
                }
            break;
        }
        }
        if (m->elite) {
            /* a burning crown marks elites */
            vec3 top = v3_add(p, v3(0, d->h * s + 0.25f, 0));
            batch3d_octa(top, 0.12f, 0.12f, m->anim * 2.0f, v3(1.0f, 0.5f, 0.2f), true);
        }
    }
}

/* Other players: a simple knight figure in the player's colour, with a torch glow. */
void players_draw(Game *g, const PlayerSlot *viewer) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps == viewer) continue;
        const Player *pl = &ps->pl;
        vec3 p = pl->pos;
        float h = player_height(pl->stance);
        vec3 light = game_light_at(g, v3_add(p, v3(0, 1.0f, 0)));
        light = v3(fmaxf(light.x, 0.3f), fmaxf(light.y, 0.3f), fmaxf(light.z, 0.3f));
        vec3 base = ps->st.dead ? v3(0.3f, 0.3f, 0.3f) : ps->color;
        if (ps->st.hurt > 0.3f) base = v3_lerp(base, v3(1.0f, 0.2f, 0.2f), 0.5f);
        vec3 col = v3(base.x * light.x, base.y * light.y, base.z * light.z);
        vec3 dark = v3_scale(col, 0.55f);
        vec3 steel = v3(0.6f * light.x, 0.62f * light.y, 0.66f * light.z);
        float y = pl->yaw;
        vec3 f = v3(cosf(y), 0, sinf(y)), r = v3(-sinf(y), 0, cosf(y));
        if (ps->st.dead) {
            /* a fallen marker until they respawn */
            batch3d_box(v3_add(p, v3(0, 0.15f, 0)), v3(0.25f, 0.15f, 0.7f), y, dark, false);
            continue;
        }
        float walk = sqrtf(pl->vel.x * pl->vel.x + pl->vel.z * pl->vel.z);
        float sw = sinf(g->time * 9.0f) * fminf(1.0f, walk / 5.0f) * 0.3f;
        float s = h / 1.75f;
        for (int k = -1; k <= 1; k += 2) {
            vec3 leg = v3_add(v3_add(p, v3_scale(r, 0.12f * k)), v3_add(v3(0, 0.38f * s, 0), v3_scale(f, sw * k)));
            batch3d_box(leg, v3(0.09f, 0.38f * s, 0.09f), y, dark, false);
        }
        vec3 torso = v3_add(p, v3(0, 1.05f * s, 0));
        batch3d_box(torso, v3(0.16f, 0.32f * s, 0.26f), y, col, false);
        vec3 head = v3_add(torso, v3(0, 0.48f * s, 0));
        batch3d_box(head, v3(0.15f, 0.15f, 0.15f), y, steel, false);
        batch3d_box(v3_add(head, v3_scale(f, 0.15f)), v3(0.02f, 0.03f, 0.1f), y, v3(1.0f, 0.85f, 0.5f), true);
        /* weapon in the right hand, torch flame in the left */
        float swing = ps->st.swing > 0 ? ps->st.swing / fmaxf(ps->st.swing_len, 0.01f) : 0.0f;
        vec3 hand = v3_add(v3_add(torso, v3_scale(r, 0.3f)), v3_add(v3(0, -0.1f, 0), v3_scale(f, 0.25f + swing * 0.3f)));
        if (ps->st.weapon == WPN_SWORD)
            batch3d_spike(hand, v3_add(f, v3(0, 1.2f - swing * 2.0f, 0)), 0.04f, 0.8f, steel, false);
        else if (ps->st.weapon == WPN_BOW)
            batch3d_box(hand, v3(0.03f, 0.35f, 0.03f), y, v3(0.5f * light.x, 0.32f * light.y, 0.18f * light.z), false);
        else
            batch3d_octa(v3_add(hand, v3(0, 0.35f, 0)), 0.07f, 0.09f, g->time * 3.0f, v3(0.5f, 0.6f, 1.0f), true);
        vec3 lhand = v3_add(v3_add(torso, v3_scale(r, -0.3f)), v3_add(v3(0, 0.05f, 0), v3_scale(f, 0.2f)));
        batch3d_octa(lhand, 0.06f, 0.1f, g->time * 5.0f, v3(1.0f, 0.65f, 0.25f), true);
        /* a coloured marker above the head so you can find your friends */
        batch3d_octa(v3_add(head, v3(0, 0.45f, 0)), 0.07f, 0.1f, g->time * 2.0f, ps->color, true);
    }
}
