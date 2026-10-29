#include "world/mesher.h"

#include "world/texgen.h"

static inline int pidx(int x, int y, int z) { return (y * MESH_P + z) * MESH_P + x; }

MeshScratch *mesh_scratch_new(void) {
    MeshScratch *s = calloc(1, sizeof *s);
    s->blocks = malloc(MESH_PVOL);
    s->light = malloc((size_t)MESH_PVOL * 4);
    s->queue = malloc(sizeof(int) * MESH_PVOL);
    s->inq = malloc(MESH_PVOL);
    return s;
}

void mesh_scratch_free(MeshScratch *s) {
    if (!s) return;
    free(s->blocks); free(s->light); free(s->queue); free(s->inq);
    free(s);
}

void mesh_light_at(const MeshScratch *s, int x, int y, int z, u8 out[4]) {
    const u8 *l = &s->light[pidx(x + MESH_PAD, y + MESH_PAD, z + MESH_PAD) * 4];
    memcpy(out, l, 4);
}

/* ---------- light ---------- */

static void gather(const MeshInput *in, MeshScratch *s) {
    for (int py = 0; py < MESH_P; py++) {
        int y = py - MESH_PAD, cy = y < 0 ? -1 : (y >= CHUNK ? 1 : 0), ly = y - cy * CHUNK;
        for (int pz = 0; pz < MESH_P; pz++) {
            int z = pz - MESH_PAD, cz = z < 0 ? -1 : (z >= CHUNK ? 1 : 0), lz = z - cz * CHUNK;
            for (int px = 0; px < MESH_P; px++) {
                int x = px - MESH_PAD, cx = x < 0 ? -1 : (x >= CHUNK ? 1 : 0), lx = x - cx * CHUNK;
                const Block *b = in->blocks[(cx + 1) + (cy + 1) * 3 + (cz + 1) * 9];
                s->blocks[pidx(px, py, pz)] = b ? b[cidx(lx, ly, lz)] : B_ROCK;
            }
        }
    }
}

static int chunk_of(int p) { return p < MESH_PAD ? 0 : (p >= MESH_PAD + CHUNK ? 2 : 1); }

