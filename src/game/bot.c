#include "game/bot.h"

#include "game/skills.h"
#include "world/layout.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* An automatic player for playtests (--bot STYLE --bot-minutes N --bot-log FILE).
 * It plays player 1 through the same inputs a person uses (move, look, attack, cast,
 * drink, equip, buy skills) and writes what happens to a log: a status line every 15 s
 * of game time, events (deaths and their cause, levels, chapters, getting stuck) and a
 * summary at the end. It finds its way with a breadth-first search over the cells a
 * player can stand in, around itself. */

enum { BS_EXPLORER, BS_DELVER, BS_ARCHER, BS_CASTER, BS_QUESTER, BS_CAUTIOUS, BS_COUNT };
static const char *STYLE_NAMES[BS_COUNT] = { "explorer", "delver", "archer", "caster", "quester", "cautious" };

/* ---------- the search grid ---------- */

#define BR 28 /* horizontal radius */
#define BV 9  /* vertical radius */
#define BW (2 * BR + 1)
#define BH (2 * BV + 1)
#define BN (BW * BW * BH)

static u8 walk_cache[BN];   /* 0 unknown, 1 stand, 2 low (crawl), 3 no */
static u16 dist_[BN];
static int parent_[BN];
static int queue_[BN];
static int ox, oy, oz;      /* grid origin (world cell of index 0) */

static int idx(int x, int y, int z) {
    x -= ox; y -= oy; z -= oz;
    if (x < 0 || y < 0 || z < 0 || x >= BW || z >= BW || y >= BH) return -1;
    return (y * BW + z) * BW + x;
}
static void cell_of(int i, int *x, int *y, int *z) {
    *x = i % BW + ox;
    *z = (i / BW) % BW + oz;
    *y = i / (BW * BW) + oy;
}

/* ---------- state ---------- */

typedef struct Bot {
    int style;
    float minutes;
    FILE *log;
    float t, next_status, next_plan, next_skills, stuck_check;
    int path[512], npath, pidx;
    int goal_kind;   /* 0 explore, 1 pickup, 2 monster, 3 realm */
    vec3 goal;
    vec3 last_pos;
    int stuck_windows, stuck_total, hard_stuck;
    int deaths;
    bool was_dead;
    int last_level, last_chapter;
    float fight_t, fight_time_total;
    int fights;
    Monster *target;
    float target_since, target_hp0;
    Monster *ignore[8];
    float ignore_until[8];
    int stalemates;
    float last_fall, low_hp_since;
    int close_calls;
    float next_shop;
    bool shopped_realm;
    int realm_rested;
    float avoid_until;
    float vertical_until; /* the thread's end is reached: now go up or down the stairs */
    bool low;
    int realm_trips, realm_phase, realm_failed;
    int builds;
    float next_home; /* a trip home to spend gold, not before this */
    Critter *prey;   /* an animal it hunts, until hunt_until */
    float hunt_until, next_hunt, next_cook, next_brew;
    u64 prey_key, skip_prey[8];  /* animals it gave up on */
    float skip_until[8];
    int nskip;
    int pick_i;                  /* the pickup it is going for, and since when */
    float pick_since;
    int skip_pick[8];            /* pickups it could not get to */
    float skip_pick_until[8];
    int nskip_pick;
    float max_dist, max_depth, max_height;
    int max_tier;
    float time_in_fight;
    int replans, no_goal;
    int utility_casts[64];
} Bot;

static Bot B;

/* visited coarse cells: 8 x 4 x 8 blocks, an open-addressing set with visit counts */
#define VCAP (1 << 16)
static struct { int x, y, z; int n; bool used; } visited[VCAP];
static int nvisited;

static u32 vhash(int x, int y, int z) { return (u32)(x * 73856093 ^ y * 19349663 ^ z * 83492791) & (VCAP - 1); }
static int *visits(int x, int y, int z, bool add) {
    int cx = floordiv(x, 8), cy = floordiv(y, 4), cz = floordiv(z, 8);
    u32 h = vhash(cx, cy, cz);
    for (int k = 0; k < VCAP; k++) {
        u32 i = (h + k) & (VCAP - 1);
        if (!visited[i].used) {
            if (!add || nvisited > VCAP * 3 / 4) return NULL;
            visited[i].used = true;
            visited[i].x = cx; visited[i].y = cy; visited[i].z = cz;
            visited[i].n = 0;
            nvisited++;
            return &visited[i].n;
        }
        if (visited[i].x == cx && visited[i].y == cy && visited[i].z == cz) return &visited[i].n;
    }
    return NULL;
}
static int visit_count(int x, int y, int z) {
    int *n = visits(x, y, z, false);
    return n ? *n : 0;
}

static const char *spell_name(int id) {
    static SpellInfo si;
    spell_info(id, &si);
    return si.name;
}

static void logf_(const char *fmt, ...) {
    if (!B.log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(B.log, fmt, ap);
    va_end(ap);
    fflush(B.log);
}

/* ---------- walking cells ---------- */

static Game *G;

static int walk(int x, int y, int z) {
    int i = idx(x, y, z);
    if (i < 0) return 3;
    if (walk_cache[i]) return walk_cache[i];
    int r = 3;
    int below = game_solid(G, x, y - 1, z), here = game_solid(G, x, y, z);
    if (below == 1 && here == 0) {
        bool known;
        Block b = chunkmap_block(&G->map, x, y - 1, z, &known);
        Block here_b = chunkmap_block(&G->map, x, y, z, &known);
        if (b != B_LAVA && b != B_BRAZIER && here_b != B_FIRE) r = game_solid(G, x, y + 1, z) == 0 ? 1 : 2;
    }
    walk_cache[i] = (u8)r;
    return r;
}

static bool air(int x, int y, int z) { return game_solid(G, x, y, z) == 0; }

static void bfs(vec3 feet) {
    int sx = (int)floorf(feet.x), sy = (int)floorf(feet.y + 0.05f), sz = (int)floorf(feet.z);
    ox = sx - BR; oy = sy - BV; oz = sz - BR;
    memset(walk_cache, 0, sizeof walk_cache);
    memset(dist_, 0xFF, sizeof dist_);
    int s = idx(sx, sy, sz);
    /* standing on a slab or in mid-air: start from the cell below if that one is standable */
    if (walk(sx, sy, sz) == 3 && walk(sx, sy - 1, sz) != 3) s = idx(sx, --sy, sz);
    int qh = 0, qt = 0;
    dist_[s] = 0;
    parent_[s] = -1;
    queue_[qt++] = s;
    static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DZ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    while (qh < qt) {
        int c = queue_[qh++], x, y, z;
        cell_of(c, &x, &y, &z);
        for (int d = 0; d < 8; d++) {
            int nx = x + DX[d], nz = z + DZ[d];
            /* diagonals (spiral stairs): only past two open corners */
            if (d >= 4 && (!air(x + DX[d], y, z) || !air(x, y, z + DZ[d]))) continue;
            int cand[8] = { 0 }, nc = 0;
            if (walk(nx, y, nz) != 3) cand[nc++] = idx(nx, y, nz);
            else if (walk(nx, y + 1, nz) != 3 && air(x, y + 1, z) && air(nx, y + 1, nz)) cand[nc++] = idx(nx, y + 1, nz); /* step up */
            else if (air(nx, y, nz)) { /* (a low ceiling over the step: the player ducks) */
                for (int dy = 1; dy <= 5; dy++) { /* drop down */
                    if (!air(nx, y - dy + 1, nz)) break;
                    if (walk(nx, y - dy, nz) != 3) { cand[nc++] = idx(nx, y - dy, nz); break; }
                }
            }
            for (int k = 0; k < nc; k++) {
                int n = cand[k];
                if (n < 0 || dist_[n] != 0xFFFF) continue;
                dist_[n] = (u16)(dist_[c] + 1);
                parent_[n] = c;
                queue_[qt++] = n;
            }
        }
    }
}

static bool set_path_to(int goal_i) {
    if (goal_i < 0 || dist_[goal_i] == 0xFFFF) return false;
    int n = 0, c = goal_i;
    int tmp[2048];
    while (c >= 0 && n < 2048) { tmp[n++] = c; c = parent_[c]; }
    B.npath = 0;
    for (int k = n - 1; k >= 0 && B.npath < 512; k--) B.path[B.npath++] = tmp[k];
    B.pidx = 1;
    return true;
}

static int reach_index(vec3 p) {
    int x = (int)floorf(p.x), y = (int)floorf(p.y + 0.05f), z = (int)floorf(p.z);
    for (int dy = 0; dy <= 3; dy++) {
        int i = idx(x, y - dy, z);
        if (i >= 0 && dist_[i] != 0xFFFF) return i;
        i = idx(x, y + dy, z);
        if (i >= 0 && dist_[i] != 0xFFFF) return i;
    }
    /* a neighbour cell will do (pickups lie at the edge of a cell) */
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -2; dy <= 1; dy++) {
                int i = idx(x + dx, y + dy, z + dz);
                if (i >= 0 && dist_[i] != 0xFFFF) return i;
            }
    return -1;
}

