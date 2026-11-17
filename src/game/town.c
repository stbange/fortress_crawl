#include "game/game.h"

#include <ctype.h>

#include "engine/font.h"
#include "render/batch.h"
#include "world/history.h"

/* The towns of the friendly peoples (towns.h, docs/TOWNS.md): dwarves, goblins, gnomes, elves,
 * humans, halflings and orcs live in halls of their own in the fortress. No monster is born in
 * a town and nothing there can be dug; guards shoot what comes in. Strike at someone face to
 * face to talk to them:
 *   the innkeeper: a bed and a meal (the night's rest heals everything, you wake there, and good
 *                  company gives more XP and faster healing for a while);
 *   the healer:    wounds, burns, poison, cold and the body's poison of elixirs, gone;
 *   the merchant and the smith: the market (the shop, with the people's own wares);
 *   the elder:     a task for the town: monsters to drive off, something to bring, someone lost
 *                  in the halls, a pest to kill. Done, it pays in gold, a gift of the people's
 *                  craft and their good opinion, which makes everything cheaper and at last free;
 *   everyone else: company: talk of their lives, their history and what is about. */

static int fl(float v) { return (int)floorf(v); }
static void upper(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }
static vec3 rgb3(const u8 c[3]) { return v3(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f); }

static const char *FOLK_ROLES[FR_COUNT] = { "innkeeper", "healer", "elder", "smith", "merchant", "guard", "townsfolk", "child", "lost" };

static void say(PlayerSlot *ps, const char *text, const char *then) {
    snprintf(ps->msg, sizeof ps->msg, "%s", text);
    snprintf(ps->msg_next, sizeof ps->msg_next, "%s", then ? then : "");
    ps->msg_t = 7.0f + (float)strlen(text) * 0.05f;
    if (g_note_hook) { g_note_hook(ps->index, "TALK", text); if (then && then[0]) g_note_hook(ps->index, "TALK", then); }
}

/* ---------- where the towns are ---------- */

bool town_chunk(const Chunk *c) {
    return c && c->info.has_struct && c->info.st.type == STRUCT_HALL && c->info.st.variant == HALL_TOWN;
}

static bool town_here(Game *g, vec3 p, Town *t) {
    Chunk *c = chunkmap_get(&g->map, floordiv(fl(p.x), CHUNK), floordiv(fl(p.y), CHUNK), floordiv(fl(p.z), CHUNK));
    if (!town_chunk(c)) return false;
    return town_from_structure(&c->info.st, t);
}

static TownRec *rec_of(Game *g, const Town *t, int level) {
    for (int i = 0; i < g->world.ntowns; i++)
        if (g->world.towns[i].key == t->key) return &g->world.towns[i];
    if (g->world.ntowns >= MAX_TOWN_RECS) { /* the oldest one with nothing to remember goes */
        int k = 0;
        for (int i = 0; i < g->world.ntowns; i++)
            if (!g->world.towns[i].qstate && g->world.towns[i].rep <= g->world.towns[k].rep) k = i;
        g->world.towns[k] = g->world.towns[--g->world.ntowns];
    }
    TownRec *r = &g->world.towns[g->world.ntowns++];
    memset(r, 0, sizeof *r);
    r->key = t->key;
    r->people = (u8)t->people;
    r->level = (u8)MINI(255, level);
    r->px = t->px; r->py = t->floor; r->pz = t->pz;
    if (g->world.king_flags & KF_FREED) r->rep = 2; /* the bound you freed (king.c) came from here too */
    return r;
}

static const char *standing(int rep) {
    return rep >= 10 ? "KIN" : rep >= 6 ? "HONOURED" : rep >= 3 ? "FRIEND" : rep >= 1 ? "GUEST" : "STRANGER";
}
/* What the market shows a guest of this standing (no discounts: the trusted see more). */
static int special_open(const TownRec *r) { return (r->rep >= 3 ? 1 : 0) | (r->rep >= 6 ? 2 : 0) | ((r->projects & TP_CARAVAN) ? 4 : 0); } /* bits: friend, honoured, caravan */

/* The works a town can be given gold for: each costs more, and each is remembered. */
static const char *WORKS[3] = { "rebuild the burned houses", "arm the guards", "send a caravan to the far halls" };
static int work_cost(const TownRec *r, int k) { static const int C[3] = { 400, 900, 1600 }, L[3] = { 20, 40, 60 }; return C[k] + L[k] * r->level; }

/* A line with the history's names and the town's. */
static void fill(Game *g, const char *tmpl, const char *town, char *out, int n) {
    const History *h = g->world.story_gen ? history_of(g->world.seed) : NULL;
    const char *F = h ? h->fortress : "the fortress", *B = h ? h->builders : "the builders", *S = h ? h->sealed : "what was sealed below",
               *N = h ? h->night : "the night it fell";
    int o = 0;
    for (const char *p = tmpl; *p && o < n - 1; p++) {
        const char *ins = NULL;
        if (p[0] == '%' && p[1]) {
            switch (p[1]) {
            case 'F': ins = F; break;
            case 'B': ins = B; break;
            case 'S': ins = S; break;
            case 'N': ins = N; break;
            case 'T': ins = town; break;
            default: break;
            }
        }
        if (ins) { for (; *ins && o < n - 1; ins++) out[o++] = *ins; p++; }
        else out[o++] = *p;
    }
    out[o] = 0;
    out[0] = (char)toupper((unsigned char)out[0]);
}

/* ---------- the people ---------- */

static Folk *folk_new(Game *g, int people, int role, vec3 pos, u64 key, int rec) {
    for (int i = 0; i < MAX_FOLK; i++) {
        Folk *f = &g->folk[i];
        if (f->alive) continue;
        memset(f, 0, sizeof *f);
        f->alive = true;
        f->people = (u8)people;
        f->role = (u8)role;
        f->pos = f->home = f->goal = pos;
        f->key = key;
        f->rec = rec;
        f->look = (u8)(mix64(key) & 0xFF);
        f->yaw = (float)(mix64(key ^ 7) % 628) / 100.0f;
        f->timer = 1.0f + (float)(mix64(key ^ 9) % 300) / 100.0f;
        people_name(people, key, f->name, sizeof f->name);
        return f;
    }
    return NULL;
}

static bool standable(Game *g, int x, int y, int z) {
    return game_solid(g, x, y - 1, z) == 1 && game_solid(g, x, y, z) == 0 && game_solid(g, x, y + 1, z) == 0;
}

