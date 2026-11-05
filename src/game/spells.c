#include "game/game.h"

#include "render/batch.h"

/* Spells: eight elements times eleven forms, plus utility spells.
 * The form decides the shape (bolt, fan, ring, chain, cone, orb, trap rune, orbiting
 * halo, storm, meteor, aura); the element decides what a hit does on top of damage. */

const char *ELEMENT_NAMES[EL_COUNT] = { "FIRE", "FROST", "LIGHTNING", "POISON", "ARCANE", "HOLY", "SHADOW", "EARTH" };
static const char *ELEMENT_PREFIX[EL_COUNT] = { "Flame", "Frost", "Thunder", "Venom", "Arcane", "Holy", "Shadow", "Stone" };
static const char *FORM_NAMES[SF_COUNT] = { "Bolt", "Barrage", "Nova", "Chain", "Breath", "Orb", "Rune", "Halo", "Storm", "Meteor", "Aura" };
static const char *ELEMENT_EFFECT[EL_COUNT] = {
    "sets foes on fire", "chills and slows foes", "arcs to nearby foes", "poisons for a long time",
    "pierces and hits hard", "heals you a little per hit", "steals life", "crushes, knocks back and stuns",
};

typedef struct FormDef { float damage, mana, cooldown; int tier; const char *desc; } FormDef;
static const FormDef FORMS[SF_COUNT] = {
    [SF_BOLT] = { 24, 6, 0.45f, 0, "a fast bolt" },
    [SF_BARRAGE] = { 13, 12, 0.9f, 0, "five bolts in a fan" },
    [SF_NOVA] = { 16, 18, 1.2f, 1, "a ring of bolts around you" },
    [SF_CHAIN] = { 26, 12, 0.8f, 1, "strikes the nearest foe in view, jumps to 3 more" },
    [SF_BREATH] = { 7, 3, 0.12f, 1, "a short cone; hold to keep breathing" },
    [SF_ORB] = { 10, 18, 1.5f, 2, "a slow orb that hurts all it passes" },
    [SF_RUNE] = { 55, 10, 0.6f, 2, "a trap rune that bursts when a foe comes near" },
    [SF_ORBIT] = { 14, 22, 6.0f, 2, "three orbs circle you for 8 seconds" },
    [SF_STORM] = { 22, 30, 4.0f, 3, "strikes rain on the target area" },
    [SF_METEOR] = { 90, 28, 2.5f, 3, "a huge impact at the target after a moment" },
    [SF_AURA] = { 12, 25, 7.0f, 3, "hurts everything near you for 6 seconds" },
};
static const struct { const char *name, *desc; float mana, cooldown; int tier; } UTILITY[SU_COUNT] = {
    [SU_HEAL] = { "Heal", "restores a third of your health", 25, 6.0f, 0 },
    [SU_BLINK] = { "Blink", "teleports you 10 m ahead", 15, 2.5f, 1 },
    [SU_WARD] = { "Ward", "a shield that absorbs damage for 20 s", 20, 12.0f, 1 },
    [SU_HASTE] = { "Haste", "faster moves and attacks for 10 s", 20, 15.0f, 2 },
    [SU_FLARE] = { "Flare", "a lasting light where you aim", 8, 1.0f, 0 },
    [SU_RECALL] = { "Recall", "back to your shrine", 30, 30.0f, 2 },
    [SU_THREAD] = { "Golden Thread", "a fading thread leads to your target; again: next mission", 6, 1.5f, 0 },
};

vec3 element_color(int e) {
    static const vec3 C[EL_COUNT] = {
        { 1.0f, 0.5f, 0.15f }, { 0.55f, 0.85f, 1.0f }, { 0.75f, 0.8f, 1.0f }, { 0.45f, 0.95f, 0.3f },
        { 0.85f, 0.4f, 1.0f }, { 1.0f, 0.95f, 0.6f }, { 0.55f, 0.25f, 0.75f }, { 0.75f, 0.6f, 0.4f },
    };
    return e >= 0 && e < EL_COUNT ? C[e] : v3(0.8f, 0.8f, 0.8f);
}

