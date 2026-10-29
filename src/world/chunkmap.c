#include "world/chunkmap.h"

#include "engine/jobs.h"
#include "world/mesher.h"
#include "world/texgen.h"

#define MAX_SCRATCH 16

static MeshScratch *g_scratch[MAX_SCRATCH];
static ChunkMap *g_map; /* job callbacks need it for unpinning */

typedef struct GenJob {
    Job base;
    Chunk *c;
    const World *w;
} GenJob;

typedef struct MeshJob {
    Job base;
    Chunk *c;
    Chunk *nb[27];
    MeshInput in;
    MeshOutput out;
    int slot;
} MeshJob;

static int slot_index(const ChunkMap *m, int cx, int cy, int cz) {
    return floormod(cx, m->dx) + floormod(cy, m->dy) * m->dx + floormod(cz, m->dz) * m->dx * m->dy;
}

static int offset_cmp(const void *a, const void *b) {
    const int *p = a, *q = b;
    float dp = (float)(p[0] * p[0] + p[2] * p[2]) + 2.25f * (float)(p[1] * p[1]);
    float dq = (float)(q[0] * q[0] + q[2] * q[2]) + 2.25f * (float)(q[1] * q[1]);
    return dp < dq ? -1 : (dp > dq ? 1 : 0);
}

void chunkmap_init(ChunkMap *m, const World *w, Renderer *r, int rh, int rv) {
    memset(m, 0, sizeof *m);
    g_map = m;
    m->world = w;
    m->renderer = r;
    m->rh = rh;
    m->rv = rv;
    m->dx = m->dz = 2 * (rh + 1) + 3;
    m->dy = 2 * (rv + 1) + 3;
    m->slots = calloc((size_t)m->dx * m->dy * m->dz, sizeof(Chunk));
    int gh = rh + 1, gv = rv + 1;
    m->offsets = malloc(sizeof(int[3]) * (size_t)((2 * gh + 1) * (2 * gh + 1) * (2 * gv + 1)));
    for (int y = -gv; y <= gv; y++)
        for (int z = -gh; z <= gh; z++)
            for (int x = -gh; x <= gh; x++) {
                m->offsets[m->noffsets][0] = x;
                m->offsets[m->noffsets][1] = y;
                m->offsets[m->noffsets][2] = z;
                m->noffsets++;
            }
    qsort(m->offsets, (size_t)m->noffsets, sizeof(int[3]), offset_cmp);
    int nw = jobs_worker_count();
    for (int i = 0; i < nw && i < MAX_SCRATCH; i++) g_scratch[i] = mesh_scratch_new();
}

void chunkmap_shutdown(ChunkMap *m) {
    /* wait for outstanding jobs so nothing writes into freed memory */
    while (jobs_in_flight() > 0) jobs_poll(1000);
    int n = m->dx * m->dy * m->dz;
    for (int i = 0; i < n; i++) {
        renderer_free_mesh(&m->slots[i].vao, &m->slots[i].vbo);
        free(m->slots[i].blocks);
        free(m->slots[i].light);
    }
    free(m->slots);
    free(m->offsets);
    for (int i = 0; i < MAX_SCRATCH; i++) {
        mesh_scratch_free(g_scratch[i]);
        g_scratch[i] = NULL;
    }
}

Chunk *chunkmap_get(ChunkMap *m, int cx, int cy, int cz) {
    Chunk *c = &m->slots[slot_index(m, cx, cy, cz)];
    if (!c->used || c->cx != cx || c->cy != cy || c->cz != cz) return NULL;
    return c;
}

Block chunkmap_block(ChunkMap *m, int wx, int wy, int wz, bool *known) {
    Chunk *c = chunkmap_get(m, floordiv(wx, CHUNK), floordiv(wy, CHUNK), floordiv(wz, CHUNK));
    if (!c || c->state != CH_READY) {
        if (known) *known = false;
        return B_ROCK;
    }
    if (known) *known = true;
    return c->blocks[cidx(floormod(wx, CHUNK), floormod(wy, CHUNK), floormod(wz, CHUNK))];
}

