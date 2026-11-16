#include "world/mesher.h"
#include "world/realms.h"

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
                const BlockDef *bd = &BLOCKS[s->blocks[i] & BLOCK_MAT];
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
                if (!ci || (ci->arch != ARCH_EXTERIOR && ci->arch != ARCH_POCKET && ci->arch != ARCH_REALM)) continue;
                u8 *l = &s->light[i * 4];
                if (ci->arch == ARCH_REALM) { /* each realm's own sky */
                    int r = realm_of_flavor(ci->flavor);
                    if (r < 0 || (REALMS[r].sky[0] | REALMS[r].sky[1] | REALMS[r].sky[2]) == 0) continue;
                    l[0] = (u8)MAXI(l[0], REALMS[r].sky[0]); l[1] = (u8)MAXI(l[1], REALMS[r].sky[1]); l[2] = (u8)MAXI(l[2], REALMS[r].sky[2]);
                } else if (ci->arch == ARCH_POCKET) { /* the light of three moons in the pocket realm */
                    l[0] = (u8)MAXI(l[0], 10); l[1] = (u8)MAXI(l[1], 11); l[2] = (u8)MAXI(l[2], 14);
                } else {
                    l[0] = (u8)MAXI(l[0], 7); l[1] = (u8)MAXI(l[1], 8); l[2] = (u8)MAXI(l[2], 12);
                }
                if (!s->inq[i]) { s->queue[tail++] = i; s->inq[i] = 1; }
            }

    /* the towns of the friendly peoples (towns.h): lamps everywhere, a warm light in all the hall */
    for (int i = 0; i < MESH_PVOL; i++) {
        if (block_opaque(s->blocks[i])) continue;
        int px = i % MESH_P, pz = (i / MESH_P) % MESH_P, py = i / (MESH_P * MESH_P);
        const ChunkInfo *ci = in->info[chunk_of(px) + chunk_of(py) * 3 + chunk_of(pz) * 9];
        if (!ci || !ci->has_struct || ci->st.type != STRUCT_HALL || ci->st.variant != 3) continue;
        u8 *l = &s->light[i * 4];
        l[0] = (u8)MAXI(l[0], 12); l[1] = (u8)MAXI(l[1], 10); l[2] = (u8)MAXI(l[2], 8);
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
                      float tint, int liquid_drop, u8 xflags) {
    int px = x + MESH_PAD, py = y + MESH_PAD, pz = z + MESH_PAD;
    MeshVertex v[4];
    int ao[4];
    bool emissive = BLOCKS[b & BLOCK_MAT].emissive;
    for (int k = 0; k < 4; k++) {
        const int *c = FACE_CORNERS[face][k];
        float l[4];
        face_vertex_light(s, px, py, pz, face, c, l, &ao[k]);
        float shade = FACE_SHADE[face] * AO_CURVE[ao[k]] * tint;
        v[k].x = (i16)((x + c[0]) * 8);
        v[k].y = (i16)((y + c[1]) * 8 - (c[1] ? liquid_drop : 0));
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
        v[k].flags = (u8)((emissive ? VF_EMISSIVE : 0) | (BLOCKS[b & BLOCK_MAT].render == RENDER_LIQUID ? VF_LIQUID : 0) | xflags);
        if (emissive) { v[k].r = v[k].g = v[k].b = (u8)(255 * shade); }
    }
    MeshVertex *q = vb_quad(vb);
    /* flip the diagonal so ambient occlusion interpolates evenly */
    if (ao[0] + ao[2] < ao[1] + ao[3]) { q[0] = v[1]; q[1] = v[2]; q[2] = v[3]; q[3] = v[0]; }
    else { q[0] = v[0]; q[1] = v[1]; q[2] = v[2]; q[3] = v[3]; }
}

