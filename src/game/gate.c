#include "game/game.h"
#include <ctype.h>

#include "render/batch.h"
#include "world/history.h"

/* The realms behind the Gate (realms.h, docs/REALMS.md).
 *
 * Each world is joined to six to eight of the twenty-one realms (by its seed). The way in:
 *   - rifts: tears in the fortress, each to one of them, at about its danger. Walking into a
 *     rift attunes the Gate to its realm and takes you through.
 *   - the Gate: an arch in the Sanctum's court. Its dial (strike the stone beside it) turns
 *     it from realm to realm; through it to any realm it is attuned to.
 * In every realm you arrive in front of an arch: back through it goes back where you came
 * from. A realm keeps its rules: weight, air, heat, storms, tides; its keep has its ruler.
 * The Astral Shore opens only once the Old King is settled; the Dream after an end that spared. */

#define GATE_X 38.5f
#define GATE_Z 84.5f
#define DIAL_X 42.5f
#define DIAL_Z 84.5f

static vec3 rgb3(const u8 c[3]) { return v3(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f); }
static int fl(float v) { return (int)floorf(v); }
static vec3 realm_col(int r) { return rgb3(REALM_FLORA[r * 3 + 2].col[0]); }

static int here(vec3 p) { return realm_at_w(p.x, p.y, p.z); }

/* Where the Gate stands in the Sanctum's court, and the realm its dial shows (the bot). */
vec3 gate_ring_pos(void) { return v3(GATE_X, (float)POCKET_G, GATE_Z); }
int gate_dialed(const Game *g) { return g->gate_sel; }

static void upper(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }

static void say(PlayerSlot *ps, const char *text, const char *then) {
    snprintf(ps->msg, sizeof ps->msg, "%s", text);
    snprintf(ps->msg_next, sizeof ps->msg_next, "%s", then ? then : "");
    ps->msg_t = 8.0f + (float)strlen(text) * 0.05f;
}

/* Warnings once per visit (bit k), and not over each other. */
static void note(PlayerSlot *ps, int k, const char *text) {
    if (ps->realm_notes & (1u << k)) return;
    if (ps->banner_time > 0.5f) return;
    ps->realm_notes |= (u8)(1u << k);
    game_banner(ps, text);
}

void gate_init(Game *g) {
    g->realms_joined = realm_pick(g->world.seed);
    g->gate_sel = -1;
    for (int r = 0; r < REALM_COUNT && g->gate_sel < 0; r++)
        if ((g->realms_joined >> r) & 1 && (g->world.realm_attuned >> r) & 1) g->gate_sel = r;
    if (g->gate_sel < 0)
        for (int r = 0; r < REALM_COUNT && g->gate_sel < 0; r++)
            if ((g->realms_joined >> r) & 1) g->gate_sel = r;
    g->nrifts = 0;
    g->eclipse_t = 0;
    g->realm_t = 0;
    g->storm_t = 5.0f;
}

/* ---------- the story ---------- */

static bool finale(const Game *g, int *choice) {
    const Deed *d = story_chapter_deed(&g->world, 8);
    if (choice) *choice = d ? d->choice : -1;
    return d != NULL;
}

bool gate_open(const Game *g, int r, const char **why) {
    int choice;
    bool done = finale(g, &choice);
    /* the Astral Shore opens only with the Old King settled (king.c), the main story's end; the
     * Dream with any end that spared: the chronicle's, or a king's by a word or the artefact */
    bool won = g->world.king_wins > 0 || g->world.king_ending != KE_NONE, mercy = g->world.king_mercy || g->world.king_ending == KE_WORD || g->world.king_ending == KE_ARTEFACT;
    if ((REALMS[r].rules & RR_STORY) && !won) { if (why) *why = "SEALED UNTIL THE OLD KING IS SETTLED"; return false; }
    if ((REALMS[r].rules & RR_SPARED) && !(done && choice == CH_SPARE) && !mercy) { if (why) *why = "IT SLEEPS - ONLY AN END THAT SPARED WAKES THIS DREAM"; return false; }
    return true;
}

/* A realm's story line with this world's names. */
static void fill(Game *g, const char *tmpl, char *out, int n) {
    const History *h = g->world.story_gen ? history_of(g->world.seed) : NULL;
    const char *F = h ? h->fortress : "the fortress", *B = h ? h->builders : "its builders", *S = h ? h->sealed : "what was sealed below",
               *N = h ? h->night : "the night it fell";
    int o = 0;
    for (const char *p = tmpl; *p && o < n - 1; p++) {
        const char *ins = NULL;
        if (p[0] == '%' && p[1]) {
            switch (p[1]) {
            case 'F': ins = F; break;
            case 'B': ins = B; break;
            case 'S': ins = S; break;
            case 'N': ins = N; break;
            default: break;
            }
        }
        if (ins) {
            for (; *ins && o < n - 1; ins++) out[o++] = *ins;
            p++;
        } else out[o++] = *p;
    }
    out[o] = 0;
}

