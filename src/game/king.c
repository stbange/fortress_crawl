#include "game/game.h"

#include <ctype.h>

#include "engine/font.h"
#include "render/batch.h"
#include "world/history.h"

/* The Old King (king.h, docs/OLD_KING.md): the main storyline. Ten steps lead to his throne:
 * steps 1-3 (finding out) in any order, then 4-7 (the means) in any order, then 8, 9 and the
 * throne. Each step is done by something in the world:
 *   the herald      the elder of a town tells you of him
 *   the witness     someone who saw him, waiting in the halls
 *   the true name   a stele with his name on it
 *   the vision      a night in the Sanctum's bed
 *   the dragon      a great dragon slain (and the wyrm, the guardian before his throne)
 *   the hoard       a thing taken from a dragon's hoard
 *   the allegiance  a town's friendship (reputation 3)
 *   the forge       a weapon or key made at the Sanctum's anvil from a dragon's heart
 *   the brew        dragon's blood and an orchid brought to a town's healer
 *   the realm       the ruler of a realm behind the Gate defeated
 *   the relic       a relic taken from its guards
 *   the lieutenant  his named servant killed
 *   the seal        one of his seals broken
 * At the throne he waits. Speak to him: with his true name, or crouched with the artefact his
 * kind needs, it ends without a blow; or fight him. Any ending wins the game: the credits,
 * then the world to explore on. */

static int fl(float v) { return (int)floorf(v); }
static void upper(char *s) { for (; *s; s++) *s = (char)toupper((unsigned char)*s); }

void king_settle(Game *g, PlayerSlot *ps, int ending);
static const KingWay *way(Game *g) { return king_way(&g->world, g->realms_joined); }
bool king_won(const Game *g) { return g->world.king_ending != KE_NONE; }
static int tracked_step(Game *g, const KingWay *K, const PlayerSlot *ps);
static vec3 king_last_pos; /* where he was last (his hoard falls there) */
static bool done(const Game *g, int i) { return (g->world.king_done >> i) & 1; }

static bool group_done(const Game *g, const KingWay *K, int grp) {
    for (int i = 0; i < KING_STEPS; i++)
        if (K->step[i].group == grp && !done(g, i)) return false;
    return true;
}

static bool step_open(const Game *g, const KingWay *K, int i) {
    if (done(g, i) || king_won(g)) return false;
    int grp = K->step[i].group;
    for (int k = 0; k < grp; k++)
        if (!group_done(g, K, k)) return false;
    return true;
}

static int find_step(const Game *g, const KingWay *K, int type) {
    for (int i = 0; i < KING_STEPS; i++)
        if (K->step[i].type == type) return i;
    return -1;
}

static bool artefact_held(const Game *g, const KingWay *K) {
    int i = find_step(g, K, KING_KINDS_DEF[K->kind].artefact_step);
    return i >= 0 && done(g, i);
}

/* ---------- names and texts ---------- */

static void town_name(Game *g, const KingStep *st, char *out, int n) {
    Town t;
    if (world_town_at(&g->world, st->cx, st->cy, st->cz, &t)) snprintf(out, (size_t)n, "%s", t.name);
    else snprintf(out, (size_t)n, "the town");
}

static void lair_name(Game *g, const KingStep *st, char *out, int n) {
    Structure s;
    Lair L;
    if (world_structure_at(&g->world, st->cx, st->cy, st->cz, &s) && lair_from_structure(&s, &L)) snprintf(out, (size_t)n, "%s", L.name);
    else snprintf(out, (size_t)n, "the dragon");
}

static void king_called(Game *g, const KingWay *K, char *out, int n) {
    if (g->world.king_flags & KF_NAME) snprintf(out, (size_t)n, "%s", K->name);
    else if (g->world.king_flags & KF_KIND) snprintf(out, (size_t)n, "%s", KING_KINDS_DEF[K->kind].name);
    else snprintf(out, (size_t)n, "the Old King");
}

/* What a step gives: the artefact his kind needs, or something of the way. */
static const char *thing_of(const KingWay *K, int type) {
    if (KING_KINDS_DEF[K->kind].artefact_step == type) return KING_KINDS_DEF[K->kind].artefact;
    switch (type) {
    case KS_HOARD: return "the Key of the Old King";
    case KS_FORGE: return "the King-Breaker";
    case KS_BREW: return "the Elixir of Clear Sight";
    case KS_RELIC: return "the Seal-Key";
    default: return "it";
    }
}

static void fortress(Game *g, char *out, int n) {
    const History *h = g->world.story_gen ? history_of(g->world.seed) : NULL;
    snprintf(out, (size_t)n, "%s", h ? h->fortress : "the fortress");
}

/* %K the king, %F the fortress, %A the artefact */
static void fill(Game *g, const KingWay *K, const char *tmpl, char *out, int n) {
    char k[64], f[64];
    snprintf(k, sizeof k, "%s", K->name);
    fortress(g, f, sizeof f);
    int o = 0;
    for (const char *p = tmpl; *p && o < n - 1; p++) {
        const char *ins = NULL;
        if (p[0] == '%' && p[1] == 'K') ins = k;
        else if (p[0] == '%' && p[1] == 'F') ins = f;
        else if (p[0] == '%' && p[1] == 'A') ins = KING_KINDS_DEF[K->kind].artefact;
        if (ins) { for (; *ins && o < n - 1; ins++) out[o++] = *ins; p++; }
        else out[o++] = *p;
    }
    out[o] = 0;
}

/* The chronicle (the motive's chapters, quests.c) is a way to step 8: its end, the opener, served him. */
static bool chronicle_on(const Game *g) {
    for (int k = 0; k < MAX_PLAYERS; k++)
        if (g->players[k].active && g->players[k].st.motive) return true;
    return false;
}

/* The short line of a step: what to do (the tracker, the STORY tab). */
static void step_goal(Game *g, const KingWay *K, int i, char *out, int n) {
    const KingStep *st = &K->step[i];
    char a[64];
    switch (st->type) {
    case KS_HERALD: town_name(g, st, a, sizeof a); snprintf(out, (size_t)n, "speak to the elder of %s", a); break;
    case KS_WITNESS: snprintf(out, (size_t)n, "find the one who saw him"); break;
    case KS_TRUENAME: snprintf(out, (size_t)n, "read the king's stele"); break;
    case KS_VISION: snprintf(out, (size_t)n, "sleep in the Sanctum's bed and dream"); break;
    case KS_DRAGON: lair_name(g, st, a, sizeof a); snprintf(out, (size_t)n, "slay %s in its lair", a); break;
    case KS_WYRM: lair_name(g, st, a, sizeof a); snprintf(out, (size_t)n, "slay %s, the guardian of the way", a); break;
    case KS_HOARD: lair_name(g, st, a, sizeof a); snprintf(out, (size_t)n, "take %s from the hoard of %s", thing_of(K, KS_HOARD), a); break;
    case KS_ALLEGIANCE: town_name(g, st, a, sizeof a); snprintf(out, (size_t)n, "become a friend of %s (do its tasks)", a); break;
    case KS_FORGE:
        snprintf(out, (size_t)n, "forge %s at the Sanctum's anvil: a titan heart, 3 essences, 2 steel", thing_of(K, KS_FORGE));
        break;
    case KS_BREW: town_name(g, st, a, sizeof a); snprintf(out, (size_t)n, "bring dragon's blood and an orchid to the healer of %s", a); break;
    case KS_REALM: snprintf(out, (size_t)n, "defeat %s in %s, through the Gate", REALMS[st->arg].ruler, REALMS[st->arg].name); break;
    case KS_RELIC: snprintf(out, (size_t)n, "take %s from its guards", thing_of(K, KS_RELIC)); break;
    case KS_LIEUTENANT: snprintf(out, (size_t)n, st->group == 2 && chronicle_on(g) ? "kill the king's lieutenant, or end the chronicle" : "kill the king's lieutenant"); break;
    case KS_SEAL: snprintf(out, (size_t)n, st->group == 2 && chronicle_on(g) ? "break the king's seal, or end the chronicle" : "break the king's seal"); break;
    default: snprintf(out, (size_t)n, "go to %s", KING_KINDS_DEF[K->kind].throne); break;
    }
}