void spell_info(int id, SpellInfo *out) {
    memset(out, 0, sizeof *out);
    if (spell_is_attack(id)) {
        int e = spell_element(id), f = spell_form(id);
        snprintf(out->name, sizeof out->name, "%s %s", ELEMENT_PREFIX[e], FORM_NAMES[f]);
        /* a few classics get their proper names */
        if (e == EL_FIRE && f == SF_ORB) snprintf(out->name, sizeof out->name, "Fireball");
        if (e == EL_LIGHTNING && f == SF_CHAIN) snprintf(out->name, sizeof out->name, "Chain Lightning");
        if (e == EL_FROST && f == SF_STORM) snprintf(out->name, sizeof out->name, "Blizzard");
        if (e == EL_FIRE && f == SF_STORM) snprintf(out->name, sizeof out->name, "Firestorm");
        if (e == EL_FIRE && f == SF_BREATH) snprintf(out->name, sizeof out->name, "Dragon Breath");
        if (e == EL_EARTH && f == SF_METEOR) snprintf(out->name, sizeof out->name, "Rockfall");
        if (e == EL_HOLY && f == SF_NOVA) snprintf(out->name, sizeof out->name, "Radiance");
        if (e == EL_SHADOW && f == SF_AURA) snprintf(out->name, sizeof out->name, "Dread Aura");
        out->desc = FORMS[f].desc;
        out->mana = FORMS[f].mana;
        out->cooldown = FORMS[f].cooldown;
        out->damage = FORMS[f].damage;
        out->tier = FORMS[f].tier;
    } else {
        int u = id - SPELL_ATTACK_COUNT;
        snprintf(out->name, sizeof out->name, "%s", UTILITY[u].name);
        out->desc = UTILITY[u].desc;
        out->mana = UTILITY[u].mana;
        out->cooldown = UTILITY[u].cooldown;
        out->tier = UTILITY[u].tier;
    }
}

const char *spell_effect_text(int id) { return spell_is_attack(id) ? ELEMENT_EFFECT[spell_element(id)] : ""; }

int spell_learn(PlayerSlot *ps, int id) {
    Stats *s = &ps->st;
    if (s->spell_rank[id] < SPELL_MAX_RANK) s->spell_rank[id]++;
    return s->spell_rank[id];
}

void spells_give_start(Stats *s) {
    s->spell_rank[spell_attack_id(EL_FIRE, SF_BOLT)] = 1;
    s->spell_rank[SPELL_ATTACK_COUNT + SU_HEAL] = 1;
    s->spell_rank[SPELL_ATTACK_COUNT + SU_THREAD] = 1;
    s->spell = spell_attack_id(EL_FIRE, SF_BOLT);
}

void spell_next(PlayerSlot *ps, int dir) {
    Stats *s = &ps->st;
    for (int k = 1; k <= SPELL_COUNT; k++) {
        int id = ((s->spell + dir * k) % SPELL_COUNT + SPELL_COUNT) % SPELL_COUNT;
        if (s->spell_rank[id]) { s->spell = id; return; }
    }
}

/* ---------- hits ---------- */

static float spell_power(const PlayerSlot *ps, int id) {
    const Stats *s = &ps->st;
    return (1.0f + 0.3f * s->ranks[SK_FOCUS]) * (1.0f + 0.07f * (s->level - 1)) * (1.0f + 0.25f * (s->spell_rank[id] - 1)) *
           (1.0f + relic_bonus(s, RB_DAMAGE));
}

static void heal_player(Game *g, int who, float hp) {
    if (who < 0 || who >= MAX_PLAYERS || !g->players[who].active || g->players[who].st.dead) return;
    Stats *s = &g->players[who].st;
    s->hp = fminf(s->maxhp, s->hp + hp);
}

