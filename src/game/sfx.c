#include "game/sfx.h"

#include "engine/audio.h"
#include "game/game.h"
#include "world/theme.h"

#define MAX_VARIANTS 4

static int sample[SFX_COUNT][MAX_VARIANTS];
static int nvar[SFX_COUNT];
static float last_play[SFX_COUNT];
static float now;
static bool loaded, muted, log_plays;
static u64 rs = 0x51F0A3C9D2E7B4ULL;

static float frand(void) {
    rs = mix64(rs + 0x9E3779B97F4A7C15ULL);
    return (float)(rs >> 40) / (float)(1ULL << 24);
}

static void music_load(void);

/* ---------- loading ---------- */

static bool try_dir(const char *dir) {
    char path[1024];
    snprintf(path, sizeof path, "%s%s.wav", dir, SFX_DEFS[SFX_STEP_STONE].name);
    SDL_IOStream *f = SDL_IOFromFile(path, "rb");
    if (!f) snprintf(path, sizeof path, "%s%s_1.wav", dir, SFX_DEFS[SFX_STEP_STONE].name), f = SDL_IOFromFile(path, "rb");
    if (!f) return false;
    SDL_CloseIO(f);
    int files = 0;
    for (int i = 0; i < SFX_COUNT; i++) {
        nvar[i] = 0;
        int n = SFX_DEFS[i].variants < MAX_VARIANTS ? SFX_DEFS[i].variants : MAX_VARIANTS;
        for (int v = 0; v < n; v++) {
            if (n == 1) snprintf(path, sizeof path, "%s%s.wav", dir, SFX_DEFS[i].name);
            else snprintf(path, sizeof path, "%s%s_%d.wav", dir, SFX_DEFS[i].name, v + 1);
            int s = audio_load(path);
            if (s >= 0) { sample[i][nvar[i]++] = s; files++; }
        }
    }
    SDL_Log("Audio: %d sound files from %s", files, dir);
    return files > 0;
}

void sfx_init(Game *g) {
    for (int i = 0; i < SFX_COUNT; i++) last_play[i] = -1e9f;
    muted = g->opt.mute;
    if (g->opt.shot && !g->opt.sound) return; /* screenshot and test runs stay silent */
    if (!audio_init()) return;
    audio_set_master(muted ? 0.0f : 0.8f);
    log_plays = g->opt.sound; /* audio test runs say what they play */
    char dir[1024];
    const char *base = SDL_GetBasePath();
#ifdef __EMSCRIPTEN__
    loaded = try_dir("/sfx/");
#else
    if (base) snprintf(dir, sizeof dir, "%ssfx/", base);
    loaded = (base && try_dir(dir)) || try_dir("assets/sfx/");
#ifdef FC_SFX_DIR
    if (!loaded) loaded = try_dir(FC_SFX_DIR "/");
#endif
#endif
    (void)base;
    (void)dir;
    if (!loaded) SDL_Log("Audio: no sound files found (run `npm run sfx` to make them)");
    music_load();
}

void sfx_shutdown(void) { audio_shutdown(); }

void sfx_toggle_mute(void) {
    muted = !muted;
    audio_set_master(muted ? 0.0f : 0.8f);
}
bool sfx_muted(void) { return muted; }

/* ---------- playing ---------- */

/* The mix: how loud each sound is next to the others, and how soon it may repeat. */
static float base_vol(int id) {
    switch (id) {
    case SFX_STEP_STONE: case SFX_STEP_DIRT: case SFX_STEP_WOOD: case SFX_STEP_WATER: case SFX_STEP_SNOW: return 0.28f;
    case SFX_MENU_MOVE: return 0.25f;
    case SFX_MENU_OPEN: case SFX_MENU_CLOSE: case SFX_MAP_OPEN: return 0.4f;
    case SFX_PICKUP_GOLD: return 0.45f;
    case SFX_SLIME_HOP: return 0.35f;
    case SFX_ARROW_WALL: case SFX_HIT_WALL: return 0.5f;
    case SFX_EXPLOSION: case SFX_METEOR_IMPACT: case SFX_NAMED_APPEAR: case SFX_NAMED_DEATH: case SFX_GATE_OPEN: return 1.0f;
    case SFX_SPIKES_RATTLE: return 0.35f;
    default: return 0.75f;
    }
}

