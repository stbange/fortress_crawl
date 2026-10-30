#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* The map only shows what the player has actually seen: every 0.1 s a fan of rays
 * from the eye marks the open cells it passes and the wall it stops at. */

static u64 ekey(int cx, int cy, int cz) {
    return mix64(((u64)(u32)cx << 42) ^ ((u64)(u32)cy << 21) ^ (u64)(u32)cz);
}

static ExploredChunk *echunk(Game *g, int cx, int cy, int cz, bool create) {
    Explored *e = &g->explored;
    if (!e->cap) {
        if (!create) return NULL;
        e->cap = 1024;
        e->table = calloc((size_t)e->cap, sizeof(ExploredChunk *));
    }
    for (int i = (int)(ekey(cx, cy, cz) & (u64)(e->cap - 1));; i = (i + 1) & (e->cap - 1)) {
        ExploredChunk *c = e->table[i];
        if (!c) break;
        if (c->cx == cx && c->cy == cy && c->cz == cz) return c;
    }
    if (!create) return NULL;
    if ((e->count + 1) * 2 > e->cap) {
        int old_cap = e->cap;
        ExploredChunk **old = e->table;
        e->cap *= 2;
        e->table = calloc((size_t)e->cap, sizeof(ExploredChunk *));
        for (int i = 0; i < old_cap; i++) {
            if (!old[i]) continue;
            int j = (int)(ekey(old[i]->cx, old[i]->cy, old[i]->cz) & (u64)(e->cap - 1));
            while (e->table[j]) j = (j + 1) & (e->cap - 1);
            e->table[j] = old[i];
        }
        free(old);
    }
    ExploredChunk *c = calloc(1, sizeof *c);
    c->cx = cx; c->cy = cy; c->cz = cz;
    int j = (int)(ekey(cx, cy, cz) & (u64)(e->cap - 1));
    while (e->table[j]) j = (j + 1) & (e->cap - 1);
    e->table[j] = c;
    e->count++;
    return c;
}

static void mark(Game *g, int x, int y, int z) {
    ExploredChunk *c = echunk(g, floordiv(x, CHUNK), floordiv(y, CHUNK), floordiv(z, CHUNK), true);
    int i = cidx(floormod(x, CHUNK), floormod(y, CHUNK), floormod(z, CHUNK));
    c->bits[i >> 3] |= (u8)(1 << (i & 7));
}

bool explore_get(Game *g, int x, int y, int z) {
    ExploredChunk *c = echunk(g, floordiv(x, CHUNK), floordiv(y, CHUNK), floordiv(z, CHUNK), false);
    if (!c) return false;
    int i = cidx(floormod(x, CHUNK), floormod(y, CHUNK), floormod(z, CHUNK));
    return (c->bits[i >> 3] >> (i & 7)) & 1;
}

/* Each player looks around for themselves; the map they fill is shared. */
void explore_update(Game *g, PlayerSlot *ps, float dt) {
    ps->explore_timer -= dt;
    if (ps->explore_timer > 0) return;
    ps->explore_timer = 0.1f;
    vec3 eye = player_eye(&ps->pl);
    static const float PITCH[3] = { -0.45f, 0.0f, 0.35f };
    for (int pi = 0; pi < 3; pi++)
        for (int i = 0; i < 240; i++) {
            float a = i * 2.0f * PI_F / 240.0f;
            vec3 d = dir_from_angles(a, PITCH[pi]);
            int lx = 1 << 30, ly = 0, lz = 0;
            for (float t = 0; t < 26.0f; t += 0.5f) {
                vec3 p = v3_add(eye, v3_scale(d, t));
                int x = (int)floorf(p.x), y = (int)floorf(p.y), z = (int)floorf(p.z);
                if (x == lx && y == ly && z == lz) continue;
                lx = x; ly = y; lz = z;
                bool known;
                Block b = chunkmap_block(&g->map, x, y, z, &known);
                if (!known) break;
                mark(g, x, y, z);
                if (block_opaque(b)) break;
                /* the floor under an open cell is part of what you see */
                Block below = chunkmap_block(&g->map, x, y - 1, z, &known);
                if (known && block_solid(below)) mark(g, x, y - 1, z);
            }
        }
}

