#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* The game side of the stories (world/story.c makes them): each character's motive and
 * chapter in the metaplot, missions found in the dungeon, the lore read so far, the
 * named enemies, and the screens and markers that show all of it. */

/* ---------- small helpers ---------- */

#define WRAP_MAX 100

/* Word wrap into lines of at most maxc characters; returns the number of lines. */
static int wrap(const char *s, int maxc, char lines[][WRAP_MAX], int maxl) {
    int n = 0, len = (int)strlen(s), pos = 0;
    maxc = MINI(maxc, WRAP_MAX - 1);
    while (pos < len && n < maxl) {
        while (pos < len && s[pos] == ' ') pos++;
        int take = MINI(maxc, len - pos);
        if (pos + take < len) {
            int k = take;
            while (k > 0 && s[pos + k] != ' ') k--;
            if (k > 0) take = k;
        }
        memcpy(lines[n], s + pos, (size_t)take);
        lines[n][take] = 0;
        n++;
        pos += take;
    }
    return n;
}

static void upper(char *s) {
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}

static void chunk_of(vec3 p, int *cx, int *cy, int *cz) {
    *cx = floordiv((int)floorf(p.x), CHUNK);
    *cy = floordiv((int)floorf(p.y), CHUNK);
    *cz = floordiv((int)floorf(p.z), CHUNK);
}

static vec3 chunk_centre(int cx, int cy, int cz) { return v3(cx * CHUNK + 16.0f, cy * CHUNK + 16.0f, cz * CHUNK + 16.0f); }

static bool chunk_ready(Game *g, int cx, int cy, int cz) {
    Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
    return c && c->state == CH_READY;
}

static bool player_near(Game *g, vec3 p, float r) { return game_nearest_player(g, p, r) != NULL; }

void lore_add(Game *g, int kind, int a, int b, int c) {
    for (int i = 0; i < g->nlore; i++)
        if (g->lore[i].kind == kind && g->lore[i].a == a && g->lore[i].b == b && g->lore[i].c == c) return;
    if (g->nlore == MAX_LORE) {
        free(g->lore[0].text);
        memmove(g->lore, g->lore + 1, sizeof(LoreEntry) * (MAX_LORE - 1));
        g->nlore--;
    }
    g->lore[g->nlore++] = (LoreEntry){ (u8)kind, a, b, c, NULL };
}

/* The text of a lore entry; generating some of them looks up domains and sites, so it is
 * done once and kept. */
static const char *lore_text(Game *g, LoreEntry *e) {
    if (!e->text) {
        char buf[STORY_TEXT];
        if (e->kind == LORE_REVEAL) story_revelation(&g->world, e->a, buf, sizeof buf);
        else story_trace_text(&g->world, e->a, e->b, e->c, buf, sizeof buf);
        e->text = malloc(strlen(buf) + 1);
        memcpy(e->text, buf, strlen(buf) + 1);
    }
    return e->text;
}

static void show_msg(PlayerSlot *ps, const char *text, const char *then) {
    snprintf(ps->msg, sizeof ps->msg, "%s", text);
    snprintf(ps->msg_next, sizeof ps->msg_next, "%s", then ? then : "");
    ps->msg_t = 9.0f + (float)strlen(text) * 0.05f;
}

static Personal personal_of(Game *g, PlayerSlot *ps) {
    Personal p;
    story_personal(g->world.seed, ps->index, &p);
    return p;
}

void story_game_init(Game *g) { story_world(g->world.seed, &g->story); }

/* ---------- motives ---------- */

bool story_motive_pending(const Game *g, const PlayerSlot *ps) {
    return g->world_started && ps->active && ps->st.motive == 0 && !g->opt.auto_motive;
}

void story_pick_motive(Game *g, PlayerSlot *ps, int motive) {
    ps->st.motive = (u8)(motive + 1);
    ps->st.chapter = 0;
    ps->target_chapter = 0;
    Personal p = personal_of(g, ps);
    char hook[STORY_TEXT];
    story_chapter_hook(&g->world, motive, 0, &p, hook, sizeof hook);
    show_msg(ps, hook, NULL);
    char buf[64];
    snprintf(buf, sizeof buf, "%s", MOTIVE_NAMES[motive]);
    game_banner(ps, buf);
}

#define PICK_ROWS MOTIVE_COUNT

static void picker_row(int W, int H, int i, float *x, float *y, float *w, float *h) {
    *w = (float)MINI(W - 16, 360);
    *h = W < 440 ? 22.0f : 28.0f;
    *x = (W - *w) * 0.5f;
    *y = H * 0.3f + i * (*h + 4);
}

