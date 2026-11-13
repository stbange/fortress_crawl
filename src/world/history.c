#include "world/history.h"
#include "world/story.h"
#include "world/story_text.h"

#include <stdio.h>
#include <string.h>

/* The world's hidden history (docs/STORY_GEN.md).
 *
 *   1. simulate: the founding, two old eras (wars, pacts, heresies, hoards), the generation
 *      before the fall (people with traits, kin and drives), the fall (who opened the seal,
 *      with whose key, why), the blame and the lie, and now (who survives, who serves whom,
 *      who burned the player's village and why)
 *   2. find the mystery: plot shapes are patterns over the events; the best one wins, and a
 *      dull history (a short causal chain, no shape) is thrown away and simulated again
 *   3. cast: the survivors become the named enemies of the first three arcs
 *   4. name: cultures follow the arcs' directions, so names are made last
 *   5. facts, revelations, clues and questions
 *
 * Indices are u8 with H_NONE for none; capacities are well above what a history uses. */

#define LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define PICK(r, a) (a)[rng_range((r), 0, LEN(a) - 1)]

const char *SHAPE_NAMES[SH_COUNT] = { "betrayal", "usurpation", "pact", "the cage was bait", "the wrong monster", "mercy",
                                      "cover-up", "your village", "the voice below", "the hidden heir" };
const char *TONE_NAMES[TONE_COUNT] = { "cosmic horror", "human drama", "tragedy" };
const char *BELOW_NAMES[BL_COUNT] = { "a hunger, fed", "a sleeper, lured", "the first people, walled in", "a wronged king, chained",
                                      "buried miners", "the walled-in sick" };
const char *EV_NAMES[EV_COUNT] = {
    "found", "seal", "bait", "imprison", "bury", "keys", "feed", "war", "massacre", "heresy", "purge", "pact", "hoard", "chronicle",
    "weapon", "take-rule", "roll", "birth", "marry", "oath", "loss", "voice", "learn", "silence", "exile", "debt", "convert", "rival",
    "steal", "give", "open", "breakout", "descend", "return", "seize", "die", "blame", "punish", "forge", "account", "flee",
    "burn", "swear", "hide",
};
const char *ROLE_NAMES[RO_COUNT] = { "founder", "ruler", "keeper", "captain", "scribe", "smith", "priest", "merchant", "soldier",
                                     "heir", "king", "witness", "kin" };
const char *MOTIVE_V_NAMES[MV_COUNT] = { "-", "grief", "revenge", "power", "greed", "faith", "truth", "duty", "pact", "reclaim" };
const char *QUESTION_NAMES[Q_COUNT] = { "who", "guilty", "why", "below", "village", "where" };

/* factions by fixed index */
enum { F_BUILD, F_K1, F_K2, F_K3, F_GUARD, F_TRADE, F_CULT, F_FIXED };

typedef struct Sim {
    History *h;
    Rng r;
    u8 ruling;
    u8 R, C, S, P, M, K[3], D; /* the generation before the fall */
    u8 pact_ev, pact_maker, war_ev;
    u8 learned; /* someone who found out the founding's secret */
    u8 silenced;
} Sim;

/* ---------- building blocks ---------- */

static bool has(const HActor *a, int t) { return a->trait[0] == t || a->trait[1] == t; }
static bool alive(const History *h, u8 a) { return a != H_NONE && h->actor[a].died == H_NONE; }

static u8 actor(Sim *s, u8 fac, u8 role, u8 era) {
    History *h = s->h;
    if (h->nactors >= H_ACTORS) return (u8)(H_ACTORS - 1);
    u8 i = (u8)h->nactors++;
    HActor *a = &h->actor[i];
    memset(a, 0, sizeof *a);
    a->faction = fac;
    a->role = role;
    a->born = era;
    a->died = H_NONE;
    a->death_ev = a->parent = a->sibling = a->spouse = a->child = a->sworn = H_NONE;
    a->motive = MV_NONE;
    a->mtarget = a->mcause = a->key = H_NONE;
    a->cast = -1;
    a->female = (u8)rng_chance(&s->r, 0.5f);
    a->trait[0] = (u8)rng_range(&s->r, 0, TT_COUNT - 1);
    do a->trait[1] = (u8)rng_range(&s->r, 0, TT_COUNT - 1);
    while (a->trait[1] == a->trait[0]);
    /* roles lean toward some traits */
    static const u8 LEAN[RO_COUNT] = { TT_PROUD, TT_PROUD, TT_LOYAL, TT_LOYAL, TT_CURIOUS, TT_PROUD, TT_DEVOUT, TT_GREEDY,
                                       TT_CRUEL, TT_AMBITIOUS, TT_PROUD, TT_CURIOUS, TT_LOYAL };
    if (rng_chance(&s->r, 0.55f) && a->trait[1] != LEAN[role]) a->trait[0] = LEAN[role];
    return i;
}

static u8 event(Sim *s, u8 type, u8 era, u8 a, u8 b, u8 cause) {
    History *h = s->h;
    u8 i = (u8)(h->nevents < H_EVENTS ? h->nevents++ : H_EVENTS - 1);
    HEvent *e = &h->ev[i];
    memset(e, 0, sizeof *e);
    e->type = type;
    e->era = era;
    e->a = a;
    e->b = b;
    e->cause = cause;
    e->f1 = a != H_NONE ? h->actor[a].faction : H_NONE;
    e->f2 = b != H_NONE ? h->actor[b].faction : H_NONE;
    e->art = e->place = H_NONE;
    return i;
}

static u8 artifact(Sim *s, u8 kind, u8 maker, u8 ev) {
    History *h = s->h;
    u8 i = (u8)(h->narts < H_ARTS ? h->narts++ : H_ARTS - 1);
    HArtifact *a = &h->art[i];
    memset(a, 0, sizeof *a);
    a->kind = kind;
    a->maker = maker;
    a->holder = H_NONE;
    a->event = ev;
    a->legend = -1;
    return i;
}

static u8 place(Sim *s) {
    History *h = s->h;
    return (u8)(h->nplaces < 8 ? h->nplaces++ : 7);
}

static void die(Sim *s, u8 who, u8 era, u8 ev) {
    if (who == H_NONE) return;
    HActor *a = &s->h->actor[who];
    if (a->died != H_NONE) return;
    a->died = era;
    a->death_ev = ev;
}

static void siblings(History *h, u8 a, u8 b) {
    h->actor[a].sibling = b;
    h->actor[b].sibling = a;
}
static void marry(History *h, u8 a, u8 b) {
    h->actor[a].spouse = b;
    h->actor[b].spouse = a;
    h->actor[b].female = (u8)!h->actor[a].female;
}
static void parent(History *h, u8 p, u8 c) {
    h->actor[p].child = c;
    h->actor[c].parent = p;
}

/* How b stands to a: RL_* (b is a's sibling, spouse, parent, child, liege). */
static int relation(const History *h, u8 a, u8 b) {
    if (a == H_NONE || b == H_NONE) return RL_NONE;
    const HActor *x = &h->actor[a];
    if (x->sibling == b) return RL_SIBLING;
    if (x->spouse == b) return RL_SPOUSE;
    if (x->parent == b) return RL_PARENT;
    if (x->child == b) return RL_CHILD;
    if (x->sworn == b || h->actor[b].sworn == a) return RL_OATH;
    return RL_NONE;
}
static bool kin(const History *h, u8 a, u8 b) {
    int r = relation(h, a, b);
    return r == RL_SIBLING || r == RL_SPOUSE || r == RL_PARENT || r == RL_CHILD;
}

/* The living kin of someone (child, sibling, spouse, parent), not in the skip list. */
static u8 living_kin(const History *h, u8 a, const u8 *skip, int nskip) {
    if (a == H_NONE) return H_NONE;
    const HActor *x = &h->actor[a];
    u8 c[4] = { x->child, x->sibling, x->spouse, x->parent };
    for (int i = 0; i < 4; i++) {
        if (!alive(h, c[i])) continue;
        bool skipped = false;
        for (int k = 0; k < nskip; k++) skipped |= skip[k] == c[i];
        if (!skipped) return c[i];
    }
    return H_NONE;
}

static void drive(History *h, u8 who, u8 motive, u8 target, u8 cause) {
    if (who == H_NONE) return;
    HActor *a = &h->actor[who];
    a->motive = motive;
    a->mtarget = target;
    a->mcause = cause;
}

/* ---------- 1. the simulation ---------- */

static void sim_found(Sim *s) {
    History *h = s->h;
    Rng *r = &s->r;
    u8 founder = actor(s, F_BUILD, RO_FOUNDER, ERA_FOUND);
    h->actor[founder].died = ERA_FOUND;
    u8 e = event(s, EV_FOUND, ERA_FOUND, founder, H_NONE, H_NONE);
    u8 sealed;
    switch (h->below) {
    case BL_HUNGER:
        sealed = event(s, EV_SEAL, ERA_FOUND, founder, H_NONE, e);
        h->ev_founding = sealed;
        h->ev_secret = sealed;
        if (rng_chance(r, 0.65f)) h->ev_secret = event(s, EV_FEED, ERA_FOUND, founder, H_NONE, sealed); /* and they fed it */
        break;
    case BL_SLEEPER:
        sealed = event(s, EV_BAIT, ERA_FOUND, founder, H_NONE, e);
        h->ev_founding = h->ev_secret = sealed;
        break;
    case BL_FIRST:
        sealed = event(s, EV_IMPRISON, ERA_FOUND, founder, H_NONE, e);
        h->ev_founding = h->ev_secret = sealed;
        break;
    case BL_KING: {
        u8 king = actor(s, H_NONE, RO_KING, ERA_FOUND);
        h->actor[king].faction = F_BUILD;
        h->actor[king].flags |= AF_HIDDEN; /* still below */
        sealed = event(s, EV_IMPRISON, ERA_FOUND, founder, king, e);
        h->ev[sealed].sub = 1;
        h->ev_founding = h->ev_secret = sealed;
        break;
    }
    default:
        sealed = event(s, EV_BURY, ERA_FOUND, founder, H_NONE, e);
        h->ev[sealed].sub = h->below == BL_PLAGUE;
        h->ev_founding = h->ev_secret = sealed;
        break;
    }
    h->ev[sealed].place = place(s); /* places[0]: the seal */
    u8 keys = event(s, EV_KEYS, ERA_FOUND, founder, H_NONE, sealed);
    for (int i = 0; i < 3; i++) artifact(s, AK_KEY, founder, keys); /* arts 0-2: the keys */
}

