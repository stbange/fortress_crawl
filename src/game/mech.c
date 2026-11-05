#include "game/game.h"
#include "render/batch.h"
#include "engine/font.h"
#include "world/mech.h"

#include <ctype.h>

/* Mechanisms (M8): vault gates and their puzzles, and traps. The generator decides
 * where everything is (chunk info); this file builds working devices for the chunks
 * near the players, runs them and draws them. A solved vault is remembered in the
 * world's taken set, so its gate stays open; half-solved puzzles reset when everyone
 * has left. Levers, statues and mirrors are worked by striking them (the attack key). */

enum { DV_LEVER, DV_PLATE, DV_STATUE, DV_MIRROR, DV_EMITTER, DV_DARTS, DV_SPIKES, DV_FLAME };

static u64 vault_key(u64 ck) { return mix64(ck ^ 0x7A0170ULL) | 1; }
static u64 vault_chest_key(u64 ck) { return mix64(ck ^ 0x7A017CULL) | 1; }

static float level_mult(int level) { return 1.0f + 0.1f * level; }

/* ---------- solid cells (closed gates, statues, mirrors, crystals) ---------- */

static u64 cell_key(int x, int y, int z) {
    return (((u64)(u32)x & 0x1FFFFF) << 42 | ((u64)(u32)y & 0x1FFFFF) << 21 | ((u64)(u32)z & 0x1FFFFF)) + 1;
}

static void solid_add(Game *g, int x, int y, int z) {
    u64 k = cell_key(x, y, z);
    for (int i = (int)(mix64(k) & (MECH_CELLS - 1)), n = 0; n < MECH_CELLS; i = (i + 1) & (MECH_CELLS - 1), n++) {
        if (g->mech_cells[i] == k) return;
        if (!g->mech_cells[i]) { g->mech_cells[i] = k; g->mech_ncells++; return; }
    }
}

bool mech_solid(const Game *g, int x, int y, int z) {
    if (!g->mech_ncells) return false;
    u64 k = cell_key(x, y, z);
    for (int i = (int)(mix64(k) & (MECH_CELLS - 1)), n = 0; n < MECH_CELLS; i = (i + 1) & (MECH_CELLS - 1), n++) {
        if (g->mech_cells[i] == k) return true;
        if (!g->mech_cells[i]) return false;
    }
    return false;
}

static void rebuild_solid(Game *g) {
    memset(g->mech_cells, 0, sizeof g->mech_cells);
    g->mech_ncells = 0;
    for (int i = 0; i < MAX_VAULTS; i++) {
        MechVault *v = &g->vaults[i];
        if (!v->alive || v->open > 0.5f) continue;
        solid_add(g, v->gx, v->y, v->gz);
        solid_add(g, v->gx, v->y + 1, v->gz);
    }
    for (int i = 0; i < MAX_DEVICES; i++) {
        Device *d = &g->devs[i];
        if (!d->alive || (d->kind != DV_STATUE && d->kind != DV_MIRROR && d->kind != DV_EMITTER)) continue;
        solid_add(g, d->x, d->y, d->z);
        solid_add(g, d->x, d->y + 1, d->z);
    }
    g->mech_dirty = false;
}

/* ---------- spawning ---------- */

static Device *dev_new(Game *g) {
    for (int i = 0; i < MAX_DEVICES; i++)
        if (!g->devs[i].alive) {
            memset(&g->devs[i], 0, sizeof g->devs[i]);
            g->devs[i].alive = true;
            g->devs[i].vault = -1;
            return &g->devs[i];
        }
    return NULL;
}

static bool chunk_has_mech(const ChunkInfo *ci) { return ci->vault.y || ci->ntraps; }

static bool mech_chunk_known(Game *g, u64 ck) {
    for (int i = 0; i < g->nmech_chunks; i++)
        if (g->mech_chunks[i] == ck) return true;
    return false;
}

static bool pickup_with_key(Game *g, u64 key) {
    for (int i = 0; i < MAX_PICKUPS; i++)
        if (g->pickups[i].alive && g->pickups[i].key == key) return true;
    return false;
}

static void vault_chest(Game *g, MechVault *v) {
    u64 key = vault_chest_key(v->chunk_key);
    if (looted_has(g, key) || pickup_with_key(g, key)) return;
    pickup_spawn(g, PK_CHEST, v3(v->cx + 0.5f, (float)v->y, v->cz + 0.5f), v->level | CHEST_VAULT, NULL, key, 0);
}

static void spawn_mech_chunk(Game *g, Chunk *c) {
    const ChunkInfo *ci = &c->info;
    u64 ck = chunk_spawn_key(c->cx, c->cy, c->cz);
    int ox = c->cx * CHUNK, oy = c->cy * CHUNK, oz = c->cz * CHUNK;
    if (g->nmech_chunks >= MAX_MECH_CHUNKS) return;
    g->mech_chunks[g->nmech_chunks++] = ck;
    const Vault *vv = &ci->vault;
    if (vv->y) {
        int vi = -1;
        for (int i = 0; i < MAX_VAULTS && vi < 0; i++)
            if (!g->vaults[i].alive) vi = i;
        if (vi >= 0) {
            MechVault *v = &g->vaults[vi];
            memset(v, 0, sizeof *v);
            v->alive = true;
            v->v = *vv;
            v->chunk_key = ck;
            v->ox = ox; v->oy = oy; v->oz = oz;
            v->cx = ox + vv->x; v->cz = oz + vv->z; v->y = oy + vv->y;
            int gx, gz;
            mech_gate_cell(vv, &gx, &gz);
            v->gx = ox + gx; v->gz = oz + gz;
            v->level = ci->level;
            v->lamps = vv->lamps;
            v->solved = vv->puzzle == PZ_NONE || looted_has(g, vault_key(ck));
            v->open = v->solved ? 1.0f : 0.0f;
            for (int i = 0; i < vv->nparts; i++) v->facing[i] = vv->parts[i].start;
            for (int i = 0; i < vv->nparts; i++) {
                const MechPart *p = &vv->parts[i];
                Device *d = dev_new(g);
                if (!d) break;
                d->kind = vv->puzzle == PZ_LEVERS ? DV_LEVER : vv->puzzle == PZ_PLATES ? DV_PLATE
                        : vv->puzzle == PZ_STATUES ? DV_STATUE : i == 0 ? DV_EMITTER : DV_MIRROR;
                d->vault = vi;
                d->part = (u8)i;
                d->x = ox + p->x; d->y = oy + p->y; d->z = oz + p->z;
                d->dir = p->dir;
                d->chunk_key = ck;
                d->anim = d->kind == DV_STATUE ? (float)p->start : d->kind == DV_MIRROR ? (float)p->start : 0.0f;
            }
            vault_chest(g, v);
        }
    }
    for (int i = 0; i < ci->ntraps; i++) {
        const Trap *t = &ci->traps[i];
        Device *d = dev_new(g);
        if (!d) break;
        d->kind = t->kind == TRAP_DARTS ? DV_DARTS : t->kind == TRAP_SPIKES ? DV_SPIKES : DV_FLAME;
        d->x = ox + t->x; d->y = oy + t->y; d->z = oz + t->z;
        d->dir = t->dir;
        d->size = t->len;
        d->level = ci->level;
        d->chunk_key = ck;
        d->t = hash3f(ci->seed, t->x, t->y, t->z, 0x7A) * 4.0f; /* out of step with each other */
    }
    g->mech_dirty = true;
}

