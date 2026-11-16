#include "game/game.h"
#include "render/batch.h"
#include "world/support.h"
#include "world/pocket.h"

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
    if (town_chunk(c)) return true; /* the towns of the friendly peoples (town.c) */
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
    case B_MOSS: case B_VINE: return v3(0.3f, 0.5f, 0.22f);
    case B_FLOWER: return v3(0.85f, 0.5f, 0.6f);
    case B_RFLORA_LOW: case B_RFLORA_TALL: case B_RFLORA_GLOW: return v3(0.5f, 0.6f, 0.45f);
    case B_HIVE: return v3(0.8f, 0.62f, 0.3f);
    case B_ASH: return v3(0.4f, 0.39f, 0.37f);
    case B_OBSIDIAN: return v3(0.15f, 0.1f, 0.2f);
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
    case B_FLOWER:
        if ((h & 1) == 0) pickup_spawn(g, PK_FOOD, at, FD_HERB, NULL, 0, 0);
        if (ps && ps->st.seeds < 99 && (h & 2)) ps->st.seeds++; /* (gone to seed) */
        break;
    case B_VINE:
        if ((h & 3) == 0) pickup_spawn(g, PK_FOOD, at, FD_HERB, NULL, 0, 0);
        break;
    case B_HIVE:
        pickup_spawn(g, PK_FOOD, at, FD_HONEY, NULL, 0, 0);
        if (h % 6 == 0) pickup_spawn(g, PK_FOOD, at, FD_ROYAL_JELLY, NULL, 0, 0);
        bio_hive_broken(g, at);
        break;
    case B_ROOTS:
        if ((h & 7) == 0) pickup_spawn(g, PK_FOOD, at, FD_TUBER, NULL, 0, 0);
        if (ps && ps->st.firewood < 20 && (h & 1)) ps->st.firewood++;
        break;
    case B_RFLORA_LOW: case B_RFLORA_TALL: case B_RFLORA_GLOW: /* the plants of the realms */
        gate_flora_broken(g, ps, b, x, y, z, h);
        break;
    case B_WOOD: /* firewood (P lights a campfire) */
        if (ps && ps->st.firewood < 20) ps->st.firewood = (u8)MINI(20, ps->st.firewood + 2);
        break;
    default: break;
    }
}

/* A crop struck: ripe, it gives food (twice as much where bees reached it from flowers or a
 * hive near) and seeds; not yet ripe, only its seed back. */
static void reap(Game *g, PlayerSlot *ps, int x, int y, int z, int age) {
    vec3 at = v3(x + 0.5f, y + 0.5f, z + 0.5f);
    if (age != 0) { if (ps && ps->st.seeds < 99) ps->st.seeds++; return; }
    bool bees = false;
    for (int dy = -2; dy <= 3 && !bees; dy++)
        for (int dz = -4; dz <= 4 && !bees; dz++)
            for (int dx = -4; dx <= 4 && !bees; dx++) {
                bool known;
                Block b = chunkmap_block(&g->map, x + dx, y + dy, z + dz, &known);
                bees = b == B_FLOWER || b == B_HIVE;
            }
    static const int CROPS[4] = { FD_GRAIN, FD_PEPPER, FD_ONION, FD_MELON };
    u64 h = mix64((u64)(x * 73856093) ^ (u64)(z * 19349663) ^ (u64)y);
    int n = bees ? 2 : 1;
    for (int k = 0; k < n; k++) pickup_spawn(g, PK_FOOD, v3_add(at, v3(0, 0.2f * k, 0)), CROPS[h & 3], NULL, 0, 0);
    if (ps) ps->st.seeds = (u8)MINI(99, ps->st.seeds + 1 + (int)((h >> 8) & 1));
    if (ps && bees && ps->banner_time <= 0) game_banner(ps, "A GOOD HARVEST - THE BEES FOUND IT");
}

/* A campfire from a bundle of firewood, on the ground in front of you: it burns two minutes or
 * so (its fuel in the level byte), warms (heat.c), cooks (food.c) and spreads to what burns
 * beside it like any fire. */