/* ---------- world facts ---------- */

static int tier_at(vec3 p) {
    int tier = 0;
    world_region(&G->world, floordiv((int)floorf(p.x), CHUNK), floordiv((int)floorf(p.y), CHUNK), floordiv((int)floorf(p.z), CHUNK), &tier);
    return tier;
}

static float gate_dist(vec3 p) { return v3_len(v3_sub(p, G->start_pos)); }

static vec3 mon_centre(const Monster *m) { return v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0)); }

/* ---------- planning ---------- */

static bool want_pickup(const PlayerSlot *ps, const Pickup *p) {
    const Stats *s = &ps->st;
    switch (p->kind) {
    case PK_GOLD: case PK_SCROLL: case PK_RELIC: case PK_CHEST: case PK_QUEST: case PK_KEYSTONE: return true;
    case PK_POTION_HP: return healing_held(s) < POTION_HP_MAX;
    case PK_POTION_MP: return s->potions_mp < POTION_MP_MAX;
    case PK_WEAPON: {
        for (int i = 0; i < BAG_SIZE; i++) if (!s->bag[i].used) return true;
        return false;
    }
    case PK_TRACE: return B.style == BS_QUESTER && ps->reading_key != p->key;
    case PK_FOOD: return s->food[p->amount] < 20;
    case PK_REAGENT: return s->reag[p->amount] < 20;
    case PK_FORMULA: return true;
    default: return false;
    }
}

static void plan(PlayerSlot *ps) {
    B.replans++;
    vec3 feet = ps->pl.pos;
    bfs(feet);
    B.npath = 0;
    /* in the Sanctum: feast, bath, then the door home */
    if (realm_has(feet)) {
        vec3 to;
        const Stats *s = &ps->st;
        int ds = sanctum_stage(G, SP_DINING), bs = sanctum_stage(G, SP_BATH);
        if (s->well_fed_t <= 0 && ds >= 1) to = v3(POCKET_TABLE_X0 - 1.5f, POCKET_G, (ds == 1 ? POCKET_TABLE_Z0_SHORT + POCKET_TABLE_Z1_SHORT : POCKET_TABLE_Z0 + POCKET_TABLE_Z1) * 0.5f);
        else if (s->refreshed_t <= 0 && bs >= 1) to = v3((POCKET_BATH_X0 + POCKET_BATH_X1) * 0.5f, POCKET_G - 1, (POCKET_BATH_Z0 + POCKET_BATH_Z1) * 0.5f);
        else if (B.realm_rested < 1) to = v3(POCKET_BED_X0 - 1.5f, POCKET_G, (POCKET_BED_Z0 + POCKET_BED_Z1) * 0.5f);
        else to = v3(POCKET_EXIT_X, POCKET_G, POCKET_EXIT_Z);
        /* the realm is bigger than the search: walk toward the far goal through the nearest cell */
        int best = -1;
        float bd = 1e9f;
        for (int i = 0; i < BN; i++) {
            if (dist_[i] == 0xFFFF) continue;
            int x, y, z;
            cell_of(i, &x, &y, &z);
            float d = v3_len(v3_sub(v3(x + 0.5f, (float)y, z + 0.5f), to)) + dist_[i] * 0.02f;
            if (d < bd) { bd = d; best = i; }
        }
        B.goal_kind = 3;
        B.goal = to;
        set_path_to(best);
        return;
    }
    /* gold to spend: the nearest shrine in reach */
    if (ps->st.gold > 800 && B.t >= B.next_shop) {
        int bi = -1;
        for (int k = 0; k < G->nshrines; k++) {
            int ri = reach_index(v3_add(G->shrines[k], v3(1.0f, -0.5f, 0)));
            if (ri >= 0 && dist_[ri] < 50 && (bi < 0 || dist_[ri] < dist_[bi])) bi = ri;
        }
        if (bi >= 0) {
            B.goal_kind = 1;
            set_path_to(bi);
            int x, y, z;
            cell_of(bi, &x, &y, &z);
            B.goal = v3(x + 0.5f, (float)y, z + 0.5f);
            return;
        }
    }
    /* things worth picking up */
    int best = -1, best_i = -1;
    float bscore = 1e9f;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &G->pickups[i];
        if (!p->alive || !want_pickup(ps, p)) continue;
        bool skip = false;
        for (int k = 0; k < 8; k++) skip |= B.skip_pick[k] == i + 1 && B.t < B.skip_pick_until[k];
        if (skip) continue;
        if (v3_len(v3_sub(p->pos, feet)) > BR) continue;
        int ri = reach_index(p->pos);
        if (ri < 0) continue;
        float sc = dist_[ri] - (p->kind == PK_CHEST ? 15.0f : 0.0f) - (p->kind == PK_RELIC ? 20.0f : 0.0f);
        int max_d = B.style == BS_QUESTER && ps->thread.n > 1 ? 10 : 60; /* the quester keeps to its thread */
        if (sc < bscore && dist_[ri] < max_d) { bscore = sc; best = ri; best_i = i; }
    }
    if (best >= 0) {
        /* the same one for long: it cannot be picked up from where the path ends */
        if (best_i != B.pick_i) { B.pick_i = best_i; B.pick_since = B.t; }
        else if (B.t - B.pick_since > 25.0f) {
            int k = B.nskip_pick++ % 8;
            B.skip_pick[k] = best_i + 1;
            B.skip_pick_until[k] = B.t + 300.0f;
            logf_("%7.1f PICKUP out of reach (kind %d at %.1f %.1f %.1f)\n", B.t, G->pickups[best_i].kind, G->pickups[best_i].pos.x,
                  G->pickups[best_i].pos.y, G->pickups[best_i].pos.z);
            B.pick_i = -1;
        }
        B.goal_kind = 1;
        set_path_to(best);
        int x, y, z;
        cell_of(best, &x, &y, &z);
        B.goal = v3(x + 0.5f, (float)y, z + 0.5f);
        return;
    }
    /* the quester follows the Golden Thread: the farthest point of it within reach */
    if (B.style == BS_QUESTER && ps->thread.n > 1 && ps->thread.t < ps->thread.life) {
        int bi = -1;
        for (int k = ps->thread.n - 1; k >= 0 && bi < 0; k--) {
            int ri = reach_index(ps->thread.pts[k]);
            if (ri >= 0 && dist_[ri] > 3) bi = ri;
        }
        vec3 end = ps->thread.pts[ps->thread.n - 1];
        if (v3_len(v3_sub(end, feet)) < 4.0f && B.vertical_until < B.t) {
            B.vertical_until = B.t + 45.0f;
            logf_("%7.1f THREADEND at the stairs: now %s\n", B.t, ps->target.cy * CHUNK > feet.y ? "up" : "down");
        }
        if (bi >= 0 && B.vertical_until < B.t) {
            B.goal_kind = 0;
            set_path_to(bi);
            int x, y, z;
            cell_of(bi, &x, &y, &z);
            B.goal = v3(x + 0.5f, (float)y, z + 0.5f);
            return;
        }
    }
    /* explore: new places, by style */
    vec3 tgt = feet;
    bool has_tgt = false;
    if (B.style == BS_QUESTER && ps->st.motive && ps->target_chapter == ps->st.chapter + 1) {
        tgt = v3(ps->target.cx * CHUNK + 16.0f, ps->target.cy * CHUNK + 8.0f, ps->target.cz * CHUNK + 16.0f);
        has_tgt = true;
    }
    float here_gate = gate_dist(feet);
    best = -1;
    float bs = -1e9f;
    for (int i = 0; i < BN; i++) {
        if (dist_[i] == 0xFFFF || dist_[i] < 6) continue;
        int x, y, z;
        cell_of(i, &x, &y, &z);
        int v = visit_count(x, y, z);
        float sc = 6.0f / (1.0f + v) - dist_[i] * 0.01f;
        /* the courtyard before the gates holds nothing: a player heads inside */
        if (world_is_exterior(floordiv(x, CHUNK), floordiv(y, CHUNK), floordiv(z, CHUNK))) sc -= 3.0f;
        vec3 c = v3(x + 0.5f, (float)y, z + 0.5f);
        if (B.style == BS_DELVER) sc += 0.08f * (gate_dist(c) - here_gate);
        if (has_tgt) sc += 0.1f * (v3_len(v3_sub(feet, tgt)) - v3_len(v3_sub(c, tgt)));
        if (has_tgt && B.vertical_until > B.t) sc += 0.5f * (tgt.y > feet.y ? c.y - feet.y : feet.y - c.y);
        /* a little noise so equal choices differ */
        sc += (float)((mix64((u64)i * 2654435761u + (u64)(B.t * 10)) & 255) / 2550.0);
        if (sc > bs) { bs = sc; best = i; }
    }
    if (best >= 0) {
        B.goal_kind = 0;
        set_path_to(best);
        int x, y, z;
        cell_of(best, &x, &y, &z);
        B.goal = v3(x + 0.5f, (float)y, z + 0.5f);
    } else {
        B.no_goal++;
    }
}