void gate_lore_text(Game *g, int a, int b, char *out, int n) {
    if (a < 0 || a >= REALM_COUNT) { snprintf(out, (size_t)n, "?"); return; }
    if (b == 0) {
        char line[STORY_TEXT];
        fill(g, REALMS[a].lore, line, sizeof line);
        snprintf(out, (size_t)n, "%c%s: %s", toupper((unsigned char)REALMS[a].name[0]), REALMS[a].name + 1, line);
        return;
    }
    /* a page of the Library of Ash: a question of the history and its answer */
    const History *h = g->world.story_gen ? history_of(g->world.seed) : NULL;
    if (!h) { snprintf(out, (size_t)n, "A burned page: the account of the fall, too charred to read."); return; }
    char q[STORY_TEXT], ans[STORY_TEXT];
    int qi = (b - 1) % Q_COUNT;
    history_question(h, qi, NULL, q, sizeof q);
    history_answer(h, qi, NULL, ans, sizeof ans);
    snprintf(out, (size_t)n, "A page from the Library of Ash. %.200s %.400s", q, ans);
}

/* ---------- travel ---------- */

static vec3 arrival_pos(int r, float *yaw) {
    int x, y, z;
    realm_arrival(r, &x, &y, &z);
    *yaw = PI_F * 0.5f; /* away from the arch, toward +z */
    return v3(x + 0.5f, (float)y, z + 3.5f);
}

static void move_player(Game *g, PlayerSlot *ps, vec3 to, float yaw, vec3 col) {
    particles_burst(g, player_eye(&ps->pl), col, 24, 3.5f, 0.06f, true);
    ps->pl.pos = to;
    ps->pl.vel = v3(0, 0, 0);
    ps->pl.yaw = yaw;
    ps->pl.pitch = 0;
    ps->st.fall_speed = 0;
    ps->gate_cd = 1.5f;
    particles_burst(g, player_eye(&ps->pl), col, 24, 3.5f, 0.06f, true);
    flash_add(g, player_eye(&ps->pl), v3_scale(col, 1.3f), 9.0f, 0.5f);
}

bool gate_go(Game *g, PlayerSlot *ps, int r, vec3 from, float yaw) {
    if (r < 0 || r >= REALM_COUNT) return false;
    const char *why = NULL;
    if (!gate_open(g, r, &why)) { game_banner(ps, why); return false; }
    Stats *s = &ps->st;
    if (here(ps->pl.pos) < 0) { /* from the fortress or the Sanctum: back there afterwards */
        s->gate_from = from;
        s->gate_from_yaw = yaw;
        s->gate_from_set = true;
    }
    bool first = !((g->world.realm_seen >> r) & 1);
    g->world.realm_attuned |= 1u << r;
    g->world.realm_seen |= 1u << r;
    g->gate_sel = r;
    float ay;
    vec3 to = arrival_pos(r, &ay);
    move_player(g, ps, to, ay, realm_col(r));
    ps->realm_notes = 0;
    sfx_at(g, SFX_GATE_OPEN, to, 1.0f);
    char buf[160];
    snprintf(buf, sizeof buf, "%s - DANGER %d", REALMS[r].name, REALMS[r].level);
    upper(buf);
    game_banner(ps, buf);
    if (first) {
        char line[STORY_TEXT];
        gate_lore_text(g, r, 0, line, sizeof line);
        say(ps, line, REALMS[r].blurb);
        lore_add(g, LORE_REALM, r, 0, 0);
    }
    g->autosave_timer = 1e9f;
    g_tele.realm_trips++;
    return true;
}

/* Out through the arch: back to where you came from, else to the Gate in the Sanctum. */
void gate_go_home(Game *g, PlayerSlot *ps);
static void go_home(Game *g, PlayerSlot *ps) {
    Stats *s = &ps->st;
    vec3 to = v3(GATE_X, (float)POCKET_G, GATE_Z + 2.0f);
    float yaw = PI_F * 0.5f;
    if (s->gate_from_set && here(s->gate_from) < 0) { to = s->gate_from; yaw = s->gate_from_yaw; }
    s->gate_from_set = false;
    int r = here(ps->pl.pos);
    move_player(g, ps, to, yaw, r >= 0 ? realm_col(r) : v3(1, 1, 1));
    ps->gate_cd = 2.5f;
    sfx_at(g, SFX_GATE_OPEN, to, 1.0f);
    g->autosave_timer = 1e9f;
}

/* (the bot, when it cannot find the arch: as if it had walked through) */
void gate_go_home(Game *g, PlayerSlot *ps) { if (here(ps->pl.pos) >= 0) go_home(g, ps); }

/* ---------- rifts in the fortress ---------- */

/* Which realm a chunk's rift leads to (-1: none here): about one chunk in thirty of the
 * fortress has one, to the joined realm closest to its danger. The Grand Vestibule always
 * has one, to the gentlest. */
static int rift_realm(Game *g, const ChunkInfo *ci, Rng *r) {
    if (!g->realms_joined) return -1;
    bool vestibule = ci->has_struct && ci->st.type == STRUCT_VESTIBULE && ci->cx == 1 && ci->cz == 0;
    if (!vestibule && hash_unit(hash3(g->world.seed, ci->cx, ci->cy, ci->cz, 0x41F7)) >= 0.035f) return -1;
    int best = -1;
    float bd = 1e9f;
    for (int k = 0; k < REALM_COUNT; k++) {
        if (!((g->realms_joined >> k) & 1)) continue;
        float d = vestibule ? (float)REALMS[k].level : fabsf((float)REALMS[k].level - (float)ci->level) + rng_float(r) * 6.0f;
        if (d < bd) { bd = d; best = k; }
    }
    return best;
}