bool chunkmap_light(ChunkMap *m, int wx, int wy, int wz, u8 out[4]) {
    Chunk *c = chunkmap_get(m, floordiv(wx, CHUNK), floordiv(wy, CHUNK), floordiv(wz, CHUNK));
    if (!c || !c->light) { memset(out, 0, 4); return false; }
    memcpy(out, &c->light[cidx(floormod(wx, CHUNK), floormod(wy, CHUNK), floormod(wz, CHUNK)) * 4], 4);
    return true;
}

/* ---------- jobs ---------- */

static void gen_run(Job *j, int worker) {
    GenJob *g = (GenJob *)j;
    world_generate_chunk(g->w, g->c->cx, g->c->cy, g->c->cz, g->c->blocks, &g->c->info);
}

static void gen_done(Job *j) {
    GenJob *g = (GenJob *)j;
    g->c->state = CH_READY;
    free(g);
}

static void mesh_run(Job *j, int worker) {
    MeshJob *mj = (MeshJob *)j;
    mesh_build(&mj->in, g_scratch[worker], &mj->out);
}

static void mesh_done(Job *j) {
    MeshJob *mj = (MeshJob *)j;
    Chunk *c = mj->c;
    Renderer *r = g_map->renderer;
    int total = mj->out.nsolid + mj->out.nsprite;
    if (c->has_mesh) renderer_slot_ref(r, c->theme_slot, -1);
    if (total > 0) renderer_upload_mesh(r, &c->vao, &c->vbo, mj->out.verts, total);
    c->nsolid = mj->out.nsolid;
    c->nsprite = mj->out.nsprite;
    c->theme_slot = mj->slot;
    renderer_slot_ref(r, mj->slot, 1);
    c->has_mesh = true;
    c->mesh_queued = false;
    for (int i = 0; i < 27; i++)
        if (mj->nb[i]) mj->nb[i]->pins--;
    free(c->light);
    c->light = mj->out.light;
    free(mj->out.verts);
    free(mj);
}

static bool evict(ChunkMap *m, Chunk *c) {
    if (c->pins > 0 || c->state == CH_GENERATING || c->mesh_queued) return false;
    if (c->has_mesh) renderer_slot_ref(m->renderer, c->theme_slot, -1);
    renderer_free_mesh(&c->vao, &c->vbo);
    free(c->light);
    c->light = NULL;
    c->used = false;
    c->has_mesh = false;
    c->state = CH_EMPTY;
    c->nsolid = c->nsprite = 0;
    return true;
}

static void submit_gen(ChunkMap *m, Chunk *c, int cx, int cy, int cz) {
    if (!c->blocks) c->blocks = malloc(CHUNK_VOL);
    c->used = true;
    c->cx = cx; c->cy = cy; c->cz = cz;
    c->state = CH_GENERATING;
    c->has_mesh = c->mesh_queued = false;
    c->pins = 0;
    GenJob *g = calloc(1, sizeof *g);
    g->base.run = gen_run;
    g->base.done = gen_done;
    g->c = c;
    g->w = m->world;
    jobs_submit(&g->base);
}

static bool submit_mesh(ChunkMap *m, Chunk *c) {
    Chunk *nb[27];
    for (int dz = -1; dz <= 1; dz++)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                Chunk *n = chunkmap_get(m, c->cx + dx, c->cy + dy, c->cz + dz);
                if (!n || n->state != CH_READY) return false;
                nb[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 9] = n;
            }
    MeshJob *mj = calloc(1, sizeof *mj);
    mj->base.run = mesh_run;
    mj->base.done = mesh_done;
    mj->c = c;
    mj->slot = renderer_theme_slot(m->renderer, c->info.theme);
    mj->in.tile_base = mj->slot * TILES_PER_THEME;
    for (int i = 0; i < 27; i++) {
        mj->nb[i] = nb[i];
        nb[i]->pins++;
        mj->in.blocks[i] = nb[i]->blocks;
        mj->in.info[i] = &nb[i]->info;
    }
    c->mesh_queued = true;
    jobs_submit(&mj->base);
    return true;
}

