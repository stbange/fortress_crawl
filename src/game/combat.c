#include "game/game.h"

#include "render/batch.h"

/* Retro (Hexen-like) combat: instant swings and projectiles, no combos.
 * Three weapons, each with an alternate attack unlocked by the last tier of its branch. */

static const char *WEAPON_NAMES[WPN_COUNT] = { "SWORD", "BOW", "STAFF" };

static float level_mult(const Stats *s) { return 1.0f + 0.07f * (float)(s->level - 1); }

float stats_max_hp(const Stats *s) { return 100.0f + 12.0f * (s->level - 1) + 25.0f * s->ranks[SK_TOUGHNESS]; }
float stats_max_mana(const Stats *s) { return 40.0f + 6.0f * (s->level - 1); }

void stats_refresh(PlayerSlot *ps) {
    ps->st.maxhp = stats_max_hp(&ps->st);
    ps->st.maxmana = stats_max_mana(&ps->st);
    ps->pl.speed_mult = 1.0f + 0.12f * ps->st.ranks[SK_FLEET_FOOT];
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
        return p;
    }
    return NULL;
}

static vec3 proj_color(const Projectile *p) {
    switch (p->kind) {
    case PJ_ARROW: return v3(0.75f, 0.65f, 0.5f);
    case PJ_BOLT: return p->explode ? v3(1.0f, 0.55f, 0.2f) : v3(0.55f, 0.6f, 1.0f);
    case PJ_NOVA: return v3(1.0f, 0.5f, 0.15f);
    default: return v3(1.0f, 0.4f, 0.15f);
    }
}

static void explode(Game *g, int owner, vec3 at, float dmg, float radius, const Monster *skip) {
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
    monster_damage(g, m, p->dmg, v3_sub(p->pos, p->vel), p->kind == PJ_ARROW ? 3.0f : 5.0f, p->owner);
    p->last_hit = index;
    if (p->kind == PJ_BOLT) {
        if (p->explode) explode(g, p->owner, p->pos, p->dmg * 0.6f, 3.0f, m);
        if (p->chain) chain_lightning(g, p->owner, m, p->dmg * 0.6f);
        flash_add(g, p->pos, proj_color(p), 6.0f, 0.2f);
    }
    if (p->pierce > 0) p->pierce--;
    else p->alive = false;
}

static void update_projectiles(Game *g, float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (!p->alive) continue;
        p->life -= dt;
        if (p->life <= 0) { p->alive = false; continue; }
        if (p->kind == PJ_ARROW) p->vel.y -= 5.0f * dt;
        float len = v3_len(p->vel) * dt;
        int steps = (int)ceilf(len / 0.25f);
        vec3 step = v3_scale(p->vel, dt / (float)steps);
        for (int s = 0; s < steps && p->alive; s++) {
            p->pos = v3_add(p->pos, step);
            if (game_solid(g, (int)floorf(p->pos.x), (int)floorf(p->pos.y), (int)floorf(p->pos.z)) != 0) {
                p->alive = false;
                vec3 back = v3_sub(p->pos, step);
                if (p->kind == PJ_BOLT && p->explode) explode(g, p->owner, back, p->dmg * 0.6f, 3.0f, NULL);
                else particles_burst(g, back, proj_color(p), 6, 2.5f, 0.04f, p->kind != PJ_ARROW);
                if (p->kind != PJ_ARROW) flash_add(g, back, proj_color(p), 5.0f, 0.15f);
                break;
            }
            if (p->kind == PJ_ENEMY_FIRE) {
                for (int k = 0; k < MAX_PLAYERS && p->alive; k++) {
                    PlayerSlot *ps = &g->players[k];
                    vec3 f = ps->pl.pos;
                    if (!ps->active || ps->st.dead) continue;
                    if (fabsf(p->pos.x - f.x) < 0.45f && fabsf(p->pos.z - f.z) < 0.45f && p->pos.y > f.y &&
                        p->pos.y < f.y + player_height(ps->pl.stance) + 0.1f) {
                        game_hurt_player(g, ps, p->dmg, v3_sub(p->pos, step));
                        particles_burst(g, p->pos, proj_color(p), 10, 3.0f, 0.05f, true);
                        p->alive = false;
                    }
                }
                continue;
            }
            for (int k = 0; k < MAX_MONSTERS; k++) {
                Monster *m = &g->mons[k];
                if (!m->alive || k == p->last_hit) continue;
                float hw = MONSTERS[m->type].hw * m->scale + 0.15f, h = MONSTERS[m->type].h * m->scale;
                if (fabsf(p->pos.x - m->pos.x) < hw && fabsf(p->pos.z - m->pos.z) < hw && p->pos.y > m->pos.y - 0.1f &&
                    p->pos.y < m->pos.y + h + 0.1f) {
                    projectile_hit_monster(g, p, m, k);
                    break;
                }
            }
        }
        /* trailing sparks for spells */
        if (p->alive && p->kind != PJ_ARROW && g->nparts < MAX_PARTICLES - 1 && game_nearest_player(g, p->pos, 40.0f)) {
            Particle *q = &g->parts[g->nparts++];
            q->pos = p->pos;
            q->vel = v3(0, 0.3f, 0);
            q->color = proj_color(p);
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
    float dmg = 20.0f * (1.0f + 0.3f * s->ranks[SK_KEEN_EDGE]) * level_mult(s) * (whirl ? 1.5f : 1.0f);
    float range = whirl ? 3.4f : 2.7f;
    float arc = whirl ? 2.0f * PI_F : (s->ranks[SK_CLEAVE] ? 150.0f : 80.0f) * PI_F / 180.0f;
    bool hit_all = whirl || s->ranks[SK_CLEAVE];
    Monster *best = NULL;
    float best_d = 1e9f;
    int hits = 0;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        vec3 c = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
        vec3 d = v3_sub(c, eye);
        float dy = d.y;
        d.y = 0;
        float dist = v3_len(d) - MONSTERS[m->type].hw * m->scale;
        if (dist > range || fabsf(dy) > 2.2f) continue;
        float ang = acosf(clampf(v3_dot(v3_norm(d), fwd), -1.0f, 1.0f));
        if (ang > arc * 0.5f && dist > 0.4f) continue;
        if (!phys_line_of_sight(eye, c, game_solid, g)) continue;
        if (hit_all) {
            monster_damage(g, m, dmg, ps->pl.pos, 5.0f, ps->index);
            hits++;
        } else if (dist < best_d) {
            best_d = dist;
            best = m;
        }
    }
    if (best) {
        monster_damage(g, best, dmg, ps->pl.pos, 5.0f, ps->index);
        hits++;
    }
    if (hits) {
        s->hp = fminf(s->maxhp, s->hp + 3.0f * s->ranks[SK_BLOODTHIRST] * hits);
    } else {
        /* sparks when hitting a wall */
        vec3 look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
        float d = phys_raycast(eye, look, range, game_solid, g);
        if (d < range) particles_burst(g, v3_add(eye, v3_scale(look, d - 0.05f)), v3(1.0f, 0.85f, 0.5f), 6, 2.5f, 0.03f, true);
    }
}