void element_hit(Game *g, Monster *m, int element, float dmg, vec3 from, float knock, int attacker) {
    if (!m->alive) return;
    switch (element) {
    case EL_FIRE:
        m->burn_dps = fmaxf(m->burn_t > 0 ? m->burn_dps : 0.0f, dmg * 0.25f);
        m->burn_t = 3.0f;
        break;
    case EL_FROST: dmg *= 0.9f; m->slow_t = 3.0f; break;
    case EL_POISON:
        dmg *= 0.6f;
        m->poison_t = 5.0f;
        m->poison_dps = fmaxf(m->poison_dps, dmg * 0.5f);
        break;
    case EL_ARCANE: dmg *= 1.15f; break;
    case EL_HOLY:
        if (m->type == MON_SKELETON || m->type == MON_WISP) dmg *= 1.5f;
        heal_player(g, attacker, dmg * 0.08f);
        break;
    case EL_SHADOW: dmg *= 1.05f; heal_player(g, attacker, dmg * 0.12f); break;
    case EL_EARTH: dmg *= 1.2f; knock *= 2.0f; break;
    default: break;
    }
    if (element != EL_NONE && element < EL_COUNT)
        particles_burst(g, v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0)), element_color(element), 4, 2.5f, 0.05f, true);
    if (element == EL_EARTH && m->alive && m->hp > dmg && (m->state == MS_CHASE || m->state == MS_WINDUP)) {
        m->state = MS_STUN;
        m->timer = 0.7f;
    }
    if (element == EL_LIGHTNING) {
        /* arc to one more foe */
        Monster *n = monster_nearest(g, m->pos, 6.0f, m);
        monster_damage(g, m, dmg * 0.95f, from, knock, attacker);
        if (n) {
            vec3 a = v3_add(m->pos, v3(0, 0.6f, 0)), b = v3_add(n->pos, v3(0, 0.6f, 0));
            for (int k = 0; k <= 6; k++) particles_burst(g, v3_lerp(a, b, k / 6.0f), v3(0.75f, 0.8f, 1.0f), 1, 0.4f, 0.04f, true);
            monster_damage(g, n, dmg * 0.5f, a, 1.0f, attacker);
        }
        return;
    }
    monster_damage(g, m, dmg, from, knock, attacker);
}

/* Damage every monster within r of c. */
static int area_hit(Game *g, vec3 c, float r, int element, float dmg, float knock, int owner) {
    int n = 0;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        vec3 mc = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
        float d = v3_len(v3_sub(mc, c));
        if (d > r + MONSTERS[m->type].hw * m->scale) continue;
        element_hit(g, m, element, dmg, c, knock, owner);
        n++;
    }
    return n;
}

void spell_projectile_hit(Game *g, Projectile *p, Monster *m) {
    element_hit(g, m, p->element, p->dmg, v3_sub(p->pos, v3_scale(p->vel, 0.05f)), p->element == EL_EARTH ? 4.0f : 2.5f, p->owner);
    flash_add(g, p->pos, element_color(p->element), 4.0f, 0.15f);
    sfx_at(g, sfx_element_impact(p->element), p->pos, p->form == SF_BREATH || p->form == SF_ORB ? 0.5f : 1.0f);
    if (p->pierce > 0) p->pierce--;
    else p->alive = false;
}

/* ---------- casting ---------- */

static Projectile *spell_proj(Game *g, int owner, int element, int form, vec3 pos, vec3 vel, float dmg, float life, float radius) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (p->alive) continue;
        memset(p, 0, sizeof *p);
        p->alive = true;
        p->kind = PJ_SPELL;
        p->element = (u8)element;
        p->form = (u8)form;
        p->pos = pos;
        p->vel = vel;
        p->dmg = dmg;
        p->life = life;
        p->radius = radius;
        p->last_hit = -1;
        p->owner = owner;
        p->pierce = element == EL_ARCANE ? 1 : 0;
        return p;
    }
    return NULL;
}

static Effect *effect_new(Game *g, int type, int element, int owner, vec3 pos, float life, float dmg, float radius) {
    int slot = -1;
    for (int i = 0; i < MAX_EFFECTS && slot < 0; i++)
        if (!g->effects[i].alive) slot = i;
    if (slot < 0) return NULL;
    Effect *e = &g->effects[slot];
    memset(e, 0, sizeof *e);
    e->alive = true;
    e->type = (u8)type;
    e->element = (u8)element;
    e->owner = owner;
    e->pos = pos;
    e->life = life;
    e->dmg = dmg;
    e->radius = radius;
    return e;
}