static void sim_old(Sim *s) {
    History *h = s->h;
    Rng *r = &s->r;
    for (int era = ERA_OLD1; era <= ERA_OLD2; era++) {
        int n = 2 + rng_chance(r, 0.6f);
        bool cult = h->faction[F_CULT].flags & 1, chronicle = false, roll = false;
        for (int i = 0; i < h->nevents; i++) {
            chronicle |= h->ev[i].type == EV_CHRONICLE;
            roll |= h->ev[i].type == EV_ROLL;
        }
        for (int i = 0; i < n || (era == ERA_OLD2 && h->ev_hoard == H_NONE); i++) {
            if (h->nevents > H_EVENTS - 50) break;
            float w[7] = { 3, cult ? 0 : 2, h->ev_hoard == H_NONE ? 3 : 0, chronicle ? 0 : 2, 2,
                           (era == ERA_OLD2 && s->ruling == F_BUILD) ? 1.5f : 0, roll ? 0 : 1 };
            if (i >= n) { memset(w, 0, sizeof w); w[2] = 1; }
            switch (pick_weighted(rng_float(r), w, 7)) {
            case 0: { /* war */
                u8 fa = (u8)rng_range(r, F_K1, F_TRADE), fb;
                do fb = (u8)rng_range(r, F_K1, F_TRADE);
                while (fb == fa);
                u8 la = actor(s, fa, RO_RULER, (u8)era), lb = actor(s, fb, RO_RULER, (u8)era);
                h->actor[la].died = h->actor[lb].died = (u8)era;
                u8 e = event(s, EV_WAR, (u8)era, la, lb, H_NONE);
                h->faction[fa].stance[fb] = h->faction[fb].stance[fa] = -3;
                s->war_ev = e;
                u8 cause = e;
                if (rng_chance(r, 0.55f)) {
                    u8 m = event(s, EV_MASSACRE, (u8)era, la, lb, e);
                    h->ev[m].place = place(s);
                    cause = m;
                }
                if (h->tone != TONE_HUMAN && s->pact_ev == H_NONE && rng_chance(r, 0.4f)) {
                    u8 p = event(s, EV_PACT, (u8)era, lb, la, cause);
                    h->ev[p].sub = (u8)rng_range(r, 0, 2);
                    h->ev[p].art = artifact(s, AK_PACT, lb, p);
                    s->pact_ev = p;
                    s->pact_maker = lb;
                }
                break;
            }
            case 1: { /* heresy */
                h->faction[F_CULT].flags |= 1;
                cult = true;
                u8 pr = actor(s, F_CULT, RO_PRIEST, (u8)era);
                h->actor[pr].died = (u8)era;
                u8 e = event(s, EV_HERESY, (u8)era, pr, H_NONE, h->ev_secret);
                h->ev[e].f2 = (u8)rng_range(r, F_K1, F_K3);
                if (rng_chance(r, 0.6f)) {
                    u8 ru = actor(s, s->ruling, RO_RULER, (u8)era);
                    h->actor[ru].died = (u8)era;
                    u8 pu = event(s, EV_PURGE, (u8)era, ru, pr, e);
                    h->ev[pu].f2 = F_CULT;
                    die(s, pr, (u8)era, pu);
                    h->faction[F_CULT].stance[s->ruling] = -3;
                }
                break;
            }
            case 2: { /* the hoard */
                u8 hider = actor(s, s->ruling, RO_RULER, (u8)era);
                h->actor[hider].died = (u8)era;
                u8 e = event(s, EV_HOARD, (u8)era, hider, H_NONE, s->war_ev);
                h->ev[e].f2 = s->war_ev != H_NONE ? h->ev[s->war_ev].f1 : (u8)rng_range(r, F_K1, F_TRADE);
                if (h->ev[e].f2 == h->ev[e].f1) h->ev[e].f2 = F_TRADE;
                h->ev[e].art = artifact(s, AK_HOARD, hider, e);
                h->ev_hoard = e;
                break;
            }
            case 3: { /* the chronicle: the founding as the founders wanted it told */
                chronicle = true;
                u8 sc = actor(s, s->ruling, RO_SCRIBE, (u8)era);
                h->actor[sc].died = (u8)era;
                u8 e = event(s, EV_CHRONICLE, (u8)era, sc, H_NONE, h->ev_founding);
                h->ev[e].art = artifact(s, AK_BOOK, sc, e);
                break;
            }
            case 4: { /* a weapon */
                u8 sm = actor(s, (u8)rng_range(r, F_BUILD, F_TRADE), RO_SMITH, (u8)era);
                u8 lord = actor(s, (u8)rng_range(r, F_BUILD, F_GUARD), RO_RULER, (u8)era);
                h->actor[sm].died = h->actor[lord].died = (u8)era;
                u8 e = event(s, EV_WEAPON, (u8)era, sm, lord, H_NONE);
                h->ev[e].art = artifact(s, AK_WEAPON, sm, e);
                break;
            }
            case 5: { /* a keeper house takes the fortress from its builders */
                u8 fa = (u8)rng_range(r, F_K1, F_K3);
                u8 l = actor(s, fa, RO_RULER, (u8)era);
                h->actor[l].died = (u8)era;
                event(s, EV_TAKE_RULE, (u8)era, l, H_NONE, s->war_ev);
                h->ev[h->nevents - 1].f2 = F_BUILD;
                s->ruling = fa;
                break;
            }
            default: { /* the Roll of the Dead */
                roll = true;
                u8 c = actor(s, F_GUARD, RO_CAPTAIN, (u8)era);
                h->actor[c].died = (u8)era;
                event(s, EV_ROLL, (u8)era, c, H_NONE, H_NONE);
                break;
            }
            }
        }
    }
}

