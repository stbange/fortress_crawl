#ifndef FC_GEN_COMMON_H
#define FC_GEN_COMMON_H

/* Internal helpers shared by the chunk generators. All coordinates are local
 * to the chunk (0..31) unless a function name ends in _w (world coordinates,
 * clipped to the chunk). */

#include "world/layout.h"

#define MAX_PORTALS 24
#define MAX_NODES 64

typedef struct Node {
    int x, y, z; /* local, y = feet level */
    int w, h;
} Node;

typedef struct GenCtx {
    const World *w;
    ChunkInfo info;
    int ox, oy, oz; /* world origin of the chunk */
    Block *b;
    u8 prot[CHUNK_VOL / 8]; /* protected cells: later carving leaves them alone */
    Rng rng;
    Portal portals[MAX_PORTALS];
    int nportals;
    Node nodes[MAX_NODES];
    int nnodes;
    int links[MAX_NODES][2]; /* node pairs already connected (by a stairwell) */
    int nlinks;
    float light_rate; /* scales torch / brazier density */
    /* Corridor floors are placed after all corridors are carved, so a corridor
     * crossing another one at a different height leaves a step or a low gap,
     * never a hole or a wall. */
    int *floors;
    int nfloors, floors_cap;
} GenCtx;

static inline bool g_in(int x, int y, int z) {
    return (unsigned)x < CHUNK && (unsigned)y < CHUNK && (unsigned)z < CHUNK;
}
static inline Block g_get(const GenCtx *g, int x, int y, int z) {
    return g_in(x, y, z) ? g->b[cidx(x, y, z)] : B_ROCK;
}
static inline bool g_prot(const GenCtx *g, int x, int y, int z) {
    int i = cidx(x, y, z);
    return (g->prot[i >> 3] >> (i & 7)) & 1;
}
static inline void g_protect(GenCtx *g, int x, int y, int z) {
    if (!g_in(x, y, z)) return;
    int i = cidx(x, y, z);
    g->prot[i >> 3] |= (u8)(1 << (i & 7));
}
/* Unconditional write (ignores protection). */
static inline void g_put(GenCtx *g, int x, int y, int z, Block v) {
    if (g_in(x, y, z)) g->b[cidx(x, y, z)] = v;
}
/* Write unless protected. */
static inline void g_set(GenCtx *g, int x, int y, int z, Block v) {
    if (g_in(x, y, z) && !g_prot(g, x, y, z)) g->b[cidx(x, y, z)] = v;
}
static inline void g_set_w(GenCtx *g, int wx, int wy, int wz, Block v) {
    g_set(g, wx - g->ox, wy - g->oy, wz - g->oz, v);
}
static inline Block g_get_w(const GenCtx *g, int wx, int wy, int wz) {
    return g_get(g, wx - g->ox, wy - g->oy, wz - g->oz);
}

void g_fill(GenCtx *g, int x0, int y0, int z0, int x1, int y1, int z1, Block v); /* inclusive */
void g_fill_w(GenCtx *g, int x0, int y0, int z0, int x1, int y1, int z1, Block v);
int g_add_node(GenCtx *g, int x, int y, int z, int w, int h);

/* Carves a w x w footprint, h high, standing on (x,y,z); queues a floor block below it. */
void carve_cell(GenCtx *g, int x, int y, int z, int w, int h);
/* Places the queued corridor floors (called by connect_nodes). */
void flush_floors(GenCtx *g);
/* Corridor with stairs between two nodes. beams: mine timbering. */
void carve_path(GenCtx *g, const Node *a, const Node *b, int w, int h, bool beams);
/* Connects all nodes (spanning tree + some loops). */
void connect_nodes(GenCtx *g, int cw, int ch, float extra, bool beams, bool narrow_mix);

/* Straight mouth from each horizontal portal on this chunk's faces, adds a node per portal.
 * platform: build a floor platform instead (open-air chunks). */
void carve_portal_mouths(GenCtx *g, int depth, bool platform);
/* Spiral staircases for the vertical portals; protects them and adds exit nodes. */
void carve_shafts(GenCtx *g);
/* Spiral stairwell from node ni (heading `dir`) to height target; returns the node at target. */
int stairwell(GenCtx *g, int ni, int dir, int target);
/* One stairwell between the chunk's two floor levels. */
void link_levels(GenCtx *g);
/* Spiral stair cells (5x5 ring around a 3x3 pillar) between world heights y0..y1. */
void carve_helix_w(GenCtx *g, int cx, int cz, int y0, int y1, int phase, bool protect);

/* Turns exposed rock into walls / floors / ceilings (built style). */
void skin_pass(GenCtx *g, bool built);
/* Lights and decoration according to stratum and flavor. */
void decorate(GenCtx *g, bool natural);
/* Keeps a solid shell toward the exterior so the outside never sees into the dungeon. */
void seal_exterior(GenCtx *g);

void gen_archetype(GenCtx *g);
void gen_structure(GenCtx *g);
void gen_exterior(GenCtx *g);

#endif
