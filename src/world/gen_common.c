#include "world/gen_common.h"

#include "core/noise.h"

void g_fill(GenCtx *g, int x0, int y0, int z0, int x1, int y1, int z1, Block v) {
    x0 = MAXI(x0, 0); y0 = MAXI(y0, 0); z0 = MAXI(z0, 0);
    x1 = MINI(x1, CHUNK - 1); y1 = MINI(y1, CHUNK - 1); z1 = MINI(z1, CHUNK - 1);
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) g_set(g, x, y, z, v);
}

void g_fill_w(GenCtx *g, int x0, int y0, int z0, int x1, int y1, int z1, Block v) {
    g_fill(g, x0 - g->ox, y0 - g->oy, z0 - g->oz, x1 - g->ox, y1 - g->oy, z1 - g->oz, v);
}

int g_add_node(GenCtx *g, int x, int y, int z, int w, int h) {
    if (g->nnodes >= MAX_NODES) return -1;
    Node *n = &g->nodes[g->nnodes];
    n->x = CLAMPI(x, 1, CHUNK - 2);
    n->y = CLAMPI(y, 1, CHUNK - 2);
    n->z = CLAMPI(z, 1, CHUNK - 2);
    n->w = w; n->h = h;
    return g->nnodes++;
}

static void queue_floor(GenCtx *g, int x, int y, int z) {
    if (!g_in(x, y, z)) return;
    if (g->nfloors >= g->floors_cap) {
        g->floors_cap = g->floors_cap ? g->floors_cap * 2 : 1024;
        g->floors = realloc(g->floors, sizeof(int) * (size_t)g->floors_cap);
    }
    g->floors[g->nfloors++] = cidx(x, y, z);
}

void carve_cell(GenCtx *g, int x, int y, int z, int w, int h) {
    int lo = -(w - 1) / 2, hi = w / 2;
    for (int dz = lo; dz <= hi; dz++)
        for (int dx = lo; dx <= hi; dx++) {
            int cx = x + dx, cz = z + dz;
            for (int yy = y; yy < y + h; yy++) g_set(g, cx, yy, cz, B_AIR);
            queue_floor(g, cx, y - 1, cz);
        }
}

void flush_floors(GenCtx *g) {
    for (int i = 0; i < g->nfloors; i++) {
        int c = g->floors[i];
        if ((g->prot[c >> 3] >> (c & 7)) & 1) continue;
        if (!block_solid(g->b[c])) g->b[c] = B_FLOOR;
    }
    g->nfloors = 0;
}

static void beam_frame(GenCtx *g, int x, int y, int z, int w, int h, bool along_x, int step) {
    int half = w / 2 + 1;
    for (int s = -half; s <= half; s++) {
        int bx = along_x ? x : x + s, bz = along_x ? z + s : z;
        /* lintel */
        if (block_solid(g_get(g, bx, y + h, bz))) g_set(g, bx, y + h, bz, B_WOOD);
        if (s == -half || s == half)
            for (int yy = y; yy < y + h; yy++)
                if (block_solid(g_get(g, bx, yy, bz))) g_set(g, bx, yy, bz, B_WOOD);
    }
    if (step % 8 == 0 && g_get(g, x, y + h - 1, z) == B_AIR) g_set(g, x, y + h - 1, z, B_LANTERN);
}

/* The 16 ring cells of a 5x5 spiral in ascending sector order. */
static void ring_cells(int out[16][2]) {
    for (int dz = -2; dz <= 2; dz++)
        for (int dx = -2; dx <= 2; dx++)
            if (MAXI(abs(dx), abs(dz)) == 2) {
                int s = helix_sector(dx, dz);
                out[s][0] = dx;
                out[s][1] = dz;
            }
}

/* ---------- flat corridors: A* on the 32x32 grid of one level ---------- */

typedef struct HeapItem { int cost, cell; } HeapItem;

static void heap_push(HeapItem *h, int *n, HeapItem it) {
    int i = (*n)++;
    h[i] = it;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h[p].cost <= h[i].cost) break;
        HeapItem t = h[p]; h[p] = h[i]; h[i] = t;
        i = p;
    }
}