void town_spawn_chunk(Game *g, Chunk *c) {
    if (!town_chunk(c)) return;
    Town t;
    if (!town_from_structure(&c->info.st, &t)) return;
    /* the people come with the plaza's chunk */
    if (c->cx != floordiv(t.px, CHUNK) || c->cz != floordiv(t.pz, CHUNK)) return;
    TownRec *r = rec_of(g, &t, c->info.level);
    int ri = (int)(r - g->world.towns);
    for (int i = 0; i < MAX_FOLK; i++)
        if (g->folk[i].alive && g->folk[i].rec == ri && g->folk[i].role != FR_LOST) return; /* already about */
    float y = (float)t.floor;
    int n = 0;
    for (int i = 0; i < TOWN_LOTS; i++) {
        const TownLot *L = &t.lot[i];
        int role = L->role == TB_INN ? FR_INNKEEPER : L->role == TB_TEMPLE ? FR_HEALER : L->role == TB_ELDER ? FR_ELDER : L->role == TB_WORKSHOP ? FR_SMITH : -1;
        if (role >= 0) folk_new(g, t.people, role, v3(L->spot_x + 0.5f, y, L->spot_z + 0.5f), mix64(t.key ^ (u64)(i + 1) * 0x51ED), ri);
        if (L->role == TB_HOUSE) /* a family at home */
            for (int k = 0; k < 2; k++)
                folk_new(g, t.people, k ? FR_CHILD : FR_FOLK, v3((L->x0 + L->x1) * 0.5f + k, y, (L->z0 + L->z1) * 0.5f), mix64(t.key ^ (u64)(i * 16 + k + 100)), ri);
    }
    for (int k = 0; k < 2; k++) /* merchants at the stalls */
        folk_new(g, t.people, FR_MERCHANT, v3(t.stall_x[k] + 0.5f, y, t.stall_z[k] + 1.5f), mix64(t.key ^ (u64)(200 + k)), ri);
    int nguards = 3 + ((r->projects & TP_ARMS) ? 2 : 0) + ((g->world.king_flags & KF_SWORN) ? 2 : 0);
    for (int k = 0; k < nguards; k++) { /* guards on the road round the walls (with arms bought, and the Old King's sworn knights) */
        Folk *f = folk_new(g, t.people, FR_GUARD, v3(t.x0 + 4.5f + (k % 3) * 30 + (k / 3) * 8, y, t.z0 + 2.5f), mix64(t.key ^ (u64)(300 + k)), ri);
        if (f) f->state = (u8)(k % 3);
    }
    if (r->projects & TP_HOUSE) /* the houses rebuilt: a new family */
        for (int k = 0; k < 3; k++) folk_new(g, t.people, k ? FR_CHILD : FR_FOLK, v3(t.px - 4.5f + k, y, t.pz + 6.5f), mix64(t.key ^ (u64)(500 + k)), ri);
    for (int k = 0; k < 5; k++, n++) /* folk about the plaza */
        folk_new(g, t.people, k < 4 ? FR_FOLK : FR_CHILD, v3(t.px + 5.5f - k * 2.5f, y, t.pz - 6.5f + (k & 1) * 12), mix64(t.key ^ (u64)(400 + k)), ri);
}

/* ---------- tasks ---------- */

static const char *need_name(int item) {
    return item >= 1000 ? REAGENTS[item - 1000].name : FOODS[item].name;
}
static int need_have(const Stats *s, int item) { return item >= 1000 ? s->reag[item - 1000] : s->food[item]; }
static void need_take(Stats *s, int item, int n) {
    u8 *v = item >= 1000 ? &s->reag[item - 1000] : &s->food[item];
    *v = (u8)MAXI(0, *v - n);
}

static void ask(Game *g, TownRec *r, const char *town, char *line, int n) {
    const PeopleDef *P = &PEOPLES[r->people];
    u64 h = mix64(r->key ^ (u64)r->done * 0x9E37);
    r->qtype = (u8)(TQ_HUNT + (int)((h + r->done) % 4));
    r->qstate = 1;
    r->got = 0;
    r->qseq = ++g->world.town_seq;
    int cx = floordiv(r->px, CHUNK), cy = floordiv(r->py, CHUNK), cz = floordiv(r->pz, CHUNK);
    switch (r->qtype) {
    case TQ_HUNT:
        r->need = (u8)(5 + (h >> 8) % 4);
        snprintf(line, (size_t)n, "The beasts press at our doors. Drive them off: kill %d of them in the halls round %s, and we will not forget it.", r->need, town);
        break;
    case TQ_FETCH:
        r->item = P->need[(h >> 12) % 3];
        r->need = (u8)(2 + (h >> 16) % 3);
        snprintf(line, (size_t)n, "Our stores run low. Bring us %d %s and you will have our thanks, and more than thanks.", r->need, need_name(r->item));
        break;
    case TQ_LOST: case TQ_PEST: {
        /* somewhere a few chunks out, on the town's layer */
        int dx = (int)((h >> 20) % 3) + 3, dz = (int)((h >> 24) % 7) - 3;
        if ((h >> 28) & 1) dx = -dx;
        if ((h >> 29) & 1) { int tmp = dx; dx = dz; dz = tmp; }
        r->tx = (i16)(cx + dx); r->ty = (i16)cy; r->tz = (i16)(cz + dz);
        r->need = 1;
        char dir[64], who[32];
        story_direction((float)r->px, (float)r->py + 1.0f, (float)r->pz, r->tx, r->ty, r->tz, dir, sizeof dir);
        people_name(r->people, r->key ^ (u64)r->done * 0x1057, who, sizeof who);
        if (r->qtype == TQ_LOST)
            snprintf(line, (size_t)n, "%s went out to the halls %s three days ago and has not come back. Find them, bring them home.", who, dir);
        else snprintf(line, (size_t)n, "A great beast has made its lair %s. It takes our people one by one. Kill it.", dir);
        break;
    }
    }
}

/* What a people gives: their own craft (a task's reward, a kin's gift). */
static void town_gift(Game *g, PlayerSlot *ps, TownRec *r, int salt, char *gift, int n) {
    Stats *s = &ps->st;
    vec3 at = v3_add(ps->pl.pos, v3(0, 0.6f, 0));
    switch (r->people) {
    case PE_DWARF: forge_give(g, ps, FG_STEEL, 1); snprintf(gift, (size_t)n, "an ingot of dwarven steel"); break;
    case PE_GNOME: for (int k = 0; k < 3; k++) s->flasks[k] = (u8)MINI(30, s->flasks[k] + 2); snprintf(gift, (size_t)n, "a crate of gnomish flasks"); break;
    case PE_ELF: alchemy_give(g, ps, RG_ORCHID, 1); pickup_spawn(g, PK_POTION_MP, at, 1, NULL, 0, 0); snprintf(gift, (size_t)n, "an elven orchid and a draught"); break;
    case PE_GOBLIN: snprintf(gift, (size_t)n, "a heavy purse (goblins pay well)"); break;
    case PE_HUMAN: for (int k = 0; k < 2; k++) pickup_spawn(g, PK_POTION_HP, at, 1, NULL, 0, 0); snprintf(gift, (size_t)n, "two healing potions"); break;
    case PE_HALFLING: food_give(g, ps, FD_HONEY, 2); food_give(g, ps, FD_STARFRUIT, 1); s->seeds = (u8)MINI(99, s->seeds + 5); snprintf(gift, (size_t)n, "a hamper of honey, starfruit and seeds"); break;
    default: {
        int tier, region = world_region(&g->world, floordiv(r->px, CHUNK), floordiv(r->py, CHUNK), floordiv(r->pz, CHUNK), &tier);
        Item it = item_roll_region(mix64(r->key ^ (u64)salt * 0xA77), (float)r->level + 2.0f, RAR_MAGIC, tier, region);
        pickup_spawn(g, PK_WEAPON, at, 0, &it, 0, 0);
        snprintf(gift, (size_t)n, "a weapon from the clan's own forge");
        break;
    }
    }
    (void)s;
}

static void reward(Game *g, PlayerSlot *ps, TownRec *r, const char *town) {
    Stats *s = &ps->st;
    int gold = 40 + 12 * r->level + (r->qtype >= TQ_LOST ? 40 : 0);
    if (r->people == PE_GOBLIN) gold = gold * 3 / 2; /* goblins pay in coin */
    s->gold += gold;
    int before = r->rep;
    r->rep = (i8)MINI(100, r->rep + (r->qtype >= TQ_LOST ? 2 : 1));
    r->done++;
    r->qstate = 0;
    char gift[96] = "";
    town_gift(g, ps, r, r->done, gift, sizeof gift);
    char line[STORY_TEXT], then[160];
    snprintf(line, sizeof line, "It is done, and %s will remember it. Take %d gold, and %s.", town, gold, gift);
    if (standing(before) != standing(r->rep)) snprintf(then, sizeof then, "The %s of %s call you %s now.", PEOPLES[r->people].plural, town, standing(r->rep));
    else then[0] = 0;
    say(ps, line, then);
    char b[96];
    snprintf(b, sizeof b, "+%d GOLD - %s", gold, standing(r->rep));
    game_banner(ps, b);
    sfx_at(g, SFX_CHAPTER_DONE, ps->pl.pos, 0.8f);
    g_tele.town_tasks++;
    g->autosave_timer = 1e9f;
}