bool camp_light(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    if (!s->firewood) { if (ps->banner_time <= 0) game_banner(ps, "NO FIREWOOD - CHOP WOOD OR ROOTS, OR THE SHOP SELLS IT"); return false; }
    vec3 f = dir_from_angles(ps->pl.yaw, 0.0f);
    int x = (int)floorf(ps->pl.pos.x + f.x * 1.4f), z = (int)floorf(ps->pl.pos.z + f.z * 1.4f), y = (int)floorf(ps->pl.pos.y + 0.1f);
    bool known;
    for (int k = 0; k < 2 && chunkmap_block(&g->map, x, y - 1, z, &known) == B_AIR; k++) y--; /* down a step to the ground */
    if (chunkmap_block(&g->map, x, y, z, &known) != B_AIR || !block_solid(chunkmap_block(&g->map, x, y - 1, z, &known)) ||
        chunkmap_fluid(&g->map, x, y, z) || dig_protected(g, x, y, z)) {
        if (ps->banner_time <= 0) game_banner(ps, "NO PLACE FOR A FIRE HERE");
        return false;
    }
    if (!chunkmap_edit(&g->map, x, y, z, B_FIRE, 0, 120)) return false;
    s->firewood--;
    particles_burst(g, v3(x + 0.5f, y + 0.3f, z + 0.5f), v3(1.0f, 0.6f, 0.2f), 10, 1.5f, 0.05f, true);
    sfx_at(g, SFX_OIL_IGNITE, v3(x + 0.5f, y + 0.5f, z + 0.5f), 0.4f);
    char buf[64];
    snprintf(buf, sizeof buf, "A CAMPFIRE (%d FIREWOOD LEFT)", s->firewood);
    game_banner(ps, buf);
    return true;
}

/* Sprites resting on a cell that is gone fall with it (grass, bones, fungus). */
static void loose_above(Game *g, int x, int y, int z) {
    bool known;
    Block up = chunkmap_block(&g->map, x, y + 1, z, &known);
    if (!known || BLOCKS[up].render != RENDER_CROSS || up == B_CHAIN || up == B_TORCH || up == B_LANTERN) return;
    if (BLOCK_HARDNESS[up] != HARD_SOFT || dig_protected(g, x, y + 1, z)) return;
    chunkmap_edit(&g->map, x, y + 1, z, B_AIR, 0, 0);
}

/* ---------- weight and collapse (support.c) ---------- */

/* Cells that stopped bearing load since the last check (what was broken, burnt or eaten). */
static SupportCell cl_removed[256];
static u8 cl_mat[256];
static int cl_n;

void collapse_note(int x, int y, int z, int mat) {
    if (cl_n < 256) { cl_removed[cl_n] = (SupportCell){ x, y, z }; cl_mat[cl_n++] = (u8)mat; }
}

/* What a cell is for the support check: its material if it bears load (a broken block with half
 * its pieces or more), else air; -1 if not known. */
static int support_block(void *user, int x, int y, int z) {
    Game *g = user;
    bool known;
    Block raw = chunkmap_block_raw(&g->map, x, y, z, &known);
    if (!known) return -1;
    if ((raw & BLOCK_CARVED) && __builtin_popcountll(chunkmap_mask(&g->map, x, y, z)) < 32) return B_AIR;
    return raw & BLOCK_MAT;
}

static bool support_fixed(void *user, int x, int y, int z) { return dig_protected((Game *)user, x, y, z); }

/* Cells about to fall: they creak and trickle dust first (a moment to run), then drop. */
typedef struct { int x, y, z; float t; } Pending;
#define MAX_PENDING 400
static Pending pend[MAX_PENDING];
static int npend;
typedef struct { vec3 pos, vel; u8 mat; bool alive; float t; } Debris;
#define MAX_DEBRIS 256
static Debris debris[MAX_DEBRIS];
static float rumble_t;

void collapse_reset(void) { npend = 0; cl_n = 0; memset(debris, 0, sizeof debris); }