static HeapItem heap_pop(HeapItem *h, int *n) {
    HeapItem top = h[0];
    h[0] = h[--(*n)];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < *n && h[l].cost < h[m].cost) m = l;
        if (r < *n && h[r].cost < h[m].cost) m = r;
        if (m == i) break;
        HeapItem t = h[m]; h[m] = h[i]; h[i] = t;
        i = m;
    }
    return top;
}

/* A corridor footprint can go here unless it would cut protected geometry
 * (stairwells, portal mouths, room floors). */
static bool footprint_ok(const GenCtx *g, int x, int y, int z, int w, int h) {
    int lo = -(w - 1) / 2, hi = w / 2;
    for (int dz = lo; dz <= hi; dz++)
        for (int dx = lo; dx <= hi; dx++) {
            int cx = x + dx, cz = z + dz;
            if (!g_in(cx, y, cz)) return false;
            if (g_in(cx, y - 1, cz) && g_prot(g, cx, y - 1, cz) && !block_solid(g_get(g, cx, y - 1, cz))) return false;
            for (int yy = y; yy < MINI(y + h, CHUNK); yy++)
                if (g_prot(g, cx, yy, cz) && block_solid(g_get(g, cx, yy, cz))) return false;
        }
    return true;
}

static bool astar_path(GenCtx *g, const Node *a, const Node *b, int w, int h, int *out, int *nout) {
    enum { G = CHUNK * CHUNK };
    static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
    int *cost = malloc(sizeof(int) * G * 5);
    int *prev = cost + G;
    u8 *ok = (u8 *)(cost + 2 * G);
    HeapItem *heap = (HeapItem *)(cost + 3 * G);
    int y = a->y;
    for (int z = 0; z < CHUNK; z++)
        for (int x = 0; x < CHUNK; x++) {
            int i = z * CHUNK + x;
            cost[i] = 1 << 29;
            prev[i] = -1;
            ok[i] = (u8)(x >= 1 && x <= CHUNK - 2 && z >= 1 && z <= CHUNK - 2 && footprint_ok(g, x, y, z, w, h));
        }
    int s = a->z * CHUNK + a->x, t = b->z * CHUNK + b->x;
    ok[s] = ok[t] = 1;
    int hn = 0;
    cost[s] = 0;
    heap_push(heap, &hn, (HeapItem){ 0, s });
    bool found = false;
    while (hn > 0 && hn < G * 2 - 8) {
        HeapItem it = heap_pop(heap, &hn);
        int c = it.cell;
        if (c == t) { found = true; break; }
        int cx = c % CHUNK, cz = c / CHUNK;
        if (it.cost - (abs(cx - b->x) + abs(cz - b->z)) * 10 > cost[c]) continue;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], nz = cz + DZ[d];
            if ((unsigned)nx >= CHUNK || (unsigned)nz >= CHUNK) continue;
            int n = nz * CHUNK + nx;
            if (!ok[n]) continue;
            /* reuse existing passages, avoid zig-zags */
            int step = g_get(g, nx, y, nz) == B_AIR ? 6 : 10;
            int p = prev[c];
            if (p >= 0 && (p % CHUNK - cx != cx - nx || p / CHUNK - cz != cz - nz)) step += 8;
            int nc = cost[c] + step;
            if (nc < cost[n]) {
                cost[n] = nc;
                prev[n] = c;
                heap_push(heap, &hn, (HeapItem){ nc + (abs(nx - b->x) + abs(nz - b->z)) * 10, n });
            }
        }
    }
    *nout = 0;
    if (found) {
        int len = 0;
        for (int c = t; c != -1 && len < G; c = prev[c]) out[len++] = c;
        /* reverse into start -> goal order */
        for (int i = 0; i < len / 2; i++) { int tmp = out[i]; out[i] = out[len - 1 - i]; out[len - 1 - i] = tmp; }
        *nout = len;
    }
    free(cost);
    return found;
}

