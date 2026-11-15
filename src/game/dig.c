#include "game/game.h"
#include "render/batch.h"

/* Breaking the world (docs/DESTRUCTION.md). Blocks stay on their 1 m grid; one that is hit
 * breaks into 4x4x4 pieces of 25 cm (world/carve.h) and loses the pieces the blow reaches.
 * How hard a material is decides what can break it: anything breaks soft things (earth,
 * wood, ice, plants, webs), a pickaxe breaks rock and ore, only explosions break masonry,
 * nothing breaks metal. Some places never break: the Sanctum, the courtyard before the gate,
 * the stairwells between layers, vaults and their gates, shrines. Every change is kept
 * (chunkmap_edit) and saved with the world. */

/* ---------- what may not break ---------- */

bool dig_protected(Game *g, int x, int y, int z) {
    int cx = floordiv(x, CHUNK), cy = floordiv(y, CHUNK), cz = floordiv(z, CHUNK);
    if (world_is_pocket(cx, cy, cz) || world_is_exterior(cx, cy, cz)) return true;
    Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
    if (!c || c->state != CH_READY) return true;
    int lx = x - cx * CHUNK, ly = y - cy * CHUNK, lz = z - cz * CHUNK;
    const Vault *v = &c->info.vault;
    if (v->puzzle != PZ_NONE && abs(lx - v->x) <= 3 && abs(lz - v->z) <= 3 && ly >= v->y - 1 && ly <= v->y + 4) return true;
    /* stairwells: the way between layers must stay */
    Portal ps[16];
    int n = world_chunk_portals(&g->world, cx, cy, cz, ps, 16);
    for (int i = 0; i < n; i++) {
        const Portal *p = &ps[i];
        if (p->type != P_SHAFT) continue;
        if (abs(x - p->x) <= 3 && abs(z - p->z) <= 3 && y >= p->y_low - 1 && y <= p->y_up + 4) return true;
    }
    return false;
}

/* ---------- breaking ---------- */

static vec3 mat_color(Block b) {
    switch (b) {
    case B_DIRT: return v3(0.42f, 0.33f, 0.24f);
    case B_WOOD: return v3(0.5f, 0.36f, 0.2f);
    case B_ICE: return v3(0.7f, 0.85f, 1.0f);
    case B_CRYSTAL: return v3(0.6f, 0.8f, 1.0f);
    case B_ORE: return v3(0.55f, 0.5f, 0.45f);
    case B_GLASS: return v3(0.8f, 0.9f, 1.0f);
    case B_GRASS: case B_ROOTS: return v3(0.35f, 0.5f, 0.25f);
    case B_FUNGUS: return v3(0.5f, 0.75f, 0.6f);
    case B_WEB: case B_BONES: return v3(0.85f, 0.83f, 0.78f);
    default: return v3(0.5f, 0.48f, 0.45f);
    }
}

/* What a material leaves when it is gone: ore for the forge, ingredients from plants. */
static void drop_for(Game *g, PlayerSlot *ps, Block b, int x, int y, int z, u64 h) {
    vec3 at = v3(x + 0.5f, y + 0.3f, z + 0.5f);
    switch (b) {
    case B_ORE:
        if (ps) forge_mine(g, ps, x, y, z);
        break;
    case B_FUNGUS:
        sim_gas(g, GAS_SPORES, v3(x + 0.5f, y + 0.6f, z + 0.5f), 0.35f); /* a broken fungus lets out its spores */
        if ((h & 3) == 0) pickup_spawn(g, PK_FOOD, at, (h >> 8) & 1 ? FD_GLOWCAP : FD_MUSHROOM, NULL, 0, 0);
        break;
    case B_GRASS:
        if ((h & 3) == 0) pickup_spawn(g, PK_FOOD, at, FD_HERB, NULL, 0, 0);
        break;
    case B_ROOTS:
        if ((h & 7) == 0) pickup_spawn(g, PK_FOOD, at, FD_TUBER, NULL, 0, 0);
        break;
    default: break;
    }
}