/* ---------- character management ---------- */

static const int SKILL_ORDER[BS_COUNT][16] = {
    [BS_EXPLORER] = { SK_KEEN_EDGE, SK_TOUGHNESS, SK_CLEAVE, SK_KEEN_EDGE, SK_SECOND_WIND, SK_BLOODTHIRST, SK_FLEET_FOOT, SK_WHIRLWIND, SK_LAST_STAND, -1 },
    [BS_DELVER] = { SK_TOUGHNESS, SK_KEEN_EDGE, SK_CLEAVE, SK_SECOND_WIND, SK_FOCUS, SK_FIREBALL, SK_BLOODTHIRST, SK_LAST_STAND, SK_WHIRLWIND, -1 },
    [BS_ARCHER] = { SK_STEADY_AIM, SK_MULTISHOT, SK_STEADY_AIM, SK_PIERCING, SK_FLEET_FOOT, SK_TOUGHNESS, SK_ARROW_STORM, SK_SECOND_WIND, -1 },
    [BS_CASTER] = { SK_FOCUS, SK_FIREBALL, SK_FOCUS, SK_CHAIN, SK_TOUGHNESS, SK_NOVA, SK_FOCUS, SK_SECOND_WIND, -1 },
    [BS_QUESTER] = { SK_KEEN_EDGE, SK_TOUGHNESS, SK_STEADY_AIM, SK_CLEAVE, SK_SECOND_WIND, SK_FOCUS, SK_LAST_STAND, -1 },
    [BS_CAUTIOUS] = { SK_TOUGHNESS, SK_SECOND_WIND, SK_TOUGHNESS, SK_LAST_STAND, SK_KEEN_EDGE, SK_FLEET_FOOT, SK_CLEAVE, -1 },
};

static void spend_points(PlayerSlot *ps) {
    Stats *s = &ps->st;
    for (int guard = 0; guard < 8 && s->points > 0; guard++) {
        bool bought = false;
        for (int k = 0; k < 16 && SKILL_ORDER[B.style][k] >= 0 && !bought; k++) {
            int id = SKILL_ORDER[B.style][k];
            if (skill_can_buy(s->ranks, id, s->points)) {
                float frac = s->hp / s->maxhp;
                s->ranks[id]++;
                s->points--;
                stats_refresh(ps);
                s->hp = s->maxhp * frac;
                bought = true;
            }
        }
        if (!bought) {
            for (int id = 0; id < SKILL_COUNT && !bought; id++)
                if (skill_can_buy(s->ranks, id, s->points)) {
                    s->ranks[id]++;
                    s->points--;
                    stats_refresh(ps);
                    bought = true;
                }
        }
        if (!bought) break;
    }
}

static float weapon_score(const Item *it) {
    if (!it->used) return 0;
    /* damage per second, with a little extra for rarity (powers) */
    float cd = WEAPON_BASES[it->base].cooldown * fmaxf(0.3f, item_speed_mult(it));
    return item_damage_mult(it) / fmaxf(0.1f, cd) * (1.0f + 0.05f * it->rarity);
}

static void manage_gear(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    for (int i = 0; i < BAG_SIZE; i++) {
        Item *it = &s->bag[i];
        if (!it->used) continue;
        int wc = WEAPON_BASES[it->base].wclass;
        if (weapon_score(it) > weapon_score(&s->equipped[wc]) * 1.03f) {
            Item old = s->equipped[wc];
            s->equipped[wc] = *it;
            *it = old;
            logf_("%7.1f EQUIP class %d rarity %d score %.2f\n", B.t, wc, s->equipped[wc].rarity, weapon_score(&s->equipped[wc]));
        }
    }
    shop_buy_kind(g, ps, SH_SCRAP);
    /* scrap the weakest when the bag is nearly full */
    int used = 0;
    for (int i = 0; i < BAG_SIZE; i++) used += s->bag[i].used;
    while (used > BAG_SIZE - 3) {
        int worst = -1;
        float ws = 1e9f;
        for (int i = 0; i < BAG_SIZE; i++)
            if (s->bag[i].used && weapon_score(&s->bag[i]) < ws) { ws = weapon_score(&s->bag[i]); worst = i; }
        if (worst < 0) break;
        ps->inv_sel = 3 + worst;
        inventory_nav(g, ps, NAV_DROP);
        if (s->bag[worst].used) break; /* scrapping needs something else (a confirm): stop */
        used--;
    }
    int want = B.style == BS_ARCHER ? WPN_BOW : B.style == BS_CASTER ? WPN_STAFF : WPN_SWORD;
    s->weapon = want;
}

