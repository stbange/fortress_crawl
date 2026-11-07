#include "game/game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* Saving: one save per world (seed). It holds every character that played in that world
 * (stats, skills, inventory, spells, relics, position and shrine), the chests and relics
 * already taken, and as much of the explored map as fits in a budget.
 * In the browser it lives in localStorage (base64), on the desktop in a file in the
 * user's preference folder. The world itself never needs saving: the seed rebuilds it. */

#define SAVE_MAGIC 0x56534346u /* "FCSV" */
#define SAVE_VERSION 6 /* 2: story (motive, chapter, missions, lore); 3: layered world; 4: more spells, the pocket realm; 5: Sanctum rooms;
                          6: weapons with materials, powers, legendaries and memory */
#define EXPLORED_BUDGET (1200 * 1024)

/* ---------- storage backends ---------- */

#ifdef __EMSCRIPTEN__
EM_JS_DEPS(fc_save, "$UTF8ToString");
EM_JS(int, js_store_write, (const char *key, const unsigned char *data, int n), {
    try {
        var bytes = HEAPU8.subarray(data, data + n);
        var s = "";
        for (var i = 0; i < n; i += 8192) s += String.fromCharCode.apply(null, bytes.subarray(i, Math.min(n, i + 8192)));
        localStorage.setItem(UTF8ToString(key), btoa(s));
        return 1;
    } catch (e) {
        console.warn('save failed: ' + e);
        return 0;
    }
});
EM_JS(int, js_store_size, (const char *key), {
    try {
        var v = localStorage.getItem(UTF8ToString(key));
        if (v === null) return -1;
        return atob(v).length;
    } catch (e) { return -1; }
});
EM_JS(void, js_store_read, (const char *key, unsigned char *out, int n), {
    try {
        var s = atob(localStorage.getItem(UTF8ToString(key)));
        for (var i = 0; i < n && i < s.length; i++) HEAPU8[out + i] = s.charCodeAt(i);
    } catch (e) {}
});
EM_JS(void, js_store_remove, (const char *key), {
    try { localStorage.removeItem(UTF8ToString(key)); } catch (e) {}
});

static bool store_write(const char *key, const u8 *data, int n) { return js_store_write(key, data, n) != 0; }
static u8 *store_read(const char *key, int *n) {
    int size = js_store_size(key);
    if (size < 0) return NULL;
    u8 *buf = malloc((size_t)size + 1);
    js_store_read(key, buf, size);
    *n = size;
    return buf;
}
static void store_remove(const char *key) { js_store_remove(key); }
#else
static void store_path(const char *key, char *out, size_t n) {
    static char *base;
    if (!base) base = SDL_GetPrefPath("fortress-crawl", "fortress-crawl");
    char clean[96];
    size_t k = 0;
    for (const char *p = key; *p && k + 1 < sizeof clean; p++) clean[k++] = (*p == ':' || *p == '/') ? '_' : *p;
    clean[k] = 0;
    snprintf(out, n, "%s%s.sav", base ? base : "", clean);
}
static bool store_write(const char *key, const u8 *data, int n) {
    char path[512], tmp[520];
    store_path(key, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, (size_t)n, f) == (size_t)n;
    ok &= fclose(f) == 0;
    /* write then rename, so a crash never leaves half a save */
    remove(path);
    return ok && rename(tmp, path) == 0;
}
static u8 *store_read(const char *key, int *n) {
    char path[512];
    store_path(key, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }
    u8 *buf = malloc((size_t)size + 1);
    *n = (int)fread(buf, 1, (size_t)size, f);
    fclose(f);
    return buf;
}
static void store_remove(const char *key) {
    char path[512];
    store_path(key, path, sizeof path);
    remove(path);
}
#endif

static void save_key(u64 seed, char *out, size_t n) { snprintf(out, n, "fortress_crawl:save:%016llx", (unsigned long long)seed); }

/* ---------- writing ---------- */

typedef struct Buf { u8 *d; int n, cap; } Buf;

