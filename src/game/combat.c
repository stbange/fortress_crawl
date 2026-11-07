#include "game/game.h"

#include "render/batch.h"

/* Retro (Hexen-like) combat: instant swings and projectiles, no combos.
 * Three weapons, each with an alternate attack unlocked by the last tier of its branch. */

static const char *WEAPON_NAMES[WPN_COUNT] = { "SWORD", "BOW", "STAFF" };

static float level_mult(const Stats *s) { return 1.0f + 0.07f * (float)(s->level - 1); }

float stats_max_hp(const Stats *s) { return 100.0f + 12.0f * (s->level - 1) + 25.0f * s->ranks[SK_TOUGHNESS]; }
float stats_max_mana(const Stats *s) { return 40.0f + 6.0f * (s->level - 1); }

void stats_refresh(PlayerSlot *ps) {
    ps->st.maxhp = stats_max_hp(&ps->st) * (1.0f + relic_bonus(&ps->st, RB_HEALTH));
    ps->st.maxmana = stats_max_mana(&ps->st) * (1.0f + relic_bonus(&ps->st, RB_MANA));
    ps->pl.speed_mult = (1.0f + 0.12f * ps->st.ranks[SK_FLEET_FOOT]) * (1.0f + relic_bonus(&ps->st, RB_SPEED));
    if (ps->st.well_fed_t > 0) ps->st.maxhp *= 1.2f; /* a feast in the Sanctum */
    ps->st.hp = fminf(ps->st.hp, ps->st.maxhp);
}

/* Damage factor of the equipped weapon of a class, with relics. */
static float weapon_mult(const Stats *s, int wclass) {
    return item_damage_mult(&s->equipped[wclass]) * (1.0f + relic_bonus(s, RB_DAMAGE));
}

/* A hit with a weapon: its elemental affix adds the element's effect. */
static void weapon_hit(Game *g, PlayerSlot *ps, Monster *m, int wclass, float dmg, vec3 from, float knock) {
    int el = item_affix_element(&ps->st.equipped[wclass]);
    int base = ps->st.equipped[wclass].base;
    sfx_at(g, base == WB_MACE ? SFX_HIT_BLUNT : SFX_HIT_BLADE, v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0)),
           base == WB_GREATSWORD || base == WB_AXE ? 1.2f : 1.0f);
    if (el != EL_NONE) sfx_at(g, sfx_element_impact(el), m->pos, 0.5f);
    weapon_impact_fx(g, el, ps->st.equipped[wclass].rarity, el == EL_SHADOW,
                     v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.55f, 0)), ps->index);
    if (el != EL_NONE) element_hit(g, m, el, dmg, from, knock, ps->index);
    else monster_damage(g, m, dmg, from, knock, ps->index);
}

/* ---------- particles and flashes ---------- */

void particles_burst(Game *g, vec3 p, vec3 color, int n, float speed, float size, bool emissive) {
    Rng r = rng_make((u64)(p.x * 1000) ^ ((u64)(p.z * 1000) << 20) ^ (u64)(g->time * 1e4));
    for (int i = 0; i < n; i++) {
        if (g->nparts >= MAX_PARTICLES) return;
        Particle *q = &g->parts[g->nparts++];
        vec3 d = v3(rng_float(&r) - 0.5f, rng_float(&r) * 0.8f, rng_float(&r) - 0.5f);
        q->pos = p;
        q->vel = v3_scale(v3_norm(d), speed * (0.4f + rng_float(&r)));
        q->color = v3_scale(color, 0.8f + 0.4f * rng_float(&r));
        q->max_life = q->life = 0.5f + rng_float(&r) * 0.8f;
        q->size = size * (0.6f + 0.8f * rng_float(&r));
        q->emissive = emissive;
        q->gravity = !emissive;
    }
}

void flash_add(Game *g, vec3 p, vec3 color, float radius, float life) {
    int slot = 0;
    for (int i = 0; i < MAX_FLASHES; i++) {
        if (g->flashes[i].life <= 0) { slot = i; break; }
        if (g->flashes[i].life < g->flashes[slot].life) slot = i;
    }
    g->flashes[slot] = (Flash){ p, color, radius, life, life };
}

static void update_particles(Game *g, float dt) {
    int n = 0;
    for (int i = 0; i < g->nparts; i++) {
        Particle *q = &g->parts[i];
        q->life -= dt;
        if (q->life <= 0) continue;
        if (q->gravity) q->vel.y -= 14.0f * dt;
        vec3 np = v3_add(q->pos, v3_scale(q->vel, dt));
        if (game_solid(g, (int)floorf(np.x), (int)floorf(np.y), (int)floorf(np.z)) != 0) {
            q->vel = v3_scale(q->vel, -0.2f);
            q->vel.x *= 0.3f; q->vel.z *= 0.3f;
        } else {
            q->pos = np;
        }
        g->parts[n++] = *q;
    }
    g->nparts = n;
    for (int i = 0; i < MAX_FLASHES; i++) g->flashes[i].life -= dt;
}