/* Shot direction. Gamepad players get a little aim assist: a monster within a few
 * degrees of the crosshair pulls the shot onto itself. */
static vec3 aim_dir(Game *g, PlayerSlot *ps, float yaw_off, float pitch_off) {
    vec3 eye = player_eye(&ps->pl);
    vec3 look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    vec3 d = dir_from_angles(ps->pl.yaw + yaw_off, ps->pl.pitch + pitch_off);
    if (!ps->pad || (ps->keyboard && g->mouse)) return d;
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
    float dmg = 15.0f * (1.0f + 0.3f * s->ranks[SK_STEADY_AIM]) * level_mult(s);
    for (int i = 0; i < count; i++) {
        float off = count > 1 ? ((float)i / (count - 1) - 0.5f) * 2.0f * spread_deg * PI_F / 180.0f : 0.0f;
        vec3 d = aim_dir(g, ps, off, 0.02f);
        Projectile *p = proj_new(g, ps->index, PJ_ARROW, v3_add(eye, v3_scale(d, 0.3f)), v3_scale(d, 48.0f), dmg);
        if (p && s->ranks[SK_PIERCING]) p->pierce = 3;
    }
}

static void cast_bolt(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    vec3 eye = player_eye(&ps->pl);
    vec3 d = aim_dir(g, ps, 0.0f, 0.0f);
    float dmg = 24.0f * (1.0f + 0.3f * s->ranks[SK_FOCUS]) * level_mult(s);
    Projectile *p = proj_new(g, ps->index, PJ_BOLT, v3_add(eye, v3_scale(d, 0.4f)), v3_scale(d, 24.0f), dmg);
    if (p) {
        p->explode = s->ranks[SK_FIREBALL] > 0;
        p->chain = s->ranks[SK_CHAIN] > 0;
    }
}