void collapse_check(Game *g, int x, int y, int z, int r) {
    if (!cl_n) return;
    static SupportCell out[300];
    int n = support_falls(support_block, support_fixed, g, x, y, z, r, cl_removed, cl_mat, cl_n, out, 300);
    cl_n = 0;
    if (!n) return;
    u64 h = mix64((u64)(x * 73856093) ^ (u64)(z * 19349663) ^ (u64)(g->time * 1000));
    for (int i = 0; i < n && npend < MAX_PENDING; i++) {
        bool dup = false;
        for (int k = 0; k < npend && !dup; k++) dup = pend[k].x == out[i].x && pend[k].y == out[i].y && pend[k].z == out[i].z;
        if (dup) continue;
        /* the lowest go first, the rest a little after: it comes down in stages */
        float stage = 1.2f + 0.6f * hash_unit(h + (u64)i) + 0.08f * (float)(out[i].y - y);
        pend[npend++] = (Pending){ out[i].x, out[i].y, out[i].z, fmaxf(0.8f, stage) };
    }
    sfx_at(g, SFX_COLLAPSE_CREAK, v3(x + 0.5f, y + 1.5f, z + 0.5f), 1.0f);
    SDL_Log("collapse: %d cells lose their support at %d %d %d", n, x, y, z);
}

int collapse_pending(void) {
    int n = npend;
    for (int i = 0; i < MAX_DEBRIS; i++) n += debris[i].alive;
    return n;
}

/* A player caught under it: heavy damage, but never killed from (nearly) full health by one collapse. */
static void crush_player(Game *g, PlayerSlot *ps, float dmg, vec3 from) {
    Stats *s = &ps->st;
    if (ps->crush_t <= 0) { ps->crush_t = 4.0f; ps->crush_guard = s->hp >= s->maxhp * 0.9f; }
    if (ps->crush_guard) dmg = fminf(dmg, s->hp - 1.0f);
    if (dmg > 0) game_hurt_player(g, ps, dmg, from);
}

void collapse_update(Game *g, float dt) {
    for (int k = 0; k < MAX_PLAYERS; k++)
        if (g->players[k].crush_t > 0) g->players[k].crush_t -= dt;
    rumble_t -= dt;
    for (int i = 0; i < npend;) {
        Pending *p = &pend[i];
        p->t -= dt;
        vec3 c = v3(p->x + 0.5f, (float)p->y, p->z + 0.5f);
        if (fmodf(p->t, 0.3f) < dt && game_nearest_player(g, c, 30.0f)) particles_burst(g, c, v3(0.5f, 0.47f, 0.42f), 1, 0.4f, 0.04f, false);
        if (p->t > 0) { i++; continue; }
        bool known;
        Block raw = chunkmap_block_raw(&g->map, p->x, p->y, p->z, &known);
        Block m = raw & BLOCK_MAT;
        if (known && block_solid(m) && !dig_protected(g, p->x, p->y, p->z) && chunkmap_edit(&g->map, p->x, p->y, p->z, B_AIR, 0, 0)) {
            for (int k = 0; k < MAX_DEBRIS; k++)
                if (!debris[k].alive) { debris[k] = (Debris){ v3(p->x + 0.5f, p->y + 0.5f, p->z + 0.5f), v3(0, -1, 0), (u8)m, true, 0 }; break; }
            loose_above(g, p->x, p->y, p->z);
        } else if (known && block_solid(m) && !dig_protected(g, p->x, p->y, p->z)) { i++; continue; } /* busy: next frame */
        pend[i] = pend[--npend];
    }
    for (int i = 0; i < MAX_DEBRIS; i++) {
        Debris *d = &debris[i];
        if (!d->alive) continue;
        d->t += dt;
        d->vel.y -= 22.0f * dt;
        vec3 np = v3_add(d->pos, v3_scale(d->vel, dt));
        /* what it falls on is hurt by its weight */
        for (int k = 0; k < MAX_MONSTERS; k++) {
            Monster *mo = &g->mons[k];
            if (mo->alive && fabsf(mo->pos.x - np.x) < 0.8f && fabsf(mo->pos.z - np.z) < 0.8f && np.y > mo->pos.y && np.y < mo->pos.y + MONSTERS[mo->type].h * mo->scale + 0.4f && d->vel.y < -3.0f) {
                monster_damage(g, mo, 25.0f + 2.0f * fabsf(d->vel.y), d->pos, 2.0f, -1);
                d->vel.y = -1.0f;
            }
        }
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *ps = &g->players[k];
            if (ps->active && !ps->st.dead && fabsf(ps->pl.pos.x - np.x) < 0.7f && fabsf(ps->pl.pos.z - np.z) < 0.7f && np.y > ps->pl.pos.y && np.y < ps->pl.pos.y + 2.0f && d->vel.y < -3.0f) {
                crush_player(g, ps, ps->st.maxhp * 0.3f, d->pos);
                d->vel.y = -1.0f;
            }
        }
        int fx = (int)floorf(np.x), fy = (int)floorf(np.y - 0.5f), fz = (int)floorf(np.z);
        if (game_solid(g, fx, fy, fz) != 0 || d->t > 6.0f) {
            /* it lands: stone as rubble, wood and metal as wreckage that is gone */
            int lx = fx, ly = fy + 1, lz = fz;
            bool known;
            Block at = chunkmap_block(&g->map, lx, ly, lz, &known);
            bool stone = SUBSTANCES[d->mat].porous && d->mat != B_WOOD;
            if (stone && known && (at == B_AIR || BLOCKS[at].render == RENDER_CROSS) && !dig_protected(g, lx, ly, lz) && !cell_occupied(g, lx, ly, lz))
                chunkmap_edit(&g->map, lx, ly, lz, B_RUBBLE, 0, 0);
            particles_burst(g, v3(np.x, (float)ly, np.z), mat_color(d->mat), 10, 3.0f, 0.07f, false);
            if (rumble_t <= 0) { sfx_at(g, SFX_COLLAPSE_RUMBLE, np, 1.0f); rumble_t = 1.5f; }
            d->alive = false;
            continue;
        }
        d->pos = np;
    }
}