/* ---------- projectiles ---------- */

static Projectile *proj_new(Game *g, int owner, u8 kind, vec3 pos, vec3 vel, float dmg) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (p->alive) continue;
        memset(p, 0, sizeof *p);
        p->alive = true;
        p->kind = kind;
        p->pos = pos;
        p->vel = vel;
        p->dmg = dmg;
        p->life = kind == PJ_NOVA ? 0.9f : 4.0f;
        p->last_hit = -1;
        p->owner = owner;
        p->element = EL_NONE;
        return p;
    }
    return NULL;
}

static vec3 proj_color(const Projectile *p) {
    switch (p->kind) {
    case PJ_ARROW: return v3(0.75f, 0.65f, 0.5f);
    case PJ_BOLT:
        if (p->element == EL_SHADOW) return v3(0.95f, 0.15f, 0.2f); /* vampiric */
        if (p->element != EL_NONE) return element_color(p->element);
        return p->explode ? v3(1.0f, 0.55f, 0.2f) : p->rarity >= RAR_RARE ? rarity_color(p->rarity) : v3(0.55f, 0.6f, 1.0f);
    case PJ_NOVA: return v3(1.0f, 0.5f, 0.15f);
    case PJ_SPELL: return element_color(p->element);
    case PJ_ENEMY_ARROW: return v3(0.85f, 0.82f, 0.7f);
    default: return p->element != EL_NONE ? element_color(p->element) : v3(1.0f, 0.4f, 0.15f);
    }
}

static void explode(Game *g, int owner, vec3 at, float dmg, float radius, const Monster *skip) {
    sfx_at(g, SFX_EXPLOSION, at, 1.0f);
    flash_add(g, at, v3(1.4f, 0.75f, 0.3f), radius * 3.0f, 0.35f);
    particles_burst(g, at, v3(1.0f, 0.55f, 0.2f), 24, 6.0f, 0.08f, true);
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || m == skip) continue;
        vec3 c = v3_add(m->pos, v3(0, MONSTERS[m->type].h * 0.5f, 0));
        float d = v3_len(v3_sub(c, at));
        if (d < radius) monster_damage(g, m, dmg * (1.0f - 0.5f * d / radius), at, 4.0f, owner);
    }
}

static void chain_lightning(Game *g, int owner, Monster *from, float dmg) {
    Monster *prev = from;
    for (int k = 0; k < 2; k++) {
        vec3 pc = v3_add(prev->pos, v3(0, 0.6f, 0));
        Monster *best = NULL;
        float bd = 9.0f;
        for (int i = 0; i < MAX_MONSTERS; i++) {
            Monster *m = &g->mons[i];
            if (!m->alive || m == from || m == prev) continue;
            float d = v3_len(v3_sub(m->pos, prev->pos));
            if (d < bd && phys_line_of_sight(pc, v3_add(m->pos, v3(0, 0.6f, 0)), game_solid, g)) { bd = d; best = m; }
        }
        if (!best) return;
        vec3 bc = v3_add(best->pos, v3(0, 0.6f, 0));
        if (k == 0) sfx_at(g, SFX_CHAIN_ZAP, bc, 1.0f);
        for (int s = 0; s <= 8; s++) {
            vec3 q = v3_lerp(pc, bc, s / 8.0f);
            q.y += (s % 2 ? 0.15f : -0.1f);
            particles_burst(g, q, v3(0.7f, 0.8f, 1.0f), 1, 0.5f, 0.05f, true);
        }
        flash_add(g, bc, v3(0.6f, 0.7f, 1.2f), 6.0f, 0.2f);
        monster_damage(g, best, dmg, pc, 2.0f, owner);
        prev = best;
    }
}

static void projectile_hit_monster(Game *g, Projectile *p, Monster *m, int index) {
    p->last_hit = index;
    if (p->kind == PJ_SPELL) { spell_projectile_hit(g, p, m); return; }
    float knock = p->kind == PJ_ARROW ? 3.0f : 5.0f;
    if (p->kind == PJ_ARROW) sfx_at(g, SFX_HIT_ARROW, p->pos, 1.0f);
    else if (p->kind == PJ_BOLT) sfx_at(g, p->element != EL_NONE ? sfx_element_impact(p->element) : SFX_BOLT_HIT, p->pos, 1.0f);
    else if (p->kind == PJ_NOVA) sfx_at(g, SFX_IMPACT_FIRE, p->pos, 0.7f);
    if (p->element != EL_NONE) element_hit(g, m, p->element, p->dmg, v3_sub(p->pos, p->vel), knock, p->owner);
    else monster_damage(g, m, p->dmg, v3_sub(p->pos, p->vel), knock, p->owner);
    if (p->kind == PJ_ARROW || p->kind == PJ_BOLT) weapon_impact_fx(g, p->element, p->rarity, p->element == EL_SHADOW, p->pos, p->owner);
    if (p->kind == PJ_BOLT) {
        if (p->explode) explode(g, p->owner, p->pos, p->dmg * 0.6f, 3.0f, m);
        if (p->chain) chain_lightning(g, p->owner, m, p->dmg * 0.6f);
        flash_add(g, p->pos, proj_color(p), 6.0f, 0.2f);
    }
    if (p->pierce > 0) p->pierce--;
    else p->alive = false;
}