static float min_gap(int id) {
    switch (id) {
    case SFX_CRAWLER_ALERT: case SFX_BRUTE_ALERT: case SFX_WISP_ALERT: case SFX_HORNED_ALERT: case SFX_SKELETON_ALERT:
    case SFX_SLIME_ALERT: case SFX_BAT_ALERT: case SFX_SPIDER_ALERT: case SFX_WRAITH_ALERT: case SFX_GOLEM_ALERT:
    case SFX_SHAMAN_ALERT: case SFX_BLOATER_ALERT: case SFX_EYE_ALERT: case SFX_KNIGHT_ALERT: case SFX_WORM_ALERT:
    case SFX_MIMIC_ALERT: case SFX_NECRO_ALERT: case SFX_HARPY_ALERT: case SFX_HOUND_ALERT: case SFX_TROLL_ALERT: return 0.5f;
    case SFX_CRAWLER_HURT: case SFX_BRUTE_HURT: case SFX_WISP_HURT: case SFX_HORNED_HURT: case SFX_SKELETON_HURT:
    case SFX_SLIME_HURT: case SFX_BAT_HURT: case SFX_SPIDER_HURT: case SFX_WRAITH_HURT: case SFX_GOLEM_HURT:
    case SFX_SHAMAN_HURT: case SFX_BLOATER_HURT: case SFX_EYE_HURT: case SFX_KNIGHT_HURT: case SFX_WORM_HURT:
    case SFX_MIMIC_HURT: case SFX_NECRO_HURT: case SFX_HARPY_HURT: case SFX_HOUND_HURT: case SFX_TROLL_HURT: return 0.12f;
    case SFX_PLAYER_HURT: return 0.25f;
    case SFX_PICKUP_GOLD: return 0.08f;
    case SFX_PLACE_ENTER: return 4.0f;
    case SFX_SPELL_FIZZLE: return 0.4f;
    case SFX_STORM_STRIKE: return 0.5f;
    case SFX_CHAIN_ZAP: case SFX_EXPLOSION: return 0.1f;
    default: return 0.045f;
    }
}

static bool ready(int id) { return loaded && id >= 0 && id < SFX_COUNT && nvar[id] > 0; }

static int pick(int id) { return sample[id][nvar[id] > 1 ? (int)(frand() * nvar[id]) % nvar[id] : 0]; }

static bool may_play(int id) {
    if (!ready(id) || now - last_play[id] < min_gap(id)) return false;
    last_play[id] = now;
    if (log_plays) SDL_Log("sfx %6.2f %s", now, SFX_DEFS[id].name);
    return true;
}

/* Loudness and pan of a point for one listener. */
static float hear(const PlayerSlot *ps, vec3 pos, float range, float *pan) {
    vec3 eye = player_eye(&ps->pl);
    vec3 d = v3_sub(pos, eye);
    float dist = v3_len(d);
    if (dist > range) return 0.0f;
    float g = 1.0f / (1.0f + fmaxf(0.0f, dist - 1.5f) * 0.18f);
    g *= 1.0f - (dist / range) * (dist / range); /* reaches 0 at the edge of the range */
    vec3 right = v3(-sinf(ps->pl.yaw), 0, cosf(ps->pl.yaw));
    *pan = dist > 0.3f ? v3_dot(v3_scale(d, 1.0f / dist), right) * 0.8f : 0.0f;
    return g;
}

/* The listener who hears pos best (split screen: each sound once, for the nearest). */
static float best_listener(Game *g, vec3 pos, float range, float *pan) {
    float best = 0.0f;
    *pan = 0.0f;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        float p, v = hear(ps, pos, range, &p);
        if (v > best) { best = v; *pan = g->nplayers > 1 ? p * 0.5f : p; }
    }
    return best;
}

void sfx_at(Game *g, int id, vec3 pos, float vol) {
    if (!ready(id)) return;
    float pan, v = best_listener(g, pos, 48.0f, &pan);
    if (v < 0.01f || !may_play(id)) return;
    audio_play(pick(id), vol * base_vol(id) * v, pan, 0.94f + frand() * 0.12f, false);
}