void story_picker_nav(Game *g, PlayerSlot *ps, int nav) {
    if (nav == NAV_UP) ps->motive_sel = (ps->motive_sel + PICK_ROWS - 1) % PICK_ROWS;
    else if (nav == NAV_DOWN) ps->motive_sel = (ps->motive_sel + 1) % PICK_ROWS;
    else if (nav == NAV_BUY) story_pick_motive(g, ps, ps->motive_sel);
}

void story_picker_tap(Game *g, PlayerSlot *ps, int W, int H, float tx, float ty) {
    for (int i = 0; i < PICK_ROWS; i++) {
        float x, y, w, h;
        picker_row(W, H, i, &x, &y, &w, &h);
        if (tx >= x && tx < x + w && ty >= y && ty < y + h) {
            if (ps->motive_sel == i) story_pick_motive(g, ps, i);
            else ps->motive_sel = i;
        }
    }
}

void story_draw_picker(Game *g, PlayerSlot *ps, int W, int H) {
    bool compact = W < 440;
    batch2d_begin(W, H);
    batch2d_rect(0, 0, (float)W, (float)H, 0.01f, 0.008f, 0.012f, 0.86f);
    for (int i = 0; i < PICK_ROWS; i++) {
        float x, y, w, h;
        picker_row(W, H, i, &x, &y, &w, &h);
        bool sel = ps->motive_sel == i;
        batch2d_rect(x, y, w, h, sel ? 0.16f : 0.07f, sel ? 0.1f : 0.05f, sel ? 0.06f : 0.04f, 0.95f);
        if (sel) {
            batch2d_rect(x, y, w, 1, 0.95f, 0.75f, 0.35f, 1.0f);
            batch2d_rect(x, y + h - 1, w, 1, 0.95f, 0.75f, 0.35f, 1.0f);
        }
    }
    batch2d_flush();
    font_begin(W, H);
    char buf[160];
    snprintf(buf, sizeof buf, compact ? "WHY %s?" : "WHY DID YOU COME TO %s?", g->story.fortress);
    upper(buf);
    font_text((W - font_text_width(buf)) / 2, (int)(H * 0.3f) - 22, buf, 0.95f, 0.8f, 0.45f);
    if (g->nplayers > 1) {
        snprintf(buf, sizeof buf, "PLAYER %d", ps->index + 1);
        font_text((W - font_text_width(buf)) / 2, (int)(H * 0.3f) - 34, buf, ps->color.x, ps->color.y, ps->color.z);
    }
    for (int i = 0; i < PICK_ROWS; i++) {
        float x, y, w, h;
        picker_row(W, H, i, &x, &y, &w, &h);
        float c = ps->motive_sel == i ? 1.0f : 0.6f;
        font_text((int)x + 6, (int)y + 3, MOTIVE_NAMES[i], c, c * 0.82f, c * 0.5f);
        char line[2][WRAP_MAX];
        int n = wrap(MOTIVE_PITCH[i], (int)(w - 12) / 6, line, compact ? 1 : 2);
        for (int k = 0; k < n && k < 1 + !compact; k++)
            font_text((int)x + 6, (int)y + (compact ? 12 : 13) + k * 9, line[k], 0.65f * c, 0.6f * c, 0.52f * c);
    }
    const char *help = ps->keyboard && g->touch.active ? "TAP TWICE TO CHOOSE"
                       : ps->pad && !(ps->keyboard && g->mouse) ? "D-PAD AND A TO CHOOSE" : "UP/DOWN AND ENTER TO CHOOSE";
    float x, y, w, h;
    picker_row(W, H, PICK_ROWS, &x, &y, &w, &h);
    font_text((W - font_text_width(help)) / 2, (int)y + 4, help, 0.5f, 0.46f, 0.4f);
    font_end();
}

/* ---------- named enemies ---------- */

static Monster *find_named(Game *g, u64 key) {
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (g->mons[i].alive && g->mons[i].story_key == key) return &g->mons[i];
    return NULL;
}