static void despawn_mech_chunk(Game *g, int idx) {
    u64 ck = g->mech_chunks[idx];
    for (int i = 0; i < MAX_DEVICES; i++)
        if (g->devs[i].alive && g->devs[i].chunk_key == ck) g->devs[i].alive = false;
    for (int i = 0; i < MAX_VAULTS; i++)
        if (g->vaults[i].alive && g->vaults[i].chunk_key == ck) g->vaults[i].alive = false;
    g->mech_chunks[idx] = g->mech_chunks[--g->nmech_chunks];
    g->mech_dirty = true;
}

static float nearest_player_dist(Game *g, vec3 p) {
    float best = 1e9f;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->players[i].active) best = fminf(best, v3_len(v3_sub(p, player_eye(&g->players[i].pl))));
    return best;
}

static void stream_mech(Game *g) {
    for (int i = 0; i < g->nmech_chunks; i++) {
        u64 ck = g->mech_chunks[i];
        /* find any device or vault of this chunk to know where it is */
        vec3 at = v3(0, 0, 0);
        bool found = false;
        for (int k = 0; k < MAX_VAULTS && !found; k++)
            if (g->vaults[k].alive && g->vaults[k].chunk_key == ck) { at = v3((float)g->vaults[k].cx, (float)g->vaults[k].y, (float)g->vaults[k].cz); found = true; }
        for (int k = 0; k < MAX_DEVICES && !found; k++)
            if (g->devs[k].alive && g->devs[k].chunk_key == ck) { at = v3((float)g->devs[k].x, (float)g->devs[k].y, (float)g->devs[k].z); found = true; }
        if (!found || nearest_player_dist(g, at) > 120.0f) { despawn_mech_chunk(g, i); i--; }
    }
    ChunkMap *m = &g->map;
    for (int i = 0; i < m->pool_cap; i++) {
        Chunk *c = &m->pool[i];
        if (!c->used || c->state != CH_READY || !chunk_has_mech(&c->info)) continue;
        vec3 centre = v3(c->cx * CHUNK + 16.0f, c->cy * CHUNK + 16.0f, c->cz * CHUNK + 16.0f);
        if (nearest_player_dist(g, centre) > 80.0f) continue;
        u64 ck = chunk_spawn_key(c->cx, c->cy, c->cz);
        if (!mech_chunk_known(g, ck)) spawn_mech_chunk(g, c);
    }
}

/* ---------- puzzles ---------- */

static int block_solid_at(void *user, int x, int y, int z) {
    Game *g = user;
    bool known;
    Block b = chunkmap_block(&g->map, x, y, z, &known);
    return !known || block_solid(b);
}

static void vault_solve(Game *g, MechVault *v, PlayerSlot *by) {
    if (v->solved) return;
    v->solved = true;
    looted_mark(g, vault_key(v->chunk_key));
    vec3 gate = v3(v->gx + 0.5f, v->y + 1.0f, v->gz + 0.5f);
    flash_add(g, gate, v3(1.2f, 1.0f, 0.6f), 9.0f, 0.8f);
    particles_burst(g, gate, v3(1.0f, 0.85f, 0.4f), 24, 3.0f, 0.05f, true);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (ps->active && v3_len(v3_sub(ps->pl.pos, gate)) < 40.0f) game_banner(ps, "SOMEWHERE NEAR, A GATE GRINDS OPEN");
    }
    (void)by;
}

/* Lamps over the gate: which are lit (bit per part). */
static u8 vault_lamps(Game *g, MechVault *v) {
    int n = v->v.nparts;
    if (v->solved) return (u8)((1u << n) - 1);
    switch (v->v.puzzle) {
    case PZ_LEVERS: return v->lamps;
    case PZ_PLATES: return (u8)((1u << v->progress) - 1);
    case PZ_STATUES: {
        u8 m = 0;
        for (int i = 0; i < n; i++)
            if (v->facing[i] == mech_statue_goal(&v->v, i)) m |= (u8)(1u << i);
        return m;
    }
    default: return 0;
    }
}

static void vault_check(Game *g, MechVault *v, PlayerSlot *by) {
    int n = v->v.nparts;
    if (v->solved || !n) return;
    u8 all = (u8)((1u << n) - 1);
    if (v->v.puzzle == PZ_BEAM) {
        v->beam_on = mech_beam(&v->v, v->facing, v->ox, v->oy, v->oz, block_solid_at, g, v->beam, &v->nbeam);
        if (v->beam_on) vault_solve(g, v, by);
    } else if (vault_lamps(g, v) == all) {
        vault_solve(g, v, by);
    }
}

static bool usable(const Device *d) { return d->kind == DV_LEVER || d->kind == DV_STATUE || d->kind == DV_MIRROR; }

static vec3 dev_centre(const Device *d) {
    if (d->kind == DV_LEVER)
        return v3(d->x + 0.5f + DIR_X[d->dir] * 0.45f, d->y + 1.1f, d->z + 0.5f + DIR_Z[d->dir] * 0.45f);
    return v3(d->x + 0.5f, d->y + 1.0f, d->z + 0.5f);
}

/* The lever, statue or mirror the player faces, within reach. */
static Device *facing_device(Game *g, PlayerSlot *ps) {
    vec3 eye = player_eye(&ps->pl), look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    Device *best = NULL;
    float bs = 0.0f;
    for (int i = 0; i < MAX_DEVICES; i++) {
        Device *d = &g->devs[i];
        if (!d->alive || !usable(d)) continue;
        if (d->vault >= 0 && g->vaults[d->vault].solved) continue;
        vec3 to = v3_sub(dev_centre(d), eye);
        float dist = v3_len(to);
        if (dist > 2.6f) continue;
        float s = v3_dot(v3_scale(to, 1.0f / fmaxf(dist, 0.01f)), look);
        if (s > 0.55f && s > bs) { bs = s; best = d; }
    }
    return best;
}