/* Crouched before the elder: give gold to the town's next work. Each is remembered and seen. */
static void works(Game *g, PlayerSlot *ps, TownRec *r, const char *town) {
    int k = 0;
    while (k < 3 && (r->projects >> k) & 1) k++;
    if (k == 3) { say(ps, "You have done more for us than any. There is nothing left to build that your gold could buy - only your company.", NULL); return; }
    int cost = work_cost(r, k);
    char line[STORY_TEXT], b[96];
    if (ps->st.gold < cost) {
        snprintf(line, sizeof line, "We mean to %s, if we had the coin: %d gold. %s would not forget who gave it.", WORKS[k], cost, town);
        say(ps, line, NULL);
        snprintf(b, sizeof b, "THE TOWN'S WORKS: %d GOLD TO %s", cost, WORKS[k]);
        upper(b);
        game_banner(ps, b);
        return;
    }
    ps->st.gold -= cost;
    r->projects |= (u8)(1 << k);
    int before = r->rep;
    r->rep = (i8)MINI(100, r->rep + 2);
    static const char *DONE[3] = { "The burned houses of %s are built again, and a family moves into the first of them.",
                                   "The guards of %s have new arms, and two more stand the walls.",
                                   "The caravan sets out from %s for the far halls. Its goods will be in the market when it is back - for you first." };
    snprintf(line, sizeof line, DONE[k], town);
    char then[160] = "";
    if (standing(before) != standing(r->rep)) snprintf(then, sizeof then, "The %s of %s call you %s now.", PEOPLES[r->people].plural, town, standing(r->rep));
    say(ps, line, then);
    snprintf(b, sizeof b, "-%d GOLD - %s", cost, standing(r->rep));
    game_banner(ps, b);
    sfx_at(g, SFX_CHAPTER_DONE, ps->pl.pos, 0.6f);
    g_tele.town_works++;
    g->autosave_timer = 1e9f;
}

static void elder(Game *g, PlayerSlot *ps, Folk *f, TownRec *r, const char *town) {
    Stats *s = &ps->st;
    const PeopleDef *P = &PEOPLES[r->people];
    if (ps->pl.stance != STANCE_STAND) { works(g, ps, r, town); return; }
    if (!r->qstate) {
        char greet[STORY_TEXT], task[STORY_TEXT];
        fill(g, P->greet, town, greet, sizeof greet);
        ask(g, r, town, task, sizeof task);
        if (r->done == 0) { /* the first time: a welcome, then the task, and the town's works */
            char t2[STORY_TEXT];
            snprintf(t2, sizeof t2, "%s (Crouch and speak to me to give to the town's works.)", task);
            say(ps, greet, t2);
        }
        else say(ps, task, NULL);
        return;
    }
    char line[STORY_TEXT];
    switch (r->qtype) {
    case TQ_HUNT:
        if (r->got >= r->need) { reward(g, ps, r, town); return; }
        snprintf(line, sizeof line, "%d of %d beasts so far. The halls round %s are still not safe.", r->got, r->need, town);
        break;
    case TQ_FETCH: {
        int have = need_have(s, r->item);
        if (have >= r->need) { need_take(s, r->item, r->need); reward(g, ps, r, town); return; }
        snprintf(line, sizeof line, "We still need %d %s. You carry %d.", r->need, need_name(r->item), have);
        break;
    }
    default: {
        if (r->got) { reward(g, ps, r, town); return; }
        char dir[64];
        story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, r->tx, r->ty, r->tz, dir, sizeof dir);
        snprintf(line, sizeof line, r->qtype == TQ_LOST ? "Have you found them? They were last seen %s." : "The beast still lives, %s. We hear it at night.", dir);
        break;
    }
    }
    say(ps, line, NULL);
    (void)f;
}

static void innkeeper(Game *g, PlayerSlot *ps, Folk *f, TownRec *r, const char *town) {
    Stats *s = &ps->st;
    int price = r->rep >= 6 ? 0 : 15 + 2 * r->level;
    if (s->gold < price) { say(ps, "A bed and a meal, and a seat by the fire. When you have the coin.", NULL); char b[64]; snprintf(b, sizeof b, "A NIGHT AT THE INN: %d GOLD", price); game_banner(ps, b); return; }
    s->gold -= price;
    s->hp = s->maxhp;
    s->mana = s->maxmana;
    s->burn_t = s->poison_t = s->chill_t = 0;
    s->cold = 0;
    s->well_fed_t = fmaxf(s->well_fed_t, 900.0f);
    stats_refresh(ps);
    s->company_t = 900.0f;
    s->respawn = f->home;
    s->respawn_yaw = f->yaw;
    g->autosave_timer = 1e9f;
    /* the company: a tale by the fire */
    const History *h = g->world.story_gen ? history_of(g->world.seed) : NULL;
    char line[STORY_TEXT], tale[STORY_TEXT];
    if (h && h->nfacts > 0) {
        char fact[STORY_TEXT];
        history_fact_line(h, (int)(mix64(f->key ^ (u64)(g->play_time / 60.0f)) % (u64)h->nfacts), NULL, fact, sizeof fact);
        snprintf(tale, sizeof tale, "By the fire an old %s tells a tale: '%.300s'", PEOPLES[r->people].name, fact);
    } else snprintf(tale, sizeof tale, "By the fire they sing the songs of %s until late.", town);
    snprintf(line, sizeof line, "You eat, you sleep, you wake rested among friends%s. You will wake here.", price ? "" : " (on the house)");
    say(ps, line, tale);
    game_banner(ps, "GOOD COMPANY: MORE XP AND FASTER HEALING FOR 15 MINUTES - THE GAME IS SAVED");
    sfx_at(g, SFX_REALM_REST, ps->pl.pos, 1.0f);
    g_tele.town_rests++;
}

static void healer(Game *g, PlayerSlot *ps, Folk *f, TownRec *r) {
    Stats *s = &ps->st;
    int price = r->rep >= 6 ? 0 : r->rep >= 3 ? (10 + r->level) / 2 : 10 + r->level;
    bool hurt = s->hp < s->maxhp - 0.5f || s->burn_t > 0 || s->poison_t > 0 || s->chill_t > 0 || s->cold > 0.05f || s->tox_t > 0;
    if (!hurt) { say(ps, "You are whole. Go carefully, and come back so.", NULL); return; }
    if (s->gold < price) { char b[64]; snprintf(b, sizeof b, "HEALING: %d GOLD", price); game_banner(ps, b); return; }
    s->gold -= price;
    s->hp = s->maxhp;
    s->burn_t = s->poison_t = s->chill_t = 0;
    s->cold = 0;
    s->tox_t = 0;
    if (r->rep >= 6) { s->ward = fmaxf(s->ward, s->maxhp * 0.25f); s->ward_t = fmaxf(s->ward_t, 300.0f); }
    particles_burst(g, player_eye(&ps->pl), v3(0.9f, 1.0f, 0.7f), 24, 2.0f, 0.06f, true);
    sfx_at(g, SFX_REALM_BATH, ps->pl.pos, 1.0f);
    say(ps, price ? "There. Hold still. ... There." : "There: no charge for one of ours.", r->rep >= 6 ? "And a blessing to keep the next blow off." : NULL);
    g_tele.town_heals++;
    (void)f;
}

