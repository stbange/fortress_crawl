#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* On-screen controls for phones and tablets, driving player 1.
 *
 * Left part of the screen: a floating move stick that appears under the thumb; pushing it
 * past the ring sprints. Right part: drag to look. Buttons near the right thumb: attack
 * (hold; dragging from it also turns the view), alternate attack, jump, crouch and weapon
 * switch; small buttons at the top open the skill tree and the map. In the menus, taps
 * select (tap a skill twice to learn it) and close. */

enum { TOUCH_NONE = -1, TOUCH_MOVE = 100, TOUCH_LOOK = 101 };

#define STICK_R 30.0f

static const char *BUTTON_LABELS[TB_COUNT] = { "HIT", "ALT", "JUMP", "DUCK", "WPN", "CAST", ">", "MENU", "MAP", "POT" };

static bool top_button(int b) { return b == TB_SKILLS || b == TB_MAP || b == TB_POTION; }

/* Button centre and radius in low-res pixels; the top buttons are rounded boxes of r x r/2. */
static void button_pos(int W, int H, int b, float *x, float *y, float *r) {
    switch (b) {
    case TB_ATTACK: *x = W - 50.0f; *y = H - 58.0f; *r = 25.0f; break;
    case TB_ALT: *x = W - 108.0f; *y = H - 34.0f; *r = 16.0f; break;
    case TB_JUMP: *x = W - 106.0f; *y = H - 82.0f; *r = 16.0f; break;
    case TB_CROUCH: *x = W - 70.0f; *y = H - 116.0f; *r = 14.0f; break;
    case TB_WEAPON: *x = W - 26.0f; *y = H - 118.0f; *r = 14.0f; break;
    case TB_SPELL: *x = W - 156.0f; *y = H - 46.0f; *r = 18.0f; break;
    case TB_NEXT_SPELL: *x = W - 160.0f; *y = H - 84.0f; *r = 11.0f; break;
    case TB_SKILLS: *x = W - 74.0f; *y = 14.0f; *r = 18.0f; break;
    case TB_POTION: *x = W - 120.0f; *y = 14.0f; *r = 18.0f; break;
    default: *x = W - 28.0f; *y = 14.0f; *r = 18.0f; break; /* TB_MAP */
    }
}

static bool button_hit(int W, int H, int b, float px, float py) {
    float x, y, r;
    button_pos(W, H, b, &x, &y, &r);
    if (top_button(b)) return fabsf(px - x) < r + 2 && fabsf(py - y) < r * 0.5f + 6;
    float slack = r + 4.0f; /* fingers are big */
    return (px - x) * (px - x) + (py - y) * (py - y) < slack * slack;
}

/* Window position (normalized 0..1) to low-res pixels, undoing the letterbox. */
void touch_to_low(Game *g, float nx, float ny, float *x, float *y) {
    Renderer *r = &g->r;
    float sx = (float)r->win_w / r->fbo_w, sy = (float)r->win_h / r->fbo_h;
    float s = sx < sy ? sx : sy;
    float ox = (r->win_w - r->fbo_w * s) * 0.5f, oy = (r->win_h - r->fbo_h * s) * 0.5f;
    *x = (nx * r->win_w - ox) / s;
    *y = (ny * r->win_h - oy) / s;
}

int touch_low_width(Game *g, int win_w, int win_h) {
    int h = g->r.set.low_h;
    float aspect = win_h > 0 ? (float)win_w / win_h : 16.0f / 9.0f;
    aspect = clampf(aspect, 1.0f, 2.4f);
    return ((int)(h * aspect) + 1) & ~1;
}

static int finger_find(TouchCtl *t, SDL_FingerID id) {
    for (int i = 0; i < t->nfingers; i++)
        if (t->fingers[i].id == id) return i;
    return -1;
}

static bool role_taken(TouchCtl *t, int role) {
    for (int i = 0; i < t->nfingers; i++)
        if (t->fingers[i].role == role) return true;
    return false;
}

static void release(Game *g, int i) {
    TouchCtl *t = &g->touch;
    int role = t->fingers[i].role;
    if (role == TOUCH_MOVE) t->moving = false;
    t->fingers[i] = t->fingers[--t->nfingers];
    /* another finger may still hold the same button */
    if (role >= 0 && role < TB_COUNT) t->held[role] = role_taken(t, role);
}

/* A tap while a menu is open. */
static void menu_tap(Game *g, PlayerSlot *ps, float x, float y) {
    int W = g->r.fbo_w;
    if (ps->show_map) { ps->show_map = false; return; }
    /* close box in the top right corner of the screen */
    if (x > W - 40 && y < 26) { ps->show_skills = false; return; }
    int vx, vy, vw, vh;
    game_view_rect(g, ps, &vx, &vy, &vw, &vh);
    game_menu_tap(g, ps, vw, vh, x - vx, y - vy);
}