/* A floor cell in a loaded chunk with `head` free cells above it. */
static bool spot_with_room(Game *g, int cx, int cy, int cz, u64 seed, int head, vec3 *out) {
    Rng r = rng_make(seed);
    for (int attempt = 0; attempt < 120; attempt++) {
        int wx = cx * CHUNK + rng_range(&r, 2, 29), wz = cz * CHUNK + rng_range(&r, 2, 29);
        for (int y = 30; y >= 1; y--) {
            int wy = cy * CHUNK + y;
            if (game_solid(g, wx, wy - 1, wz) != 1) continue;
            bool free_ = true;
            for (int k = 0; k < head && free_; k++) free_ = game_solid(g, wx, wy + k, wz) == 0;
            if (free_) { *out = v3(wx + 0.5f, (float)wy, wz + 0.5f); return true; }
        }
    }
    return false;
}

static Monster *spawn_named(Game *g, const Named *n, u64 chunk_key) {
    float scale = n->scale;
    vec3 p;
    bool found = false;
    /* shrink a giant that does not fit, rather than not have it at all */
    for (; scale >= 1.0f && !found; scale -= 0.2f)
        found = spot_with_room(g, n->cx, n->cy, n->cz, n->key, (int)ceilf(MONSTERS[n->mon].h * scale) + (MONSTERS[n->mon].flies ? 2 : 1), &p);
    if (!found) return NULL;
    scale += 0.2f;
    if (MONSTERS[n->mon].flies) p.y += 1.5f;
    int level = (int)world_difficulty(n->cx, n->cy, n->cz);
    static const vec3 TINT[4] = { { 0.7f, 0.3f, 0.2f }, { 0.4f, 0.25f, 0.6f }, { 0.15f, 0.1f, 0.12f }, { 0.6f, 0.5f, 0.3f } };
    Monster *m = monster_spawn(g, n->mon, p, level, true, TINT[n->kind & 3], chunk_key);
    if (!m) return NULL;
    m->scale = scale;
    m->maxhp = m->hp = m->hp / 3.0f * n->hp_mult; /* monster_spawn tripled it for an elite */
    m->dmg = m->dmg / 1.5f * n->dmg_mult;
    m->xp *= n->hp_mult * 0.4f;
    m->story_key = n->key;
    m->named_kind = (u8)(n->kind + 1);
    snprintf(m->name, sizeof m->name, "%s", n->name);
    upper(m->name);
    return m;
}

/* Demos and tests: a named enemy at a given place. */
Monster *story_spawn_named_at(Game *g, const Named *n, vec3 pos) {
    int level = (int)world_difficulty(n->cx, n->cy, n->cz);
    Monster *m = monster_spawn(g, n->mon, pos, level, true, v3(0.6f, 0.3f, 0.2f), n->key);
    if (!m) return NULL;
    m->scale = n->scale;
    m->maxhp = m->hp = m->hp / 3.0f * n->hp_mult;
    m->dmg = m->dmg / 1.5f * n->dmg_mult;
    m->story_key = n->key;
    m->named_kind = (u8)(n->kind + 1);
    snprintf(m->name, sizeof m->name, "%s", n->name);
    upper(m->name);
    return m;
}

void story_spawn_lord(Game *g, Chunk *c) {
    Named l;
    if (!story_lord_here(&g->world, c->cx, c->cy, c->cz, &l)) return;
    if (looted_has(g, l.key) || find_named(g, l.key)) return;
    spawn_named(g, &l, chunk_spawn_key(c->cx, c->cy, c->cz));
}

static void ensure_target(Game *g, PlayerSlot *ps) {
    if (!ps->st.motive || ps->target_chapter == ps->st.chapter + 1) return;
    story_chapter_target(&g->world, ps->st.chapter, &ps->target);
    ps->target_chapter = ps->st.chapter + 1;
}

/* ---------- chapters ---------- */