void collapse_draw(Game *g) {
    for (int i = 0; i < MAX_DEBRIS; i++) {
        Debris *d = &debris[i];
        if (!d->alive) continue;
        vec3 l = game_light_at(g, d->pos), c = mat_color(d->mat);
        batch3d_box(d->pos, v3(0.45f, 0.45f, 0.45f), d->t * 2.0f, v3(c.x * l.x, c.y * l.y, c.z * l.z), false);
    }
}

/* Removes the pieces of the cell (x, y, z) that lie within r of p, if the material yields to
 * power. Returns the number of pieces removed. */
static int break_cell(Game *g, PlayerSlot *ps, int x, int y, int z, vec3 p, float r, int power) {
    bool known;
    Block raw = chunkmap_block_raw(&g->map, x, y, z, &known);
    Block b = raw & BLOCK_MAT;
    if (!known || b == B_AIR) return 0;
    int hard = BLOCK_HARDNESS[b];
    if (hard == HARD_NEVER || hard > power || (dig_protected(g, x, y, z) && b != B_CROP)) return 0; /* (what you sowed you reap, at home too) */
    if (BLOCKS[b].render == RENDER_CROSS || BLOCKS[b].render == RENDER_LIQUID) {
        /* a sprite (grass, web, a torch) goes at once if the blow reaches its middle */
        if (v3_len(v3_sub(v3(x + 0.5f, y + 0.5f, z + 0.5f), p)) > r + 0.45f) return 0;
        int age = STAIN_AGE(chunkmap_stain(&g->map, x, y, z));
        if (!chunkmap_edit(&g->map, x, y, z, B_AIR, 0, 0)) return 0;
        if (b == B_CROP) reap(g, ps, x, y, z, age);
        particles_burst(g, v3(x + 0.5f, y + 0.4f, z + 0.5f), mat_color(b), 5, 1.5f, 0.04f, false);
        drop_for(g, ps, b, x, y, z, mix64(((u64)(u32)x << 32) ^ (u64)(u32)z ^ (u64)(u32)y * 0x9E37));
        return 1;
    }
    if (raw == B_MOSS && v3_len(v3_sub(v3(x + 0.5f, y + 0.5f, z + 0.5f), p)) < r + 0.8f) {
        /* the first blow scrapes the moss off the stone */
        if (!chunkmap_edit(&g->map, x, y, z, B_ROCK, 0, 0)) return 0;
        particles_burst(g, v3(x + 0.5f, y + 0.5f, z + 0.5f), mat_color(b), 8, 1.5f, 0.05f, false);
        pickup_spawn(g, PK_FOOD, v3(x + 0.5f, y + 1.1f, z + 0.5f), FD_MOSS, NULL, 0, 0);
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
    bool bore = __builtin_popcountll(mask) >= 32; /* (bearing load: half its pieces or more) */
    int wl = (raw & BLOCK_CARVED) ? chunkmap_fluid(&g->map, x, y, z) : 0; /* water held in its gaps stays */
    if (!chunkmap_edit(&g->map, x, y, z, keep ? (Block)(b | BLOCK_CARVED) : (wl ? B_WATER : B_AIR), keep, wl)) return 0;
    if (bore && __builtin_popcountll(keep) < 32) collapse_note(x, y, z, b);
    vec3 at = v3_add(p, v3((x + 0.5f - p.x) * 0.3f, (y + 0.5f - p.y) * 0.3f, (z + 0.5f - p.z) * 0.3f));
    particles_burst(g, at, mat_color(b), MINI(4 + removed, 14), 2.5f, 0.05f, false);
    if (!keep) {
        drop_for(g, ps, b, x, y, z, mix64(((u64)(u32)x << 32) ^ (u64)(u32)z ^ (u64)(u32)y * 0x9E37));
        loose_above(g, x, y, z);
    }
    return removed;
}

int world_break_sphere(Game *g, PlayerSlot *ps, vec3 p, float r, int power) {
    int removed = 0;
    int x0 = (int)floorf(p.x - r), x1 = (int)floorf(p.x + r), y0 = (int)floorf(p.y - r), y1 = (int)floorf(p.y + r), z0 = (int)floorf(p.z - r),
        z1 = (int)floorf(p.z + r);
    for (int y = y1; y >= y0; y--) /* top first, so what rests on a cell goes with it */
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) removed += break_cell(g, ps, x, y, z, p, r, power);
    if (removed) collapse_check(g, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z), MINI(SUPPORT_MAX_R, (int)ceilf(r) + 5));
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
    /* the walls round it are scorched black */
    for (int k = 0; k < 40; k++) {
        u64 h = mix64((u64)(p.x * 1000) ^ ((u64)(p.z * 1000) << 21) ^ (u64)k * 0x9E37);
        vec3 d = v3_norm(v3(hash_unit(h) - 0.5f, hash_unit(h >> 16) - 0.5f, hash_unit(h >> 32) - 0.5f));
        for (float t = 0.5f; t < radius + 1.0f; t += 0.5f) {
            vec3 q = v3_add(p, v3_scale(d, t));
            int x = (int)floorf(q.x), y = (int)floorf(q.y), z = (int)floorf(q.z);
            bool known;
            Block b = chunkmap_block(&g->map, x, y, z, &known);
            if (!known) break;
            if (block_solid(b)) {
                if (BLOCKS[b].render == RENDER_CUBE && !dig_protected(g, x, y, z)) {
                    u8 st = chunkmap_stain(&g->map, x, y, z);
                    chunkmap_set_stain(&g->map, x, y, z, (u8)((st & ~(3 << 4)) | (3 << 4)));
                }
                break;
            }
        }
    }
    /* the blast sets light to what burns, lights swamp gas around it and smokes */
    sim_element_hit(g, p, EL_FIRE, owner, 0.0f);
    sim_gas_fire(g, p, radius);
    sim_gas(g, GAS_SMOKE, p, 0.5f);
}