void touch_event(Game *g, const SDL_Event *e) {
    TouchCtl *t = &g->touch;
    t->active = true;
    const SDL_TouchFingerEvent *f = &e->tfinger;
    float x, y;
    touch_to_low(g, f->x, f->y, &x, &y);
    int W = g->r.fbo_w, H = g->r.fbo_h;
    PlayerSlot *ps = &g->players[0];
    int i = finger_find(t, f->fingerID);

    if (e->type == SDL_EVENT_FINGER_DOWN) {
        if (i >= 0) release(g, i);
        if (t->nfingers >= TOUCH_MAX_FINGERS) return;
        int role = TOUCH_NONE;
        if (story_motive_pending(g, ps)) {
            int vx, vy, vw, vh;
            game_view_rect(g, ps, &vx, &vy, &vw, &vh);
            story_picker_tap(g, ps, vw, vh, x - vx, y - vy);
        } else if (ps->show_skills || ps->show_map) {
            menu_tap(g, ps, x, y);
        } else {
            for (int b = 0; b < TB_COUNT && role == TOUCH_NONE; b++)
                if (button_hit(W, H, b, x, y)) role = b;
            if (role == TB_WEAPON) ps->st.weapon = (ps->st.weapon + 1) % WPN_COUNT;
            else if (role == TB_SKILLS) { ps->show_skills = true; ps->show_map = false; role = TOUCH_NONE; }
            else if (role == TB_MAP) { ps->show_map = true; ps->show_skills = false; role = TOUCH_NONE; }
            else if (role == TB_NEXT_SPELL) spell_next(ps, 1);
            else if (role == TB_POTION) { drink_potion(g, ps, ps->st.hp >= ps->st.maxhp * 0.9f); role = TOUCH_NONE; }
            if (role == TOUCH_NONE && !ps->show_skills && !ps->show_map) {
                if (x < W * 0.45f && !role_taken(t, TOUCH_MOVE)) {
                    role = TOUCH_MOVE;
                    t->mx0 = t->mx = x;
                    t->my0 = t->my = y;
                    t->moving = true;
                } else if (x >= W * 0.45f && !role_taken(t, TOUCH_LOOK)) {
                    role = TOUCH_LOOK;
                }
            }
        }
        t->fingers[t->nfingers].id = f->fingerID;
        t->fingers[t->nfingers].role = role;
        t->nfingers++;
        if (role >= 0 && role < TB_COUNT) t->held[role] = true;
        return;
    }
    if (i < 0) return;
    int role = t->fingers[i].role;
    if (e->type == SDL_EVENT_FINGER_MOTION) {
        if (role == TOUCH_MOVE) {
            t->mx = x;
            t->my = y;
            /* the stick follows a thumb that slides far away */
            float dx = t->mx - t->mx0, dy = t->my - t->my0, d = sqrtf(dx * dx + dy * dy);
            if (d > STICK_R * 1.6f) {
                t->mx0 = t->mx - dx / d * STICK_R * 1.6f;
                t->my0 = t->my - dy / d * STICK_R * 1.6f;
            }
        } else if (role == TOUCH_LOOK || role == TB_ATTACK) {
            /* a full-width swipe turns about 180 degrees (game_look: 0.0025 rad per unit) */
            game_look(g, f->dx * 1250.0f, f->dy * 800.0f);
        }
        return;
    }
    /* up or cancelled */
    release(g, i);
}

void touch_input(Game *g, PlayerInput *in) {
    TouchCtl *t = &g->touch;
    if (!t->active) return;
    if (t->moving) {
        float dx = (t->mx - t->mx0) / STICK_R, dy = (t->my - t->my0) / STICK_R;
        float d = sqrtf(dx * dx + dy * dy);
        if (d > 0.15f) {
            float k = d > 1.0f ? 1.0f / d : 1.0f;
            in->move_x = dx * k;
            in->move_z = -dy * k;
            if (d > 1.25f) in->sprint = true;
        }
    }
    if (t->held[TB_JUMP]) in->jump = in->up = true;
    if (t->held[TB_CROUCH]) in->crouch = in->down = true;
}

/* Pixel-art discs and rings from horizontal strips. */
static void disc(float cx, float cy, float r, float red, float gr, float bl, float a) {
    for (int y = (int)-r; y <= (int)r; y++) {
        float hw = sqrtf(fmaxf(0.0f, r * r - (y + 0.5f) * (y + 0.5f)));
        if (hw < 0.5f) continue;
        batch2d_rect(floorf(cx - hw), floorf(cy + y), floorf(hw * 2.0f + 0.5f), 1, red, gr, bl, a);
    }
}