/* the best attack spell known (rank, then form strength) */
static int best_attack_spell(const Stats *s) {
    int best = -1, bs = -1;
    for (int id = 0; id < SPELL_ATTACK_COUNT; id++) {
        if (!s->spell_rank[id]) continue;
        int f = spell_form(id);
        if (f == SF_BREATH || f == SF_RUNE || f == SF_AURA) continue; /* needs holding or placing */
        int sc = s->spell_rank[id] * 10 + f;
        if (sc > bs) { bs = sc; best = id; }
    }
    return best;
}

static bool cast_util(Game *g, PlayerSlot *ps, int u) {
    int id = SPELL_ATTACK_COUNT + u;
    if (!ps->st.spell_rank[id] || ps->st.spell_cd > 0) return false;
    float mana = ps->st.mana;
    bool ok = spell_cast_id(g, ps, id);
    if (ok) {
        B.utility_casts[u]++;
        g_tele.casts_utility++;
        logf_("%7.1f CAST %s (mana %.0f) \"%s\"\n", B.t, spell_name(id), mana, ps->banner_time > 0 ? ps->banner : "");
    }
    return ok;
}

/* ---------- acting ---------- */

static void face(PlayerSlot *ps, vec3 at, float rate, float dt) {
    vec3 eye = player_eye(&ps->pl);
    vec3 d = v3_sub(at, eye);
    float yaw = atan2f(d.z, d.x);
    float pitch = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
    float dy = yaw - ps->pl.yaw;
    while (dy > PI_F) dy -= 2 * PI_F;
    while (dy < -PI_F) dy += 2 * PI_F;
    float step = rate * dt;
    ps->pl.yaw += fmaxf(-step, fminf(step, dy));
    ps->pl.pitch += fmaxf(-step, fminf(step, pitch - ps->pl.pitch));
}

static bool facing(const PlayerSlot *ps, vec3 at, float tol) {
    vec3 d = v3_sub(at, player_eye(&ps->pl));
    float yaw = atan2f(d.z, d.x), dy = yaw - ps->pl.yaw;
    while (dy > PI_F) dy -= 2 * PI_F;
    while (dy < -PI_F) dy += 2 * PI_F;
    return fabsf(dy) < tol;
}

static void follow_path(PlayerSlot *ps, PlayerInput *in, float dt, bool sprint) {
    if (B.pidx >= B.npath) return;
    vec3 f = ps->pl.pos;
    /* advance past reached nodes */
    while (B.pidx < B.npath) {
        int x, y, z;
        cell_of(B.path[B.pidx], &x, &y, &z);
        float dx = x + 0.5f - f.x, dz = z + 0.5f - f.z;
        if (dx * dx + dz * dz < 0.35f * 0.35f && fabsf(y - f.y) < 1.2f) B.pidx++;
        else break;
    }
    if (B.pidx >= B.npath) return;
    /* look a few nodes ahead while the way there is straight and level */
    int k = B.pidx, x, y, z;
    cell_of(B.path[k], &x, &y, &z);
    for (int j = k + 1; j < B.npath && j < k + 5; j++) {
        int x2, y2, z2;
        cell_of(B.path[j], &x2, &y2, &z2);
        if (y2 != y) break;
        vec3 a = v3(f.x, f.y + 0.5f, f.z), b = v3(x2 + 0.5f, y2 + 0.5f, z2 + 0.5f);
        if (!phys_line_of_sight(a, b, game_solid, G)) break;
        k = j;
    }
    cell_of(B.path[k], &x, &y, &z);
    vec3 to = v3(x + 0.5f, (float)y + 1.5f, z + 0.5f);
    face(ps, to, 8.0f, dt);
    ps->pl.pitch *= 0.9f;
    in->move_z = facing(ps, to, 0.8f) ? 1.0f : 0.3f;
    in->sprint = sprint;
    int nx, ny, nz;
    cell_of(B.path[B.pidx], &nx, &ny, &nz);
    if (ny > (int)floorf(f.y + 0.05f) && ps->pl.on_ground) in->jump = true;
}

static Monster *pick_enemy(PlayerSlot *ps) {
    vec3 eye = player_eye(&ps->pl);
    Monster *best = NULL;
    float bd = 1e9f;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &G->mons[i];
        if (!m->alive || m->hidden || m->charm_t > 0 || m->sulk_t > 0) continue; /* (a sulking one leaves you be) */
        bool skip = false;
        for (int k = 0; k < 8; k++) skip |= B.ignore[k] == m && B.ignore_until[k] > B.t;
        if (skip) continue;
        vec3 c = mon_centre(m);
        float d = v3_len(v3_sub(c, eye));
        float range = m->awake ? 18.0f : 7.0f;
        if (d > range) continue;
        if (d > 3.0f && !phys_line_of_sight(eye, c, game_solid, G)) continue;
        if (d < bd) { bd = d; best = m; }
    }
    return best;
}

static void fight(Game *g, PlayerSlot *ps, Monster *m, PlayerInput *in, float dt) {
    Stats *s = &ps->st;
    vec3 c = mon_centre(m), eye = player_eye(&ps->pl);
    float d = v3_len(v3_sub(c, eye));
    float hd = sqrtf((c.x - eye.x) * (c.x - eye.x) + (c.z - eye.z) * (c.z - eye.z));
    face(ps, c, 10.0f, dt);
    bool ranged = s->weapon != WPN_SWORD;
    float reach = MONSTERS[m->type].hw * m->scale + 1.9f;
    if (!ranged) {
        if (hd > reach - 0.4f) in->move_z = 1.0f;
        if (m->state == MS_WINDUP && B.style == BS_CAUTIOUS && hd < reach + 0.5f) in->move_z = -1.0f; /* step back from a raised blow */
        ps->pad_attack = hd < reach + 0.3f && facing(ps, c, 0.35f);
        if (m->type == MON_KNIGHT && hd < reach + 2.0f) {
            /* its shield faces you: step round it */
            vec3 fw = v3(cosf(m->yaw), 0, sinf(m->yaw)), to = v3_norm(v3(ps->pl.pos.x - m->pos.x, 0, ps->pl.pos.z - m->pos.z));
            if (v3_dot(fw, to) > 0.2f) in->move_x = 1.0f;
        }
    } else {
        if (d < 5.0f) in->move_z = -1.0f;
        else if (d > 15.0f) in->move_z = 1.0f;
        in->move_x = sinf(B.t * 0.7f) > 0 ? 0.6f : -0.6f;
        ps->pad_attack = facing(ps, c, 0.12f);
    }
    /* spells: attack spells for casters always, for others when mana is plentiful */
    int sp = best_attack_spell(s);
    bool caster = B.style == BS_CASTER;
    if (sp >= 0 && s->spell_cd <= 0 && facing(ps, c, 0.15f) && d < 18.0f && s->mana > s->maxmana * (caster ? 0.15f : 0.6f)) {
        s->spell = sp;
        if (spell_cast_id(g, ps, sp)) g_tele.casts_attack++;
    }
    /* panic buttons */
    int near = 0;
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (g->mons[i].alive && g->mons[i].awake && v3_len(v3_sub(g->mons[i].pos, ps->pl.pos)) < 8.0f) near++;
    if (near >= 3 && s->hp < s->maxhp * 0.6f) {
        if (B.style == BS_CAUTIOUS) {
            if (!cast_util(g, ps, SU_SANCTUARY) && !cast_util(g, ps, SU_FEAR)) cast_util(g, ps, SU_MIRROR);
        } else {
            if (!cast_util(g, ps, SU_FEAR)) cast_util(g, ps, SU_WARD);
        }
    }
    if (m->grade == GRADE_BOSS && s->ward <= 0) cast_util(g, ps, SU_WARD);
}

