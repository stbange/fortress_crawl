#ifndef FC_LAYOUT_H
#define FC_LAYOUT_H

#include "core/common.h"
#include "world/block.h"
#include "world/theme.h"

/* World layout: pure functions of (seed, coordinates). Nothing here needs any
 * other chunk to be generated, which is what makes the world endless and lets
 * chunks be generated in any order on any thread.
 *
 * Axes: +Y up, +X east, +Z south (so north is -Z).
 * The castle gate is at world x = 0, z = -4..3, facing west. The valley in
 * front of it (chunks x -3..-1, z -2..1, y >= 0) is the only "outside". */

#define CHUNK 32
#define CHUNK_VOL (CHUNK * CHUNK * CHUNK)

static inline int cidx(int x, int y, int z) { return (y * CHUNK + z) * CHUNK + x; }

typedef struct World { u64 seed; } World;

typedef struct Structure {
    u8 type, variant;
    int cx0, cy0, cz0, sx, sy, sz; /* chunk box */
    u64 seed;
    int floor_y;                   /* world feet height of the main floor */
    int levels[8];                 /* walkable levels (feet y): floor and galleries */
    int nlevels;
    bool levels_have_floor;        /* levels[0] is a full floor (false: chasm ledge) */
} Structure;

typedef struct ChunkInfo {
    int cx, cy, cz;
    u8 stratum, flavor;
    u16 theme;
    u8 arch;
    bool has_struct;
    Structure st;
    float difficulty;
    int level;
    u64 seed;
    /* Archetype chunks: walkable floor levels (local feet y). Corridors run flat on
     * these; height changes go through spiral stairwells. */
    int nlevels;
    int levels[2];
} ChunkInfo;

enum { P_CORRIDOR, P_NARROW, P_CRAWL, P_GRAND, P_GATE, P_SHAFT, P_TYPE_COUNT };
extern const char *PORTAL_NAMES[P_TYPE_COUNT];

/* An opening on the face between two chunks. Both chunks compute the identical
 * list from the face's own hash, so passages always line up. */
typedef struct Portal {
    u8 axis;      /* 0 = X face, 1 = Y face (shaft), 2 = Z face */
    u8 type, w, h;
    /* X/Z faces: plane coordinate (first cell of the upper chunk) on the face axis,
     * y = feet level, and the other horizontal coordinate is the opening's min corner.
     * Y faces: x, z = shaft centre, y = plane (first cell of the upper chunk). */
    int x, y, z;
    int y_low, y_up;   /* shafts: feet level of the bottom and top exits */
    u8 low_dir, exit_dir;
    int phase;         /* spiral stair phase */
} Portal;

/* Direction indices for horizontal moves. */
extern const int DIR_X[4], DIR_Z[4];

void world_init(World *w, u64 seed);

bool world_is_exterior(int cx, int cy, int cz);
int world_stratum(const World *w, int cx, int cy, int cz);
int world_flavor(const World *w, int cx, int cy, int cz);
float world_difficulty(int cx, int cy, int cz);
bool world_structure_at(const World *w, int cx, int cy, int cz, Structure *out);
void world_chunk_info(const World *w, int cx, int cy, int cz, ChunkInfo *out);

/* Portals on the face between chunk (lx,ly,lz) and its +axis neighbour. */
int world_face_portals(const World *w, int axis, int lx, int ly, int lz, Portal *out, int max);
/* All portals on the six faces of a chunk. */
int world_chunk_portals(const World *w, int cx, int cy, int cz, Portal *out, int max);

/* Shared geometry helpers. */
int helix_sector(int dx, int dz);                        /* 0..15 on the 5x5 ring */
int cavern_floor(const Structure *s, int wx, int wz);    /* world feet height */

/* Name for the debug overlay: "Frostbitten Mines" etc. */
void world_place_name(const ChunkInfo *ci, char *buf, int n);

/* Generates the blocks of one chunk (CHUNK_VOL bytes). Thread-safe. */
void world_generate_chunk(const World *w, int cx, int cy, int cz, Block *blocks, ChunkInfo *info);

#endif