/* Where the player aims: the first wall within range, then down to the floor. */
static vec3 aim_point(Game *g, PlayerSlot *ps, float range, bool to_floor) {
    vec3 eye = player_eye(&ps->pl);
    vec3 d = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    float t = phys_raycast(eye, d, range, game_solid, g);
    vec3 p = v3_add(eye, v3_scale(d, fmaxf(0.0f, t - 0.3f)));
    if (!to_floor) return p;
    for (int k = 0; k < 12; k++) {
        if (game_solid(g, (int)floorf(p.x), (int)floorf(p.y - 0.05f), (int)floorf(p.z)) != 0) break;
        p.y -= 1.0f;
    }
    p.y = floorf(p.y) + 0.05f;
    return p;
}

static void cast_chain(Game *g, PlayerSlot *ps, int element, float dmg) {
    vec3 eye = player_eye(&ps->pl);
    vec3 look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    Monster *best = NULL;
    float bs = cosf(35.0f * PI_F / 180.0f);
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        vec3 c = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
        vec3 to = v3_sub(c, eye);
        float d = v3_len(to);
        if (d > 22.0f || d < 0.3f) continue;
        float cs = v3_dot(v3_scale(to, 1.0f / d), look) - d * 0.004f; /* prefer near and centred */
        if (cs > bs && phys_line_of_sight(eye, c, game_solid, g)) { bs = cs; best = m; }
    }
    vec3 from = v3_add(eye, v3_scale(look, 0.4f));
    if (!best) {
        /* a fizzle along the view */
        for (int k = 0; k < 8; k++) particles_burst(g, v3_add(from, v3_scale(look, k * 1.2f)), element_color(element), 1, 0.5f, 0.04f, true);
        return;
    }
    Monster *hit[4] = { 0 };
    for (int j = 0; j < 4 && best; j++) {
        hit[j] = best;
        vec3 to = v3_add(best->pos, v3(0, MONSTERS[best->type].h * best->scale * 0.5f, 0));
        for (int k = 0; k <= 8; k++) {
            vec3 q = v3_lerp(from, to, k / 8.0f);
            q.y += (k % 2 ? 0.12f : -0.08f);
            particles_burst(g, q, element_color(element), 1, 0.4f, 0.05f, true);
        }
        flash_add(g, to, element_color(element), 5.0f, 0.2f);
        element_hit(g, best, element, dmg, from, 2.0f, ps->index);
        dmg *= 0.8f;
        from = to;
        /* next: nearest not yet hit */
        Monster *next = NULL;
        float bd = 8.0f;
        for (int i = 0; i < MAX_MONSTERS; i++) {
            Monster *m = &g->mons[i];
            bool seen = false;
            for (int q = 0; q <= j; q++) seen |= hit[q] == m;
            if (!m->alive || seen) continue;
            float d = v3_len(v3_sub(m->pos, best->pos));
            if (d < bd && phys_line_of_sight(from, v3_add(m->pos, v3(0, 0.5f, 0)), game_solid, g)) { bd = d; next = m; }
        }
        best = next;
    }
}

static bool cast_utility_do(Game *g, PlayerSlot *ps, int u, int rank);

static bool cast_utility(Game *g, PlayerSlot *ps, int u, int rank) {
    static const int S[SU_COUNT] = { SFX_SPELL_HEAL, SFX_SPELL_BLINK, SFX_SPELL_WARD, SFX_SPELL_HASTE,
                                     SFX_SPELL_FLARE, SFX_SPELL_RECALL, SFX_SPELL_THREAD };
    if (!cast_utility_do(g, ps, u, rank)) return false;
    if (u >= 0 && u < SU_COUNT) sfx_player(g, ps, S[u], 1.0f);
    return true;
}