void carve_path(GenCtx *g, const Node *a, const Node *b, int w, int h, bool beams) {
    if (a->y == b->y) {
        int *cells = malloc(sizeof(int) * CHUNK * CHUNK);
        int n = 0;
        if (astar_path(g, a, b, w, h, cells, &n)) {
            for (int i = 0; i < n; i++) {
                int cx = cells[i] % CHUNK, cz = cells[i] / CHUNK;
                carve_cell(g, cx, a->y, cz, w, h);
                if (beams && i % 4 == 0 && i + 1 < n) {
                    bool along_x = cells[i + 1] / CHUNK == cz;
                    beam_frame(g, cx, a->y, cz, w, h, along_x, i);
                }
            }
            free(cells);
            return;
        }
        free(cells);
    }
    int x = a->x, y = a->y, z = a->z;
    bool xfirst = rng_next(&g->rng) & 1;
    int steps = 0;
    carve_cell(g, x, y, z, w, h);
    for (int phase = 0; phase < 2; phase++) {
        bool along_x = (phase == 0) == xfirst;
        int target = along_x ? b->x : b->z;
        while ((along_x ? x : z) != target) {
            int dir = target > (along_x ? x : z) ? 1 : -1;
            int ny = y;
            if (y != b->y && steps >= 1) ny = y + (b->y > y ? 1 : -1);
            int py = y;
            if (ny != y) carve_cell(g, x, y, z, w, h + 1); /* headroom for the step */
            if (along_x) x += dir; else z += dir;
            y = ny;
            carve_cell(g, x, y, z, w, ny != py ? h + 1 : h);
            if (beams && steps % 4 == 0) beam_frame(g, x, y, z, w, h, along_x, steps);
            steps++;
        }
    }
    if (y != b->y) {
        /* Not enough horizontal run for the height difference: wind a narrow spiral around b. */
        int ring[16][2];
        ring_cells(ring);
        int dir = b->y > y ? 1 : -1;
        int k = helix_sector(2, 0);
        carve_cell(g, b->x + 1, y, b->z, 1, h + 1);
        while (y != b->y) {
            carve_cell(g, b->x + ring[k][0], y, b->z + ring[k][1], 1, MAXI(h, 2) + 1);
            y += dir;
            k = floormod(k + dir, 16);
        }
        carve_cell(g, b->x + ring[k][0], y, b->z + ring[k][1], 1, MAXI(h, 2) + 1);
        carve_cell(g, b->x + ring[k][0] / 2, y, b->z + ring[k][1] / 2, 1, MAXI(h, 2));
        carve_cell(g, b->x, y, b->z, w, h);
    }
}

static int uf_find(int *p, int i) {
    while (p[i] != i) i = p[i] = p[p[i]];
    return i;
}

typedef struct Edge { int a, b, cost; } Edge;

static int edge_cmp(const void *x, const void *y) {
    const Edge *a = x, *b = y;
    if (a->cost != b->cost) return a->cost - b->cost;
    if (a->a != b->a) return a->a - b->a;
    return a->b - b->b;
}