/* The generation before the fall: people, kin, and what drives them. */
static void sim_before(Sim *s) {
    History *h = s->h;
    Rng *r = &s->r;
    const u8 E = ERA_BEFORE;
    s->R = actor(s, s->ruling, RO_RULER, E);
    for (int i = 0; i < 3; i++) {
        u8 f = (u8)(F_K1 + i);
        if (f == s->ruling) f = F_BUILD; /* the ruling house's key is held by the builders' last line */
        s->K[i] = actor(s, f, RO_KEEPER, E);
        h->actor[s->K[i]].key = (u8)i;
        h->art[i].holder = s->K[i];
    }
    s->C = actor(s, F_GUARD, RO_CAPTAIN, E);
    h->actor[s->C].sworn = s->R;
    event(s, EV_OATH, E, s->C, s->R, H_NONE);
    s->S = actor(s, rng_chance(r, 0.6f) ? s->ruling : (u8)rng_range(r, F_K1, F_K3), RO_SCRIBE, E);
    s->P = actor(s, (h->faction[F_CULT].flags & 1) ? F_CULT : (u8)rng_range(r, F_K1, F_K3), RO_PRIEST, E);
    s->M = actor(s, F_TRADE, RO_MERCHANT, E);
    if (s->pact_ev != H_NONE) {
        /* the bargain's price falls due in this generation: an heir of the one who made it */
        s->D = actor(s, h->actor[s->pact_maker].faction, RO_HEIR, E);
        h->actor[s->D].parent = s->pact_maker; /* "of the line of" */
    }
    /* kin: one or two pairs of siblings, perhaps a marriage, some children */
    u8 pool[8] = { s->K[0], s->K[1], s->K[2], s->C, s->P, s->S, s->R, s->M };
    int pairs = 1 + rng_chance(r, 0.5f);
    for (int p = 0; p < pairs; p++) {
        u8 a = pool[rng_range(r, 0, 5)], b = pool[rng_range(r, 0, 6)];
        if (a == b || h->actor[a].sibling != H_NONE || h->actor[b].sibling != H_NONE) continue;
        siblings(h, a, b);
    }
    if (rng_chance(r, 0.6f)) {
        u8 b = pool[rng_range(r, 0, 4)];
        if (h->actor[b].spouse == H_NONE && h->actor[b].sibling != s->R) marry(h, s->R, b);
    }
    int kids = 2 + rng_chance(r, 0.6f);
    for (int k = 0; k < kids; k++) {
        u8 p = pool[rng_range(r, 0, 7)];
        if (h->actor[p].child != H_NONE) continue;
        u8 c = actor(s, h->actor[p].faction, RO_KIN, E);
        parent(h, p, c);
        event(s, EV_BIRTH, E, p, c, H_NONE);
    }

    /* what happens to them: a few personal events, chosen by who could do them */
    int done[EV_COUNT] = { 0 };
    int rounds = 4 + rng_range(r, 0, 2);
    for (int round = 0; round < rounds; round++) {
        if (h->nevents > H_EVENTS - 30) break;
        enum { P_LOSS, P_VOICE, P_LEARN, P_SILENCE, P_RIVAL, P_EXILE, P_DEBT, P_CONVERT, P_PACT, P_MARRY, P_COUNT };
        float w[P_COUNT] = { 0 };
        /* candidates for each */
        u8 c_loss = H_NONE, c_voice = H_NONE, c_learn = H_NONE, c_rival = H_NONE, c_exile = H_NONE, c_debt = H_NONE,
           c_conv = H_NONE;
        for (int i = 0; i < h->nactors; i++) {
            HActor *a = &h->actor[i];
            if (a->born != E || a->died != H_NONE || a->role == RO_KIN) continue;
            if (alive(h, a->child) && h->actor[a->child].role == RO_KIN && a->motive == MV_NONE && (c_loss == H_NONE || rng_chance(r, 0.4f)))
                c_loss = (u8)i;
            if (a->motive == MV_GRIEF && !(a->flags & AF_HEARD) && h->tone != TONE_HUMAN) c_voice = (u8)i;
            if ((a->role == RO_SCRIBE || has(a, TT_CURIOUS)) && a->motive == MV_NONE && (int)i != s->R &&
                (c_learn == H_NONE || rng_chance(r, 0.5f)))
                c_learn = (u8)i;
            if ((has(a, TT_AMBITIOUS) || has(a, TT_PROUD)) && (int)i != s->R && a->motive == MV_NONE && a->role != RO_MERCHANT)
                c_rival = (u8)i;
            if ((a->motive == MV_POWER || a->motive == MV_FAITH || a->motive == MV_TRUTH) && !(a->flags & AF_EXILED) && (int)i != s->R)
                c_exile = (u8)i;
            if (a->key != H_NONE && (has(a, TT_GREEDY) || has(a, TT_COWARDLY)) && a->motive == MV_NONE) c_debt = (u8)i;
            if ((has(a, TT_DEVOUT) || has(a, TT_CURIOUS) || a->role == RO_PRIEST) && a->motive == MV_NONE && (int)i != s->R &&
                a->faction != F_CULT)
                c_conv = (u8)i;
        }
        w[P_LOSS] = c_loss != H_NONE && done[EV_LOSS] < 2 ? 3.0f : 0;
        w[P_VOICE] = c_voice != H_NONE ? 5.0f : 0;
        w[P_LEARN] = c_learn != H_NONE && !done[EV_LEARN]
                         ? (h->ev[h->ev_secret].type == EV_SEAL ? 1.0f : h->tone == TONE_HORROR ? 2.0f : 3.5f) : 0;
        w[P_SILENCE] = s->learned != H_NONE && s->silenced == H_NONE && alive(h, s->learned) &&
                               (has(&h->actor[s->R], TT_CRUEL) || has(&h->actor[s->R], TT_PROUD) || h->tone != TONE_HORROR)
                           ? 3.0f : 0;
        w[P_RIVAL] = c_rival != H_NONE && done[EV_RIVAL] < 1 ? 2.0f : 0;
        w[P_EXILE] = c_exile != H_NONE && !done[EV_EXILE] ? 2.0f : 0;
        w[P_DEBT] = c_debt != H_NONE && !done[EV_DEBT] ? 2.0f : 0;
        w[P_CONVERT] = c_conv != H_NONE && !done[EV_CONVERT] ? 2.0f : 0;
        w[P_PACT] = s->D != H_NONE && h->actor[s->D].motive == MV_NONE ? 4.0f : 0;
        w[P_MARRY] = done[EV_MARRY] ? 0 : 0.7f;
        int pick = pick_weighted(rng_float(r), w, P_COUNT);
        switch (pick) {
        case P_LOSS: {
            HActor *a = &h->actor[c_loss];
            u8 child = a->child;
            u8 cause = H_NONE, how = (u8)rng_range(r, 0, 1);
            if (h->ev[h->ev_secret].type == EV_FEED && rng_chance(r, 0.7f)) { how = 2; cause = h->ev_secret; } /* the winter tithe */
            else if (h->tone != TONE_HUMAN && rng_chance(r, 0.3f)) how = 3;                                         /* the deep took it */
            u8 e = event(s, EV_LOSS, E, c_loss, child, cause);
            h->ev[e].sub = how;
            die(s, child, E, e);
            drive(h, c_loss, MV_GRIEF, child, e);
            done[EV_LOSS]++;
            break;
        }
        case P_VOICE: {
            HActor *a = &h->actor[c_voice];
            u8 e = event(s, EV_VOICE, E, c_voice, a->mtarget, a->mcause);
            h->ev[e].sub = h->tone == TONE_TRAGEDY;
            a->flags |= AF_HEARD;
            a->mcause = e;
            done[EV_VOICE]++;
            break;
        }
        case P_LEARN: {
            u8 e = event(s, EV_LEARN, E, c_learn, H_NONE, h->ev_secret);
            drive(h, c_learn, MV_TRUTH, H_NONE, e);
            s->learned = c_learn;
            done[EV_LEARN]++;
            break;
        }
        case P_SILENCE: {
            u8 by = alive(h, s->C) ? s->C : s->R;
            u8 e = event(s, EV_SILENCE, E, s->R, s->learned, h->actor[s->learned].mcause);
            h->ev[e].sub = by == s->C;
            die(s, s->learned, E, e);
            s->silenced = s->learned;
            /* the dead one's kin want the ruler's blood */
            u8 k = living_kin(h, s->learned, NULL, 0);
            if (k != H_NONE) drive(h, k, MV_REVENGE, s->R, e);
            done[EV_SILENCE]++;
            break;
        }
        case P_RIVAL: {
            u8 cause = H_NONE; /* an old grudge: their house lost a war */
            if (s->war_ev != H_NONE && h->ev[s->war_ev].f2 == h->actor[c_rival].faction) cause = s->war_ev;
            else if (s->silenced != H_NONE && kin(h, c_rival, s->silenced)) cause = h->actor[s->silenced].death_ev;
            u8 e = event(s, EV_RIVAL, E, c_rival, s->R, cause);
            drive(h, c_rival, MV_POWER, s->R, e);
            done[EV_RIVAL]++;
            break;
        }
        case P_EXILE: {
            HActor *a = &h->actor[c_exile];
            u8 e = event(s, EV_EXILE, E, s->R, c_exile, a->mcause);
            a->flags |= AF_EXILED;
            drive(h, c_exile, MV_REVENGE, s->R, e);
            done[EV_EXILE]++;
            break;
        }
        case P_DEBT: {
            u8 e = event(s, EV_DEBT, E, c_debt, s->M, H_NONE);
            h->ev[e].f2 = F_TRADE;
            drive(h, c_debt, MV_GREED, s->M, e);
            done[EV_DEBT]++;
            break;
        }
        case P_CONVERT: {
            h->faction[F_CULT].flags |= 1;
            u8 e = event(s, EV_CONVERT, E, c_conv, H_NONE, h->ev_secret);
            h->ev[e].f2 = F_CULT;
            drive(h, c_conv, MV_FAITH, H_NONE, e);
            done[EV_CONVERT]++;
            break;
        }
        case P_PACT:
            drive(h, s->D, MV_PACT, H_NONE, s->pact_ev);
            break;
        case P_MARRY: {
            u8 a = pool[rng_range(r, 0, 5)], b = pool[rng_range(r, 0, 7)];
            if (a != b && h->actor[a].spouse == H_NONE && h->actor[b].spouse == H_NONE && h->actor[a].sibling != b) {
                marry(h, a, b);
                event(s, EV_MARRY, E, a, b, H_NONE);
            }
            done[EV_MARRY]++;
            break;
        }
        default: break;
        }
    }
}

static int motive_score(const HActor *a, const Sim *s) {
    switch (a->motive) {
    case MV_GRIEF: return (a->flags & AF_HEARD) ? 7 : 1;
    case MV_PACT: return 7;
    case MV_REVENGE: return a->mtarget == s->R ? 6 : 3;
    case MV_FAITH: return 5;
    case MV_POWER: return 5;
    case MV_TRUTH: return s->h->tone == TONE_HORROR ? 3 : 6;
    case MV_GREED: return 5;
    default: return 0;
    }
}