/* Sprites resting on a cell that is gone fall with it (grass, bones, fungus). */
static void loose_above(Game *g, int x, int y, int z) {
    bool known;
    Block up = chunkmap_block(&g->map, x, y + 1, z, &known);
    if (!known || BLOCKS[up].render != RENDER_CROSS || up == B_CHAIN || up == B_TORCH || up == B_LANTERN) return;
    if (BLOCK_HARDNESS[up] != HARD_SOFT || dig_protected(g, x, y + 1, z)) return;
    chunkmap_edit(&g->map, x, y + 1, z, B_AIR, 0, 0);
}

/* Removes the pieces of the cell (x, y, z) that lie within r of p, if the material yields to
 * power. Returns the number of pieces removed. */
static int break_cell(Game *g, PlayerSlot *ps, int x, int y, int z, vec3 p, float r, int power) {
    bool known;
    Block raw = chunkmap_block_raw(&g->map, x, y, z, &known);
    Block b = raw & BLOCK_MAT;
    if (!known || b == B_AIR) return 0;
    int hard = BLOCK_HARDNESS[b];
    if (hard == HARD_NEVER || hard > power || dig_protected(g, x, y, z)) return 0;
    if (BLOCKS[b].render == RENDER_CROSS || BLOCKS[b].render == RENDER_LIQUID) {
        /* a sprite (grass, web, a torch) goes at once if the blow reaches its middle */
        if (v3_len(v3_sub(v3(x + 0.5f, y + 0.5f, z + 0.5f), p)) > r + 0.45f) return 0;
        if (!chunkmap_edit(&g->map, x, y, z, B_AIR, 0, 0)) return 0;
        particles_burst(g, v3(x + 0.5f, y + 0.4f, z + 0.5f), mat_color(b), 5, 1.5f, 0.04f, false);
        drop_for(g, ps, b, x, y, z, mix64(((u64)(u32)x << 32) ^ (u64)(u32)z ^ (u64)(u32)y * 0x9E37));
        return 1;
    }
    u64 mask = (raw & BLOCK_CARVED) ? chunkmap_mask(&g->map, x, y, z) : CARVE_FULL, keep = mask;
    int removed = 0;
    for (int sy = 0; sy < 4; sy++)
        for (int sz = 0; sz < 4; sz++)
            for (int sx = 0; sx < 4; sx++) {
                int bit = carve_bit(sx, sy, sz);
                if (!(keep >> bit & 1)) continue;
                vec3 c = v3(x + (sx + 0.5f) * 0.25f, y + (sy + 0.5f) * 0.25f, z + (sz + 0.5f) * 0.25f);
                if (v3_len(v3_sub(c, p)) > r) continue;
                keep &= ~(1ULL << bit);
                removed++;
            }
    if (!removed) return 0;
    /* a few pieces left are no block any more: they crumble */
    if (__builtin_popcountll(keep) <= 6) keep = 0;
    if (!chunkmap_edit(&g->map, x, y, z, keep ? (Block)(b | BLOCK_CARVED) : B_AIR, keep, 0)) return 0;
    vec3 at = v3_add(p, v3((x + 0.5f - p.x) * 0.3f, (y + 0.5f - p.y) * 0.3f, (z + 0.5f - p.z) * 0.3f));
    particles_burst(g, at, mat_color(b), MINI(4 + removed, 14), 2.5f, 0.05f, false);
    if (!keep) {
        drop_for(g, ps, b, x, y, z, mix64(((u64)(u32)x << 32) ^ (u64)(u32)z ^ (u64)(u32)y * 0x9E37));
        loose_above(g, x, y, z);
    }
    return removed;
}

/* Pieces with nothing to hold them: a small island of blocks no longer connected to the
 * rest crumbles (floating rock left by a blast, a pillar cut at both ends). */