static void map_arrow(float cx, float cy, float yaw, vec3 c) {
    for (int k = 0; k < 5; k++) {
        float d = (float)k * 1.6f;
        batch2d_rect(cx + cosf(yaw) * d - 1, cy + sinf(yaw) * d - 1, 3, 3, c.x, c.y, c.z, 1.0f);
    }
}

/* Top-down map of explored cells around the player, sized to their view; north (-Z) is up. */
void explore_draw_map(Game *g, PlayerSlot *ps, int w, int h) {
    int px = 2;
    int cells = MINI(110, (MINI(w, h) - 30) / px);
    int size = cells * px;
    float ox = (w - size) * 0.5f, oy = (h - size) * 0.5f + 4;
    batch2d_begin(w, h);
    batch2d_rect(0, 0, (float)w, (float)h, 0, 0, 0, 0.55f);
    batch2d_rect(ox - 3, oy - 3, size + 6.0f, size + 6.0f, 0.35f, 0.28f, 0.18f, 1.0f);
    batch2d_rect(ox - 2, oy - 2, size + 4.0f, size + 4.0f, 0.06f, 0.05f, 0.05f, 1.0f);
    int pxw = (int)floorf(ps->pl.pos.x), pyw = (int)floorf(ps->pl.pos.y + 0.01f), pzw = (int)floorf(ps->pl.pos.z);
    for (int j = 0; j < cells; j++)
        for (int i = 0; i < cells; i++) {
            int wx = pxw + i - cells / 2, wz = pzw + j - cells / 2;
            /* highest seen walkable cell within a few metres of the player's height */
            for (int y = pyw + 3; y >= pyw - 6; y--) {
                if (!explore_get(g, wx, y, wz)) continue;
                bool known;
                Block b = chunkmap_block(&g->map, wx, y, wz, &known);
                float fx = ox + i * px, fy = oy + j * px;
                if (known && block_opaque(b)) {
                    bool known_above;
                    bool floor = !block_opaque(chunkmap_block(&g->map, wx, y + 1, wz, &known_above));
                    if (!floor || !known_above) {
                        batch2d_rect(fx, fy, (float)px, (float)px, 0.32f, 0.26f, 0.2f, 1.0f);
                        break;
                    }
                    y++; /* a seen floor: shade by the height you would stand at */
                }
                float depth = clampf(1.0f - (pyw - y) * 0.12f, 0.35f, 1.0f);
                if (y > pyw) depth = 1.0f - (y - pyw) * 0.12f;
                if (b == B_WATER) batch2d_rect(fx, fy, (float)px, (float)px, 0.15f * depth, 0.3f * depth, 0.55f * depth, 1.0f);
                else if (b == B_SHRINE || BLOCKS[b].light) batch2d_rect(fx, fy, (float)px, (float)px, 1.0f, 0.75f, 0.3f, 1.0f);
                else batch2d_rect(fx, fy, (float)px, (float)px, 0.72f * depth, 0.66f * depth, 0.52f * depth, 1.0f);
                break;
            }
        }
    /* the other players in their colours, then you */
    float cx = ox + (cells / 2) * px + 1, cy = oy + (cells / 2) * px + 1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *o = &g->players[i];
        if (!o->active || o == ps) continue;
        float mx = cx + (floorf(o->pl.pos.x) - pxw) * px, my = cy + (floorf(o->pl.pos.z) - pzw) * px;
        mx = clampf(mx, ox, ox + size - 3), my = clampf(my, oy, oy + size - 3);
        map_arrow(mx, my, o->pl.yaw, o->color);
    }
    map_arrow(cx, cy, ps->pl.yaw, g->nplayers > 1 ? ps->color : v3(1.0f, 0.25f, 0.15f));
    batch2d_flush();
    font_begin(w, h);
    font_text((int)ox + size / 2 - 3, (int)oy - 12, "N", 0.9f, 0.8f, 0.6f);
    char buf[160];
    snprintf(buf, sizeof buf, "MAP - %s", ps->place);
    if (size < 200) buf[MINI((int)sizeof buf - 1, size / 6)] = 0; /* 6 px per glyph */
    font_text((int)ox, (int)(oy + size + 5), buf, 0.85f, 0.75f, 0.55f);
    if (size >= 160) font_text((int)(ox + size - 60), (int)(oy + size + 5), ps->pad && !ps->keyboard ? "B: CLOSE" : "M: CLOSE", 0.6f, 0.55f, 0.45f);
    font_end();
}