/* How to do a step: the way teaches the game (said when its group opens). */
static const char *step_hint(int type) {
    switch (type) {
    case KS_WITNESS: return "Someone who saw him waits in the halls, with his servants about.";
    case KS_TRUENAME: return "His name is carved on a stele; what you read stays in the STORY tab.";
    case KS_VISION: return "Sleep in the Sanctum's bed: the Sanctum Door takes you home.";
    case KS_DRAGON: return "A great dragon sleeps on its hoard: crouch to come close unheard. Its heart is a titan heart for the forge.";
    case KS_HOARD: return "Take the thing from a dragon's hoard crouching, or it wakes.";
    case KS_ALLEGIANCE: return "A town's friendship: do its elder's tasks, or crouch before the elder and give to its works.";
    case KS_FORGE: return "Build the forge in the Sanctum (after the island's second stage); at its anvil a titan heart, 3 essences of one element and 2 steel.";
    case KS_BREW: return "A healer can brew it: dragon's blood (from a great dragon's body) and an orchid (the elves sell them).";
    case KS_REALM: return "Rifts in the fortress lead behind the Gate; once one is walked, the Gate in the Sanctum's court goes there too.";
    case KS_RELIC: return "A relic, held by his servants.";
    case KS_LIEUTENANT: return "His lieutenant comes at you through the dark: stand your ground.";
    case KS_SEAL: return "His seal is guarded by five.";
    case KS_WYRM: return "A wyrm guards the way, the lair nearest his throne.";
    default: return "";
    }
}

/* What is told when a step is done. */
static void step_told(Game *g, const KingWay *K, int i, char *out, int n) {
    const KingStep *st = &K->step[i];
    const KingKind *KD = &KING_KINDS_DEF[K->kind];
    char a[64], f[64];
    fortress(g, f, sizeof f);
    switch (st->type) {
    case KS_HERALD:
        town_name(g, st, a, sizeof a);
        if (K->gen)
            snprintf(out, (size_t)n, "The elder of %s shakes her head. 'You ended one king, and the throne of %s took another. Find out what this one is - "
                     "his name is written somewhere too - and go to him again. Further out, this time.'", a, f);
        else
            snprintf(out, (size_t)n, "The elder of %s lowers her voice. 'There is a king in %s still, the Old King, who never died. "
                     "Find out what he is - his name is carved somewhere, and someone saw him - then go to him.' "
                     "(The Golden Thread, G, leads the way; the Sanctum Door takes you home.)", a, f);
        break;
    case KS_WITNESS:
        snprintf(out, (size_t)n, "The witness grips your arm. 'I saw him. He is %s - %s. I ran. I have been running since.'", KD->name, KD->what);
        break;
    case KS_TRUENAME:
        snprintf(out, (size_t)n, "The stele is carved with a name, scratched out a hundred times and carved again: %s. Whoever wrote it wanted it remembered.", K->name);
        break;
    case KS_VISION:
        snprintf(out, (size_t)n, "You dream of %s, and of a figure on a throne there: %s, %s. It turns its head and looks at you, and you wake.", KD->throne, KD->name, KD->what);
        break;
    case KS_DRAGON: case KS_WYRM:
        lair_name(g, st, a, sizeof a);
        snprintf(out, (size_t)n, st->type == KS_WYRM ? "%s is dead, and the way to the throne is open. Somewhere ahead the Old King has felt it." :
                                                       "%s is dead. It served the Old King; in its hoard is a tribute with his seal. He will know you are coming.", a);
        break;
    case KS_HOARD: snprintf(out, (size_t)n, "You take %s from the hoard. It is cold, and heavier than it looks, and it hums when you turn towards the throne.", thing_of(K, KS_HOARD)); break;
    case KS_ALLEGIANCE: town_name(g, st, a, sizeof a); snprintf(out, (size_t)n, "%s will stand with you. When you face the Old King, their soldiers will be at your side.", a); break;
    case KS_FORGE: snprintf(out, (size_t)n, "The dragon's heart burns in the forge all night. At dawn %s lies on the anvil, still warm.", thing_of(K, KS_FORGE)); break;
    case KS_BREW: town_name(g, st, a, sizeof a); snprintf(out, (size_t)n, "The healer of %s brews all night and gives you %s in a stoppered flask. 'Do not drink it. It is not for you.'", a, thing_of(K, KS_BREW)); break;
    case KS_REALM: snprintf(out, (size_t)n, "%s falls, and with its last breath names the Old King as the one it served. Through the Gate his hold is broken.", REALMS[st->arg].ruler); break;
    case KS_RELIC: snprintf(out, (size_t)n, "You take %s. Its guards served the Old King; it was hidden from him as much as for him.", thing_of(K, KS_RELIC)); break;
    case KS_LIEUTENANT: case KS_SEAL:
        if (st->group == 2 && (g->world.king_flags & KF_CHRONICLE))
            snprintf(out, (size_t)n, "The one who opened the seal of %s served the Old King all along. Among their papers are his orders, and the way to %s.", f, KD->throne);
        else if (st->type == KS_LIEUTENANT) snprintf(out, (size_t)n, "His lieutenant falls and in its hand are the king's orders: the way to %s.", KD->throne);
        else snprintf(out, (size_t)n, "The seal cracks like ice. Far off, in %s, something stirs on a throne.", KD->throne);
        break;
    default: snprintf(out, (size_t)n, "The throne."); break;
    }
}

void king_lore_text(Game *g, int a, char *out, int n) {
    const KingWay *K = way(g);
    if (!K->valid || a < 0 || a >= KING_STEPS) { snprintf(out, (size_t)n, "The Old King."); return; }
    char t[STORY_TEXT];
    step_told(g, K, a, t, sizeof t);
    snprintf(out, (size_t)n, "The Old King, step %d: %s", a + 1, t);
}

static void say(PlayerSlot *ps, const char *text, const char *then) {
    snprintf(ps->msg, sizeof ps->msg, "%s", text);
    snprintf(ps->msg_next, sizeof ps->msg_next, "%s", then ? then : "");
    ps->msg_t = 9.0f + (float)strlen(text) * 0.05f;
    if (g_note_hook) { g_note_hook(ps->index, "KING", text); if (then && then[0]) g_note_hook(ps->index, "KING", then); }
}

static void complete(Game *g, PlayerSlot *ps, int i) {
    const KingWay *K = way(g);
    if (done(g, i)) return;
    int grp = K->step[i].group;
    g->world.king_done |= (u16)(1u << i);
    int t = K->step[i].type;
    if (t == KS_TRUENAME) g->world.king_flags |= KF_NAME | KF_KIND;
    if (t == KS_WITNESS || t == KS_VISION) g->world.king_flags |= KF_KIND;
    int na = 0;
    for (int k = 0; k < KING_STEPS; k++) na += K->step[k].group == 0 && done(g, k);
    if (na >= 2) g->world.king_flags |= KF_KIND;
    lore_add(g, LORE_KING, i, 0, 0);
    char told[STORY_TEXT], then[STORY_TEXT] = "";
    step_told(g, K, i, told, sizeof told);
    if (group_done(g, K, grp) && grp < 4) {
        static const char *NEXT[4] = { "Now you know what he is. Gather what you will need to face him (four things, in any order).",
                                       "You have what you need. Now the approach: his servants stand between you and the throne.",
                                       "One more stands in the way.", "The way to the throne is open. He will not listen standing: bring him to his knees first." };
        snprintf(then, sizeof then, "%s", NEXT[grp]);
    }
    for (int k = 0; k < MAX_PLAYERS; k++)
        if (g->players[k].active) say(&g->players[k], told, then);
    char b[96];
    int ndone = 0;
    for (int k = 0; k < KING_STEPS; k++) ndone += done(g, k);
    snprintf(b, sizeof b, "THE OLD KING - %d OF %d", ndone, KING_STEPS);
    game_banner_all(g, b);
    sfx_at(g, SFX_CHAPTER_DONE, ps ? ps->pl.pos : g->players[0].pl.pos, 1.0f);
    g_tele.king_steps++;
    g->autosave_timer = 1e9f;
}

/* ---------- the steps done by talking and resting ---------- */

bool king_town_talk(Game *g, PlayerSlot *ps, Folk *f, u64 town_key) {
    const KingWay *K = way(g);
    if (!K->valid) return false;
    for (int i = 0; i < KING_STEPS; i++) {
        const KingStep *st = &K->step[i];
        if (!step_open(g, K, i) || st->key != town_key) continue;
        if (st->type == KS_HERALD && f->role == FR_ELDER) { complete(g, ps, i); return true; }
        if (st->type == KS_BREW && f->role == FR_HEALER) {
            Stats *s = &ps->st;
            int blood = s->reag[RG_PHOENIX] ? RG_PHOENIX : s->reag[RG_HEARTSTONE] ? RG_HEARTSTONE : -1;
            if (blood < 0 || !s->reag[RG_ORCHID]) {
                say(ps, "'For what you ask I need a dragon's blood - phoenix ash or a heartstone from its body - and an elven orchid. Bring them, and I will brew it.'", NULL);
                return true;
            }
            s->reag[blood]--;
            s->reag[RG_ORCHID]--;
            complete(g, ps, i);
            return true;
        }
    }
    return false;
}