static bool rift_spot(Game *g, Chunk *c, Rng *r, vec3 *out) {
    for (int attempt = 0; attempt < 60; attempt++) {
        int x = c->cx * CHUNK + rng_range(r, 3, 28), z = c->cz * CHUNK + rng_range(r, 3, 28);
        for (int y = c->cy * CHUNK + 28; y > c->cy * CHUNK + 1; y--) {
            if (game_solid(g, x, y - 1, z) != 1) continue;
            bool room = true;
            for (int k = 0; k < 3 && room; k++) room = !game_solid(g, x, y + k, z);
            if (!room) break;
            *out = v3(x + 0.5f, (float)y, z + 0.5f);
            return true;
        }
    }
    return false;
}

static void ruler_spawn(Game *g, Chunk *c, int r) {
    if ((g->world.realm_beaten >> r) & 1) return;
    int kx, ky, kz;
    realm_keep(r, &kx, &ky, &kz);
    bool patch = REALMS[r].terrain == TR_MIRROR || REALMS[r].terrain == TR_DREAM;
    vec3 p = patch ? v3(5 * CHUNK + 21.5f, (float)ky, 5 * CHUNK + 16.5f) : v3(kx + REALM_KEEP_R - 5.5f, (float)ky, kz + 0.5f);
    if (fl(p.x / CHUNK) != c->cx || fl(p.y / CHUNK) != c->cy || fl(p.z / CHUNK) != c->cz) return;
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (g->mons[i].alive && g->mons[i].ruler == r + 1) return;
    static const u8 RULER[REALM_COUNT] = {
        MON_KNIGHT, MON_BRUTE, MON_WRAITH, MON_EYE, MON_GOLEM, MON_NECRO, MON_SHAMAN, MON_TROLL, MON_WORM, MON_HARPY,
        MON_WISP, MON_EYE, MON_HORNED, MON_SLIME, MON_SKELETON, MON_GOLEM, MON_WRAITH, MON_HORNED, MON_EYE, MON_KNIGHT, MON_BRUTE,
    };
    ThemeInfo ti;
    theme_info(c->info.theme, &ti);
    vec3 tint = v3(ti.mat[B_ACCENT].r / 255.0f, ti.mat[B_ACCENT].g / 255.0f, ti.mat[B_ACCENT].b / 255.0f);
    Monster *m = monster_spawn(g, RULER[r], p, REALMS[r].level + 2, true, tint, chunk_spawn_key(c->cx, c->cy, c->cz));
    if (!m) return;
    monster_make_boss(g, m);
    m->ruler = (u8)(r + 1);
    snprintf(m->name, sizeof m->name, "%c%s", toupper((unsigned char)REALMS[r].ruler[0]), REALMS[r].ruler + 1);
    if (r == RM_MIRROR) { /* your reflection: your own colours */
        m->color = g->players[0].color;
        m->color2 = v3_scale(g->players[0].color, 0.6f);
    }
}

void gate_spawn_chunk(Game *g, Chunk *c) {
    const ChunkInfo *ci = &c->info;
    if (ci->arch == ARCH_REALM) {
        int r = realm_of_flavor(ci->flavor);
        if (r >= 0) ruler_spawn(g, c, r);
        return;
    }
    if (ci->arch == ARCH_EXTERIOR || ci->arch == ARCH_POCKET || town_chunk(c)) return;
    Rng rng = rng_make(ci->seed ^ 0x41F7ULL);
    int r = rift_realm(g, ci, &rng);
    if (r < 0) return;
    vec3 p;
    if (!rift_spot(g, c, &rng, &p)) return;
    for (int i = 0; i < g->nrifts; i++)
        if (g->rifts[i].alive && v3_len(v3_sub(g->rifts[i].pos, p)) < 1.0f) return;
    int slot = -1;
    for (int i = 0; i < g->nrifts && slot < 0; i++)
        if (!g->rifts[i].alive) slot = i;
    if (slot < 0) {
        if (g->nrifts >= (int)(sizeof g->rifts / sizeof g->rifts[0])) return;
        slot = g->nrifts++;
    }
    struct Rift *rf = &g->rifts[slot];
    rf->pos = p;
    rf->realm = (u8)r;
    rf->alive = true;
    rf->chunk_key = chunk_spawn_key(c->cx, c->cy, c->cz);
    rf->yaw = rng_float(&rng) * PI_F;
    if (g_note_hook) {
        char buf[128];
        snprintf(buf, sizeof buf, "a rift to %s at %.0f %.0f %.0f", REALMS[r].name, p.x, p.y, p.z);
        g_note_hook(0, "RIFT", buf);
    }
}

void gate_ruler_slain(Game *g, Monster *m) {
    int r = m->ruler - 1;
    if (r < 0 || r >= REALM_COUNT) return;
    g->world.realm_beaten |= 1u << r;
    char buf[128];
    snprintf(buf, sizeof buf, "%s IS FALLEN", REALMS[r].ruler);
    upper(buf);
    game_banner_all(g, buf);
    for (int k = 0; k < 2; k++) pickup_spawn(g, PK_KEYSTONE, v3_add(m->pos, v3(0.6f * k - 0.3f, 0.6f, 0.4f)), 0, NULL, 0, 0);
    /* something of the realm: a reagent of each of its animals */
    for (int k = 0; k < 3; k++)
        if (REALM_FAUNA[r * 3 + k].reagent >= 0) pickup_spawn(g, PK_REAGENT, v3_add(m->pos, v3(0, 0.6f, 0.5f * k)), REALM_FAUNA[r * 3 + k].reagent, NULL, 0, 0);
    g_tele.rulers++;
    g->autosave_timer = 1e9f;
}

/* ---------- plants and animals ---------- */

void gate_note_fauna(Game *g, int species) {
    if (species >= 0 && species < 64) g->world.fauna_seen |= 1ull << species;
}

