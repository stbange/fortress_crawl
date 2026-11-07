#include "game/game.h"

#include "render/batch.h"

/* What monsters look like: 20 kinds built from boxes and spikes. The colours come from
 * where they live (monster_palette in monsters.c): the body, the armour, bone or horn
 * (color2) and the glow of eyes and markings. Elites add spines and burning eyes, bosses a
 * crown, glowing bands and circling runes. */

static vec3 lit(vec3 c, vec3 l) { return v3(c.x * l.x, c.y * l.y, c.z * l.z); }

/* A box from a to b, r thick (legs, arms, staves). */
static void limb(vec3 a, vec3 b, float r, vec3 col, bool em) {
    vec3 d = v3_scale(v3_sub(b, a), 0.5f);
    vec3 n = v3_norm(d);
    vec3 up = fabsf(n.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    vec3 sx = v3_scale(v3_norm(v3_cross(n, up)), r), sy = v3_scale(v3_norm(v3_cross(sx, n)), r);
    batch3d_obox(v3_add(a, d), d, sx, sy, col, em);
}

typedef struct Body {
    vec3 p, f, r;   /* feet, forward, right */
    float s, y;     /* scale, yaw */
} Body;

/* A point fw forward, rt right and up above the feet, in monster units. */
static vec3 at(const Body *b, float fw, float rt, float up) {
    return v3_add(b->p, v3_add(v3_scale(b->f, fw * b->s), v3_add(v3_scale(b->r, rt * b->s), v3(0, up * b->s, 0))));
}
/* A box with half sizes along forward, right and up. */
static void bx(const Body *b, vec3 c, float hf, float hr, float hu, vec3 col, bool em) {
    batch3d_obox(c, v3_scale(b->f, hf * b->s), v3_scale(b->r, hr * b->s), v3(0, hu * b->s, 0), col, em);
}

static vec3 monster_centre(const Monster *m) { return v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0)); }

/* Elites: spines down the back. Bosses: a crown, glowing bands and two circling runes. */
static void grade_marks(const Monster *m, const Body *b, vec3 col2, vec3 glow) {
    const MonsterDef *d = &MONSTERS[m->type];
    float h = d->h, hw = d->hw;
    if (m->grade >= GRADE_ELITE && m->type != MON_MIMIC) {
        /* armour on the shoulders, spines down the back */
        for (int k = -1; k <= 1; k += 2) bx(b, at(b, 0, hw * 0.95f * k, h * 0.78f), hw * 0.35f, hw * 0.3f, h * 0.06f, col2, false);
        for (int k = 0; k < 3; k++) {
            vec3 base = at(b, (-0.1f - k * 0.18f) * hw * 2.0f, 0, h * (0.82f - k * 0.1f));
            batch3d_spike(base, v3_add(v3(0, 1, 0), v3_scale(b->f, -0.6f)), 0.06f * b->s, (0.3f - k * 0.06f) * b->s, col2, false);
        }
    }
    if (m->grade == GRADE_BOSS) {
        vec3 top = at(b, 0, 0, h + 0.06f);
        vec3 gold = v3_lerp(col2, v3(1.0f, 0.8f, 0.35f), 0.5f);
        for (int k = 0; k < 5; k++) {
            float a = k * 2.0f * PI_F / 5.0f + b->y;
            vec3 q = v3_add(top, v3(cosf(a) * 0.14f * b->s, 0, sinf(a) * 0.14f * b->s));
            batch3d_spike(q, v3(0, 1, 0), 0.03f * b->s, 0.16f * b->s, gold, false);
        }
        batch3d_octa(v3_add(top, v3(0, 0.08f * b->s, 0)), 0.035f * b->s, 0.05f * b->s, m->anim, glow, true);
        /* a burning sign on its front */
        vec3 em = at(b, hw * 1.05f, 0, h * 0.62f);
        bx(b, em, 0.015f, 0.07f, 0.07f, glow, true);
        bx(b, em, 0.02f, 0.025f, 0.11f, v3_scale(glow, 0.8f), true);
        for (int k = 0; k < 2; k++) {
            float a = m->anim * 1.2f + k * PI_F;
            vec3 q = v3_add(m->pos, v3(cosf(a) * (hw * b->s + 0.5f), h * b->s * 0.55f, sinf(a) * (hw * b->s + 0.5f)));
            batch3d_octa(q, 0.06f, 0.09f, m->anim * 3.0f, glow, true);
        }
    }
}