static void sim_fall(Sim *s) {
    History *h = s->h;
    Rng *r = &s->r;
    const u8 E = ERA_FALL;
    /* who opens it: the strongest drive among the living */
    u8 O = H_NONE;
    float best = 0;
    for (int i = 0; i < h->nactors; i++) {
        HActor *a = &h->actor[i];
        if (a->died != H_NONE || a->born != ERA_BEFORE || a->role == RO_KING) continue;
        float sc = (float)motive_score(a, s);
        if (sc <= 1) continue;
        sc += rng_float(r) * 4.5f;
        if (sc > best) { best = sc; O = (u8)i; }
    }
    if (O == H_NONE || (h->below == BL_SLEEPER && rng_chance(r, 0.6f))) {
        if (h->below == BL_SLEEPER || h->tone == TONE_HORROR) {
            /* the last of the builders, doing what the fortress was built for */
            O = actor(s, F_BUILD, RO_KEEPER, ERA_BEFORE);
            u8 e = event(s, EV_LEARN, ERA_BEFORE, O, H_NONE, h->ev_founding); /* read what the founders meant */
            drive(h, O, MV_DUTY, H_NONE, e);
            if (h->actor[s->K[0]].faction != F_BUILD && rng_chance(r, 0.5f)) siblings(h, O, s->K[0]);
        } else {
            /* the heir of the wronged, who learned the truth */
            O = actor(s, F_CULT, RO_HEIR, ERA_BEFORE);
            u8 e = event(s, EV_LEARN, ERA_BEFORE, O, H_NONE, h->ev_secret);
            drive(h, O, MV_TRUTH, H_NONE, e);
            h->faction[F_CULT].flags |= 1;
        }
    }
    h->opener = O;
    HActor *o = &h->actor[O];
    /* whose key */
    u8 holder = H_NONE, key_ev = H_NONE;
    bool stolen = false;
    if (o->key != H_NONE) {
        holder = O;
    } else {
        for (int i = 0; i < 3 && holder == H_NONE; i++)
            if (alive(h, s->K[i]) && kin(h, O, s->K[i])) holder = s->K[i];
        if (holder != H_NONE) {
            stolen = rng_chance(r, 0.7f) || !has(&h->actor[holder], TT_LOYAL);
        } else {
            for (int i = 0; i < 3 && holder == H_NONE; i++) {
                HActor *k = &h->actor[s->K[i]];
                if (alive(h, s->K[i]) && (k->motive == MV_GREED || k->motive == MV_FAITH || k->motive == MV_TRUTH)) holder = s->K[i];
            }
            if (holder == H_NONE) {
                for (int t = 0; t < 6 && holder == H_NONE; t++) {
                    u8 k = s->K[rng_range(r, 0, 2)];
                    if (alive(h, k)) holder = k;
                }
                stolen = true;
            }
        }
        if (holder == H_NONE) holder = s->K[0];
        u8 key = h->actor[holder].key;
        key_ev = event(s, stolen ? EV_STEAL : EV_GIVE, E, stolen ? O : holder, stolen ? holder : O, stolen ? o->mcause : h->actor[holder].mcause);
        h->ev[key_ev].art = key;
        if (!stolen && h->actor[holder].motive == MV_GREED) h->ev[key_ev].cause = h->actor[holder].mcause;
        h->actor[holder].key = H_NONE;
        o->key = key;
    }
    h->keyholder = holder;
    u8 open = event(s, EV_OPEN, E, O, holder, o->mcause);
    h->ev[open].art = o->key;
    h->ev[open].place = 0;
    h->ev_open = open;
    h->art[o->key].holder = O;
    (void)key_ev;

    /* what came of it */
    bool r_dies = rng_chance(r, 0.5f);
    switch (o->motive) {
    case MV_GRIEF: {
        u8 d = event(s, EV_DESCEND, E, O, o->mtarget, open);
        float u = rng_float(r);
        if (h->tone == TONE_HORROR && u < 0.6f) {
            event(s, EV_RETURN, E, O, H_NONE, d);
            o->flags |= AF_RETURNED;
        } else if (u > 0.75f) {
            die(s, O, E, d); /* went down and did not come back */
        } else {
            o->flags |= AF_HIDDEN; /* below, alive, with them */
        }
        break;
    }
    case MV_POWER: {
        event(s, EV_SEIZE, E, O, s->R, open);
        u8 k = event(s, EV_DIE, E, O, s->R, open);
        die(s, s->R, E, k);
        r_dies = false;
        break;
    }
    case MV_REVENGE:
        if (o->mtarget == s->R) {
            u8 k = event(s, EV_DIE, E, rng_chance(r, 0.5f) ? O : H_NONE, s->R, open);
            die(s, s->R, E, k);
            r_dies = false;
        }
        if (rng_chance(r, 0.5f)) o->flags |= AF_HIDDEN;
        break;
    case MV_FAITH:
    case MV_DUTY:
        if (h->tone == TONE_TRAGEDY) o->flags |= AF_HIDDEN; /* went down to them */
        break;
    case MV_TRUTH:
        if (h->tone == TONE_HUMAN && rng_chance(r, 0.4f)) {
            u8 k = event(s, EV_DIE, E, alive(h, s->C) ? s->C : H_NONE, O, open);
            die(s, O, E, k);
        } else {
            o->flags |= AF_HIDDEN;
        }
        break;
    case MV_GREED: {
        u8 f = event(s, EV_FLEE, E, O, H_NONE, open);
        h->ev[f].art = h->ev_hoard != H_NONE ? h->ev[h->ev_hoard].art : H_NONE;
        o->flags |= AF_HIDDEN;
        break;
    }
    default: break;
    }
    if (r_dies && alive(h, s->R) && s->R != O) {
        u8 k = event(s, EV_DIE, E, H_NONE, s->R, open);
        die(s, s->R, E, k);
    }
    /* a keeper or two dies in the night */
    for (int i = 0; i < 3; i++)
        if (s->K[i] != holder && s->K[i] != O && alive(h, s->K[i]) && rng_chance(r, 0.25f)) {
            u8 k = event(s, EV_DIE, E, H_NONE, s->K[i], open);
            die(s, s->K[i], E, k);
        }

    /* the lie: who tells it, and whom it blames */
    u8 cover = s->ruling;
    u8 leader = alive(h, s->R) && s->R != O ? s->R : H_NONE;
    if (leader == H_NONE && alive(h, s->C) && s->C != O) { leader = s->C; cover = F_GUARD; }
    if (leader == H_NONE) {
        for (int i = 0; i < 3 && leader == H_NONE; i++)
            if (alive(h, s->K[i]) && s->K[i] != O && s->K[i] != holder) { leader = s->K[i]; cover = h->actor[s->K[i]].faction; }
    }
    if (leader == H_NONE) { leader = actor(s, s->ruling, RO_HEIR, ERA_BEFORE); cover = s->ruling; if (s->R != H_NONE) parent(h, s->R, leader); }
    if (cover == h->actor[O].faction && cover != F_GUARD && alive(h, s->C) && s->C != O) { leader = s->C; cover = F_GUARD; }
    h->cover = cover;
    h->faction[cover].leader = leader;
    u8 goat = H_NONE;
    bool blame_below = h->tone != TONE_HUMAN && o->motive != MV_DUTY && rng_chance(r, 0.25f);
    if (!blame_below) {
        if (stolen && holder != H_NONE && holder != leader) goat = holder;
        else {
            u8 cand[6] = { s->P, s->M, s->S, s->K[0], s->K[1], s->K[2] };
            for (int t = 0; t < 10 && goat == H_NONE; t++) {
                u8 c = t < 6 && t % 2 == 0 ? cand[rng_range(r, 0, 5)] : cand[t % 6];
                if (c != O && c != leader && alive(h, c) && h->actor[c].faction != cover) goat = c;
            }
            if (goat == H_NONE) {
                for (int i = 0; i < h->nactors && goat == H_NONE; i++)
                    if ((u8)i != O && (u8)i != leader && alive(h, (u8)i) && h->actor[i].born == ERA_BEFORE && h->actor[i].faction != cover &&
                        h->actor[i].role != RO_KING)
                        goat = (u8)i;
            }
        }
    }
    if (goat == H_NONE && h->tone == TONE_HUMAN) {
        goat = actor(s, F_CULT, RO_PRIEST, ERA_BEFORE); /* an agitator to hang */
        h->faction[F_CULT].flags |= 1;
    }
    h->scapegoat = goat;
    u8 blame = event(s, EV_BLAME, E, leader, goat, open);
    h->ev[blame].f1 = cover;
    h->ev_blame = blame;
    if (goat != H_NONE) {
        u8 by = alive(h, s->C) && s->C != goat && s->C != O ? s->C : leader;
        u8 p = event(s, EV_PUNISH, E, by, goat, blame);
        u8 how = (u8)rng_range(r, 0, 2);
        h->ev[p].sub = how;
        h->ev_punish = p;
        if (how == 2) h->actor[goat].flags |= AF_EXILED;
        else {
            die(s, goat, E, p);
            if (how == 0) h->actor[goat].flags |= AF_WALLED;
        }
        u8 k = living_kin(h, goat, &O, 1);
        if (k != H_NONE && h->actor[k].motive == MV_NONE) drive(h, k, MV_REVENGE, by, p);
    }
    /* the false chronicle, and the true account */
    u8 forger = (s->S != H_NONE && h->actor[s->S].faction == cover && alive(h, s->S)) ? s->S : actor(s, cover, RO_SCRIBE, ERA_BEFORE);
    u8 fe = event(s, EV_FORGE, E, forger, goat, blame);
    h->ev[fe].f1 = cover;
    h->ev[fe].art = artifact(s, AK_FORGED, forger, fe);
    h->faction[cover].flags |= 4;
    u8 W = H_NONE;
    if (s->S != forger && s->S != O && alive(h, s->S)) W = s->S;
    else {
        u8 cand[5] = { living_kin(h, O, NULL, 0), s->K[0], s->K[1], s->K[2], s->P };
        for (int i = 0; i < 5 && W == H_NONE; i++)
            if (alive(h, cand[i]) && cand[i] != O && cand[i] != leader && cand[i] != goat && h->actor[cand[i]].faction != cover) W = cand[i];
        if (W == H_NONE) W = actor(s, F_TRADE, RO_WITNESS, ERA_BEFORE);
    }
    h->witness = W;
    u8 ae = event(s, EV_ACCOUNT, E, W, O, open);
    h->ev[ae].art = artifact(s, AK_ACCOUNT, W, ae);
    h->ev_account = ae;
}

/* Now: who survives, who serves whom, and the village. */
static u8 follower(Sim *s, u8 fac, u8 role, u8 lord) {
    u8 a = actor(s, fac, role, ERA_FALL);
    if (lord != H_NONE) {
        s->h->actor[a].sworn = lord;
        event(s, EV_SWEAR, ERA_NOW, a, lord, H_NONE);
    }
    return a;
}

static bool used(const u8 *cast, int n, u8 a) {
    for (int i = 0; i < n; i++)
        if (cast[i] == a) return true;
    return false;
}