void connect_nodes(GenCtx *g, int cw, int ch, float extra, bool beams, bool narrow_mix) {
    int n = g->nnodes;
    static const int NARROW[3][2] = { { 1, 2 }, { 1, 1 }, { 2, 3 } };
    Edge *e_list = malloc(sizeof(Edge) * (size_t)(MAX_NODES * (MAX_NODES - 1) / 2));
    int ne = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            const Node *a = &g->nodes[i], *b = &g->nodes[j];
            e_list[ne].a = i; e_list[ne].b = j;
            e_list[ne].cost = abs(a->x - b->x) + abs(a->z - b->z);
            /* corridors stay flat; a sloped corridor is only the last resort */
            if (a->y != b->y) e_list[ne].cost += 100000 + 8 * abs(a->y - b->y);
            ne++;
        }
    qsort(e_list, (size_t)ne, sizeof(Edge), edge_cmp);
    int parent[MAX_NODES];
    for (int i = 0; i < n; i++) parent[i] = i;
    for (int i = 0; i < g->nlinks; i++) parent[uf_find(parent, g->links[i][0])] = uf_find(parent, g->links[i][1]);
    for (int e = 0; e < ne; e++) {
        int ra = uf_find(parent, e_list[e].a), rb = uf_find(parent, e_list[e].b);
        bool tree = ra != rb;
        if (!tree && !(e_list[e].cost < 40 && rng_chance(&g->rng, extra))) continue;
        if (tree) parent[ra] = rb;
        const Node *a = &g->nodes[e_list[e].a], *b = &g->nodes[e_list[e].b];
        int w = cw, h = ch;
        if (narrow_mix) {
            int k = rng_range(&g->rng, 0, 2);
            w = NARROW[k][0]; h = NARROW[k][1];
        }
        w = MINI(w, MINI(a->w, b->w));
        h = MINI(h, MINI(a->h, b->h));
        if (a->w == b->w && a->h == b->h && a->w > w && a->w < 99) { w = a->w; h = a->h; } /* grand to grand */
        carve_path(g, a, b, MAXI(w, 1), MAXI(h, 1), beams);
    }
    free(e_list);
    flush_floors(g);
}

static bool is_level(const GenCtx *g, int y) {
    for (int i = 0; i < g->info.nlevels; i++)
        if (g->info.levels[i] == y) return true;
    return g->info.nlevels == 0;
}

static int nearest_level(const GenCtx *g, int y) {
    int best = g->info.levels[0];
    for (int i = 1; i < g->info.nlevels; i++)
        if (abs(g->info.levels[i] - y) < abs(best - y)) best = g->info.levels[i];
    return best;
}

static bool area_free(const GenCtx *g, int cx, int cz, int lo, int hi) {
    if (cx < 4 || cx > CHUNK - 5 || cz < 4 || cz > CHUNK - 5) return false;
    for (int y = MAXI(lo - 1, 0); y <= MINI(hi + 3, CHUNK - 1); y++)
        for (int dz = -2; dz <= 2; dz++)
            for (int dx = -2; dx <= 2; dx++)
                if (g_prot(g, cx + dx, y, cz + dz)) return false;
    return true;
}

int stairwell(GenCtx *g, int ni, int dir, int target) {
    Node n = g->nodes[ni];
    if (n.y == target) return ni;
    int lo = MINI(n.y, target), hi = MAXI(n.y, target);
    /* find a free spot: straight ahead first, then deeper or to the side */
    static const int LAT[5] = { 0, 3, -3, 6, -6 };
    int side = (dir + 1) % 4, cx = 0, cz = 0;
    bool found = false;
    for (int k = 3; k <= 10 && !found; k++)
        for (int l = 0; l < 5 && !found; l++) {
            cx = n.x + DIR_X[dir] * k + DIR_X[side] * LAT[l];
            cz = n.z + DIR_Z[dir] * k + DIR_Z[side] * LAT[l];
            found = area_free(g, cx, cz, lo, hi);
        }
    if (!found) return ni;
    /* going up, the top exit faces away from the node; going down, it faces the node */
    int top_dir = n.y < target ? dir : (dir + 2) % 4;
    int phase = floormod(g->oy + hi - 1 - helix_sector(DIR_X[top_dir] * 2, DIR_Z[top_dir] * 2), 16);
    for (int dz = -2; dz <= 2; dz++)
        for (int dx = -2; dx <= 2; dx++) {
            g_set(g, cx + dx, lo - 1, cz + dz, B_FLOOR);
            g_protect(g, cx + dx, lo - 1, cz + dz);
        }
    carve_helix_w(g, g->ox + cx, g->oz + cz, g->oy + lo, g->oy + hi + 3, phase, true);
    Node entry = { cx - DIR_X[dir] * 3, n.y, cz - DIR_Z[dir] * 3, n.w, n.h };
    if (entry.x != n.x || entry.z != n.z) carve_path(g, &n, &entry, MINI(n.w, 3), MAXI(MINI(n.h, 4), 1), false);
    else carve_cell(g, n.x, n.y, n.z, MINI(n.w, 3), MAXI(MINI(n.h, 4), 1));
    int x = g_add_node(g, cx + DIR_X[dir] * 3, target, cz + DIR_Z[dir] * 3, 3, 4);
    if (x >= 0 && g->nlinks < MAX_NODES) {
        g->links[g->nlinks][0] = ni;
        g->links[g->nlinks][1] = x;
        g->nlinks++;
    }
    return x < 0 ? ni : x;
}

