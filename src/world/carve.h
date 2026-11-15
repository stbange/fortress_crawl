#ifndef FC_CARVE_H
#define FC_CARVE_H

/* The pieces of broken blocks (docs/DESTRUCTION.md). A block whose BLOCK_CARVED bit is set
 * keeps a 64-bit mask here: bit (sx + sz * 4 + sy * 16) set = the 25 cm piece at (sx, sy, sz)
 * is still there. A chunk keeps its carved blocks in a small sorted set. */

#include "core/common.h"

#define CARVE_FULL 0xFFFFFFFFFFFFFFFFULL

typedef struct CarveSet {
    int n, cap;
    u16 *idx;  /* chunk cell index (cidx) */
    u64 *mask;
} CarveSet;

static inline int carve_bit(int sx, int sy, int sz) { return sx + sz * 4 + sy * 16; }

/* The mask of a cell, or CARVE_FULL if the cell has none. */
static inline u64 carve_get(const CarveSet *c, int idx) {
    if (!c) return CARVE_FULL;
    int lo = 0, hi = c->n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (c->idx[mid] == idx) return c->mask[mid];
        if (c->idx[mid] < idx) lo = mid + 1;
        else hi = mid - 1;
    }
    return CARVE_FULL;
}

static inline void carve_put(CarveSet **cs, int idx, u64 mask) {
    if (!*cs) *cs = calloc(1, sizeof(CarveSet));
    CarveSet *c = *cs;
    int lo = 0, hi = c->n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (c->idx[mid] == idx) { c->mask[mid] = mask; return; }
        if (c->idx[mid] < idx) lo = mid + 1;
        else hi = mid - 1;
    }
    if (c->n == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 16;
        c->idx = realloc(c->idx, sizeof(u16) * (size_t)c->cap);
        c->mask = realloc(c->mask, sizeof(u64) * (size_t)c->cap);
    }
    memmove(&c->idx[lo + 1], &c->idx[lo], sizeof(u16) * (size_t)(c->n - lo));
    memmove(&c->mask[lo + 1], &c->mask[lo], sizeof(u64) * (size_t)(c->n - lo));
    c->idx[lo] = (u16)idx;
    c->mask[lo] = mask;
    c->n++;
}

static inline void carve_del(CarveSet *c, int idx) {
    if (!c) return;
    for (int i = 0; i < c->n; i++)
        if (c->idx[i] == idx) {
            memmove(&c->idx[i], &c->idx[i + 1], sizeof(u16) * (size_t)(c->n - i - 1));
            memmove(&c->mask[i], &c->mask[i + 1], sizeof(u64) * (size_t)(c->n - i - 1));
            c->n--;
            return;
        }
}

static inline void carve_free(CarveSet *c) {
    if (!c) return;
    free(c->idx);
    free(c->mask);
    free(c);
}

#endif