#define ISLAND_MAX 48
static void crumble_islands(Game *g, PlayerSlot *ps, int x0, int y0, int z0, int rad) {
    static const int N6[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    for (int dy = -rad; dy <= rad; dy++)
        for (int dz = -rad; dz <= rad; dz++)
            for (int dx = -rad; dx <= rad; dx++) {
                int sx = x0 + dx, sy = y0 + dy, sz = z0 + dz;
                bool known;
                Block b = chunkmap_block(&g->map, sx, sy, sz, &known);
                if (!known || !block_solid(b)) continue;
                /* flood the solid blocks connected to it, a few dozen at most */
                int q[ISLAND_MAX][3], n = 0;
                q[n][0] = sx; q[n][1] = sy; q[n][2] = sz; n++;
                bool anchored = false;
                for (int h = 0; h < n && !anchored; h++)
                    for (int d = 0; d < 6 && !anchored; d++) {
                        int nx = q[h][0] + N6[d][0], ny = q[h][1] + N6[d][1], nz = q[h][2] + N6[d][2];
                        Block nb = chunkmap_block(&g->map, nx, ny, nz, &known);
                        if (!known) { anchored = true; break; }
                        if (!block_solid(nb)) continue;
                        if (BLOCK_HARDNESS[nb] == HARD_NEVER || BLOCK_HARDNESS[nb] >= HARD_METAL || dig_protected(g, nx, ny, nz)) { anchored = true; break; }
                        bool seen = false;
                        for (int k = 0; k < n && !seen; k++) seen = q[k][0] == nx && q[k][1] == ny && q[k][2] == nz;
                        if (seen) continue;
                        if (n == ISLAND_MAX) { anchored = true; break; }
                        q[n][0] = nx; q[n][1] = ny; q[n][2] = nz; n++;
                    }
                if (anchored) continue;
                for (int k = 0; k < n; k++) {
                    Block cb = chunkmap_block(&g->map, q[k][0], q[k][1], q[k][2], &known);
                    if (!chunkmap_edit(&g->map, q[k][0], q[k][1], q[k][2], B_AIR, 0, 0)) continue;
                    particles_burst(g, v3(q[k][0] + 0.5f, q[k][1] + 0.5f, q[k][2] + 0.5f), mat_color(cb), 10, 3.0f, 0.07f, false);
                    drop_for(g, ps, cb, q[k][0], q[k][1], q[k][2], mix64((u64)k ^ (u64)(u32)q[k][0] << 20));
                }
                if (n) sfx_at(g, SFX_AMB_ROCKS, v3(sx + 0.5f, sy + 0.5f, sz + 0.5f), 1.0f);
            }
}

int world_break_sphere(Game *g, PlayerSlot *ps, vec3 p, float r, int power) {
    int removed = 0;
    int x0 = (int)floorf(p.x - r), x1 = (int)floorf(p.x + r), y0 = (int)floorf(p.y - r), y1 = (int)floorf(p.y + r), z0 = (int)floorf(p.z - r),
        z1 = (int)floorf(p.z + r);
    for (int y = y1; y >= y0; y--) /* top first, so what rests on a cell goes with it */
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) removed += break_cell(g, ps, x, y, z, p, r, power);
    if (removed && r >= 0.9f) crumble_islands(g, ps, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z), (int)ceilf(r) + 1);
    return removed;
}

/* ---------- explosions ---------- */