/* A troll's boulder lands: it hurts everyone near, more the closer. */
static void boulder_burst(Game *g, Projectile *p, vec3 at) {
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        float d = v3_len(v3_sub(v3_add(ps->pl.pos, v3(0, 0.6f, 0)), at));
        if (d > p->radius) continue;
        tele_src(HS_BLAST, 31, 0, 0);
        game_hurt_player(g, ps, p->dmg * (1.0f - 0.5f * d / p->radius), at);
        if (p->element != EL_NONE) player_element_hit(g, ps, p->element, p->dmg * 0.5f);
    }
    particles_burst(g, at, v3(0.45f, 0.4f, 0.34f), 24, 6.0f, 0.12f, false);
    particles_burst(g, at, v3(0.6f, 0.55f, 0.48f), 10, 3.0f, 0.2f, false);
    sfx_at(g, SFX_BOULDER_IMPACT, at, 1.0f);
}

static void update_projectiles(Game *g, float dt0) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (!p->alive) continue;
        /* Time Slip slows what the monsters threw */
        bool enemy = p->kind == PJ_ENEMY_FIRE || p->kind == PJ_ENEMY_ARROW || p->kind == PJ_BOULDER;
        float dt = enemy ? dt0 * utility_time_scale(g) : dt0;
        p->life -= dt;
        if (p->life <= 0) { p->alive = false; continue; }
        if (p->kind == PJ_ARROW || p->kind == PJ_ENEMY_ARROW) p->vel.y -= 5.0f * dt;
        else if (p->kind == PJ_BOULDER) p->vel.y -= 14.0f * dt;
        p->age += dt;
        float len = v3_len(p->vel) * dt;
        int steps = (int)ceilf(len / 0.25f);
        vec3 step = v3_scale(p->vel, dt / (float)steps);
        for (int s = 0; s < steps && p->alive; s++) {
            p->pos = v3_add(p->pos, step);
            if (game_solid(g, (int)floorf(p->pos.x), (int)floorf(p->pos.y), (int)floorf(p->pos.z)) != 0) {
                p->alive = false;
                vec3 back = v3_sub(p->pos, step);
                switch (p->kind) {
                case PJ_ARROW: case PJ_ENEMY_ARROW: sfx_at(g, SFX_ARROW_WALL, back, 1.0f); break;
                case PJ_BOLT: if (!p->explode) sfx_at(g, SFX_BOLT_HIT, back, 0.8f); break;
                case PJ_SPELL: sfx_at(g, sfx_element_impact(p->element), back, 0.7f); break;
                case PJ_ENEMY_FIRE: sfx_at(g, SFX_ENEMY_FIRE_HIT, back, 0.8f); break;
                case PJ_BOULDER: boulder_burst(g, p, back); break;
                default: break;
                }
                if (p->kind == PJ_BOLT && p->explode) explode(g, p->owner, back, p->dmg * 0.6f, 3.0f, NULL);
                else particles_burst(g, back, proj_color(p), 6, 2.5f, 0.04f, p->kind != PJ_ARROW && p->kind != PJ_ENEMY_ARROW);
                if (p->kind != PJ_ARROW && p->kind != PJ_ENEMY_ARROW) flash_add(g, back, proj_color(p), 5.0f, 0.15f);
                break;
            }
            if (p->kind == PJ_ENEMY_FIRE || p->kind == PJ_ENEMY_ARROW || p->kind == PJ_BOULDER) {
                for (int k = 0; k < MAX_PLAYERS && p->alive; k++) {
                    PlayerSlot *ps = &g->players[k];
                    vec3 f = ps->pl.pos;
                    if (!ps->active || ps->st.dead) continue;
                    if (fabsf(p->pos.x - f.x) < 0.45f && fabsf(p->pos.z - f.z) < 0.45f && p->pos.y > f.y &&
                        p->pos.y < f.y + player_height(ps->pl.stance) + 0.1f) {
                        if (p->kind == PJ_BOULDER) {
                            boulder_burst(g, p, p->pos);
                            p->alive = false;
                            continue;
                        }
                        sfx_at(g, p->kind == PJ_ENEMY_ARROW ? SFX_HIT_ARROW : SFX_ENEMY_FIRE_HIT, p->pos, 1.0f);
                        tele_src(HS_PROJ, p->kind == PJ_ENEMY_ARROW ? MON_SKELETON : 30, 0, 0);
                        game_hurt_player(g, ps, p->dmg, v3_sub(p->pos, step));
                        if (p->element != EL_NONE) player_element_hit(g, ps, p->element, p->dmg);
                        particles_burst(g, p->pos, proj_color(p), 10, 3.0f, 0.05f, true);
                        p->alive = false;
                    }
                }
                continue;
            }
            if (p->kind == PJ_SPELL && p->form == SF_ORB) continue; /* orbs hurt by touch, in spells_update */
            for (int k = 0; k < MAX_MONSTERS; k++) {
                Monster *m = &g->mons[k];
                if (!m->alive || m->hidden || k == p->last_hit) continue;
                float hw = MONSTERS[m->type].hw * m->scale + 0.15f, h = MONSTERS[m->type].h * m->scale;
                if (fabsf(p->pos.x - m->pos.x) < hw && fabsf(p->pos.z - m->pos.z) < hw && p->pos.y > m->pos.y - 0.1f &&
                    p->pos.y < m->pos.y + h + 0.1f) {
                    projectile_hit_monster(g, p, m, k);
                    break;
                }
            }
        }
        /* trailing sparks for spells */
        bool magic_arrow = p->kind == PJ_ARROW && (p->element != EL_NONE || p->rarity > RAR_COMMON);
        if (p->alive && (magic_arrow || (p->kind != PJ_ARROW && p->kind != PJ_ENEMY_ARROW && p->kind != PJ_BOULDER)) && g->nparts < MAX_PARTICLES - 1 &&
            game_nearest_player(g, p->pos, 40.0f)) {
            Particle *q = &g->parts[g->nparts++];
            q->pos = p->pos;
            q->vel = v3(0, 0.3f, 0);
            q->color = magic_arrow ? (p->element == EL_SHADOW ? v3(0.95f, 0.15f, 0.2f)
                                      : p->element != EL_NONE ? element_color(p->element) : rarity_color(p->rarity))
                                   : proj_color(p);
            q->max_life = q->life = 0.3f;
            q->size = 0.05f;
            q->emissive = true;
            q->gravity = false;
        }
    }
}