void king_rest(Game *g, PlayerSlot *ps) {
    const KingWay *K = way(g);
    if (!K->valid) return;
    int i = find_step(g, K, KS_VISION);
    if (i >= 0 && step_open(g, K, i)) complete(g, ps, i);
}

/* ---------- markers in the world: a stele, a relic, a seal, the hoard's thing, the anvil ---------- */

enum { MK_STELE, MK_RELIC, MK_SEAL, MK_HOARD, MK_FORGE };
#define MAX_MARKS 8
static struct { bool on; int step, kind; vec3 pos; } marks[MAX_MARKS];
static u64 marks_seed;

static bool standable(Game *g, int x, int y, int z) {
    return game_solid(g, x, y - 1, z) == 1 && game_solid(g, x, y, z) == 0 && game_solid(g, x, y + 1, z) == 0 && game_solid(g, x, y + 2, z) == 0;
}

static bool spot_in_chunk(Game *g, int cx, int cy, int cz, u64 seed, vec3 *out) {
    /* on the layer's own floor first, where one walks (not atop a pillar or in a sealed pocket) */
    for (int a = 0; a < 300; a++) {
        u64 h = mix64(seed ^ (u64)a * 0x31);
        int x = cx * CHUNK + 3 + (int)(h % 26), z = cz * CHUNK + 3 + (int)((h >> 8) % 26);
        for (int y = cy * CHUNK + LAYER_FLOOR; y <= cy * CHUNK + LAYER_FLOOR + 2; y++)
            if (standable(g, x, y, z) && standable(g, x + 1, y, z) && standable(g, x, y, z + 1)) { *out = v3(x + 0.5f, (float)y, z + 0.5f); return true; }
    }
    for (int a = 0; a < 300; a++) {
        u64 h = mix64(seed ^ (u64)a * 0x31);
        int x = cx * CHUNK + 3 + (int)(h % 26), z = cz * CHUNK + 3 + (int)((h >> 8) % 26);
        for (int y = cy * CHUNK + 28; y > cy * CHUNK + 1; y--)
            if (standable(g, x, y, z)) { *out = v3(x + 0.5f, (float)y, z + 0.5f); return true; }
    }
    return false;
}

static vec3 step_centre(const KingStep *st) { return v3(st->cx * CHUNK + 16.0f, st->cy * CHUNK + 8.0f, st->cz * CHUNK + 16.0f); }

static int minion_type(int kind) {
    static const u8 M[KING_KINDS] = { MON_KNIGHT, MON_SKELETON, MON_WISP, MON_HOUND, MON_WRAITH, MON_GOLEM, MON_HOUND, MON_SKELETON, MON_SLIME, MON_WISP };
    return M[kind];
}

static void guards(Game *g, const KingWay *K, vec3 at, int level, int n, u64 key) {
    for (int k = 0; k < n; k++) {
        float a = k * 6.283f / n;
        vec3 p = v3_add(at, v3(cosf(a) * 3.0f, 0, sinf(a) * 3.0f));
        if (!standable(g, fl(p.x), fl(p.y), fl(p.z))) p = at;
        Monster *m = monster_spawn(g, minion_type(K->kind), p, level, k == 0, v3(0.5f, 0.45f, 0.35f), key);
        if (m) m->king = 3;
    }
}

static int chunk_level(Game *g, int cx, int cy, int cz) {
    ChunkInfo ci;
    world_chunk_info(&g->world, cx, cy, cz, &ci);
    return ci.level;
}

static void place_things(Game *g, const KingWay *K) {
    if (marks_seed != g->world.seed) { memset(marks, 0, sizeof marks); marks_seed = g->world.seed; }
    for (int i = 0; i < KING_STEPS; i++) {
        const KingStep *st = &K->step[i];
        if (!step_open(g, K, i)) continue;
        int mk = st->type == KS_TRUENAME ? MK_STELE : st->type == KS_RELIC ? MK_RELIC : st->type == KS_SEAL ? MK_SEAL : st->type == KS_HOARD ? MK_HOARD :
                 st->type == KS_FORGE ? MK_FORGE : -1;
        bool there = false;
        for (int k = 0; k < MAX_MARKS; k++) there |= marks[k].on && marks[k].step == i;
        if (mk >= 0 && !there) {
            vec3 p;
            bool found = false;
            if (mk == MK_FORGE) {
                if (sanctum_stage(g, SP_FORGE) >= 1 && sanctum_stage(g, SP_ISLAND) >= 2) { p = v3(POCKET_ANVIL_X + 0.5f, (float)POCKET_G + 1.2f, POCKET_ANVIL_Z + 0.5f); found = true; }
            } else if (mk == MK_HOARD) {
                Structure s;
                Lair L;
                if (world_structure_at(&g->world, st->cx, st->cy, st->cz, &s) && lair_from_structure(&s, &L)) { p = v3(L.cx + 0.5f, (float)L.floor + 3.5f, L.cz + 0.5f); found = true; }
            } else {
                vec3 c = step_centre(st);
                Chunk *ch = chunkmap_get(&g->map, st->cx, st->cy, st->cz);
                if (ch && ch->state == CH_READY && game_nearest_player(g, c, 70.0f)) found = spot_in_chunk(g, st->cx, st->cy, st->cz, g->world.seed ^ (u64)i * 0xC0DE, &p);
            }
            if (found && (mk == MK_FORGE || game_nearest_player(g, p, 90.0f)))
                for (int k = 0; k < MAX_MARKS; k++)
                    if (!marks[k].on) {
                        marks[k].on = true; marks[k].step = i; marks[k].kind = mk; marks[k].pos = p;
                        if (g_note_hook) { char b[96]; snprintf(b, sizeof b, "marker for step %d at %.0f %.0f %.0f", i + 1, p.x, p.y, p.z); g_note_hook(0, "KINGMARK", b); }
                        if (mk == MK_RELIC || mk == MK_SEAL) guards(g, K, p, chunk_level(g, st->cx, st->cy, st->cz) + 2, mk == MK_SEAL ? 5 : 3, chunk_spawn_key(st->cx, st->cy, st->cz));
                        break;
                    }
        }
        /* people and enemies that belong to a step */
        if (st->type == KS_WITNESS || st->type == KS_LIEUTENANT) {
            vec3 c = step_centre(st);
            if (!game_nearest_player(g, c, 60.0f)) continue;
            Chunk *ch = chunkmap_get(&g->map, st->cx, st->cy, st->cz);
            if (!ch || ch->state != CH_READY) continue;
            bool alive = false;
            for (int k = 0; k < MAX_FOLK && !alive; k++) alive = g->folk[k].alive && g->folk[k].rec == -100 - i;
            for (int k = 0; k < MAX_MONSTERS && !alive; k++) alive = g->mons[k].alive && g->mons[k].kingstep == i + 1;
            if (alive) continue;
            vec3 p;
            if (!spot_in_chunk(g, st->cx, st->cy, st->cz, g->world.seed ^ (u64)i * 0x7E57, &p)) continue;
            if (game_nearest_player(g, p, 10.0f)) continue;
            int level = chunk_level(g, st->cx, st->cy, st->cz);
            if (st->type == KS_WITNESS) {
                Folk *f = NULL;
                for (int k = 0; k < MAX_FOLK && !f; k++)
                    if (!g->folk[k].alive) f = &g->folk[k];
                if (!f) continue;
                memset(f, 0, sizeof *f);
                f->alive = true; f->people = PE_HUMAN; f->role = FR_LOST; f->pos = f->home = f->goal = p; f->rec = -100 - i;
                f->key = g->world.seed ^ 0x3E7;
                people_name(PE_HUMAN, f->key, f->name, sizeof f->name);
                guards(g, K, p, level, 3, chunk_spawn_key(st->cx, st->cy, st->cz));
            } else {
                Monster *m = monster_spawn(g, minion_type(K->kind) == MON_SKELETON ? MON_NECRO : MON_KNIGHT, p, level + 3, true, v3(0.6f, 0.2f, 0.2f), chunk_spawn_key(st->cx, st->cy, st->cz));
                if (m) {
                    monster_make_boss(g, m);
                    m->king = 2;
                    m->kingstep = (u8)(i + 1);
                    static const char *T[5] = { "the Black Herald", "the Iron Hand", "the King's Shadow", "the Grey Marshal", "the Crowned Hound" };
                    snprintf(m->name, sizeof m->name, "%s", T[mix64(g->world.seed ^ (u64)i) % 5]);
                    upper(m->name);
                    guards(g, K, p, level, 3, m->chunk_key);
                }
            }
        }
    }
}