static bool cast_utility_do(Game *g, PlayerSlot *ps, int u, int rank) {
    Stats *s = &ps->st;
    vec3 eye = player_eye(&ps->pl);
    switch (u) {
    case SU_HEAL:
        if (s->hp >= s->maxhp) return false;
        s->hp = fminf(s->maxhp, s->hp + s->maxhp * (0.3f + 0.05f * rank));
        particles_burst(g, eye, v3(0.5f, 1.0f, 0.5f), 16, 2.0f, 0.05f, true);
        flash_add(g, eye, v3(0.4f, 1.0f, 0.5f), 6.0f, 0.4f);
        return true;
    case SU_BLINK: {
        vec3 d = dir_from_angles(ps->pl.yaw, 0.0f);
        vec3 best = ps->pl.pos;
        float h = player_height(ps->pl.stance);
        for (float t = 0.5f; t <= 10.0f + 2.0f * rank; t += 0.5f) {
            vec3 p = v3_add(ps->pl.pos, v3_scale(d, t));
            if (!phys_box_free(p, PLAYER_HALF_W, h, game_solid, g)) {
                vec3 up = v3_add(p, v3(0, 1.0f, 0));
                if (!phys_box_free(up, PLAYER_HALF_W, h, game_solid, g)) break;
                p = up;
            }
            best = p;
        }
        particles_burst(g, eye, v3(0.7f, 0.5f, 1.0f), 14, 3.0f, 0.05f, true);
        ps->pl.pos = best;
        ps->pl.vel = v3(0, 0, 0);
        particles_burst(g, player_eye(&ps->pl), v3(0.7f, 0.5f, 1.0f), 14, 3.0f, 0.05f, true);
        return true;
    }
    case SU_WARD:
        s->ward = 40.0f + 15.0f * rank + 4.0f * s->level;
        s->ward_t = 20.0f;
        flash_add(g, eye, v3(0.5f, 0.7f, 1.0f), 6.0f, 0.4f);
        return true;
    case SU_HASTE:
        s->haste_t = 10.0f + 2.0f * rank;
        particles_burst(g, eye, v3(1.0f, 0.9f, 0.4f), 12, 3.0f, 0.04f, true);
        return true;
    case SU_FLARE: {
        Effect *e = effect_new(g, EF_FLARE, EL_HOLY, ps->index, aim_point(g, ps, 30.0f, false), 40.0f + 10.0f * rank, 0, 12.0f);
        return e != NULL;
    }
    case SU_THREAD:
        if (!story_cast_thread(g, ps)) return false;
        particles_burst(g, eye, v3(1.0f, 0.8f, 0.35f), 12, 2.0f, 0.04f, true);
        return true;
    case SU_RECALL:
        particles_burst(g, eye, v3(0.9f, 0.8f, 0.5f), 20, 3.0f, 0.05f, true);
        ps->pl.pos = s->respawn;
        ps->pl.yaw = s->respawn_yaw;
        ps->pl.vel = v3(0, 0, 0);
        flash_add(g, player_eye(&ps->pl), v3(1.0f, 0.85f, 0.5f), 8.0f, 0.5f);
        return true;
    }
    return false;
}

bool spell_cast(Game *g, PlayerSlot *ps) { return spell_cast_id(g, ps, ps->st.spell); }