static void compute_light(const MeshInput *in, MeshScratch *s) {
    memset(s->light, 0, (size_t)MESH_PVOL * 4);
    memset(s->inq, 0, MESH_PVOL);
    ThemeInfo ti[27];
    bool have[27] = { false };
    int head = 0, tail = 0;

    /* emissive blocks */
    for (int py = 0; py < MESH_P; py++)
        for (int pz = 0; pz < MESH_P; pz++)
            for (int px = 0; px < MESH_P; px++) {
                int i = pidx(px, py, pz);
                const BlockDef *bd = &BLOCKS[s->blocks[i]];
                if (!bd->light) continue;
                int ci = chunk_of(px) + chunk_of(py) * 3 + chunk_of(pz) * 9;
                if (!in->info[ci]) continue;
                if (!have[ci]) { theme_info(in->info[ci]->theme, &ti[ci]); have[ci] = true; }
                const u8 *col = ti[ci].light[bd->light_kind];
                u8 *l = &s->light[i * 4];
                l[0] = (u8)((col[0] * bd->light + 7) / 15);
                l[1] = (u8)((col[1] * bd->light + 7) / 15);
                l[2] = (u8)((col[2] * bd->light + 7) / 15);
                l[3] = bd->fire ? bd->light : 0;
                s->queue[tail++] = i;
                s->inq[i] = 1;
            }

    /* moonlight on open-sky cells outside */
    for (int pz = 0; pz < MESH_P; pz++)
        for (int px = 0; px < MESH_P; px++)
            for (int py = MESH_P - 1; py >= 0; py--) {
                int i = pidx(px, py, pz);
                if (block_opaque(s->blocks[i])) break;
                const ChunkInfo *ci = in->info[chunk_of(px) + chunk_of(py) * 3 + chunk_of(pz) * 9];
                if (!ci || ci->arch != ARCH_EXTERIOR) continue;
                u8 *l = &s->light[i * 4];
                l[0] = (u8)MAXI(l[0], 7); l[1] = (u8)MAXI(l[1], 8); l[2] = (u8)MAXI(l[2], 12);
                if (!s->inq[i]) { s->queue[tail++] = i; s->inq[i] = 1; }
            }

    static const int OFF[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    int count = tail;
    while (count > 0) {
        int c = s->queue[head];
        head = (head + 1) % MESH_PVOL;
        count--;
        s->inq[c] = 0;
        const u8 *lc = &s->light[c * 4];
        if (lc[0] <= 1 && lc[1] <= 1 && lc[2] <= 1) continue;
        int px = c % MESH_P, pz = (c / MESH_P) % MESH_P, py = c / (MESH_P * MESH_P);
        u8 nr = lc[0] ? lc[0] - 1 : 0, ng = lc[1] ? lc[1] - 1 : 0, nb = lc[2] ? lc[2] - 1 : 0, nf = lc[3] ? lc[3] - 1 : 0;
        for (int d = 0; d < 6; d++) {
            int qx = px + OFF[d][0], qy = py + OFF[d][1], qz = pz + OFF[d][2];
            if ((unsigned)qx >= MESH_P || (unsigned)qy >= MESH_P || (unsigned)qz >= MESH_P) continue;
            int n = pidx(qx, qy, qz);
            if (block_opaque(s->blocks[n])) continue;
            u8 *ln = &s->light[n * 4];
            if (nr > ln[0] || ng > ln[1] || nb > ln[2] || nf > ln[3]) {
                if (nr > ln[0]) ln[0] = nr;
                if (ng > ln[1]) ln[1] = ng;
                if (nb > ln[2]) ln[2] = nb;
                if (nf > ln[3]) ln[3] = nf;
                if (!s->inq[n]) {
                    s->queue[(head + count) % MESH_PVOL] = n;
                    s->inq[n] = 1;
                    count++;
                }
            }
        }
    }
}

/* ---------- geometry ---------- */

typedef struct VBuf { MeshVertex *v; int n, cap; } VBuf;

static MeshVertex *vb_quad(VBuf *b) {
    if (b->n + 4 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 4096;
        b->v = realloc(b->v, sizeof(MeshVertex) * (size_t)b->cap);
    }
    MeshVertex *q = &b->v[b->n];
    b->n += 4;
    return q;
}

/* Corner positions (in 0/1) for each face, counter-clockwise seen from outside,
 * and the texel coordinates for each corner. Face order: +X -X +Y -Y +Z -Z. */
static const int FACE_CORNERS[6][4][3] = {
    { { 1, 0, 1 }, { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 } },
    { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 } },
    { { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 } },
    { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } },
    { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } },
    { { 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } },
};
static const u8 FACE_UV[4][2] = { { 0, 16 }, { 16, 16 }, { 16, 0 }, { 0, 0 } };
static const int FACE_N[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
static const float FACE_SHADE[6] = { 0.82f, 0.82f, 1.0f, 0.55f, 0.7f, 0.7f };
static const float AO_CURVE[4] = { 0.42f, 0.62f, 0.8f, 1.0f };

static float brightness(float level) { return level <= 0.01f ? 0.0f : powf(0.85f, 15.0f - level); }

static void face_vertex_light(const MeshScratch *s, int px, int py, int pz, int face, const int corner[3],
                              float out[4], int *ao) {
    const int *n = FACE_N[face];
    int ax = px + n[0], ay = py + n[1], az = pz + n[2];
    /* the two tangent axes */
    int t1 = n[0] ? 1 : 0, t2 = n[2] ? 1 : 2;
    if (n[1]) { t1 = 0; t2 = 2; }
    int o1[3] = { 0, 0, 0 }, o2[3] = { 0, 0, 0 };
    o1[t1] = corner[t1] ? 1 : -1;
    o2[t2] = corner[t2] ? 1 : -1;
    int c0 = pidx(ax, ay, az);
    int c1 = pidx(ax + o1[0], ay + o1[1], az + o1[2]);
    int c2 = pidx(ax + o2[0], ay + o2[1], az + o2[2]);
    int c3 = pidx(ax + o1[0] + o2[0], ay + o1[1] + o2[1], az + o1[2] + o2[2]);
    bool s1 = block_opaque(s->blocks[c1]), s2 = block_opaque(s->blocks[c2]), s3 = block_opaque(s->blocks[c3]);
    *ao = (s1 && s2) ? 0 : 3 - (s1 + s2 + s3);
    int cells[4] = { c0, c1, c2, c3 };
    bool use[4] = { true, !s1, !s2, !s3 && !(s1 && s2) };
    float sum[4] = { 0, 0, 0, 0 };
    int cnt = 0;
    for (int k = 0; k < 4; k++) {
        if (!use[k]) continue;
        const u8 *l = &s->light[cells[k] * 4];
        for (int ch = 0; ch < 4; ch++) sum[ch] += l[ch];
        cnt++;
    }
    for (int ch = 0; ch < 4; ch++) out[ch] = sum[ch] / (float)cnt;
}

static void emit_face(VBuf *vb, const MeshScratch *s, int x, int y, int z, int face, Block b, u16 tile,
                      float tint, bool liquid_top) {
    int px = x + MESH_PAD, py = y + MESH_PAD, pz = z + MESH_PAD;
    MeshVertex v[4];
    int ao[4];
    bool emissive = BLOCKS[b].emissive;
    for (int k = 0; k < 4; k++) {
        const int *c = FACE_CORNERS[face][k];
        float l[4];
        face_vertex_light(s, px, py, pz, face, c, l, &ao[k]);
        float shade = FACE_SHADE[face] * AO_CURVE[ao[k]] * tint;
        v[k].x = (i16)((x + c[0]) * 8);
        v[k].y = (i16)((y + c[1]) * 8 - ((liquid_top && c[1]) ? 1 : 0));
        v[k].z = (i16)((z + c[2]) * 8);
        v[k].u = FACE_UV[k][0];
        v[k].v = FACE_UV[k][1];
        if (face == 2 || face == 3) { v[k].u = (u8)(c[0] * 16); v[k].v = (u8)(c[2] * 16); }
        v[k].tile = tile;
        v[k].r = (u8)clampf(brightness(l[0]) * shade * 255.0f, 0, 255);
        v[k].g = (u8)clampf(brightness(l[1]) * shade * 255.0f, 0, 255);
        v[k].b = (u8)clampf(brightness(l[2]) * shade * 255.0f, 0, 255);
        v[k].f = (u8)clampf(l[3] / 15.0f * 255.0f, 0, 255);
        v[k].n = (u8)face;
        v[k].flags = (u8)((emissive ? VF_EMISSIVE : 0) | (BLOCKS[b].render == RENDER_LIQUID ? VF_LIQUID : 0));
        if (emissive) { v[k].r = v[k].g = v[k].b = (u8)(255 * shade); }
    }
    MeshVertex *q = vb_quad(vb);
    /* flip the diagonal so ambient occlusion interpolates evenly */
    if (ao[0] + ao[2] < ao[1] + ao[3]) { q[0] = v[1]; q[1] = v[2]; q[2] = v[3]; q[3] = v[0]; }
    else { q[0] = v[0]; q[1] = v[1]; q[2] = v[2]; q[3] = v[3]; }
}

static void emit_sprite(VBuf *vb, const MeshScratch *s, int x, int y, int z, Block b, u16 tile) {
    int px = x + MESH_PAD, py = y + MESH_PAD, pz = z + MESH_PAD;
    const u8 *l = &s->light[pidx(px, py, pz) * 4];
    int lo = 1, hi = 7, ox = 0, oz = 0;
    if (b == B_TORCH) {
        lo = 2; hi = 6;
        /* lean against the first wall found */
        for (int d = 0; d < 4; d++) {
            static const int DX[4] = { 1, 0, -1, 0 }, DZ[4] = { 0, 1, 0, -1 };
            if (block_opaque(s->blocks[pidx(px + DX[d], py, pz + DZ[d])])) { ox = DX[d] * 2; oz = DZ[d] * 2; break; }
        }
    }
    bool emissive = BLOCKS[b].emissive;
    u8 r = (u8)clampf(brightness(l[0]) * 255.0f, 0, 255);
    u8 g = (u8)clampf(brightness(l[1]) * 255.0f, 0, 255);
    u8 bb = (u8)clampf(brightness(l[2]) * 255.0f, 0, 255);
    if (emissive) r = g = bb = 255;
    const int pts[2][2][2] = { { { lo, lo }, { hi, hi } }, { { lo, hi }, { hi, lo } } };
    for (int q = 0; q < 2; q++) {
        MeshVertex *v = vb_quad(vb);
        for (int k = 0; k < 4; k++) {
            int end = (k == 1 || k == 2) ? 1 : 0;
            int top = (k >= 2) ? 1 : 0;
            v[k].x = (i16)(x * 8 + pts[q][end][0] + ox);
            v[k].y = (i16)((y + top) * 8);
            v[k].z = (i16)(z * 8 + pts[q][end][1] + oz);
            v[k].u = (u8)(end * 16);
            v[k].v = (u8)(top ? 0 : 16);
            v[k].tile = tile;
            v[k].r = r; v[k].g = g; v[k].b = bb;
            v[k].f = (u8)clampf(l[3] / 15.0f * 255.0f, 0, 255);
            v[k].n = 6;
            v[k].flags = (u8)(VF_SPRITE | (emissive ? VF_EMISSIVE : 0));
        }
    }
}

void mesh_build(const MeshInput *in, MeshScratch *s, MeshOutput *out) {
    gather(in, s);
    compute_light(in, s);
    VBuf solid = { 0 }, sprite = { 0 };
    const ChunkInfo *ci = in->info[13];
    int ox = ci->cx * CHUNK, oy = ci->cy * CHUNK, oz = ci->cz * CHUNK;
    for (int y = 0; y < CHUNK; y++)
        for (int z = 0; z < CHUNK; z++)
            for (int x = 0; x < CHUNK; x++) {
                int px = x + MESH_PAD, py = y + MESH_PAD, pz = z + MESH_PAD;
                Block b = s->blocks[pidx(px, py, pz)];
                const BlockDef *bd = &BLOCKS[b];
                if (bd->render == RENDER_NONE) continue;
                u64 h = hash3(0x7A11, ox + x, oy + y, oz + z, 9);
                u16 tile = (u16)(in->tile_base + b * TEX_VARIANTS + (int)(h % TEX_VARIANTS));
                if (bd->render == RENDER_CROSS) {
                    emit_sprite(&sprite, s, x, y, z, b, tile);
                    continue;
                }
                float tint = 0.93f + 0.1f * hash_unit(h >> 3);
                bool liquid = bd->render == RENDER_LIQUID;
                for (int f = 0; f < 6; f++) {
                    Block nb = s->blocks[pidx(px + FACE_N[f][0], py + FACE_N[f][1], pz + FACE_N[f][2])];
                    if (block_opaque(nb)) continue;
                    if (liquid && nb == b) continue;
                    bool liquid_top = liquid && f == 2;
                    emit_face(&solid, s, x, y, z, f, b, tile, tint, liquid_top);
                }
            }
    out->nsolid = solid.n / 4;
    out->nsprite = sprite.n / 4;
    out->verts = malloc(sizeof(MeshVertex) * (size_t)(solid.n + sprite.n + 1));
    if (solid.n) memcpy(out->verts, solid.v, sizeof(MeshVertex) * (size_t)solid.n);
    if (sprite.n) memcpy(out->verts + solid.n, sprite.v, sizeof(MeshVertex) * (size_t)sprite.n);
    free(solid.v);
    free(sprite.v);
}
