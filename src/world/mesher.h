#ifndef FC_MESHER_H
#define FC_MESHER_H

#include "world/layout.h"

/* 16-byte vertex. Positions are chunk-local in 1/8 m units. */
typedef struct MeshVertex {
    i16 x, y, z;
    u8 u, v;      /* texel coordinates inside the tile, 0..16 */
    u16 tile;     /* atlas tile */
    u8 r, g, b;   /* baked light incl. ambient occlusion */
    u8 f;         /* share of firelight (drives flicker) */
    u8 n;         /* face direction 0..5, 6 = sprite */
    u8 flags;
} MeshVertex;

enum { VF_EMISSIVE = 1, VF_SPRITE = 2, VF_LIQUID = 4 };

/* Light spreads at most 15 cells, so a 15-cell border of neighbour data is enough. */
#define MESH_PAD 15
#define MESH_P (CHUNK + 2 * MESH_PAD)
#define MESH_PVOL (MESH_P * MESH_P * MESH_P)

typedef struct MeshScratch {
    Block *blocks;   /* MESH_PVOL */
    u8 *light;       /* MESH_PVOL * 4: r, g, b, fire */
    int *queue;      /* MESH_PVOL */
    u8 *inq;         /* MESH_PVOL */
} MeshScratch;

typedef struct MeshInput {
    /* 3x3x3 neighbourhood, index (dx+1) + (dy+1)*3 + (dz+1)*9; NULL = treat as rock */
    const Block *blocks[27];
    const ChunkInfo *info[27];
    int tile_base; /* first atlas tile of the centre chunk's theme */
} MeshInput;

typedef struct MeshOutput {
    MeshVertex *verts; /* solid quads first, then sprite quads; 4 vertices per quad */
    int nsolid, nsprite;
} MeshOutput;

MeshScratch *mesh_scratch_new(void);
void mesh_scratch_free(MeshScratch *s);
/* Fills out->verts (malloc'd, caller frees). */
void mesh_build(const MeshInput *in, MeshScratch *s, MeshOutput *out);

/* Only the light field (for tests and tools): returns the light at a centre-chunk cell. */
void mesh_light_at(const MeshScratch *s, int x, int y, int z, u8 out[4]);

#endif