static void put(Buf *b, const void *p, int n) {
    if (b->n + n > b->cap) {
        b->cap = MAXI(b->cap * 2, b->n + n + 4096);
        b->d = realloc(b->d, (size_t)b->cap);
    }
    memcpy(b->d + b->n, p, (size_t)n);
    b->n += n;
}
static void w8(Buf *b, u8 v) { put(b, &v, 1); }
static void w16(Buf *b, u16 v) { u8 t[2] = { (u8)v, (u8)(v >> 8) }; put(b, t, 2); }
static void w32(Buf *b, u32 v) { u8 t[4] = { (u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24) }; put(b, t, 4); }
static void w64(Buf *b, u64 v) { w32(b, (u32)v); w32(b, (u32)(v >> 32)); }
static void wf(Buf *b, float f) { u32 v; memcpy(&v, &f, 4); w32(b, v); }
static void wv(Buf *b, vec3 v) { wf(b, v.x); wf(b, v.y); wf(b, v.z); }
static void wbytes(Buf *b, const u8 *p, int n) { w16(b, (u16)n); put(b, p, n); }
static void witem(Buf *b, const Item *it) {
    w8(b, it->used); w8(b, it->base); w8(b, it->rarity); w8(b, it->level); w8(b, it->affix[0]); w8(b, it->affix[1]);
    w8(b, it->material); w8(b, it->unique); w8(b, it->power[0]); w8(b, it->power[1]); w8(b, it->hidden); w8(b, it->bane);
    w16(b, it->kills); w16(b, it->bane_kills);
}

static void write_player(Buf *b, const Stats *s, const Player *pl) {
    wv(b, pl->pos); wf(b, pl->yaw); wf(b, pl->pitch); w8(b, pl->fly);
    w32(b, (u32)s->level); w32(b, (u32)s->points); w32(b, (u32)s->kills);
    wf(b, s->xp); wf(b, s->xp_next); wf(b, s->hp); wf(b, s->mana);
    wbytes(b, s->ranks, SKILL_COUNT);
    w32(b, (u32)s->weapon);
    wv(b, s->respawn); wf(b, s->respawn_yaw);
    w16(b, WPN_COUNT);
    for (int i = 0; i < WPN_COUNT; i++) witem(b, &s->equipped[i]);
    w16(b, BAG_SIZE);
    for (int i = 0; i < BAG_SIZE; i++) witem(b, &s->bag[i]);
    w32(b, (u32)s->gold); w32(b, (u32)s->potions_hp); w32(b, (u32)s->potions_mp);
    wbytes(b, s->relics, RELIC_COUNT);
    w16(b, SPELL_COUNT);
    wbytes(b, s->spell_rank, SPELL_COUNT);
    w32(b, (u32)s->spell);
    /* v2: story */
    w8(b, s->motive);
    w32(b, (u32)s->chapter);
    w8(b, (u8)s->nmissions);
    for (int i = 0; i < s->nmissions; i++) {
        const Mission *m = &s->missions[i];
        w8(b, m->type);
        w64(b, m->key);
        w32(b, (u32)m->tx); w32(b, (u32)m->ty); w32(b, (u32)m->tz);
        w32(b, (u32)m->sx); w32(b, (u32)m->sy); w32(b, (u32)m->sz);
        w32(b, (u32)m->reward);
    }
    /* v4: where the Sanctum Door brought you from */
    w8(b, s->realm_return_set);
    wv(b, s->realm_return); wf(b, s->realm_return_yaw);
    w8(b, s->sanctum); wf(b, s->library_t);
}

/* Explored cells: per chunk a 512-byte mask of the non-zero bytes, then those bytes. */
static void write_explored(Buf *b, Game *g) {
    Explored *e = &g->explored;
    int count_at = b->n, count = 0;
    w32(b, 0);
    vec3 eye = player_eye(&g->players[0].pl);
    int pcx = floordiv((int)floorf(eye.x), CHUNK), pcy = floordiv((int)floorf(eye.y), CHUNK), pcz = floordiv((int)floorf(eye.z), CHUNK);
    int start = b->n;
    /* nearest chunks first, so a long journey keeps the map around where you are */
    for (int ring = 0; ring < 2; ring++)
        for (int i = 0; i < e->cap; i++) {
            ExploredChunk *c = e->table ? e->table[i] : NULL;
            if (!c) continue;
            bool near = abs(c->cx - pcx) <= 6 && abs(c->cy - pcy) <= 3 && abs(c->cz - pcz) <= 6;
            if (near != (ring == 0)) continue;
            u8 mask[CHUNK_VOL / 64] = { 0 };
            int nz = 0;
            for (int k = 0; k < CHUNK_VOL / 8; k++)
                if (c->bits[k]) { mask[k >> 3] |= (u8)(1 << (k & 7)); nz++; }
            if (!nz) continue;
            if (b->n - start + 12 + (int)sizeof mask + nz > EXPLORED_BUDGET) break;
            w32(b, (u32)c->cx); w32(b, (u32)c->cy); w32(b, (u32)c->cz);
            put(b, mask, sizeof mask);
            for (int k = 0; k < CHUNK_VOL / 8; k++)
                if (c->bits[k]) w8(b, c->bits[k]);
            count++;
        }
    u8 t[4] = { (u8)count, (u8)(count >> 8), (u8)(count >> 16), (u8)(count >> 24) };
    memcpy(b->d + count_at, t, 4);
}