static void sim_now(Sim *s) {
    History *h = s->h;
    Rng *r = &s->r;
    u8 O = h->opener, goat = h->scapegoat, cover = h->cover, *cast = h->cast;
    memset(cast, H_NONE, H_CAST);
    /* arc 0: the ones who tell the lie */
    u8 leader = h->ev[h->ev_blame].a;
    cast[2] = alive(h, leader) ? leader : H_NONE;
    if (cast[2] == H_NONE) {
        cast[2] = actor(s, cover, RO_HEIR, ERA_FALL);
        if (leader != H_NONE && h->actor[leader].child == H_NONE) parent(h, leader, cast[2]);
    }
    u8 punisher = h->ev_punish != H_NONE ? h->ev[h->ev_punish].a : H_NONE;
    cast[1] = alive(h, punisher) && punisher != cast[2] ? punisher : follower(s, cover, RO_CAPTAIN, cast[2]);
    cast[0] = follower(s, cover, RO_SOLDIER, cast[1]);
    /* arc 2: the one who opened it, and those who serve it now */
    u8 cf = h->actor[O].faction;
    if (alive(h, O) && !used(cast, 3, O)) cast[8] = O;
    else {
        u8 k = living_kin(h, O, cast, 3);
        cast[8] = k != H_NONE ? k : follower(s, cf, RO_PRIEST, H_NONE);
        if (k == H_NONE && h->actor[O].child == H_NONE) parent(h, O, cast[8]);
    }
    u8 hand = H_NONE;
    if (h->keyholder != O && alive(h, h->keyholder) && !used(cast, 9, h->keyholder) && h->ev[h->ev_open].type == EV_OPEN &&
        h->keyholder != goat)
        hand = h->keyholder;
    if (hand == H_NONE) {
        u8 skip[10];
        memcpy(skip, cast, 9);
        skip[9] = goat;
        hand = living_kin(h, O, skip, 10);
    }
    if (hand == H_NONE || used(cast, 9, hand)) hand = follower(s, cf, RO_CAPTAIN, cast[8]);
    cast[7] = hand;
    cast[6] = follower(s, cf, RO_SOLDIER, cast[7]);
    /* arc 1: the ones who know the turn */
    u8 p1 = H_NONE;
    u8 cand[6] = { goat, h->keyholder, living_kin(h, goat, cast, 9), h->witness, s->M, s->P };
    for (int i = 0; i < 6 && p1 == H_NONE; i++)
        if (alive(h, cand[i]) && !used(cast, 9, cand[i]) && cand[i] != O) p1 = cand[i];
    if (p1 == H_NONE) {
        p1 = actor(s, F_TRADE, RO_WITNESS, ERA_BEFORE);
        h->actor[p1].sibling = H_NONE;
    }
    cast[5] = p1;
    u8 g1 = H_NONE;
    u8 cand2[4] = { s->M, living_kin(h, goat, cast, 9), living_kin(h, p1, cast, 9), s->P };
    for (int i = 0; i < 4 && g1 == H_NONE; i++)
        if (alive(h, cand2[i]) && !used(cast, 9, cand2[i]) && cand2[i] != O) g1 = cand2[i];
    if (g1 == H_NONE) g1 = follower(s, h->actor[p1].faction, RO_CAPTAIN, p1);
    cast[4] = g1;
    cast[3] = follower(s, h->actor[g1].faction, RO_SOLDIER, g1);
    for (int c = 0; c < H_CAST; c++) h->actor[cast[c]].cast = (i8)c;
    h->arc_faction[0] = cover;
    h->arc_faction[1] = h->actor[cast[5]].faction;
    h->arc_faction[2] = h->actor[cast[8]].faction;
    /* those who walk the halls now: the opener's fate */
    if (alive(h, O)) {
        u8 e = event(s, EV_HIDE, ERA_NOW, O, H_NONE, h->ev_open);
        h->ev[e].sub = (h->actor[O].flags & AF_RETURNED) ? 1 : (h->actor[O].flags & AF_HIDDEN) ? 2 : h->actor[O].motive == MV_POWER ? 0 : 3;
    }
    /* the village: who burned it, on whose word, and why */
    u8 why = H_NONE, sub = 0;
    u8 W = h->witness;
    float w[4] = { W != H_NONE && h->actor[W].faction != cover ? 3.0f : 0, (goat != H_NONE && living_kin(h, goat, NULL, 0) != H_NONE) ? 2.0f : 0,
                   s->pact_ev != H_NONE ? 1.5f : 0, 1.0f };
    switch (pick_weighted(rng_float(r), w, 4)) {
    case 0: why = h->ev_account; sub = 0; break; /* the pages were hidden there */
    case 1: why = h->ev_punish; sub = 1; break;  /* the traitor's child hid there */
    case 2: why = s->pact_ev; sub = 2; break;    /* the bargain wanted it */
    default: why = h->ev_blame; sub = 3; break;  /* to make an example of those who ask */
    }
    u8 b = h->actor[cast[0]].sworn != H_NONE ? h->actor[cast[0]].sworn : cast[1];
    if (rng_chance(r, 0.5f)) b = cast[2];
    u8 e = event(s, EV_BURN, ERA_NOW, cast[0], b, why);
    h->ev[e].sub = sub;
    h->ev_burn = e;
    h->burner = cast[0];
    h->orderer = b;
    h->player_kin = H_NONE;
    if (sub == 0 && h->actor[W].cast < 0 && rng_chance(r, 0.6f)) {
        /* the witness was the player's own grandparent, and died in the fire */
        h->player_kin = W;
        h->actor[W].flags |= AF_KIN_OF_PLAYER;
        die(s, W, ERA_NOW, e);
    }
}

/* ---------- 2. the mystery ---------- */

static int chain_len(const History *h, u8 ev) {
    int n = 0;
    for (u8 e = ev; e != H_NONE && n < 12 && h->ev[e].type != EV_FOUND && h->ev[e].type != EV_KEYS; e = h->ev[e].cause) n++;
    return n;
}

static void find_shape(History *h) {
    memset(h->score, 0, sizeof h->score);
    u8 O = h->opener, goat = h->scapegoat;
    const HActor *o = &h->actor[O];
    int rel = relation(h, O, goat);
    if (goat != H_NONE && kin(h, O, goat)) h->score[SH_BETRAYAL] = 10;
    else if (goat != H_NONE && rel == RL_OATH) h->score[SH_BETRAYAL] = 8;
    else if (h->keyholder != O && kin(h, O, h->keyholder)) h->score[SH_BETRAYAL] = 7;
    if (o->motive == MV_POWER) h->score[SH_USURPATION] = 9;
    if (o->motive == MV_PACT) h->score[SH_PACT] = 9;
    if (h->below == BL_SLEEPER) h->score[SH_BAIT] = o->motive == MV_DUTY ? 10 : 6;
    if (o->motive == MV_DUTY && h->below != BL_SLEEPER) h->score[SH_BAIT] = 7;
    if (goat == H_NONE && h->ev_blame != H_NONE) h->score[SH_WRONG_MONSTER] = 9;
    if (h->tone == TONE_TRAGEDY && (o->motive == MV_FAITH || o->motive == MV_TRUTH)) h->score[SH_MERCY] = 9;
    if (has(o, TT_MERCIFUL) && o->motive != MV_POWER && o->motive != MV_GREED) h->score[SH_MERCY] = MAXI(h->score[SH_MERCY], 6);
    if (h->tone == TONE_HUMAN) h->score[SH_COVERUP] = 7 + (o->motive == MV_TRUTH);
    else if (h->ev[h->ev_secret].type == EV_FEED) h->score[SH_COVERUP] = 5;
    if (h->ev_burn != H_NONE && h->ev[h->ev_burn].sub == 0) h->score[SH_VILLAGE] = h->player_kin != H_NONE ? 9 : 6;
    if (o->flags & AF_HEARD) h->score[SH_VOICE] = 8 + (h->ev[o->mcause].cause != H_NONE);
    if (o->motive == MV_REVENGE && (o->flags & AF_HIDDEN)) h->score[SH_HEIR] = 7;
    if (o->motive == MV_POWER && h->cast[2] != H_NONE && h->actor[h->cast[2]].role == RO_HEIR) h->score[SH_HEIR] = 8;
    if (h->cast[2] != H_NONE && h->actor[h->cast[2]].role == RO_HEIR) h->score[SH_HEIR] = MAXI(h->score[SH_HEIR], 5);
    h->chain = chain_len(h, h->ev_open);
    int order[SH_COUNT];
    for (int i = 0; i < SH_COUNT; i++) order[i] = i;
    /* ties go to a shape the seed picks, so equal scores do not always resolve the same way */
    float key[SH_COUNT];
    for (int i = 0; i < SH_COUNT; i++) key[i] = (float)h->score[i] + hash_unit(hash3(h->seed, i, h->attempt, 0, 0x5A9E)) * 1.5f;
    for (int i = 0; i < SH_COUNT; i++)
        for (int k = i + 1; k < SH_COUNT; k++)
            if (key[order[k]] > key[order[i]]) { int t = order[i]; order[i] = order[k]; order[k] = t; }
    h->shape = (u8)order[0];
    h->shape2 = h->score[order[1]] >= 5 ? (u8)order[1] : H_NONE;
    h->shape3 = h->score[order[2]] >= 5 ? (u8)order[2] : H_NONE;
    h->culprit_role = o->role;
}

static bool dull(const History *h) { return h->chain < 3 || h->score[h->shape] < 8 || h->ev[h->ev_open].type != EV_OPEN; }

/* ---------- 4. names ---------- */