static void chapter_complete(Game *g, PlayerSlot *ps, bool other_killed) {
    Stats *s = &ps->st;
    int m = s->motive - 1, ch = s->chapter;
    Personal p = personal_of(g, ps);
    char done[STORY_TEXT], rev[STORY_TEXT], text[STORY_TEXT * 2 + 64], hook[STORY_TEXT];
    story_chapter_done(&g->world, m, ch, &p, done, sizeof done);
    story_revelation(&g->world, ch, rev, sizeof rev);
    lore_add(g, LORE_REVEAL, ch, 0, 0);
    if (other_killed) snprintf(text, sizeof text, "Someone was faster: %.40s is already dead. %.200s %.200s", ps->target.name, done, rev);
    else snprintf(text, sizeof text, "%.300s %.300s", done, rev);
    /* the motive decides the reward */
    game_add_xp(g, ps, 120.0f * (ch + 1));
    vec3 at = v3_add(ps->pl.pos, v3(0, 0.5f, 0));
    switch (m) {
    case MOTIVE_REVENGE:
        s->points++;
        game_banner(ps, "VENGEANCE HARDENS YOU: +1 SKILL POINT");
        break;
    case MOTIVE_GLORY: {
        int r = (int)(mix64(g->world.seed ^ (u64)ch * 31) % RELIC_COUNT);
        s->relics[r]++;
        stats_refresh(ps);
        char b[96];
        snprintf(b, sizeof b, "THEY CARVE YOUR NAME. RELIC: %s", RELICS[r].name);
        upper(b);
        game_banner(ps, b);
        break;
    }
    case MOTIVE_RICHES: {
        s->gold += 120 * (ch + 1);
        Item it = item_roll(g->world.seed ^ (u64)ch * 977, (float)(6 + ch * 4), (ch % 3) == 2 ? RAR_EPIC : RAR_RARE);
        pickup_spawn(g, PK_WEAPON, at, 0, &it, 0, 0);
        game_banner(ps, "A SHARE OF THE HOARD");
        break;
    }
    default: {
        /* knowledge: spells */
        for (int k = 0; k < 2; k++) {
            int id = (int)(mix64(g->world.seed ^ (u64)(ch * 7 + k) * 0x9E37) % SPELL_COUNT);
            spell_learn(ps, id);
        }
        game_banner(ps, "THE PAGES TEACH YOU: 2 SPELLS");
        break;
    }
    }
    s->chapter++;
    ps->target_chapter = 0;
    story_chapter_hook(&g->world, m, s->chapter, &p, hook, sizeof hook);
    show_msg(ps, text, hook);
    g->autosave_timer = 1e9f;
}

/* ---------- missions ---------- */

static Mission *mission_find(Stats *s, u64 key) {
    for (int i = 0; i < s->nmissions; i++)
        if (s->missions[i].key == key) return &s->missions[i];
    return NULL;
}

static void mission_remove(Stats *s, Mission *m) {
    int i = (int)(m - s->missions);
    s->missions[i] = s->missions[--s->nmissions];
}

static bool mission_add(PlayerSlot *ps, const Mission *m) {
    Stats *s = &ps->st;
    if (mission_find(s, m->key)) return false;
    if (s->nmissions >= MAX_MISSIONS) {
        if (ps->banner_time <= 0) game_banner(ps, "TOO MANY MISSIONS - FINISH ONE FIRST");
        return false;
    }
    s->missions[s->nmissions++] = *m;
    return true;
}

static void mission_title(Game *g, const Mission *m, char *out, int n) {
    if (m->type == MI_BOUNTY) {
        Named l;
        int gold;
        story_bounty(&g->world, m->sx, m->sy, m->sz, &l, &gold);
        snprintf(out, (size_t)n, "BOUNTY: %s", l.name);
    } else {
        Retrieve rt;
        story_retrieve(&g->world, m->sx, m->sy, m->sz, &rt);
        snprintf(out, (size_t)n, "RETURN %s", rt.item);
    }
    upper(out);
}

/* Reading a trace: lore for the world, maybe a mission for the reader. */
static void read_trace(Game *g, PlayerSlot *ps, Pickup *p) {
    int cx, cy, cz;
    chunk_of(p->pos, &cx, &cy, &cz);
    story_trace_text(&g->world, cx, cy, cz, ps->reading, sizeof ps->reading);
    lore_add(g, LORE_TRACE, cx, cy, cz);
    Trace t;
    if (!story_trace(&g->world, cx, cy, cz, &t) || !t.mission) return;
    Mission m = { 0 };
    m.sx = cx; m.sy = cy; m.sz = cz;
    char title[96], buf[128];
    if (t.kind == TR_BOUNTY) {
        Named l;
        int gold;
        story_bounty(&g->world, cx, cy, cz, &l, &gold);
        if (looted_has(g, l.key)) {
            strncat(ps->reading, " - Crossed out. Already dead.", sizeof ps->reading - strlen(ps->reading) - 1);
            return;
        }
        m.type = MI_BOUNTY;
        m.key = l.key;
        m.tx = l.cx; m.ty = l.cy; m.tz = l.cz;
        m.reward = gold;
    } else {
        Retrieve rt;
        if (!story_retrieve(&g->world, cx, cy, cz, &rt)) return;
        if (looted_has(g, rt.key)) return;
        m.type = MI_RETRIEVE;
        m.key = rt.key;
        m.tx = rt.cx; m.ty = rt.cy; m.tz = rt.cz;
        m.reward = 30 + (int)world_difficulty(cx, cy, cz) * 8;
    }
    if (mission_add(ps, &m)) {
        mission_title(g, &m, title, sizeof title);
        snprintf(buf, sizeof buf, "NEW MISSION - %s", title);
        game_banner(ps, buf);
    }
}