void link_levels(GenCtx *g) {
    if (g->info.nlevels < 2) return;
    int dir = rng_range(&g->rng, 0, 3);
    int cx = rng_range(&g->rng, 8, 23), cz = rng_range(&g->rng, 8, 23);
    int ni = g_add_node(g, cx - DIR_X[dir] * 3, g->info.levels[0], cz - DIR_Z[dir] * 3, 3, 4);
    if (ni >= 0) stairwell(g, ni, dir, g->info.levels[1]);
}

void carve_portal_mouths(GenCtx *g, int depth, bool platform) {
    for (int i = 0; i < g->nportals; i++) {
        const Portal *p = &g->portals[i];
        if (p->axis == 1 || p->type == P_GATE) continue;
        int plane = p->axis == 0 ? p->x : p->z;
        int origin = p->axis == 0 ? g->ox : g->oz;
        int start, dir;
        if (plane == origin) { start = 0; dir = 1; }
        else if (plane == origin + CHUNK) { start = CHUNK - 1; dir = -1; }
        else continue;
        int along = (p->axis == 0 ? p->z - g->oz : p->x - g->ox);
        int y = p->y - g->oy;
        int pad = platform ? 1 : 0;
        for (int d = 0; d < depth; d++) {
            int a = start + dir * d;
            for (int k = -pad; k < p->w + pad; k++) {
                int x = p->axis == 0 ? a : along + k;
                int z = p->axis == 0 ? along + k : a;
                bool in_opening = k >= 0 && k < p->w;
                if (in_opening)
                    for (int yy = y; yy < y + p->h; yy++) {
                        g_set(g, x, yy, z, B_AIR);
                        if (d == 0) g_protect(g, x, yy, z);
                    }
                if (in_opening || platform) {
                    if (platform || !block_solid(g_get(g, x, y - 1, z))) g_set(g, x, y - 1, z, B_FLOOR);
                    g_protect(g, x, y - 1, z);
                }
            }
        }
        int na = start + dir * (depth - 1);
        int nx = p->axis == 0 ? na : along + p->w / 2;
        int nz = p->axis == 0 ? along + p->w / 2 : na;
        int ni = g_add_node(g, nx, y, nz, p->w, p->h);
        if (ni >= 0 && !is_level(g, y)) {
            int inward = p->axis == 0 ? (dir > 0 ? 0 : 2) : (dir > 0 ? 1 : 3);
            stairwell(g, ni, inward, nearest_level(g, y));
        }
    }
}

void carve_helix_w(GenCtx *g, int cx, int cz, int y0, int y1, int phase, bool protect) {
    for (int wy = MAXI(y0, g->oy); wy <= MINI(y1, g->oy + CHUNK - 1); wy++)
        for (int dz = -2; dz <= 2; dz++)
            for (int dx = -2; dx <= 2; dx++) {
                int x = cx + dx - g->ox, y = wy - g->oy, z = cz + dz - g->oz;
                if (!g_in(x, y, z) || g_prot(g, x, y, z)) continue;
                int c = MAXI(abs(dx), abs(dz));
                Block v;
                if (c <= 1) v = B_PILLAR;
                else v = (floormod(wy - phase, 16) == helix_sector(dx, dz)) ? B_FLOOR : B_AIR;
                g_put(g, x, y, z, v);
                if (protect) g_protect(g, x, y, z);
            }
}