bool game_save(Game *g) {
    if (!g->world_started) return false;
    Buf b = { 0 };
    PlayerSlot *p1 = &g->players[0];
    w32(&b, SAVE_MAGIC);
    w32(&b, SAVE_VERSION);
    w64(&b, g->opt.seed);
    put(&b, g->opt.seed_text, sizeof g->opt.seed_text);
    w32(&b, (u32)p1->st.level); w32(&b, (u32)p1->st.gold); w32(&b, (u32)p1->st.kills);
    wf(&b, g->play_time);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (ps->active) {
            w8(&b, 1);
            /* a fallen character is saved standing at its shrine */
            Player pl = ps->pl;
            if (ps->st.dead) { pl.pos = ps->st.respawn; pl.yaw = ps->st.respawn_yaw; }
            write_player(&b, &ps->st, &pl);
        } else if (g->saved[i].valid) {
            w8(&b, 1); /* keep characters that did not join this time */
            write_player(&b, &g->saved[i].st, &g->saved[i].pl);
        } else {
            w8(&b, 0);
        }
    }
    w32(&b, (u32)g->looted_count);
    for (int i = 0; i < g->looted_cap; i++)
        if (g->looted[i]) w64(&b, g->looted[i]);
    w32(&b, (u32)g->nlore);
    for (int i = 0; i < g->nlore; i++) {
        w8(&b, g->lore[i].kind);
        w32(&b, (u32)g->lore[i].a); w32(&b, (u32)g->lore[i].b); w32(&b, (u32)g->lore[i].c);
    }
    write_explored(&b, g);
    char key[64];
    save_key(g->opt.seed, key, sizeof key);
    bool ok = store_write(key, b.d, b.n);
    if (ok) store_write("fortress_crawl:last", (const u8 *)g->opt.seed_text, (int)strlen(g->opt.seed_text));
    free(b.d);
    return ok;
}

/* ---------- reading ---------- */

typedef struct Rd { const u8 *d; int n, at; bool bad; int ver; } Rd;

static bool take(Rd *r, void *out, int n) {
    if (r->bad || r->at + n > r->n) { r->bad = true; memset(out, 0, (size_t)n); return false; }
    memcpy(out, r->d + r->at, (size_t)n);
    r->at += n;
    return true;
}
static u8 r8(Rd *r) { u8 v; take(r, &v, 1); return v; }
static u16 r16(Rd *r) { u8 t[2]; take(r, t, 2); return (u16)(t[0] | t[1] << 8); }
static u32 r32(Rd *r) { u8 t[4]; take(r, t, 4); return (u32)t[0] | (u32)t[1] << 8 | (u32)t[2] << 16 | (u32)t[3] << 24; }
static u64 r64(Rd *r) { u64 lo = r32(r); return lo | (u64)r32(r) << 32; }
static float rf(Rd *r) { u32 v = r32(r); float f; memcpy(&f, &v, 4); return f; }
static vec3 rv(Rd *r) { vec3 v; v.x = rf(r); v.y = rf(r); v.z = rf(r); return v; }
/* A counted byte array: copies what fits, skips the rest (saves from other versions). */
static void rbytes(Rd *r, u8 *out, int cap) {
    int n = r16(r);
    for (int i = 0; i < n; i++) {
        u8 v = r8(r);
        if (i < cap) out[i] = v;
    }
}
static Item ritem(Rd *r) {
    Item it;
    memset(&it, 0, sizeof it);
    it.used = r8(r); it.base = r8(r); it.rarity = r8(r); it.level = r8(r); it.affix[0] = r8(r); it.affix[1] = r8(r);
    if (r->ver >= 6) {
        it.material = r8(r); it.unique = r8(r); it.power[0] = r8(r); it.power[1] = r8(r); it.hidden = r8(r); it.bane = r8(r);
        it.kills = r16(r); it.bane_kills = r16(r);
    }
    if (it.base >= WB_COUNT || it.rarity >= RAR_COUNT || it.affix[0] >= AF_COUNT || it.affix[1] >= AF_COUNT || it.material >= MAT_COUNT ||
        it.hidden >= AF_COUNT || it.unique > UNIQUE_COUNT || it.bane > MON_TYPE_COUNT)
        memset(&it, 0, sizeof it);
    return it;
}