void gate_flora_broken(Game *g, PlayerSlot *ps, Block b, int x, int y, int z, u64 h) {
    int r = realm_at(floordiv(x, CHUNK), floordiv(y, CHUNK), floordiv(z, CHUNK));
    if (r < 0) return;
    int sp = r * 3 + (b - B_RFLORA_LOW);
    const FloraDef *f = &REALM_FLORA[sp];
    g->world.flora_seen |= 1ull << sp;
    vec3 at = v3(x + 0.5f, y + 0.4f, z + 0.5f);
    if (f->food >= 0 && (h & 1) == 0) pickup_spawn(g, PK_FOOD, at, f->food, NULL, 0, 0);
    if (f->reagent >= 0 && (h % 3) == 0) pickup_spawn(g, PK_REAGENT, at, f->reagent, NULL, 0, 0);
    g_tele.harvested++;
    if (!ps) return;
    Stats *s = &ps->st;
    char buf[96];
    const char *var = f->var[(h >> 3) % 3];
    switch (f->fx) {
    case FX_AIR:
        ps->breath = BREATH_MAX;
        snprintf(buf, sizeof buf, "A %s %s - A BREATH OF AIR", var, f->name);
        break;
    case FX_HEAL:
        s->hp = fminf(s->maxhp, s->hp + s->maxhp * 0.2f);
        snprintf(buf, sizeof buf, "A %s %s - IT CLOSES WOUNDS", var, f->name);
        break;
    case FX_MANA:
        s->mana = fminf(s->maxmana, s->mana + s->maxmana * 0.25f);
        snprintf(buf, sizeof buf, "A %s %s - MANA", var, f->name);
        break;
    case FX_SPORES:
        sim_gas(g, GAS_SPORES, at, 0.4f);
        snprintf(buf, sizeof buf, "A %s %s - SPORES", var, f->name);
        break;
    case FX_WARM:
        s->warm_t = fmaxf(s->warm_t, 180.0f);
        s->cold = 0;
        snprintf(buf, sizeof buf, "A %s %s - IT WARMS YOU", var, f->name);
        break;
    case FX_COOL:
        s->sunward_t = fmaxf(s->sunward_t, 180.0f);
        snprintf(buf, sizeof buf, "A %s %s - A SUN-WARD FOR THREE MINUTES", var, f->name);
        break;
    case FX_SEEDS:
        s->seeds = (u8)MINI(99, s->seeds + 2);
        snprintf(buf, sizeof buf, "A %s %s - SEEDS", var, f->name);
        break;
    case FX_SHOCK:
        game_hurt_player(g, ps, s->maxhp * 0.08f, at);
        flash_add(g, at, v3(0.7f, 0.8f, 1.2f), 6.0f, 0.3f);
        sfx_at(g, SFX_IMPACT_LIGHTNING, at, 0.6f);
        snprintf(buf, sizeof buf, "A %s %s - IT HELD A CHARGE", var, f->name);
        break;
    case FX_LIGHT:
        s->mana = fminf(s->maxmana, s->mana + s->maxmana * 0.1f);
        particles_burst(g, at, rgb3(f->col[(h >> 3) % 3]), 16, 2.0f, 0.05f, true);
        snprintf(buf, sizeof buf, "A %s %s - ITS LIGHT LINGERS", var, f->name);
        break;
    case FX_PAGE: {
        int q = 1 + (int)((h >> 5) % Q_COUNT);
        lore_add(g, LORE_REALM, r, q, 0);
        char line[STORY_TEXT];
        gate_lore_text(g, r, q, line, sizeof line);
        say(ps, line, NULL);
        snprintf(buf, sizeof buf, "A PAGE OF THE %s - IT ANSWERS A QUESTION", f->name);
        break;
    }
    default:
        snprintf(buf, sizeof buf, "A %s %s", var, f->name);
        break;
    }
    upper(buf);
    if (ps->banner_time <= 0.3f) game_banner(ps, buf);
}

void gate_fauna_speaks(Game *g, PlayerSlot *ps, int species, u64 key) {
    const History *h = g->world.story_gen ? history_of(g->world.seed) : NULL;
    char line[STORY_TEXT];
    const FaunaDef *f = &REALM_FAUNA[species];
    gate_note_fauna(g, species);
    if (!h) snprintf(line, sizeof line, "The %s looks at you as if it knew you. It says nothing you understand.", f->name);
    else if (!strcmp(f->say, "remembers") && h->nfacts > 0) {
        char fact[STORY_TEXT];
        history_fact_line(h, (int)(mix64(key) % (u64)h->nfacts), NULL, fact, sizeof fact);
        snprintf(line, sizeof line, "The %s remembers, in a voice that is not its own: '%.400s'", f->name, fact);
    } else if (!strcmp(f->say, "recites") && h->nclues > 0) {
        char clue[STORY_TEXT];
        history_clue_text(h, (int)(mix64(key ^ 0xB00C) % (u64)h->nclues), NULL, clue, sizeof clue);
        snprintf(line, sizeof line, "The %s recites a page it ate: '%.400s'", f->name, clue);
    } else {
        int choice;
        bool done = finale(g, &choice);
        snprintf(line, sizeof line, "In the %s's eyes you see %s the other way round: what you killed stands, what you spared lies still.%s",
                 f->name, h->fortress, done ? " And the end you chose, unchosen." : "");
    }
    say(ps, line, NULL);
    sfx_at(g, SFX_CRITTER_SQUEAK, ps->pl.pos, 0.4f);
}