static void mission_reward(Game *g, PlayerSlot *ps, const Mission *m, const char *what) {
    ps->st.gold += m->reward;
    game_add_xp(g, ps, 25.0f + m->reward * 0.6f);
    if (m->type == MI_RETRIEVE) ps->st.potions_hp = MINI(9, ps->st.potions_hp + 1);
    char buf[128];
    snprintf(buf, sizeof buf, "%s - %d GOLD", what, m->reward);
    upper(buf);
    game_banner(ps, buf);
}

bool story_collect_quest(Game *g, PlayerSlot *ps, Pickup *p) {
    Mission *m = mission_find(&ps->st, p->key);
    if (!m) {
        if (ps->banner_time <= 0) game_banner(ps, "SOMEONE ELSE IS LOOKING FOR THIS");
        return false;
    }
    looted_mark(g, p->key);
    Retrieve rt;
    story_retrieve(&g->world, m->sx, m->sy, m->sz, &rt);
    char what[96];
    snprintf(what, sizeof what, "%s - taken back", rt.item);
    Mission done = *m;
    mission_remove(&ps->st, m);
    mission_reward(g, ps, &done, what);
    flash_add(g, p->pos, v3(1.0f, 0.9f, 0.6f), 6.0f, 0.4f);
    return true;
}

void story_on_kill(Game *g, Monster *mo) {
    u64 key = mo->story_key;
    looted_mark(g, key);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        ensure_target(g, ps);
        bool chapter = ps->st.motive && ps->target.key == key;
        Mission *m = mission_find(&ps->st, key);
        if (m) {
            Mission done = *m;
            mission_remove(&ps->st, m);
            char what[96];
            snprintf(what, sizeof what, "Bounty paid: %s", mo->name);
            mission_reward(g, ps, &done, what);
        }
        if (chapter) chapter_complete(g, ps, false);
        else if (!m && v3_len(v3_sub(ps->pl.pos, mo->pos)) < 40.0f) {
            char buf[96];
            snprintf(buf, sizeof buf, "%s IS DEAD", mo->name);
            game_banner(ps, buf);
        }
    }
}

/* ---------- per frame ---------- */

static void update_domain(Game *g, PlayerSlot *ps) {
    int cx, cy, cz;
    chunk_of(ps->pl.pos, &cx, &cy, &cz);
    if (ps->domain_id && cx == ps->domain_cx && cy == ps->domain_cy && cz == ps->domain_cz) return;
    ps->domain_cx = cx; ps->domain_cy = cy; ps->domain_cz = cz;
    if (world_is_exterior(cx, cy, cz)) {
        snprintf(ps->domain_name, sizeof ps->domain_name, "before the gates of %.24s", g->story.fortress);
        ps->domain_id = 1;
        return;
    }
    Domain d;
    story_domain(&g->world, cx, cy, cz, &d);
    if (d.id == ps->domain_id) return;
    ps->domain_id = d.id;
    snprintf(ps->domain_name, sizeof ps->domain_name, "%s", d.name);
    char line[96], buf[160];
    story_domain_line(&d, line, sizeof line);
    snprintf(buf, sizeof buf, "%s - %s", d.name, line);
    upper(buf);
    if (!g->opt.shot) game_banner(ps, buf);
}

static void update_reading(Game *g, PlayerSlot *ps) {
    Pickup *best = NULL;
    float bd = 1.9f;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &g->pickups[i];
        if (!p->alive || p->kind != PK_TRACE) continue;
        float dx = p->pos.x - ps->pl.pos.x, dz = p->pos.z - ps->pl.pos.z;
        float d = sqrtf(dx * dx + dz * dz);
        if (d < bd && fabsf(p->pos.y - ps->pl.pos.y) < 2.0f) { bd = d; best = p; }
    }
    if (!best) return;
    if (best->key != ps->reading_key) {
        ps->reading_key = best->key;
        read_trace(g, ps, best);
    }
    ps->reading_t = 0.3f;
}

