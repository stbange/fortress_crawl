#ifndef FC_MECH_H
#define FC_MECH_H

/* Rules of the mechanical puzzles (M8), shared by the generator (which checks that
 * every puzzle it places can be solved) and the game (which runs them).
 *
 *   levers   - lamps over the gate; pulling lever i flips lamps i and i+1. All lit: open.
 *   plates   - stones marked I, II, III...; step on them in that order.
 *   statues  - the watchers must look at the gate; striking one turns it a quarter.
 *   beam     - a crystal casts light at a mirror; mirrors turn an eighth when struck
 *              and send the light where they face. Light on the gate's lock: open.
 * Directions: 0..3 as DIR_X/DIR_Z (east, south, west, north); 0..7 for the light,
 * where 2*d is the same direction as d in 0..3. */

#include "world/layout.h"

static const int DIR8_X[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const int DIR8_Z[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

/* The gate cell (local, at feet height and one above). */
static inline void mech_gate_cell(const Vault *v, int *x, int *z) {
    *x = v->x + 2 * DIR_X[v->door];
    *z = v->z + 2 * DIR_Z[v->door];
}

/* Nearest of the four directions to (dx, dz). */
static inline int mech_dir4(int dx, int dz) {
    if ((dx < 0 ? -dx : dx) >= (dz < 0 ? -dz : dz)) return dx >= 0 ? 0 : 2;
    return dz >= 0 ? 1 : 3;
}

/* The rotation a statue must have: looking at the gate. */
static inline int mech_statue_goal(const Vault *v, int i) {
    int gx, gz;
    mech_gate_cell(v, &gx, &gz);
    return mech_dir4(gx - v->parts[i].x, gz - v->parts[i].z);
}

static inline u8 mech_lever_pull(u8 lamps, int i, int n) {
    return (u8)(lamps ^ (1u << i) ^ (1u << ((i + 1) % n)));
}

/* Is the cell (in the same coordinates as the vault, offset by ox/oy/oz) solid for light? */
typedef int (*MechSolidFn)(void *user, int x, int y, int z);

#define MECH_BEAM_PTS (VAULT_PARTS + 2)

/* Follows the light from the emitter (parts[0]) through the mirrors with the given
 * facings, one cell per step at the row above the feet. pts gets the turning points
 * (x, z pairs, emitter first, the last cell reached last). True if it reaches the gate. */
static inline bool mech_beam(const Vault *v, const u8 *facing, int ox, int oy, int oz, MechSolidFn solid, void *user,
                             int *pts, int *npts) {
    int gx, gz;
    mech_gate_cell(v, &gx, &gz);
    int x = v->parts[0].x, z = v->parts[0].z, d = v->parts[0].dir, row = v->y + 1;
    u8 used = 0;
    int n = 0;
    pts[n * 2] = x; pts[n * 2 + 1] = z; n++;
    bool reached = false;
    for (int step = 0; step < 96; step++) {
        int nx = x + DIR8_X[d], nz = z + DIR8_Z[d];
        if (DIR8_X[d] && DIR8_Z[d] && solid(user, ox + nx, oy + row, oz + z) && solid(user, ox + x, oy + row, oz + nz)) break;
        x = nx; z = nz;
        if (x == gx && z == gz) { reached = true; break; }
        int hit = -1;
        for (int i = 1; i < v->nparts; i++)
            if (v->parts[i].x == x && v->parts[i].z == z) hit = i;
        if (hit > 0) {
            if (used & (1u << hit) || n >= MECH_BEAM_PTS - 1) break;
            used |= (u8)(1u << hit);
            pts[n * 2] = x; pts[n * 2 + 1] = z; n++;
            d = facing[hit];
            continue;
        }
        if (x == v->parts[0].x && z == v->parts[0].z) break;
        if (solid(user, ox + x, oy + row, oz + z)) break;
    }
    pts[n * 2] = x; pts[n * 2 + 1] = z; n++;
    *npts = n;
    return reached;
}

#endif