static void name_unique(History *h, int i, u64 seed) {
    HActor *a = &h->actor[i];
    for (int t = 0; t < 12; t++) {
        story_make_name(hash3(seed, i, t, h->attempt, 0x4A3E), a->culture, a->name, H_NAME);
        bool clash = false;
        for (int k = 0; k < i && !clash; k++) clash = !strcmp(h->actor[k].name, a->name);
        clash |= !strcmp(a->name, h->fortress);
        if (!clash) return;
    }
}

static void name_all(History *h, Rng *r) {
    u64 seed = h->seed;
    int cult[3];
    for (int k = 0; k < 3; k++) cult[k] = story_arc_culture(seed, k);
    bool named[H_FACTIONS] = { 0 };
    h->faction[F_BUILD].culture = 0;
    snprintf(h->faction[F_BUILD].name, H_NAME, "%s", h->builders);
    named[F_BUILD] = true;
    int idx = 3;
    for (int k = 0; k < 3; k++) {
        u8 f = h->arc_faction[k];
        if (named[f]) continue;
        h->faction[f].culture = (u8)cult[k];
        named[f] = true;
    }
    for (int f = 1; f < h->nfactions; f++) {
        if (!named[f] || f == F_BUILD) h->faction[f].culture = named[f] ? h->faction[f].culture : (u8)rng_range(r, 0, CULT_COUNT - 1);
        if (f == F_BUILD) continue;
        for (int t = 0; t < 12; t++) {
            story_faction_name(seed ^ (u64)t * 0x51ED, h->faction[f].culture, idx++, h->faction[f].name, H_NAME);
            bool clash = !strcmp(h->faction[f].name, h->builders);
            for (int g = 1; g < f && !clash; g++) clash = !strcmp(h->faction[g].name, h->faction[f].name);
            if (!clash) break;
        }
    }
    for (int i = 0; i < h->nactors; i++) {
        HActor *a = &h->actor[i];
        a->culture = a->cast >= 0 ? (u8)cult[a->cast / 3] : a->faction < h->nfactions ? h->faction[a->faction].culture : 0;
        if (a->born <= ERA_OLD2) a->culture = a->faction == F_BUILD ? 0 : a->culture;
        name_unique(h, i, seed);
    }
    /* places */
    static const char *const SEAL_PLACE[] = { "the Lowest Gate", "the Ninth Door", "the Well of Bells", "the Black Stair",
                                              "the Salt Door", "the Deep Chapel", "the Iron Throat" };
    static const char *const HALL[] = { "the Hall of %s", "%s's Gallery", "the Red Court of %s", "the Cellars of %s", "%s Bridge" };
    snprintf(h->places[0], H_NAME, "%s", PICK(r, SEAL_PLACE));
    for (int p = 1; p < h->nplaces; p++) {
        char n[32];
        story_make_name(rng_next(r), CULT_OLD, n, sizeof n);
        snprintf(h->places[p], H_NAME, PICK(r, HALL), n);
    }
    /* artifacts */
    static const char *const KEY[] = { "Iron", "Salt", "Bone", "Glass", "Silver", "Black", "Ash", "Amber" };
    int k0 = rng_range(r, 0, LEN(KEY) - 1);
    for (int i = 0; i < h->narts; i++) {
        HArtifact *a = &h->art[i];
        const char *maker = a->maker != H_NONE ? h->actor[a->maker].name : "someone";
        switch (a->kind) {
        case AK_KEY: snprintf(a->name, H_NAME, "the %s Key", KEY[(k0 + i * 3) % LEN(KEY)]); break;
        case AK_HOARD: {
            static const char *const T[] = { "the Hoard of %s", "%s's Treasury", "the Drowned Gold of %s", "the Vault of %s" };
            snprintf(a->name, H_NAME, PICK(r, T), maker);
            break;
        }
        case AK_BOOK: snprintf(a->name, H_NAME, "the Chronicle of %s", maker); break;
        case AK_FORGED: snprintf(a->name, H_NAME, "the Book of %s", h->fall + (strncmp(h->fall, "the ", 4) ? 0 : 4)); break;
        case AK_ACCOUNT: {
            static const char *const T[] = { "the Testament of %s", "%s's Ledger", "the Account of %s", "%s's Book of Days" };
            snprintf(a->name, H_NAME, PICK(r, T), maker);
            break;
        }
        case AK_PACT: snprintf(a->name, H_NAME, "the Bargain of %s", maker); break;
        default: snprintf(a->name, H_NAME, "%s's blade", maker); break;
        }
    }
    /* eras */
    static const char *const OLD[] = { "the War of Fire", "the Drowning", "the Long Rot", "the Great Burial", "the Deep Delving",
                                       "the Hardening", "the Chaining", "the First Hearths" };
    snprintf(h->era_name[ERA_FOUND], H_NAME, "%s", h->tone == TONE_HUMAN ? "the Founding" : "the Sealing");
    int o1 = rng_range(r, 0, LEN(OLD) - 1), o2 = (o1 + rng_range(r, 1, LEN(OLD) - 1)) % LEN(OLD);
    snprintf(h->era_name[ERA_OLD1], H_NAME, "%s", OLD[o1]);
    snprintf(h->era_name[ERA_OLD2], H_NAME, "%s", OLD[o2]);
    snprintf(h->era_name[ERA_BEFORE], H_NAME, "the last years of %.28s", h->fortress);
    snprintf(h->era_name[ERA_FALL], H_NAME, "%s", h->fall);
    snprintf(h->era_name[ERA_NOW], H_NAME, "these days");
}

/* ---------- 5. facts, revelations, clues, questions ---------- */

static int add_fact(History *h, u8 kind, u8 ev, u8 a, u8 b, u8 holder, u8 weight) {
    for (int i = 0; i < h->nfacts; i++) {
        HFact *f = &h->fact[i];
        if (f->kind == kind && f->ev == ev && f->a == a && f->b == b) {
            if (weight > f->weight) f->weight = weight;
            return i;
        }
    }
    if (h->nfacts >= H_FACTS) return -1;
    HFact *f = &h->fact[h->nfacts];
    f->kind = kind;
    f->ev = ev;
    f->a = a;
    f->b = b;
    f->holder = holder;
    f->weight = weight;
    f->order = H_NONE;
    return h->nfacts++;
}

static int add_clue(History *h, int fact, u8 src, u8 speaker, u8 chapter, u8 dist) {
    if (fact < 0 || h->nclues >= H_CLUES) return -1;
    HClue *c = &h->clue[h->nclues];
    c->fact = (u8)fact;
    c->src = src;
    c->speaker = speaker;
    c->chapter = chapter;
    c->dist = dist;
    return h->nclues++;
}

static float chapter_dist(int c) {
    static const float FRAC[3] = { 0.42f, 0.7f, 1.0f };
    if (c == 0) return 3.5f;
    return (12.0f + 16.0f * (float)(c / 3)) * FRAC[c % 3];
}

static int find_fact(const History *h, u8 kind, u8 ev) {
    for (int i = 0; i < h->nfacts; i++)
        if (h->fact[i].kind == kind && h->fact[i].ev == ev) return i;
    return -1;
}