static void read_player(Rd *r, SavedPlayer *sp) {
    Stats *s = &sp->st;
    Player *pl = &sp->pl;
    memset(sp, 0, sizeof *sp);
    pl->pos = rv(r); pl->yaw = rf(r); pl->pitch = rf(r); pl->fly = r8(r) != 0;
    s->level = (int)r32(r); s->points = (int)r32(r); s->kills = (int)r32(r);
    s->xp = rf(r); s->xp_next = rf(r); s->hp = rf(r); s->mana = rf(r);
    rbytes(r, s->ranks, SKILL_COUNT);
    s->weapon = (int)r32(r);
    s->respawn = rv(r); s->respawn_yaw = rf(r);
    int ne = r16(r);
    for (int i = 0; i < ne; i++) {
        Item it = ritem(r);
        if (i < WPN_COUNT) s->equipped[i] = it;
    }
    int nb = r16(r);
    for (int i = 0; i < nb; i++) {
        Item it = ritem(r);
        if (i < BAG_SIZE) s->bag[i] = it;
    }
    s->gold = (int)r32(r); s->potions_hp = (int)r32(r); s->potions_mp = (int)r32(r);
    rbytes(r, s->relics, RELIC_COUNT);
    {
        /* the number of spells grew in version 4: read what there is, skip what we do not know */
        int ns = r->ver >= 4 ? r16(r) : SPELL_COUNT_V3;
        u8 tmp[1024];
        if (ns > (int)sizeof tmp) { r->bad = true; ns = 0; }
        rbytes(r, tmp, ns);
        memcpy(s->spell_rank, tmp, (size_t)MINI(ns, SPELL_COUNT));
    }
    s->spell = (int)r32(r);
    /* before version 3 the world had another shape: the place is meaningless now */
    sp->moved = r->ver < 3;
    if (r->ver >= 2) {
        s->motive = r8(r);
        s->chapter = (int)r32(r);
        int nm = r8(r);
        for (int i = 0; i < nm; i++) {
            Mission m = { 0 };
            m.type = r8(r);
            m.key = r64(r);
            m.tx = (int)r32(r); m.ty = (int)r32(r); m.tz = (int)r32(r);
            m.sx = (int)r32(r); m.sy = (int)r32(r); m.sz = (int)r32(r);
            m.reward = (int)r32(r);
            if (s->nmissions < MAX_MISSIONS && (m.type == MI_BOUNTY || m.type == MI_RETRIEVE)) s->missions[s->nmissions++] = m;
        }
        if (s->motive > MOTIVE_COUNT) s->motive = 0;
        if (s->chapter < 0 || s->chapter > 10000) s->chapter = 0;
    }
    if (r->ver >= 4) {
        s->realm_return_set = r8(r) != 0;
        s->realm_return = rv(r);
        s->realm_return_yaw = rf(r);
    }
    if (r->ver >= 5) {
        s->sanctum = r8(r);
        s->library_t = rf(r);
    } else if (r->ver == 4) {
        s->sanctum = SANCTUM_BATH | SANCTUM_FEAST; /* they were free when this character found them */
    }
    /* sanity: a save from a broken build must not crash the game */
    if (s->level < 1 || s->level > 999) s->level = 1;
    if (s->weapon < 0 || s->weapon >= WPN_COUNT) s->weapon = 0;
    if (s->spell < 0 || s->spell >= SPELL_COUNT || !s->spell_rank[s->spell]) s->spell = spell_attack_id(EL_FIRE, SF_BOLT);
    s->spell_rank[spell_attack_id(EL_FIRE, SF_BOLT)] = MAXI(1, s->spell_rank[spell_attack_id(EL_FIRE, SF_BOLT)]);
    s->spell_rank[SPELL_ATTACK_COUNT + SU_THREAD] = MAXI(1, s->spell_rank[SPELL_ATTACK_COUNT + SU_THREAD]); /* everyone has it */
    s->spell_rank[SPELL_ATTACK_COUNT + SU_SANCTUM] = MAXI(1, s->spell_rank[SPELL_ATTACK_COUNT + SU_SANCTUM]);
    for (int i = 0; i < WPN_COUNT; i++)
        if (!s->equipped[i].used || WEAPON_BASES[s->equipped[i].base].wclass != i) {
            static const int START[WPN_COUNT] = { WB_SWORD, WB_SHORTBOW, WB_STAFF };
            memset(&s->equipped[i], 0, sizeof(Item));
            s->equipped[i].used = 1;
            s->equipped[i].base = (u8)START[i];
        }
    if (!isfinite(pl->pos.x) || !isfinite(pl->pos.y) || !isfinite(pl->pos.z)) sp->valid = false;
    else sp->valid = !r->bad;
}