void sfx_player(Game *g, const PlayerSlot *ps, int id, float vol) {
    (void)g;
    (void)ps;
    if (!may_play(id)) return;
    audio_play(pick(id), vol * base_vol(id), 0.0f, 0.95f + frand() * 0.1f, false);
}

void sfx_player_pitch(Game *g, const PlayerSlot *ps, int id, float vol, float pitch) {
    (void)g;
    (void)ps;
    if (!may_play(id)) return;
    audio_play(pick(id), vol * base_vol(id), 0.0f, pitch * (0.96f + frand() * 0.08f), false);
}

void sfx_ui(int id, float vol) {
    if (!may_play(id)) return;
    audio_play(pick(id), vol * base_vol(id), 0.0f, 1.0f, false);
}

static const int MON_SFX[MON_TYPE_COUNT][4] = {
    { SFX_CRAWLER_ALERT, SFX_CRAWLER_ATTACK, SFX_CRAWLER_HURT, SFX_CRAWLER_DEATH },
    { SFX_BRUTE_ALERT, SFX_BRUTE_ATTACK, SFX_BRUTE_HURT, SFX_BRUTE_DEATH },
    { SFX_WISP_ALERT, SFX_WISP_ATTACK, SFX_WISP_HURT, SFX_WISP_DEATH },
    { SFX_HORNED_ALERT, SFX_HORNED_ATTACK, SFX_HORNED_HURT, SFX_HORNED_DEATH },
    { SFX_SKELETON_ALERT, SFX_SKELETON_ATTACK, SFX_SKELETON_HURT, SFX_SKELETON_DEATH },
    { SFX_SLIME_ALERT, SFX_SLIME_ATTACK, SFX_SLIME_HURT, SFX_SLIME_DEATH },
    { SFX_BAT_ALERT, SFX_BAT_ATTACK, SFX_BAT_HURT, SFX_BAT_DEATH },
    { SFX_SPIDER_ALERT, SFX_SPIDER_ATTACK, SFX_SPIDER_HURT, SFX_SPIDER_DEATH },
    { SFX_WRAITH_ALERT, SFX_WRAITH_ATTACK, SFX_WRAITH_HURT, SFX_WRAITH_DEATH },
    { SFX_GOLEM_ALERT, SFX_GOLEM_ATTACK, SFX_GOLEM_HURT, SFX_GOLEM_DEATH },
    { SFX_SHAMAN_ALERT, SFX_SHAMAN_ATTACK, SFX_SHAMAN_HURT, SFX_SHAMAN_DEATH },
    { SFX_BLOATER_ALERT, SFX_BLOATER_ATTACK, SFX_BLOATER_HURT, SFX_BLOATER_DEATH },
    { SFX_EYE_ALERT, SFX_EYE_ATTACK, SFX_EYE_HURT, SFX_EYE_DEATH },
    { SFX_KNIGHT_ALERT, SFX_KNIGHT_ATTACK, SFX_KNIGHT_HURT, SFX_KNIGHT_DEATH },
    { SFX_WORM_ALERT, SFX_WORM_ATTACK, SFX_WORM_HURT, SFX_WORM_DEATH },
    { SFX_MIMIC_ALERT, SFX_MIMIC_ATTACK, SFX_MIMIC_HURT, SFX_MIMIC_DEATH },
    { SFX_NECRO_ALERT, SFX_NECRO_ATTACK, SFX_NECRO_HURT, SFX_NECRO_DEATH },
    { SFX_HARPY_ALERT, SFX_HARPY_ATTACK, SFX_HARPY_HURT, SFX_HARPY_DEATH },
    { SFX_HOUND_ALERT, SFX_HOUND_ATTACK, SFX_HOUND_HURT, SFX_HOUND_DEATH },
    { SFX_TROLL_ALERT, SFX_TROLL_ATTACK, SFX_TROLL_HURT, SFX_TROLL_DEATH },
};