bool mech_use(Game *g, PlayerSlot *ps) {
    Device *d = facing_device(g, ps);
    if (!d) return false;
    MechVault *v = &g->vaults[d->vault];
    vec3 c = dev_centre(d);
    switch (d->kind) {
    case DV_LEVER:
        d->state ^= 1;
        v->lamps = mech_lever_pull(v->lamps, d->part, v->v.nparts);
        break;
    case DV_STATUE:
        v->facing[d->part] = (u8)((v->facing[d->part] + 1) & 3);
        break;
    case DV_MIRROR:
        v->facing[d->part] = (u8)((v->facing[d->part] + 1) & 7);
        break;
    }
    particles_burst(g, c, v3(0.75f, 0.7f, 0.6f), 6, 1.5f, 0.03f, false);
    vault_check(g, v, ps);
    return true;
}

static bool on_cell(vec3 feet, int x, int y, int z, bool grounded) {
    return grounded && (int)floorf(feet.x) == x && (int)floorf(feet.z) == z && fabsf(feet.y - y) < 0.6f;
}

static void update_plate(Game *g, Device *d) {
    MechVault *v = &g->vaults[d->vault];
    bool held = false;
    PlayerSlot *who = NULL;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (ps->active && !ps->st.dead && on_cell(ps->pl.pos, d->x, d->y, d->z, ps->pl.on_ground)) { held = true; who = ps; }
    }
    if (held && !d->state && !v->solved) {
        vec3 c = v3(d->x + 0.5f, d->y + 0.1f, d->z + 0.5f);
        if (d->part == v->progress) {
            v->progress++;
            particles_burst(g, c, v3(1.0f, 0.8f, 0.35f), 8, 1.5f, 0.04f, true);
            vault_check(g, v, who);
        } else if (d->part >= v->progress) {
            /* the wrong stone: the lamps go out and the floor spits fire */
            v->progress = 0;
            flash_add(g, c, v3(1.2f, 0.5f, 0.15f), 5.0f, 0.3f);
            particles_burst(g, c, v3(1.0f, 0.45f, 0.1f), 16, 3.0f, 0.06f, true);
            game_hurt_player(g, who, 3.0f + v->level * 0.5f, v3(c.x, c.y - 1.0f, c.z));
            game_banner(who, "THE WRONG STONE - THE LAMPS GO OUT");
        }
    }
    d->state = held;
}

/* ---------- traps ---------- */

static void fire_dart(Game *g, Device *d, float yoff) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (p->alive) continue;
        memset(p, 0, sizeof *p);
        p->alive = true;
        p->kind = PJ_ENEMY_ARROW;
        p->element = EL_NONE;
        p->owner = -1;
        float off = d->size + 0.45f;
        p->pos = v3(d->x + 0.5f + DIR_X[d->dir] * off, d->y + 1.0f + yoff, d->z + 0.5f + DIR_Z[d->dir] * off);
        p->vel = v3(-DIR_X[d->dir] * 22.0f, 1.0f, -DIR_Z[d->dir] * 22.0f);
        p->dmg = 6.0f * level_mult(d->level);
        p->life = 2.0f;
        p->last_hit = -1;
        return;
    }
}

static void trap_hit(Game *g, Device *d, vec3 c, float half, float top, float dmg, u8 *mask) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->st.dead || (mask && (*mask & (1u << i)))) continue;
        vec3 f = ps->pl.pos;
        if (fabsf(f.x - c.x) < half && fabsf(f.z - c.z) < half && f.y > d->y - 0.3f && f.y < d->y + top) {
            game_hurt_player(g, ps, dmg, v3(f.x, f.y - 1.0f, f.z));
            if (mask) *mask |= (u8)(1u << i);
        }
    }
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || MONSTERS[m->type].flies) continue;
        if (fabsf(m->pos.x - c.x) < half && fabsf(m->pos.z - c.z) < half && m->pos.y > d->y - 0.3f && m->pos.y < d->y + top) {
            if (mask && (d->hit_mons >> (i & 63) & 1)) continue;
            monster_damage(g, m, dmg, v3(m->pos.x, m->pos.y - 1.0f, m->pos.z), 1.0f, -1);
            if (mask) d->hit_mons |= 1ULL << (i & 63);
        }
    }
}

/* spikes: 3.2 s cycle: rattle, up, down */
#define SPIKE_CYCLE 3.2f
static float spike_height(float t) {
    float p = fmodf(t, SPIKE_CYCLE);
    if (p < 0.5f) return 0.08f * (p / 0.5f);
    if (p < 0.62f) return 0.08f + 0.62f * ((p - 0.5f) / 0.12f);
    if (p < 1.6f) return 0.7f;
    if (p < 2.0f) return 0.7f * (1.0f - (p - 1.6f) / 0.4f);
    return 0.0f;
}

/* flames: 4 s cycle: sputter, burn, quiet */
#define FLAME_CYCLE 4.0f
static int flame_phase(float t) {
    float p = fmodf(t, FLAME_CYCLE);
    return p < 0.7f ? 1 : p < 1.9f ? 2 : 0;
}

static void update_trap(Game *g, Device *d, float dt) {
    float prev = d->t;
    d->t += dt;
    d->cool -= dt;
    vec3 c = v3(d->x + 0.5f, (float)d->y, d->z + 0.5f);
    if (d->kind == DV_DARTS) {
        bool held = false;
        for (int i = 0; i < MAX_PLAYERS && !held; i++) {
            PlayerSlot *ps = &g->players[i];
            held = ps->active && !ps->st.dead && on_cell(ps->pl.pos, d->x, d->y, d->z, ps->pl.on_ground);
        }
        for (int i = 0; i < MAX_MONSTERS && !held; i++) {
            Monster *m = &g->mons[i];
            held = m->alive && !MONSTERS[m->type].flies && on_cell(m->pos, d->x, d->y, d->z, m->on_ground);
        }
        if (held && !d->state && d->cool <= 0) {
            fire_dart(g, d, 0.0f);
            fire_dart(g, d, 0.35f);
            fire_dart(g, d, -0.3f);
            d->cool = 1.5f;
            vec3 hole = v3(c.x + DIR_X[d->dir] * (d->size + 0.45f), c.y + 1.0f, c.z + DIR_Z[d->dir] * (d->size + 0.45f));
            particles_burst(g, hole, v3(0.5f, 0.45f, 0.4f), 5, 1.0f, 0.03f, false);
        }
        d->state = held;
        d->anim += ((held ? 1.0f : 0.0f) - d->anim) * fminf(1.0f, dt * 12.0f);
    } else if (d->kind == DV_SPIKES) {
        if (fmodf(d->t, SPIKE_CYCLE) < fmodf(prev, SPIKE_CYCLE)) { d->hit_players = 0; d->hit_mons = 0; }
        float h = spike_height(d->t);
        float s = d->size * 0.5f;
        vec3 mid = v3(d->x + s, (float)d->y, d->z + s);
        if (h > 0.4f) trap_hit(g, d, mid, s + 0.15f, 1.0f, 10.0f * level_mult(d->level), &d->hit_players);
    } else {
        int ph = flame_phase(d->t);
        bool near = nearest_player_dist(g, c) < 40.0f;
        if (ph == 1 && near && g->nparts < MAX_PARTICLES - 40 && fmodf(d->t, 0.15f) < dt)
            particles_burst(g, v3(c.x, c.y + 0.1f, c.z), v3(1.0f, 0.5f, 0.15f), 2, 1.0f, 0.03f, true);
        if (ph == 2) {
            if (near && g->nparts < MAX_PARTICLES - 40) {
                Rng r = rng_make((u64)(d->t * 1000.0f) ^ (u64)d->x);
                for (int k = 0; k < 3; k++) {
                    vec3 p = v3(c.x + (rng_float(&r) - 0.5f) * 0.5f, c.y + 0.2f, c.z + (rng_float(&r) - 0.5f) * 0.5f);
                    particles_burst(g, p, rng_chance(&r, 0.5f) ? v3(1.0f, 0.55f, 0.12f) : v3(1.0f, 0.85f, 0.3f), 1, 2.5f, 0.07f, true);
                }
            }
            if (d->cool <= 0) {
                d->cool = 0.3f;
                trap_hit(g, d, c, 0.85f, 2.4f, 4.0f * level_mult(d->level), NULL);
            }
        }
    }
}