static void chatter(Game *g, PlayerSlot *ps, Folk *f, TownRec *r, const char *town) {
    const PeopleDef *P = &PEOPLES[r->people];
    char line[STORY_TEXT];
    u64 h = mix64(f->key ^ (u64)(g->play_time / 20.0f));
    int k = (int)(h % 8);
    if (f->role == FR_GUARD && k < 3) snprintf(line, sizeof line, "%s", r->rep >= 6 ? "The town is glad of you. Go safely." : "Keep your blade sheathed in %s, and we'll get on.");
    else if (k == 6 && ps->st.motive && ps->target.name[0]) {
        char dir[64];
        story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, ps->target.cx, ps->target.cy, ps->target.cz, dir, sizeof dir);
        snprintf(line, sizeof line, "Looking for %s? Someone saw them %s, not long ago.", ps->target.name, dir);
    } else if (k == 7 && r->qstate) snprintf(line, sizeof line, "Have you spoken to the elder? We need help, and you look like help.");
    else if (king_won(g) && k < 5) {
        static const char *J[5] = { "Did you hear? The king on the throne is no more! They say it was you. Drink with us!",
                                    "Lanterns all night, and nobody's working tomorrow. The old dread is gone.",
                                    "My mother never thought she'd see it: the Old King settled. Thank you.",
                                    "We'll sing about this for a hundred years. Badly, but we'll sing it.",
                                    "The children want to know if the throne was gold. Was it?" };
        snprintf(line, sizeof line, "%s", J[k]);
    }
    else fill(g, P->chatter[k % 6], town, line, sizeof line);
    if (strstr(line, "%s")) { char tmp[STORY_TEXT]; snprintf(tmp, sizeof tmp, line, town); snprintf(line, sizeof line, "%s", tmp); }
    char who[96];
    snprintf(who, sizeof who, "%s, %s %s: ", f->name, f->role == FR_CHILD ? "a" : "the", f->role == FR_CHILD ? "child" : FOLK_ROLES[f->role]);
    char full[STORY_TEXT];
    snprintf(full, sizeof full, "%s%s", who, line);
    say(ps, full, NULL);
}

static bool kin_talk(Game *g, PlayerSlot *ps, Folk *f);

bool town_use(Game *g, PlayerSlot *ps) {
    vec3 fw = v3(cosf(ps->pl.yaw), 0, sinf(ps->pl.yaw));
    Folk *best = NULL;
    float bd = 2.6f;
    for (int i = 0; i < MAX_FOLK; i++) {
        Folk *f = &g->folk[i];
        if (!f->alive) continue;
        vec3 dv = v3_sub(f->pos, ps->pl.pos);
        if (fabsf(dv.y) > 2.0f) continue;
        dv.y = 0;
        float d = v3_len(dv);
        if (d > bd || (d > 0.5f && v3_dot(v3_scale(dv, 1.0f / d), fw) < 0.5f)) continue;
        bd = d;
        best = f;
    }
    if (best && best->rec <= -60 && best->rec > -100) return kin_talk(g, ps, best); /* one of your kin in the Sanctum */
    if (best && best->rec < 0) return king_folk_talk(g, ps, best); /* the Old King's witness, or allies (king.c) */
    if (!best || best->rec >= g->world.ntowns) return false;
    Folk *f = best;
    TownRec *r = &g->world.towns[f->rec];
    f->talk_t = 6.0f;
    f->yaw = atan2f(ps->pl.pos.z - f->pos.z, ps->pl.pos.x - f->pos.x);
    Town t;
    char town[64];
    if (town_here(g, f->home, &t)) snprintf(town, sizeof town, "%s", t.name);
    else snprintf(town, sizeof town, "the town");
    if (r->rep == 0 && f->role != FR_LOST) r->rep = 1; /* you came to us: a guest */
    if (king_town_talk(g, ps, f, r->key)) return true; /* the Old King's way: the herald, the brew (king.c) */
    switch (f->role) {
    case FR_ELDER: elder(g, ps, f, r, town); break;
    case FR_INNKEEPER: innkeeper(g, ps, f, r, town); break;
    case FR_HEALER: healer(g, ps, f, r); break;
    case FR_MERCHANT: case FR_SMITH:
        ps->show_skills = true;
        ps->menu_tab = TAB_SHOP;
        ps->shop_sel = 0;
        say(ps, f->role == FR_SMITH ? "Metal, flux, a blade if you've the coin. Have a look." : "Have a look, have a look. Best prices this side of the Gate.", NULL);
        break;
    case FR_LOST:
        if (r->qstate && r->qtype == TQ_LOST && !r->got) {
            r->got = 1;
            say(ps, "You came for me? Oh, thank the stone. I know the way home from here. Tell the elder I'm coming!", NULL);
            particles_burst(g, v3_add(f->pos, v3(0, 1, 0)), v3(1.0f, 0.9f, 0.6f), 20, 2.0f, 0.05f, true);
            f->alive = false;
            game_banner(ps, "FOUND - TELL THE ELDER");
        }
        break;
    default: chatter(g, ps, f, r, town); break;
    }
    return true;
}

/* ---------- kin: one of a town that calls you kin lives in the Sanctum ---------- */

static bool kin_talk(Game *g, PlayerSlot *ps, Folk *f) {
    int ri = -60 - f->rec;
    if (ri < 0 || ri >= g->world.ntowns) return false;
    TownRec *r = &g->world.towns[ri];
    f->talk_t = 5.0f;
    f->yaw = atan2f(ps->pl.pos.z - f->pos.z, ps->pl.pos.x - f->pos.x);
    u64 day = mix64(r->key ^ 0x6111F7ULL ^ ((u64)(g->play_time / 1200.0f) << 4) ^ ((u64)ps->index << 40)) | 1;
    char line[STORY_TEXT];
    if (looted_has(g, day)) {
        snprintf(line, sizeof line, "%s, of the %s: 'It is quiet up here, and the view! Come back tomorrow - I will have something for you.'", f->name, PEOPLES[r->people].plural);
        say(ps, line, NULL);
        return true;
    }
    looted_mark(g, day);
    char gift[96];
    town_gift(g, ps, r, (int)(g->play_time / 1200.0f) + 7, gift, sizeof gift);
    snprintf(line, sizeof line, "%s, of the %s, kin to you now: 'From home - they send it with their love: %s.'", f->name, PEOPLES[r->people].plural, gift);
    say(ps, line, NULL);
    sfx_at(g, SFX_CHAPTER_DONE, ps->pl.pos, 0.4f);
    return true;
}

static void kin_update(Game *g) {
    bool home = false;
    for (int i = 0; i < MAX_PLAYERS; i++) home |= g->players[i].active && realm_has(g->players[i].pl.pos);
    if (!home) return;
    int slot = 0;
    for (int i = 0; i < g->world.ntowns && slot < 4; i++) {
        TownRec *r = &g->world.towns[i];
        if (r->rep < 10) continue;
        int s = slot++;
        bool there = false;
        for (int k = 0; k < MAX_FOLK && !there; k++) there = g->folk[k].alive && g->folk[k].rec == -60 - i;
        if (there) continue;
        for (int t = 0; t < 12; t++) {
            int x = (int)POCKET_ARRIVE_X + 4 + s * 2 + (t % 3), z = (int)POCKET_ARRIVE_Z - 4 - t / 3;
            if (game_solid(g, x, POCKET_G - 1, z) != 1 || game_solid(g, x, POCKET_G, z) || game_solid(g, x, POCKET_G + 1, z)) continue;
            Folk *f = folk_new(g, r->people, FR_FOLK, v3(x + 0.5f, (float)POCKET_G, z + 0.5f), mix64(r->key ^ 0x6111ULL), -60 - i);
            if (f) f->timer = 1e9f;
            break;
        }
    }
}

/* ---------- raids: the Old King's servants come for the towns (king.c) ---------- */

static struct { int rec; float cd; bool on; } raid = { -1, 300.0f, false };