/* ---------- bombs ---------- */

static const char *const THROW_NAMES[THROW_COUNT] = { "BOMB", "OIL FLASK", "FROST SALT", "GROWTH TONIC" };
const char *throw_name(int kind) { return THROW_NAMES[CLAMPI(kind, 0, THROW_COUNT - 1)]; }

static int throw_count(const Stats *s, int kind) { return kind == THROW_BOMB ? s->bombs : s->flasks[kind - 1]; }

void throw_cycle(PlayerSlot *ps) {
    Stats *s = &ps->st;
    /* the next kind there is some of (else just the next) */
    for (int k = 1; k <= THROW_COUNT; k++) {
        int c = (s->throw_sel + k) % THROW_COUNT;
        if (throw_count(s, c) > 0 || k == THROW_COUNT) { s->throw_sel = (u8)c; break; }
    }
    char buf[64];
    snprintf(buf, sizeof buf, "X THROWS: %s (%d)", THROW_NAMES[s->throw_sel], throw_count(s, s->throw_sel));
    game_banner(ps, buf);
}

static void throw_kind(Game *g, PlayerSlot *ps, int kind);
void throw_selected(Game *g, PlayerSlot *ps) { throw_kind(g, ps, ps->st.throw_sel); }
void bomb_throw(Game *g, PlayerSlot *ps) { throw_kind(g, ps, THROW_BOMB); }