/* hgt: how tall it stands (a shoot, a dying fire), 1 for full; a vine hangs from its top. */
static void emit_sprite(VBuf *vb, const MeshScratch *s, int x, int y, int z, Block b, u16 tile, float hgt) {
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
            if (b == B_VINE) v[k].y = (i16)(top ? (y + 1) * 8 : (y + 1) * 8 - (int)lroundf(8.0f * hgt));
            else v[k].y = (i16)(top ? y * 8 + (int)lroundf(8.0f * hgt) : y * 8);
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

/* ---------- broken blocks: their 25 cm pieces ---------- */

/* The pieces of a cell of the padded neighbourhood (only blocks with BLOCK_CARVED have any). */
static u64 cell_mask(const MeshInput *in, const MeshScratch *s, int px, int py, int pz) {
    Block b = s->blocks[pidx(px, py, pz)];
    if (!(b & BLOCK_CARVED)) return block_opaque(b) ? CARVE_FULL : 0;
    int x = px - MESH_PAD, y = py - MESH_PAD, z = pz - MESH_PAD;
    int cx = x < 0 ? -1 : (x >= CHUNK ? 1 : 0), cy = y < 0 ? -1 : (y >= CHUNK ? 1 : 0), cz = z < 0 ? -1 : (z >= CHUNK ? 1 : 0);
    const CarveSet *cs = in->carve[(cx + 1) + (cy + 1) * 3 + (cz + 1) * 9];
    return carve_get(cs, cidx(x - cx * CHUNK, y - cy * CHUNK, z - cz * CHUNK));
}

static void emit_pieces(VBuf *vb, const MeshInput *in, const MeshScratch *s, int x, int y, int z, Block b, u16 tile, float tint) {
    int px = x + MESH_PAD, py = y + MESH_PAD, pz = z + MESH_PAD;
    u64 mask = cell_mask(in, s, px, py, pz);
    if (!mask) return;
    const u8 *l = &s->light[pidx(px, py, pz) * 4];
    u64 nb[6];
    for (int f = 0; f < 6; f++) nb[f] = cell_mask(in, s, px + FACE_N[f][0], py + FACE_N[f][1], pz + FACE_N[f][2]);
    bool emissive = BLOCKS[b & BLOCK_MAT].emissive;
    for (int sy = 0; sy < 4; sy++)
        for (int sz = 0; sz < 4; sz++)
            for (int sx = 0; sx < 4; sx++) {
                if (!(mask >> carve_bit(sx, sy, sz) & 1)) continue;
                for (int f = 0; f < 6; f++) {
                    int qx = sx + FACE_N[f][0], qy = sy + FACE_N[f][1], qz = sz + FACE_N[f][2];
                    bool inside = qx >= 0 && qx < 4 && qy >= 0 && qy < 4 && qz >= 0 && qz < 4;
                    u64 m = inside ? mask : nb[f];
                    int bit = carve_bit((qx + 4) & 3, (qy + 4) & 3, (qz + 4) & 3);
                    if (m >> bit & 1) continue; /* hidden by a piece next to it */
                    float shade = FACE_SHADE[f] * tint * (inside ? 0.85f : 1.0f);
                    MeshVertex *q = vb_quad(vb);
                    for (int k = 0; k < 4; k++) {
                        const int *c = FACE_CORNERS[f][k];
                        int vx = sx + c[0], vy = sy + c[1], vz = sz + c[2];
                        q[k].x = (i16)(x * 8 + vx * 2);
                        q[k].y = (i16)(y * 8 + vy * 2);
                        q[k].z = (i16)(z * 8 + vz * 2);
                        if (f == 2 || f == 3) { q[k].u = (u8)(vx * 4); q[k].v = (u8)(vz * 4); }
                        else if (f <= 1) { q[k].u = (u8)(vz * 4); q[k].v = (u8)(16 - vy * 4); }
                        else { q[k].u = (u8)(vx * 4); q[k].v = (u8)(16 - vy * 4); }
                        q[k].tile = tile;
                        q[k].r = (u8)clampf(brightness(l[0]) * shade * 255.0f, 0, 255);
                        q[k].g = (u8)clampf(brightness(l[1]) * shade * 255.0f, 0, 255);
                        q[k].b = (u8)clampf(brightness(l[2]) * shade * 255.0f, 0, 255);
                        q[k].f = (u8)clampf(l[3] / 15.0f * 255.0f, 0, 255);
                        q[k].n = (u8)f;
                        q[k].flags = emissive ? VF_EMISSIVE : 0;
                        if (emissive) q[k].r = q[k].g = q[k].b = (u8)(255 * shade);
                    }
                }
            }
}

/* The level 1..8 of a liquid cell (padded coordinates), 8 where nothing set one. */
static int fluid_level(const MeshInput *in, int px, int py, int pz) {
    int x = px - MESH_PAD, y = py - MESH_PAD, z = pz - MESH_PAD;
    int cx = x < 0 ? -1 : (x >= CHUNK ? 1 : 0), cy = y < 0 ? -1 : (y >= CHUNK ? 1 : 0), cz = z < 0 ? -1 : (z >= CHUNK ? 1 : 0);
    const u8 *fl = in->fluid[(cx + 1) + (cy + 1) * 3 + (cz + 1) * 9];
    int level = fl ? fl[cidx(x - cx * CHUNK, y - cy * CHUNK, z - cz * CHUNK)] : 0;
    return level ? level : 8;
}

/* The marks of a cell (padded coordinates; chunkmap.h STAIN_*). */
static u8 stain_at(const MeshInput *in, int px, int py, int pz) {
    int x = px - MESH_PAD, y = py - MESH_PAD, z = pz - MESH_PAD;
    int cx = x < 0 ? -1 : (x >= CHUNK ? 1 : 0), cy = y < 0 ? -1 : (y >= CHUNK ? 1 : 0), cz = z < 0 ? -1 : (z >= CHUNK ? 1 : 0);
    const u8 *st = in->stain[(cx + 1) + (cy + 1) * 3 + (cz + 1) * 9];
    return st ? st[cidx(x - cx * CHUNK, y - cy * CHUNK, z - cz * CHUNK)] : 0;
}

/* How much darker a block's faces are drawn for its marks: soot and scorch, and wet stone (marked
 * wet, or with water beside it). */
static float stain_tint(const MeshInput *in, const MeshScratch *s, int px, int py, int pz) {
    u8 st = stain_at(in, px, py, pz);
    float k = 1.0f - 0.15f * (float)STAIN_SOOT(st) - 0.12f * (float)STAIN_SCORCH(st);
    bool wet = STAIN_WET(st) > 0;
    for (int f = 0; f < 6 && !wet; f++) wet = (s->blocks[pidx(px + FACE_N[f][0], py + FACE_N[f][1], pz + FACE_N[f][2])] & BLOCK_MAT) == B_WATER;
    if (wet) k *= 0.78f;
    return k;
}

/* Which way a liquid runs (vertex flags VF_FLOW and the direction): along the surface toward the
 * steepest drop, and down the sides where it falls; 0 if it lies still. */
static u8 flow_flags(const MeshInput *in, const MeshScratch *s, int px, int py, int pz, Block b, bool side) {
    Block below = s->blocks[pidx(px, py - 1, pz)] & BLOCK_MAT;
    bool falls = !BLOCKS[below].solid && (below != b || fluid_level(in, px, py - 1, pz) < 8);
    if (side) return falls ? (u8)(VF_FLOW | (4 << 4)) : 0;
    int me = fluid_level(in, px, py, pz), best = -1, drop = 0;
    for (int d = 0; d < 4; d++) {
        static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
        int qx = px + DX[d], qz = pz + DZ[d];
        Block nb = s->blocks[pidx(qx, py, qz)] & BLOCK_MAT;
        int dd = 0;
        if (nb == b) dd = me - fluid_level(in, qx, py, qz);
        else if (nb == B_AIR && !BLOCKS[s->blocks[pidx(qx, py - 1, qz)] & BLOCK_MAT].solid) dd = me; /* an edge it spills over */
        if (dd > drop) { drop = dd; best = d; }
    }
    return best >= 0 ? (u8)(VF_FLOW | (best << 4)) : 0;
}

/* A cell of lava that does not fill its cell (a flow): drawn like a liquid, at its height. */
static bool thin_lava(const MeshInput *in, const MeshScratch *s, int px, int py, int pz) {
    return s->blocks[pidx(px, py, pz)] == B_LAVA && fluid_level(in, px, py, pz) < 8;
}

/* How far a liquid's top sits below the top of its cell, in 1/8 m (sim.c's levels). */
static int liquid_drop(const MeshInput *in, const MeshScratch *s, int x, int y, int z) {
    /* a liquid with liquid above it fills its cell */
    if (block_mat(s->blocks[pidx(x + MESH_PAD, y + MESH_PAD + 1, z + MESH_PAD)]) == block_mat(s->blocks[pidx(x + MESH_PAD, y + MESH_PAD, z + MESH_PAD)])) return 0;
    int level = fluid_level(in, x + MESH_PAD, y + MESH_PAD, z + MESH_PAD);
    return MAXI(1, MINI(7, 8 - level));
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
                Block raw = s->blocks[pidx(px, py, pz)];
                Block b = raw & BLOCK_MAT;
                const BlockDef *bd = &BLOCKS[b];
                if (bd->render == RENDER_NONE) continue;
                u64 h = hash3(0x7A11, ox + x, oy + y, oz + z, 9);
                u16 tile = (u16)(in->tile_base + b * TEX_VARIANTS + (int)(h % TEX_VARIANTS));
                if (raw & BLOCK_CARVED) {
                    emit_pieces(&solid, in, s, x, y, z, b, tile, (0.93f + 0.1f * hash_unit(h >> 3)) * stain_tint(in, s, px, py, pz));
                    /* water held in among its pieces: its surface */
                    const u8 *fl = in->fluid[13];
                    int wl = fl ? fl[cidx(x, y, z)] : 0;
                    if (wl > 0 && !block_opaque(s->blocks[pidx(px, py + 1, pz)])) {
                        u16 wt = (u16)(in->tile_base + B_WATER * TEX_VARIANTS + (int)(h % TEX_VARIANTS));
                        emit_face(&solid, s, x, y, z, 2, B_WATER, wt, 1.0f, MAXI(1, MINI(7, 8 - wl)), 0);
                    }
                    continue;
                }
                if (bd->render == RENDER_CROSS) {
                    /* plants grow from shoots; a fire is as big as its fuel */
                    float hgt = 1.0f;
                    int age = STAIN_AGE(stain_at(in, px, py, pz));
                    if (age == 1) hgt = 0.45f;
                    else if (age == 2) hgt = 0.75f;
                    if (b == B_FIRE) {
                        int fuel = fluid_level(in, px, py, pz); /* (8 where the world lit it) */
                        if (fuel < 8) hgt = 0.35f + 0.09f * (float)fuel;
                    }
                    emit_sprite(&sprite, s, x, y, z, b, tile, hgt);
                    continue;
                }
                float tint = 0.93f + 0.1f * hash_unit(h >> 3);
                if (bd->render == RENDER_CUBE && b != B_LAVA) tint *= stain_tint(in, s, px, py, pz);
                bool liquid = bd->render == RENDER_LIQUID || (b == B_LAVA && thin_lava(in, s, px, py, pz));
                if (b == B_LAVA && liquid) tint *= 0.45f + 0.07f * (float)fluid_level(in, px, py, pz); /* a thin flow cools darker */
                for (int f = 0; f < 6; f++) {
                    Block nb = s->blocks[pidx(px + FACE_N[f][0], py + FACE_N[f][1], pz + FACE_N[f][2])];
                    if (block_opaque(nb) && !(nb == B_LAVA && thin_lava(in, s, px + FACE_N[f][0], py + FACE_N[f][1], pz + FACE_N[f][2]))) continue;
                    if (liquid && nb == b) {
                        /* between two cells of the same liquid only a step in the surface shows */
                        if (f == 2 || f == 3 || FACE_N[f][1]) continue;
                        int pn = pidx(px + FACE_N[f][0], py, pz + FACE_N[f][2]);
                        if (s->blocks[pn + (pidx(0, 1, 0) - pidx(0, 0, 0))] == b) continue; /* we are below its surface */
                        if (fluid_level(in, px + FACE_N[f][0], py, pz + FACE_N[f][2]) >= fluid_level(in, px, py, pz)) continue;
                        if (block_mat(s->blocks[pidx(px, py + 1, pz)]) == b) continue;
                    }
                    u8 xf = bd->render == RENDER_LIQUID ? flow_flags(in, s, px, py, pz, b, f != 2) : 0;
                    emit_face(&solid, s, x, y, z, f, b, tile, tint, liquid ? liquid_drop(in, s, x, y, z) : 0, xf);
                }
            }
    out->light = malloc((size_t)CHUNK_VOL * 4);
    for (int y = 0; y < CHUNK; y++)
        for (int z = 0; z < CHUNK; z++)
            memcpy(&out->light[cidx(0, y, z) * 4], &s->light[pidx(MESH_PAD, y + MESH_PAD, z + MESH_PAD) * 4], CHUNK * 4);
    out->nsolid = solid.n / 4;
    out->nsprite = sprite.n / 4;
    out->verts = malloc(sizeof(MeshVertex) * (size_t)(solid.n + sprite.n + 1));
    if (solid.n) memcpy(out->verts, solid.v, sizeof(MeshVertex) * (size_t)solid.n);
    if (sprite.n) memcpy(out->verts + solid.n, sprite.v, sizeof(MeshVertex) * (size_t)sprite.n);
    free(solid.v);
    free(sprite.v);
}
