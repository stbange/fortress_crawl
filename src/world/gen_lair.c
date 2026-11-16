#include "world/dragons.h"
#include "world/gen_common.h"

#include "core/noise.h"

/* A dragon's lair (dragons.h, docs/DRAGONS.md): a cave of three by three chunks under a dome as
 * high as the layer allows, room for a great dragon to fly. In the middle its hoard, a mound of
 * gold; round it four rock perches; bones of its prey on the floor; and the walls marked by its
 * breath: scorched, frozen, crystal-grown, rotten, rusted. The passages of the layer open into
 * it like any hall's; its stairs stand in two corners. */

static const Lair *L;
static float hh(int x, int z, u32 k) { return hash3f(L->key, x, 0, z, k); }

void gen_lair(GenCtx *g) {
    Lair lair;
    if (!lair_from_structure(&g->info.st, &lair)) return;
    L = &lair;
    const DragonDef *D = &DRAGON_KINDS[lair.kind];
    const Structure *s = &g->info.st;
    int Y1 = s->cy0 * CHUNK + CHUNK - 1, floor = lair.floor;
    float cx = lair.cx + 0.5f, cz = lair.cz + 0.5f;
    float rx = (lair.x1 - lair.x0) * 0.5f, rz = (lair.z1 - lair.z0) * 0.5f;
    /* the cave: a dome, rough at the edges */
    for (int ly = 0; ly < CHUNK; ly++)
        for (int lz = 0; lz < CHUNK; lz++)
            for (int lx = 0; lx < CHUNK; lx++) {
                int wx = g->ox + lx, wy = g->oy + ly, wz = g->oz + lz;
                if (wx < lair.x0 || wx > lair.x1 || wz < lair.z0 || wz > lair.z1) continue;
                float dx = (wx + 0.5f - cx) / rx, dz = (wz + 0.5f - cz) / rz;
                float d = sqrtf(dx * dx + dz * dz) + fbm2(lair.key, wx * 0.06f, wz * 0.06f, 2) * 0.12f;
                if (d > 1.0f) continue;
                int top = floor + (int)((Y1 - 1 - floor) * sqrtf(fmaxf(0.0f, 1.0f - d * d * 0.85f)));
                int bump = (int)(fbm2(lair.key + 7, wx * 0.1f, wz * 0.1f, 2) * 2.0f);
                if (wy >= floor + MAXI(0, bump) && wy <= top) g_put(g, lx, ly, lz, B_AIR);
                else if (wy == floor - 1 + MAXI(0, bump)) g_put(g, lx, ly, lz, D->traits & DT_SWIM ? B_ROCK : B_DIRT);
            }
    /* the hoard: a mound of gold with the dragon's bed on top */
    for (int dz = -8; dz <= 8; dz++)
        for (int dx = -8; dx <= 8; dx++) {
            float d = sqrtf((float)(dx * dx + dz * dz));
            if (d > 8.0f) continue;
            int h = (int)((8.0f - d) / 3.0f);
            for (int k = 0; k < h; k++) g_set_w(g, lair.cx + dx, floor + k, lair.cz + dz, B_ACCENT);
            if (hh(dx + lair.cx, dz + lair.cz, 0x40) < 0.06f) g_set_w(g, lair.cx + dx, floor + h, lair.cz + dz, B_BONES);
        }
    /* perches: pillars of rock with flat tops */
    for (int k = 0; k < 4; k++) {
        int px = lair.perch_x[k], pz = lair.perch_z[k], ph = lair.perch_h[k];
        for (int dz = -2; dz <= 2; dz++)
            for (int dx = -2; dx <= 2; dx++)
                if (dx * dx + dz * dz <= 5) g_fill_w(g, px + dx, floor, pz + dz, px + dx, floor + ph, pz + dz, B_ROCK);
    }
    /* bones of its prey, and the marks of its breath */
    for (int lz = 0; lz < CHUNK; lz++)
        for (int lx = 0; lx < CHUNK; lx++) {
            int wx = g->ox + lx, wz = g->oz + lz;
            if (wx < lair.x0 || wx > lair.x1 || wz < lair.z0 || wz > lair.z1) continue;
            float r = hh(wx, wz, 0x41);
            int y = floor;
            if (g_get_w(g, wx, y, wz) != B_AIR || !block_solid(g_get_w(g, wx, y - 1, wz))) continue;
            if (r < 0.025f) g_set_w(g, wx, y, wz, B_BONES);
            float m = hh(wx, wz, 0x42);
            switch (D->breath) {
            case BR_FIRE: case BR_MAGMA:
                if (m < 0.18f) g_set_w(g, wx, y - 1, wz, B_OBSIDIAN);
                else if (m < 0.2f) g_set_w(g, wx, y - 1, wz, B_LAVA);
                else if (m < 0.23f) g_set_w(g, wx, y, wz, B_ASH);
                break;
            case BR_FROST: if (m < 0.3f) g_set_w(g, wx, y - 1, wz, B_ICE); break;
            case BR_LIGHT: case BR_FORCE: case BR_VOID:
                if (m < 0.03f) g_fill_w(g, wx, y, wz, wx, y + 1 + (int)(r * 40.0f) % 4, wz, B_CRYSTAL);
                break;
            case BR_GAS: case BR_ROT:
                if (m < 0.08f) g_set_w(g, wx, y, wz, B_FUNGUS);
                else if (m < 0.12f) g_set_w(g, wx, y - 1, wz, B_WATER);
                break;
            case BR_RUST: case BR_STEAM: if (m < 0.1f) g_set_w(g, wx, y - 1, wz, B_ORE); break;
            case BR_WAVE: if (m < 0.35f && (wx - lair.cx) * (wx - lair.cx) + (wz - lair.cz) * (wz - lair.cz) > 150) g_set_w(g, wx, y - 1, wz, B_WATER); break;
            case BR_NECRO: if (m < 0.06f) g_set_w(g, wx, y, wz, B_BONES); break;
            case BR_HOLY: if (m < 0.01f) g_fill_w(g, wx, y, wz, wx, y + 3, wz, B_ACCENT); break;
            default: if (m < 0.1f) g_set_w(g, wx, y - 1, wz, B_OBSIDIAN); break;
            }
        }
    /* a few lights, unless it lives in the dark */
    if (!(D->traits & DT_DARK))
        for (int k = 0; k < 4; k++) {
            int x = lair.perch_x[k], z = lair.perch_z[k], y = floor + lair.perch_h[k] + 1;
            g_set_w(g, x, y, z, D->breath == BR_FIRE || D->breath == BR_MAGMA ? B_FIRE : B_CRYSTAL);
        }
    carve_shafts(g);
    carve_portal_mouths(g, 4, false);
    skin_pass(g, false);
}