static void cast_nova(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    vec3 c = v3_add(ps->pl.pos, v3(0, 0.8f, 0));
    float dmg = 30.0f * (1.0f + 0.3f * s->ranks[SK_FOCUS]) * level_mult(s);
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
    switch (s->weapon) {
    case WPN_SWORD:
        if (alt) {
            if (!s->ranks[SK_WHIRLWIND] || s->mana < 12) return;
            s->mana -= 12;
            sword_strike(g, ps, true);
            s->cooldown = s->swing_len = 0.8f;
        } else {
            sword_strike(g, ps, false);
            s->cooldown = s->swing_len = 0.45f;
        }
        break;
    case WPN_BOW:
        if (alt) {
            if (!s->ranks[SK_ARROW_STORM] || s->mana < 15) return;
            s->mana -= 15;
            fire_arrows(g, ps, 9, 20.0f);
            s->cooldown = s->swing_len = 1.0f;
        } else {
            fire_arrows(g, ps, s->ranks[SK_MULTISHOT] ? 3 : 1, 6.0f);
            s->cooldown = s->swing_len = 0.55f;
        }
        break;
    case WPN_STAFF:
        if (alt) {
            float c = mana_cost(s, 25);
            if (!s->ranks[SK_NOVA] || s->mana < c) return;
            s->mana -= c;
            cast_nova(g, ps);
            s->cooldown = s->swing_len = 1.1f;
        } else {
            float c = mana_cost(s, 6);
            if (s->mana < c) return;
            s->mana -= c;
            cast_bolt(g, ps);
            s->cooldown = s->swing_len = 0.45f;
        }
        break;
    }
    s->swing = s->swing_len;
    s->swing_kind = alt ? 1 : 0;
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
        if (s->dead) continue;
        s->mana = fminf(s->maxmana, s->mana + (3.0f + 0.1f * s->level) * dt);
        if (s->since_hurt > 4.0f) s->hp = fminf(s->maxhp, s->hp + (1.5f + 2.0f * s->ranks[SK_SECOND_WIND]) * dt);
        bool mouse = ps->keyboard && (g->mouse || g->opt.demo);
        bool attack = (mouse && ps->attack_held) || ps->pad_attack;
        bool alt = (mouse && ps->alt_held) || ps->pad_alt;
        if (attack) try_attack(g, ps, false);
        else if (alt) try_attack(g, ps, true);
    }
    update_projectiles(g, dt);
    update_particles(g, dt);
}

/* ---------- drawing ---------- */

void combat_draw_world(Game *g, vec3 eye) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (!p->alive) continue;
        if (p->kind == PJ_ARROW) {
            vec3 d = v3_norm(p->vel);
            vec3 light = game_light_at(g, p->pos);
            vec3 col = v3(0.6f * light.x, 0.5f * light.y, 0.35f * light.z);
            vec3 up = fabsf(d.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
            vec3 sx = v3_scale(v3_norm(v3_cross(d, up)), 0.02f), sy = v3_scale(v3_norm(v3_cross(sx, d)), 0.02f);
            batch3d_obox(p->pos, sx, sy, v3_scale(d, 0.35f), col, false);
            batch3d_spike(v3_add(p->pos, v3_scale(d, 0.35f)), d, 0.04f, 0.1f, v3(0.5f * light.x, 0.5f * light.y, 0.55f * light.z), false);
        } else {
            float r = p->kind == PJ_NOVA ? 0.18f : 0.16f;
            batch3d_octa(p->pos, r, r, g->time * 8.0f, proj_color(p), true);
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
    vec3 eye = player_eye(&viewer->pl);
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
            if (!p->alive || p->kind == PJ_ARROW) continue;
            float d = v3_len(v3_sub(p->pos, eye));
            if ((pass == 0) != (d < 20.0f)) continue;
            sp->dl_pos[sp->ndl] = p->pos;
            sp->dl_col[sp->ndl] = v3_scale(proj_color(p), 0.9f);
            sp->dl_rad[sp->ndl] = p->kind == PJ_NOVA ? 4.0f : 7.0f;
            sp->ndl++;
        }
}