static void touch(Game *g, PlayerSlot *ps, int k) {
    const KingWay *K = way(g);
    int i = marks[k].step;
    if (!step_open(g, K, i)) { marks[k].on = false; return; }
    if (marks[k].kind == MK_FORGE) {
        Stats *s = &ps->st;
        int el = EL_FIRE;
        int ess = 0, best = 0;
        for (int e = 0; e < 8; e++) if (s->forge[FG_ESS0 + e] > best) { best = s->forge[FG_ESS0 + e]; el = e; }
        ess = best;
        if (!s->forge[FG_TITAN_HEART] || ess < 3 || s->forge[FG_STEEL] < 2) {
            if (ps->banner_time <= 0) game_banner(ps, "THE ANVIL NEEDS A TITAN HEART (A DRAGON'S), 3 ESSENCES OF ONE ELEMENT AND 2 STEEL");
            return;
        }
        s->forge[FG_TITAN_HEART]--;
        s->forge[FG_ESS0 + el] -= 3;
        s->forge[FG_STEEL] -= 2;
        particles_burst(g, marks[k].pos, v3(1.4f, 0.8f, 0.3f), 30, 4.0f, 0.06f, true);
        sfx_at(g, SFX_ANVIL, marks[k].pos, 1.0f);
    }
    marks[k].on = false;
    particles_burst(g, marks[k].pos, v3(1.2f, 1.0f, 0.5f), 24, 3.0f, 0.05f, true);
    complete(g, ps, i);
}

bool king_folk_talk(Game *g, PlayerSlot *ps, Folk *f) {
    if (f->rec == -50) { say(ps, "'We are with you. Lead on.'", NULL); return true; }
    int i = -100 - f->rec;
    const KingWay *K = way(g);
    if (i < 0 || i >= KING_STEPS || !K->valid) return false;
    if (step_open(g, K, i)) complete(g, ps, i);
    particles_burst(g, v3_add(f->pos, v3(0, 1, 0)), v3(1.0f, 0.9f, 0.6f), 16, 2.0f, 0.05f, true);
    f->alive = false;
    return true;
}

void king_on_kill(Game *g, Monster *m) {
    const KingWay *K = way(g);
    if (!K->valid) return;
    if (m->king == 2 && m->kingstep >= 1 && m->kingstep <= KING_STEPS) {
        int i = m->kingstep - 1;
        if (step_open(g, K, i)) complete(g, game_nearest_player(g, m->pos, 1e9f), i);
    }
    if (m->king == 1 && !king_won(g)) {
        king_last_pos = m->pos;
        king_settle(g, game_nearest_player(g, m->pos, 1e9f), KE_FIGHT);
    }
}

/* ---------- the throne ---------- */

static bool throne_place(Game *g, const KingWay *K, vec3 *seat, vec3 *centre) {
    const KingStep *st = &K->step[9];
    Structure s;
    if (!world_structure_at(&g->world, st->cx, st->cy, st->cz, &s) || s.variant != HALL_THRONE) return false;
    int X0 = s.cx0 * CHUNK, Z0 = s.cz0 * CHUNK, X1 = X0 + s.sx * CHUNK - 1, Z1 = Z0 + s.sz * CHUNK - 1;
    int mz = (Z0 + Z1) / 2, back = X1 - 6;
    *seat = v3(back - 2.5f, (float)s.floor_y + 3.0f, mz + 0.5f);
    *centre = v3((X0 + X1) * 0.5f, (float)s.floor_y, mz + 0.5f);
    return true;
}

static Monster *the_king(Game *g) {
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (g->mons[i].alive && g->mons[i].king == 1) return &g->mons[i];
    return NULL;
}

/* He falters: brought to his knees (half his life, then a fifth), he listens for a while. Only
 * then do his true name and the artefact reach him; or he can be finished from afar. */
static int falters;
static float falter_t;
bool king_faltering(const Monster *m) { return m && m->king == 1 && falter_t > 0; }

static void spawn_king(Game *g, const KingWay *K, vec3 seat) {
    int level = chunk_level(g, K->step[9].cx, K->step[9].cy, K->step[9].cz) + 5 + 6 * MINI(K->gen, 4);
    falters = 0;
    falter_t = 0;
    Monster *m;
    if (K->kind == KK_DRAGON) {
        m = dragon_spawn(g, DK_GOLD, DS_ANCIENT, v3(seat.x - 12.0f, seat.y - 3.0f, seat.z), level, g->world.seed ^ 0xD4A60);
        if (m) m->dhome = m->pos;
    } else {
        static const u8 T[KING_KINDS] = { MON_KNIGHT, MON_NECRO, MON_SHAMAN, MON_KNIGHT, MON_WRAITH, MON_GOLEM, MON_TROLL, MON_SHAMAN, MON_BLOATER, MON_GOLEM };
        m = monster_spawn(g, T[K->kind], seat, level, true, v3(0.7f, 0.6f, 0.3f), chunk_spawn_key(K->step[9].cx, K->step[9].cy, K->step[9].cz));
        if (m) {
            monster_make_boss(g, m);
            m->scale *= 1.5f;
            m->maxhp = m->hp = m->maxhp * 3.0f;
            m->dmg *= 1.3f;
        }
    }
    if (!m) return;
    m->king = 1;
    m->resident = 1; /* he waits on his throne until spoken to, or struck */
    m->awake = false;
    snprintf(m->name, sizeof m->name, "%s", K->name);
    upper(m->name);
}

void king_provoked(Game *g, Monster *m) {
    if (!m->resident) return;
    m->resident = 0;
    m->awake = true;
    m->state = MS_CHASE;
    char b[96];
    snprintf(b, sizeof b, "%s RISES FROM THE THRONE", m->name);
    game_banner_all(g, b);
    sfx_at(g, SFX_NAMED_DEATH, m->pos, 1.0f);
    /* the friends you made stand with you */
    const KingWay *K = way(g);
    int a = find_step(g, K, KS_ALLEGIANCE);
    if (a >= 0 && done(g, a)) {
        PlayerSlot *ps = game_nearest_player(g, m->pos, 1e9f);
        for (int k = 0; k < 4 && ps; k++)
            for (int j = 0; j < MAX_FOLK; j++) {
                Folk *f = &g->folk[j];
                if (f->alive) continue;
                memset(f, 0, sizeof *f);
                f->alive = true; f->people = (u8)K->step[a].arg; f->role = FR_GUARD; f->rec = -50;
                f->pos = f->home = f->goal = v3_add(ps->pl.pos, v3((k - 1.5f) * 1.5f, 0, -1.5f));
                f->key = g->world.seed ^ (u64)(k + 77);
                people_name(f->people, f->key, f->name, sizeof f->name);
                break;
            }
        game_banner_all(g, "YOUR ALLIES ARE WITH YOU");
    }
}