/* ---------- attacks ---------- */

static void sword_strike(Game *g, PlayerSlot *ps, bool whirl) {
    Stats *s = &ps->st;
    vec3 eye = player_eye(&ps->pl);
    vec3 fwd = v3(cosf(ps->pl.yaw), 0, sinf(ps->pl.yaw));
    const WeaponBase *wb = equipped_base(s, WPN_SWORD);
    float dmg = 20.0f * (1.0f + 0.3f * s->ranks[SK_KEEN_EDGE]) * level_mult(s) * (whirl ? 1.5f : 1.0f) * weapon_mult(s, WPN_SWORD);
    float range = whirl ? wb->range + 0.7f : wb->range;
    float arc = whirl ? 2.0f * PI_F : fmaxf(wb->arc_deg, s->ranks[SK_CLEAVE] ? 150.0f : 0.0f) * PI_F / 180.0f;
    float knock = wb->knock * (item_has_affix(&s->equipped[WPN_SWORD], AF_HEAVY) ? 1.5f : 1.0f);
    bool hit_all = whirl || s->ranks[SK_CLEAVE];
    Monster *best = NULL;
    float best_d = 1e9f;
    int hits = 0;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || m->hidden) continue;
        float mh = MONSTERS[m->type].h * m->scale;
        vec3 c = v3_add(m->pos, v3(0, mh * 0.5f, 0)), top = v3_add(m->pos, v3(0, mh * 0.9f, 0));
        /* reach is measured from the chest, and goes further down than up: a slime a step
         * below can be struck as it strikes you */
        vec3 chest = v3_add(eye, v3(0, -0.5f, 0));
        vec3 d = v3_sub(c, chest);
        float dy = d.y;
        d.y = 0;
        float dist = v3_len(d) - MONSTERS[m->type].hw * m->scale;
        if (dist > range || dy > 2.2f || dy < -2.8f) continue;
        float ang = acosf(clampf(v3_dot(v3_norm(d), fwd), -1.0f, 1.0f));
        if (ang > arc * 0.5f && dist > 0.4f) continue;
        /* over a ledge's edge: seeing its top is enough */
        if (!phys_line_of_sight(eye, c, game_solid, g) && !phys_line_of_sight(eye, top, game_solid, g)) continue;
        if (hit_all) {
            weapon_hit(g, ps, m, WPN_SWORD, dmg, ps->pl.pos, knock);
            hits++;
        } else if (dist < best_d) {
            best_d = dist;
            best = m;
        }
    }
    if (best) {
        weapon_hit(g, ps, best, WPN_SWORD, dmg, ps->pl.pos, knock);
        hits++;
    }
    if (hits) {
        s->hp = fminf(s->maxhp, s->hp + 3.0f * s->ranks[SK_BLOODTHIRST] * hits);
    } else {
        /* sparks when hitting a wall */
        vec3 look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
        float d = phys_raycast(eye, look, range, game_solid, g);
        if (d < range) {
            vec3 at = v3_add(eye, v3_scale(look, d - 0.05f));
            particles_burst(g, at, v3(1.0f, 0.85f, 0.5f), 6, 2.5f, 0.03f, true);
            sfx_at(g, SFX_HIT_WALL, at, 1.0f);
        }
    }
}