/* Weapon in view: drawn in camera space (x right, y up, -z forward). */
void combat_draw_viewmodel(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    if (s->dead || ps->pl.fly) return;
    vec3 light = game_light_at(g, player_eye(&ps->pl));
    light = v3(fminf(light.x + 0.15f, 1.3f), fminf(light.y + 0.12f, 1.3f), fminf(light.z + 0.1f, 1.3f));
    float t = s->swing_len > 0 ? 1.0f - s->swing / s->swing_len : 1.0f; /* 0 at start of an attack */
    float walk = sinf(ps->pl.bob * 2.0f) * 0.015f;
    switch (s->weapon) {
    case WPN_SWORD: {
        /* swing from upper right to lower left */
        float a = s->swing > 0 ? (s->swing_kind ? t * 2.0f * PI_F : lerpf(0.9f, -1.3f, smoothf(t))) : 0.25f;
        float tilt = s->swing > 0 ? 0.9f : 0.35f;
        vec3 base = v3(0.36f + walk, -0.42f + fabsf(walk), -0.72f);
        if (s->swing > 0 && !s->swing_kind) base.x -= 0.25f * smoothf(t);
        /* blade direction: mostly up/forward, rotated by the swing around the view axis */
        vec3 dir = v3_norm(v3(-sinf(a) * tilt, cosf(a) * 0.9f, -0.55f));
        vec3 side = v3_norm(v3_cross(dir, v3(0, 0, 1)));
        vec3 thick = v3_cross(side, dir);
        vec3 steel = v3(0.75f * light.x, 0.78f * light.y, 0.85f * light.z);
        vec3 hilt = v3(0.45f * light.x, 0.3f * light.y, 0.15f * light.z);
        batch3d_obox(v3_add(base, v3_scale(dir, 0.3f)), v3_scale(side, 0.03f), v3_scale(dir, 0.24f), v3_scale(thick, 0.01f), steel, false);
        batch3d_spike(v3_add(base, v3_scale(dir, 0.54f)), dir, 0.025f, 0.07f, steel, false);
        batch3d_obox(v3_add(base, v3_scale(dir, 0.05f)), v3_scale(side, 0.11f), v3_scale(dir, 0.02f), v3_scale(thick, 0.025f), hilt, false);
        batch3d_obox(v3_sub(base, v3_scale(dir, 0.06f)), v3_scale(side, 0.02f), v3_scale(dir, 0.09f), v3_scale(thick, 0.02f), v3_scale(hilt, 0.7f), false);
        break;
    }
    case WPN_BOW: {
        float draw = s->swing > 0 ? (t < 0.3f ? 0.0f : (t - 0.3f) / 0.7f) : 1.0f; /* re-drawing after a shot */
        vec3 c = v3(-0.12f + walk, -0.15f, -0.55f);
        vec3 wood = v3(0.5f * light.x, 0.33f * light.y, 0.18f * light.z);
        vec3 prev = v3(0, 0, 0);
        for (int i = 0; i <= 8; i++) {
            float u = (i / 8.0f - 0.5f) * 2.0f;
            vec3 p = v3_add(c, v3(0.05f * (1.0f - u * u), u * 0.32f, 0.08f * (1.0f - u * u)));
            if (i > 0) {
                vec3 mid = v3_scale(v3_add(p, prev), 0.5f), d = v3_scale(v3_sub(p, prev), 0.5f);
                batch3d_obox(mid, v3(0.018f, 0, 0), d, v3(0, 0, 0.018f), wood, false);
            }
            prev = p;
        }
        float pull = 0.12f * draw;
        vec3 nock = v3_add(c, v3(0.05f, 0, 0.08f + pull));
        vec3 string = v3(0.8f * light.x, 0.8f * light.y, 0.75f * light.z);
        vec3 top = v3_add(c, v3(0, 0.32f, 0)), bot = v3_add(c, v3(0, -0.32f, 0));
        batch3d_obox(v3_scale(v3_add(top, nock), 0.5f), v3(0.004f, 0, 0), v3_scale(v3_sub(top, nock), 0.5f), v3(0, 0, 0.004f), string, false);
        batch3d_obox(v3_scale(v3_add(bot, nock), 0.5f), v3(0.004f, 0, 0), v3_scale(v3_sub(bot, nock), 0.5f), v3(0, 0, 0.004f), string, false);
        if (draw > 0.6f) batch3d_obox(v3_add(nock, v3(0, 0, -0.3f)), v3(0.008f, 0, 0), v3(0, 0.008f, 0), v3(0, 0, 0.3f), v3(0.55f * light.x, 0.45f * light.y, 0.3f * light.z), false);
        break;
    }
    case WPN_STAFF: {
        float thrust = s->swing > 0 ? sinf(t * PI_F) * 0.12f : 0.0f;
        vec3 tip = v3(0.25f + walk, -0.1f, -0.75f - thrust);
        vec3 butt = v3(0.42f, -0.65f, -0.35f);
        vec3 d = v3_scale(v3_sub(tip, butt), 0.5f);
        batch3d_obox(v3_add(butt, d), v3(0.02f, 0, 0), d, v3(0, 0, 0.02f), v3(0.4f * light.x, 0.28f * light.y, 0.18f * light.z), false);
        float glow = 0.7f + 0.3f * sinf(g->time * 6.0f) + (s->swing > 0 ? 0.5f * (1.0f - t) : 0.0f);
        vec3 gem = s->ranks[SK_FIREBALL] ? v3(1.0f, 0.55f, 0.2f) : v3(0.55f, 0.6f, 1.0f);
        batch3d_octa(v3_add(tip, v3(0, 0.04f, 0)), 0.032f, 0.05f, g->time * 2.0f, v3_scale(gem, glow), true);
        break;
    }
    }
    (void)WEAPON_NAMES;
}

const char *combat_weapon_name(int w) { return WEAPON_NAMES[w]; }