static void ring(float cx, float cy, float r, float w, float red, float gr, float bl, float a) {
    float ri = r - w;
    for (int y = (int)-r; y <= (int)r; y++) {
        float yy = (y + 0.5f) * (y + 0.5f);
        float ho = sqrtf(fmaxf(0.0f, r * r - yy)), hi = ri * ri > yy ? sqrtf(ri * ri - yy) : 0.0f;
        if (ho < 0.5f) continue;
        if (hi <= 0.0f) {
            batch2d_rect(floorf(cx - ho), floorf(cy + y), floorf(ho * 2.0f + 0.5f), 1, red, gr, bl, a);
        } else {
            batch2d_rect(floorf(cx - ho), floorf(cy + y), fmaxf(1.0f, floorf(ho - hi + 0.5f)), 1, red, gr, bl, a);
            batch2d_rect(floorf(cx + hi), floorf(cy + y), fmaxf(1.0f, floorf(ho - hi + 0.5f)), 1, red, gr, bl, a);
        }
    }
}

void touch_draw(Game *g) {
    TouchCtl *t = &g->touch;
    if (!t->active) return;
    Renderer *r = &g->r;
    int W = r->fbo_w, H = r->fbo_h;
    PlayerSlot *ps = &g->players[0];
    renderer_set_view(r, 0, 0, W, H);
    batch2d_begin(W, H);
    if (r->win_h > r->win_w) {
        /* portrait: the game needs a wide screen */
        batch2d_rect(0, 0, (float)W, (float)H, 0.02f, 0.015f, 0.02f, 0.85f);
        batch2d_flush();
        font_begin(W, H);
        const char *a = "TURN YOUR PHONE SIDEWAYS";
        font_text((W - font_text_width(a)) / 2, H / 2 - 4, a, 0.95f, 0.8f, 0.5f);
        font_end();
        return;
    }
    if (story_motive_pending(g, ps)) {
        batch2d_flush();
        return;
    }
    if (ps->show_skills || ps->show_map) {
        if (ps->show_skills) {
            batch2d_rect(W - 34.0f, 4, 30, 18, 0.5f, 0.3f, 0.2f, 0.6f);
            batch2d_flush();
            font_begin(W, H);
            font_text(W - 22, 9, "X", 1.0f, 0.9f, 0.7f);
            font_end();
        } else {
            batch2d_flush();
        }
        return;
    }
    /* move stick: where the thumb is, or a faint hint where it usually goes */
    if (t->moving) {
        float dx = t->mx - t->mx0, dy = t->my - t->my0, d = sqrtf(dx * dx + dy * dy);
        float k = d > STICK_R ? STICK_R / d : 1.0f;
        bool sprint = d > STICK_R * 1.25f;
        ring(t->mx0, t->my0, STICK_R, 2, 0.9f, 0.8f, 0.6f, 0.35f);
        disc(t->mx0 + dx * k, t->my0 + dy * k, 11, sprint ? 1.0f : 0.9f, sprint ? 0.6f : 0.8f, sprint ? 0.25f : 0.6f, 0.45f);
    } else {
        ring(58, H - 78.0f, STICK_R, 1, 0.9f, 0.8f, 0.6f, 0.12f);
    }
    for (int b = 0; b < TB_COUNT; b++) {
        float x, y, rr;
        button_pos(W, H, b, &x, &y, &rr);
        float a = t->held[b] ? 0.5f : 0.22f;
        vec3 c = b == TB_ATTACK ? v3(1.0f, 0.45f, 0.3f) : (b == TB_ALT ? v3(0.5f, 0.6f, 1.0f) : v3(0.9f, 0.8f, 0.6f));
        if (b == TB_ALT) {
            /* the alternate attack needs the weapon's last skill */
            static const int NEED[WPN_COUNT] = { SK_WHIRLWIND, SK_ARROW_STORM, SK_NOVA };
            if (!ps->st.ranks[NEED[ps->st.weapon]]) a *= 0.45f;
        }
        if (b == TB_SPELL) {
            c = ps->st.spell >= 0 && spell_is_attack(ps->st.spell) ? element_color(spell_element(ps->st.spell)) : v3(0.95f, 0.9f, 0.7f);
        }
        if (top_button(b)) {
            batch2d_rect(x - rr, y - rr * 0.5f, rr * 2, rr, c.x * 0.4f, c.y * 0.4f, c.z * 0.4f, 0.5f);
            batch2d_rect(x - rr, y - rr * 0.5f, rr * 2, 1, c.x, c.y, c.z, a);
            continue;
        }
        disc(x, y, rr, c.x * 0.35f, c.y * 0.35f, c.z * 0.35f, a);
        ring(x, y, rr, 1.5f, c.x, c.y, c.z, a + 0.15f);
    }
    batch2d_flush();
    font_begin(W, H);
    for (int b = 0; b < TB_COUNT; b++) {
        float x, y, rr;
        button_pos(W, H, b, &x, &y, &rr);
        const char *l = BUTTON_LABELS[b];
        if (b == TB_ATTACK) l = combat_weapon_name(ps->st.weapon);
        float k = t->held[b] ? 1.0f : 0.7f;
        if (b == TB_SKILLS && ps->st.points > 0) font_text((int)x - font_text_width(l) / 2, (int)y - 3, l, 1.0f, 0.85f, 0.35f);
        else font_text((int)x - font_text_width(l) / 2, (int)y - 3, l, k, k * 0.9f, k * 0.75f);
    }
    font_end();
}