void world_explode(Game *g, vec3 p, float radius, int power, float dmg, int owner) {
    sfx_at(g, SFX_EXPLOSION, p, 1.0f);
    flash_add(g, p, v3(1.4f, 0.9f, 0.5f), radius * 5.0f, 0.5f);
    particles_burst(g, p, v3(1.0f, 0.6f, 0.2f), 40, 7.0f, 0.12f, true);
    particles_burst(g, p, v3(0.3f, 0.28f, 0.26f), 30, 4.0f, 0.16f, false);
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        vec3 c = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
        float d = v3_len(v3_sub(c, p));
        if (d > radius * 1.6f) continue;
        float k = clampf(1.0f - d / (radius * 1.6f), 0.15f, 1.0f);
        monster_damage(g, m, dmg * k, p, 9.0f * k, owner);
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->st.dead) continue;
        float d = v3_len(v3_sub(v3_add(ps->pl.pos, v3(0, 0.9f, 0)), p));
        if (d > radius * 1.4f) continue;
        float k = clampf(1.0f - d / (radius * 1.4f), 0.1f, 1.0f);
        game_hurt_player(g, ps, dmg * k * (i == owner ? 0.4f : 0.6f), p);
        vec3 away = v3_norm(v3_sub(ps->pl.pos, p));
        ps->pl.vel = v3_add(ps->pl.vel, v3(away.x * 9.0f * k, 4.0f * k, away.z * 9.0f * k));
    }
    PlayerSlot *who = owner >= 0 && owner < MAX_PLAYERS && g->players[owner].active ? &g->players[owner] : NULL;
    world_break_sphere(g, who, p, radius, power);
    /* the blast sets light to what burns, lights swamp gas around it and smokes */
    sim_element_hit(g, p, EL_FIRE, owner, 0.0f);
    sim_gas_fire(g, p, radius);
    sim_gas(g, GAS_SMOKE, p, 0.5f);
}

/* ---------- bombs ---------- */

void bomb_throw(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    if (s->bombs <= 0) {
        if (ps->banner_time <= 0) game_banner(ps, "NO BOMBS - THE SMITH SELLS THEM");
        return;
    }
    if (ps->bomb_cd > 0) return;
    for (int i = 0; i < MAX_BOMBS; i++) {
        Bomb *b = &g->bombs[i];
        if (b->alive) continue;
        vec3 look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
        b->alive = true;
        b->pos = v3_add(player_eye(&ps->pl), v3_scale(look, 0.5f));
        b->vel = v3_add(v3_scale(look, 11.0f), v3(0, 3.0f, 0));
        b->fuse = 2.2f;
        b->owner = ps->index;
        b->level = s->level;
        s->bombs--;
        ps->bomb_cd = 0.6f;
        sfx_player(g, ps, SFX_SWING_DAGGER, 0.8f);
        return;
    }
}

static bool bomb_blocked(Game *g, vec3 p) { return !phys_box_free(v3_sub(p, v3(0, 0.1f, 0)), 0.1f, 0.2f, game_solid, g); }

void bombs_update(Game *g, float dt) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        Bomb *b = &g->bombs[i];
        if (!b->alive) continue;
        b->vel.y -= 20.0f * dt;
        for (int a = 0; a < 3; a++) {
            vec3 np = b->pos;
            float d = (a == 0 ? b->vel.x : a == 1 ? b->vel.y : b->vel.z) * dt;
            if (a == 0) np.x += d; else if (a == 1) np.y += d; else np.z += d;
            if (bomb_blocked(g, np)) { /* bounce, losing most of the speed */
                if (a == 0) b->vel.x *= -0.35f; else if (a == 1) b->vel.y *= -0.3f; else b->vel.z *= -0.35f;
                if (a == 1) { b->vel.x *= 0.7f; b->vel.z *= 0.7f; }
            } else b->pos = np;
        }
        if (fmodf(b->fuse, 0.12f) < dt) particles_burst(g, v3_add(b->pos, v3(0, 0.15f, 0)), v3(1.0f, 0.7f, 0.3f), 1, 0.6f, 0.03f, true);
        if ((b->fuse -= dt) <= 0) {
            b->alive = false;
            world_explode(g, b->pos, 2.6f, HARD_MASONRY, 50.0f + 10.0f * (float)b->level, b->owner);
        }
    }
}

void bombs_draw(Game *g) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        Bomb *b = &g->bombs[i];
        if (!b->alive) continue;
        vec3 light = game_light_at(g, b->pos);
        batch3d_octa(b->pos, 0.14f, 0.14f, b->fuse * 3.0f, v3(0.16f * light.x + 0.04f, 0.15f * light.y + 0.04f, 0.15f * light.z + 0.04f), false);
        batch3d_box(v3_add(b->pos, v3(0, 0.16f, 0)), v3(0.02f, 0.05f, 0.02f), 0, v3(1.0f, 0.75f, 0.35f), true);
    }
}