static bool read_header(Rd *r, SaveInfo *info) {
    if (r32(r) != SAVE_MAGIC) return false;
    r->ver = (int)r32(r);
    if (r->ver < 1 || r->ver > SAVE_VERSION) return false; /* older saves load, newer ones do not */
    info->seed = r64(r);
    take(r, info->seed_text, sizeof info->seed_text);
    info->seed_text[sizeof info->seed_text - 1] = 0;
    info->level = (int)r32(r); info->gold = (int)r32(r); info->kills = (int)r32(r);
    info->play_time = rf(r);
    return !r->bad;
}

bool save_peek(u64 seed, SaveInfo *info) {
    char key[64];
    save_key(seed, key, sizeof key);
    int n = 0;
    u8 *d = store_read(key, &n);
    if (!d) return false;
    Rd r = { d, n, 0, false };
    bool ok = read_header(&r, info) && info->seed == seed;
    free(d);
    return ok;
}

bool game_load(Game *g) {
    char key[64];
    save_key(g->opt.seed, key, sizeof key);
    int n = 0;
    u8 *d = store_read(key, &n);
    if (!d) return false;
    Rd r = { d, n, 0, false };
    SaveInfo info;
    if (!read_header(&r, &info) || info.seed != g->opt.seed) { free(d); return false; }
    g->play_time = info.play_time;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (r8(&r)) read_player(&r, &g->saved[i]);
    u32 nl = r32(&r);
    for (u32 i = 0; i < nl && !r.bad; i++) looted_mark(g, r64(&r));
    if (r.ver >= 2) {
        u32 nlo = r32(&r);
        for (u32 i = 0; i < nlo && !r.bad; i++) {
            u8 kind = r8(&r);
            int a = (int)r32(&r), b = (int)r32(&r), c = (int)r32(&r);
            if (!r.bad && (kind == LORE_TRACE || kind == LORE_REVEAL)) lore_add(g, kind, a, b, c);
        }
    }
    u32 nc = r32(&r);
    for (u32 i = 0; i < nc && !r.bad; i++) {
        int cx = (int)r32(&r), cy = (int)r32(&r), cz = (int)r32(&r);
        u8 mask[CHUNK_VOL / 64], bits[CHUNK_VOL / 8] = { 0 };
        take(&r, mask, sizeof mask);
        for (int k = 0; k < CHUNK_VOL / 8; k++)
            if (mask[k >> 3] & (1 << (k & 7))) bits[k] = r8(&r);
        if (!r.bad && r.ver >= 3) explore_import(g, cx, cy, cz, bits); /* old maps show old layouts */
    }
    free(d);
    SDL_Log("Loaded the save of world %s: %d characters, %u chests and relics taken, %u map chunks", g->opt.seed_text,
            g->saved[0].valid + g->saved[1].valid + g->saved[2].valid + g->saved[3].valid, nl, nc);
    return g->saved[0].valid;
}

void save_delete(u64 seed) {
    char key[64];
    save_key(seed, key, sizeof key);
    store_remove(key);
}

bool save_last_seed(char *out, int n) {
    int len = 0;
    u8 *d = store_read("fortress_crawl:last", &len);
    if (!d) return false;
    len = MINI(len, n - 1);
    memcpy(out, d, (size_t)len);
    out[len] = 0;
    free(d);
    return len > 0;
}

/* A saved character takes the slot over: stats, inventory, spells, place. */
void save_apply_player(Game *g, PlayerSlot *ps) {
    SavedPlayer *sp = &g->saved[ps->index];
    if (!sp->valid) return;
    Stats *s = &ps->st;
    float hp = sp->st.hp, mana = sp->st.mana;
    *s = sp->st;
    stats_refresh(ps);
    s->hp = hp > 1.0f ? fminf(hp, s->maxhp) : s->maxhp;
    s->mana = fminf(mana, s->maxmana);
    s->since_hurt = 100.0f;
    s->invuln = 2.0f;
    ps->book_sel = s->spell;
    char buf[96];
    if (sp->moved) {
        /* the fortress was rebuilt in layers since this save: back to the gate */
        s->respawn = g->start_pos;
        s->respawn_yaw = g->start_yaw;
        snprintf(buf, sizeof buf, "THE FORTRESS HAS SHIFTED - LEVEL %d, BACK AT THE GATE", s->level);
    } else {
        ps->pl.pos = sp->pl.pos;
        ps->pl.yaw = sp->pl.yaw;
        ps->pl.pitch = sp->pl.pitch;
        ps->pl.fly = sp->pl.fly;
        snprintf(buf, sizeof buf, "WELCOME BACK - LEVEL %d", s->level);
    }
    game_banner(ps, buf);
}