static void throw_kind(Game *g, PlayerSlot *ps, int kind) {
    Stats *s = &ps->st;
    if (kind != THROW_BOMB) {
        if (s->flasks[kind - 1] == 0) {
            if (ps->banner_time <= 0) game_banner(ps, "NONE LEFT - THE SHOP SELLS FLASKS (C CHOOSES WHAT TO THROW)");
            return;
        }
        if (ps->bomb_cd > 0) return;
        for (int i = 0; i < MAX_BOMBS; i++) {
            Bomb *b = &g->bombs[i];
            if (b->alive) continue;
            vec3 look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
            *b = (Bomb){ true, v3_add(player_eye(&ps->pl), v3_scale(look, 0.5f)), v3_add(v3_scale(look, 12.0f), v3(0, 2.5f, 0)), 5.0f, ps->index, s->level, (u8)kind };
            s->flasks[kind - 1]--;
            ps->bomb_cd = 0.5f;
            sfx_player(g, ps, SFX_SWING_DAGGER, 0.7f);
            return;
        }
        return;
    }
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
        b->kind = THROW_BOMB;
        s->bombs--;
        ps->bomb_cd = 0.6f;
        sfx_player(g, ps, SFX_SWING_DAGGER, 0.8f);
        return;
    }
}

static bool bomb_blocked(Game *g, vec3 p) { return !phys_box_free(v3_sub(p, v3(0, 0.1f, 0)), 0.1f, 0.2f, game_solid, g); }

/* A flask breaks: what is in it acts on the world where it lands (docs/SIMULATION.md). */
static void flask_break(Game *g, Bomb *b) {
    b->alive = false;
    vec3 p = b->pos;
    int x = (int)floorf(p.x), y = (int)floorf(p.y), z = (int)floorf(p.z);
    for (int k = 0; k < 4 && sim_fluid(g, x, y, z); k++) y++; /* on water: on top of it */
    sfx_at(g, SFX_FLASK_BREAK, p, 0.8f);
    switch (b->kind) {
    case THROW_OIL: {
        /* a pool of oil, spilling round where it broke */
        particles_burst(g, p, v3(0.3f, 0.22f, 0.1f), 12, 2.5f, 0.05f, false);
        for (int k = 0; k < 5; k++) {
            int dx = k == 1 ? 1 : k == 2 ? -1 : 0, dz = k == 3 ? 1 : k == 4 ? -1 : 0;
            bool known;
            Block c = chunkmap_block(&g->map, x + dx, y, z + dz, &known);
            if (known && (c == B_AIR || c == B_OIL) && !dig_protected(g, x + dx, y, z + dz)) chunkmap_edit(&g->map, x + dx, y, z + dz, B_OIL, 0, k ? 3 : 6);
        }
        break;
    }
    case THROW_FROST:
        particles_burst(g, p, v3(0.85f, 0.95f, 1.0f), 24, 3.0f, 0.05f, true);
        for (int k = 0; k < 5; k++) {
            float a = k * 2.0f * PI_F / 5.0f;
            sim_element_hit(g, k ? v3_add(p, v3(cosf(a) * 1.8f, 0, sinf(a) * 1.8f)) : p, EL_FROST, b->owner, 30.0f);
        }
        heat_add(g, p, -80.0f);
        for (int k = 0; k < MAX_MONSTERS; k++)
            if (g->mons[k].alive && v3_len(v3_sub(g->mons[k].pos, p)) < 3.0f) element_hit(g, &g->mons[k], EL_FROST, 15.0f, p, 0.0f, b->owner);
        break;
    case THROW_TONIC:
        particles_burst(g, p, v3(0.4f, 0.9f, 0.35f), 24, 2.0f, 0.05f, true);
        bio_tonic(g, p, 3.0f);
        break;
    default: break;
    }
}

