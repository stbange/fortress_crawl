/* The world goes on while you are away (docs/SIMULATION.md, docs/SIMULATION_PLAN.md step 1).
 *
 * The simulation only runs near the players. Every chunk a player has been near keeps the time
 * it was last seen (the world's play time). When a player comes near it again after a while,
 * it catches up on what it missed: fires still burning have burnt out (spreading a little
 * first), and growth and withering run for the time away, compressed (bio_catch_up). An area
 * never visited needs nothing: it is as the world made it. The catch-up reaches two hours. */
#include "game/game.h"
#include "world/pocket.h"
#include "world/eco.h"

#define REGIONS 4096          /* chunks remembered; the longest unseen are forgotten first */
#define AWAY_MIN 60.0f        /* seconds away before anything is caught up */
#define AWAY_MAX 7200.0f      /* at most two hours */

typedef struct Region { int cx, cy, cz; float last_seen; bool used; Eco eco; } Region;
static Region regions[REGIONS];
static int nregions;
static float check_t;
static float banner_t;
static float eco_t;

static u32 rhash(int cx, int cy, int cz) { return ((u32)cx * 73856093u ^ (u32)cy * 19349663u ^ (u32)cz * 83492791u) & (REGIONS - 1); }

static Region *region_find(int cx, int cy, int cz) {
    u32 i = rhash(cx, cy, cz);
    for (int k = 0; k < 64; k++, i = (i + 1) & (REGIONS - 1)) {
        Region *r = &regions[i];
        if (!r->used) return NULL;
        if (r->cx == cx && r->cy == cy && r->cz == cz) return r;
    }
    return NULL;
}

/* (open addressing without deletion: when it fills up, it is rebuilt without the oldest half) */
static void regions_prune(void);

static Region *region_add(int cx, int cy, int cz, float t) {
    if (nregions > REGIONS * 3 / 4) regions_prune();
    u32 i = rhash(cx, cy, cz);
    for (int k = 0; k < 64; k++, i = (i + 1) & (REGIONS - 1)) {
        Region *r = &regions[i];
        if (!r->used) {
            *r = (Region){ cx, cy, cz, t, true, { 0 } };
            nregions++;
            return r;
        }
    }
    return NULL;
}