float king_damage(Game *g, Monster *m, float dmg) {
    const KingWay *K = way(g);
    if (m->king != 1 || !K->valid) return dmg;
    if (K->kind == KK_NECRO && !artefact_held(g, K)) { /* his life is elsewhere */
        PlayerSlot *ps = game_nearest_player(g, m->pos, 30.0f);
        if (ps && ps->banner_time <= 0) game_banner(ps, "HIS LIFE IS NOT IN HIS BODY - THE PHYLACTERY HOLDS IT");
        return dmg * 0.05f;
    }
    int f = find_step(g, K, KS_FORGE);
    if (f >= 0 && done(g, f)) dmg *= 1.4f; /* the King-Breaker */
    /* brought to his knees: at half his life, and again at a fifth, he falters and listens */
    bool can_speak = (g->world.king_flags & KF_NAME) || artefact_held(g, K);
    float thr = falters == 0 ? 0.5f : falters == 1 ? 0.2f : -1.0f;
    if (can_speak && thr > 0 && falter_t <= 0 && m->hp - dmg <= m->maxhp * thr) {
        dmg = fmaxf(0.0f, m->hp - m->maxhp * thr);
        falters++;
        falter_t = 12.0f;
        m->state = MS_STUN;
        m->timer = 12.0f;
        char b[128];
        snprintf(b, sizeof b, "%s FALTERS - STRIKE TO SPEAK%s, OR END HIM FROM AFAR", m->name, artefact_held(g, K) ? " (CROUCH: THE ARTEFACT)" : "");
        game_banner_all(g, b);
        particles_burst(g, v3_add(m->pos, v3(0, 2, 0)), v3(1.2f, 1.0f, 0.5f), 40, 3.0f, 0.06f, true);
        sfx_at(g, SFX_MONSTER_STUN, m->pos, 1.0f);
        for (int k = 0; k < MAX_PLAYERS; k++)
            if (g->players[k].active) say(&g->players[k], "He sinks to one knee on the steps of his throne. For a moment he is only old, and he is listening.", NULL);
    }
    /* at two thirds and one third his servants come */
    float before = (m->hp) / m->maxhp, after = (m->hp - dmg) / m->maxhp;
    if ((before > 0.66f && after <= 0.66f) || (before > 0.33f && after <= 0.33f)) {
        char b[96];
        snprintf(b, sizeof b, "%s CALLS %s", m->name, KING_KINDS_DEF[K->kind].minions);
        upper(b);
        game_banner_all(g, b);
        guards(g, K, m->pos, (int)m->level - 3, 4, m->chunk_key);
    }
    return dmg;
}

void king_settle(Game *g, PlayerSlot *ps, int ending) {
    const KingWay *K = way(g);
    if (king_won(g)) return;
    g->world.king_ending = (u8)ending;
    g->world.king_done |= 1u << 9;
    g->world.king_wins = (u8)MINI(255, g->world.king_wins + 1);
    if (ending != KE_FIGHT) g->world.king_mercy = 1;
    falter_t = 0;
    Monster *m = the_king(g);
    vec3 at = m ? m->pos : king_last_pos;
    vec3 up = v3_add(at, v3(0, 0.8f, 0));
    float lvl = m ? m->level : (float)(chunk_level(g, K->step[9].cx, K->step[9].cy, K->step[9].cz) + 5);
    switch (ending) {
    case KE_FIGHT: { /* the sword: his crown, his own blade and the throne's hoard */
        for (int k = 0; k < 14; k++) pickup_spawn(g, PK_GOLD, v3_add(up, v3(cosf(k * 0.9f) * 2.5f, 0, sinf(k * 0.9f) * 2.5f)), 80 + (int)lvl * 12, NULL, 0, 0);
        for (int k = 0; k < 4; k++) loot_drop(g, at, lvl + 5.0f, true, 1.0f);
        int tier, region = world_region(&g->world, floordiv(fl(at.x), CHUNK), floordiv(fl(at.y), CHUNK), floordiv(fl(at.z), CHUNK), &tier);
        Item it = item_roll_region(mix64(g->world.seed ^ 0xC20A ^ (u64)K->gen), lvl + 10.0f, RAR_LEGENDARY, tier, region);
        pickup_spawn(g, PK_WEAPON, v3_add(up, v3(0, 0.6f, 0)), 0, &it, 0, 0);
        pickup_spawn(g, PK_RELIC, v3_add(up, v3(0.8f, 0, 0)), 0, NULL, 0, 0); /* the crown */
        pickup_spawn(g, PK_FORGE, v3_add(up, v3(-0.8f, 0, 0)), FG_TITAN_HEART, NULL, 0, 0);
        for (int k = 0; k < 3; k++) pickup_spawn(g, PK_KEYSTONE, v3_add(up, v3(0, 0, 0.7f * (k - 1))), 1, NULL, 0, 0);
        sanctum_trophy_named(g, g->world.seed ^ 0xC20A ^ (u64)K->gen);
        break;
    }
    case KE_WORD: /* his knights swear to the towns; his treasury is yours */
        g->world.king_flags |= KF_SWORN;
        for (int k = 0; k < 8; k++) pickup_spawn(g, PK_GOLD, v3_add(up, v3(cosf(k * 0.8f) * 2.0f, 0, sinf(k * 0.8f) * 2.0f)), 100 + (int)lvl * 8, NULL, 0, 0);
        for (int k = 0; k < 2; k++) pickup_spawn(g, PK_KEYSTONE, v3_add(up, v3(0, 0, 0.7f * k)), 1, NULL, 0, 0);
        break;
    case KE_ARTEFACT: /* the bound are free: every town knows it was you */
        g->world.king_flags |= KF_FREED;
        for (int i = 0; i < g->world.ntowns; i++) g->world.towns[i].rep = (i8)MINI(100, g->world.towns[i].rep + 2);
        pickup_spawn(g, PK_KEYSTONE, up, 1, NULL, 0, 0);
        loot_drop(g, at, lvl + 3.0f, true, 1.0f);
        break;
    }
    if (m && ending != KE_FIGHT) {
        particles_burst(g, v3_add(m->pos, v3(0, 2, 0)), v3(1.2f, 1.0f, 0.6f), 60, 4.0f, 0.08f, true);
        m->alive = false;
    }
    for (int i = 0; i < MAX_MONSTERS; i++) /* his servants (and raiders) are gone with him */
        if (g->mons[i].alive && (g->mons[i].king == 3 || g->mons[i].king == 4)) { particles_burst(g, g->mons[i].pos, v3(0.8f, 0.8f, 0.8f), 10, 2.0f, 0.05f, false); g->mons[i].alive = false; }
    char text[STORY_TEXT];
    const KingKind *KD = &KING_KINDS_DEF[K->kind];
    fill(g, K, ending == KE_WORD ? KD->word_end : ending == KE_ARTEFACT ? KD->art_end : KD->fight_end, text, sizeof text);
    for (int k = 0; k < MAX_PLAYERS; k++)
        if (g->players[k].active) { say(&g->players[k], text, NULL); g->players[k].credits_t = 0.01f; }
    lore_add(g, LORE_KING, 9, 0, 0);
    sfx_at(g, SFX_CHAPTER_DONE, ps ? ps->pl.pos : v3(0, 0, 0), 1.0f);
    g_tele.king_won++;
    g->autosave_timer = 1e9f;
    (void)ps;
}

bool king_use(Game *g, PlayerSlot *ps) {
    Monster *m = the_king(g);
    if (!m) return false;
    if (v3_len(v3_sub(m->pos, ps->pl.pos)) > (m->type == MON_DRAGON ? 14.0f : 9.0f)) return false;
    const KingWay *K = way(g);
    bool name = g->world.king_flags & KF_NAME, art = artefact_held(g, K);
    if (falter_t > 0) { /* on his knees, he listens: the word or the artefact settles it */
        if (!name && !art) return false;
        int end = ps->pl.stance != STANCE_STAND && art ? KE_ARTEFACT : name ? KE_WORD : KE_ARTEFACT;
        /* in co-op the most of those at the throne must want it (the sword needs no vote) */
        int present = 0, agree = 0;
        ps->king_vote = (u8)end;
        for (int k = 0; k < MAX_PLAYERS; k++) {
            PlayerSlot *p = &g->players[k];
            if (!p->active || p->st.dead || v3_len(v3_sub(p->pl.pos, m->pos)) > 40.0f) continue;
            present++;
            agree += p->king_vote == end;
        }
        if (agree * 2 > present) { king_settle(g, ps, end); return true; }
        char b[96];
        snprintf(b, sizeof b, "%d OF %d WANT TO %s - THE OTHERS MUST STRIKE TOO", agree, present, end == KE_WORD ? "SPEAK HIS NAME" : "RAISE THE ARTEFACT (CROUCHED)");
        game_banner_all(g, b);
        return true;
    }
    if (!m->resident) return false;
    char called[64], line[STORY_TEXT];
    king_called(g, K, called, sizeof called);
    snprintf(line, sizeof line, "%s looks down at you from the throne: %s. 'So. You came the whole way. Most do not. And you think words will move me?'", called,
             KING_KINDS_DEF[K->kind].what);
    say(ps, line, name || art ? "He will not listen standing. Bring him to his knees - then speak his name, or raise the artefact crouched. Or finish it." :
                                "You have nothing to say to him that he will hear. He rises.");
    if (name || art) game_banner(ps, "BRING HIM TO HIS KNEES, THEN SPEAK");
    king_provoked(g, m);
    return true;
}