void sfx_monster(Game *g, const Monster *m, int what) {
    if (m->type >= MON_TYPE_COUNT || what < 0 || what > 3) return;
    int id = MON_SFX[m->type][what];
    if (!ready(id)) return;
    vec3 c = v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0));
    float pan, v = best_listener(g, c, 48.0f, &pan);
    if (v < 0.01f || !may_play(id)) return;
    /* big ones sound deeper, small slimes higher */
    float pitch = (1.0f / sqrtf(fmaxf(0.4f, m->scale))) * (m->elite ? 0.88f : 1.0f) * (0.94f + frand() * 0.12f);
    float vol = base_vol(id) * v * (m->elite || m->named_kind ? 1.15f : 1.0f);
    audio_play(pick(id), vol, pan, pitch, false);
}

int sfx_element_cast(int e) {
    static const int T[EL_COUNT] = { SFX_CAST_FIRE, SFX_CAST_FROST, SFX_CAST_LIGHTNING, SFX_CAST_POISON,
                                     SFX_CAST_ARCANE, SFX_CAST_HOLY, SFX_CAST_SHADOW, SFX_CAST_EARTH };
    return e >= 0 && e < EL_COUNT ? T[e] : SFX_CAST_ARCANE;
}

int sfx_element_impact(int e) {
    static const int T[EL_COUNT] = { SFX_IMPACT_FIRE, SFX_IMPACT_FROST, SFX_IMPACT_LIGHTNING, SFX_IMPACT_POISON,
                                     SFX_IMPACT_ARCANE, SFX_IMPACT_HOLY, SFX_IMPACT_SHADOW, SFX_IMPACT_EARTH };
    return e >= 0 && e < EL_COUNT ? T[e] : SFX_BOLT_HIT;
}

/* ---------- loops: ambience and things nearby ---------- */

typedef struct Loop {
    int id;
    unsigned voice;
    float vol, want, pan;
} Loop;

static const int STRATUM_LOOP[ST_COUNT] = {
    [ST_CORE] = SFX_LOOP_CORE, [ST_MAW] = SFX_LOOP_INFERNO, [ST_IRONHELL] = SFX_LOOP_INFERNO,
    [ST_INFERNO] = SFX_LOOP_INFERNO, [ST_CRYSTAL] = SFX_LOOP_ASTRAL, [ST_LIMBO] = SFX_LOOP_LIMBO,
    [ST_FUNGAL] = SFX_LOOP_ROOTS, [ST_OSSUARY] = SFX_LOOP_LIMBO, [ST_ROOTS] = SFX_LOOP_ROOTS,
    [ST_MINES] = SFX_LOOP_MINES, [ST_DUNGEON] = SFX_LOOP_DUNGEON,
    [ST_CASTLE] = SFX_LOOP_CASTLE, [ST_SPIRES] = SFX_LOOP_SPIRES, [ST_AERIE] = SFX_LOOP_SPIRES,
    [ST_CLOUDS] = SFX_LOOP_CLOUDS, [ST_GARDENS] = SFX_LOOP_CLOUDS, [ST_HEAVEN] = SFX_LOOP_HEAVEN,
    [ST_SUNCOURT] = SFX_LOOP_HEAVEN, [ST_STORM] = SFX_LOOP_SPIRES, [ST_ORRERY] = SFX_LOOP_ASTRAL,
    [ST_ASTRAL] = SFX_LOOP_ASTRAL,
};

enum { LP_FIRE, LP_LAVA, LP_WATER, LP_FLAMES, LP_POINT_COUNT };
static const int POINT_LOOP[LP_POINT_COUNT] = { SFX_LOOP_FIRE, SFX_LOOP_LAVA, SFX_LOOP_WATER, SFX_LOOP_FLAMES };
static const float POINT_VOL[LP_POINT_COUNT] = { 0.45f, 0.7f, 0.45f, 0.8f };

#define AMB_COUNT (ST_COUNT + 1) /* strata and the outside */
static Loop amb[AMB_COUNT];
static Loop point[LP_POINT_COUNT];
static float scan_t, stinger_t = 6.0f;
static int listener_stratum = ST_DUNGEON;
static bool listener_outside;

static void loop_drive(Loop *l, float dt, float fade) {
    float k = fminf(1.0f, dt * fade);
    l->vol += (l->want - l->vol) * k;
    if (l->vol > 0.004f && !l->voice && ready(l->id)) l->voice = audio_play(pick(l->id), l->vol, l->pan, 1.0f, true);
    if (l->voice) {
        if (l->vol <= 0.004f && l->want <= 0.0f) { audio_stop(l->voice); l->voice = 0; l->vol = 0; }
        else audio_set(l->voice, l->vol, l->pan);
    }
}