/* ---------- the rules of each realm ---------- */

/* Open sky above: nothing that blocks it (glass counts as a roof) within 48 cells. */
static bool open_sky(Game *g, vec3 p) {
    int x = fl(p.x), z = fl(p.z), y = fl(p.y + 1.7f);
    for (int k = 0; k < 48; k++) {
        bool known;
        Block b = block_mat(chunkmap_block(&g->map, x, y + k, z, &known));
        if (!known) return true;
        if (b == B_GLASS || block_opaque(b) || b == B_WATER) return false;
    }
    return true;
}

bool gate_no_air(Game *g, PlayerSlot *ps) {
    int r = here(ps->pl.pos);
    return r >= 0 && (REALMS[r].rules & RR_AIRLESS) && !ps->pl.fly && open_sky(g, ps->pl.pos);
}

float gate_gravity(Game *g, PlayerSlot *ps) {
    int r = here(ps->pl.pos);
    if (r < 0) return 1.0f;
    float gr = REALMS[r].gravity;
    if (REALMS[r].rules & RR_SHIFT) gr = 1.0f + 0.65f * sinf(g->realm_t * 2.0f * PI_F / 45.0f);
    return gr;
}

/* Base temperature: the realm's, or the desert's day and night (ten minutes a day). */
static float g_realm_clock;
float gate_temperature(int r) {
    if (r < 0 || r >= REALM_COUNT) return 12.0f;
    if (REALMS[r].rules & RR_DAYNIGHT) return 14.0f + 32.0f * sinf(g_realm_clock * 2.0f * PI_F / 600.0f);
    return REALMS[r].temp;
}

static void lightning(Game *g, vec3 near) {
    float a = (float)(g->realm_t * 7.31) , d = 6.0f + fmodf(g->realm_t * 13.7f, 18.0f);
    vec3 p = v3_add(near, v3(cosf(a) * d, 0, sinf(a) * d));
    int x = fl(p.x), z = fl(p.z), y = fl(near.y) + 24;
    bool known;
    /* a lightning tree or a rod near takes it */
    for (int k = 0; k < 24; k++) {
        int dx = (k % 5) - 2, dz = (k / 5) - 2;
        for (int yy = fl(near.y) + 16; yy > fl(near.y) - 6; yy--)
            if (block_mat(chunkmap_block(&g->map, x + dx * 2, yy, z + dz * 2, &known)) == B_METAL) { x += dx * 2; z += dz * 2; y = yy + 1; goto struck; }
    }
    while (y > fl(near.y) - 12 && !block_solid(chunkmap_block(&g->map, x, y - 1, z, &known))) y--;
struck:;
    vec3 at = v3(x + 0.5f, (float)y, z + 0.5f);
    for (int k = 0; k < 10; k++)
        particles_burst(g, v3_add(at, v3(sinf(k * 2.1f) * 0.3f, k * 2.0f, cosf(k * 1.7f) * 0.3f)), v3(0.8f, 0.9f, 1.3f), 3, 1.0f, 0.08f, true);
    flash_add(g, v3_add(at, v3(0, 2, 0)), v3(0.9f, 0.95f, 1.5f), 22.0f, 0.25f);
    sfx_at(g, SFX_IMPACT_LIGHTNING, at, 1.0f);
    sim_element_hit(g, at, EL_LIGHTNING, -1, 30.0f);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->st.dead) continue;
        float dd = v3_len(v3_sub(ps->pl.pos, at));
        if (dd < 3.0f) {
            tele_src(HS_OTHER, 0, 0, 0);
            game_hurt_player(g, ps, ps->st.maxhp * (0.12f + 0.1f * ps->st.wet), at);
            game_banner(ps, "STRUCK BY LIGHTNING - KEEP NEAR THE LIGHTNING TREES AND RODS");
        }
    }
}