static void build_facts(History *h, Rng *r) {
    u8 O = h->opener;
    /* the founding: what they say, and what is true */
    u8 sec = h->ev_secret;
    if (h->ev[sec].type != EV_SEAL) {
        add_fact(h, FK_CLAIM, h->ev_founding, h->ev[h->ev_founding].a, H_NONE, F_BUILD, 2); /* "they sealed it" */
        add_fact(h, FK_DEED, sec, h->ev[sec].a, H_NONE, H_NONE, 3);
    } else {
        add_fact(h, FK_DEED, sec, h->ev[sec].a, H_NONE, H_NONE, 2);
    }
    /* the chain of the fall, and everything else, by its event */
    for (int i = 0; i < h->nevents; i++) {
        HEvent *e = &h->ev[i];
        u8 w = 1;
        switch (e->type) {
        case EV_SEAL: case EV_BAIT: case EV_IMPRISON: case EV_BURY: case EV_FEED: if ((u8)i != sec) add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 1); break;
        case EV_FOUND: case EV_KEYS: case EV_WAR: case EV_MASSACRE: case EV_HERESY: case EV_PURGE: case EV_HOARD: case EV_CHRONICLE:
        case EV_WEAPON: case EV_TAKE_RULE: case EV_ROLL: case EV_DEBT: case EV_CONVERT: case EV_RIVAL: case EV_EXILE: case EV_LEARN:
        case EV_SILENCE: case EV_LOSS: case EV_VOICE: case EV_PACT:
            add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, w);
            break;
        case EV_STEAL: case EV_GIVE: add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 2); break;
        case EV_DESCEND: case EV_RETURN: case EV_SEIZE: case EV_FLEE: add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 2); break;
        case EV_DIE: if (e->b != H_NONE && h->actor[e->b].role <= RO_KEEPER) add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 1); break;
        case EV_PUNISH: add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 2); break;
        case EV_FORGE: add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 2); break;
        case EV_ACCOUNT: add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 1); break;
        case EV_BURN: add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 2); break;
        case EV_SWEAR: add_fact(h, FK_DEED, (u8)i, e->a, e->b, H_NONE, 1); break;
        default: break;
        }
    }
    /* raise what the fall's chain runs through */
    for (u8 e = h->ev_open, n = 0; e != H_NONE && n < 12 && h->ev[e].type != EV_FOUND && h->ev[e].type != EV_KEYS; e = h->ev[e].cause, n++) {
        int f = find_fact(h, FK_DEED, e);
        if (f >= 0) h->fact[f].weight = 3;
        if (n == 1) h->ev_why = e;
        h->ev_deep = e;
    }
    add_fact(h, FK_DEED, h->ev_open, O, h->keyholder, H_NONE, 3);
    add_fact(h, FK_ANON, h->ev_open, H_NONE, H_NONE, H_NONE, 2);
    add_fact(h, FK_WHY, h->ev_open, O, H_NONE, H_NONE, 3);
    add_fact(h, FK_CLAIM, h->ev_blame, h->ev[h->ev_blame].a, h->scapegoat, h->cover, 3);
    add_fact(h, FK_CRACK, h->ev_blame, h->scapegoat, O, H_NONE, 3);
    if (h->ev_burn != H_NONE) add_fact(h, FK_WHY, h->ev_burn, h->burner, h->orderer, H_NONE, 2);
    if (alive(h, O)) add_fact(h, FK_FATE, h->ev_open, O, H_NONE, H_NONE, 2);
    /* kin among those who matter */
    u8 who[6] = { O, h->scapegoat, h->keyholder, h->witness, h->cast[2], h->cast[8] };
    for (int i = 0; i < 6; i++)
        for (int k = 0; k < 6; k++) {
            if (who[i] == H_NONE || who[k] == H_NONE || who[i] >= who[k]) continue;
            if (kin(h, who[i], who[k])) add_fact(h, FK_KIN, H_NONE, who[i], who[k], H_NONE, (i == 0 || k == 0) ? 3 : 2);
        }
    for (int c = 0; c < H_CAST; c++) {
        u8 a = h->cast[c];
        if (h->actor[a].parent != H_NONE && h->actor[a].born >= ERA_BEFORE && h->actor[h->actor[a].parent].role != RO_KIN)
            add_fact(h, FK_KIN, H_NONE, h->actor[a].parent, a, H_NONE, 1);
    }

    /* ---- revelations: the false belief, the crack, the turn, the cause, the cost ---- */
    int F_claim = find_fact(h, FK_CLAIM, h->ev_blame), F_crack = find_fact(h, FK_CRACK, h->ev_blame),
        F_open = find_fact(h, FK_DEED, h->ev_open), F_anon = find_fact(h, FK_ANON, h->ev_open), F_why = find_fact(h, FK_WHY, h->ev_open),
        F_secret = find_fact(h, FK_DEED, h->ev_secret), F_punish = h->ev_punish != H_NONE ? find_fact(h, FK_DEED, h->ev_punish) : -1,
        F_forge = -1, F_key = -1, F_fate = find_fact(h, FK_FATE, h->ev_open), F_kin = -1, F_deep = -1, F_burnwhy = find_fact(h, FK_WHY, h->ev_burn),
        F_found_claim = find_fact(h, FK_CLAIM, h->ev_founding), F_root = -1;
    for (int i = 0; i < h->nfacts; i++) {
        HFact *f = &h->fact[i];
        if (f->kind == FK_DEED && f->ev != H_NONE && h->ev[f->ev].type == EV_FORGE) F_forge = i;
        if (f->kind == FK_DEED && f->ev != H_NONE && (h->ev[f->ev].type == EV_STEAL || h->ev[f->ev].type == EV_GIVE)) F_key = i;
        if (f->kind == FK_KIN && (f->a == O || f->b == O) && (F_kin < 0 || f->a == h->scapegoat || f->b == h->scapegoat)) F_kin = i;
    }
    if (h->ev_deep != H_NONE && h->ev_deep != h->ev_open) F_deep = find_fact(h, FK_DEED, h->ev_deep);
    if (h->ev_why != H_NONE) F_root = find_fact(h, FK_DEED, h->ev_why);
    if (F_deep == F_root) F_deep = -1;
    if (F_deep < 0 || F_deep == F_secret) F_deep = F_secret;
    /* a slot list per chapter, first unused wins */
    int slots[H_CAST][5] = {
        { F_claim, F_found_claim, F_punish, F_anon, -1 },
        { F_punish, F_forge, F_anon, F_found_claim, -1 },
        { F_crack, F_anon, F_forge, -1, -1 },
        { F_forge, F_burnwhy, F_root, F_found_claim, -1 },
        { F_key, F_kin, F_anon, F_root, -1 },
        { F_open, -1, -1, -1, -1 },
        { F_root, F_kin, F_why, -1, -1 },
        { F_deep, F_secret, F_kin, F_fate, -1 },
        { h->cast[8] == O ? F_why : F_fate, F_why, F_secret, F_kin, F_burnwhy },
    };
    /* shapes move the emphasis */
    if (h->shape == SH_BETRAYAL && F_kin >= 0) { slots[4][0] = F_kin; slots[4][1] = F_key; }
    if (h->shape == SH_COVERUP) { slots[1][0] = F_forge; slots[3][0] = F_punish; }
    if (h->shape == SH_VILLAGE && F_burnwhy >= 0) slots[3][0] = F_burnwhy;
    if (h->shape == SH_VOICE || h->shape == SH_PACT) slots[6][0] = F_root >= 0 ? F_root : F_why;
    bool taken[H_FACTS] = { 0 };
    for (int c = 0; c < H_CAST; c++) {
        int f = -1;
        for (int k = 0; k < 5 && f < 0; k++)
            if (slots[c][k] >= 0 && !taken[slots[c][k]]) f = slots[c][k];
        if (f < 0) /* any weighty fact left */
            for (int i = 0; i < h->nfacts && f < 0; i++)
                if (!taken[i] && h->fact[i].weight >= 2 && h->fact[i].kind != FK_CLAIM) f = i;
        if (f < 0) f = F_anon;
        taken[f] = true;
        h->fact[f].order = (u8)c;
        u8 src = (c % 3 == 2 || c == 5) ? SRC_DYING : (rng_chance(r, 0.5f) ? SRC_DYING : SRC_BODY);
        if (h->fact[f].kind == FK_CLAIM) src = SRC_BODY;
        h->reveal[c] = (u8)add_clue(h, f, src, h->cast[c], (u8)c, 0);
    }
    /* the truth motive's pages, and the hunters' orders: other facts, in the same order */
    int pool[H_FACTS], np = 0;
    for (int i = 0; i < h->nfacts; i++)
        if (!taken[i] && h->fact[i].weight >= 2 && h->fact[i].kind != FK_CLAIM) pool[np++] = i;
    for (int i = 0; i < h->nfacts && np < 12; i++)
        if (!taken[i] && h->fact[i].weight == 1 && h->fact[i].kind != FK_CLAIM && h->fact[i].ev != H_NONE && h->ev[h->fact[i].ev].type != EV_SWEAR)
            pool[np++] = i;
    for (int c = 0; c < H_CAST; c++) {
        int f = np ? pool[c % np] : h->clue[h->reveal[c]].fact;
        if (np && h->fact[f].order == H_NONE) h->fact[f].order = (u8)MINI(c + 1, H_CAST - 1);
        h->page[c] = (u8)add_clue(h, f, SRC_PAGE, h->witness, (u8)c, 0);
        /* orders: who the target answers to */
        int sw = -1;
        for (int i = 0; i < h->nfacts && sw < 0; i++)
            if (h->fact[i].kind == FK_DEED && h->fact[i].ev != H_NONE && h->ev[h->fact[i].ev].type == EV_SWEAR && h->fact[i].a == h->cast[c]) sw = i;
        if (sw < 0) sw = c < 3 && F_burnwhy >= 0 ? F_burnwhy : h->clue[h->reveal[c]].fact;
        h->orders[c] = (u8)add_clue(h, sw, SRC_ORDERS, h->cast[c], (u8)c, 0);
    }
    /* traces: stones carry the old and the partisan, notes the witnessed; never too early */
    for (int i = 0; i < h->nfacts; i++) {
        HFact *f = &h->fact[i];
        u8 era = f->ev != H_NONE ? h->ev[f->ev].era : ERA_BEFORE;
        float d = f->order != H_NONE ? chapter_dist(f->order) * (f->weight >= 3 ? 0.55f : 0.35f) : (era <= ERA_OLD2 ? 0.0f : 2.0f);
        if (i == F_open || i == F_why || i == F_fate) d = MAXI(d, chapter_dist(3));
        if (i == F_secret && h->ev[h->ev_secret].type != EV_SEAL) d = MAXI(d, chapter_dist(4) * 0.5f); /* the founding's secret: not by the gate */
        if (f->weight >= 3 && f->order == H_NONE) d = MAXI(d, chapter_dist(2) * 0.5f);
        int copies = f->weight >= 3 ? 3 : f->weight == 2 ? 2 : 1;
        if (f->ev != H_NONE && h->ev[f->ev].type == EV_SWEAR) copies = 0;
        for (int k = 0; k < copies; k++) {
            u8 src;
            if (f->kind == FK_CLAIM) src = k == 0 ? SRC_STELE : SRC_BOUNTY;
            else if (f->kind == FK_KIN) src = SRC_STELE;
            else if (f->kind == FK_CRACK || f->kind == FK_WHY || f->kind == FK_FATE) src = SRC_NOTE;
            else if (era <= ERA_OLD2) src = k != 1 ? SRC_STELE : SRC_NOTE;
            else src = k != 1 ? SRC_NOTE : SRC_STELE;
            add_clue(h, i, src, H_NONE, H_NONE, (u8)MINI(255, (int)(d * (1.0f + 0.5f * k))));
        }
    }
    /* legendaries: the blades of those who matter carry their deeds; the old blades their makers */
    int legend[UNIQUE_SLOTS];
    for (int i = 0; i < UNIQUE_SLOTS; i++) legend[i] = i;
    for (int i = UNIQUE_SLOTS - 1; i > 0; i--) {
        int k = rng_range(r, 0, i), t = legend[i];
        legend[i] = legend[k];
        legend[k] = t;
    }
    int li = 0;
    for (int c = 0; c < H_CAST; c++) {
        u8 a = h->cast[c];
        int f = -1;
        for (int i = 0; i < h->nfacts && f < 0; i++) /* their best deed */
            if (h->fact[i].kind == FK_DEED && h->fact[i].a == a && h->fact[i].ev != H_NONE && h->ev[h->fact[i].ev].type != EV_SWEAR) f = i;
        for (int i = 0; i < h->nfacts && f < 0; i++) /* or whom they serve */
            if (h->fact[i].kind == FK_DEED && h->fact[i].a == a) f = i;
        for (int i = 0; i < h->nfacts && f < 0; i++) /* or whose kin they are */
            if (h->fact[i].kind == FK_KIN && (h->fact[i].a == a || h->fact[i].b == a)) f = i;
        if (f < 0) f = add_fact(h, FK_FATE, H_NONE, a, H_NONE, H_NONE, 1); /* only that they live */
        if (li >= UNIQUE_SLOTS) break;
        add_clue(h, f, SRC_LEGEND, a, (u8)legend[li], 0);
        h->cast_legend[c] = (u8)legend[li++];
    }
    for (int i = 0; i < h->narts && li < UNIQUE_SLOTS; i++) {
        if (h->art[i].kind != AK_WEAPON) continue;
        h->art[i].legend = (i8)legend[li];
        int f = find_fact(h, FK_DEED, h->art[i].event);
        add_clue(h, f, SRC_LEGEND, H_NONE, (u8)legend[li++], 0);
    }
    /* questions */
    h->nq = 0;
    HQuestion *q;
    q = &h->q[h->nq++];
    q->kind = Q_WHO; q->need[0] = (u8)F_open; q->nneed = 1; q->alt = (u8)F_why; /* the reason names the opener too */
    if (h->scapegoat != H_NONE) {
        q = &h->q[h->nq++];
        q->kind = Q_GUILTY; q->need[0] = (u8)F_crack; q->nneed = 1; q->alt = (u8)F_open;
    } else {
        q = &h->q[h->nq++];
        q->kind = Q_GUILTY; q->need[0] = (u8)F_crack; q->nneed = 1; q->alt = (u8)F_open; /* did it come up by itself? */
    }
    q = &h->q[h->nq++];
    q->kind = Q_WHY; q->need[0] = (u8)F_why; q->nneed = 1; q->alt = F_root >= 0 ? (u8)F_root : H_NONE;
    q = &h->q[h->nq++];
    q->kind = Q_BELOW; q->need[0] = (u8)F_secret; q->nneed = 1; q->alt = H_NONE;
    if (F_burnwhy >= 0) {
        q = &h->q[h->nq++];
        q->kind = Q_VILLAGE; q->need[0] = (u8)F_burnwhy; q->nneed = 1; q->alt = H_NONE;
    }
    if (F_fate >= 0) {
        q = &h->q[h->nq++];
        q->kind = Q_WHERE; q->need[0] = (u8)F_fate; q->nneed = 1; q->alt = H_NONE;
    }
}