static int point_kind(Block b) {
    if (b == B_LAVA) return LP_LAVA;
    if (b == B_WATER) return LP_WATER;
    if (b < B_COUNT && BLOCKS[b].fire) return LP_FIRE;
    return -1;
}

/* The nearest fire, lava and water to the listeners, and the flame vents that burn. */
static void scan_points(Game *g) {
    float best[LP_POINT_COUNT], pan[LP_POINT_COUNT];
    for (int k = 0; k < LP_POINT_COUNT; k++) best[k] = 0, pan[k] = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        vec3 eye = player_eye(&ps->pl);
        int ex = (int)floorf(eye.x), ey = (int)floorf(eye.y), ez = (int)floorf(eye.z);
        const int R = 8;
        float nd[LP_POINT_COUNT] = { 1e9f, 1e9f, 1e9f, 1e9f };
        vec3 np[LP_POINT_COUNT];
        for (int y = ey - 4; y <= ey + 3; y++)
            for (int z = ez - R; z <= ez + R; z++)
                for (int x = ex - R; x <= ex + R; x++) {
                    bool known;
                    Block b = chunkmap_block(&g->map, x, y, z, &known);
                    int k = known ? point_kind(b) : -1;
                    if (k < 0) continue;
                    float dx = x + 0.5f - eye.x, dy = y + 0.5f - eye.y, dz = z + 0.5f - eye.z;
                    float d2 = dx * dx + dy * dy + dz * dz;
                    if (d2 < nd[k]) { nd[k] = d2; np[k] = v3(x + 0.5f, y + 0.5f, z + 0.5f); }
                }
        vec3 vents[MAX_DEVICES];
        int nv = mech_burning_flames(g, vents, MAX_DEVICES);
        for (int d = 0; d < nv; d++) {
            float dd = v3_len(v3_sub(vents[d], eye));
            if (dd * dd < nd[LP_FLAMES]) { nd[LP_FLAMES] = dd * dd; np[LP_FLAMES] = vents[d]; }
        }
        for (int k = 0; k < LP_POINT_COUNT; k++) {
            if (nd[k] > 1e8f) continue;
            float p, v = hear(ps, np[k], k == LP_LAVA ? 14.0f : k == LP_FLAMES ? 24.0f : 11.0f, &p);
            if (v > best[k]) { best[k] = v; pan[k] = g->nplayers > 1 ? p * 0.5f : p; }
        }
    }
    for (int k = 0; k < LP_POINT_COUNT; k++) {
        point[k].want = best[k] * POINT_VOL[k];
        point[k].pan = pan[k];
    }
}

/* Where the first living listener is: its stratum, or outside. */
static void listener_place(Game *g) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        vec3 eye = player_eye(&ps->pl);
        int cx = floordiv((int)floorf(eye.x), CHUNK), cy = floordiv((int)floorf(eye.y), CHUNK), cz = floordiv((int)floorf(eye.z), CHUNK);
        Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
        ChunkInfo ci;
        if (c && c->state == CH_READY) ci = c->info;
        else world_chunk_info(&g->world, cx, cy, cz, &ci);
        listener_outside = ci.arch == ARCH_EXTERIOR;
        listener_stratum = theme_stratum(ci.theme);
        if (listener_stratum < 0 || listener_stratum >= ST_COUNT) listener_stratum = ST_DUNGEON;
        return;
    }
}

