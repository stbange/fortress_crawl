/* The world goes on while you are away (docs/SIMULATION.md, docs/SIMULATION_PLAN.md step 1).
 *
 * The simulation only runs near the players. Every chunk a player has been near keeps the time
 * it was last seen (the world's play time). When a player comes near it again after a while,
 * it catches up on what it missed: fires still burning have burnt out (spreading a little
 * first), and growth and withering run for the time away, compressed (bio_catch_up). An area
 * never visited needs nothing: it is as the world made it. The catch-up reaches two hours. */
#include "game/game.h"
#include "world/pocket.h"

#define REGIONS 4096          /* chunks remembered; the longest unseen are forgotten first */
#define AWAY_MIN 60.0f        /* seconds away before anything is caught up */
#define AWAY_MAX 7200.0f      /* at most two hours */

typedef struct Region { int cx, cy, cz; float last_seen; bool used; } Region;
static Region regions[REGIONS];
static int nregions;
static float check_t;
static float banner_t;

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
            *r = (Region){ cx, cy, cz, t, true };
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
    for (int i = 0; i < m; i++) region_add(keep[i].cx, keep[i].cy, keep[i].cz, keep[i].last_seen);
}

void away_reset(void) {
    memset(regions, 0, sizeof regions);
    nregions = 0;
    check_t = 0;
    banner_t = 0;
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

int away_list(int *xyz, float *t, int max) {
    int n = 0;
    for (int i = 0; i < REGIONS && n < max; i++)
        if (regions[i].used) {
            xyz[n * 3] = regions[i].cx; xyz[n * 3 + 1] = regions[i].cy; xyz[n * 3 + 2] = regions[i].cz;
            t[n++] = regions[i].last_seen;
        }
    return n;
}

void away_restore(int cx, int cy, int cz, float t) {
    Region *r = region_find(cx, cy, cz);
    if (r) r->last_seen = t;
    else region_add(cx, cy, cz, t);
}

/* For the demo: forget that a stretch of time went by everywhere except as last seen. */
void away_pretend(float seconds) {
    for (int i = 0; i < REGIONS; i++)
        if (regions[i].used) regions[i].last_seen -= seconds;
}