/* Named targets and quest items appear when their chunk is loaded and someone is near. */
static void update_targets(Game *g, PlayerSlot *ps, float dt) {
    ensure_target(g, ps);
    ps->target_retry -= dt;
    Named *t = &ps->target;
    if (looted_has(g, t->key)) {
        chapter_complete(g, ps, true);
        return;
    }
    if (ps->target_retry <= 0 && !find_named(g, t->key) && chunk_ready(g, t->cx, t->cy, t->cz) &&
        player_near(g, chunk_centre(t->cx, t->cy, t->cz), 70.0f)) {
        if (!spawn_named(g, t, t->key)) ps->target_retry = 5.0f;
    }
    for (int i = 0; i < ps->st.nmissions; i++) {
        Mission *m = &ps->st.missions[i];
        if (m->type == MI_BOUNTY) {
            /* someone else already took the head */
            if (looted_has(g, m->key)) { mission_remove(&ps->st, m); i--; }
            continue;
        }
        if (looted_has(g, m->key)) continue;
        bool present = false;
        for (int k = 0; k < MAX_PICKUPS && !present; k++) present = g->pickups[k].alive && g->pickups[k].key == m->key;
        if (present || !chunk_ready(g, m->tx, m->ty, m->tz) || !player_near(g, chunk_centre(m->tx, m->ty, m->tz), 70.0f)) continue;
        vec3 p;
        if (spot_with_room(g, m->tx, m->ty, m->tz, m->key, 2, &p))
            pickup_spawn(g, PK_QUEST, v3_add(p, v3(0, 0.3f, 0)), 0, NULL, m->key, 0);
    }
}

void story_update(Game *g, float dt) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        if (ps->st.motive == 0 && g->opt.auto_motive) ps->st.motive = MOTIVE_REVENGE + 1;
        ps->reading_t -= dt;
        if (ps->reading_t <= 0) ps->reading_key = 0;
        if (ps->msg_t > 0) {
            ps->msg_t -= dt;
            if (ps->msg_t <= 0 && ps->msg_next[0]) show_msg(ps, ps->msg_next, NULL);
        }
        if (story_motive_pending(g, ps) || ps->st.dead) continue;
        update_domain(g, ps);
        update_reading(g, ps);
        if (ps->st.motive) update_targets(g, ps, dt);
    }
}

/* ---------- drawing ---------- */

/* The trace you stand at, or the latest chapter message, in a box above the HUD. */
void story_draw_panel(Game *g, PlayerSlot *ps, int W, int H) {
    const char *text = ps->reading_t > 0 ? ps->reading : ps->msg_t > 0 ? ps->msg : NULL;
    if (!text || !text[0]) return;
    char lines[6][WRAP_MAX];
    int maxc = MINI(78, (W - 24) / 6);
    int n = wrap(text, maxc, lines, 6);
    int bw = 0;
    for (int i = 0; i < n; i++) bw = MAXI(bw, font_text_width(lines[i]));
    float x = (W - bw) * 0.5f - 6, y = H - 52.0f - n * 9;
    batch2d_begin(W, H);
    batch2d_rect(x, y - 5, bw + 12.0f, n * 9 + 9.0f, 0.04f, 0.03f, 0.025f, 0.82f);
    batch2d_rect(x, y - 5, bw + 12.0f, 1, 0.55f, 0.45f, 0.3f, 0.9f);
    batch2d_flush();
    font_begin(W, H);
    bool msg = ps->reading_t <= 0;
    for (int i = 0; i < n; i++)
        font_text((W - font_text_width(lines[i])) / 2, (int)y + i * 9, lines[i], msg ? 1.0f : 0.86f, msg ? 0.82f : 0.8f,
                  msg ? 0.55f : 0.68f);
    font_end();
}

void story_draw_tracker(Game *g, PlayerSlot *ps, int W, int H, int y) {
    if (!ps->st.motive || ps->st.dead || ps->target_chapter != ps->st.chapter + 1) return;
    char dir[64], buf[160];
    story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, ps->target.cx, ps->target.cy, ps->target.cz, dir, sizeof dir);
    snprintf(buf, sizeof buf, "> %s - %s", ps->target.name, dir);
    upper(buf);
    int maxc = (W - 8) / 6;
    if ((int)strlen(buf) > maxc) buf[maxc] = 0;
    font_begin(W, H);
    font_text((W - font_text_width(buf)) / 2, y, buf, 0.95f, 0.55f, 0.4f);
    font_end();
}

