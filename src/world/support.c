#include "world/support.h"

#define SN (2 * SUPPORT_MAX_R + 1)
#define SV (SN * SN * SN)
#define FAR 255

static u8 mat_[SV];      /* solid material, 0 for none */
static u8 cost_a[SV], cost_b[SV];
static u8 fix_[SV];

static int span_of(int m) { return SUBSTANCES[m].span; }

/* Least load on each solid cell: 0 at the edge of the box and fixed cells, and for a cell resting
 * on a held one; reaching sideways or hanging from above costs one more each step. A cell carries on only
 * if it is itself held (its cost within its span). A small bucket queue: costs stay below 32. */
static void relax(int n, u8 *cost) {
    static int q[32][SV];
    static int qn[32], qr[32];
    memset(qn, 0, sizeof qn);
    memset(qr, 0, sizeof qr);
    for (int i = 0; i < SV; i++) cost[i] = FAR;
    for (int y = 0; y < n; y++)
        for (int z = 0; z < n; z++)
            for (int x = 0; x < n; x++) {
                int i = (y * SN + z) * SN + x;
                if (!mat_[i]) continue;
                bool edge = x == 0 || y == 0 || z == 0 || x == n - 1 || y == n - 1 || z == n - 1;
                if (edge || fix_[i]) { cost[i] = 0; q[0][qn[0]++] = i; }
            }
    static const int D[6][3] = { { 0, 1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 } };
    /* (a cell resting on a held one goes back to cost 0: the queue then goes back down) */
    for (int c = 0; c < 32;) {
        if (qr[c] >= qn[c]) { c++; continue; }
        {
            int i = q[c][qr[c]++];
            if (cost[i] != c) continue;
            if (c > span_of(mat_[i])) continue; /* not held: it holds nothing */
            int x = i % SN, z = (i / SN) % SN, y = i / (SN * SN);
            for (int d = 0; d < 6; d++) {
                int ax = x + D[d][0], ay = y + D[d][1], az = z + D[d][2];
                if (ax < 0 || ay < 0 || az < 0 || ax >= n || ay >= n || az >= n) continue;
                int j = (ay * SN + az) * SN + ax;
                if (!mat_[j]) continue;
                int nc = d == 0 ? 0 : c + 1; /* resting on a held cell (d == 0: j is above i) is held fully */
                if (nc < cost[j] && nc < 32 && qn[nc] < SV) {
                    cost[j] = (u8)nc;
                    q[nc][qn[nc]++] = j;
                    if (nc < c) c = nc;
                }
            }
        }
    }
}

int support_falls(SupportBlockFn block, SupportFixedFn fixed, void *user, int cx, int cy, int cz, int r,
                  const SupportCell *removed, const u8 *mat, int nremoved, SupportCell *out, int max) {
    if (r > SUPPORT_MAX_R) r = SUPPORT_MAX_R;
    int n = 2 * r + 1, ox = cx - r, oy = cy - r, oz = cz - r;
    memset(mat_, 0, sizeof mat_);
    memset(fix_, 0, sizeof fix_);
    for (int y = 0; y < n; y++)
        for (int z = 0; z < n; z++)
            for (int x = 0; x < n; x++) {
                int b = block(user, ox + x, oy + y, oz + z), i = (y * SN + z) * SN + x;
                if (b < 0) { mat_[i] = B_ROCK; fix_[i] = 1; continue; } /* unknown: as if solid and held */
                if (b == B_AIR || !BLOCKS[b].solid || SUBSTANCES[b].span == 0) continue;
                mat_[i] = (u8)b;
                if (fixed && fixed(user, ox + x, oy + y, oz + z)) fix_[i] = 1;
            }
    relax(n, cost_a);
    /* as it was: the removed cells back in */
    for (int k = 0; k < nremoved; k++) {
        int x = removed[k].x - ox, y = removed[k].y - oy, z = removed[k].z - oz;
        if (x < 0 || y < 0 || z < 0 || x >= n || y >= n || z >= n) continue;
        if (mat[k] != B_AIR && BLOCKS[mat[k]].solid && SUBSTANCES[mat[k]].span) mat_[(y * SN + z) * SN + x] = mat[k];
    }
    relax(n, cost_b);
    for (int k = 0; k < nremoved; k++) { /* and out again */
        int x = removed[k].x - ox, y = removed[k].y - oy, z = removed[k].z - oz;
        if (x < 0 || y < 0 || z < 0 || x >= n || y >= n || z >= n) continue;
        mat_[(y * SN + z) * SN + x] = 0;
    }
    int nout = 0;
    for (int y = n - 1; y >= 0 && nout < max; y--) /* top first */
        for (int z = 0; z < n && nout < max; z++)
            for (int x = 0; x < n && nout < max; x++) {
                int i = (y * SN + z) * SN + x;
                if (!mat_[i] || fix_[i]) continue;
                int sp = span_of(mat_[i]);
                if (cost_a[i] > sp && cost_b[i] <= sp) out[nout++] = (SupportCell){ ox + x, oy + y, oz + z };
            }
    return nout;
}