/* Now and then a sound from somewhere in the dark, fitting the place. */
static void stingers(Game *g, float dt) {
    stinger_t -= dt;
    if (stinger_t > 0) return;
    stinger_t = 9.0f + frand() * 18.0f;
    static const int DEEP[] = { SFX_AMB_DRIP, SFX_AMB_RUMBLE, SFX_AMB_CHAINS, SFX_AMB_SCREAM, SFX_AMB_DOOR, SFX_AMB_GROWL, SFX_AMB_ROCKS };
    static const int MINES[] = { SFX_AMB_ROCKS, SFX_AMB_DRIP, SFX_AMB_RUMBLE, SFX_AMB_GROWL };
    static const int HELL[] = { SFX_AMB_RUMBLE, SFX_AMB_SCREAM, SFX_AMB_GROWL, SFX_AMB_WHISPER };
    static const int GHOST[] = { SFX_AMB_WHISPER, SFX_AMB_SCREAM, SFX_AMB_BELL, SFX_AMB_CHAINS };
    static const int HIGH[] = { SFX_AMB_BELL, SFX_AMB_DOOR, SFX_AMB_CHAINS, SFX_AMB_WHISPER };
    static const int SKY[] = { SFX_AMB_BELL, SFX_AMB_WHISPER };
    static const int OUT[] = { SFX_AMB_GROWL, SFX_AMB_BELL, SFX_AMB_ROCKS };
    const int *set = DEEP;
    int n = 7;
    if (listener_outside) set = OUT, n = 3;
    else switch (listener_stratum) {
    case ST_MINES: case ST_ROOTS: case ST_FUNGAL: case ST_CRYSTAL: set = MINES; n = 4; break;
    case ST_CORE: case ST_INFERNO: case ST_IRONHELL: case ST_MAW: set = HELL; n = 4; break;
    case ST_LIMBO: case ST_OSSUARY: set = GHOST; n = 4; break;
    case ST_CASTLE: case ST_SPIRES: case ST_AERIE: case ST_STORM: set = HIGH; n = 4; break;
    case ST_CLOUDS: case ST_GARDENS: case ST_HEAVEN: case ST_SUNCOURT: case ST_ORRERY: case ST_ASTRAL: set = SKY; n = 2; break;
    default: break;
    }
    int id = set[(int)(frand() * n) % n];
    PlayerSlot *ps = NULL;
    for (int i = 0; i < MAX_PLAYERS && !ps; i++)
        if (g->players[i].active) ps = &g->players[i];
    if (!ps || !ready(id)) return;
    /* somewhere around, not too close: carried far, muffled by the distance */
    float a = frand() * 2.0f * PI_F;
    float pan = sinf(a - ps->pl.yaw) * 0.7f;
    audio_play(pick(id), (0.25f + frand() * 0.25f) * base_vol(id), pan, 0.85f + frand() * 0.2f, false);
}

/* ---------- footsteps, jumps, menus ---------- */

typedef struct Watch {
    bool known, on_ground, menu, map;
    float bob, vy;
    int tab, inv, book, skill, points;
    bool dead;
} Watch;
static Watch watch[MAX_PLAYERS];

static int step_sound(Game *g, PlayerSlot *ps) {
    vec3 f = ps->pl.pos;
    bool known;
    Block at = chunkmap_block(&g->map, (int)floorf(f.x), (int)floorf(f.y + 0.1f), (int)floorf(f.z), &known);
    if (at == B_WATER) return SFX_STEP_WATER;
    Block b = chunkmap_block(&g->map, (int)floorf(f.x), (int)floorf(f.y - 0.1f), (int)floorf(f.z), &known);
    switch (b) {
    case B_WATER: return SFX_STEP_WATER;
    case B_DIRT: case B_GRASS: case B_ROOTS: case B_FUNGUS: return SFX_STEP_DIRT;
    case B_WOOD: return SFX_STEP_WOOD;
    case B_ICE: return SFX_STEP_SNOW;
    default: return SFX_STEP_STONE;
    }
}