static void raid_update(Game *g, float dt) {
    if (raid.on) {
        TownRec *r = raid.rec >= 0 && raid.rec < g->world.ntowns ? &g->world.towns[raid.rec] : NULL;
        int left = 0;
        for (int i = 0; i < MAX_MONSTERS; i++) left += g->mons[i].alive && g->mons[i].king == 4;
        PlayerSlot *ps = r ? game_nearest_player(g, v3((float)r->px, (float)r->py, (float)r->pz), 160.0f) : NULL;
        if (!r || !ps) { /* you left them to it: the guards hold, but no one forgets */
            for (int i = 0; i < MAX_MONSTERS; i++) if (g->mons[i].alive && g->mons[i].king == 4) g->mons[i].alive = false;
            raid.on = false;
            return;
        }
        if (left == 0) {
            raid.on = false;
            r->rep = (i8)MINI(100, r->rep + 1);
            Town t;
            char town[64] = "the town";
            if (town_here(g, v3((float)r->px + 0.5f, (float)r->py, (float)r->pz + 0.5f), &t)) snprintf(town, sizeof town, "%s", t.name);
            char line[STORY_TEXT];
            snprintf(line, sizeof line, "The raiders are dead or gone, and %s stands. The %s will not forget who stood with them.", town, PEOPLES[r->people].plural);
            for (int k = 0; k < MAX_PLAYERS; k++) if (g->players[k].active) say(&g->players[k], line, NULL);
            game_banner_all(g, "THE RAID IS DRIVEN OFF - THE TOWN IS GRATEFUL");
            g_tele.town_raids++;
            g->autosave_timer = 1e9f;
        }
        return;
    }
    if ((raid.cd -= dt) > 0 || !king_raids_on(g)) return;
    /* a town with you in it */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        Town t;
        if (!ps->active || ps->st.dead || !town_here(g, ps->pl.pos, &t)) continue;
        int ri = -1;
        for (int k = 0; k < g->world.ntowns; k++) if (g->world.towns[k].key == t.key) ri = k;
        if (ri < 0) continue;
        TownRec *r = &g->world.towns[ri];
        /* they come in by the gate side of the plaza */
        vec3 at = v3((float)t.px + 0.5f, (float)t.floor, (float)t.pz - 11.5f);
        if (!standable(g, fl(at.x), fl(at.y), fl(at.z))) at = v3((float)t.px + 0.5f, (float)t.floor, (float)t.pz + 0.5f);
        int n = king_raid(g, at, (int)r->level + 1, 4 + r->level / 6, mix64(t.key ^ 0x7A1D));
        if (!n) return;
        raid.on = true;
        raid.rec = ri;
        raid.cd = 900.0f;
        char line[STORY_TEXT];
        snprintf(line, sizeof line, "Horns at the gate of %s: the Old King's servants are in the town! The guards cannot hold them alone.", t.name);
        for (int k = 0; k < MAX_PLAYERS; k++) if (g->players[k].active) say(&g->players[k], line, NULL);
        game_banner_all(g, "A RAID ON THE TOWN - DRIVE THEM OFF");
        sfx_at(g, SFX_NAMED_DEATH, at, 0.8f);
        return;
    }
}

/* ---------- after the Old King: the towns celebrate ---------- */

static void celebrate(Game *g, float dt) {
    static float t;
    if (!king_won(g) || (t -= dt) > 0) return;
    t = 0.7f;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        Town town;
        if (!ps->active || !town_here(g, ps->pl.pos, &town)) continue;
        u64 h = mix64((u64)(g->time * 1000.0f));
        vec3 at = v3(town.px + (float)(h % 24) - 12.0f, (float)town.floor + 9.0f + (float)((h >> 8) % 5), town.pz + (float)((h >> 16) % 24) - 12.0f);
        static const float C[5][3] = { { 1.4f, 0.5f, 0.3f }, { 0.4f, 1.2f, 0.5f }, { 0.5f, 0.6f, 1.4f }, { 1.4f, 1.2f, 0.4f }, { 1.2f, 0.5f, 1.2f } };
        const float *c = C[(h >> 24) % 5];
        particles_burst(g, at, v3(c[0], c[1], c[2]), 40, 6.0f, 0.06f, true);
        flash_add(g, at, v3(c[0], c[1], c[2]), 12.0f, 0.4f);
        sfx_at(g, SFX_SPELL_BLINK, at, 0.25f);
        break;
    }
}

/* ---------- monsters killed for a town ---------- */

void town_on_kill(Game *g, Monster *m) {
    for (int i = 0; i < g->world.ntowns; i++) {
        TownRec *r = &g->world.towns[i];
        if (r->qstate != 1) continue;
        if (r->qtype == TQ_PEST && m->townq == i + 1) { r->got = 1; game_banner_all(g, "THE BEAST IS DEAD - TELL THE ELDER"); continue; }
        if (r->qtype != TQ_HUNT || r->got >= r->need) continue;
        float dx = m->pos.x - r->px, dz = m->pos.z - r->pz, dy = m->pos.y - r->py;
        if (dx * dx + dz * dz < 170.0f * 170.0f && fabsf(dy) < 50.0f) {
            r->got++;
            char b[64];
            snprintf(b, sizeof b, r->got >= r->need ? "THE TOWN IS SAFER - TELL THE ELDER" : "FOR THE TOWN: %d OF %d", r->got, r->need);
            game_banner_all(g, b);
        }
    }
}

/* ---------- the market ---------- */

int town_market(Game *g, PlayerSlot *ps, int *special) {
    Town t;
    if (!town_here(g, ps->pl.pos, &t)) return -1;
    if (special) {
        *special = 0;
        for (int i = 0; i < g->world.ntowns; i++)
            if (g->world.towns[i].key == t.key) *special = special_open(&g->world.towns[i]);
    }
    return t.people;
}

/* The key that marks today's special ware of this town bought. */
u64 town_special_key(Game *g, PlayerSlot *ps, int k) {
    Town t;
    if (!town_here(g, ps->pl.pos, &t)) return 0;
    return mix64(t.key ^ 0x5BEC1A1ULL ^ ((u64)(g->play_time / 1200.0f) << 4) ^ (u64)k ^ ((u64)ps->index << 40)) | 1;
}

bool town_buy(Game *g, PlayerSlot *ps, int ware) {
    int open;
    int people = town_market(g, ps, &open);
    if (people < 0 || ware < 0 || ware >= 8 || (ware >= 5 && !((open >> (ware - 5)) & 1))) return false;
    const Ware *w = ware < 5 ? &PEOPLES[people].wares[ware] : &PEOPLE_SPECIAL[people][ware - 5];
    if (ware >= 5) {
        u64 k = town_special_key(g, ps, ware);
        if (looted_has(g, k)) return false;
        looted_mark(g, k);
    }
    Stats *s = &ps->st;
    vec3 at = v3_add(ps->pl.pos, v3(0, 0.6f, 0));
    switch (w->kind) {
    case WA_FOOD: food_give(g, ps, w->id, w->n); break;
    case WA_REAGENT: alchemy_give(g, ps, w->id, w->n); break;
    case WA_FORGE: forge_give(g, ps, w->id, w->n); break;
    case WA_POTION: pickup_spawn(g, w->id ? PK_POTION_MP : PK_POTION_HP, at, 1, NULL, 0, 0); break;
    case WA_SUPPLY:
        if (w->id == 100) s->bombs += w->n;
        else if (w->id >= 104 && w->id <= 106) s->flasks[w->id - 104] = (u8)MINI(30, s->flasks[w->id - 104] + w->n);
        else if (w->id == 107) s->firewood = (u8)MINI(20, s->firewood + w->n);
        else if (w->id == 108) s->seeds = (u8)MINI(99, s->seeds + w->n);
        else if (w->id == 103) { s->warm_t = 3600.0f; s->cold = 0; }
        break;
    case WA_WEAPON: {
        int tier, region = world_region(&g->world, floordiv(fl(ps->pl.pos.x), CHUNK), floordiv(fl(ps->pl.pos.y), CHUNK), floordiv(fl(ps->pl.pos.z), CHUNK), &tier);
        Item it = item_roll_region(mix64((u64)(g->play_time * 1000.0f) ^ (u64)ps->index), (float)MAXI(3, s->level) + 2.0f, w->id == 1 ? RAR_EPIC : RAR_RARE, tier, region);
        pickup_spawn(g, PK_WEAPON, at, 0, &it, 0, 0);
        break;
    }
    case WA_FORMULA: pickup_spawn(g, PK_FORMULA, at, -1, NULL, 0, 0); break;
    case WA_KEYSTONE: pickup_spawn(g, PK_KEYSTONE, at, 1, NULL, 0, 0); break;
    }
    g_tele.town_trades++;
    return true;
}