/* ---------- the bot on the main way (bot.c, --bot king) ---------- */

/* Where the bot should go for the nearest step it can do now, and whether to strike there (to
 * talk). Steps that need home (the vision, the forge), the Gate, a town's tasks or a brew's
 * reagents are left to the rest of its play. */
/* Where a step happens now: the one to talk to, its marker, the lair, the king; and whether to
 * strike there (to talk). False for a step with no place in the fortress. */
static bool step_place(Game *g, const KingWay *K, int i, const PlayerSlot *ps, vec3 *at, bool *talk) {
    const KingStep *st = &K->step[i];
    vec3 p = step_centre(st);
    bool t = false;
    switch (st->type) {
    case KS_VISION: return false;
    case KS_REALM: { /* in the realm: its ruler, or the keep; at home: the Gate, if it shows the realm;
                      * in the fortress: its rift (the Grand Vestibule has one to the gentlest realm) */
        int r = st->arg, in = realm_at_w(ps->pl.pos.x, ps->pl.pos.y, ps->pl.pos.z);
        if (in == r) {
            int kx, ky, kz;
            realm_keep(r, &kx, &ky, &kz);
            p = v3(kx + 0.5f, (float)ky, kz + 0.5f);
            for (int k = 0; k < MAX_MONSTERS; k++) if (g->mons[k].alive && g->mons[k].ruler == r + 1) p = g->mons[k].pos;
            break;
        }
        if (in >= 0) return false; /* another realm: out first */
        if (realm_has(ps->pl.pos)) {
            if (!((g->world.realm_attuned >> r) & 1) || gate_dialed(g) != r) return false;
            p = gate_ring_pos();
            break;
        }
        p = v3(1 * CHUNK + 16.0f, (float)LAYER_FLOOR, 16.0f);
        for (int k = 0; k < g->nrifts; k++)
            if (g->rifts[k].alive && g->rifts[k].realm == r) p = g->rifts[k].pos;
        break;
    }
    case KS_FORGE: { /* the anvil, once its marker is there (the forge built, you at home) */
        bool on = false;
        for (int k = 0; k < MAX_MARKS; k++) if (marks[k].on && marks[k].step == i) { p = marks[k].pos; on = true; }
        if (!on) return false;
        break;
    }
    case KS_HERALD: case KS_BREW: case KS_ALLEGIANCE: {
        int role = st->type == KS_BREW ? FR_HEALER : FR_ELDER;
        for (int k = 0; k < MAX_FOLK; k++) {
            Folk *f = &g->folk[k];
            if (f->alive && f->role == role && f->rec >= 0 && f->rec < g->world.ntowns && g->world.towns[f->rec].key == st->key) { p = f->pos; t = true; }
        }
        break;
    }
    case KS_WITNESS:
        for (int k = 0; k < MAX_FOLK; k++) if (g->folk[k].alive && g->folk[k].rec == -100 - i) { p = g->folk[k].pos; t = true; }
        break;
    case KS_THRONE: {
        vec3 seat, centre;
        Monster *m = the_king(g);
        if (m) { p = m->pos; t = m->resident || falter_t > 0; }
        else if (throne_place(g, K, &seat, &centre)) p = centre;
        break;
    }
    default:
        for (int k = 0; k < MAX_MARKS; k++) if (marks[k].on && marks[k].step == i) p = marks[k].pos;
        if (st->type == KS_DRAGON || st->type == KS_WYRM) {
            Structure s;
            Lair L;
            if (world_structure_at(&g->world, st->cx, st->cy, st->cz, &s) && lair_from_structure(&s, &L)) p = v3(L.cx + 0.5f, (float)L.floor, L.cz + 0.5f);
        }
        for (int k = 0; k < MAX_MONSTERS; k++) if (g->mons[k].alive && g->mons[k].kingstep == i + 1) p = g->mons[k].pos;
        break;
    }
    *at = p;
    *talk = t;
    return true;
}

/* The bot: is a step open that is done at home (the vision; the forge with its marker there)? */
bool king_bot_home(Game *g) {
    const KingWay *K = way(g);
    if (!K->valid || king_won(g)) return false;
    for (int i = 0; i < KING_STEPS; i++)
        if (step_open(g, K, i) && K->step[i].type == KS_VISION) return true;
    return false;
}

/* The bot: what the open steps need that it lacks (KN_* bits). */
int king_bot_needs(Game *g, PlayerSlot *ps) {
    const KingWay *K = way(g);
    if (!K->valid || king_won(g)) return 0;
    const Stats *s = &ps->st;
    int need = 0, ess = 0;
    for (int e = 0; e < FG_ESS_COUNT; e++) ess = MAXI(ess, s->forge[FG_ESS0 + e]);
    for (int i = 0; i < KING_STEPS; i++) {
        if (!step_open(g, K, i)) continue;
        if (K->step[i].type == KS_FORGE) {
            if (!s->forge[FG_TITAN_HEART] || ess < 3) need |= KN_DRAGON;
            if (s->forge[FG_STEEL] < 2) need |= KN_STEEL;
            if (!(sanctum_stage(g, SP_ISLAND) >= 2 && sanctum_stage(g, SP_FORGE) >= 1)) need |= KN_FORGE;
        }
        if (K->step[i].type == KS_BREW) {
            if (!s->reag[RG_PHOENIX] && !s->reag[RG_HEARTSTONE]) need |= KN_DRAGON;
            if (!s->reag[RG_ORCHID]) need |= KN_ORCHID;
        }
    }
    return need;
}

/* The Golden Thread (quests.c) follows the line at the top: the step it follows, if it has a place. */
bool king_thread_goal(Game *g, PlayerSlot *ps, vec3 *at, char *name, int n) {
    const KingWay *K = way(g);
    if (!K->valid || king_won(g)) return false;
    int s = tracked_step(g, K, ps);
    bool talk;
    if (s < 0 || !step_place(g, K, s, ps, at, &talk)) return false;
    char goal[160];
    step_goal(g, K, s, goal, sizeof goal);
    snprintf(name, (size_t)n, "%.40s", goal);
    return true;
}

/* Where the bot should go for the nearest step it can do now, and whether to strike there (to
 * talk). Steps that need home (the vision, the forge), the Gate, a town's tasks or a brew's
 * reagents are left to the rest of its play. */
bool king_bot_target(Game *g, PlayerSlot *ps, vec3 *at, bool *talk) {
    const KingWay *K = way(g);
    if (!K->valid || king_won(g)) return false;
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < KING_STEPS; i++) {
        if (!step_open(g, K, i)) continue;
        int t = K->step[i].type;
        if (t == KS_BREW && (!(ps->st.reag[RG_PHOENIX] || ps->st.reag[RG_HEARTSTONE]) || !ps->st.reag[RG_ORCHID])) continue;
        if (t == KS_ALLEGIANCE && ps->st.gold < 500) continue; /* (the bot wins friendship with gold for the works) */
        vec3 p;
        bool tk;
        if (!step_place(g, K, i, ps, &p, &tk)) continue;
        float d = v3_len(v3_sub(p, ps->pl.pos));
        if (d < bd) { bd = d; best = i; *at = p; *talk = tk; }
    }
    return best >= 0;
}

/* ---------- raids on the towns ---------- */

/* Once you know what he is (steps 1-3 done), his servants raid the towns now and then. */
bool king_raids_on(Game *g) {
    const KingWay *K = way(g);
    return K->valid && !king_won(g) && group_done(g, K, 0);
}

int king_raid(Game *g, vec3 at, int level, int n, u64 key) {
    const KingWay *K = way(g);
    int made = 0;
    for (int k = 0; k < n; k++) {
        float a = k * 6.283f / n;
        vec3 p = v3_add(at, v3(cosf(a) * 2.5f, 0, sinf(a) * 2.5f));
        if (!standable(g, fl(p.x), fl(p.y), fl(p.z))) p = at;
        Monster *m = monster_spawn(g, minion_type(K->kind), p, level, k == 0, v3(0.5f, 0.45f, 0.35f), key);
        if (m) { m->king = 4; m->awake = true; m->state = MS_CHASE; made++; }
    }
    return made;
}

/* ---------- the heir ---------- */