/* ---------- lava ---------- */

static void update_lava(Game *g, PlayerSlot *ps, float dt) {
    if (ps->st.dead || !ps->pl.on_ground || ps->pl.fly) return;
    vec3 f = ps->pl.pos;
    bool known;
    Block b = chunkmap_block(&g->map, (int)floorf(f.x), (int)floorf(f.y - 0.05f), (int)floorf(f.z), &known);
    ps->lava_t -= dt;
    if (b != B_LAVA || ps->lava_t > 0) return;
    ps->lava_t = 0.4f;
    game_hurt_player(g, ps, 4.0f + ps->st.maxhp * 0.04f, v3(f.x, f.y - 2.0f, f.z));
    particles_burst(g, v3(f.x, f.y + 0.1f, f.z), v3(1.0f, 0.5f, 0.1f), 8, 2.0f, 0.05f, true);
    if (ps->banner_time <= 0) game_banner(ps, "THE LAVA BURNS");
}

/* ---------- update ---------- */

static const char *HINTS[PZ_COUNT] = {
    "",
    "A sealed vault. The lamps above the gate must all burn; each lever near here moves two of them.",
    "A sealed vault. Marked stones lie near here: step on them in the order of their marks.",
    "A sealed vault. The watchers near here must all look upon this gate. Strike one to turn it.",
    "A sealed vault with a crystal lock. Turn the mirrors until the light falls on it.",
};

void mech_update(Game *g, float dt) {
    stream_mech(g);
    for (int i = 0; i < MAX_DEVICES; i++) {
        Device *d = &g->devs[i];
        if (!d->alive) continue;
        if (d->kind == DV_PLATE) update_plate(g, d);
        else if (d->kind >= DV_DARTS) update_trap(g, d, dt);
        else if (d->kind == DV_STATUE || d->kind == DV_MIRROR) {
            /* turn smoothly toward the facing */
            int steps = d->kind == DV_STATUE ? 4 : 8;
            float want = g->vaults[d->vault].facing[d->part], diff = want - d->anim;
            while (diff > steps * 0.5f) diff -= steps;
            while (diff < -steps * 0.5f) diff += steps;
            d->anim += diff * fminf(1.0f, dt * 8.0f);
            if (d->anim < 0) d->anim += steps;
            if (d->anim >= steps) d->anim -= steps;
        } else if (d->kind == DV_LEVER) {
            d->anim += ((float)d->state - d->anim) * fminf(1.0f, dt * 10.0f);
        }
    }
    bool dirty = g->mech_dirty;
    for (int i = 0; i < MAX_VAULTS; i++) {
        MechVault *v = &g->vaults[i];
        if (!v->alive) continue;
        if (v->v.puzzle == PZ_BEAM)
            v->beam_on = mech_beam(&v->v, v->facing, v->ox, v->oy, v->oz, block_solid_at, g, v->beam, &v->nbeam) || v->solved;
        if (v->solved && v->open < 1.0f) {
            bool was = v->open > 0.5f;
            v->open = fminf(1.0f, v->open + dt * 0.6f);
            if (!was && v->open > 0.5f) dirty = true;
        }
    }
    if (dirty) rebuild_solid(g);
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active) continue;
        update_lava(g, ps, dt);
        /* the hint at a sealed gate, unless something else is being read */
        if (ps->reading_t > 0 || ps->st.dead) continue;
        for (int i = 0; i < MAX_VAULTS; i++) {
            MechVault *v = &g->vaults[i];
            if (!v->alive || v->solved) continue;
            float dx = v->gx + 0.5f - ps->pl.pos.x, dz = v->gz + 0.5f - ps->pl.pos.z;
            if (dx * dx + dz * dz > 3.2f * 3.2f || fabsf(ps->pl.pos.y - v->y) > 2.0f) continue;
            snprintf(ps->reading, sizeof ps->reading, "%s", HINTS[v->v.puzzle]);
            for (char *c = ps->reading; *c; c++) *c = (char)toupper((unsigned char)*c);
            ps->reading_t = 0.3f;
            ps->reading_key = 0;
        }
    }
}

/* ---------- drawing ---------- */

static vec3 lit(vec3 col, vec3 l) { return v3(col.x * l.x, col.y * l.y, col.z * l.z); }