/* ---------- living ---------- */

static void wander_goal(Game *g, Folk *f, const Town *t) {
    u64 h = mix64(f->key ^ (u64)(g->time * 3.0f));
    switch (f->role) {
    case FR_GUARD: { /* round the road by the walls */
        f->state = (u8)((f->state + 1) & 3);
        int k = f->state;
        f->goal = v3((k == 0 || k == 3 ? t->x0 + 4.5f : t->x1 - 3.5f), (float)t->floor, (k < 2 ? t->z0 + 3.5f : t->z1 - 2.5f));
        break;
    }
    case FR_FOLK: case FR_CHILD: {
        float r = f->role == FR_CHILD ? 9.0f : 16.0f;
        if ((h & 3) == 0) f->goal = f->home; /* home for a while */
        else f->goal = v3(t->px + 0.5f + ((float)(h % 200) / 100.0f - 1.0f) * r, (float)t->floor, t->pz + 0.5f + ((float)((h >> 8) % 200) / 100.0f - 1.0f) * r);
        break;
    }
    default: /* keepers stay by their place */
        f->goal = v3_add(f->home, v3(((float)(h % 100) / 100.0f - 0.5f) * 1.5f, 0, ((float)((h >> 8) % 100) / 100.0f - 0.5f) * 1.5f));
        break;
    }
}

static void guard_shoots(Game *g, Folk *f) {
    Monster *best = NULL;
    float bd = 20.0f;
    vec3 eye = v3_add(f->pos, v3(0, 1.5f, 0));
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &g->mons[i];
        if (!m->alive || m->resident || m->charm_t > 0 || (f->rec == -50 && m->king == 0 && v3_len(v3_sub(m->pos, f->pos)) > 14.0f)) continue;
        float d = v3_len(v3_sub(m->pos, f->pos));
        if (d < bd && phys_line_of_sight(eye, v3_add(m->pos, v3(0, MONSTERS[m->type].h * m->scale * 0.5f, 0)), game_solid, g)) { bd = d; best = m; }
    }
    if (!best) return;
    vec3 c = v3_add(best->pos, v3(0, MONSTERS[best->type].h * best->scale * 0.5f, 0)), dir = v3_sub(c, eye);
    float len = v3_len(dir);
    dir = v3_scale(dir, 1.0f / fmaxf(len, 0.01f));
    for (float t = 0.5f; t < len; t += 1.0f) particles_burst(g, v3_add(eye, v3_scale(dir, t)), v3(0.9f, 0.85f, 0.6f), 1, 0.2f, 0.03f, true);
    f->yaw = atan2f(dir.z, dir.x);
    float level = 0;
    if (f->rec >= 0 && f->rec < g->world.ntowns) level = g->world.towns[f->rec].level;
    if (f->rec == -50) level = (float)g->players[0].st.level; /* allies fight as well as you do */
    monster_damage(g, best, 12.0f + level * 3.0f, f->pos, 2.0f, -1);
    sfx_at(g, SFX_SHOOT_SHORTBOW, f->pos, 0.6f);
}

/* Someone lost in the halls, or a pest in its lair: there when you come near the place. */
static void quest_spawns(Game *g) {
    for (int i = 0; i < g->world.ntowns; i++) {
        TownRec *r = &g->world.towns[i];
        if (r->qstate != 1 || r->got || (r->qtype != TQ_LOST && r->qtype != TQ_PEST)) continue;
        vec3 c = v3(r->tx * CHUNK + 16.0f, r->ty * CHUNK + 16.0f, r->tz * CHUNK + 16.0f);
        if (!game_nearest_player(g, c, 60.0f)) continue;
        Chunk *ch = chunkmap_get(&g->map, r->tx, r->ty, r->tz);
        if (!ch || ch->state != CH_READY) continue;
        bool there = false;
        for (int k = 0; k < MAX_FOLK && !there; k++) there = g->folk[k].alive && g->folk[k].role == FR_LOST && g->folk[k].rec == i;
        for (int k = 0; k < MAX_MONSTERS && !there; k++) there = g->mons[k].alive && g->mons[k].townq == i + 1;
        if (there) continue;
        /* a place to stand in that chunk */
        vec3 p = v3(0, 0, 0);
        bool found = false;
        for (int a = 0; a < 200 && !found; a++) {
            u64 h = mix64(r->key ^ (u64)a * 0x31);
            int x = r->tx * CHUNK + 2 + (int)(h % 28), z = r->tz * CHUNK + 2 + (int)((h >> 8) % 28);
            for (int y = r->ty * CHUNK + 28; y > r->ty * CHUNK + 1 && !found; y--)
                if (standable(g, x, y, z) && !game_nearest_player(g, v3(x + 0.5f, (float)y, z + 0.5f), 10.0f)) { p = v3(x + 0.5f, (float)y, z + 0.5f); found = true; }
        }
        if (!found) { r->got = 1; continue; } /* (nowhere: they made it home by themselves) */
        u64 key = chunk_spawn_key(r->tx, r->ty, r->tz);
        int level = MAXI(1, r->level);
        if (r->qtype == TQ_LOST) {
            Folk *f = folk_new(g, r->people, FR_LOST, p, r->key ^ (u64)r->done * 0x1057, i);
            if (f) people_name(r->people, r->key ^ (u64)r->done * 0x1057, f->name, sizeof f->name);
            /* and what keeps them there */
            for (int k = 0; k < 3; k++) {
                vec3 q = v3_add(p, v3((float)(k - 1) * 3.0f, 0, 3.0f));
                if (standable(g, fl(q.x), fl(q.y), fl(q.z))) monster_spawn(g, level < 3 ? MON_CRAWLER : MON_HOUND, q, level, false, v3(0.6f, 0.5f, 0.4f), key);
            }
        } else {
            static const u8 BEAST[4] = { MON_BRUTE, MON_TROLL, MON_SPIDER, MON_WORM };
            int type = level < 3 ? MON_HORNED : BEAST[(r->key >> 3) % 4];
            Monster *m = monster_spawn(g, type, p, level + 1, true, v3(0.5f, 0.35f, 0.3f), key);
            if (m) {
                monster_make_boss(g, m);
                m->townq = (u8)(i + 1);
                static const char *NAMES[6] = { "the Gnawer", "Old Tusk", "the Hollow Mother", "Lamp-Eater", "the Grey Hunger", "Stair-Stalker" };
                snprintf(m->name, sizeof m->name, "%s", NAMES[(r->key ^ r->done) % 6]);
            }
        }
    }
}