void king_heir(Game *g) {
    const KingWay *old = way(g);
    char was[40];
    snprintf(was, sizeof was, "%s", old->name);
    g->world.king_gen++;
    g->world.king_done = 0;
    g->world.king_flags &= KF_SWORN | KF_FREED; /* what the last one's end changed stays */
    g->world.king_ending = KE_NONE;
    marks_seed = 0;
    const KingWay *K = way(g);
    if (!K->valid) return;
    /* the lairs on his way are taken again: new dragons, his */
    for (int i = 0; i < KING_STEPS; i++) {
        int t = K->step[i].type;
        if (t != KS_DRAGON && t != KS_WYRM && t != KS_HOARD) continue;
        for (int k = 0; k < g->world.ndragons_slain; k++)
            if (g->world.dragons_slain[k] == K->step[i].key) g->world.dragons_slain[k--] = g->world.dragons_slain[--g->world.ndragons_slain];
    }
    char f[64], text[STORY_TEXT];
    fortress(g, f, sizeof f);
    snprintf(text, sizeof text, "Word comes up from the towns: the throne of %s did not stay empty. One who served %s has taken it - an heir no one named, and %s. "
             "The way to him is new, and longer. The elders will know more.", f, was, KING_KINDS_DEF[K->kind].what);
    for (int k = 0; k < MAX_PLAYERS; k++)
        if (g->players[k].active) say(&g->players[k], text, NULL);
    game_banner_all(g, "AN HEIR ON THE THRONE - THE WAY BEGINS AGAIN");
    g->autosave_timer = 1e9f;
}

/* ---------- each frame ---------- */

void king_update(Game *g, float dt) {
    const KingWay *K = way(g);
    if (!K->valid) return;
    static float slow;
    if ((slow -= dt) <= 0) {
        slow = 0.5f;
        /* steps that are done by what happened anywhere */
        for (int i = 0; i < KING_STEPS; i++) {
            if (!step_open(g, K, i)) continue;
            const KingStep *st = &K->step[i];
            bool now = false;
            if (st->type == KS_DRAGON || st->type == KS_WYRM)
                for (int k = 0; k < g->world.ndragons_slain; k++) now |= g->world.dragons_slain[k] == st->key;
            if (st->type == KS_ALLEGIANCE) /* (an heir asks more: the town must honour you) */
                for (int k = 0; k < g->world.ntowns; k++) now |= g->world.towns[k].key == st->key && g->world.towns[k].rep >= (K->gen ? 6 : 3);
            if (st->type == KS_REALM) now = (g->world.realm_beaten >> st->arg) & 1;
            if (st->group == 2 && !K->gen && story_chapter_deed(&g->world, 8)) { /* the chronicle's end: the opener served him */
                g->world.king_flags |= KF_CHRONICLE;
                now = true;
            }
            if (now) complete(g, game_nearest_player(g, g->players[0].pl.pos, 1e9f), i);
        }
        place_things(g, K);
        /* the throne: the king waits there when the way to him is open */
        vec3 seat, centre;
        if (step_open(g, K, 9) && throne_place(g, K, &seat, &centre) && game_nearest_player(g, centre, 70.0f) && !the_king(g)) {
            Chunk *c = chunkmap_get(&g->map, floordiv(fl(seat.x), CHUNK), floordiv(fl(seat.y), CHUNK), floordiv(fl(seat.z), CHUNK));
            if (c && c->state == CH_READY) spawn_king(g, K, seat);
        }
    }
    for (int k = 0; k < MAX_MARKS; k++) {
        if (!marks[k].on) continue;
        PlayerSlot *ps = game_nearest_player(g, marks[k].pos, 1.8f);
        if (ps && !ps->st.dead) touch(g, ps, k);
    }
    /* the king on his knees */
    Monster *km = the_king(g);
    if (km) {
        king_last_pos = km->pos;
        if (falter_t > 0) {
            falter_t -= dt;
            km->state = MS_STUN;
            km->timer = fmaxf(km->timer, 0.2f);
            if (falter_t <= 0) {
                for (int k = 0; k < MAX_PLAYERS; k++) g->players[k].king_vote = 0;
                char b[96];
                snprintf(b, sizeof b, "%s RISES AGAIN", km->name);
                game_banner_all(g, b);
                km->state = MS_CHASE;
            }
        }
    }
    /* his lieutenants do not let an archer keep them at a distance: they step through the dark to you */
    static float blink_t[MAX_MONSTERS];
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || m->king != 2 || !m->awake || m->resident) continue;
        if ((blink_t[i] -= dt) > 0) continue;
        PlayerSlot *ps = game_nearest_player(g, m->pos, 40.0f);
        if (!ps || v3_len(v3_sub(ps->pl.pos, m->pos)) < 9.0f) continue;
        blink_t[i] = 7.0f;
        float a = ps->pl.yaw + PI_F * (0.5f + 0.5f * (float)(i & 1));
        vec3 to = v3_add(ps->pl.pos, v3(cosf(a) * 3.0f, 0, sinf(a) * 3.0f));
        if (!standable(g, fl(to.x), fl(to.y), fl(to.z))) continue;
        particles_burst(g, v3_add(m->pos, v3(0, 1, 0)), v3(0.3f, 0.1f, 0.4f), 18, 3.0f, 0.06f, true);
        m->pos = to;
        particles_burst(g, v3_add(m->pos, v3(0, 1, 0)), v3(0.3f, 0.1f, 0.4f), 18, 3.0f, 0.06f, true);
        sfx_at(g, SFX_SPELL_BLINK, m->pos, 0.8f);
    }
    /* an heir: some while after a king is settled, the throne does not stay empty */
    static float heir_t;
    bool credits = false;
    for (int i = 0; i < MAX_PLAYERS; i++) credits |= g->players[i].active && g->players[i].credits_t > 0;
    if (king_won(g) && !credits && g->world.king_gen < 9) {
        heir_t += dt;
        if (heir_t > KING_HEIR_DELAY) { heir_t = 0; king_heir(g); }
    } else if (!king_won(g)) heir_t = 0;
    /* the way teaches: the first time the line follows a step, how it is done */
    static u16 hinted[MAX_PLAYERS];
    static int hinted_gen = -1;
    if (hinted_gen != K->gen || marks_seed == 0) { memset(hinted, 0, sizeof hinted); hinted_gen = K->gen; }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active || ps->st.dead || ps->msg_t > 0 || king_won(g)) continue;
        int s = tracked_step(g, K, ps);
        if (s < 0 || ((hinted[i] >> s) & 1)) continue;
        hinted[i] |= (u16)(1u << s);
        const char *h = step_hint(K->step[s].type);
        if (h[0]) say(ps, h, NULL);
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (!ps->active) continue;
        if (ps->king_talk_t > 0) ps->king_talk_t -= dt;
        if (ps->credits_t > 0) {
            ps->credits_t += dt;
            bool press = ps->attack_held || ps->pad_attack || (g->touch.active && g->touch.held[TB_ATTACK]);
            if (ps->credits_t > 6.0f && press) {
                ps->credits_t = 0;
                ps->st.cooldown = 0.6f;
                game_banner(ps, "THE FORTRESS IS YOURS TO EXPLORE");
            }
        }
    }
}

/* (the test: step onto the markers near) */
void king_test_touch(Game *g, PlayerSlot *ps) {
    for (int k = 0; k < MAX_MARKS; k++)
        if (marks[k].on && v3_len(v3_sub(marks[k].pos, ps->pl.pos)) < 120.0f) { ps->pl.pos = marks[k].pos; touch(g, ps, k); }
}

/* ---------- drawing ---------- */