/* Names and health bars over named enemies in view. */
void story_draw_plates(Game *g, PlayerSlot *viewer, int W, int H) {
    vec3 eye = player_eye(&viewer->pl);
    const float *m = g->r.vp.m;
    struct { float x, y, frac; const char *name; int kind; } plates[16];
    int n = 0;
    for (int i = 0; i < MAX_MONSTERS && n < 16; i++) {
        Monster *mo = &g->mons[i];
        if (!mo->alive || !mo->named_kind) continue;
        vec3 top = v3_add(mo->pos, v3(0, MONSTERS[mo->type].h * mo->scale + 0.5f, 0));
        float d = v3_len(v3_sub(top, eye));
        if (d > 32.0f || !phys_line_of_sight(eye, v3_sub(top, v3(0, 0.6f, 0)), game_solid, g)) continue;
        float cx = m[0] * top.x + m[4] * top.y + m[8] * top.z + m[12];
        float cy = m[1] * top.x + m[5] * top.y + m[9] * top.z + m[13];
        float cw = m[3] * top.x + m[7] * top.y + m[11] * top.z + m[15];
        if (cw < 0.1f) continue;
        float sx = (cx / cw * 0.5f + 0.5f) * W, sy = (1.0f - (cy / cw * 0.5f + 0.5f)) * H;
        if (sx < -50 || sx > W + 50 || sy < 0 || sy > H) continue;
        plates[n].x = sx;
        plates[n].y = sy;
        plates[n].frac = mo->hp / mo->maxhp;
        plates[n].name = mo->name;
        plates[n].kind = mo->named_kind - 1;
        n++;
    }
    if (!n) return;
    batch2d_begin(W, H);
    for (int i = 0; i < n; i++) {
        float w = 40;
        batch2d_rect(plates[i].x - w / 2 - 1, plates[i].y + 9, w + 2, 4, 0, 0, 0, 0.8f);
        batch2d_rect(plates[i].x - w / 2, plates[i].y + 10, w * clampf(plates[i].frac, 0, 1), 2, 0.9f, 0.2f, 0.15f, 1.0f);
    }
    batch2d_flush();
    font_begin(W, H);
    for (int i = 0; i < n; i++) {
        bool lord = plates[i].kind == NK_LORD;
        font_text((int)plates[i].x - font_text_width(plates[i].name) / 2, (int)plates[i].y, plates[i].name, lord ? 0.9f : 1.0f,
                  lord ? 0.75f : 0.45f, lord ? 0.5f : 0.3f);
    }
    font_end();
}

/* Markers on the map: the chapter target (red) and missions (gold). Off-map targets sit
 * on the edge in their direction; the label says how far up or down. */
static void marker(float cx, float cy, int pxw, int pyw, int pzw, int px, float x0, float y0, float x1, float y1, int tcx, int tcy,
                   int tcz, vec3 col) {
    float mx = cx + (tcx * CHUNK + 16 - pxw) * (float)px, my = cy + (tcz * CHUNK + 16 - pzw) * (float)px;
    bool edge = mx < x0 || mx > x1 - 4 || my < y0 || my > y1 - 4;
    if (edge) {
        float dx = mx - cx, dy = my - cy, k = 1e9f;
        if (dx > 0) k = fminf(k, (x1 - 6 - cx) / dx);
        if (dx < 0) k = fminf(k, (x0 + 2 - cx) / dx);
        if (dy > 0) k = fminf(k, (y1 - 6 - cy) / dy);
        if (dy < 0) k = fminf(k, (y0 + 2 - cy) / dy);
        mx = cx + dx * k;
        my = cy + dy * k;
    }
    for (int k = -2; k <= 2; k++) {
        batch2d_rect(mx + k, my + k, 2, 2, col.x, col.y, col.z, 1.0f);
        batch2d_rect(mx + k, my - k, 2, 2, col.x, col.y, col.z, 1.0f);
    }
    (void)pyw;
    (void)tcy;
}

void story_map_markers(Game *g, PlayerSlot *ps, float cx, float cy, int pxw, int pyw, int pzw, int px, float x0, float y0,
                       float x1, float y1) {
    for (int i = 0; i < ps->st.nmissions; i++) {
        Mission *m = &ps->st.missions[i];
        marker(cx, cy, pxw, pyw, pzw, px, x0, y0, x1, y1, m->tx, m->ty, m->tz, v3(1.0f, 0.8f, 0.3f));
    }
    if (ps->st.motive && ps->target_chapter == ps->st.chapter + 1)
        marker(cx, cy, pxw, pyw, pzw, px, x0, y0, x1, y1, ps->target.cx, ps->target.cy, ps->target.cz, v3(1.0f, 0.25f, 0.2f));
}

/* ---------- the STORY tab ---------- */