static void rules(Game *g, PlayerSlot *ps, int r, float dt) {
    Stats *s = &ps->st;
    const RealmDef *d = &REALMS[r];
    vec3 p = ps->pl.pos;
    bool sky = open_sky(g, p);
    if (s->sunward_t > 0) s->sunward_t -= dt;
    if ((d->rules & RR_AIRLESS) && sky) note(ps, 0, "NO AIR OUT HERE - BREATHE UNDER THE DOMES, OR FROM A FROST LILY");
    if (d->rules & RR_SCORCH) {
        if (sky && s->sunward_t <= 0) {
            s->burn_t = fmaxf(s->burn_t, 0.6f);
            s->burn_dps = fmaxf(s->burn_dps, s->maxhp * 0.025f);
            note(ps, 1, "THE SUN BURNS - A SUN-WARD (GLASS FLOWERS, OR THE SHOP'S SALVE) KEEPS IT OFF");
        }
    }
    if (d->rules & RR_WIND || ((d->rules & RR_SANDSTORM) && fmodf(g->realm_t, 90.0f) > 65.0f)) {
        float a = g->realm_t * 0.05f + r, gust = 0.5f + 0.5f * sinf(g->realm_t * 0.7f);
        if (sky && !ps->pl.fly) {
            ps->pl.vel.x += cosf(a) * gust * 9.0f * dt;
            ps->pl.vel.z += sinf(a) * gust * 9.0f * dt;
        }
        if (d->rules & RR_SANDSTORM) note(ps, 2, r == RM_RED_MOON ? "A RUST STORM - TAKE COVER" : "A SANDSTORM - TAKE COVER");
    }
    if (d->rules & RR_TIDE) {
        /* the tide follows the moons: at high water the waves sweep the causeway */
        if (g->tide > 0.35f && p.y < realm_ground_y(r) + 1.5f && sim_water_depth(g, p) < 0.5f) {
            int kx, ky, kz;
            realm_keep(r, &kx, &ky, &kz);
            float ax = REALM_ARRIVE_X, az = REALM_ARRIVE_Z, lx = kx - ax, lz = kz - az, len = sqrtf(lx * lx + lz * lz);
            float t = ((p.x - ax) * lx + (p.z - az) * lz) / (len * len);
            if (t > 0.08f && t < 0.88f) {
                float side = ((p.x - ax) * lz - (p.z - az) * lx) >= 0 ? 1.0f : -1.0f;
                ps->pl.vel.x += lz / len * side * 14.0f * g->tide * dt;
                ps->pl.vel.z += -lx / len * side * 14.0f * g->tide * dt;
                s->wet = 1.0f;
                if (fmodf(g->time * 3.0f, 1.0f) < dt * 3.0f) particles_burst(g, v3_add(p, v3(0, 0.3f, 0)), v3(0.6f, 0.7f, 0.9f), 6, 2.5f, 0.06f, false);
                note(ps, 3, "THE TIDE IS IN - THE WAVES SWEEP THE CAUSEWAY. CROSS AT LOW WATER");
            }
        }
    }
    if (d->rules & RR_DRAIN) {
        bool near = false, known;
        for (int k = 0; k < 40 && !near; k++) {
            int dx = (int)(mix64((u64)k * 31) % 7) - 3, dy = (int)(mix64((u64)k * 37) % 5) - 1, dz = (int)(mix64((u64)k * 41) % 7) - 3;
            near = block_mat(chunkmap_block(&g->map, fl(p.x) + dx, fl(p.y) + dy, fl(p.z) + dz, &known)) == B_CRYSTAL;
        }
        if (near) {
            s->mana = fmaxf(0.0f, s->mana - 4.0f * dt);
            note(ps, 4, "THE CRYSTAL DRINKS YOUR MANA");
        }
    }
    /* standing in thorns; stepping on mycelium wakes the monsters */
    bool known;
    Block in = block_mat(chunkmap_block(&g->map, fl(p.x), fl(p.y + 0.1f), fl(p.z), &known));
    if (in >= B_RFLORA_LOW && in <= B_RFLORA_GLOW) {
        const FloraDef *f = &REALM_FLORA[r * 3 + (in - B_RFLORA_LOW)];
        if (f->fx == FX_THORNS && v3_len(v3(ps->pl.vel.x, 0, ps->pl.vel.z)) > 0.5f) {
            tele_src(HS_OTHER, 0, 0, 0);
            game_dot_player(g, ps, s->maxhp * 0.03f * dt);
            note(ps, 5, "THORNS - WALK AROUND THEM, OR CUT THEM DOWN");
        }
        if (f->fx == FX_ALERT && ps->realm_note_t <= 0) {
            ps->realm_note_t = 4.0f;
            monsters_alert(g, p, 22.0f);
            note(ps, 6, "THE MYCELIUM TREMBLES UNDER YOUR FEET - THE MONSTERS KNOW WHERE YOU ARE");
        }
    }
    if (ps->realm_note_t > 0) ps->realm_note_t -= dt;
    if (d->rules & RR_SHIFT) note(ps, 7, "THE DREAM SHIFTS - YOUR WEIGHT COMES AND GOES");
    /* fell out of the realm */
    if (p.y < realm_ground_y(r) - 90.0f) {
        float yaw;
        vec3 to = arrival_pos(r, &yaw);
        move_player(g, ps, to, yaw, realm_col(r));
    }
}