static void watch_player(Game *g, PlayerSlot *ps, float dt) {
    Watch *w = &watch[ps->index];
    Player *pl = &ps->pl;
    if (!w->known) {
        *w = (Watch){ true, pl->on_ground, ps->show_skills, ps->show_map, pl->bob, pl->vel.y,
                      ps->menu_tab, ps->inv_sel, ps->book_sel, ps->skill_sel, ps->st.points, ps->st.dead };
        return;
    }
    if (!ps->st.dead && !pl->fly) {
        /* a step every half cycle of the head bob */
        if (pl->on_ground && floorf(pl->bob / PI_F) != floorf(w->bob / PI_F)) {
            float speed = sqrtf(pl->vel.x * pl->vel.x + pl->vel.z * pl->vel.z);
            float vol = clampf(0.5f + speed * 0.08f, 0.5f, 1.2f) * (pl->stance == STANCE_STAND ? 1.0f : 0.5f);
            int id = step_sound(g, ps);
            if (ready(id)) audio_play(pick(id), vol * base_vol(id), 0.0f, 0.92f + frand() * 0.16f, false);
            if (log_plays) SDL_Log("sfx %6.2f %s", now, SFX_DEFS[id].name);
        }
        if (w->on_ground && !pl->on_ground && pl->vel.y > 6.0f) sfx_player(g, ps, SFX_JUMP, 0.6f);
        if (!w->on_ground && pl->on_ground && w->vy < -7.0f) sfx_player(g, ps, SFX_LAND, clampf(-w->vy / 14.0f, 0.4f, 1.0f));
    }
    if (w->dead && !ps->st.dead) sfx_player(g, ps, SFX_RESPAWN, 0.8f);
    /* menus */
    if (ps->show_skills != w->menu) sfx_ui(ps->show_skills ? SFX_MENU_OPEN : SFX_MENU_CLOSE, 1.0f);
    else if (ps->show_skills && (ps->menu_tab != w->tab)) sfx_ui(SFX_MENU_OPEN, 0.7f);
    else if (ps->show_skills && (ps->inv_sel != w->inv || ps->book_sel != w->book || ps->skill_sel != w->skill))
        sfx_ui(SFX_MENU_MOVE, 1.0f);
    if (ps->show_map != w->map) sfx_ui(ps->show_map ? SFX_MAP_OPEN : SFX_MENU_CLOSE, 1.0f);
    if (ps->st.points < w->points) sfx_ui(SFX_SKILL_BUY, 1.0f);
    w->on_ground = pl->on_ground;
    w->bob = pl->bob;
    w->vy = pl->vel.y;
    w->menu = ps->show_skills;
    w->map = ps->show_map;
    w->tab = ps->menu_tab;
    w->inv = ps->inv_sel;
    w->book = ps->book_sel;
    w->skill = ps->skill_sel;
    w->points = ps->st.points;
    w->dead = ps->st.dead;
    (void)dt;
}

void sfx_update(Game *g, float dt) {
    now += dt;
    if (!loaded) return;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g->players[i].active) watch_player(g, &g->players[i], dt);
        else watch[i].known = false;
    }
    listener_place(g);
    for (int i = 0; i < AMB_COUNT; i++) {
        amb[i].id = i < ST_COUNT ? STRATUM_LOOP[i] : SFX_LOOP_EXTERIOR;
        bool here = listener_outside ? i == ST_COUNT : i == listener_stratum;
        amb[i].want = here ? 0.55f : 0.0f;
        amb[i].pan = 0;
        loop_drive(&amb[i], dt, 0.6f); /* slow crossfade between places */
    }
    scan_t -= dt;
    if (scan_t <= 0) { scan_t = 0.2f; scan_points(g); }
    for (int k = 0; k < LP_POINT_COUNT; k++) {
        point[k].id = POINT_LOOP[k];
        loop_drive(&point[k], dt, 4.0f);
    }
    stingers(g, dt);
}

/* ---------- music ---------- */

#define MUSIC_VOL 0.42f

typedef struct Track {
    int sample;   /* -1: missing */
    unsigned voice;
    float vol, want;
} Track;

static Track tracks[MUS_COUNT];
static bool music_ready, music_enabled = true;
static float explore_gap = 20.0f; /* silence before the exploration music (again) */
static float battle_hold;

static bool music_try_dir(const char *dir) {
    int n = 0;
    for (int i = 0; i < MUS_COUNT; i++) {
        char path[1200];
        snprintf(path, sizeof path, "%s%s.wav", dir, MUSIC_NAMES[i]);
        tracks[i].sample = audio_load(path);
        n += tracks[i].sample >= 0;
    }
    if (n) SDL_Log("Audio: %d music pieces from %s", n, dir);
    return n > 0;
}