void story_tab_nav(Game *g, PlayerSlot *ps, int nav) {
    if (nav == NAV_UP) ps->story_scroll = MAXI(0, ps->story_scroll - 1);
    else if (nav == NAV_DOWN) ps->story_scroll = MINI(MAXI(0, g->nlore - 1), ps->story_scroll + 1);
}

void story_draw_tab(Game *g, PlayerSlot *ps, int W, int H, int top) {
    Stats *s = &ps->st;
    bool compact = W < 440;
    int maxc = (W - 16) / 6, x = compact ? 6 : MAXI(8, (W - 78 * 6) / 2);
    int y = top + 6;
    char buf[STORY_TEXT], lines[8][WRAP_MAX];
    font_begin(W, H);
    if (s->motive) {
        int m = s->motive - 1;
        story_arc_title(&g->world, s->chapter / 3, buf, sizeof buf);
        char head[STORY_TEXT];
        snprintf(head, sizeof head, "%s - CHAPTER %d - ARC %.200s", MOTIVE_NAMES[m], s->chapter + 1, buf);
        upper(head);
        int n = wrap(head, maxc, lines, 2);
        for (int i = 0; i < n; i++, y += 9) font_text(x, y, lines[i], 0.95f, 0.78f, 0.42f);
        ensure_target(g, ps);
        char dir[64];
        story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, ps->target.cx, ps->target.cy, ps->target.cz, dir, sizeof dir);
        snprintf(buf, sizeof buf, "KILL %s, %s - %s", ps->target.name, ps->target.title, dir);
        upper(buf);
        n = wrap(buf, maxc, lines, 2);
        for (int i = 0; i < n; i++, y += 9) font_text(x, y, lines[i], 1.0f, 0.45f, 0.35f);
        Personal p = personal_of(g, ps);
        story_chapter_hook(&g->world, m, s->chapter, &p, buf, sizeof buf);
        n = wrap(buf, maxc, lines, compact ? 2 : 3);
        for (int i = 0; i < n; i++, y += 9) font_text(x, y, lines[i], 0.75f, 0.7f, 0.6f);
    }
    y += 5;
    snprintf(buf, sizeof buf, "MISSIONS %d/%d", s->nmissions, MAX_MISSIONS);
    font_text(x, y, buf, 0.95f, 0.78f, 0.42f);
    y += 10;
    if (!s->nmissions) {
        font_text(x, y, compact ? "READ NOTES AND BOUNTY BOARDS" : "NONE. READ THE NOTES OF THE DEAD AND THE BOUNTY BOARDS.", 0.5f, 0.46f, 0.4f);
        y += 9;
    }
    for (int i = 0; i < s->nmissions; i++) {
        Mission *mi = &s->missions[i];
        char title[96], dir[64];
        mission_title(g, mi, title, sizeof title);
        story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, mi->tx, mi->ty, mi->tz, dir, sizeof dir);
        snprintf(buf, sizeof buf, "%s - %s", title, dir);
        upper(buf);
        if ((int)strlen(buf) > maxc) buf[maxc] = 0;
        font_text(x, y, buf, 1.0f, 0.8f, 0.35f);
        y += 9;
    }
    y += 5;
    snprintf(buf, sizeof buf, "WHAT THE DEAD SAY (%d)", g->nlore);
    font_text(x, y, buf, 0.95f, 0.78f, 0.42f);
    y += 10;
    /* newest first; up/down scroll */
    for (int i = g->nlore - 1 - ps->story_scroll; i >= 0 && y < H - 18; i--) {
        int n = wrap(lore_text(g, &g->lore[i]), maxc - 2, lines, 4);
        bool reveal = g->lore[i].kind == LORE_REVEAL;
        for (int k = 0; k < n && y < H - 18; k++, y += 9) {
            if (k == 0) font_text(x, y, reveal ? "*" : "-", 0.6f, 0.5f, 0.4f);
            font_text(x + 12, y, lines[k], reveal ? 0.95f : 0.68f, reveal ? 0.6f : 0.65f, reveal ? 0.45f : 0.58f);
        }
        y += 2;
    }
    if (!compact) {
        const char *help = ps->keyboard && g->touch.active ? "DRAG UP/DOWN TO SCROLL"
                           : ps->pad && !(ps->keyboard && g->mouse) ? "D-PAD SCROLLS  LB/RB TABS" : "UP/DOWN SCROLL  Q/E TABS  L OPENS THIS";
        font_text((W - font_text_width(help)) / 2, H - 10, help, 0.5f, 0.46f, 0.4f);
    }
    font_end();
}