void carve_shafts(GenCtx *g) {
    for (int i = 0; i < g->nportals; i++) {
        const Portal *p = &g->portals[i];
        if (p->axis != 1) continue;
        int lx = p->x - g->ox, lz = p->z - g->oz;
        int ni = -1, d = 0;
        if (p->y == g->oy + CHUNK) {
            /* this chunk holds the bottom of the shaft */
            g_fill_w(g, p->x - 2, p->y_low - 1, p->z - 2, p->x + 2, p->y_low - 1, p->z + 2, B_FLOOR);
            for (int dz = -2; dz <= 2; dz++)
                for (int dx = -2; dx <= 2; dx++) g_protect(g, lx + dx, p->y_low - 1 - g->oy, lz + dz);
            carve_helix_w(g, p->x, p->z, p->y_low, g->oy + CHUNK - 1, p->phase, true);
            d = p->low_dir;
            ni = g_add_node(g, lx + DIR_X[d] * 3, p->y_low - g->oy, lz + DIR_Z[d] * 3, 3, 4);
        } else if (p->y == g->oy) {
            carve_helix_w(g, p->x, p->z, g->oy, p->y_up + 3, p->phase, true);
            d = p->exit_dir;
            ni = g_add_node(g, lx + DIR_X[d] * 3, p->y_up - g->oy, lz + DIR_Z[d] * 3, 3, 4);
        }
        if (ni >= 0 && !is_level(g, g->nodes[ni].y)) stairwell(g, ni, d, nearest_level(g, g->nodes[ni].y));
    }
}

void skin_pass(GenCtx *g, bool built) {
    for (int y = 0; y < CHUNK; y++)
        for (int z = 0; z < CHUNK; z++)
            for (int x = 0; x < CHUNK; x++) {
                Block b = g->b[cidx(x, y, z)];
                if (b != B_ROCK) continue;
                bool up = y + 1 < CHUNK && !block_opaque(g->b[cidx(x, y + 1, z)]);
                bool down = y > 0 && !block_opaque(g->b[cidx(x, y - 1, z)]);
                bool side = false;
                for (int d = 0; d < 4 && !side; d++) {
                    int nx = x + DIR_X[d], nz = z + DIR_Z[d];
                    if (g_in(nx, y, nz) && !block_opaque(g->b[cidx(nx, y, nz)])) side = true;
                }
                if (built) {
                    if (up) g->b[cidx(x, y, z)] = B_FLOOR;
                    else if (down) g->b[cidx(x, y, z)] = B_CEIL;
                    else if (side) g->b[cidx(x, y, z)] = B_WALL;
                } else if (up) {
                    g->b[cidx(x, y, z)] = B_DIRT;
                }
            }
}

/* ---------- decoration ---------- */

typedef struct LightList { int n; i16 p[256][3]; } LightList;

static bool light_ok(const LightList *l, int x, int y, int z, int min_dist) {
    for (int i = 0; i < l->n; i++) {
        int dx = l->p[i][0] - x, dy = l->p[i][1] - y, dz = l->p[i][2] - z;
        if (dx * dx + dy * dy + dz * dz < min_dist * min_dist) return false;
    }
    return true;
}

static void light_add(LightList *l, int x, int y, int z) {
    if (l->n >= 256) return;
    l->p[l->n][0] = (i16)x; l->p[l->n][1] = (i16)y; l->p[l->n][2] = (i16)z;
    l->n++;
}

static bool open_air(const GenCtx *g, int x, int y, int z) {
    Block b = g_get(g, x, y, z);
    return b == B_AIR && !g_prot(g, x, y, z);
}