void town_update(Game *g, float dt) {
    static float spawn_t;
    if ((spawn_t -= dt) <= 0) { spawn_t = 1.0f; quest_spawns(g); }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerSlot *ps = &g->players[i];
        if (ps->active && ps->st.company_t > 0) ps->st.company_t -= dt;
    }
    kin_update(g);
    raid_update(g, dt);
    celebrate(g, dt);
    for (int i = 0; i < MAX_FOLK; i++) {
        Folk *f = &g->folk[i];
        if (!f->alive) continue;
        if (!game_nearest_player(g, f->pos, 130.0f) && f->rec != -50) { f->alive = false; continue; } /* (they come again with their town) */
        if (f->rec <= -60 && f->rec > -100) { /* kin in the Sanctum: at ease, and they look at you */
            f->anim += dt;
            if (f->talk_t > 0) f->talk_t -= dt;
            PlayerSlot *ps = game_nearest_player(g, f->pos, 5.0f);
            if (ps) f->yaw = atan2f(ps->pl.pos.z - f->pos.z, ps->pl.pos.x - f->pos.x);
            if (ps && ps->banner_time <= 0 && ps->msg_t <= 0 && v3_len(v3_sub(ps->pl.pos, f->pos)) < 3.0f) {
                char b[96];
                snprintf(b, sizeof b, "%s, YOUR KIN - STRIKE TO TALK", f->name);
                upper(b);
                game_banner(ps, b);
            }
            continue;
        }
        f->anim += dt;
        if (f->talk_t > 0) { f->talk_t -= dt; continue; } /* listening */
        Town t;
        if (f->rec == -50) { /* allies at the Old King's throne (king.c): close by, shooting what attacks */
            PlayerSlot *lead = game_nearest_player(g, f->pos, 60.0f);
            if (lead && v3_len(v3_sub(lead->pl.pos, f->pos)) > 4.0f) {
                vec3 to = v3_sub(lead->pl.pos, f->pos);
                to.y = 0;
                f->pos = v3_add(f->pos, v3_scale(v3_norm(to), 3.5f * dt));
                f->pos.y = lead->pl.pos.y;
                f->yaw = atan2f(to.z, to.x);
            }
            if ((f->act_t -= dt) <= 0) { f->act_t = 1.2f; guard_shoots(g, f); }
            continue;
        }
        if (f->role == FR_LOST) {
            PlayerSlot *ps = game_nearest_player(g, f->pos, 12.0f);
            if (ps) f->yaw = atan2f(ps->pl.pos.z - f->pos.z, ps->pl.pos.x - f->pos.x);
            if (ps && ps->banner_time <= 0) game_banner(ps, "SOMEONE CALLS FOR HELP - STRIKE TO SPEAK TO THEM");
            continue;
        }
        if (!town_here(g, f->home, &t)) continue;
        if (f->role == FR_GUARD && (f->act_t -= dt) <= 0) { f->act_t = 1.4f; guard_shoots(g, f); }
        /* walk toward the goal; a new one now and then or when blocked */
        f->timer -= dt;
        vec3 to = v3_sub(f->goal, f->pos);
        to.y = 0;
        float d = v3_len(to);
        float sp = f->role == FR_CHILD ? 2.6f : f->role == FR_GUARD ? 1.6f : 1.2f;
        if (d > 0.3f && f->timer > 0) {
            to = v3_scale(to, 1.0f / d);
            vec3 n = v3_add(f->pos, v3_scale(to, sp * dt));
            if (game_solid(g, fl(n.x), fl(n.y + 0.1f), fl(n.z)) || game_solid(g, fl(n.x), fl(n.y + 1.1f), fl(n.z)) ||
                !game_solid(g, fl(n.x), fl(n.y - 0.5f), fl(n.z))) f->timer = 0;
            else { f->pos = n; f->yaw = atan2f(to.z, to.x); }
        } else if (f->timer <= 0) {
            wander_goal(g, f, &t);
            f->timer = f->role >= FR_GUARD ? 8.0f + (float)(mix64(f->key ^ (u64)(g->time)) % 600) / 100.0f : 4.0f;
        }
        /* someone near: they look at you, and say who they are */
        PlayerSlot *ps = game_nearest_player(g, f->pos, 3.0f);
        if (ps && d < 0.5f) f->yaw = atan2f(ps->pl.pos.z - f->pos.z, ps->pl.pos.x - f->pos.x);
        if (ps && ps->banner_time <= 0 && ps->msg_t <= 0) {
            char b[96];
            snprintf(b, sizeof b, "%s, %s - STRIKE TO TALK", f->name, f->role == FR_CHILD ? "a child" : FOLK_ROLES[f->role]);
            upper(b);
            game_banner(ps, b);
        }
    }
}

/* ---------- drawing ---------- */