bool spell_cast_id(Game *g, PlayerSlot *ps, int id) {
    Stats *s = &ps->st;
    if (s->dead || s->spell_cd > 0 || id < 0 || id >= SPELL_COUNT || !s->spell_rank[id]) return false;
    SpellInfo si;
    spell_info(id, &si);
    float cost = si.mana * (1.0f - 0.15f * s->ranks[SK_FOCUS]);
    if (s->mana < cost) {
        if (ps->banner_time <= 0) game_banner(ps, "NOT ENOUGH MANA");
        sfx_player(g, ps, SFX_SPELL_FIZZLE, 1.0f);
        s->spell_cd = 0.3f;
        return false;
    }
    int rank = s->spell_rank[id];
    float haste = s->haste_t > 0 ? 0.7f : 1.0f;
    if (!spell_is_attack(id)) {
        if (!cast_utility(g, ps, id - SPELL_ATTACK_COUNT, rank)) return false;
        s->mana -= cost;
        s->spell_cd = si.cooldown * haste;
        return true;
    }
    int e = spell_element(id), f = spell_form(id);
    float dmg = si.damage * spell_power(ps, id);
    vec3 eye = player_eye(&ps->pl);
    vec3 look = dir_from_angles(ps->pl.yaw, ps->pl.pitch);
    vec3 start = v3_add(eye, v3_scale(look, 0.4f));
    float speed = e == EL_ARCANE || e == EL_LIGHTNING ? 30.0f : (e == EL_EARTH ? 18.0f : 24.0f);
    vec3 col = element_color(e);
    if (f == SF_BREATH) sfx_player(g, ps, SFX_BREATH, 0.8f);
    else sfx_player(g, ps, sfx_element_cast(e), 1.0f);
    if (f == SF_ORBIT) sfx_player(g, ps, SFX_ORBIT_HUM, 0.8f);
    if (f == SF_AURA) sfx_player(g, ps, SFX_AURA_PULSE, 0.9f);
    switch (f) {
    case SF_BOLT: spell_proj(g, ps->index, e, f, start, v3_scale(look, speed), dmg, 3.0f, 0.16f); break;
    case SF_BARRAGE:
        for (int i = 0; i < 5; i++) {
            vec3 d = dir_from_angles(ps->pl.yaw + (i - 2) * 0.12f, ps->pl.pitch);
            spell_proj(g, ps->index, e, f, start, v3_scale(d, speed), dmg, 2.5f, 0.12f);
        }
        break;
    case SF_NOVA: {
        vec3 c = v3_add(ps->pl.pos, v3(0, 0.8f, 0));
        for (int i = 0; i < 16; i++) {
            float a = i * 2.0f * PI_F / 16.0f;
            vec3 d = v3(cosf(a), 0, sinf(a));
            Projectile *p = spell_proj(g, ps->index, e, f, v3_add(c, v3_scale(d, 0.5f)), v3_scale(d, 14.0f), dmg, 0.8f, 0.15f);
            if (p) p->pierce = 2;
        }
        flash_add(g, c, col, 10.0f, 0.4f);
        break;
    }
    case SF_CHAIN: cast_chain(g, ps, e, dmg); break;
    case SF_BREATH:
        for (int i = 0; i < 4; i++) {
            float jy = ((float)((g->mons_anim_seed + i * 37 + (int)(g->time * 977)) % 100) / 100.0f - 0.5f) * 0.6f;
            float jp = ((float)((g->mons_anim_seed + i * 53 + (int)(g->time * 613)) % 100) / 100.0f - 0.5f) * 0.3f;
            vec3 d = dir_from_angles(ps->pl.yaw + jy, ps->pl.pitch + jp);
            Projectile *p = spell_proj(g, ps->index, e, f, start, v3_scale(d, 13.0f), dmg, 0.45f, 0.1f);
            if (p) p->pierce = 3;
        }
        break;
    case SF_ORB: {
        Projectile *p = spell_proj(g, ps->index, e, f, start, v3_scale(look, 8.0f), dmg, 3.5f, 0.35f);
        if (p) p->pierce = 999;
        break;
    }
    case SF_RUNE: {
        int mine = 0, oldest = -1;
        for (int i = 0; i < MAX_EFFECTS; i++)
            if (g->effects[i].alive && g->effects[i].type == EF_RUNE && g->effects[i].owner == ps->index) {
                mine++;
                if (oldest < 0 || g->effects[i].t > g->effects[oldest].t) oldest = i;
            }
        if (mine >= 4 && oldest >= 0) g->effects[oldest].alive = false;
        Effect *rn = effect_new(g, EF_RUNE, e, ps->index, aim_point(g, ps, 8.0f, true), 40.0f, dmg, 3.5f);
        if (rn) sfx_at(g, SFX_RUNE_PLACE, rn->pos, 1.0f);
        break;
    }
    case SF_ORBIT: effect_new(g, EF_ORBIT, e, ps->index, ps->pl.pos, 8.0f, dmg, 1.7f); break;
    case SF_STORM: effect_new(g, EF_STORM, e, ps->index, aim_point(g, ps, 30.0f, true), 4.0f, dmg, 4.5f); break;
    case SF_METEOR: {
        Effect *mt = effect_new(g, EF_METEOR, e, ps->index, aim_point(g, ps, 35.0f, true), 1.0f, dmg, 4.5f);
        if (mt) sfx_at(g, SFX_METEOR_FALL, v3_add(mt->pos, v3(0, 6.0f, 0)), 1.0f);
        break;
    }
    case SF_AURA: effect_new(g, EF_AURA, e, ps->index, ps->pl.pos, 6.0f, dmg, 3.8f); break;
    }
    s->mana -= cost;
    s->spell_cd = si.cooldown * haste;
    return true;
}