/* ---------- the pickaxe ---------- */

static const char *const PICK_NAMES[4] = { "", "IRON PICKAXE", "STEEL PICKAXE", "MITHRIL PICKAXE" };
const char *dig_pick_name(const Stats *s) { return PICK_NAMES[CLAMPI(s->pick, 1, 3)]; }

/* A swing of the pickaxe: a monster in front takes a blow, else the block looked at loses
 * a handful of pieces (soft things more, rock fewer; masonry and metal only spark). */
void dig_swing(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    int lvl = CLAMPI(s->pick, 1, 3);
    s->cooldown = s->swing_len = s->swing = 0.42f - 0.06f * (float)lvl;
    s->swing_kind = 0;
    vec3 eye = player_eye(&ps->pl), look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    /* a monster close in front: the pick is a poor weapon, but a weapon */
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || m->parley || m->resident) continue;
        vec3 c = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
        vec3 to = v3_sub(c, eye);
        float d = v3_len(to);
        if (d > 2.2f + MONSTERS[m->type].hw * m->scale || v3_dot(v3_scale(to, 1.0f / fmaxf(d, 0.01f)), look) < 0.75f) continue;
        monster_damage(g, m, (6.0f + 2.0f * (float)s->level) * (float)lvl * 0.6f, ps->pl.pos, 2.0f, ps->index);
        sfx_at(g, SFX_HIT_WALL, c, 0.6f);
        return;
    }
    const float reach = 3.0f;
    float d = phys_raycast(eye, look, reach, game_solid, g);
    /* a sprite in the way (grass, web, fungus) is cut first */
    for (float t = 0.3f; t < fminf(d, reach); t += 0.25f) {
        vec3 p = v3_add(eye, v3_scale(look, t));
        int x = (int)floorf(p.x), y = (int)floorf(p.y), z = (int)floorf(p.z);
        bool known;
        Block b = chunkmap_block(&g->map, x, y, z, &known);
        if (known && BLOCKS[b].render == RENDER_CROSS && BLOCK_HARDNESS[b] == HARD_SOFT && break_cell(g, ps, x, y, z, p, 0.4f, HARD_ROCK)) {
            sfx_at(g, SFX_HIT_WALL, p, 0.5f);
            return;
        }
    }
    if (d >= reach) return;
    vec3 hit = v3_add(eye, v3_scale(look, d + 0.02f));
    int x = (int)floorf(hit.x), y = (int)floorf(hit.y), z = (int)floorf(hit.z);
    bool known;
    Block b = chunkmap_block(&g->map, x, y, z, &known);
    int hard = known ? BLOCK_HARDNESS[b] : HARD_NEVER;
    if (hard == HARD_NEVER || hard > HARD_ROCK || dig_protected(g, x, y, z)) {
        particles_burst(g, v3_sub(hit, v3_scale(look, 0.05f)), v3(1.0f, 0.85f, 0.5f), 6, 2.5f, 0.03f, true);
        sfx_at(g, SFX_HIT_WALL, hit, 1.0f);
        if (ps->banner_time <= 0)
            game_banner(ps, hard == HARD_MASONRY && !dig_protected(g, x, y, z) ? "MASONRY - THIS NEEDS A BOMB" : "THIS WILL NOT BREAK");
        return;
    }
    float r = (hard == HARD_SOFT ? 0.5f : 0.3f) + 0.06f * (float)lvl;
    int n = world_break_sphere(g, ps, hit, r, HARD_ROCK);
    sfx_at(g, n ? SFX_STEP_STONE : SFX_HIT_WALL, hit, 1.0f);
    if (b == B_ORE && n) forge_mine(g, ps, x, y, z); /* ore gives ore to the pick, too */
}

void dig_update(Game *g, PlayerSlot *ps, bool attack, bool alt) {
    Stats *s = &ps->st;
    if (ps->bomb_cd > 0) ps->bomb_cd -= 1.0f / 60.0f;
    if (attack && s->cooldown <= 0) dig_swing(g, ps);
    else if (alt) bomb_throw(g, ps);
}