static void survive(Game *g, PlayerSlot *ps, bool in_fight) {
    Stats *s = &ps->st;
    float heal_at = B.style == BS_CAUTIOUS ? 0.55f : 0.4f;
    if (s->hp < s->maxhp * heal_at && !cast_util(g, ps, SU_HEAL) && s->hp < s->maxhp * (heal_at - 0.1f) && s->potions_hp > 0 &&
        ps->potion_cd <= 0)
        drink_potion(g, ps, false);
    if (s->mana < s->maxmana * 0.15f && s->potions_mp > 0 && ps->potion_cd <= 0 && in_fight && B.style == BS_CASTER) drink_potion(g, ps, true);
    /* a trip home when worn out and nobody is close */
    bool worn = s->hp < s->maxhp * 0.5f && s->potions_hp == 0;
    bool rich = s->gold >= 2000 && B.t >= B.next_home; /* ranks, relics and building are sold at home */
    if (!in_fight && !realm_has(ps->pl.pos) && (worn || rich) && s->spell_cd <= 0 && B.realm_phase == 0) {
        if (cast_util(g, ps, SU_SANCTUM)) {
            B.realm_phase = 1;
            B.realm_trips++;
            B.shopped_realm = false;
            B.realm_rested = 0;
            B.next_home = B.t + 600.0f;
            logf_("%7.1f REALM trip (%s, hp %.0f/%.0f, gold %d)\n", B.t, worn ? "worn" : "gold", s->hp, s->maxhp, s->gold);
        }
    }
}

/* ---------- food: hunt what crosses the path, cook at fires, eat before going on ---------- */

/* The best dish the larder makes here: the most ingredients, a known recipe or a new one. */
static bool best_pot(PlayerSlot *ps, int place, u8 *out, int *nout) {
    const Stats *s = &ps->st;
    int have[FD_COUNT], nh = 0;
    for (int f = 0; f < FD_COUNT; f++) if (s->food[f]) have[nh++] = f;
    int best = -1;
    u8 pot[4];
    for (int n = MINI(4, place + 1); n >= 2 && best < 0; n--) {
        int ix[4] = { 0, 0, 0, 0 };
        /* combinations with repetition, limited by what is held */
        for (;;) {
            bool ok = true;
            for (int k = 0; k < n && ok; k++) {
                int cnt = 0;
                for (int j = 0; j < n; j++) cnt += ix[j] == ix[k];
                ok = s->food[have[ix[k]]] >= cnt;
            }
            if (ok && nh) {
                for (int k = 0; k < n; k++) pot[k] = (u8)have[ix[k]];
                int r = food_match(pot, n);
                if (r != RC_SIMPLE) {
                    memcpy(out, pot, (size_t)n);
                    *nout = n;
                    best = r;
                    break;
                }
            }
            int k = n - 1;
            while (k >= 0 && ix[k] >= nh - 1) k--;
            if (k < 0) break;
            ix[k]++;
            for (int j = k + 1; j < n; j++) ix[j] = ix[k];
        }
    }
    return best >= 0;
}

static bool bot_food(Game *g, PlayerSlot *ps, PlayerInput *in, float dt) {
    Stats *s = &ps->st;
    /* eat when there is no meal */
    if (!(s->meal && s->meal_t > 0) && s->well_fed_t <= 0 && food_dishes_held(s) && ps->potion_cd <= 0) {
        if (food_eat_best(g, ps)) logf_("%7.1f EAT %s (q%d, %.0f s)\n", B.t, RECIPES[s->meal - 1].name, s->meal_q, s->meal_t);
    }
    /* cook at a fire */
    int place = food_cook_place(g, ps);
    if (place != CP_NONE && B.t >= B.next_cook && food_held(s) >= 2 && food_dishes_held(s) < FOOD_POUCH) {
        B.next_cook = B.t + 20.0f;
        for (int k = 0; k < 3; k++) {
            u8 pot[4];
            int n;
            if (!best_pot(ps, place, pot, &n)) break;
            ps->npot = 0;
            for (int i = 0; i < n; i++) food_pot_add(g, ps, pot[i]);
            int r = food_match(ps->pot, ps->npot);
            if (!food_cook(g, ps)) break;
            logf_("%7.1f COOK %s at place %d (%d ingredients)\n", B.t, RECIPES[r].name, place, n);
        }
    }
    /* hunt an animal close by, for a few seconds at most */
    if (realm_has(ps->pl.pos)) return false;
    if (B.prey && (!B.prey->alive || B.prey->key != B.prey_key)) {
        logf_("%7.1f HUNT done\n", B.t);
        B.prey = NULL;
    }
    if (B.prey && B.t > B.hunt_until) { /* out of reach: leave it alone for two minutes */
        int k = B.nskip++ % 8;
        B.skip_prey[k] = B.prey_key;
        B.skip_until[k] = B.t + 120.0f;
        logf_("%7.1f HUNT gave up\n", B.t);
        B.prey = NULL;
    }
    if (!B.prey && B.t >= B.next_hunt && (s->weapon != WPN_STAFF || s->mana > s->maxmana * 0.6f)) {
        B.next_hunt = B.t + 1.0f;
        Critter *c = wildlife_nearest(g, ps->pl.pos, 9.0f, true);
        bool skip = false;
        for (int k = 0; k < 8 && c; k++) skip |= B.skip_prey[k] == c->key && B.t < B.skip_until[k];
        bool ranged = s->weapon != WPN_SWORD;
        if (c && !skip && c->kind != CR_HIVE && (ranged || fabsf(c->pos.y - ps->pl.pos.y) < 1.2f) &&
            phys_line_of_sight(player_eye(&ps->pl), v3_add(c->pos, v3(0, 0.2f, 0)), game_solid, g)) {
            B.prey = c;
            B.prey_key = c->key;
            B.hunt_until = B.t + 6.0f;
            B.next_hunt = B.t + 4.0f;
        }
    }
    if (!B.prey) return false;
    vec3 c = v3_add(B.prey->pos, v3(0, 0.15f, 0));
    face(ps, c, 10.0f, dt);
    float hd = sqrtf((c.x - ps->pl.pos.x) * (c.x - ps->pl.pos.x) + (c.z - ps->pl.pos.z) * (c.z - ps->pl.pos.z));
    bool ranged = s->weapon != WPN_SWORD;
    if (!ranged && hd > 1.4f) in->move_z = 1.0f;
    ps->pad_attack = facing(ps, c, ranged ? 0.1f : 0.4f) && (ranged || hd < 2.2f);
    return true;
}

/* ---------- alchemy: brew what it knows, drink before a hard fight ---------- */

static bool can_brew(const Stats *s, int r) {
    int need_f[FD_COUNT] = { 0 }, need_r[RG_COUNT] = { 0 };
    for (int k = 0; k < FORMULA_STEPS; k++) {
        u8 st = ELIXIRS[r].steps[k];
        if (step_is_food(st)) need_f[st]++;
        else if (step_is_rg(st)) need_r[st - 64]++;
    }
    for (int f = 0; f < FD_COUNT; f++) if (s->food[f] < need_f[f]) return false;
    for (int k = 0; k < RG_COUNT; k++) if (s->reag[k] < need_r[k]) return false;
    return true;
}