static void draw_segment(vec3 a, vec3 b, float r, vec3 col) {
    vec3 d = v3_sub(b, a);
    float len = v3_len(d);
    if (len < 0.01f) return;
    vec3 dn = v3_scale(d, 1.0f / len);
    vec3 up = fabsf(dn.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    vec3 sx = v3_scale(v3_norm(v3_cross(dn, up)), r), sy = v3_scale(v3_norm(v3_cross(sx, dn)), r);
    batch3d_obox(v3_add(a, v3_scale(d, 0.5f)), sx, sy, v3_scale(d, 0.5f), col, true);
}

static void draw_vault(Game *g, MechVault *v, vec3 eye) {
    vec3 gate = v3(v->gx + 0.5f, (float)v->y, v->gz + 0.5f);
    if (v3_len(v3_sub(gate, eye)) > 60.0f) return;
    int dir = v->v.door;
    vec3 n = v3((float)DIR_X[dir], 0, (float)DIR_Z[dir]), along = v3((float)DIR_Z[dir], 0, (float)DIR_X[dir]);
    float yaw = atan2f(n.z, n.x);
    vec3 l = game_light_at(g, v3_add(gate, v3_add(v3(0, 1.0f, 0), v3_scale(n, 0.8f))));
    l = v3(fmaxf(l.x, 0.25f), fmaxf(l.y, 0.24f), fmaxf(l.z, 0.22f));
    vec3 iron = lit(v3(0.32f, 0.3f, 0.3f), l);
    /* the portcullis rises into the lintel */
    float lift = v->open * 2.0f;
    if (v->open < 1.0f) {
        for (int k = 0; k < 4; k++) {
            vec3 p = v3_add(gate, v3_add(v3_scale(along, -0.36f + k * 0.24f), v3(0, 1.0f + lift, 0)));
            batch3d_box(p, v3(0.05f, 1.0f, 0.05f), yaw, iron, false);
        }
        for (int k = 0; k < 2; k++)
            batch3d_box(v3_add(gate, v3(0, 0.5f + k * 0.9f + lift, 0)), v3(0.04f, 0.05f, 0.48f), yaw, v3_scale(iron, 0.85f), false);
    }
    /* lamps over the gate */
    int np = v->v.nparts;
    u8 lamps = vault_lamps(g, v);
    vec3 face = v3_add(gate, v3_scale(n, 0.53f));
    if (v->v.puzzle == PZ_BEAM) {
        bool on = v->beam_on;
        vec3 lock = v3_add(face, v3(0, 2.5f, 0));
        batch3d_octa(lock, 0.13f, 0.2f, g->time * (on ? 2.0f : 0.3f), on ? v3(1.0f, 0.95f, 0.7f) : lit(v3(0.4f, 0.45f, 0.5f), l), on);
    } else {
        for (int i = 0; i < np; i++) {
            vec3 p = v3_add(v3_add(face, v3(0, 2.55f, 0)), v3_scale(along, (i - (np - 1) * 0.5f) * 0.26f));
            bool on = lamps >> i & 1;
            float fl = 0.85f + 0.15f * sinf(g->time * 9.0f + i * 1.7f);
            /* an iron cup; a flame in it when lit */
            batch3d_box(v3_add(p, v3(0, -0.08f, 0)), v3(0.07f, 0.04f, 0.09f), yaw, v3(0.05f, 0.045f, 0.04f), false);
            if (on) batch3d_octa(v3_add(p, v3(0, 0.05f, 0)), 0.08f * fl, 0.16f, g->time * 4.0f + i, v3(1.0f, 0.8f * fl, 0.3f), true);
            else batch3d_box(v3_add(p, v3(0, -0.035f, 0)), v3(0.05f, 0.01f, 0.07f), yaw, v3(0.02f, 0.02f, 0.02f), false);
        }
    }
    /* the beam */
    if (v->v.puzzle == PZ_BEAM && v->nbeam >= 2) {
        float h = v->y + 1.5f;
        float pulse = 0.85f + 0.15f * sinf(g->time * 6.0f);
        vec3 col = v3(1.0f * pulse, 0.9f * pulse, 0.6f * pulse);
        for (int i = 0; i + 1 < v->nbeam; i++) {
            vec3 a = v3(v->ox + v->beam[i * 2] + 0.5f, h, v->oz + v->beam[i * 2 + 1] + 0.5f);
            vec3 b = v3(v->ox + v->beam[i * 2 + 2] + 0.5f, h, v->oz + v->beam[i * 2 + 3] + 0.5f);
            if (i + 2 == v->nbeam) {
                /* the last leg stops at the face of whatever it hit */
                vec3 d = v3_sub(b, a);
                float len = v3_len(d);
                if (len > 0.6f) b = v3_sub(b, v3_scale(d, 0.5f / len * (fabsf(d.x) > 0.01f && fabsf(d.z) > 0.01f ? 1.41f : 1.0f)));
            }
            draw_segment(a, b, 0.035f, col);
        }
    }
}

static void draw_device(Game *g, Device *d, vec3 eye) {
    vec3 c = v3(d->x + 0.5f, (float)d->y, d->z + 0.5f);
    if (v3_len(v3_sub(c, eye)) > 60.0f) return;
    vec3 l = game_light_at(g, v3_add(c, v3(0, 0.8f, 0)));
    l = v3(fmaxf(l.x, 0.22f), fmaxf(l.y, 0.21f), fmaxf(l.z, 0.2f));
    vec3 stone = lit(v3(0.46f, 0.44f, 0.42f), l), iron = lit(v3(0.3f, 0.29f, 0.3f), l);
    MechVault *v = d->vault >= 0 ? &g->vaults[d->vault] : NULL;
    switch (d->kind) {
    case DV_LEVER: {
        vec3 n = v3((float)-DIR_X[d->dir], 0, (float)-DIR_Z[d->dir]); /* out of the wall */
        vec3 base = v3(c.x - n.x * 0.47f, c.y + 1.1f, c.z - n.z * 0.47f);
        float yaw = atan2f(n.z, n.x);
        batch3d_box(base, v3(0.05f, 0.3f, 0.16f), yaw, iron, false);
        batch3d_box(v3_add(base, v3_scale(n, 0.05f)), v3(0.02f, 0.06f, 0.1f), yaw, v3(0.03f, 0.03f, 0.03f), false);
        /* the handle: up (0) or down (1), swinging out of the wall */
        float a = (0.75f - d->anim * 1.5f);
        vec3 hd = v3_add(v3_scale(n, cosf(a) * 0.55f), v3(0, sinf(a) * 0.55f, 0));
        draw_segment(v3_add(base, v3_scale(n, 0.05f)), v3_add(base, hd), 0.04f, lit(v3(0.55f, 0.38f, 0.2f), l));
        batch3d_box(v3_add(base, hd), v3(0.08f, 0.08f, 0.08f), yaw, lit(v3(0.75f, 0.6f, 0.3f), l), false);
        break;
    }
    case DV_PLATE: {
        bool done = v && (v->solved || d->part < v->progress);
        batch3d_box(v3_add(c, v3(0, d->state ? 0.01f : 0.035f, 0)), v3(0.4f, 0.035f, 0.4f), 0, stone, false);
        /* its mark: one to four strokes */
        int n = d->part + 1;
        for (int k = 0; k < n; k++)
            batch3d_box(v3_add(c, v3((k - (n - 1) * 0.5f) * 0.14f, d->state ? 0.06f : 0.085f, 0)), v3(0.04f, 0.015f, 0.22f), 0,
                        done ? v3(1.0f, 0.75f, 0.3f) : v3(0.32f, 0.24f, 0.14f), true);
        break;
    }
    case DV_STATUE: {
        float yaw = d->anim * PI_F * 0.5f;
        bool right = v && (v->solved || v->facing[d->part] == mech_statue_goal(&v->v, d->part));
        batch3d_box(v3_add(c, v3(0, 0.15f, 0)), v3(0.42f, 0.15f, 0.42f), 0, v3_scale(stone, 0.85f), false);
        batch3d_box(v3_add(c, v3(0, 0.95f, 0)), v3(0.2f, 0.65f, 0.28f), yaw, stone, false);
        batch3d_box(v3_add(c, v3(0, 1.75f, 0)), v3(0.16f, 0.17f, 0.16f), yaw, stone, false);
        vec3 f = v3(cosf(yaw), 0, sinf(yaw)), r = v3(-sinf(yaw), 0, cosf(yaw));
        /* eyes and an outstretched arm show where it looks */
        for (int k = -1; k <= 1; k += 2)
            batch3d_box(v3_add(v3_add(c, v3(0, 1.8f, 0)), v3_add(v3_scale(f, 0.165f), v3_scale(r, k * 0.07f))), v3(0.01f, 0.025f, 0.03f),
                        yaw, right ? v3(1.0f, 0.75f, 0.35f) : v3(0.35f, 0.5f, 0.6f), true);
        batch3d_box(v3_add(v3_add(c, v3(0, 1.35f, 0)), v3_add(v3_scale(f, 0.4f), v3_scale(r, 0.16f))), v3(0.22f, 0.05f, 0.05f), yaw, stone, false);
        break;
    }
    case DV_MIRROR:
    case DV_EMITTER: {
        batch3d_box(v3_add(c, v3(0, 0.5f, 0)), v3(0.22f, 0.5f, 0.22f), 0, stone, false);
        batch3d_box(v3_add(c, v3(0, 1.02f, 0)), v3(0.28f, 0.04f, 0.28f), 0, v3_scale(stone, 0.85f), false);
        vec3 top = v3_add(c, v3(0, 1.5f, 0));
        if (d->kind == DV_EMITTER) {
            batch3d_octa(top, 0.16f, 0.3f, g->time * 1.2f, v3(1.0f, 0.92f, 0.65f), true);
        } else {
            float yaw = d->anim * PI_F * 0.25f;
            bool hit = false;
            for (int i = 1; v && i + 1 < v->nbeam && !hit; i++)
                hit = v->ox + v->beam[i * 2] == d->x && v->oz + v->beam[i * 2 + 1] == d->z;
            batch3d_box(top, v3(0.03f, 0.36f, 0.36f), yaw, hit ? v3(0.95f, 0.95f, 0.85f) : v3(0.3f, 0.34f, 0.4f), true);
            batch3d_box(v3_sub(top, v3_scale(v3(cosf(yaw), 0, sinf(yaw)), 0.05f)), v3(0.03f, 0.4f, 0.4f), yaw, iron, false);
            batch3d_box(v3_add(c, v3(0, 1.1f, 0)), v3(0.05f, 0.06f, 0.05f), 0, iron, false);
        }
        break;
    }
    case DV_DARTS: {
        /* a slightly loose flagstone, and a slit in the wall */
        batch3d_box(v3_add(c, v3(0, 0.02f - d->anim * 0.015f, 0)), v3(0.42f, 0.02f, 0.42f), 0, lit(v3(0.36f, 0.34f, 0.31f), l), false);
        float off = d->size + 0.49f;
        vec3 w = v3(c.x + DIR_X[d->dir] * off, c.y + 1.0f, c.z + DIR_Z[d->dir] * off);
        float yaw = atan2f((float)DIR_Z[d->dir], (float)DIR_X[d->dir]);
        batch3d_box(w, v3(0.02f, 0.3f, 0.2f), yaw, lit(v3(0.2f, 0.18f, 0.16f), l), false);
        for (int k = -1; k <= 1; k++) batch3d_box(v3_add(w, v3(0, k * 0.3f + (k == 0 ? 0 : -0.05f * k), 0)), v3(0.025f, 0.03f, 0.03f), yaw, v3(0.02f, 0.02f, 0.02f), false);
        break;
    }
    case DV_SPIKES: {
        float h = spike_height(d->t);
        for (int cz = 0; cz < d->size; cz++)
            for (int cx = 0; cx < d->size; cx++) {
                vec3 cc = v3(d->x + cx + 0.5f, (float)d->y, d->z + cz + 0.5f);
                batch3d_box(v3_add(cc, v3(0, 0.015f, 0)), v3(0.46f, 0.015f, 0.46f), 0, lit(v3(0.17f, 0.16f, 0.15f), l), false);
                for (int k = 0; k < 4; k++) {
                    vec3 sp = v3_add(cc, v3((k & 1) * 0.5f - 0.25f, h - 0.68f + 0.02f, (k >> 1) * 0.5f - 0.25f));
                    if (h > 0.02f) batch3d_spike(sp, v3(0, 1, 0), 0.06f, 0.7f, lit(v3(0.55f, 0.52f, 0.5f), l), false);
                }
            }
        break;
    }
    case DV_FLAME: {
        batch3d_box(v3_add(c, v3(0, 0.02f, 0)), v3(0.35f, 0.02f, 0.35f), 0, lit(v3(0.12f, 0.11f, 0.1f), l), false);
        for (int k = -1; k <= 1; k++) batch3d_box(v3_add(c, v3(k * 0.18f, 0.045f, 0)), v3(0.03f, 0.01f, 0.3f), 0, iron, false);
        int ph = flame_phase(d->t);
        if (ph == 1) batch3d_box(v3_add(c, v3(0, 0.05f, 0)), v3(0.25f, 0.005f, 0.25f), 0, v3(0.6f, 0.2f, 0.05f), true);
        if (ph == 2) {
            for (int k = 0; k < 4; k++) {
                float s = 0.32f - k * 0.06f, fl = 0.8f + 0.2f * sinf(g->time * 20.0f + k);
                batch3d_octa(v3_add(c, v3(0, 0.35f + k * 0.45f, 0)), s, s * 1.6f, g->time * 5.0f + k, v3(1.0f * fl, (0.45f + k * 0.1f) * fl, 0.1f), true);
            }
        }
        break;
    }
    }
}

void mech_draw(Game *g, vec3 eye) {
    for (int i = 0; i < MAX_VAULTS; i++)
        if (g->vaults[i].alive) draw_vault(g, &g->vaults[i], eye);
    for (int i = 0; i < MAX_DEVICES; i++)
        if (g->devs[i].alive) draw_device(g, &g->devs[i], eye);
}

void mech_collect_lights(Game *g, vec3 eye, SceneParams *sp) {
    for (int i = 0; i < MAX_DEVICES && sp->ndl < MAX_DLIGHTS; i++) {
        Device *d = &g->devs[i];
        if (!d->alive) continue;
        vec3 c = v3(d->x + 0.5f, d->y + 1.0f, d->z + 0.5f);
        if (v3_len(v3_sub(c, eye)) > 30.0f) continue;
        if (d->kind == DV_FLAME && flame_phase(d->t) == 2) {
            sp->dl_pos[sp->ndl] = c;
            sp->dl_col[sp->ndl] = v3(1.3f, 0.6f, 0.2f);
            sp->dl_rad[sp->ndl++] = 8.0f;
        } else if (d->kind == DV_EMITTER) {
            sp->dl_pos[sp->ndl] = v3_add(c, v3(0, 0.5f, 0));
            sp->dl_col[sp->ndl] = v3(0.9f, 0.8f, 0.5f);
            sp->dl_rad[sp->ndl++] = 6.0f;
        }
    }
}

/* "ATTACK TO PULL THE LEVER" under the crosshair. */
void mech_draw_hud(Game *g, PlayerSlot *ps, int W, int H) {
    if (ps->st.dead) return;
    Device *d = facing_device(g, ps);
    if (!d) return;
    const char *what = d->kind == DV_LEVER ? "ATTACK TO PULL THE LEVER" : d->kind == DV_STATUE ? "ATTACK TO TURN THE STATUE" : "ATTACK TO TURN THE MIRROR";
    font_begin(W, H);
    font_text((W - font_text_width(what)) / 2, H / 2 + 14, what, 0.95f, 0.85f, 0.6f);
    font_end();
}

/* ---------- demos and the scripted test ---------- */

static MechVault *nearest_vault(Game *g, vec3 p, int puzzle) {
    MechVault *best = NULL;
    float bd = 1e9f;
    for (int i = 0; i < MAX_VAULTS; i++) {
        MechVault *v = &g->vaults[i];
        if (!v->alive || (puzzle && v->v.puzzle != puzzle)) continue;
        float d = v3_len(v3_sub(v3((float)v->cx, (float)v->y, (float)v->cz), p));
        if (d < bd) { bd = d; best = v; }
    }
    return best;
}

static void look_at(PlayerSlot *ps, vec3 target) {
    vec3 d = v3_sub(target, player_eye(&ps->pl));
    ps->pl.yaw = atan2f(d.z, d.x);
    ps->pl.pitch = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
}

/* Free cells (head height) from (x, z) in direction d8, up to max. */
static int free_run(Game *g, int x, int y, int z, int d8, int max) {
    int k = 0;
    while (k < max && !game_solid(g, x + DIR8_X[d8] * (k + 1), y + 1, z + DIR8_Z[d8] * (k + 1)) &&
           !game_solid(g, x + DIR8_X[d8] * (k + 1), y + 2, z + DIR8_Z[d8] * (k + 1)))
        k++;
    return k;
}

/* Puts the camera where a part of the nearest vault of that kind can be seen (puzzle 0:
 * the gate of the nearest vault). */
bool mech_demo_view(Game *g, PlayerSlot *ps, int puzzle) {
    MechVault *v = nearest_vault(g, ps->pl.pos, puzzle);
    if (!v) return false;
    int d = v->v.door;
    Device *part = NULL;
    for (int i = 0; i < MAX_DEVICES && puzzle; i++)
        if (g->devs[i].alive && g->devs[i].vault == (int)(v - g->vaults) && g->devs[i].part == (puzzle == PZ_BEAM ? 1 : 0)) part = &g->devs[i];
    ps->pl.fly = true;
    if (puzzle == PZ_BEAM || puzzle == PZ_STATUES) {
        /* from the gate, looking out over all the parts */
        vec3 mid = v3(0, 0, 0);
        int n = 0;
        for (int i = 0; i < MAX_DEVICES; i++)
            if (g->devs[i].alive && g->devs[i].vault == (int)(v - g->vaults)) { mid = v3_add(mid, dev_centre(&g->devs[i])); n++; }
        ps->pl.pos = v3(v->gx + 0.5f + DIR_X[d] * 1.4f, v->y + 0.8f, v->gz + 0.5f + DIR_Z[d] * 1.4f);
        look_at(ps, v3_add(v3_scale(mid, 1.0f / (n ? n : 1)), v3(0, -0.6f, 0)));
    } else if (!part) {
        ps->pl.pos = v3(v->gx + 0.5f + DIR_X[d] * 3.5f, v->y + 0.3f, v->gz + 0.5f + DIR_Z[d] * 3.5f);
        look_at(ps, v3(v->gx + 0.5f, v->y + 1.6f, v->gz + 0.5f));
    } else {
        /* back off from the part along the most open direction, a little raised */
        int bd = 0, bk = -1;
        for (int k = 0; k < 8; k++) {
            int run = free_run(g, part->x, part->y, part->z, k, 4);
            if (part->kind == DV_LEVER && (DIR8_X[k] == DIR_X[part->dir] && DIR8_Z[k] == DIR_Z[part->dir])) run = -1;
            if (run > bk) { bk = run; bd = k; }
        }
        float back = fmaxf(1.0f, bk - 0.3f) * (DIR8_X[bd] && DIR8_Z[bd] ? 1.0f : 1.0f);
        ps->pl.pos = v3(part->x + 0.5f + DIR8_X[bd] * back, part->y + 0.7f, part->z + 0.5f + DIR8_Z[bd] * back);
        look_at(ps, part->kind == DV_PLATE ? v3(part->x + 0.5f, part->y + 0.1f, part->z + 0.5f) : dev_centre(part));
    }
    SDL_Log("mech demo: vault (puzzle %d) at %d %d %d", v->v.puzzle, v->gx, v->y, v->gz);
    return true;
}

/* Shows the nearest trap of a kind (DV_SPIKES / DV_FLAME / DV_DARTS by TRAP_*) at work. */
bool mech_demo_trap(Game *g, PlayerSlot *ps, int trap) {
    int want = trap == TRAP_DARTS ? DV_DARTS : trap == TRAP_SPIKES ? DV_SPIKES : DV_FLAME;
    Device *best = NULL;
    float bd = 1e9f;
    for (int i = 0; i < MAX_DEVICES; i++) {
        Device *d = &g->devs[i];
        if (!d->alive || d->kind != want) continue;
        float dist = v3_len(v3_sub(v3((float)d->x, (float)d->y, (float)d->z), ps->pl.pos));
        if (dist < bd) { bd = dist; best = d; }
    }
    if (!best) return false;
    float s = best->kind == DV_SPIKES ? best->size * 0.5f : 0.5f;
    vec3 c = v3(best->x + s, (float)best->y, best->z + s);
    best->t = best->kind == DV_SPIKES ? 1.0f : best->kind == DV_FLAME ? 1.2f : 0.0f;
    /* stand back along the most open direction */
    ps->pl.fly = true;
    int bestd = 0, bestk = 0;
    for (int dd = 0; dd < 4; dd++) {
        int k = 0;
        while (k < 4 && !game_solid(g, (int)floorf(c.x + DIR_X[dd] * (k + 1 + s)), best->y + 1, (int)floorf(c.z + DIR_Z[dd] * (k + 1 + s)))) k++;
        if (k > bestk) { bestk = k; bestd = dd; }
    }
    float back = s + fmaxf(1.0f, bestk - 0.5f);
    ps->pl.pos = v3(c.x + DIR_X[bestd] * back, best->y + 0.6f, c.z + DIR_Z[bestd] * back);
    look_at(ps, v3_add(c, v3(0, 0.3f, 0)));
    SDL_Log("mech demo: trap %d at %d %d %d", trap, best->x, best->y, best->z);
    return true;
}

/* Stands next to a device and faces it. */
static void step_to(Game *g, PlayerSlot *ps, Device *d) {
    vec3 c = dev_centre(d);
    vec3 at = v3(d->x + 0.5f, (float)d->y, d->z + 0.5f);
    if (d->kind != DV_LEVER)
        for (int k = 0; k < 4; k++) {
            int x = d->x + DIR_X[k], z = d->z + DIR_Z[k];
            if (!game_solid(g, x, d->y, z) && !game_solid(g, x, d->y + 1, z)) { at = v3(x + 0.5f, (float)d->y, z + 0.5f); break; }
        }
    ps->pl.pos = at;
    ps->pl.fly = false;
    look_at(ps, c);
}

static bool strike(Game *g, PlayerSlot *ps, Device *d, int times) {
    for (int i = 0; i < times; i++) {
        step_to(g, ps, d);
        if (facing_device(g, ps) != d || !mech_use(g, ps)) return false;
    }
    return true;
}

/* Solves the nearest vault by working its parts as a player would, opens it and takes
 * the hoard. Logs each step; true if everything worked. */
bool mech_test_solve(Game *g, PlayerSlot *ps) {
    MechVault *v = nearest_vault(g, ps->pl.pos, 0);
    if (!v) { SDL_Log("mechtest: no vault loaded nearby"); return false; }
    int vi = (int)(v - g->vaults), n = v->v.nparts;
    Device *parts[VAULT_PARTS] = { 0 };
    for (int i = 0; i < MAX_DEVICES; i++)
        if (g->devs[i].alive && g->devs[i].vault == vi) parts[g->devs[i].part] = &g->devs[i];
    SDL_Log("mechtest: vault with puzzle %d (%d parts) at %d %d %d, solved %d, gate solid %d", v->v.puzzle, n, v->gx, v->y, v->gz, v->solved,
            mech_solid(g, v->gx, v->y, v->gz));
    bool ok = true;
    switch (v->v.puzzle) {
    case PZ_LEVERS: {
        int pick = -1;
        for (int mask = 0; mask < (1 << n) && pick < 0; mask++) {
            u8 l = v->lamps;
            for (int i = 0; i < n; i++)
                if (mask >> i & 1) l = mech_lever_pull(l, i, n);
            if (l == (1u << n) - 1) pick = mask;
        }
        for (int i = 0; i < n && ok; i++)
            if (pick >> i & 1) ok = strike(g, ps, parts[i], 1);
        SDL_Log("mechtest: pulled levers %#x, lamps %#x", pick, v->lamps);
        break;
    }
    case PZ_STATUES:
        for (int i = 0; i < n && ok; i++) ok = strike(g, ps, parts[i], (mech_statue_goal(&v->v, i) - v->facing[i]) & 3);
        SDL_Log("mechtest: turned the statues");
        break;
    case PZ_BEAM: {
        int nm = n - 1, pts[(VAULT_PARTS + 2) * 2], np, found = -1;
        u8 f[VAULT_PARTS] = { 0 };
        for (int combo = 0; combo < (1 << (3 * nm)) && found < 0; combo++) {
            for (int i = 0; i < nm; i++) f[i + 1] = (u8)((combo >> (3 * i)) & 7);
            if (mech_beam(&v->v, f, v->ox, v->oy, v->oz, block_solid_at, g, pts, &np)) found = combo;
        }
        for (int i = 0; i < nm && ok && found >= 0; i++) ok = strike(g, ps, parts[i + 1], (((found >> (3 * i)) & 7) - v->facing[i + 1]) & 7);
        SDL_Log("mechtest: turned the mirrors (solution %d)", found);
        break;
    }
    case PZ_PLATES:
        for (int i = 0; i < n; i++) {
            ps->pl.fly = false;
            ps->pl.on_ground = true;
            ps->pl.pos = v3(parts[i]->x + 0.5f, (float)parts[i]->y, parts[i]->z + 0.5f);
            mech_update(g, 0.02f);
            ps->pl.pos = v3(v->gx + 0.5f + DIR_X[v->v.door] * 1.0f, (float)v->y, v->gz + 0.5f + DIR_Z[v->v.door] * 1.0f);
            mech_update(g, 0.02f);
        }
        SDL_Log("mechtest: stepped on the plates, progress %d", v->progress);
        break;
    }
    if (!ok) SDL_Log("mechtest: could not work a part");
    for (int i = 0; i < 200; i++) mech_update(g, 1.0f / 60.0f);
    bool open = !mech_solid(g, v->gx, v->y, v->gz) && !mech_solid(g, v->gx, v->y + 1, v->gz);
    int gold = ps->st.gold;
    ps->pl.fly = false;
    ps->pl.pos = v3(v->cx + 0.5f, (float)v->y, v->cz + 0.5f);
    pickups_update(g, 0.02f);
    bool taken = looted_has(g, vault_chest_key(v->chunk_key));
    SDL_Log("mechtest: solved %d, remembered %d, gate open %d, hoard taken %d", v->solved, looted_has(g, vault_key(v->chunk_key)), open, taken);
    /* step back out to look at the open gate */
    int d = v->v.door;
    ps->pl.pos = v3(v->gx + 0.5f + DIR_X[d] * 2.5f, (float)v->y, v->gz + 0.5f + DIR_Z[d] * 2.5f);
    look_at(ps, v3(v->cx + 0.5f, v->y + 0.5f, v->cz + 0.5f));
    (void)gold;
    return ok && v->solved && open && taken;
}