/* Shot direction. Gamepad players get a little aim assist: a monster within a few
 * degrees of the crosshair pulls the shot onto itself. */
static vec3 aim_dir(Game *g, PlayerSlot *ps, float yaw_off, float pitch_off) {
    vec3 eye = player_eye(&ps->pl);
    vec3 look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    vec3 d = dir_from_angles(ps->pl.yaw + yaw_off, ps->pl.pitch + pitch_off);
    bool touch = ps->keyboard && g->touch.active;
    if (!touch && (!ps->pad || (ps->keyboard && g->mouse))) return d;
    float best = cosf(6.0f * PI_F / 180.0f);
    vec3 target = d;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        vec3 c = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
        vec3 to = v3_sub(c, eye);
        float dist = v3_len(to);
        if (dist > 40.0f || dist < 0.5f) continue;
        float cs = v3_dot(v3_scale(to, 1.0f / dist), look);
        if (cs <= best || !phys_line_of_sight(eye, c, game_solid, g)) continue;
        best = cs;
        target = v3_scale(to, 1.0f / dist);
    }
    if (target.x == d.x && target.y == d.y && target.z == d.z) return d;
    /* keep the spread of multishot around the assisted direction */
    float ty = atan2f(target.z, target.x), tp = asinf(clampf(target.y, -1.0f, 1.0f));
    return dir_from_angles(ty + yaw_off, tp + pitch_off * 0.5f);
}

static void fire_arrows(Game *g, PlayerSlot *ps, int count, float spread_deg) {
    Stats *s = &ps->st;
    vec3 eye = player_eye(&ps->pl);
    const WeaponBase *wb = equipped_base(s, WPN_BOW);
    float dmg = 15.0f * (1.0f + 0.3f * s->ranks[SK_STEADY_AIM]) * level_mult(s) * weapon_mult(s, WPN_BOW);
    for (int i = 0; i < count; i++) {
        float off = count > 1 ? ((float)i / (count - 1) - 0.5f) * 2.0f * spread_deg * PI_F / 180.0f : 0.0f;
        vec3 d = aim_dir(g, ps, off, 0.02f);
        Projectile *p = proj_new(g, ps->index, PJ_ARROW, v3_add(eye, v3_scale(d, 0.3f)), v3_scale(d, wb->speed), dmg);
        if (!p) continue;
        p->pierce = wb->pierce + (s->ranks[SK_PIERCING] ? 3 : 0);
        p->element = (u8)item_affix_element(&s->equipped[WPN_BOW]);
        p->rarity = s->equipped[WPN_BOW].rarity;
    }
}

static void cast_bolt(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    vec3 eye = player_eye(&ps->pl);
    vec3 d = aim_dir(g, ps, 0.0f, 0.0f);
    const WeaponBase *wb = equipped_base(s, WPN_STAFF);
    float dmg = 24.0f * (1.0f + 0.3f * s->ranks[SK_FOCUS]) * level_mult(s) * weapon_mult(s, WPN_STAFF);
    Projectile *p = proj_new(g, ps->index, PJ_BOLT, v3_add(eye, v3_scale(d, 0.4f)), v3_scale(d, wb->speed), dmg);
    if (p) {
        p->element = (u8)item_affix_element(&s->equipped[WPN_STAFF]);
        p->rarity = s->equipped[WPN_STAFF].rarity;
        p->explode = s->ranks[SK_FIREBALL] > 0;
        p->chain = s->ranks[SK_CHAIN] > 0;
    }
}

static void cast_nova(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    vec3 c = v3_add(ps->pl.pos, v3(0, 0.8f, 0));
    float dmg = 30.0f * (1.0f + 0.3f * s->ranks[SK_FOCUS]) * level_mult(s) * weapon_mult(s, WPN_STAFF);
    for (int i = 0; i < 20; i++) {
        float a = i * 2.0f * PI_F / 20.0f;
        vec3 d = v3(cosf(a), 0, sinf(a));
        Projectile *p = proj_new(g, ps->index, PJ_NOVA, v3_add(c, v3_scale(d, 0.5f)), v3_scale(d, 14.0f), dmg);
        if (p) p->pierce = 2;
    }
    flash_add(g, c, v3(1.5f, 0.7f, 0.25f), 12.0f, 0.5f);
}

static float mana_cost(const Stats *s, float base) { return base * (1.0f - 0.15f * s->ranks[SK_FOCUS]); }