void decorate(GenCtx *g, bool natural) {
    const StratumDef *st = &STRATA[g->info.stratum];
    const FlavorDef *fl = &FLAVORS[g->info.flavor];
    u64 seed = g->info.seed;
    LightList lights;
    lights.n = 0;
    float lr = g->light_rate;
    Block wall_light = fl->decor == B_LANTERN ? B_LANTERN : B_TORCH;

    /* Surface materials first: fluids, ice, lava, crystals, ore. */
    float lava_amt = fmaxf(st->lava, fl->fluid == B_LAVA ? 0.5f : 0.0f);
    for (int y = 1; y < CHUNK - 1; y++)
        for (int z = 0; z < CHUNK; z++)
            for (int x = 0; x < CHUNK; x++) {
                Block b = g->b[cidx(x, y, z)];
                if (!block_solid(b) || g_prot(g, x, y, z)) continue;
                int wx = g->ox + x, wy = g->oy + y, wz = g->oz + z;
                bool air_above = g->b[cidx(x, y + 1, z)] == B_AIR;
                if (air_above && (b == B_FLOOR || b == B_DIRT || b == B_ROCK) && block_solid(g->b[cidx(x, y - 1, z)])) {
                    float n = noise3(seed ^ 0xF1, wx * 0.18f, wy * 0.18f, wz * 0.18f);
                    if (fl->fluid == B_WATER && n > 0.32f) { g->b[cidx(x, y, z)] = B_WATER; continue; }
                    if (fl->fluid == B_ICE && n > 0.25f) { g->b[cidx(x, y, z)] = B_ICE; continue; }
                    if (lava_amt > 0 && n > 0.55f - lava_amt * 0.25f) { g->b[cidx(x, y, z)] = B_LAVA; continue; }
                }
                bool exposed = false;
                for (int d = 0; d < 4; d++)
                    if (g_get(g, x + DIR_X[d], y, z + DIR_Z[d]) == B_AIR) exposed = true;
                if (!exposed) continue;
                float h = hash3f(seed, x, y, z, 0xDEC0);
                if (st->crystal > 0 && h < st->crystal * (natural ? 0.03f : 0.008f)) g->b[cidx(x, y, z)] = B_CRYSTAL;
                else if (g->info.stratum == ST_MINES && b == B_ROCK && h > 0.95f) g->b[cidx(x, y, z)] = B_ORE;
            }

    for (int y = 1; y < CHUNK - 1; y++)
        for (int z = 1; z < CHUNK - 1; z++)
            for (int x = 1; x < CHUNK - 1; x++) {
                if (!open_air(g, x, y, z)) continue;
                float h = hash3f(seed, x, y, z, 0xDEC1);
                Block below = g->b[cidx(x, y - 1, z)];
                Block above = g->b[cidx(x, y + 1, z)];
                int walls = 0;
                for (int d = 0; d < 4; d++)
                    if (block_opaque(g->b[cidx(x + DIR_X[d], y, z + DIR_Z[d])])) walls++;

                /* wall torch, two above the floor */
                if (walls > 0 && walls < 3 && y >= 2 && below == B_AIR && block_solid(g->b[cidx(x, y - 2, z)]) &&
                    above == B_AIR && !natural && h < 0.07f * lr && light_ok(&lights, x, y, z, 7)) {
                    g->b[cidx(x, y, z)] = wall_light;
                    light_add(&lights, x, y, z);
                    continue;
                }
                /* free-standing brazier in open floor areas */
                if (block_solid(below) && below != B_LAVA && above == B_AIR && walls == 0 && h > 0.985f - 0.02f * lr) {
                    bool open = true;
                    for (int dz = -1; dz <= 1 && open; dz++)
                        for (int dx = -1; dx <= 1 && open; dx++)
                            if (g->b[cidx(x + dx, y, z + dz)] != B_AIR || !block_solid(g->b[cidx(x + dx, y - 1, z + dz)])) open = false;
                    if (open && light_ok(&lights, x, y, z, 8)) {
                        g->b[cidx(x, y, z)] = B_BRAZIER;
                        g->b[cidx(x, y + 1, z)] = B_FIRE;
                        light_add(&lights, x, y, z);
                        continue;
                    }
                }
                /* hanging things */
                if (block_opaque(above)) {
                    if (walls >= 2 && h < (fl->decor == B_WEB ? 0.35f : 0.08f)) { g->b[cidx(x, y, z)] = B_WEB; continue; }
                    bool rooty = fl->decor == B_ROOTS || st->roots > 0;
                    if (rooty && h < 0.10f + st->roots * 0.1f) {
                        int len = 1 + (int)(hash3f(seed, x, y, z, 0xDEC2) * 3);
                        for (int k = 0; k < len && open_air(g, x, y - k, z); k++) g->b[cidx(x, y - k, z)] = B_ROOTS;
                        continue;
                    }
                    if (h > 0.985f || (fl->decor == B_LANTERN && h > 0.96f && light_ok(&lights, x, y, z, 7))) {
                        int len = 2 + (int)(hash3f(seed, x, y, z, 0xDEC3) * 5);
                        int k = 0;
                        for (; k < len && y - k > 1 && open_air(g, x, y - k, z) && open_air(g, x, y - k - 1, z); k++)
                            g->b[cidx(x, y - k, z)] = B_CHAIN;
                        if (k > 0 && (fl->decor == B_LANTERN || h > 0.993f)) {
                            g->b[cidx(x, y - k + 1, z)] = B_LANTERN;
                            light_add(&lights, x, y - k, z);
                        }
                        continue;
                    }
                }
                /* floor clutter */
                if (block_solid(below) && below != B_LAVA) {
                    float fung = fmaxf(st->fungus, natural ? 0.25f : 0.0f);
                    if (fung > 0 && h > 1.0f - 0.04f * fung) { g->b[cidx(x, y, z)] = B_FUNGUS; continue; }
                    bool bony = fl->decor == B_BONES || g->info.stratum == ST_LIMBO || g->info.stratum == ST_INFERNO ||
                                g->info.arch == ARCH_CELLS;
                    if (bony && h < 0.02f) { g->b[cidx(x, y, z)] = B_BONES; continue; }
                }
            }
}