static void bot_alchemy(Game *g, PlayerSlot *ps, bool in_fight, const Monster *m) {
    Stats *s = &ps->st;
    if (!in_fight && B.t >= B.next_brew && alchemy_station(g, ps)) {
        B.next_brew = B.t + 30.0f;
        for (int r = 0; r < ELIXIR_COUNT; r++) {
            if (!(s->formulae & (1u << r)) || s->elixirs[r] >= 2 || !can_brew(s, r)) continue;
            if (r == 0 ? healing_held(s) >= POTION_HP_MAX : alchemy_belt(s) >= ELIXIR_BELT) continue;
            ps->ncauldron = 0;
            for (int k = 0; k < alchemy_steps(ELIXIRS[r].steps); k++) alchemy_add(g, ps, ELIXIRS[r].steps[k]);
            if (alchemy_bottle(g, ps)) logf_("%7.1f BREW %s (%d)\n", B.t, ELIXIRS[r].name, s->elixirs[r]);
        }
    }
    if (!in_fight || !m || s->elixir) return;
    int awake = 0;
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (g->mons[i].alive && g->mons[i].awake && v3_len(v3_sub(g->mons[i].pos, ps->pl.pos)) < 12.0f) awake++;
    if (m->grade < GRADE_ELITE && awake < 4) return;
    /* the strongest it has for this: damage or staying alive */
    static const int PREF[] = { 7, 3, 2, 16, 25, 4, 12, 0, 10, 24, 9, 1 };
    for (int k = 0; k < (int)(sizeof PREF / sizeof PREF[0]); k++)
        if (s->elixirs[PREF[k]] && alchemy_drink(g, ps, PREF[k])) {
            logf_("%7.1f DRINK %s (vs %s grade %d, %d awake)\n", B.t, ELIXIRS[PREF[k]].name, MONSTERS[m->type].name, m->grade, awake);
            return;
        }
}

/* ---------- the loop ---------- */

void bot_init(Game *g, const char *style, const char *log_path, float minutes) {
    memset(&B, 0, sizeof B);
    G = g;
    B.style = BS_EXPLORER;
    for (int i = 0; i < BS_COUNT; i++)
        if (style && !strcmp(style, STYLE_NAMES[i])) B.style = i;
    B.minutes = minutes > 0 ? minutes : 20.0f;
    B.log = log_path ? fopen(log_path, "w") : stdout;
    B.last_chapter = -1;
    logf_("BOT style=%s minutes=%.0f seed=%llu\n", STYLE_NAMES[B.style], B.minutes, (unsigned long long)g->opt.seed);
}

static void status(PlayerSlot *ps) {
    Stats *s = &ps->st;
    int awake = 0;
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (G->mons[i].alive && G->mons[i].awake && v3_len(v3_sub(G->mons[i].pos, ps->pl.pos)) < 25.0f) awake++;
    int alive = 0;
    for (int i = 0; i < MAX_MONSTERS; i++) alive += G->mons[i].alive;
    int bag = 0;
    for (int i = 0; i < BAG_SIZE; i++) bag += s->bag[i].used;
    int known = 0;
    for (int i = 0; i < SPELL_COUNT; i++) known += s->spell_rank[i] > 0;
    vec3 p = ps->pl.pos;
    logf_("%7.1f STATUS pos %.0f %.0f %.0f gate %.0f tier %d lvl %d hp %.0f/%.0f mana %.0f/%.0f kills %d deaths %d gold %d pot %d/%d "
          "bag %d wpn %.2f/%.2f/%.2f spells %d chap %d miss %d lore %d awake %d alive %d cells %d stuck %d fight %.0f%%\n",
          B.t, p.x, p.y, p.z, gate_dist(p), tier_at(p), s->level, s->hp, s->maxhp, s->mana, s->maxmana, s->kills, B.deaths, s->gold,
          s->potions_hp, s->potions_mp, bag, weapon_score(&s->equipped[0]), weapon_score(&s->equipped[1]), weapon_score(&s->equipped[2]),
          known, s->chapter + 1, s->nmissions, G->nlore, awake, alive, nvisited, B.stuck_total, 100.0f * B.time_in_fight / fmaxf(1.0f, B.t));
}

static const char *SRC_NAMES[HS_COUNT] = { "melee", "projectile", "blast", "trap", "lava", "fall", "burn/poison", "other" };

static void summary(PlayerSlot *ps) {
    Stats *s = &ps->st;
    logf_("SUMMARY style=%s minutes=%.1f level=%d kills=%d deaths=%d gold=%d max_gate_dist=%.0f max_tier=%d max_depth=%.0f max_height=%.0f "
          "cells=%d stuck=%d hard_stuck=%d realm_trips=%d realm_failed=%d keystones=%d builds=%d chapter=%d missions=%d lore=%d fights=%d avg_fight=%.1f fight_share=%.0f%% "
          "replans=%d no_goal=%d stalemates=%d close_calls=%d food=%d hunted=%d harvested=%d cooked=%d eaten=%d recipes=%d reagents=%d brewed=%d elixirs=%d formulae=%d\n",
          STYLE_NAMES[B.style], B.t / 60.0f, s->level, s->kills, B.deaths, s->gold, B.max_dist, B.max_tier, B.max_depth, B.max_height,
          nvisited, B.stuck_total, B.hard_stuck, B.realm_trips, B.realm_failed, G->sanctum.keystones, B.builds, s->chapter + 1, s->nmissions, G->nlore, B.fights,
          B.fights ? B.fight_time_total / B.fights : 0.0f, 100.0f * B.time_in_fight / fmaxf(1.0f, B.t), B.replans, B.no_goal, B.stalemates, B.close_calls,
          g_tele.food, g_tele.hunted, g_tele.harvested, g_tele.cooked, g_tele.eaten, __builtin_popcount(s->recipes),
          g_tele.reagents, g_tele.brewed, g_tele.elixirs, __builtin_popcount(s->formulae));
    float total = 0;
    for (int i = 0; i < HS_COUNT; i++) total += g_tele.taken[i];
    logf_("DAMAGE_TAKEN total=%.0f", total);
    for (int i = 0; i < HS_COUNT; i++) logf_(" %s=%.0f", SRC_NAMES[i], g_tele.taken[i]);
    logf_("\nDAMAGE_BY_MONSTER");
    for (int i = 0; i < MON_TYPE_COUNT; i++)
        if (g_tele.taken_mon[i] > 0) logf_(" %s=%.0f", MONSTERS[i].name, g_tele.taken_mon[i]);
    logf_("\nKILLS");
    for (int i = 0; i < MON_TYPE_COUNT; i++)
        if (g_tele.kills[i][0] + g_tele.kills[i][1] + g_tele.kills[i][2])
            logf_(" %s=%d/%d/%d", MONSTERS[i].name, g_tele.kills[i][0], g_tele.kills[i][1], g_tele.kills[i][2]);
    logf_("\nLOOT dealt=%.0f gold=%d chests=%d vaults=%d scrolls=%d relics=%d weapons=%d/%d/%d/%d/%d potions_used=%d/%d casts=%d/%d\n",
          g_tele.dealt, g_tele.gold, g_tele.chests, g_tele.vaults, g_tele.scrolls, g_tele.relics, g_tele.weapons[0], g_tele.weapons[1],
          g_tele.weapons[2], g_tele.weapons[3], g_tele.weapons[4], g_tele.potions_hp, g_tele.potions_mp, g_tele.casts_attack,
          g_tele.casts_utility);
    logf_("ARMS lost_found=%d uniques_found=%d powers_fired=%d awakened=%d banes=%d\n", g_tele.lost_found, g_tele.uniques_found,
          g_tele.powers_fired, g_tele.awakened, g_tele.banes);
    for (int w = 0; w < WPN_COUNT; w++) {
        char name[128], pw[96] = "";
        item_name(&s->equipped[w], name, sizeof name);
        for (int k = 0; k < 2; k++)
            if (s->equipped[w].power[k]) {
                char t[64];
                arms_power_text(s->equipped[w].power[k], t, sizeof t);
                snprintf(pw + strlen(pw), sizeof pw - strlen(pw), " [%s]", t);
            }
        logf_("WEAPON %d: %s (rarity %d, level %d, kills %d)%s\n", w, name, s->equipped[w].rarity, s->equipped[w].level, s->equipped[w].kills, pw);
    }
    logf_("SKILLS");
    for (int i = 0; i < SKILL_COUNT; i++) if (s->ranks[i]) logf_(" %s=%d", SKILLS[i].name, s->ranks[i]);
    logf_(" unspent=%d\nSPELLS", s->points);
    for (int i = 0; i < SPELL_COUNT; i++) if (s->spell_rank[i]) logf_(" [%s %d]", spell_name(i), s->spell_rank[i]);
    logf_("\n");
}