static void try_attack(Game *g, PlayerSlot *ps, bool alt) {
    Stats *s = &ps->st;
    if (s->cooldown > 0 || s->dead) return;
    utility_player_attacked(ps);
    const Item *it = &s->equipped[s->weapon];
    const WeaponBase *wb = &WEAPON_BASES[it->base];
    float speed = item_speed_mult(it) * (s->haste_t > 0 ? 0.7f : 1.0f);
    switch (s->weapon) {
    case WPN_SWORD:
        if (alt) {
            if (!s->ranks[SK_WHIRLWIND] || s->mana < 12) return;
            s->mana -= 12;
            sword_strike(g, ps, true);
            s->cooldown = s->swing_len = 0.8f * speed;
        } else {
            sword_strike(g, ps, false);
            s->cooldown = s->swing_len = wb->cooldown * speed;
        }
        break;
    case WPN_BOW:
        if (alt) {
            if (!s->ranks[SK_ARROW_STORM] || s->mana < 15) return;
            s->mana -= 15;
            fire_arrows(g, ps, 9, 20.0f);
            s->cooldown = s->swing_len = 1.0f * speed;
        } else {
            fire_arrows(g, ps, s->ranks[SK_MULTISHOT] ? 3 : 1, 6.0f);
            s->cooldown = s->swing_len = wb->cooldown * speed;
        }
        break;
    case WPN_STAFF:
        if (alt) {
            float c = mana_cost(s, 25);
            if (!s->ranks[SK_NOVA] || s->mana < c) return;
            s->mana -= c;
            cast_nova(g, ps);
            s->cooldown = s->swing_len = 1.1f * speed;
        } else {
            float c = mana_cost(s, wb->mana);
            if (s->mana < c) return;
            s->mana -= c;
            cast_bolt(g, ps);
            s->cooldown = s->swing_len = wb->cooldown * speed;
        }
        break;
    }
    s->swing = s->swing_len;
    s->swing_kind = alt ? 1 : 0;
    /* the sound of the weapon in hand */
    static const int SWING[WB_COUNT] = {
        [WB_DAGGER] = SFX_SWING_DAGGER, [WB_SWORD] = SFX_SWING_SWORD, [WB_AXE] = SFX_SWING_AXE, [WB_MACE] = SFX_SWING_MACE,
        [WB_SPEAR] = SFX_SWING_SPEAR, [WB_GREATSWORD] = SFX_SWING_GREATSWORD, [WB_SHORTBOW] = SFX_SHOOT_SHORTBOW,
        [WB_LONGBOW] = SFX_SHOOT_LONGBOW, [WB_CROSSBOW] = SFX_SHOOT_CROSSBOW, [WB_WAND] = SFX_CAST_WAND,
        [WB_STAFF] = SFX_CAST_STAFF, [WB_SCEPTER] = SFX_CAST_SCEPTER,
    };
    static const int ALT[WPN_COUNT] = { SFX_WHIRLWIND, SFX_ARROW_STORM, SFX_STAFF_NOVA };
    /* metal weapons a little lower the heavier they are */
    static const float PITCH[WB_COUNT] = { [WB_DAGGER] = 1.0f, [WB_SWORD] = 0.85f, [WB_AXE] = 0.82f, [WB_MACE] = 0.75f,
                                           [WB_SPEAR] = 0.9f, [WB_GREATSWORD] = 0.72f, [WB_SHORTBOW] = 1.0f, [WB_LONGBOW] = 0.95f,
                                           [WB_CROSSBOW] = 0.95f, [WB_WAND] = 1.0f, [WB_STAFF] = 1.0f, [WB_SCEPTER] = 0.95f };
    int b = it->base < WB_COUNT ? it->base : WB_SWORD;
    if (alt) sfx_player(g, ps, ALT[s->weapon], 1.0f);
    else sfx_player_pitch(g, ps, SWING[b], 1.0f, PITCH[b]);
}