/* ---------- lasting effects ---------- */

static float frand(u64 *st) {
    *st = mix64(*st + 0x9E3779B97F4A7C15ULL);
    return (float)(*st >> 40) / (float)(1ULL << 24);
}

void spells_update(Game *g, float dt) {
    static u64 rs = 12345;
    /* orbs hurt what they pass */
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g->proj[i];
        if (!p->alive || p->kind != PJ_SPELL || p->form != SF_ORB) continue;
        p->tick -= dt;
        if (p->tick > 0) continue;
        p->tick = 0.3f;
        area_hit(g, p->pos, 1.6f, p->element, p->dmg, 1.0f, p->owner);
    }
    for (int i = 0; i < MAX_EFFECTS; i++) {
        Effect *e = &g->effects[i];
        if (!e->alive) continue;
        e->t += dt;
        e->tick -= dt;
        PlayerSlot *owner = e->owner >= 0 && e->owner < MAX_PLAYERS && g->players[e->owner].active ? &g->players[e->owner] : NULL;
        vec3 col = element_color(e->element);
        switch (e->type) {
        case EF_STORM:
            if (e->tick <= 0) {
                e->tick = 0.28f;
                float a = frand(&rs) * 2.0f * PI_F, r = sqrtf(frand(&rs)) * e->radius;
                vec3 p = v3_add(e->pos, v3(cosf(a) * r, 0.3f, sinf(a) * r));
                for (int k = 0; k < 6; k++) particles_burst(g, v3_add(p, v3(0, k * 1.2f, 0)), col, 1, 0.5f, 0.06f, true);
                flash_add(g, p, col, 5.0f, 0.15f);
                sfx_at(g, SFX_STORM_STRIKE, p, 0.7f);
                area_hit(g, p, 1.8f, e->element, e->dmg, 2.0f, e->owner);
            }
            break;
        case EF_RUNE: {
            bool trig = false;
            for (int k = 0; k < MAX_MONSTERS && !trig; k++) {
                Monster *m = &g->mons[k];
                trig = m->alive && v3_len(v3_sub(m->pos, e->pos)) < 2.2f;
            }
            if (trig) {
                vec3 c = v3_add(e->pos, v3(0, 0.6f, 0));
                flash_add(g, c, col, 10.0f, 0.35f);
                sfx_at(g, SFX_RUNE_TRIGGER, c, 1.0f);
                sfx_at(g, sfx_element_impact(e->element), c, 0.8f);
                particles_burst(g, c, col, 26, 6.0f, 0.08f, true);
                area_hit(g, c, e->radius, e->element, e->dmg, 5.0f, e->owner);
                e->alive = false;
            }
            break;
        }
        case EF_METEOR:
            if (e->t >= e->life) {
                vec3 c = v3_add(e->pos, v3(0, 0.5f, 0));
                flash_add(g, c, col, 16.0f, 0.5f);
                sfx_at(g, SFX_METEOR_IMPACT, c, 1.0f);
                sfx_at(g, sfx_element_impact(e->element), c, 0.8f);
                particles_burst(g, c, col, 40, 8.0f, 0.1f, true);
                particles_burst(g, c, v3(0.4f, 0.35f, 0.3f), 20, 6.0f, 0.1f, false);
                area_hit(g, c, e->radius, e->element, e->dmg, 7.0f, e->owner);
                e->alive = false;
            }
            break;
        case EF_ORBIT:
            if (!owner || owner->st.dead) { e->alive = false; break; }
            e->pos = owner->pl.pos;
            if (e->tick <= 0) {
                e->tick = 0.25f;
                for (int k = 0; k < 3; k++) {
                    float a = e->t * 3.2f + k * 2.0f * PI_F / 3.0f;
                    vec3 o = v3_add(e->pos, v3(cosf(a) * e->radius, 1.0f, sinf(a) * e->radius));
                    area_hit(g, o, 0.8f, e->element, e->dmg, 2.0f, e->owner);
                }
            }
            break;
        case EF_AURA:
            if (!owner || owner->st.dead) { e->alive = false; break; }
            e->pos = owner->pl.pos;
            if (e->tick <= 0) {
                e->tick = 0.5f;
                if (owner && fmodf(e->t, 1.5f) < 0.5f) sfx_player(g, owner, SFX_AURA_PULSE, 0.3f);
                area_hit(g, v3_add(e->pos, v3(0, 0.8f, 0)), e->radius, e->element, e->dmg, 1.0f, e->owner);
                for (int k = 0; k < 10; k++) {
                    float a = k * 2.0f * PI_F / 10.0f + e->t;
                    particles_burst(g, v3_add(e->pos, v3(cosf(a) * e->radius, 0.2f, sinf(a) * e->radius)), col, 1, 0.6f, 0.05f, true);
                }
            }
            break;
        default: break;
        }
        if (e->type != EF_METEOR && e->t >= e->life) e->alive = false;
    }
}