void king_draw(Game *g, PlayerSlot *viewer) {
    vec3 eye = game_view_eye(viewer);
    float t = g->time;
    for (int k = 0; k < MAX_MARKS; k++) {
        if (!marks[k].on || v3_len(v3_sub(marks[k].pos, eye)) > 80.0f) continue;
        vec3 p = marks[k].pos;
        vec3 gold = v3(1.3f, 1.0f, 0.45f);
        switch (marks[k].kind) {
        case MK_STELE: {
            vec3 l = game_light_at(g, v3_add(p, v3(0, 1, 0)));
            vec3 stone = v3(0.5f * fmaxf(l.x, 0.4f), 0.48f * fmaxf(l.y, 0.4f), 0.45f * fmaxf(l.z, 0.4f));
            batch3d_box(v3_add(p, v3(0, 1.3f, 0)), v3(0.5f, 1.3f, 0.18f), 0.3f, stone, false);
            for (int j = 0; j < 6; j++) batch3d_box(v3_add(p, v3(0, 0.6f + j * 0.3f, 0)), v3(0.3f, 0.04f, 0.2f), 0.3f, v3_scale(gold, 0.6f + 0.4f * sinf(t * 2 + j)), true);
            break;
        }
        case MK_RELIC: case MK_HOARD:
            batch3d_octa(v3_add(p, v3(0, 1.0f + 0.15f * sinf(t * 2.0f), 0)), 0.22f, 0.35f, t, gold, true);
            for (int j = 0; j < 8; j++) {
                float a = t + j * 0.785f;
                batch3d_box(v3_add(p, v3(cosf(a) * 0.6f, 1.0f, sinf(a) * 0.6f)), v3(0.04f, 0.04f, 0.04f), a, gold, true);
            }
            break;
        case MK_SEAL:
            for (int j = 0; j < 16; j++) {
                float a = j * 0.3927f + t * 0.4f;
                batch3d_box(v3_add(p, v3(cosf(a) * 1.6f, 0.1f + 0.05f * sinf(t * 3 + j), sinf(a) * 1.6f)), v3(0.12f, 0.1f, 0.12f), a, v3(1.3f, 0.3f, 0.2f), true);
            }
            batch3d_octa(v3_add(p, v3(0, 1.2f, 0)), 0.3f, 0.5f, t * 0.7f, v3(1.2f, 0.25f, 0.2f), true);
            break;
        case MK_FORGE:
            batch3d_octa(v3_add(p, v3(0, 0.4f + 0.1f * sinf(t * 3.0f), 0)), 0.12f, 0.2f, t, v3(1.4f, 0.7f, 0.25f), true);
            break;
        }
    }
    /* the crown on his head */
    for (int i = 0; i < MAX_MONSTERS; i++) {
        const Monster *m = &g->mons[i];
        if (!m->alive || m->king != 1 || m->type == MON_DRAGON) continue;
        float h = MONSTERS[m->type].h * m->scale;
        vec3 c = v3_add(m->pos, v3(0, h + 0.1f, 0));
        float r = 0.22f * m->scale;
        for (int j = 0; j < 8; j++) {
            float a = j * 0.785f + m->yaw;
            batch3d_box(v3_add(c, v3(cosf(a) * r, 0, sinf(a) * r)), v3(0.06f * m->scale, 0.05f * m->scale, 0.06f * m->scale), a, v3(1.2f, 0.95f, 0.35f), true);
            if (j % 2 == 0) batch3d_spike(v3_add(c, v3(cosf(a) * r, 0.05f * m->scale, sinf(a) * r)), v3(0, 1, 0), 0.03f * m->scale, 0.18f * m->scale, v3(1.2f, 0.95f, 0.35f), true);
        }
    }
}

/* The step the line at the top follows: the nearest open one with a place, else the first open. */
static int tracked_step(Game *g, const KingWay *K, const PlayerSlot *ps) {
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < KING_STEPS; i++) {
        if (!step_open(g, K, i)) continue;
        const KingStep *st = &K->step[i];
        bool placed = st->type != KS_VISION && st->type != KS_FORGE && st->type != KS_REALM;
        float d = placed ? v3_len(v3_sub(step_centre(st), ps->pl.pos)) : 5e8f;
        if (best < 0 || d < bd) { bd = d; best = i; }
    }
    return best;
}

int king_draw_tracker(Game *g, PlayerSlot *ps, int W, int H, int y) {
    const KingWay *K = way(g);
    if (!K->valid || ps->st.dead || king_won(g)) return 0;
    int best = tracked_step(g, K, ps);
    if (best < 0) return 0;
    if (!H) return 1;
    const KingStep *st = &K->step[best];
    char goal[160], buf[220], dir[64] = "";
    step_goal(g, K, best, goal, sizeof goal);
    if (st->type == KS_VISION || st->type == KS_FORGE) snprintf(dir, sizeof dir, "in the Sanctum");
    else if (st->type == KS_REALM) snprintf(dir, sizeof dir, "through the Gate");
    else story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, st->cx, st->cy, st->cz, dir, sizeof dir);
    snprintf(buf, sizeof buf, K->gen ? "* THE HEIR %d/10: %s - %s" : "* THE OLD KING %d/10: %s - %s", st->group == 0 ? 1 : st->group == 1 ? 4 : st->group == 2 ? 8 : st->group == 3 ? 9 : 10, goal, dir);
    upper(buf);
    int maxc = (W - 8) / 6;
    if ((int)strlen(buf) > maxc) buf[maxc] = 0;
    font_begin(W, H);
    game_tracker_text(ps, W, y, buf, 1.0f, 0.85f, 0.35f);
    font_end();
    return 1;
}

static int wrap(const char *s, int maxc, char lines[][100], int maxl) {
    int n = 0, len = (int)strlen(s), pos = 0;
    maxc = MINI(maxc, 99);
    while (pos < len && n < maxl) {
        while (pos < len && s[pos] == ' ') pos++;
        int take = MINI(maxc, len - pos);
        if (pos + take < len) {
            int k = take;
            while (k > 0 && s[pos + k] != ' ') k--;
            if (k > 0) take = k;
        }
        memcpy(lines[n], s + pos, (size_t)take);
        lines[n][take] = 0;
        n++;
        pos += take;
    }
    return n;
}

void king_draw_credits(Game *g, PlayerSlot *ps, int W, int H) {
    if (ps->credits_t <= 0) return;
    const KingWay *K = way(g);
    float a = fminf(0.92f, ps->credits_t * 0.5f);
    batch2d_rect(0, 0, (float)W, (float)H, 0.02f, 0.015f, 0.01f, a);
    batch2d_flush();
    static char lines[96][100];
    static float col[96];
    int n = 0;
    int maxc = MINI(90, (W - 16) / 6);
    char text[STORY_TEXT], f[64];
    fortress(g, f, sizeof f);
    snprintf(lines[n], 100, K->gen ? "THE HEIR IS NO MORE" : "THE OLD KING IS NO MORE"); col[n++] = 1.0f;
    lines[n++][0] = 0;
    const KingKind *KD = &KING_KINDS_DEF[K->kind];
    int e = g->world.king_ending;
    fill(g, K, e == KE_WORD ? KD->word_end : e == KE_ARTEFACT ? KD->art_end : KD->fight_end, text, sizeof text);
    int m = wrap(text, maxc, &lines[n], 8);
    for (int k = 0; k < m; k++) col[n + k] = 0.8f;
    n += m;
    lines[n++][0] = 0;
    snprintf(lines[n], 100, "%s, %s", K->name, KD->name); upper(lines[n]); col[n++] = 1.0f;
    snprintf(lines[n], 100, "settled by %s", e == KE_WORD ? "his true name" : e == KE_ARTEFACT ? KD->artefact : "the sword"); col[n++] = 0.7f;
    snprintf(lines[n], 100, "%s", e == KE_WORD ? "his knights are sworn to the towns, his treasury is yours" :
                                  e == KE_ARTEFACT ? "the bound are free, and every town knows your name" : "his crown, his blade and his hoard are yours"); col[n++] = 0.7f;
    lines[n++][0] = 0;
    snprintf(lines[n], 100, "THE WAY"); col[n++] = 1.0f;
    for (int i = 0; i < KING_STEPS && n < 80; i++) {
        char goal[160];
        step_goal(g, K, i, goal, sizeof goal);
        snprintf(lines[n], 100, "%d. %.88s", i + 1, goal); col[n++] = 0.7f;
    }
    lines[n++][0] = 0;
    snprintf(lines[n], 100, "%s, world %s", f, g->opt.seed_text); col[n++] = 0.7f;
    lines[n++][0] = 0;
    snprintf(lines[n], 100, "FORTRESS CRAWL"); col[n++] = 1.0f;
    snprintf(lines[n], 100, "the towns, the realms, the dragons and the king go on"); col[n++] = 0.7f;
    lines[n++][0] = 0;
    snprintf(lines[n], 100, ps->credits_t > 6.0f ? "STRIKE TO EXPLORE ON" : ""); col[n++] = 1.0f;
    /* rising slowly, then still */
    float scroll = fminf(ps->credits_t * 14.0f, (float)(H * 0.55f));
    int y0 = (int)(H * 0.85f - scroll);
    font_begin(W, H);
    for (int i = 0; i < n; i++) {
        int y = y0 + i * 10;
        if (y < 4 || y > H - 12) continue;
        font_text((W - font_text_width(lines[i])) / 2, y, lines[i], col[i], col[i] * 0.85f, col[i] * 0.5f);
    }
    font_end();
}