void combat_update(Game *g, float dt) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        Stats *s = &ps->st;
        s->cooldown -= dt;
        s->swing = fmaxf(0.0f, s->swing - dt);
        s->hurt = fmaxf(0.0f, s->hurt - dt * 1.5f);
        s->since_hurt += dt;
        s->spell_cd -= dt;
        ps->potion_cd -= dt;
        if (s->ward_t > 0 && (s->ward_t -= dt) <= 0) s->ward = 0;
        if (s->haste_t > 0) s->haste_t -= dt;
        ps->pl.speed_mult = (1.0f + 0.12f * s->ranks[SK_FLEET_FOOT]) * (1.0f + relic_bonus(s, RB_SPEED)) * (s->haste_t > 0 ? 1.4f : 1.0f);
        /* monsters' elements: burning and poison eat health, a chill slows */
        if (s->chill_t > 0) { s->chill_t -= dt; ps->pl.speed_mult *= 0.6f; }
        if (s->stoneskin_t > 0) ps->pl.speed_mult *= 0.75f;
        if (s->steed_t > 0) ps->pl.speed_mult *= 1.9f;
        if (s->burn_t > 0) {
            s->burn_t -= dt;
            game_dot_player(g, ps, s->burn_dps * dt);
            if (fmodf(s->burn_t, 0.25f) < dt) particles_burst(g, v3_add(ps->pl.pos, v3(0, 0.8f, 0)), element_color(EL_FIRE), 1, 1.5f, 0.04f, true);
        }
        if (s->poison_t > 0) { s->poison_t -= dt; game_dot_player(g, ps, s->poison_dps * dt); }
        if (s->dead) continue;
        s->mana = fminf(s->maxmana, s->mana + (3.0f + 0.1f * s->level) * (s->refreshed_t > 0 ? 2.0f : 1.0f) * dt);
        /* wounds close only out of a fight: some time unhurt and nothing awake close by */
        if (s->since_hurt > 6.0f) {
            bool calm = true;
            for (int k = 0; k < MAX_MONSTERS && calm; k++)
                calm = !(g->mons[k].alive && g->mons[k].awake && g->mons[k].charm_t <= 0 &&
                         v3_len(v3_sub(g->mons[k].pos, ps->pl.pos)) < 12.0f);
            if (calm) s->hp = fminf(s->maxhp, s->hp + (1.5f + 2.0f * s->ranks[SK_SECOND_WIND]) * dt);
        }
        if (s->well_fed_t > 0) s->hp = fminf(s->maxhp, s->hp + s->maxhp * 0.004f * dt); /* well fed: slowly, even in a fight */
        bool mouse = ps->keyboard && (g->mouse || g->opt.demo);
        bool touch = ps->keyboard && g->touch.active;
        bool attack = (mouse && ps->attack_held) || ps->pad_attack || (touch && g->touch.held[TB_ATTACK]);
        bool alt = (mouse && ps->alt_held) || ps->pad_alt || (touch && g->touch.held[TB_ALT]);
        if (attack && s->cooldown <= 0 && mech_use(g, ps)) {
            /* struck a lever, a statue or a mirror */
            s->cooldown = s->swing_len = s->swing = 0.45f;
            s->swing_kind = 0;
        } else if (attack) try_attack(g, ps, false);
        else if (alt) try_attack(g, ps, true);
        bool spell = (ps->keyboard && ps->spell_held && (g->mouse || g->opt.demo)) || ps->pad_spell || (touch && g->touch.held[TB_SPELL]);
        if (spell) spell_cast(g, ps);
    }
    spells_update(g, dt);
    pickups_update(g, dt);
    update_projectiles(g, dt);
    update_particles(g, dt);
}

/* ---------- drawing ---------- */

void combat_draw_world(Game *g, vec3 eye) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (!p->alive) continue;
        if (p->kind == PJ_BOULDER) {
            vec3 light = game_light_at(g, p->pos);
            vec3 rock = v3(0.45f * light.x + 0.05f, 0.4f * light.y + 0.05f, 0.34f * light.z + 0.05f);
            batch3d_box(p->pos, v3(0.3f, 0.26f, 0.3f), p->age * 5.0f, rock, false);
            batch3d_box(v3_add(p->pos, v3(0.1f, 0.12f, 0)), v3(0.18f, 0.16f, 0.2f), -p->age * 4.0f, v3_scale(rock, 0.8f), false);
            continue;
        }
        if (p->kind == PJ_ARROW || p->kind == PJ_ENEMY_ARROW) {
            vec3 d = v3_norm(p->vel);
            vec3 light = game_light_at(g, p->pos);
            vec3 tint = p->kind == PJ_ENEMY_ARROW ? (p->element != EL_NONE ? element_color(p->element) : v3(0.85f, 0.82f, 0.7f))
                                                  : p->element != EL_NONE ? element_color(p->element) : v3(0.6f, 0.5f, 0.35f);
            bool magic = p->kind == PJ_ARROW && (p->element != EL_NONE || p->rarity > RAR_COMMON);
            if (p->element == EL_SHADOW) tint = v3(0.95f, 0.15f, 0.2f);
            else if (magic && p->element == EL_NONE) tint = rarity_color(p->rarity);
            vec3 col = magic ? tint : v3(tint.x * light.x, tint.y * light.y, tint.z * light.z);
            vec3 up = fabsf(d.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
            vec3 sx = v3_scale(v3_norm(v3_cross(d, up)), 0.02f), sy = v3_scale(v3_norm(v3_cross(sx, d)), 0.02f);
            batch3d_obox(p->pos, sx, sy, v3_scale(d, 0.35f), magic ? v3(0.5f * light.x, 0.4f * light.y, 0.3f * light.z) : col, false);
            batch3d_spike(v3_add(p->pos, v3_scale(d, 0.35f)), d, 0.04f, 0.1f, magic ? col : v3(0.5f * light.x, 0.5f * light.y, 0.55f * light.z), magic);
            if (magic) batch3d_obox(v3_sub(p->pos, v3_scale(d, 0.3f)), v3_scale(sx, 2.0f), v3_scale(sy, 2.0f), v3_scale(d, 0.06f), col, true); /* fletching */
        } else {
            float r = p->kind == PJ_NOVA ? 0.18f : p->kind == PJ_SPELL ? p->radius : 0.16f;
            if (p->kind == PJ_SPELL && p->form == SF_ORB) r *= 1.0f + 0.15f * sinf(p->age * 10.0f);
            batch3d_octa(p->pos, r, r, g->time * 8.0f, proj_color(p), true);
            if (p->kind == PJ_SPELL && p->radius > 0.3f) batch3d_octa(p->pos, r * 0.5f, r * 1.3f, -g->time * 5.0f, v3(1, 1, 1), true);
        }
    }
    for (int i = 0; i < g->nparts; i++) {
        Particle *q = &g->parts[i];
        if (v3_len(v3_sub(q->pos, eye)) > 50.0f) continue;
        float t = q->life / q->max_life;
        vec3 col = q->color;
        if (!q->emissive) {
            vec3 l = game_light_at(g, q->pos);
            col = v3(col.x * l.x, col.y * l.y, col.z * l.z);
        } else {
            col = v3_scale(col, 0.5f + 0.5f * t);
        }
        float sz = q->size * (q->emissive ? t : 1.0f);
        batch3d_box(q->pos, v3(sz, sz, sz), q->life * 7.0f, col, q->emissive);
    }
}