void gate_update(Game *g, float dt) {
    g->realm_t += dt;
    g_realm_clock = g->realm_t;
    if (g->eclipse_t > 0) g->eclipse_t -= dt;
    g->tide = sinf(g->realm_t * 2.0f * PI_F / 180.0f);
    int storm_realm = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        if (ps->st.gills_t > 0) ps->st.gills_t -= dt;
        if (ps->st.dead) continue;
        vec3 p = ps->pl.pos;
        int r = here(p);
        if (r >= 0) {
            rules(g, ps, r, dt);
            if ((REALMS[r].rules & RR_STORM) && open_sky(g, p)) storm_realm = i;
            if ((REALMS[r].rules & RR_SPORES) && fmodf(g->realm_t + i, 6.0f) < dt)
                sim_gas(g, GAS_SPORES, v3_add(p, v3(sinf(g->realm_t) * 6.0f, 1.0f, cosf(g->realm_t) * 6.0f)), 0.25f);
            if ((REALMS[r].rules & RR_MIASMA) && fmodf(g->realm_t + i, 8.0f) < dt)
                sim_gas(g, GAS_SWAMP, v3_add(p, v3(cosf(g->realm_t * 1.3f) * 9.0f, 0.5f, sinf(g->realm_t * 1.3f) * 9.0f)), 0.35f);
            /* the arch home */
            int ax, ay, az;
            realm_arrival(r, &ax, &ay, &az);
            if (ps->gate_cd <= 0 && fabsf(p.x - (ax + 0.5f)) < 2.2f && fabsf(p.z - (az + 0.5f)) < 0.55f && fabsf(p.y - ay) < 2.0f) go_home(g, ps);
            continue;
        }
        ps->realm_notes = 0;
        /* the Gate in the Sanctum */
        if (realm_has(p)) {
            float dx = p.x - GATE_X, dz = p.z - GATE_Z;
            if (fabsf(dx) < 3.5f && fabsf(dz) < 3.5f && ps->banner_time <= 0) {
                char buf[160];
                int sr = g->gate_sel;
                if (sr < 0) snprintf(buf, sizeof buf, "THE GATE - NO REALM IS JOINED TO THIS WORLD");
                else if (!((g->world.realm_attuned >> sr) & 1))
                    snprintf(buf, sizeof buf, "THE GATE: %s - NOT YET ATTUNED: FIND ITS RIFT IN THE FORTRESS (STRIKE THE DIAL: NEXT)", REALMS[sr].name);
                else snprintf(buf, sizeof buf, "THE GATE: %s - DANGER %d (STRIKE THE DIAL: NEXT)", REALMS[sr].name, REALMS[sr].level);
                upper(buf);
                game_banner(ps, buf);
            }
            if (ps->gate_cd <= 0 && fabsf(dx) < 1.3f && fabsf(dz) < 0.45f && fabsf(p.y - POCKET_G) < 1.6f && g->gate_sel >= 0) {
                int sr = g->gate_sel;
                if ((g->world.realm_attuned >> sr) & 1) gate_go(g, ps, sr, v3(GATE_X, (float)POCKET_G, GATE_Z + 2.0f), PI_F * 0.5f);
                else if (ps->banner_time <= 0) game_banner(ps, "THE GATE IS DARK - IT IS NOT ATTUNED TO THIS REALM YET");
                ps->gate_cd = 1.5f;
            }
            continue;
        }
        /* rifts */
        for (int k = 0; k < g->nrifts; k++) {
            struct Rift *rf = &g->rifts[k];
            if (!rf->alive) continue;
            float d = v3_len(v3_sub(v3_add(rf->pos, v3(0, 1.0f, 0)), v3_add(p, v3(0, 0.9f, 0))));
            if (d < 6.0f && ps->banner_time <= 0) {
                char buf[160];
                snprintf(buf, sizeof buf, "A RIFT TO %s - DANGER %d", REALMS[rf->realm].name, REALMS[rf->realm].level);
                upper(buf);
                game_banner(ps, buf);
            }
            if (d < 0.9f && ps->gate_cd <= 0) {
                bool first = !((g->world.realm_attuned >> rf->realm) & 1);
                if (gate_go(g, ps, rf->realm, v3_add(rf->pos, v3(cosf(rf->yaw) * 1.5f, 0, sinf(rf->yaw) * 1.5f)), rf->yaw)) {
                    if (first) game_banner(ps, "ATTUNED - THE GATE IN THE SANCTUM WILL OPEN THIS REALM NOW");
                } else ps->gate_cd = 2.0f;
                break;
            }
        }
    }
    /* rifts far from everyone close (they open again with their chunk) */
    for (int k = 0; k < g->nrifts; k++)
        if (g->rifts[k].alive && !game_nearest_player(g, g->rifts[k].pos, 120.0f)) g->rifts[k].alive = false;
    /* storms */
    if (storm_realm >= 0 && (g->storm_t -= dt) <= 0) {
        g->storm_t = 5.0f + fmodf(g->realm_t * 3.7f, 7.0f);
        lightning(g, g->players[storm_realm].pl.pos);
    }
}

/* The Gate's dial; in the Clockwork Orrery, the moons on their rods. */
bool gate_use(Game *g, PlayerSlot *ps) {
    vec3 p = ps->pl.pos, f = v3(cosf(ps->pl.yaw), 0, sinf(ps->pl.yaw));
    if (realm_has(p)) {
        vec3 dv = v3_sub(v3(DIAL_X, p.y, DIAL_Z), p);
        float d = v3_len(dv);
        if (d > 2.0f || (d > 0.3f && v3_dot(v3_scale(dv, 1.0f / d), f) < 0.4f)) return false;
        if (!g->realms_joined) return false;
        int start = g->gate_sel < 0 ? 0 : g->gate_sel;
        for (int k = 1; k <= REALM_COUNT; k++) {
            int r = (start + k) % REALM_COUNT;
            if ((g->realms_joined >> r) & 1) { g->gate_sel = r; break; }
        }
        sfx_at(g, SFX_GATE_OPEN, v3(GATE_X, (float)POCKET_G + 1.5f, GATE_Z), 0.4f);
        ps->banner_time = 0;
        return true;
    }
    int r = here(p);
    if (r == RM_ORRERY) {
        int kx, ky, kz;
        realm_keep(r, &kx, &ky, &kz);
        vec3 throne = v3(kx + REALM_KEEP_R - 2.0f, (float)ky, kz + 0.5f);
        if (v3_len(v3_sub(throne, p)) > 3.0f) return false;
        g->eclipse_t = 600.0f;
        game_banner_all(g, "YOU TURN THE MOONS ON THEIR RODS - AN ECLIPSE OVER THE FORTRESS FOR TEN MINUTES");
        sfx_at(g, SFX_GATE_OPEN, throne, 1.0f);
        flash_add(g, v3_add(throne, v3(0, 2, 0)), v3(0.6f, 0.6f, 1.2f), 14.0f, 0.8f);
        return true;
    }
    return false;
}

/* ---------- drawing ---------- */