void bombs_update(Game *g, float dt) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        Bomb *b = &g->bombs[i];
        if (!b->alive) continue;
        b->vel.y -= 20.0f * dt;
        if (b->kind != THROW_BOMB) {
            /* a flask breaks on the first thing it meets */
            vec3 np = v3_add(b->pos, v3_scale(b->vel, dt));
            bool hit = bomb_blocked(g, np) || (b->fuse -= dt) <= 0 ||
                       sim_fluid(g, (int)floorf(np.x), (int)floorf(np.y), (int)floorf(np.z)) > 0; /* (or on water's surface) */
            for (int k = 0; k < MAX_MONSTERS && !hit; k++)
                hit = g->mons[k].alive && v3_len(v3_sub(v3_add(g->mons[k].pos, v3(0, 0.6f, 0)), np)) < 0.8f;
            if (hit) flask_break(g, b);
            else b->pos = np;
            continue;
        }
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
        if (b->kind != THROW_BOMB) {
            static const vec3 FC[4] = { { 0, 0, 0 }, { 0.45f, 0.32f, 0.12f }, { 0.85f, 0.92f, 1.0f }, { 0.35f, 0.85f, 0.3f } };
            vec3 c = FC[b->kind];
            batch3d_octa(b->pos, 0.09f, 0.13f, b->fuse * 6.0f, v3(c.x * light.x + 0.05f, c.y * light.y + 0.05f, c.z * light.z + 0.05f), false);
            continue;
        }
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

/* Sowing: the earth looked at (within 3 m) gets a seed in the cell above it. Allowed in the
 * Sanctum too (its garden is yours to sow). */
static bool sow(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    if (!s->seeds) { if (ps->banner_time <= 0) game_banner(ps, "NO SEEDS - CUT FLOWERS FOR THEM, OR THE SHOP SELLS THEM"); return false; }
    vec3 eye = player_eye(&ps->pl), look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    for (float t = 0.3f; t < 3.0f; t += 0.1f) {
        vec3 q = v3_add(eye, v3_scale(look, t));
        int x = (int)floorf(q.x), y = (int)floorf(q.y), z = (int)floorf(q.z);
        bool known;
        Block b = chunkmap_block(&g->map, x, y, z, &known);
        if (!known) return false;
        if (!block_solid(b) && b != B_ASH && b != B_GRASS) continue;
        /* the cell to sow into: this one if it is ash or grass on earth, else the one above */
        int sy = block_solid(b) ? y + 1 : y;
        Block below = chunkmap_block(&g->map, x, sy - 1, z, &known), at = chunkmap_block(&g->map, x, sy, z, &known);
        bool home = world_is_pocket(floordiv(x, CHUNK), floordiv(sy, CHUNK), floordiv(z, CHUNK));
        if (!(SUBSTANCES[below].nutrient && block_solid(below)) || !(at == B_AIR || at == B_ASH || at == B_GRASS) ||
            (dig_protected(g, x, sy, z) && !home)) {
            if (ps->banner_time <= 0) game_banner(ps, "SEEDS NEED EARTH (OR MOSS, OR ASH) TO GROW IN");
            return false;
        }
        if (!chunkmap_edit_stain(&g->map, x, sy, z, B_CROP, 0, 0, 1 << 6)) return false;
        s->seeds--;
        s->cooldown = 0.4f;
        particles_burst(g, v3(x + 0.5f, sy + 0.1f, z + 0.5f), v3(0.45f, 0.35f, 0.2f), 6, 1.0f, 0.04f, false);
        char buf[64];
        snprintf(buf, sizeof buf, "SOWN (%d SEEDS LEFT) - IT NEEDS LIGHT AND WATER NEAR", s->seeds);
        game_banner(ps, buf);
        return true;
    }
    return false;
}

void dig_update(Game *g, PlayerSlot *ps, bool attack, bool alt) {
    Stats *s = &ps->st;
    if (ps->bomb_cd > 0) ps->bomb_cd -= 1.0f / 60.0f;
    if (attack && s->cooldown <= 0 && ps->pl.stance == STANCE_CROUCH) sow(g, ps); /* crouching: sow */
    else if (attack && s->cooldown <= 0) dig_swing(g, ps);
    else if (alt) throw_selected(g, ps); /* (what C, the d-pad or the > button chose with the pickaxe out) */
}