void combat_collect_lights(Game *g, PlayerSlot *viewer, SceneParams *sp) {
    vec3 eye = game_view_eye(viewer);
    sp->ndl = 0;
    /* the other players' torches (the viewer's own torch is part of the scene lighting) */
    for (int i = 0; i < MAX_PLAYERS && sp->ndl < MAX_DLIGHTS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps == viewer || ps->st.dead) continue;
        vec3 p = player_eye(&ps->pl);
        if (v3_len(v3_sub(p, eye)) > 60.0f) continue;
        sp->dl_pos[sp->ndl] = p;
        sp->dl_col[sp->ndl] = v3(1.0f, 0.62f, 0.3f);
        sp->dl_rad[sp->ndl] = 9.0f;
        sp->ndl++;
    }
    /* flashes first, then the nearest glowing projectiles */
    for (int i = 0; i < MAX_FLASHES && sp->ndl < MAX_DLIGHTS; i++) {
        Flash *f = &g->flashes[i];
        if (f->life <= 0) continue;
        float t = f->life / f->max_life;
        sp->dl_pos[sp->ndl] = f->pos;
        sp->dl_col[sp->ndl] = v3_scale(f->color, t);
        sp->dl_rad[sp->ndl] = f->radius;
        sp->ndl++;
    }
    for (int pass = 0; pass < 2 && sp->ndl < MAX_DLIGHTS; pass++)
        for (int i = 0; i < MAX_PROJECTILES && sp->ndl < MAX_DLIGHTS; i++) {
            Projectile *p = &g->proj[i];
            if (!p->alive || p->kind == PJ_ENEMY_ARROW || p->kind == PJ_BOULDER) continue;
            if (p->kind == PJ_ARROW && p->element == EL_NONE) continue; /* only elemental arrows give light */
            if (p->kind == PJ_SPELL && p->form == SF_BREATH && (i & 3)) continue; /* breath: a few lights are enough */
            float d = v3_len(v3_sub(p->pos, eye));
            if ((pass == 0) != (d < 20.0f)) continue;
            sp->dl_pos[sp->ndl] = p->pos;
            sp->dl_col[sp->ndl] = p->kind == PJ_ARROW ? v3_scale(p->element == EL_SHADOW ? v3(0.95f, 0.15f, 0.2f) : element_color(p->element), 0.6f)
                                                      : v3_scale(proj_color(p), 0.9f);
            sp->dl_rad[sp->ndl] = p->kind == PJ_NOVA ? 4.0f : p->kind == PJ_ARROW ? 4.0f : 7.0f;
            sp->ndl++;
        }
    story_thread_lights(g, eye, sp);
    spell_collect_lights(g, eye, sp);
}

/* Weapon in view: drawn in camera space (x right, y up, -z forward). */
void combat_draw_viewmodel(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    if (s->dead || ps->pl.fly) return;
    vec3 light = game_light_at(g, player_eye(&ps->pl));
    light = v3(fminf(light.x + 0.15f, 1.3f), fminf(light.y + 0.12f, 1.3f), fminf(light.z + 0.1f, 1.3f));
    weapon_draw_view(g, ps, light);
    (void)WEAPON_NAMES;
}

const char *combat_weapon_name(int w) { return WEAPON_NAMES[w]; }