void chunkmap_update(ChunkMap *m, vec3 pos) {
    jobs_poll(64);
    int pcx = floordiv((int)floorf(pos.x), CHUNK), pcy = floordiv((int)floorf(pos.y), CHUNK),
        pcz = floordiv((int)floorf(pos.z), CHUNK);
    int budget = jobs_worker_count() * 3 + 2 - jobs_in_flight();
    for (int i = 0; i < m->noffsets && budget > 0; i++) {
        int ox = m->offsets[i][0], oy = m->offsets[i][1], oz = m->offsets[i][2];
        int cx = pcx + ox, cy = pcy + oy, cz = pcz + oz;
        Chunk *c = &m->slots[slot_index(m, cx, cy, cz)];
        if (c->used && (c->cx != cx || c->cy != cy || c->cz != cz)) {
            if (!evict(m, c)) continue;
        }
        if (!c->used) {
            submit_gen(m, c, cx, cy, cz);
            budget--;
            continue;
        }
        bool in_mesh_range = abs(ox) <= m->rh && abs(oz) <= m->rh && abs(oy) <= m->rv;
        if (in_mesh_range && c->state == CH_READY && !c->has_mesh && !c->mesh_queued) {
            if (submit_mesh(m, c)) budget--;
        }
    }
    /* stats */
    memset(&m->stats, 0, sizeof m->stats);
    int n = m->dx * m->dy * m->dz;
    for (int i = 0; i < n; i++) {
        if (!m->slots[i].used) continue;
        m->stats.loaded++;
        if (m->slots[i].state == CH_READY) m->stats.ready++;
        if (m->slots[i].has_mesh) m->stats.meshed++;
    }
    m->stats.jobs = jobs_in_flight();
}

bool chunkmap_settled(ChunkMap *m, vec3 pos) {
    int pcx = floordiv((int)floorf(pos.x), CHUNK), pcy = floordiv((int)floorf(pos.y), CHUNK),
        pcz = floordiv((int)floorf(pos.z), CHUNK);
    for (int oy = -m->rv; oy <= m->rv; oy++)
        for (int oz = -m->rh; oz <= m->rh; oz++)
            for (int ox = -m->rh; ox <= m->rh; ox++) {
                Chunk *c = chunkmap_get(m, pcx + ox, pcy + oy, pcz + oz);
                if (!c || !c->has_mesh) return false;
            }
    return true;
}

static bool aabb_visible(const float planes[6][4], float x0, float y0, float z0, float s) {
    for (int i = 0; i < 6; i++) {
        const float *p = planes[i];
        float px = p[0] > 0 ? x0 + s : x0, py = p[1] > 0 ? y0 + s : y0, pz = p[2] > 0 ? z0 + s : z0;
        if (p[0] * px + p[1] * py + p[2] * pz + p[3] < 0) return false;
    }
    return true;
}

void chunkmap_draw(ChunkMap *m) {
    const float *a = m->renderer->vp.m;
    float planes[6][4];
    for (int i = 0; i < 3; i++)
        for (int s = 0; s < 2; s++) {
            float *p = planes[i * 2 + s];
            float sign = s ? -1.0f : 1.0f;
            for (int k = 0; k < 4; k++) p[k] = a[k * 4 + 3] + sign * a[k * 4 + i];
        }
    int n = m->dx * m->dy * m->dz;
    m->stats.drawn = 0;
    for (int i = 0; i < n; i++) {
        Chunk *c = &m->slots[i];
        if (!c->used || !c->has_mesh || c->nsolid + c->nsprite == 0) continue;
        float x0 = (float)(c->cx * CHUNK), y0 = (float)(c->cy * CHUNK), z0 = (float)(c->cz * CHUNK);
        if (!aabb_visible(planes, x0, y0, z0, (float)CHUNK)) continue;
        renderer_draw_chunk(m->renderer, c->vao, c->nsolid, c->nsprite, v3(x0, y0, z0));
        m->stats.drawn++;
    }
}