static int cmp_float(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

static void regions_prune(void) {
    static Region keep[REGIONS];
    static float times[REGIONS];
    int n = 0;
    for (int i = 0; i < REGIONS; i++)
        if (regions[i].used) times[n++] = regions[i].last_seen;
    qsort(times, (size_t)n, sizeof(float), cmp_float);
    float cut = times[n / 2];
    int m = 0;
    for (int i = 0; i < REGIONS; i++)
        if (regions[i].used && regions[i].last_seen >= cut) keep[m++] = regions[i];
    memset(regions, 0, sizeof regions);
    nregions = 0;
    for (int i = 0; i < m; i++) {
        Region *r = region_add(keep[i].cx, keep[i].cy, keep[i].cz, keep[i].last_seen);
        if (r) r->eco = keep[i].eco;
    }
}

void away_reset(void) {
    memset(regions, 0, sizeof regions);
    nregions = 0;
    eco_t = 0;
    check_t = 0;
    banner_t = 0;
}

/* ---------- populations (eco.c) ---------- */

#define ECO_HOURS_PER_S (1.0f / 600.0f) /* ten minutes of play are an hour for the animals */

/* Counts the plants and flowers of a chunk, and keeps a few of their cells to graze. */
static int count_plants(Game *g, int cx, int cy, int cz, float *plants, float *flowers, int *cells, int max) {
    Chunk *c = chunkmap_get(&g->map, cx, cy, cz);
    *plants = *flowers = 0;
    int n = 0;
    if (!c || c->state != CH_READY) return -1;
    u64 h = mix64((u64)(cx * 73856093) ^ (u64)(cz * 19349663) ^ (u64)(g->time * 10));
    for (int i = 0; i < CHUNK_VOL; i++) {
        Block b = c->blocks[i];
        if (b == B_GRASS || b == B_FLOWER) {
            *plants += 1;
            if (b == B_FLOWER) *flowers += 1;
            if (n < max) cells[n++] = i;
            else if ((h = mix64(h)) % 8 == 0) cells[h % (u64)max] = i;
        } else if (b == B_MOSS || b == B_FUNGUS || b == B_VINE || b == B_ROOTS) *plants += 0.5f;
    }
    return n;
}

/* Grazers eat what grows: `eaten` cells of grass and flowers are gone. */
static void graze(Game *g, int cx, int cy, int cz, float eaten, const int *cells, int ncells) {
    int n = MINI((int)eaten, MINI(ncells, 40));
    for (int k = 0; k < n; k++) {
        int i = cells[k];
        int x = cx * CHUNK + i % CHUNK, z = cz * CHUNK + (i / CHUNK) % CHUNK, y = cy * CHUNK + i / (CHUNK * CHUNK);
        if (!dig_protected(g, x, y, z)) chunkmap_edit(&g->map, x, y, z, B_AIR, 0, 0);
    }
}

static void eco_region(Game *g, Region *r, float seconds) {
    static int cells[64];
    float plants, flowers;
    int nc = count_plants(g, r->cx, r->cy, r->cz, &plants, &flowers, cells, 64);
    if (nc < 0) return;
    if (!r->eco.init) {
        Chunk *c = chunkmap_get(&g->map, r->cx, r->cy, r->cz);
        eco_start(&r->eco, plants, flowers, monster_hunts_here(theme_stratum(c->info.theme)));
        return;
    }
    r->eco.plants = plants;
    r->eco.flowers = flowers;
    float eaten = eco_step(&r->eco, seconds * ECO_HOURS_PER_S);
    if (eaten >= 1.0f) graze(g, r->cx, r->cy, r->cz, eaten, cells, nc);
}

/* The numbers of a chunk, or NULL if nobody has been near it. */
Eco *eco_at(int cx, int cy, int cz) {
    Region *r = region_find(cx, cy, cz);
    return r && r->eco.init ? &r->eco : NULL;
}

/* Something died or was killed at p: a grazer taken (by you or a hound), or a body left. */
void eco_note(vec3 p, int what) {
    Eco *e = eco_at(floordiv((int)floorf(p.x), CHUNK), floordiv((int)floorf(p.y), CHUNK), floordiv((int)floorf(p.z), CHUNK));
    if (!e) return;
    if (what == ECO_GRAZER_TAKEN) { e->grazers = fmaxf(0.0f, e->grazers - 1.0f); e->carrion += 0.5f; }
    else if (what == ECO_BODY) e->carrion += 1.0f;
}

/* How many hounds and bats a chunk's prey feeds, against the usual: 0.5 to 1.5. */
float eco_prey_factor(int cx, int cy, int cz) {
    Eco *e = eco_at(cx, cy, cz);
    if (!e) return 1.0f;
    return clampf(0.5f + 0.5f * e->grazers / fmaxf(0.3f, eco_capacity(e) * 0.6f), 0.5f, 1.5f);
}

/* How many grazers, against the usual: 0 to 2. */
float eco_grazer_factor(int cx, int cy, int cz) {
    Eco *e = eco_at(cx, cy, cz);
    if (!e) return 1.0f;
    return clampf(e->grazers / fmaxf(0.3f, eco_capacity(e) * 0.6f), 0.0f, 2.0f);
}

static void eco_update(Game *g, float dt) {
    eco_t -= dt;
    if (eco_t > 0) return;
    eco_t = 10.0f;
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        int pcx = floordiv((int)floorf(ps->pl.pos.x), CHUNK), pcy = floordiv((int)floorf(ps->pl.pos.y), CHUNK),
            pcz = floordiv((int)floorf(ps->pl.pos.z), CHUNK);
        for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++) {
                Region *r = region_find(pcx + dx, pcy, pcz + dz);
                if (!r || world_is_pocket(r->cx, r->cy, r->cz)) continue;
                eco_region(g, r, 10.0f);
                /* animals wander to the neighbours east and south where there is more to eat */
                Region *e = region_find(r->cx + 1, r->cy, r->cz), *s = region_find(r->cx, r->cy, r->cz + 1);
                if (e && e->eco.init && r->eco.init) eco_wander(&r->eco, &e->eco, 10.0f * ECO_HOURS_PER_S);
                if (s && s->eco.init && r->eco.init) eco_wander(&r->eco, &s->eco, 10.0f * ECO_HOURS_PER_S);
            }
    }
}

/* ---------- catch-up jobs ---------- */

typedef struct Job { int cx, cy, cz; float dt; int phase; int grown0; } Job;
#define MAX_JOBS 64
static Job jobs[MAX_JOBS];
static int njobs;

static void job_add(int cx, int cy, int cz, float dt) {
    for (int i = 0; i < njobs; i++)
        if (jobs[i].cx == cx && jobs[i].cy == cy && jobs[i].cz == cz) { jobs[i].dt = fmaxf(jobs[i].dt, dt); return; }
    if (njobs >= MAX_JOBS) return;
    jobs[njobs++] = (Job){ cx, cy, cz, dt, 0, 0 };
}