/* ---------- the whole ---------- */

static void gen_once(History *h, u64 seed, int attempt) {
    memset(h, 0, sizeof *h);
    h->seed = seed;
    h->attempt = attempt;
    Sim s;
    memset(&s, 0, sizeof s);
    s.h = h;
    s.r = rng_make(hash3(seed, attempt, 0, 0, 0x4157));
    s.ruling = F_BUILD;
    s.pact_ev = s.pact_maker = s.war_ev = s.learned = s.silenced = s.D = H_NONE;
    h->ev_founding = h->ev_secret = h->ev_open = h->ev_blame = h->ev_punish = h->ev_burn = h->ev_why = h->ev_deep = h->ev_hoard =
        h->ev_account = H_NONE;
    h->opener = h->scapegoat = h->keyholder = h->burner = h->orderer = h->witness = h->player_kin = H_NONE;
    StoryWorld sw;
    story_world(seed, &sw);
    snprintf(h->fortress, H_NAME, "%s", sw.fortress);
    snprintf(h->builders, H_NAME, "%s", sw.builders);
    snprintf(h->sealed, H_NAME, "%s", sw.sealed);
    snprintf(h->fall, H_NAME, "%s", sw.fall);
    snprintf(h->night, sizeof h->night, strstr(sw.fall, "Night") ? "%s" : "the night of %s", sw.fall);
    /* the tone is the world's, not the attempt's */
    Rng tr = rng_make(seed ^ 0x70E5);
    static const float TW[TONE_COUNT] = { 55, 25, 20 };
    h->tone = (u8)pick_weighted(rng_float(&tr), TW, TONE_COUNT);
    float u = rng_float(&tr);
    h->below = h->tone == TONE_HORROR ? (u < 0.6f ? BL_HUNGER : BL_SLEEPER) : h->tone == TONE_HUMAN ? (u < 0.6f ? BL_MINERS : BL_PLAGUE)
                                                                                                    : (u < 0.55f ? BL_FIRST : BL_KING);
    h->nfactions = F_FIXED;
    static const u8 CREED[F_FIXED] = { CRD_BUILDERS, CRD_KEEPERS, CRD_KEEPERS, CRD_KEEPERS, CRD_GUARD, CRD_TRADERS, CRD_CULT };
    for (int f = 0; f < F_FIXED; f++) {
        h->faction[f].creed = CREED[f];
        h->faction[f].leader = H_NONE;
    }
    h->nplaces = 0;
    sim_found(&s);
    sim_old(&s);
    sim_before(&s);
    sim_fall(&s);
    sim_now(&s);
    find_shape(h);
    name_all(h, &s.r);
    build_facts(h, &s.r);
}

void history_generate(u64 seed, History *out) {
    for (int attempt = 0; attempt < 8; attempt++) {
        gen_once(out, seed, attempt);
        if (!dull(out)) return;
    }
    /* none was good enough: the best of the dull is the last */
}

#define HCACHE 4
static History g_cache[HCACHE];
static bool g_cached[HCACHE];
static int g_next;

const History *history_of(u64 seed) {
    for (int i = 0; i < HCACHE; i++)
        if (g_cached[i] && g_cache[i].seed == seed) return &g_cache[i];
    int i = g_next;
    g_next = (g_next + 1) % HCACHE;
    history_generate(seed, &g_cache[i]);
    g_cached[i] = true;
    return &g_cache[i];
}

/* ---------- the validator ---------- */

static void problem(History *h, const char *msg) {
    size_t n = strlen(h->problems);
    if (n + strlen(msg) + 3 < sizeof h->problems) snprintf(h->problems + n, sizeof h->problems - n, "%s%s", n ? "; " : "", msg);
}

bool history_validate(History *h) {
    h->problems[0] = 0;
    int count[H_FACTS] = { 0 };
    for (int i = 0; i < h->nclues; i++) count[h->clue[i].fact]++;
    for (int i = 0; i < h->nfacts; i++)
        if (h->fact[i].weight >= 3 && count[i] < 3) {
            char m[64];
            snprintf(m, sizeof m, "fact %d (key) has %d clues", i, count[i]);
            problem(h, m);
        }
    /* every lie can be seen through */
    for (int i = 0; i < h->nfacts; i++) {
        if (h->fact[i].kind != FK_CLAIM || h->fact[i].ev != h->ev_blame) continue;
        int crack = find_fact(h, FK_CRACK, h->ev_blame), open = find_fact(h, FK_DEED, h->ev_open);
        if ((crack < 0 || !count[crack]) && (open < 0 || !count[open])) problem(h, "a claim without a crack");
    }
    /* no one acts after their death (the returned excepted) */
    for (int i = 0; i < h->nevents; i++) {
        HEvent *e = &h->ev[i];
        if (e->a == H_NONE || e->type == EV_ACCOUNT || e->type == EV_RETURN || e->type == EV_HIDE) continue;
        HActor *a = &h->actor[e->a];
        if (a->died != H_NONE && e->era > a->died && !(a->flags & AF_RETURNED)) {
            char m[96];
            snprintf(m, sizeof m, "%s acts in era %d after dying in era %d (%s)", a->name, e->era, a->died, EV_NAMES[e->type]);
            problem(h, m);
        }
    }
    /* the cast: distinct, alive */
    for (int c = 0; c < H_CAST; c++) {
        if (h->cast[c] == H_NONE) { problem(h, "a chapter without a target"); continue; }
        if (!alive(h, h->cast[c])) problem(h, "a dead chapter target");
        for (int k = 0; k < c; k++)
            if (h->cast[k] == h->cast[c]) problem(h, "the same target twice");
    }
    /* every question can be answered */
    for (int q = 0; q < h->nq; q++) {
        bool ok = true;
        for (int k = 0; k < h->q[q].nneed; k++) ok &= h->q[q].need[k] < h->nfacts && count[h->q[q].need[k]] > 0;
        if (!ok && h->q[q].alt != H_NONE) ok = count[h->q[q].alt] > 0;
        if (!ok) problem(h, "a question with no answer");
    }
    /* names are distinct */
    for (int i = 0; i < h->nactors; i++)
        for (int k = 0; k < i; k++)
            if (!strcmp(h->actor[i].name, h->actor[k].name)) { problem(h, "two people share a name"); i = h->nactors; break; }
    /* every text fits */
    char buf[512];
    for (int i = 0; i < h->nclues; i++) {
        HPersonal p = { "Aldmere", "sister" };
        history_clue_text(h, i, &p, buf, sizeof buf);
        if ((int)strlen(buf) >= 319 || !buf[0]) {
            char m[64];
            snprintf(m, sizeof m, "clue %d text %s", i, buf[0] ? "too long" : "empty");
            problem(h, m);
        }
    }
    return h->problems[0] == 0;
}