static void music_load(void) {
    for (int i = 0; i < MUS_COUNT; i++) tracks[i] = (Track){ -1, 0, 0, 0 };
    if (!audio_ready()) return;
#ifdef __EMSCRIPTEN__
    music_ready = music_try_dir("/music/");
#else
    char dir[1024];
    const char *base = SDL_GetBasePath();
    if (base) snprintf(dir, sizeof dir, "%smusic/", base);
    music_ready = (base && music_try_dir(dir)) || music_try_dir("assets/music/");
#ifdef FC_MUSIC_DIR
    if (!music_ready) music_ready = music_try_dir(FC_MUSIC_DIR "/");
#endif
#endif
    if (!music_ready) SDL_Log("Audio: no music found (run `npm run music` to make it)");
}

void music_toggle(void) { music_enabled = !music_enabled; }
bool music_on(void) { return music_enabled; }

/* What the players face: a named enemy close by, or monsters on the hunt. */
static void music_threat(Game *g, bool *champion, bool *battle) {
    *champion = *battle = false;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        const Monster *m = &g->mons[i];
        if (!m->alive || !m->awake) continue;
        for (int k = 0; k < MAX_PLAYERS; k++) {
            const PlayerSlot *ps = &g->players[k];
            if (!ps->active || ps->st.dead) continue;
            float d = v3_len(v3_sub(m->pos, ps->pl.pos));
            if (m->named_kind && d < 35.0f) *champion = true;
            if (d < 18.0f) *battle = true;
        }
    }
}

void music_update(Game *g, float dt, bool menu) {
    if (!music_ready) return;
    int want = -1;
    bool looping = true;
    if (menu) want = MUS_GATE;
    else {
        bool champion, battle;
        music_threat(g, &champion, &battle);
        if (battle || champion) battle_hold = 6.0f; /* stays a little after the last foe */
        else battle_hold -= dt;
        listener_place(g);
        if (champion) want = MUS_CHAMPION;
        else if (battle_hold > 0) want = MUS_BATTLE;
        else if (listener_outside) want = MUS_GATE;
        else {
            /* exploring: a piece now and then, silence between */
            int piece = listener_stratum > ST_DUNGEON ? MUS_HEIGHTS : MUS_DEEP;
            int other = piece == MUS_HEIGHTS ? MUS_DEEP : MUS_HEIGHTS;
            looping = false;
            if (tracks[piece].voice || tracks[other].voice) want = piece; /* one is on: carry on (or cross over) */
            else if ((explore_gap -= dt) <= 0) {
                want = piece;
                explore_gap = 70.0f + frand() * 80.0f;
            }
        }
    }
    if (!music_enabled) want = -1;
    for (int i = 0; i < MUS_COUNT; i++) {
        Track *t = &tracks[i];
        if (t->sample < 0) continue;
        /* a piece that played to its end: done until the next gap is over */
        if (t->voice && !audio_playing(t->voice)) { t->voice = 0; t->vol = 0; }
        t->want = i == want ? MUSIC_VOL * (i == MUS_BATTLE || i == MUS_CHAMPION ? 1.15f : 1.0f) : 0.0f;
        if (t->want > 0 && !t->voice) {
            bool loop = looping || i == MUS_GATE;
            t->voice = audio_play(t->sample, 0.01f, 0.0f, 1.0f, loop);
            t->vol = 0.01f;
            if (log_plays) SDL_Log("music %6.2f start %s", now, MUSIC_NAMES[i]);
        }
        /* fade: fights come in fast, everything else drifts */
        float rate = t->want > t->vol ? (i == MUS_BATTLE || i == MUS_CHAMPION ? 0.8f : 0.15f) : 0.12f;
        if (t->vol < t->want) t->vol = fminf(t->want, t->vol + rate * dt);
        else t->vol = fmaxf(t->want, t->vol - rate * dt);
        if (t->voice) {
            if (t->vol <= 0.0005f && t->want <= 0) {
                if (log_plays) SDL_Log("music %6.2f stop %s", now, MUSIC_NAMES[i]);
                audio_stop(t->voice);
                t->voice = 0;
                /* a fight interrupted the exploration music: it comes back after a while */
                if (i == MUS_DEEP || i == MUS_HEIGHTS) explore_gap = fminf(explore_gap, 25.0f);
            } else audio_set(t->voice, t->vol, 0.0f);
        }
    }
}