int away_pending(void) { return njobs; }

static int grown_total(void) {
    int n = 0;
    for (int b = 0; b < B_COUNT; b++) n += bio_grown(b);
    return n;
}

/* Runs the first job a little; true when it is done. */
static bool job_run(Game *g, Job *j) {
    Chunk *c = chunkmap_get(&g->map, j->cx, j->cy, j->cz);
    if (!c || c->state != CH_READY) return true; /* not loaded any more: it catches up when it is seen again */
    if (j->phase == 0) {
        /* fires that were burning have long burnt out */
        if (!sim_catch_up_fires(g, j->cx, j->cy, j->cz, j->dt)) return false;
        j->grown0 = grown_total();
        bio_catch_up_begin(g, j->cx, j->cy, j->cz, j->dt);
        /* the animals lived on too */
        Region *r = region_find(j->cx, j->cy, j->cz);
        if (r && r->eco.init) eco_region(g, r, j->dt);
        j->phase = 1;
        return false;
    }
    if (!bio_catch_up_run(g, 3000)) return false;
    int grown = grown_total() - j->grown0;
    if (grown > 0) SDL_Log("away: chunk %d %d %d caught up %.0f s: %d cells grew", j->cx, j->cy, j->cz, j->dt, grown);
    /* if it shows, the world says so */
    if (grown >= 12 && g->time > banner_t) {
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *ps = &g->players[k];
            if (!ps->active) continue;
            int pcx = floordiv((int)floorf(ps->pl.pos.x), CHUNK), pcz = floordiv((int)floorf(ps->pl.pos.z), CHUNK);
            if (abs(pcx - j->cx) <= 1 && abs(pcz - j->cz) <= 1) {
                game_banner(ps, j->dt > 3000.0f ? "THIS PLACE HAS GROWN OVER SINCE YOU WERE HERE" : "THINGS HAVE GROWN HERE SINCE YOU LEFT");
                banner_t = g->time + 600.0f;
            }
        }
    }
    return true;
}

void away_update(Game *g, float dt) {
    eco_update(g, dt);
    if (njobs && job_run(g, &jobs[0])) {
        memmove(&jobs[0], &jobs[1], sizeof(Job) * (size_t)(njobs - 1));
        njobs--;
    }
    check_t -= dt;
    if (check_t > 0) return;
    check_t = 1.0f;
    float now = g->play_time;
    for (int k = 0; k < MAX_PLAYERS; k++) {
        PlayerSlot *ps = &g->players[k];
        if (!ps->active || ps->st.dead) continue;
        int pcx = floordiv((int)floorf(ps->pl.pos.x), CHUNK), pcy = floordiv((int)floorf(ps->pl.pos.y), CHUNK),
            pcz = floordiv((int)floorf(ps->pl.pos.z), CHUNK);
        for (int dy = -1; dy <= 1; dy++)
            for (int dz = -1; dz <= 1; dz++)
                for (int dx = -1; dx <= 1; dx++) {
                    int cx = pcx + dx, cy = pcy + dy, cz = pcz + dz;
                    if (world_is_pocket(cx, cy, cz)) continue;
                    Region *r = region_find(cx, cy, cz);
                    if (!r) { region_add(cx, cy, cz, now); continue; }
                    float away = now - r->last_seen;
                    if (away > AWAY_MIN) job_add(cx, cy, cz, fminf(away, AWAY_MAX));
                    r->last_seen = now;
                }
    }
}

/* ---------- saving ---------- */

int away_list(int *xyz, float *t, Eco *eco, int max) {
    int n = 0;
    for (int i = 0; i < REGIONS && n < max; i++)
        if (regions[i].used) {
            xyz[n * 3] = regions[i].cx; xyz[n * 3 + 1] = regions[i].cy; xyz[n * 3 + 2] = regions[i].cz;
            if (eco) eco[n] = regions[i].eco;
            t[n++] = regions[i].last_seen;
        }
    return n;
}

void away_restore(int cx, int cy, int cz, float t, const Eco *eco) {
    Region *r = region_find(cx, cy, cz);
    if (r) r->last_seen = t;
    else r = region_add(cx, cy, cz, t);
    if (r && eco) r->eco = *eco;
}

/* For the demo: forget that a stretch of time went by everywhere except as last seen. */
void away_pretend(float seconds) {
    for (int i = 0; i < REGIONS; i++)
        if (regions[i].used) regions[i].last_seen -= seconds;
}