void seal_exterior(GenCtx *g) {
    const int cx = g->info.cx, cy = g->info.cy, cz = g->info.cz;
    for (int d = 0; d < 4; d++) {
        if (!world_is_exterior(cx + DIR_X[d], cy, cz + DIR_Z[d])) continue;
        for (int y = 0; y < CHUNK; y++)
            for (int a = 0; a < CHUNK; a++)
                for (int t = 0; t < 2; t++) {
                    int x = DIR_X[d] > 0 ? CHUNK - 1 - t : (DIR_X[d] < 0 ? t : a);
                    int z = DIR_Z[d] > 0 ? CHUNK - 1 - t : (DIR_Z[d] < 0 ? t : a);
                    if (!block_opaque(g_get(g, x, y, z))) g_set(g, x, y, z, B_ROCK);
                }
    }
    if (world_is_exterior(cx, cy + 1, cz))
        for (int z = 0; z < CHUNK; z++)
            for (int x = 0; x < CHUNK; x++)
                for (int t = 0; t < 2; t++)
                    if (!block_opaque(g_get(g, x, CHUNK - 1 - t, z))) g_set(g, x, CHUNK - 1 - t, z, B_ROCK);
}

void world_generate_chunk(const World *w, int cx, int cy, int cz, Block *blocks, ChunkInfo *info) {
    GenCtx *g = calloc(1, sizeof(GenCtx));
    g->w = w;
    world_chunk_info(w, cx, cy, cz, &g->info);
    g->ox = cx * CHUNK; g->oy = cy * CHUNK; g->oz = cz * CHUNK;
    g->b = blocks;
    g->rng = rng_make(g->info.seed);
    g->nportals = world_chunk_portals(w, cx, cy, cz, g->portals, MAX_PORTALS);
    g->light_rate = clampf(1.25f - g->info.difficulty * 0.02f, 0.35f, 1.25f);
    memset(blocks, B_ROCK, CHUNK_VOL);

    if (g->info.arch == ARCH_EXTERIOR) gen_exterior(g);
    else if (g->info.has_struct) gen_structure(g);
    else gen_archetype(g);

    if (g->info.arch != ARCH_EXTERIOR) seal_exterior(g);
    if (info) *info = g->info;
    free(g->floors);
    free(g);
}