static void ring(vec3 c, vec3 r, float rad, int n, float t, vec3 col, float size, bool em) {
    for (int k = 0; k < n; k++) {
        float a = k * 2.0f * PI_F / n + t;
        vec3 q = v3_add(c, v3_add(v3_scale(r, cosf(a) * rad), v3(0, sinf(a) * rad, 0)));
        batch3d_box(q, v3(size, size, size), a, col, em);
    }
}

static void swirl(Game *g, vec3 c, vec3 r, float rad, vec3 col, float open) {
    vec3 f = v3(r.z, 0, -r.x);
    batch3d_obox(c, v3_scale(r, rad * 0.75f * open), v3(0, rad * 0.95f * open, 0), v3_scale(f, 0.02f), v3_scale(col, 0.45f + 0.15f * sinf(g->time * 3.0f)), true);
    for (int k = 0; k < 10; k++) {
        float a = g->time * 1.7f + k * 0.63f, rr = rad * (0.3f + 0.6f * fmodf(k * 0.37f + g->time * 0.2f, 1.0f)) * open;
        batch3d_box(v3_add(c, v3_add(v3_scale(r, cosf(a) * rr), v3(0, sinf(a) * rr, 0))), v3(0.04f, 0.04f, 0.04f), a, v3_add(col, v3(0.3f, 0.3f, 0.3f)), true);
    }
}

void gate_draw(Game *g, PlayerSlot *viewer) {
    if (!viewer) return;
    vec3 eye = game_view_eye(viewer);
    int r = here(eye);
    if (r >= 0) { /* the way home, in the arch */
        int ax, ay, az;
        realm_arrival(r, &ax, &ay, &az);
        vec3 c = v3(ax + 0.5f, ay + 3.0f, az + 0.5f);
        if (v3_len(v3_sub(c, eye)) < 80.0f) swirl(g, c, v3(1, 0, 0), 2.6f, v3(1.0f, 0.85f, 0.55f), 1.0f);
        return;
    }
    if (realm_has(eye)) { /* the Gate in the Sanctum's court and its dial */
        vec3 base = v3(GATE_X, (float)POCKET_G, GATE_Z), c = v3_add(base, v3(0, 1.9f, 0)), rx = v3(1, 0, 0);
        vec3 l = game_light_at(g, c);
        l = v3(fmaxf(l.x, 0.4f), fmaxf(l.y, 0.38f), fmaxf(l.z, 0.36f));
        vec3 stone = v3(0.7f * l.x, 0.68f * l.y, 0.62f * l.z);
        int sr = g->gate_sel;
        bool lit_ = sr >= 0 && (g->world.realm_attuned >> sr) & 1;
        vec3 col = sr >= 0 ? realm_col(sr) : v3(0.4f, 0.4f, 0.4f);
        ring(c, rx, 1.75f, 28, 0, stone, 0.16f, false);
        ring(c, rx, 1.95f, 14, g->time * 0.2f, lit_ ? col : v3_scale(stone, 0.6f), 0.08f, lit_);
        for (int k = -1; k <= 1; k += 2) batch3d_box(v3_add(base, v3(1.6f * k, 0.15f, 0)), v3(0.35f, 0.15f, 0.35f), 0, stone, false);
        if (lit_) swirl(g, c, rx, 1.6f, col, 1.0f);
        /* the dial: a stone with the joined realms around it, the chosen one bright */
        vec3 dial = v3(DIAL_X, (float)POCKET_G, DIAL_Z);
        batch3d_box(v3_add(dial, v3(0, 0.5f, 0)), v3(0.25f, 0.5f, 0.25f), 0, stone, false);
        int n = 0;
        for (int k = 0; k < REALM_COUNT; k++) n += (g->realms_joined >> k) & 1;
        int i = 0;
        for (int k = 0; k < REALM_COUNT; k++) {
            if (!((g->realms_joined >> k) & 1)) continue;
            float a = i++ * 2.0f * PI_F / MAXI(1, n);
            bool att = (g->world.realm_attuned >> k) & 1;
            vec3 q = v3_add(dial, v3(cosf(a) * 0.32f, 1.05f, sinf(a) * 0.32f));
            float sz = k == sr ? 0.08f : 0.05f;
            batch3d_octa(q, sz, sz * 1.4f, g->time + a, att ? realm_col(k) : v3(0.25f, 0.25f, 0.28f), att);
        }
        return;
    }
    /* rifts */
    for (int k = 0; k < g->nrifts; k++) {
        struct Rift *rf = &g->rifts[k];
        if (!rf->alive || v3_len(v3_sub(rf->pos, eye)) > 70.0f) continue;
        vec3 c = v3_add(rf->pos, v3(0, 1.2f, 0)), rx = v3(cosf(rf->yaw), 0, sinf(rf->yaw));
        vec3 col = realm_col(rf->realm);
        bool open = gate_open(g, rf->realm, NULL);
        for (int j = 0; j < 14; j++) { /* a jagged tear */
            float t = j / 13.0f, wob = sinf(g->time * 3.0f + j * 1.7f) * 0.08f;
            float w = sinf(t * PI_F) * (open ? 0.45f : 0.2f);
            vec3 q = v3_add(c, v3(0, (t - 0.5f) * 2.2f, 0));
            batch3d_obox(v3_add(q, v3_scale(rx, wob)), v3_scale(rx, w + 0.02f), v3(0, 0.09f, 0), v3(0.02f, 0, 0.02f), v3_scale(col, 0.6f + 0.4f * sinf(g->time * 5.0f + j)), true);
        }
        if (fmodf(g->time + k, 0.5f) < 0.05f) particles_burst(g, c, col, 2, 1.0f, 0.04f, true);
    }
}