static void draw_folk(Game *g, const Folk *f) {
    const PeopleDef *P = &PEOPLES[f->people];
    float s = P->height * (f->role == FR_CHILD ? 0.6f : 1.0f);
    float w = P->width * (f->role == FR_CHILD ? 0.8f : 1.0f);
    vec3 l = game_light_at(g, v3_add(f->pos, v3(0, 1.0f, 0)));
    l = v3(fmaxf(l.x, 0.35f), fmaxf(l.y, 0.32f), fmaxf(l.z, 0.3f));
    vec3 skin = rgb3(P->skin[f->look % 3]), hair = rgb3(P->hair[(f->look >> 2) % 3]), cloth = rgb3(P->cloth[(f->look >> 4) % 3]);
    if (f->role == FR_HEALER) cloth = v3(0.88f, 0.86f, 0.8f);
    if (f->role == FR_GUARD) cloth = v3(0.45f, 0.45f, 0.5f);
    if (f->role == FR_ELDER) cloth = v3_scale(cloth, 0.6f);
    skin = v3(skin.x * l.x, skin.y * l.y, skin.z * l.z);
    hair = v3(hair.x * l.x, hair.y * l.y, hair.z * l.z);
    cloth = v3(cloth.x * l.x, cloth.y * l.y, cloth.z * l.z);
    vec3 dark = v3_scale(cloth, 0.6f);
    float yaw = f->yaw;
    vec3 fw = v3(cosf(yaw), 0, sinf(yaw)), rt = v3(-sinf(yaw), 0, cosf(yaw));
    bool walking = f->timer > 0 && f->talk_t <= 0 && v3_len(v3_sub(f->goal, f->pos)) > 0.3f;
    float swing = walking ? sinf(f->anim * 8.0f) * 0.25f : 0.0f;
    vec3 p = f->pos;
    /* legs */
    for (int k = -1; k <= 1; k += 2) {
        vec3 hip = v3_add(p, v3_add(v3_scale(rt, 0.1f * w * k), v3(0, 0.42f * s, 0)));
        vec3 foot = v3_add(hip, v3_add(v3_scale(fw, swing * k * 0.4f * s), v3(0, -0.4f * s, 0)));
        batch3d_box(v3_scale(v3_add(hip, foot), 0.5f), v3(0.07f * w, 0.22f * s, 0.07f * w), yaw, dark, false);
    }
    /* body (a robe for the healer and the elder) */
    vec3 torso = v3_add(p, v3(0, 0.95f * s, 0));
    batch3d_box(torso, v3(0.13f * w, 0.28f * s, 0.2f * w), yaw, cloth, false);
    if (f->role == FR_HEALER || f->role == FR_ELDER) batch3d_box(v3_add(p, v3(0, 0.5f * s, 0)), v3(0.15f * w, 0.25f * s, 0.21f * w), yaw, cloth, false);
    batch3d_box(v3_add(torso, v3(0, -0.2f * s, 0)), v3(0.135f * w, 0.03f * s, 0.205f * w), yaw, dark, false); /* belt */
    /* arms */
    for (int k = -1; k <= 1; k += 2) {
        vec3 sh = v3_add(torso, v3_add(v3_scale(rt, 0.25f * w * k), v3(0, 0.18f * s, 0)));
        vec3 hand = v3_add(sh, v3_add(v3_scale(fw, -swing * k * 0.35f * s), v3(0, -0.45f * s, 0)));
        if (f->talk_t > 0 && k == 1) hand = v3_add(sh, v3_add(v3_scale(fw, 0.3f * s), v3(0, -0.15f * s + sinf(f->anim * 6.0f) * 0.05f, 0))); /* talking with its hands */
        batch3d_box(v3_scale(v3_add(sh, hand), 0.5f), v3(0.055f * w, 0.22f * s, 0.055f * w), yaw, cloth, false);
        batch3d_box(hand, v3(0.045f, 0.045f, 0.045f), yaw, skin, false);
    }
    /* head */
    vec3 head = v3_add(torso, v3(0, 0.45f * s, 0));
    float hs = 0.13f * (P->height < 0.8f ? 1.2f : 1.0f) * (f->role == FR_CHILD ? 1.25f : 1.0f) * s / fmaxf(s, 0.6f);
    batch3d_box(head, v3(hs, hs, hs), yaw, skin, false);
    batch3d_box(v3_add(head, v3_add(v3_scale(fw, -0.02f), v3(0, hs * 0.75f, 0))), v3(hs * 1.05f, hs * 0.35f, hs * 1.05f), yaw, hair, false);
    for (int k = -1; k <= 1; k += 2) /* eyes */
        batch3d_box(v3_add(head, v3_add(v3_add(v3_scale(fw, hs * 0.95f), v3_scale(rt, hs * 0.4f * k)), v3(0, hs * 0.15f, 0))), v3(0.015f, 0.015f, 0.015f), yaw,
                    v3(0.05f, 0.05f, 0.05f), false);
    if (P->ears)
        for (int k = -1; k <= 1; k += 2) {
            float len = P->ears == 1 ? 0.12f : P->ears == 2 ? 0.14f : 0.06f;
            batch3d_spike(v3_add(head, v3_scale(rt, hs * k)), v3_norm(v3_add(v3_scale(rt, (float)k), v3(0, P->ears == 2 ? 0.2f : 0.7f, 0))), P->ears == 2 ? 0.05f : 0.025f, len, skin, false);
        }
    if (P->beard && (f->look & 7) < P->beard && f->role != FR_CHILD) /* a beard */
        batch3d_box(v3_add(head, v3_add(v3_scale(fw, hs * 0.85f), v3(0, -hs * 0.9f, 0))), v3(hs * 0.3f, hs * (P->height < 0.9f ? 0.9f : 0.5f), hs * 0.8f), yaw, hair, false);
    if (P->tusks)
        for (int k = -1; k <= 1; k += 2) batch3d_spike(v3_add(head, v3_add(v3_scale(fw, hs), v3_add(v3_scale(rt, hs * 0.4f * k), v3(0, -hs * 0.5f, 0)))), v3(0, 1, 0), 0.015f, 0.06f, v3(0.9f, 0.88f, 0.8f), false);
    /* hats */
    vec3 crown = v3_add(head, v3(0, hs * 1.05f, 0));
    int hat = f->role == FR_GUARD ? 3 : P->hat;
    switch (hat) {
    case 1: batch3d_spike(crown, v3(0, 1, 0), hs * 1.1f, 0.4f, f->role == FR_ELDER ? v3(0.5f, 0.3f, 0.6f) : cloth, false); break;
    case 2: batch3d_box(v3_add(head, v3(0, hs * 0.5f, -0.0f)), v3(hs * 1.2f, hs * 0.8f, hs * 1.2f), yaw, dark, false); break;
    case 3: batch3d_box(v3_add(crown, v3(0, -hs * 0.1f, 0)), v3(hs * 1.15f, hs * 0.35f, hs * 1.15f), yaw, v3(0.55f * l.x, 0.55f * l.y, 0.6f * l.z), false); break;
    case 4: batch3d_box(crown, v3(hs * 1.08f, 0.015f, hs * 1.08f), yaw, v3(0.9f * l.x, 0.8f * l.y, 0.4f * l.z), false); break;
    case 5: batch3d_box(crown, v3(hs * 1.05f, 0.04f, hs * 1.05f), yaw, v3(0.6f * l.x, 0.15f * l.y, 0.12f * l.z), false); break;
    case 6: batch3d_box(v3_add(crown, v3(0, -0.02f, 0)), v3(hs * 2.0f, 0.015f, hs * 2.0f), yaw, v3(0.85f * l.x, 0.75f * l.y, 0.45f * l.z), false); break;
    default: break;
    }
    /* what they carry */
    vec3 hand = v3_add(torso, v3_add(v3_scale(rt, 0.3f * w), v3_add(v3_scale(fw, 0.1f), v3(0, -0.25f * s, 0))));
    switch (f->role) {
    case FR_GUARD: batch3d_spike(v3_add(hand, v3(0, -0.4f, 0)), v3(0, 1, 0), 0.025f, 2.0f * s, v3(0.5f * l.x, 0.4f * l.y, 0.3f * l.z), false); break;
    case FR_ELDER: batch3d_spike(v3_add(hand, v3(0, -0.6f * s, 0)), v3(0.05f, 1, 0), 0.03f, 1.6f * s, v3(0.45f * l.x, 0.3f * l.y, 0.15f * l.z), false); break;
    case FR_SMITH: batch3d_box(hand, v3(0.04f, 0.12f, 0.04f), yaw, v3(0.4f, 0.4f, 0.42f), false); batch3d_box(v3_add(hand, v3(0, 0.12f, 0)), v3(0.08f, 0.04f, 0.05f), yaw, v3(0.5f, 0.5f, 0.55f), false); break;
    case FR_INNKEEPER: batch3d_box(hand, v3(0.05f, 0.07f, 0.05f), yaw, v3(0.7f * l.x, 0.5f * l.y, 0.25f * l.z), false); break;
    case FR_HEALER: batch3d_octa(v3_add(hand, v3(0, 0.08f, 0)), 0.05f, 0.07f, f->anim, v3(0.7f, 1.0f, 0.7f), true); break;
    case FR_MERCHANT: batch3d_box(v3_add(torso, v3_scale(fw, 0.2f * w)), v3(0.02f, 0.25f * s, 0.16f * w), yaw, v3(0.8f * l.x, 0.75f * l.y, 0.65f * l.z), false); break; /* an apron */
    case FR_LOST: batch3d_octa(v3_add(head, v3(0, 0.5f + 0.08f * sinf(f->anim * 3.0f), 0)), 0.06f, 0.1f, f->anim, v3(1.0f, 0.9f, 0.4f), true); break;
    default: break;
    }
}

void town_draw(Game *g, PlayerSlot *viewer) {
    vec3 eye = game_view_eye(viewer);
    for (int i = 0; i < MAX_FOLK; i++) {
        const Folk *f = &g->folk[i];
        if (f->alive && v3_len(v3_sub(f->pos, eye)) < 70.0f) draw_folk(g, f);
    }
}

/* The newest task under the story's line. */
int town_draw_tracker(Game *g, PlayerSlot *ps, int W, int H, int y) {
    if (ps->st.dead) return 0;
    const TownRec *best = NULL;
    for (int i = 0; i < g->world.ntowns; i++) {
        const TownRec *r = &g->world.towns[i];
        if (r->qstate == 1 && (!best || r->qseq > best->qseq)) best = r;
    }
    if (!best) return 0;
    if (!H) return 1;
    char buf[160], dir[64];
    const char *home = "THE ELDER";
    switch (best->qtype) {
    case TQ_HUNT:
        if (best->got >= best->need) snprintf(buf, sizeof buf, "! DONE - BACK TO %s", home);
        else snprintf(buf, sizeof buf, "! FOR THE TOWN: BEASTS %d/%d", best->got, best->need);
        break;
    case TQ_FETCH: snprintf(buf, sizeof buf, "! FOR THE TOWN: %s %d/%d", need_name(best->item), MINI(need_have(&ps->st, best->item), best->need), best->need); break;
    default:
        if (best->got) {
            story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, floordiv(best->px, CHUNK), floordiv(best->py, CHUNK), floordiv(best->pz, CHUNK), dir, sizeof dir);
            snprintf(buf, sizeof buf, "! DONE - BACK TO %s, %s", home, dir);
        } else {
            story_direction(ps->pl.pos.x, ps->pl.pos.y + 1.0f, ps->pl.pos.z, best->tx, best->ty, best->tz, dir, sizeof dir);
            snprintf(buf, sizeof buf, "! %s - %s", best->qtype == TQ_LOST ? "SOMEONE LOST" : "THE BEAST", dir);
        }
        break;
    }
    upper(buf);
    int maxc = (W - 8) / 6;
    if ((int)strlen(buf) > maxc) buf[maxc] = 0;
    font_begin(W, H);
    font_text((W - font_text_width(buf)) / 2, y, buf, 0.75f, 0.9f, 0.55f);
    font_end();
    return 1;
}
