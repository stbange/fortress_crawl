#ifndef FC_CHUNKMAP_H
#define FC_CHUNKMAP_H

#include "core/fmath.h"
#include "engine/gl.h"
#include "render/renderer.h"
#include "world/layout.h"

/* Streams chunks around the camera: generation and meshing run on worker
 * threads, uploads happen on the main thread. Chunks live in a 3D ring buffer
 * indexed by coordinates modulo its size, so far-away chunks are recycled
 * automatically when their slot is needed. */

enum { CH_EMPTY, CH_GENERATING, CH_READY };

typedef struct Chunk {
    bool used;
    int cx, cy, cz;
    int state;
    Block *blocks;
    u8 *light; /* light field from the last meshing, CHUNK_VOL * 4, or NULL */
    ChunkInfo info;
    bool mesh_queued, has_mesh;
    GLuint vao, vbo;
    int nsolid, nsprite;
    int theme_slot;
    int pins; /* mesh jobs currently reading this chunk */
} Chunk;

typedef struct ChunkStats {
    int loaded, ready, meshed, drawn, jobs;
} ChunkStats;

typedef struct ChunkMap {
    const World *world;
    Renderer *renderer;
    int rh, rv;      /* mesh radius in chunks (horizontal, vertical) */
    int dx, dy, dz;  /* ring buffer size */
    Chunk *slots;
    int (*offsets)[3];
    int noffsets;
    ChunkStats stats;
} ChunkMap;

void chunkmap_init(ChunkMap *m, const World *w, Renderer *r, int rh, int rv);
void chunkmap_shutdown(ChunkMap *m);
/* Collects finished jobs and schedules new ones around pos. */
void chunkmap_update(ChunkMap *m, vec3 pos);
void chunkmap_draw(ChunkMap *m);
/* Loaded chunk or NULL. */
Chunk *chunkmap_get(ChunkMap *m, int cx, int cy, int cz);
/* Block at world coordinates; *known is false if the chunk is not generated yet. */
Block chunkmap_block(ChunkMap *m, int wx, int wy, int wz, bool *known);
/* Light at a world cell (r, g, b, fire as 0..15); false if not known yet. */
bool chunkmap_light(ChunkMap *m, int wx, int wy, int wz, u8 out[4]);
/* True once every chunk within the mesh radius has a mesh. */
bool chunkmap_settled(ChunkMap *m, vec3 pos);

#endif