bool bot_step(Game *g, PlayerInput *in, float dt) {
    G = g;
    PlayerSlot *ps = &g->players[0];
    Stats *s = &ps->st;
    memset(in, 0, sizeof *in);
    ps->pad_attack = ps->pad_alt = ps->pad_spell = false;
    ps->attack_held = ps->alt_held = ps->spell_held = false;
    B.t += dt;
    if (B.t > B.minutes * 60.0f) {
        status(ps);
        summary(ps);
        if (B.log && B.log != stdout) fclose(B.log);
        B.log = NULL;
        return false;
    }
    if (story_motive_pending(g, ps)) story_pick_motive(g, ps, (B.style + (int)(g->opt.seed % 4)) % MOTIVE_COUNT);
    ps->show_map = ps->show_skills = false;
    vec3 p = ps->pl.pos;
    if (!realm_has(p)) {
        B.max_dist = fmaxf(B.max_dist, gate_dist(p));
        B.max_depth = fmaxf(B.max_depth, G->start_pos.y - p.y);
        B.max_height = fmaxf(B.max_height, p.y - G->start_pos.y);
        int t = tier_at(p);
        if (t > B.max_tier) { B.max_tier = t; logf_("%7.1f TIER %d reached (gate %.0f m, level %d)\n", B.t, t, gate_dist(p), s->level); }
    }
    /* events */
    if (s->dead && !B.was_dead) {
        B.deaths++;
        const char *src = SRC_NAMES[g_tele.last_src % HS_COUNT];
        const char *who = g_tele.last_src <= HS_BLAST && g_tele.last_mon < MON_TYPE_COUNT ? MONSTERS[g_tele.last_mon].name : "-";
        int awake = 0;
        for (int i = 0; i < MAX_MONSTERS; i++)
            if (g->mons[i].alive && g->mons[i].awake && v3_len(v3_sub(g->mons[i].pos, p)) < 12.0f) awake++;
        logf_("%7.1f DEATH by %s (%s grade %d rank %d) level %d tier %d gate %.0f awake_near %d potions %d\n", B.t, src, who, g_tele.last_grade,
              g_tele.last_rank, s->level, tier_at(p), gate_dist(p), awake, s->potions_hp);
        B.npath = 0;
        B.realm_phase = 0;
        if (g_tele.last_grade == GRADE_BOSS) {
            /* killed by a boss: keep away from there for a while */
            B.avoid_until = B.t + 300.0f;
            for (int dz = -24; dz <= 24; dz += 8)
                for (int dy = -4; dy <= 4; dy += 4)
                    for (int dx = -24; dx <= 24; dx += 8) {
                        int *bv = visits((int)floorf(p.x) + dx, (int)floorf(p.y) + dy, (int)floorf(p.z) + dz, true);
                        if (bv) *bv += 300;
                    }
        }
    }
    B.was_dead = s->dead;
    if (g_tele.taken[HS_FALL] > B.last_fall + 0.5f) {
        logf_("%7.1f FALL damage %.0f (hp %.0f/%.0f) at %.0f %.0f %.0f\n", B.t, g_tele.taken[HS_FALL] - B.last_fall, s->hp, s->maxhp, p.x, p.y, p.z);
        B.last_fall = g_tele.taken[HS_FALL];
    }
    if (!s->dead && s->hp < s->maxhp * 0.15f && !B.low) { B.low = true; B.low_hp_since = B.t; }
    if (B.low && (s->dead || s->hp > s->maxhp * 0.5f)) {
        if (!s->dead) { B.close_calls++; logf_("%7.1f CLOSECALL survived (potions %d)\n", B.t, s->potions_hp); }
        B.low = false;
    }
    if (s->level != B.last_level) {
        if (B.last_level) logf_("%7.1f LEVEL %d (tier %d)\n", B.t, s->level, tier_at(p));
        B.last_level = s->level;
    }
    if (s->chapter != B.last_chapter) {
        if (B.last_chapter >= 0) logf_("%7.1f CHAPTER %d\n", B.t, s->chapter + 1);
        B.last_chapter = s->chapter;
    }
    if (B.t >= B.next_status) { status(ps); B.next_status = B.t + 15.0f; }
    if (s->dead) return true;
    /* back where it fell? the delver and quester pay for it, the others take the shrine */
    if (s->fell_t > 0 && s->fell_t < 14.0f && (B.style == BS_DELVER || B.style == BS_QUESTER) && B.avoid_until < B.t) {
        int gold = s->gold;
        game_thread_key(g, ps);
        logf_("%7.1f RETURN where it fell (-%d gold)\n", B.t, gold - s->gold);
        B.npath = 0;
    }
    /* at a shrine: shop */
    if (B.t >= B.next_shop && shop_open_here(g, ps)) {
        B.next_shop = B.t + 30.0f;
        int bought = 0;
        if (realm_has(ps->pl.pos)) {
            /* build up the Sanctum: the comforts first, then wider, then the rest */
            static const int ORDER[SP_COUNT] = { SP_DINING, SP_BATH, SP_ISLAND, SP_LIBRARY, SP_ARMORY, SP_GARDEN, SP_HALL, SP_COURT };
            for (int k = 0; k < SP_COUNT; k++)
                if (s->gold > 400 && shop_buy_arg(g, ps, SH_BUILD, ORDER[k])) {
                    bought++;
                    B.builds++;
                    logf_("%7.1f BUILD plot %d stage %d (gold left %d, keystones %d)\n", B.t, ORDER[k], sanctum_stage(g, ORDER[k]), s->gold,
                          g->sanctum.keystones);
                }
            if (shop_buy_kind(g, ps, SH_SCROLL)) bought++;
            if (s->gold > 2600 && shop_buy_kind(g, ps, SH_KEYSTONE)) { bought++; logf_("%7.1f BUY keystone\n", B.t); }
            while (s->gold > 1500 && shop_buy_kind(g, ps, SH_MERCHANT)) bought++;
            /* the forge for every weapon it uses, then beauty with what is left */
            for (int w = 0; w < WPN_COUNT; w++) {
                int keep = s->weapon;
                s->weapon = w;
                if (s->gold > 150 * s->level + 600 && shop_buy_kind(g, ps, SH_REFORGE)) { bought++; logf_("%7.1f FORGE weapon %d to %d\n", B.t, w, s->level); }
                if (s->gold > 3000 && shop_buy_kind(g, ps, SH_TEMPER)) { bought++; logf_("%7.1f TEMPER weapon %d\n", B.t, w); }
                s->weapon = keep;
            }
            if (s->gold > 1200 && shop_buy_kind(g, ps, SH_PAGE)) bought++;
            if (s->gold > 1000 && shop_buy_kind(g, ps, SH_RECIPE)) bought++;
            while (s->gold > 5000 && shop_buy_kind(g, ps, SH_DECOR)) { bought++; logf_("%7.1f DECOR\n", B.t); }
            int used = 0;
            for (int i = 0; i < BAG_SIZE; i++) used += s->bag[i].used;
            if (used > BAG_SIZE - 4 && shop_buy_kind(g, ps, SH_STORE)) bought++;
            B.shopped_realm = true;
        }
        while (healing_held(s) < POTION_HP_MAX && shop_buy_kind(g, ps, SH_POTION_HP)) bought++;
        if (s->gold > 2500 && shop_buy_kind(g, ps, SH_SEALED)) { bought++; logf_("%7.1f SEALED weapon bought\n", B.t); }
        int sp = best_attack_spell(s);
        if (sp >= 0) {
            s->spell = sp;
            while (s->gold > 300 && shop_buy_kind(g, ps, SH_RANK)) bought++;
        }
        if (s->gold > 1500 && shop_buy_kind(g, ps, SH_RELIC)) bought++;
        if (bought) logf_("%7.1f SHOP %d bought, gold left %d\n", B.t, bought, s->gold);
    }
    if (B.realm_phase && realm_has(ps->pl.pos) && s->respawn.y > POCKET_Y0 && B.realm_rested < 1) B.realm_rested = 1;
    if (B.t >= B.next_skills) {
        spend_points(ps);
        manage_gear(g, ps);
        B.next_skills = B.t + 2.0f;
    }
    int *v = visits((int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z), true);
    if (v && fmodf(B.t, 1.0f) < dt) (*v)++;

    /* the Sanctum door: step through it */
    if (B.realm_phase == 1) {
        for (int i = 0; i < MAX_EFFECTS; i++) {
            Effect *e = &g->effects[i];
            if (e->alive && e->type == EF_DOOR && e->owner == ps->index) {
                face(ps, v3_add(e->pos, v3(0, 1.5f, 0)), 10.0f, dt);
                in->move_z = 1.0f;
                if (realm_has(ps->pl.pos)) break;
                return true;
            }
        }
        if (ps->ritual_t > 0 && !realm_has(ps->pl.pos)) return true; /* the ritual: stand still */
        B.realm_phase = realm_has(ps->pl.pos) ? 2 : 0;
        if (!B.realm_phase) {
            B.realm_failed++;
            B.next_home = B.t + 30.0f; /* try again soon */
            logf_("%7.1f REALM trip failed (the ritual broke or the door closed)\n", B.t);
        }
        B.npath = 0;
    }
    if (B.realm_phase == 2 && !realm_has(ps->pl.pos)) {
        logf_("%7.1f REALM back (hp %.0f/%.0f fed %.0f refreshed %.0f)\n", B.t, s->hp, s->maxhp, s->well_fed_t, s->refreshed_t);
        B.realm_phase = 0;
        B.npath = 0;
    }

    Monster *m = realm_has(p) ? NULL : pick_enemy(ps);
    if (m != B.target) {
        if (B.target && !B.target->alive) {
            B.fights++;
            B.fight_time_total += B.t - B.target_since;
        }
        B.target = m;
        B.target_since = B.t;
        B.target_hp0 = m ? m->hp : 0;
    }
    /* a fight going nowhere (out of reach, or it cannot be hurt): leave it for a while */
    if (m && B.t - B.target_since > 20.0f && m->hp >= B.target_hp0 * 0.95f) {
        B.stalemates++;
        logf_("%7.1f STALEMATE with %s (grade %d, state %d, flies %d, dist %.1f, dy %.1f, hp %.0f/%.0f, los %d)\n", B.t,
              MONSTERS[m->type].name, m->grade, m->state, MONSTERS[m->type].flies, v3_len(v3_sub(m->pos, ps->pl.pos)), m->pos.y - ps->pl.pos.y,
              m->hp, m->maxhp, phys_line_of_sight(player_eye(&ps->pl), mon_centre(m), game_solid, g));
        int slot = B.stalemates % 8;
        B.ignore[slot] = m;
        B.ignore_until[slot] = B.t + 40.0f;
        B.target = NULL;
        m = NULL;
    }
    survive(g, ps, m != NULL);
    bot_alchemy(g, ps, m != NULL, m);
    if (m) {
        B.time_in_fight += dt;
        vec3 c = mon_centre(m);
        float d = v3_len(v3_sub(c, player_eye(&ps->pl)));
        bool los = phys_line_of_sight(player_eye(&ps->pl), c, game_solid, g);
        if (!los || (d > 6.0f && s->weapon == WPN_SWORD && !m->awake)) {
            /* go round to it */
            if (B.t >= B.next_plan || B.goal_kind != 2) {
                bfs(ps->pl.pos);
                int ri = reach_index(m->pos);
                if (ri >= 0 && set_path_to(ri)) B.goal_kind = 2;
                B.next_plan = B.t + 1.0f;
            }
            follow_path(ps, in, dt, false);
        }
        fight(g, ps, m, in, dt);
        if (B.goal_kind == 2 && los) B.npath = 0;
        return true;
    }
    if (B.goal_kind == 2) B.npath = 0;
    if (bot_food(g, ps, in, dt)) return true;

    /* stuck? */
    if (B.t >= B.stuck_check) {
        float moved = v3_len(v3_sub(p, B.last_pos));
        if (moved < 1.0f && B.npath > 0) {
            B.stuck_windows++;
            B.stuck_total++;
            int *gv = visits((int)floorf(B.goal.x), (int)floorf(B.goal.y), (int)floorf(B.goal.z), true);
            if (gv) *gv += 50; /* that goal is not reachable this way */
            B.npath = 0;
            if (B.stuck_windows == 6) {
                B.hard_stuck++;
                /* the whole area round that goal is out of reach this way: forget it */
                for (int dz = -16; dz <= 16; dz += 8)
                    for (int dy = -4; dy <= 4; dy += 4)
                        for (int dx = -16; dx <= 16; dx += 8) {
                            int *bv = visits((int)floorf(B.goal.x) + dx, (int)floorf(B.goal.y) + dy, (int)floorf(B.goal.z) + dz, true);
                            if (bv) *bv += 200;
                        }
                logf_("%7.1f HARDSTUCK at %.1f %.1f %.1f (goal %.1f %.1f %.1f) - recall\n", B.t, p.x, p.y, p.z, B.goal.x, B.goal.y, B.goal.z);
                if (!cast_util(g, ps, SU_RECALL)) { ps->pl.pos = s->respawn; ps->pl.vel = v3(0, 0, 0); }
                B.stuck_windows = 0;
            }
        } else if (moved >= 1.0f) {
            B.stuck_windows = 0;
        }
        B.last_pos = p;
        B.stuck_check = B.t + 4.0f;
    }
    if (B.npath == 0 || B.pidx >= B.npath || B.t >= B.next_plan) {
        plan(ps);
        B.next_plan = B.t + (B.goal_kind == 1 ? 2.0f : 4.0f);
    }
    follow_path(ps, in, dt, B.goal_kind == 0 && s->hp > s->maxhp * 0.5f);
    /* the golden thread now and then for the quester */
    if (B.style == BS_QUESTER && (ps->thread.n == 0 || ps->thread.t >= ps->thread.life) && fmodf(B.t, 5.0f) < dt) cast_util(g, ps, SU_THREAD);
    return true;
}