void monsters_draw(Game *g, vec3 eye) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive) continue;
        if (v3_len(v3_sub(m->pos, eye)) > 90.0f) continue;
        /* utility spells: Reveal the Hidden marks what hides; charmed and afraid ones show it */
        float top_y = MONSTERS[m->type].h * m->scale + 0.45f + 0.08f * sinf(g->time * 4.0f + i);
        bool reveal = g->draw_viewer && g->draw_viewer->st.reveal_t > 0;
        if (reveal && (m->hidden || (m->type == MON_MIMIC && !m->awake))) {
            vec3 q = v3_add(m->pos, v3(0, m->hidden ? 0.6f : top_y, 0));
            batch3d_octa(q, 0.16f, 0.22f, g->time * 2.0f, v3(1.4f, 0.25f, 0.2f), true);
            batch3d_box(v3_add(m->pos, v3(0, 0.04f, 0)), v3(MONSTERS[m->type].hw * m->scale + 0.2f, 0.02f, MONSTERS[m->type].hw * m->scale + 0.2f),
                        g->time, v3(1.0f, 0.2f, 0.15f), true);
        }
        if (m->charm_t > 0) batch3d_octa(v3_add(m->pos, v3(0, top_y, 0)), 0.1f, 0.14f, g->time * 3.0f, v3(1.4f, 0.5f, 0.9f), true);
        else if (m->fear_t > 0) batch3d_octa(v3_add(m->pos, v3(0, top_y, 0)), 0.08f, 0.16f, g->time * 9.0f, v3(0.9f, 0.9f, 1.3f), true);
        if (m->hidden && m->type == MON_WRAITH) continue; /* vanished */
        const MonsterDef *d = &MONSTERS[m->type];
        float s = m->scale;
        vec3 light = game_light_at(g, monster_centre(m));
        /* a faint glow keeps silhouettes readable in pitch darkness */
        light = v3(fmaxf(light.x, 0.16f), fmaxf(light.y, 0.14f), fmaxf(light.z, 0.14f));
        vec3 base = m->flash > 0 ? v3(1.6f, 1.6f, 1.6f) : m->color;
        vec3 col = lit(base, light);
        vec3 dark = v3_scale(col, 0.6f);
        vec3 col2 = lit(m->flash > 0 ? v3(1.6f, 1.6f, 1.6f) : m->color2, light);
        vec3 glow = m->glow;
        vec3 eyes = m->grade >= GRADE_ELITE ? glow : v3(1.0f, 0.85f, 0.4f);
        if (m->state == MS_WINDUP || m->state == MS_CHARGE) eyes = v3(1.0f, 0.15f, 0.1f);
        float y = m->yaw;
        vec3 f = v3(cosf(y), 0, sinf(y)), r = v3(-sinf(y), 0, cosf(y));
        vec3 p = m->pos;
        float bob = sinf(m->anim * 6.0f) * 0.04f;
        float wp = m->state == MS_WINDUP ? clampf(1.0f - m->timer / fmaxf(0.05f, d->windup), 0.0f, 1.0f) : 0.0f; /* raising an attack */
        Body B = { p, f, r, s, y };
        Body *b = &B;
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
                batch3d_spike(base_h, v3_add(f, v3(0, 0.6f, 0)), 0.06f * s, 0.45f * s, col2, false);
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
        case MON_SKELETON: {
            /* thin bones, a skull and a bow */
            float sw = sinf(m->anim * 5.0f) * 0.25f;
            for (int k = -1; k <= 1; k += 2) {
                vec3 leg = v3_add(v3_add(p, v3_scale(r, 0.1f * k * s)), v3_add(v3(0, 0.42f * s, 0), v3_scale(f, sw * k * 0.3f)));
                batch3d_box(leg, v3(0.05f * s, 0.42f * s, 0.05f * s), y, col, false);
            }
            vec3 torso = v3_add(p, v3(0, 1.1f * s + bob, 0));
            batch3d_box(torso, v3(0.12f * s, 0.26f * s, 0.2f * s), y, dark, false);
            for (int k = -1; k <= 1; k += 2) batch3d_box(v3_add(torso, v3(0, 0.08f * k * s, 0)), v3(0.13f * s, 0.02f * s, 0.21f * s), y, col, false);
            vec3 head = v3_add(torso, v3(0, 0.42f * s, 0));
            batch3d_box(head, v3(0.13f * s, 0.14f * s, 0.12f * s), y, col, false);
            for (int k = -1; k <= 1; k += 2)
                batch3d_box(v3_add(v3_add(head, v3_scale(f, 0.13f * s)), v3_scale(r, 0.05f * k * s)), v3(0.025f * s, 0.025f * s, 0.025f * s), y, eyes, true);
            vec3 bow = v3_add(torso, v3_add(v3_scale(f, 0.35f * s), v3_scale(r, 0.15f * s)));
            batch3d_box(bow, v3(0.02f * s, 0.38f * s, 0.02f * s), y, v3(0.45f * light.x, 0.3f * light.y, 0.15f * light.z), false);
            break;
        }
        case MON_SLIME: {
            float squash = 1.0f + 0.15f * sinf(m->anim * 4.0f);
            vec3 body = v3_add(p, v3(0, 0.3f * s / squash, 0));
            batch3d_box(body, v3(0.42f * s * squash, 0.3f * s / squash, 0.42f * s * squash), y, col, false);
            batch3d_box(v3_add(body, v3(0, 0.05f * s, 0)), v3(0.2f * s, 0.12f * s, 0.2f * s), y, v3_scale(col, 1.3f), true);
            for (int k = -1; k <= 1; k += 2)
                batch3d_box(v3_add(v3_add(body, v3_scale(f, 0.43f * s)), v3_add(v3_scale(r, 0.12f * k * s), v3(0, 0.1f * s, 0))),
                            v3(0.04f * s, 0.04f * s, 0.04f * s), y, eyes, true);
            break;
        }
        case MON_BAT: {
            float flap = sinf(m->anim * 18.0f);
            vec3 body = v3_add(p, v3(0, 0.2f * s, 0));
            batch3d_box(body, v3(0.12f * s, 0.1f * s, 0.1f * s), y, col, false);
            for (int k = -1; k <= 1; k += 2) {
                vec3 wing = v3_add(body, v3_add(v3_scale(r, 0.28f * k * s), v3(0, flap * 0.12f * s, 0)));
                batch3d_obox(wing, v3_scale(r, 0.2f * s), v3(0, 0.02f * s, 0), v3_scale(f, 0.12f * s), dark, false);
                batch3d_box(v3_add(v3_add(body, v3_scale(f, 0.11f * s)), v3_scale(r, 0.04f * k * s)), v3(0.02f * s, 0.02f * s, 0.02f * s), y, eyes, true);
            }
            break;
        }
        case MON_SPIDER: {
            vec3 up = v3(0, bob, 0);
            bx(b, v3_add(at(b, -0.22f, 0, 0.4f), up), 0.26f, 0.22f, 0.2f, col, false);           /* abdomen */
            bx(b, v3_add(at(b, -0.22f, 0, 0.6f), up), 0.09f, 0.03f, 0.012f, col2, false);       /* its mark */
            bx(b, v3_add(at(b, 0.12f, 0, 0.32f), up), 0.14f, 0.12f, 0.1f, dark, false);         /* head */
            for (int k = -1; k <= 1; k += 2) {
                batch3d_spike(at(b, 0.24f, 0.05f * k, 0.26f), v3_sub(f, v3(0, 0.8f, 0)), 0.02f * s, 0.1f * s, col2, false);
                for (int e = 0; e < 2; e++) bx(b, at(b, 0.26f, (0.04f + 0.05f * e) * k, 0.38f + 0.03f * e), 0.02f, 0.02f, 0.02f, eyes, true);
                for (int j = 0; j < 4; j++) {
                    float ph = sinf(m->anim * 10.0f + j * 1.7f + k) * 0.06f;
                    vec3 hip = at(b, 0.12f - j * 0.07f, 0.1f * k, 0.33f);
                    vec3 knee = at(b, 0.18f - j * 0.12f + ph, 0.36f * k, 0.58f);
                    vec3 foot = at(b, 0.26f - j * 0.17f + ph * 1.5f, 0.62f * k, 0.0f);
                    limb(hip, knee, 0.025f * s, dark, false);
                    limb(knee, foot, 0.02f * s, dark, false);
                }
            }
            break;
        }
        case MON_WRAITH: {
            float hv = 0.15f + sinf(m->anim * 2.0f) * 0.08f;
            bx(b, at(b, 0, 0, 0.6f + hv), 0.2f, 0.24f, 0.36f, col, false);
            bx(b, at(b, 0, 0, 1.1f + hv), 0.16f, 0.2f, 0.2f, col, false);
            bx(b, at(b, 0.02f, 0, 1.42f + hv), 0.14f, 0.14f, 0.14f, dark, false);   /* hood */
            bx(b, at(b, 0.13f, 0, 1.4f + hv), 0.02f, 0.09f, 0.08f, v3(0.02f, 0.02f, 0.03f), false);
            for (int k = -1; k <= 1; k += 2) {
                bx(b, at(b, 0.155f, 0.045f * k, 1.42f + hv), 0.01f, 0.02f, 0.02f, glow, true);
                float reach = m->state == MS_WINDUP ? 0.3f : 0.0f;
                vec3 sh = at(b, 0.05f, 0.2f * k, 1.2f + hv), hand = at(b, 0.45f + reach, 0.2f * k, 1.0f + hv + reach);
                limb(sh, hand, 0.04f * s, col, false);
                batch3d_spike(hand, f, 0.03f * s, 0.12f * s, col2, false);
            }
            for (int k = 0; k < 5; k++) { /* tatters */
                float a = k * 2.0f * PI_F / 5.0f, sw = sinf(m->anim * 3.0f + k) * 0.05f;
                bx(b, at(b, cosf(a) * 0.15f + sw, sinf(a) * 0.18f, 0.18f + hv), 0.035f, 0.05f, 0.14f, dark, false);
            }
            batch3d_octa(at(b, 0, 0, 0.85f + hv), 0.08f * s, 0.12f * s, m->anim, v3_scale(glow, 0.7f), true);
            break;
        }
        case MON_GOLEM: {
            float sw = sinf(m->anim * 2.0f) * 0.3f;
            for (int k = -1; k <= 1; k += 2) bx(b, at(b, sw * 0.2f * k, 0.25f * k, 0.42f), 0.18f, 0.16f, 0.42f, dark, false);
            bx(b, at(b, 0, 0, 1.35f + bob), 0.32f, 0.5f, 0.5f, col, false);
            bx(b, at(b, 0.33f, 0, 1.4f), 0.02f, 0.3f, 0.3f, col2, false);
            bx(b, at(b, 0.36f, 0, 1.45f), 0.02f, 0.08f, 0.08f, glow, true);           /* the rune that drives it */
            bx(b, at(b, 0, 0.51f, 1.3f), 0.25f, 0.005f, 0.02f, v3_scale(glow, 0.6f), true);
            bx(b, at(b, 0.08f, 0, 2.0f), 0.18f, 0.18f, 0.15f, col, false);
            bx(b, at(b, 0.27f, 0, 2.02f), 0.01f, 0.12f, 0.025f, eyes, true);
            for (int k = -1; k <= 1; k += 2) {
                batch3d_spike(at(b, 0, 0.5f * k, 1.8f), v3(0.2f * r.x * k, 1, 0.2f * r.z * k), 0.08f * s, 0.3f * s, col2, false);
                float lift = wp;
                bx(b, at(b, 0.05f + lift * 0.3f, 0.68f * k, 1.15f + lift * 0.7f), 0.17f, 0.17f, 0.48f, col, false);
                bx(b, at(b, 0.1f + lift * 0.4f, 0.7f * k, 0.62f + lift * 1.2f), 0.2f, 0.2f, 0.18f, dark, false);
            }
            break;
        }
        case MON_SHAMAN: {
            bool casting = m->cooldown > d->attack_time - 0.5f;
            bx(b, at(b, 0, 0, 0.55f), 0.2f, 0.22f, 0.5f, col, false);                  /* robe */
            bx(b, at(b, 0.02f, 0, 1.1f + bob), 0.15f, 0.2f, 0.2f, col, false);
            bx(b, at(b, 0.12f, 0, 1.38f + bob), 0.13f, 0.12f, 0.13f, col2, false);      /* mask */
            for (int k = -1; k <= 1; k += 2) {
                bx(b, at(b, 0.255f, 0.05f * k, 1.41f + bob), 0.01f, 0.025f, 0.02f, eyes, true);
                batch3d_spike(at(b, 0.05f, 0.09f * k, 1.5f + bob), v3_add(v3(0, 1, 0), v3_add(v3_scale(r, 0.4f * k), v3_scale(f, -0.4f))),
                              0.025f * s, 0.25f * s, k > 0 ? col2 : glow, k < 0);
            }
            vec3 top = at(b, 0.3f, 0.3f, 1.7f + (casting ? 0.25f : 0.0f));
            limb(at(b, 0.25f, 0.3f, 0.0f), top, 0.025f * s, dark, false);
            batch3d_octa(top, (casting ? 0.11f : 0.07f) * s, (casting ? 0.14f : 0.09f) * s, m->anim * 2.0f, glow, true);
            break;
        }
        case MON_BLOATER: {
            float swell = m->state == MS_WINDUP ? 1.0f + 0.35f * wp : 1.0f + 0.04f * sinf(m->anim * 3.0f);
            for (int k = -1; k <= 1; k += 2) bx(b, at(b, 0, 0.25f * k, 0.12f), 0.1f, 0.1f, 0.12f, dark, false);
            bx(b, at(b, 0.05f, 0, 0.62f), 0.45f * swell, 0.48f * swell, 0.42f * swell, col, false);
            bx(b, at(b, 0.28f, 0, 1.08f), 0.12f, 0.13f, 0.11f, dark, false);
            for (int k = -1; k <= 1; k += 2) bx(b, at(b, 0.4f, 0.05f * k, 1.1f), 0.015f, 0.02f, 0.02f, eyes, true);
            static const float PU[6][3] = { { 0.5f, 0.2f, 0.7f }, { 0.48f, -0.25f, 0.5f }, { 0.3f, 0.45f, 0.85f },
                                            { 0.2f, -0.46f, 0.75f }, { -0.1f, 0.48f, 0.45f }, { 0.45f, 0.0f, 0.95f } };
            for (int k = 0; k < 6; k++)
                bx(b, at(b, PU[k][0] * swell, PU[k][1] * swell, PU[k][2]), 0.05f, 0.05f, 0.05f, v3_scale(glow, 0.5f + 0.5f * wp), true);
            break;
        }
        case MON_EYE: {
            float hv = sinf(m->anim * 2.0f) * 0.1f;
            vec3 cc = at(b, 0, 0, 0.5f + hv);
            batch3d_octa(cc, 0.42f * s, 0.42f * s, m->anim * 0.3f, col, false);
            batch3d_box(cc, v3(0.3f * s, 0.3f * s, 0.3f * s), y, col, false);
            bx(b, v3_add(cc, v3_scale(f, 0.36f * s)), 0.03f, 0.17f, 0.17f, col2, false);
            bx(b, v3_add(cc, v3_scale(f, 0.39f * s)), 0.02f, 0.07f + 0.04f * wp, 0.07f + 0.04f * wp,
               m->state == MS_WINDUP ? glow : v3(0.03f, 0.02f, 0.02f), m->state == MS_WINDUP);
            for (int k = -1; k <= 1; k += 2)
                batch3d_spike(v3_add(cc, v3(0, 0.3f * s, 0)), v3_add(v3(0, 1, 0), v3_scale(r, 0.6f * k)), 0.04f * s, 0.22f * s, col2, false);
            for (int k = 0; k < 5; k++) { /* tendrils */
                float a = k * 2.0f * PI_F / 5.0f, sw = sinf(m->anim * 2.5f + k) * 0.12f;
                vec3 t0 = v3_add(cc, v3(cosf(a) * 0.18f * s, -0.28f * s, sinf(a) * 0.18f * s));
                vec3 t1 = v3_add(t0, v3((cosf(a) * 0.1f + sw) * s, -0.3f * s, (sinf(a) * 0.1f - sw) * s));
                vec3 t2 = v3_add(t1, v3(sw * s, -0.25f * s, -sw * s));
                limb(t0, t1, 0.03f * s, col, false);
                limb(t1, t2, 0.02f * s, dark, false);
            }
            break;
        }
        case MON_KNIGHT: {
            float sw = sinf(m->anim * 4.0f) * 0.25f;
            for (int k = -1; k <= 1; k += 2) bx(b, at(b, sw * 0.2f * k, 0.11f * k, 0.45f), 0.08f, 0.08f, 0.45f, col2, false);
            bx(b, at(b, 0, 0, 1.2f + bob), 0.16f, 0.24f, 0.32f, col2, false);
            bx(b, at(b, 0.17f, 0, 1.05f + bob), 0.01f, 0.13f, 0.38f, col, false);          /* tabard */
            bx(b, at(b, 0, 0, 1.68f + bob), 0.15f, 0.14f, 0.16f, col2, false);              /* helm */
            bx(b, at(b, 0.155f, 0, 1.7f + bob), 0.005f, 0.1f, 0.015f, eyes, true);
            batch3d_spike(at(b, -0.02f, 0, 1.84f + bob), v3_sub(v3(0, 1, 0), v3_scale(f, 0.5f)), 0.04f * s, 0.2f * s, col, false);
            bx(b, at(b, 0.3f, -0.28f, 1.15f), 0.04f, 0.22f, 0.32f, col, false);              /* shield */
            bx(b, at(b, 0.35f, -0.28f, 1.2f), 0.01f, 0.08f, 0.1f, col2, false);
            vec3 hand = at(b, 0.25f, 0.3f, 1.0f);
            float raise = m->state == MS_WINDUP ? 1.0f - wp * 0.3f : 0.2f;
            vec3 sdir = v3_norm(v3_add(v3_scale(f, 0.5f), v3(0, raise, 0)));
            limb(hand, v3_add(hand, v3_scale(sdir, 0.85f * s)), 0.025f * s, v3_lerp(col2, v3(0.85f, 0.88f, 0.95f), 0.5f), false);
            bx(b, hand, 0.03f, 0.1f, 0.02f, col, false);
            break;
        }
        case MON_WORM: {
            if (m->hidden) { /* a mound moving under the floor */
                bx(b, at(b, 0, 0, 0.06f), 0.32f, 0.32f, 0.07f, v3_scale(dark, 0.8f), false);
                for (int k = 0; k < 3; k++)
                    bx(b, at(b, sinf(m->anim * 5.0f + k) * 0.2f, cosf(m->anim * 4.0f + k * 2.0f) * 0.2f, 0.12f), 0.06f, 0.06f, 0.05f, dark, false);
                break;
            }
            float rise = m->state == MS_WINDUP ? 0.3f + 0.7f * wp : 1.0f;
            vec3 top = p;
            for (int k = 0; k < 5; k++) {
                float sw = sinf(m->anim * 2.0f + k * 0.8f) * 0.08f * k;
                vec3 q = at(b, sw, 0, (0.15f + k * 0.3f) * rise);
                float w = 0.34f - 0.035f * k;
                bx(b, q, w, w, 0.16f, k % 2 ? dark : col, false);
                top = q;
            }
            for (int k = 0; k < 4; k++) { /* mandibles round the maw */
                float a = k * PI_F * 0.5f + PI_F * 0.25f;
                vec3 q = v3_add(top, v3_add(v3_scale(f, cosf(a) * 0.18f * s), v3_add(v3_scale(r, sinf(a) * 0.18f * s), v3(0, 0.12f * s, 0))));
                batch3d_spike(q, v3_add(v3(0, 1, 0), v3_add(v3_scale(f, cosf(a) * -0.5f), v3_scale(r, sinf(a) * -0.5f))), 0.03f * s, 0.2f * s, col2, false);
            }
            bx(b, v3_add(top, v3(0, 0.15f * s, 0)), 0.1f, 0.1f, 0.02f, glow, true);
            break;
        }
        case MON_MIMIC: {
            bool open = m->awake;
            float a = open ? 0.9f + 0.35f * sinf(m->anim * 8.0f) : 0.0f; /* the lid snaps */
            float lift = open ? 0.18f : 0.0f;
            if (open)
                for (int k = 0; k < 4; k++) {
                    float ph = sinf(m->anim * 10.0f + k * 1.6f) * 0.06f;
                    int fx = k < 2 ? 1 : -1, rx = k % 2 ? 1 : -1;
                    limb(at(b, 0.18f * fx, 0.32f * rx, 0.2f), at(b, 0.25f * fx + ph, 0.5f * rx, 0.0f), 0.04f * s, dark, false);
                }
            bx(b, at(b, 0, 0, 0.25f + lift), 0.3f, 0.42f, 0.25f, col, false);
            for (int k = -1; k <= 1; k += 2) bx(b, at(b, 0, 0.3f * k, 0.26f + lift), 0.31f, 0.025f, 0.26f, col2, false);
            vec3 hinge = at(b, -0.3f, 0, 0.5f + lift);
            vec3 ldir = v3_add(v3_scale(f, cosf(a)), v3(0, sinf(a), 0));
            vec3 lnorm = v3_sub(v3(0, cosf(a), 0), v3_scale(f, sinf(a)));
            batch3d_obox(v3_add(hinge, v3_scale(ldir, 0.31f * s)), v3_scale(ldir, 0.32f * s), v3_scale(r, 0.44f * s), v3_scale(lnorm, 0.05f * s),
                         v3_scale(col, 0.85f), false);
            if (!open) {
                bx(b, at(b, 0.31f, 0, 0.38f), 0.02f, 0.07f, 0.08f, col2, false); /* the latch */
            } else {
                vec3 tooth = v3(0.92f * light.x, 0.9f * light.y, 0.8f * light.z);
                for (int k = 0; k < 5; k++) {
                    float rt = (k - 2) * 0.17f;
                    batch3d_spike(at(b, 0.27f, rt, 0.5f + lift), v3(0, 1, 0), 0.03f * s, 0.1f * s, tooth, false);
                    vec3 lip = v3_add(hinge, v3_add(v3_scale(ldir, 0.6f * s), v3_scale(r, rt * s)));
                    batch3d_spike(lip, v3_scale(lnorm, -1.0f), 0.03f * s, 0.1f * s, tooth, false);
                }
                bx(b, at(b, 0.08f, 0, 0.52f + lift), 0.2f, 0.08f, 0.025f, lit(v3(0.7f, 0.15f, 0.2f), light), false);
                for (int k = -1; k <= 1; k += 2) bx(b, at(b, -0.08f, 0.15f * k, 0.62f + lift), 0.03f, 0.03f, 0.03f, glow, true);
            }
            break;
        }
        case MON_NECRO: {
            bx(b, at(b, 0, 0, 0.6f), 0.22f, 0.24f, 0.6f, col, false);
            bx(b, at(b, 0, 0, 1.32f + bob), 0.16f, 0.22f, 0.22f, col, false);
            bx(b, at(b, -0.04f, 0, 1.66f + bob), 0.12f, 0.14f, 0.15f, dark, false);            /* hood */
            bx(b, at(b, 0.05f, 0, 1.62f + bob), 0.11f, 0.11f, 0.12f, col2, false);              /* skull */
            for (int k = -1; k <= 1; k += 2) {
                bx(b, at(b, 0.16f, 0.045f * k, 1.64f + bob), 0.01f, 0.025f, 0.02f, glow, true);
                batch3d_spike(at(b, 0, 0.2f * k, 1.5f + bob), v3_add(v3(0, 1, 0), v3_scale(r, 0.5f * k)), 0.03f * s, 0.15f * s, col2, false);
            }
            vec3 top = at(b, 0.25f, 0.32f, 1.95f);
            limb(at(b, 0.2f, 0.3f, 0.05f), top, 0.025f * s, dark, false);
            bx(b, top, 0.06f, 0.06f, 0.06f, col2, false);
            batch3d_octa(v3_add(top, v3(0, 0.14f * s, 0)), 0.06f * s, 0.12f * s, m->anim * 3.0f, glow, true);
            for (int k = 0; k < 3; k++) { /* souls about it */
                float a2 = m->anim * 1.5f + k * 2.1f;
                batch3d_box(v3_add(p, v3(cosf(a2) * 0.55f * s, (1.0f + 0.3f * sinf(a2 * 2.0f)) * s, sinf(a2) * 0.55f * s)),
                            v3(0.03f * s, 0.03f * s, 0.03f * s), a2, v3_scale(glow, 0.8f), true);
            }
            break;
        }
        case MON_HARPY: {
            float flap = sinf(m->anim * 12.0f);
            bool dive = m->state == MS_CHARGE;
            bx(b, at(b, 0, 0, 0.45f), 0.12f, 0.12f, 0.18f, col2, false);
            bx(b, at(b, 0.12f, 0, 0.72f), 0.08f, 0.08f, 0.09f, col2, false);
            for (int k = -1; k <= 1; k += 2) {
                bx(b, at(b, 0.2f, 0.035f * k, 0.74f), 0.01f, 0.015f, 0.015f, eyes, true);
                vec3 sh = at(b, 0, 0.12f * k, 0.55f);
                float wing_up = dive ? 0.3f : flap * 0.15f;
                vec3 mid = v3_add(sh, v3_add(v3_scale(r, 0.32f * k * s), v3(0, wing_up * s, 0)));
                vec3 tip = v3_add(mid, v3_add(v3_scale(r, 0.32f * k * s), v3(0, (wing_up * 1.4f - (dive ? 0.0f : 0.05f)) * s, -0.0f)));
                batch3d_obox(v3_scale(v3_add(sh, mid), 0.5f), v3_scale(v3_sub(mid, sh), 0.5f), v3(0, 0.02f * s, 0), v3_scale(f, 0.2f * s), col, false);
                batch3d_obox(v3_scale(v3_add(mid, tip), 0.5f), v3_scale(v3_sub(tip, mid), 0.5f), v3(0, 0.015f * s, 0), v3_scale(f, 0.14f * s), dark, false);
                limb(at(b, 0, 0.06f * k, 0.3f), at(b, 0.05f, 0.08f * k, 0.05f), 0.02f * s, col2, false);
                batch3d_spike(at(b, 0.05f, 0.08f * k, 0.05f), f, 0.02f * s, 0.08f * s, dark, false);
            }
            batch3d_spike(at(b, 0.08f, 0, 0.8f), v3_sub(v3(0, 0.6f, 0), f), 0.05f * s, 0.2f * s, col, false);   /* crest */
            bx(b, at(b, -0.25f, 0, 0.4f), 0.15f, 0.08f, 0.02f, col, false);                                    /* tail */
            break;
        }
        case MON_HOUND: {
            bool lunge = m->state == MS_CHARGE;
            bx(b, at(b, 0, 0, 0.5f + bob), 0.42f, 0.16f, 0.16f, col, false);
            bx(b, at(b, 0.25f, 0, 0.52f + bob), 0.16f, 0.18f, 0.2f, col, false);
            bx(b, at(b, 0.52f, 0, 0.68f + bob), 0.13f, 0.11f, 0.11f, col, false);
            bx(b, at(b, 0.68f, 0, 0.62f + bob), 0.08f, 0.07f, 0.06f, dark, false);
            if (lunge) bx(b, at(b, 0.7f, 0, 0.56f), 0.07f, 0.05f, 0.015f, glow, true); /* bared */
            for (int k = -1; k <= 1; k += 2) {
                batch3d_spike(at(b, 0.48f, 0.08f * k, 0.78f + bob), v3_sub(v3(0, 1, 0), v3_scale(f, 0.4f)), 0.03f * s, 0.12f * s, dark, false);
                bx(b, at(b, 0.63f, 0.06f * k, 0.72f + bob), 0.01f, 0.02f, 0.02f, eyes, true);
                for (int fb = -1; fb <= 1; fb += 2) {
                    float ph = sinf(m->anim * 12.0f + k * fb * 1.3f) * 0.12f;
                    bx(b, at(b, 0.3f * fb + ph, 0.11f * k, 0.2f), 0.05f, 0.05f, 0.2f, dark, false);
                }
            }
            limb(at(b, -0.42f, 0, 0.6f), at(b, -0.7f, 0, 0.75f + sinf(m->anim * 6.0f) * 0.08f), 0.03f * s, col, false);
            break;
        }
        case MON_TROLL: {
            float sw = sinf(m->anim * 3.0f) * 0.3f;
            bool throwing = m->state == MS_WINDUP && m->aux == 1;
            for (int k = -1; k <= 1; k += 2) bx(b, at(b, sw * 0.2f * k, 0.22f * k, 0.35f), 0.15f, 0.14f, 0.35f, dark, false);
            bx(b, at(b, 0.1f, 0, 1.2f + bob), 0.35f, 0.4f, 0.42f, col, false);
            bx(b, at(b, 0.2f, 0, 0.95f), 0.25f, 0.32f, 0.25f, col, false);
            bx(b, at(b, 0.45f, 0, 1.55f + bob), 0.15f, 0.15f, 0.14f, col, false);
            bx(b, at(b, 0.52f, 0, 1.42f + bob), 0.1f, 0.13f, 0.05f, dark, false);
            for (int k = -1; k <= 1; k += 2) {
                bx(b, at(b, 0.6f, 0.06f * k, 1.6f + bob), 0.01f, 0.025f, 0.02f, eyes, true);
                batch3d_spike(at(b, 0.6f, 0.09f * k, 1.4f + bob), v3_add(v3(0, 1, 0), v3_scale(f, 0.3f)), 0.025f * s, 0.12f * s, col2, false);
                vec3 sh = at(b, 0.1f, 0.48f * k, 1.45f);
                vec3 hand;
                if (throwing && k > 0) hand = at(b, -0.25f + wp * 0.3f, 0.45f, 2.2f + wp * 0.2f);
                else if (m->state == MS_WINDUP && !throwing) hand = at(b, 0.3f, 0.5f * k, 1.6f + wp * 0.6f);
                else hand = at(b, 0.35f - sw * 0.2f * k, 0.55f * k, 0.3f);
                limb(sh, hand, 0.1f * s, col, false);
                bx(b, hand, 0.12f, 0.12f, 0.1f, dark, false);
                if (throwing && k > 0) bx(b, v3_add(hand, v3(0, 0.25f * s, 0)), 0.25f, 0.24f, 0.22f, lit(v3(0.45f, 0.4f, 0.34f), light), false);
            }
            break;
        }
        }
        grade_marks(m, b, col2, glow);
        /* status: a tint of the element on top */
        if (m->burn_t > 0 || m->slow_t > 0 || m->poison_t > 0) {
            vec3 sc = m->burn_t > 0 ? element_color(EL_FIRE) : m->slow_t > 0 ? element_color(EL_FROST) : element_color(EL_POISON);
            batch3d_octa(v3_add(p, v3(0, d->h * s + (m->grade == GRADE_BOSS ? 0.4f : 0.12f), 0)), 0.05f, 0.06f, m->anim * 3.0f, sc, true);
        }
    }
}