void spells_draw_world(Game *g) {
    for (int i = 0; i < MAX_EFFECTS; i++) {
        Effect *e = &g->effects[i];
        if (!e->alive) continue;
        vec3 col = element_color(e->element);
        switch (e->type) {
        case EF_RUNE: {
            float pulse = 0.6f + 0.4f * sinf(e->t * 4.0f);
            batch3d_box(v3_add(e->pos, v3(0, 0.02f, 0)), v3(0.5f, 0.02f, 0.5f), e->t * 0.5f, v3_scale(col, pulse), true);
            batch3d_box(v3_add(e->pos, v3(0, 0.03f, 0)), v3(0.25f, 0.02f, 0.25f), -e->t * 0.8f + 0.78f, v3_scale(col, 0.4f), true);
            break;
        }
        case EF_METEOR: {
            float k = 1.0f - e->t / e->life;
            vec3 p = v3_add(e->pos, v3(k * 6.0f, 0.5f + k * 22.0f, k * 3.0f));
            batch3d_octa(p, 0.6f, 0.6f, e->t * 6.0f, col, true);
            batch3d_box(v3_add(e->pos, v3(0, 0.03f, 0)), v3(e->radius * (1.0f - k), 0.01f, e->radius * (1.0f - k)), 0, v3_scale(col, 0.5f), true);
            break;
        }
        case EF_ORBIT:
            for (int k = 0; k < 3; k++) {
                float a = e->t * 3.2f + k * 2.0f * PI_F / 3.0f;
                vec3 o = v3_add(e->pos, v3(cosf(a) * e->radius, 1.0f, sinf(a) * e->radius));
                batch3d_octa(o, 0.16f, 0.2f, e->t * 5.0f, col, true);
            }
            break;
        case EF_STORM:
            /* a ring of sparks circling above the area */
            for (int k = 0; k < 10; k++) {
                float a = e->t * 1.5f + k * 2.0f * PI_F / 10.0f, r = e->radius * (0.4f + 0.6f * ((k * 7) % 10) / 10.0f);
                batch3d_octa(v3_add(e->pos, v3(cosf(a) * r, 5.0f + 0.4f * sinf(e->t * 3.0f + k), sinf(a) * r)), 0.08f, 0.1f, a, col, true);
            }
            break;
        case EF_FLARE:
            batch3d_octa(e->pos, 0.15f, 0.2f, e->t * 2.0f, v3(1.0f, 0.95f, 0.75f), true);
            break;
        default: break;
        }
    }
}

void spell_collect_lights(Game *g, vec3 eye, SceneParams *sp) {
    for (int i = 0; i < MAX_EFFECTS && sp->ndl < MAX_DLIGHTS; i++) {
        Effect *e = &g->effects[i];
        if (!e->alive || v3_len(v3_sub(e->pos, eye)) > 50.0f) continue;
        if (e->type == EF_FLARE) {
            sp->dl_pos[sp->ndl] = e->pos;
            sp->dl_col[sp->ndl] = v3(1.2f, 1.1f, 0.85f);
            sp->dl_rad[sp->ndl] = 14.0f;
            sp->ndl++;
        } else if (e->type == EF_AURA || e->type == EF_METEOR) {
            sp->dl_pos[sp->ndl] = v3_add(e->pos, v3(0, 1.0f, 0));
            sp->dl_col[sp->ndl] = element_color(e->element);
            sp->dl_rad[sp->ndl] = 7.0f;
            sp->ndl++;
        }
    }
}
