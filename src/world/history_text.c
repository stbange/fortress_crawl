#include "world/history.h"
#include "world/story.h"
#include "world/story_text.h"

#include <stdio.h>
#include <string.h>

/* The history's text: every event has a small grammar in three forms (told about the doer,
 * told by the doer, told without knowing who did it), and every source of a clue has its
 * voices (a stone of a faction, a dead delver's note, dying words, a ledger, a page). Picks
 * among alternatives come from an RNG seeded by the clue, so a text never changes. */

#define LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define PICK(r, a) (a)[rng_range((r), 0, LEN(a) - 1)]

enum { FORM_THIRD, FORM_FIRST, FORM_ANON };

typedef struct TCtx {
    Ctx c;
    char a[H_NAME + 24], b[H_NAME + 24], f1[H_NAME], f2[H_NAME], art[H_NAME], place[H_NAME], era[H_NAME];
    char rel[16], relb[16], child[16], village[H_NAME], kin[24], how[64], price[96], holder[H_NAME];
} TCtx;

static const char *rel_word(const History *h, u8 a, u8 b) {
    /* what b is to a */
    if (a == H_NONE || b == H_NONE) return "kin";
    const HActor *x = &h->actor[a], *y = &h->actor[b];
    bool f = y->female;
    if (x->sibling == b) return f ? "sister" : "brother";
    if (x->spouse == b) return f ? "wife" : "husband";
    if (x->parent == b) return y->born < ERA_BEFORE ? "forebear" : f ? "mother" : "father";
    if (x->child == b) return f ? "daughter" : "son";
    if (x->sworn == b) return "liege";
    if (y->sworn == a) return "sworn blade";
    return "kin";
}

static void actor_name(const History *h, u8 a, const HPersonal *p, char *out, int n) {
    if (a == H_NONE) { snprintf(out, (size_t)n, "someone"); return; }
    const HActor *x = &h->actor[a];
    if ((x->flags & AF_KIN_OF_PLAYER) && p) snprintf(out, (size_t)n, "your %s %s", x->female ? "grandmother" : "grandfather", x->name);
    else snprintf(out, (size_t)n, "%s", x->name);
}

static void tctx(const History *h, int ev, u8 a, u8 b, const HPersonal *p, TCtx *t) {
    memset(t, 0, sizeof *t);
    Ctx *c = &t->c;
    const HEvent *e = ev >= 0 && ev < h->nevents ? &h->ev[ev] : NULL;
    if (e && a == H_NONE) a = e->a;
    if (e && b == H_NONE) b = e->b;
    actor_name(h, a, p, t->a, sizeof t->a);
    actor_name(h, b, p, t->b, sizeof t->b);
    snprintf(t->f1, sizeof t->f1, "%s", e && e->f1 != H_NONE ? h->faction[e->f1].name : a != H_NONE ? h->faction[h->actor[a].faction].name : "they");
    snprintf(t->f2, sizeof t->f2, "%s", e && e->f2 != H_NONE ? h->faction[e->f2].name : "their enemies");
    snprintf(t->art, sizeof t->art, "%s", e && e->art != H_NONE ? h->art[e->art].name : "a key");
    snprintf(t->place, sizeof t->place, "%s", e && e->place != H_NONE ? h->places[e->place] : h->places[0]);
    snprintf(t->era, sizeof t->era, "%s", e ? h->era_name[e->era] : "the old days");
    snprintf(t->rel, sizeof t->rel, "%s", rel_word(h, a, b));
    snprintf(t->relb, sizeof t->relb, "%s", rel_word(h, b, a));
    snprintf(t->child, sizeof t->child, "%s", b != H_NONE && h->actor[b].female ? "daughter" : "son");
    snprintf(t->village, sizeof t->village, "%s", p && p->village ? p->village : "the village");
    snprintf(t->kin, sizeof t->kin, "%s", p && p->kin ? p->kin : "kin");
    story_cset(c, "a", t->a);
    story_cset(c, "b", t->b);
    story_cset(c, "f1", t->f1);
    story_cset(c, "f2", t->f2);
    story_cset(c, "art", t->art);
    story_cset(c, "place", t->place);
    story_cset(c, "era", t->era);
    story_cset(c, "rel", t->rel);
    story_cset(c, "relb", t->relb);
    story_cset(c, "child", t->child);
    story_cset(c, "fort", h->fortress);
    story_cset(c, "builders", h->builders);
    story_cset(c, "sealed", h->sealed);
    story_cset(c, "fall", h->fall);
    story_cset(c, "night", h->night);
    story_cset(c, "village", t->village);
    story_cset(c, "kin", t->kin);
    story_cset(c, "seal", h->places[0]);
    story_cset(c, "how", t->how);
    story_cset(c, "price", t->price);
    story_cset(c, "holder", t->holder);
    if (e) {
        static const char *const HOW[4] = { "to the fever", "down a shaft in the lower works", "to the winter tithe", "to the dark below the seal" };
        snprintf(t->how, sizeof t->how, "%s", HOW[e->sub & 3]);
        if (e->type == EV_PACT) {
            static const char *const PRICE[3] = { "vengeance on %s", "victory over %s", "a hundred years without war against %s" };
            snprintf(t->price, sizeof t->price, PRICE[e->sub % 3], t->f2);
        }
    }
}

/* ---------- the grammar of events ---------- */

/* [type][form][alternative] */
static const char *const DEED[EV_COUNT][3][2] = {
    [EV_FOUND] = { { "{a} of {f1} raised {fort} in {era}", "{f1} built {fort}, and {a} laid the first stone" },
                   { "I raised {fort}", "I laid the first stone of {fort}" },
                   { "{fort} was raised in {era}", "{f1} built {fort} in {era}" } },
    [EV_SEAL] = { { "{a} sealed {sealed} beneath {fort}", "{a} of {f1} shut {sealed} under {fort} at {seal}" },
                  { "I sealed {sealed} beneath {fort}", "I shut it in at {seal} with my own hands" },
                  { "{sealed} was sealed beneath {fort} in {era}", "{f1} shut {sealed} under {fort}" } },
    [EV_BAIT] = { { "{a} built {fort} as a lure for {sealed}, not a cage", "{a} never meant {fort} to hold {sealed}. It was built to call it up" },
                  { "I built {fort} to call {sealed} up, not to keep it down", "it was never a cage. I built a lure" },
                  { "{fort} was built to bring {sealed} up, not to keep it down", "{fort} is no cage. It is bait" } },
    [EV_IMPRISON] = { { "{a} walled the first people of the mountain below {fort} and named them {sealed}",
                        "{f1} drove the first people under the mountain, walled them in, and called them {sealed}" },
                      { "I walled the first people below and gave them a monster's name", "I made them {sealed}. They were only people" },
                      { "the ones below were people once. {f1} walled them in and called them {sealed}",
                        "{sealed} is a name {f1} gave to the people they walled in" } },
    [EV_BURY] = { { "{a} sealed the lower shaft with the miners still alive behind it, and made up {sealed} so no one would dig",
                    "{f1} shut the lower shaft on living miners and told the world of {sealed}" },
                  { "I sealed the shaft with the miners alive. I made {sealed} up myself", "I heard them knock for three days. Then I invented {sealed}" },
                  { "the lower shaft was sealed with living miners behind it. {sealed} is a story to keep the diggers out",
                    "the knocking below stopped after three days. Then they told of {sealed}" } },
    [EV_KEYS] = { { "{a} made three keys to {seal} and gave them to three houses", "{a} trusted the seal to three keys in three houses" },
                  { "I made three keys and trusted three houses", "three keys, three houses. I thought that was enough" },
                  { "there were three keys to {seal}, in three houses", "three houses kept the keys to {seal}" } },
    [EV_FEED] = { { "{f1} fed {sealed} one child of {fort} every winter to keep it asleep", "{a} began the winter tithe: a child for {sealed}, every year" },
                  { "we fed it. One child each winter. It kept the peace", "I gave it a child every winter, and it slept" },
                  { "every winter a child of {fort} went down to {sealed}, and did not come back", "the winter tithe was a child, given to {sealed}" } },
    [EV_WAR] = { { "{f1} made war on {f2} in {era}", "{a} led {f1} against {f2}" },
                 { "I led {f1} against {f2}", "I made the war on {f2}" },
                 { "in {era} {f1} and {f2} went to war", "{f1} fought {f2} in {era}" } },
    [EV_MASSACRE] = { { "{f1} put {f2} to the sword in {place}", "{a} gave the order in {place}, and none of {f2} walked out" },
                      { "I gave the order in {place}. None of {f2} walked out", "I put {f2} to the sword in {place}" },
                      { "in {place}, {f2} died to the last", "no one of {f2} came out of {place} alive" } },
    [EV_HERESY] = { { "{a} began to pray to {sealed}, and {f1} followed", "{a} founded {f1}, who pray to {sealed}" },
                    { "I prayed to it, and it answered", "I heard {sealed}, and I taught others to listen" },
                    { "{f1} prayed to {sealed} in secret", "in {era} some began to pray to {sealed}" } },
    [EV_PURGE] = { { "{a} burned the faithful of {f2}", "{f1} hunted {f2} through the halls and burned them" },
                   { "I burned them, every one", "I burned the faithful of {f2}" },
                   { "the faithful of {f2} were burned in {era}", "{f2} were hunted and burned" } },
    [EV_PACT] = { { "{a} of {f1} made a bargain with {sealed} for {price}. The price: one night with the gate open",
                    "{a} bargained with {sealed} for {price}, and promised it one open night" },
                  { "I bargained with {sealed} for {price}, and promised it one night with the gate open",
                    "I sold it one open night for {price}" },
                  { "someone of {f1} bargained with {sealed}. The price was one night with the gate open",
                    "there is a bargain with {sealed}, made in {era}, and it falls due" } },
    [EV_HOARD] = { { "{a} hid the gold of {f1} in {art}, out of the reach of {f2}", "{a} took the gold of {f1} into the dark, away from {f2}" },
                   { "I hid our gold where {f2} would never look", "I hid the gold of {f1} myself" },
                   { "the gold of {f1} was hidden in {era}", "{f1} hid their gold from {f2}" } },
    [EV_CHRONICLE] = { { "{a} wrote the chronicle of {f1}, and wrote what {f1} wanted", "{a} set down the founding of {fort} for {f1}" },
                       { "I wrote the chronicle as I was told to", "I wrote the founding down. Not all of it" },
                       { "the chronicle of {f1} was written in {era}", "{f1} wrote down their founding in {era}" } },
    [EV_WEAPON] = { { "{a} forged a blade for {b} in {era}", "{a} of {f1} made a blade for {b}" },
                    { "I forged it for {b}", "I made this for {b}" },
                    { "a blade was forged for {b} in {era}", "{b}'s blade was forged in {era}" } },
    [EV_TAKE_RULE] = { { "{f1} took {fort} from {builders}", "{a} of {f1} took {fort} from {builders}" },
                       { "I took {fort} from {builders}", "I took the seat of {builders}" },
                       { "{builders} lost {fort} in {era}", "in {era} {fort} passed from {builders}" } },
    [EV_ROLL] = { { "{f1} began the Roll of the Dead by the gate", "{a} carved the first name on the Roll of the Dead" },
                  { "I carved the first name on the Roll", "I began the Roll of the Dead" },
                  { "the Roll of the Dead was begun in {era}", "the first names on the Roll were carved in {era}" } },
    [EV_BIRTH] = { { "{b} was born to {a}", "{a} had a {child}, {b}" },
                   { "my {child} {b} was born", "I had a {child}" },
                   { "a child was born to {a}", "{b} was born in {era}" } },
    [EV_MARRY] = { { "{a} married {b}", "{a} and {b} were wed" },
                   { "I married {b}", "I wed {b}" },
                   { "{a} and {b} were wed", "a marriage joined {f1} and {f2}" } },
    [EV_OATH] = { { "{a} swore to serve {b}", "{a} took an oath to {b}" },
                  { "I swore to serve {b}", "I gave {b} my oath" },
                  { "{a} was sworn to {b}", "the captain was sworn to {b}" } },
    [EV_LOSS] = { { "{a} lost a {child}, {b}, {how}", "{b}, {a}'s {child}, died {how}" },
                  { "I lost my {child}, {b}, {how}", "my {child} {b} died {how}" },
                  { "{b}, the child of {a}, died {how}", "a child of {fort} died {how}" } },
    [EV_VOICE] = { { "{a} heard {b}'s voice beneath {seal}, calling", "{a} heard the dead {child} {b} calling from below the seal" },
                   { "I heard {b} calling from below the seal. My {child}. Calling", "{b} called me from beneath {seal}. Every night" },
                   { "someone heard a dead child calling from beneath {seal}", "a dead child's voice called from below the seal" } },
    [EV_LEARN] = { { "{a} found out what lies beneath {fort}", "{a} read the old books and learned what lies below" },
                   { "I found the truth in the oldest books", "I learned what lies below. I wish I had not" },
                   { "someone found out what lies beneath {fort}", "someone read what the founders hid" } },
    [EV_SILENCE] = { { "{a} had {b} killed for what {b} knew", "{b} learned too much, and {a} had {b} killed" },
                     { "I had {b} killed. {b} knew too much", "I silenced {b}" },
                     { "{b} was killed for knowing too much", "{b} died for what {b} had read" } },
    [EV_EXILE] = { { "{a} drove {b} out of {fort}", "{a} cast {b} out into the deep" },
                   { "I drove {b} out", "I cast {b} out of {fort}" },
                   { "{b} was driven out of {fort}", "{b} was cast out into the deep" } },
    [EV_DEBT] = { { "{a} owed {f2} more than a keeper's oath was worth", "{a} was deep in debt to {f2}" },
                  { "I owed {f2} everything I had, and then a key", "I owed {f2}. They wanted the key" },
                  { "a keeper of the seal was deep in debt to {f2}", "{f2} held a keeper's debts" } },
    [EV_CONVERT] = { { "{a} went over to {f2}", "{a} joined {f2} in secret" },
                     { "I went over to {f2}", "I joined {f2}. I do not regret it" },
                     { "someone of {f1} went over to {f2}", "{f2} had a friend inside {f1}" } },
    [EV_RIVAL] = { { "{a} wanted {b}'s seat", "{a} meant to take {b}'s place" },
                   { "I wanted {b}'s seat", "I meant to have {b}'s place" },
                   { "someone wanted {b}'s seat", "{b} had an enemy close by" } },
    [EV_STEAL] = { { "{a} stole {art} from {b}, {a}'s own {rel}", "{a} took {art} from {b} while {b} slept" },
                   { "I took {art} from {b} while {b} slept", "I stole my {rel}'s key" },
                   { "{art} was taken from {b} on {night}", "someone took {b}'s key before {fall}" } },
    [EV_GIVE] = { { "{a} gave {art} to {b}", "{a} put {art} in {b}'s hand on {night}" },
                  { "I gave {b} my key. I thought I knew why", "I gave {art} away. To {b}" },
                  { "a keeper gave {art} away on {night}", "{art} changed hands {night}" } },
    [EV_OPEN] = { { "{a} opened {seal} with {art}", "{a} turned {art} in {seal} on {night}" },
                  { "I opened the seal. With {art}, with my own hands", "I turned {art} in {seal}" },
                  { "{seal} was opened from inside, with a key", "on {night}, a key turned in {seal}" } },
    [EV_BREAKOUT] = { { "{sealed} broke the seal from below", "no key turned: {sealed} broke the seal" },
                      { "I saw it break from below", "it broke from below" },
                      { "{sealed} broke the seal from below", "the seal broke from below" } },
    [EV_DESCEND] = { { "{a} went down through the open seal to find {b}", "{a} went down the stair below {seal}, calling {b}'s name" },
                     { "I went down to find {b}", "I went down. {b} was calling" },
                     { "someone went down through the open seal and did not come back", "someone went down below {seal}" } },
    [EV_RETURN] = { { "what came back up wears {a}'s face", "{a} came back up the stair. Not all of {a}" },
                    { "I came back. Most of me", "I came back up. Something came with me" },
                    { "something came back up the stair wearing a face", "not everyone who went down stayed down" } },
    [EV_SEIZE] = { { "{a} took {fort} in the chaos of {fall}", "{a} seized {fort} while it burned" },
                   { "I took {fort} while it burned", "I took the seat that night" },
                   { "someone took {fort} while it burned", "{fort} was seized in the chaos" } },
    [EV_DIE] = { { "{a} killed {b} in the chaos of {fall}", "{b} died by {a}'s hand on {night}" },
                 { "I killed {b} that night", "{b} died by my hand" },
                 { "{b} died on {night}", "{b} did not live through {fall}" } },
    [EV_BLAME] = { { "{f1} named {b} the traitor of {fall}", "{a} said {b} opened the seal" },
                   { "I named {b} the traitor", "I said it was {b}" },
                   { "{b} was named the traitor of {fall}", "{f1} found someone to blame" } },
    [EV_PUNISH] = { { "{a} walled {b} in alive for opening the seal", "{a} bricked {b} into the wall at {seal}" },
                    { "I walled {b} in. {b} did not scream", "I laid the last brick in front of {b}'s face" },
                    { "{b} was walled in alive for {fall}", "they walled {b} in at {seal}" } },
    [EV_FORGE] = { { "{a} wrote the chronicle of {fall} for {f1}, and wrote it false", "{a} wrote {art}, and every page of it a lie" },
                   { "I wrote what {f1} told me to write", "I wrote {art}. I wrote it false" },
                   { "the chronicle of {fall} was written by those who profited", "{art} was written to order" } },
    [EV_ACCOUNT] = { { "{a} wrote down what truly happened on {fall}", "{a} wrote {art}, and hid the pages" },
                     { "I wrote it all down. Someone must read it", "I wrote down what I saw. Every word is true" },
                     { "someone wrote down the truth of {fall}, and hid the pages", "the truth of {fall} is written somewhere" } },
    [EV_FLEE] = { { "{a} fled {fort} with what {a} could carry", "{a} ran with the gold before the dawn" },
                  { "I ran, and I took what I could", "I fled with the gold" },
                  { "someone fled {fort} with the gold that night", "gold left {fort} on {night}" } },
    [EV_BURN] = { { "{a} burned {village} on the word of {b}", "{a} barred the chapel door in {village}, on {b}'s order" },
                  { "I held the bar on the chapel door in {village}. {b} gave the order", "I burned {village}. {b} said to" },
                  { "{village} burned on someone's order", "someone barred the chapel door in {village}" } },
    [EV_SWEAR] = { { "{a} answers to {b}", "{a} is sworn to {b}" },
                   { "I answer to {b}", "I am sworn to {b}" },
                   { "someone answers to {b}", "{b} has sworn blades" } },
    [EV_HIDE] = { { "{a} still lives", "{a} did not die" },
                  { "I am still here", "I did not die" },
                  { "someone from {fall} still lives", "not everyone died" } },
};

/* special forms for events whose sub changes them */
static const char *special(const History *h, const HEvent *e, int form, Rng *r) {
    switch (e->type) {
    case EV_IMPRISON:
        if (e->sub == 1) {
            static const char *const K[3][2] = {
                { "{a} chained {b}, the last king under the mountain, below {fort}, and told the world it was {sealed}",
                  "{a} called {b} a monster and chained the king below {fort}" },
                { "I chained {b} below and called a king a monster", "I made {b} into {sealed}. A king. A monster. My word did that" },
                { "{sealed} is no monster. It is {b}, the king who was there first", "a king lies chained under {fort}, and they call it {sealed}" },
            };
            return K[form][rng_range(r, 0, 1)];
        }
        return NULL;
    case EV_BURY:
        if (e->sub == 1) {
            static const char *const P[3][2] = {
                { "{a} walled the sick below {fort} while they still breathed, and named the plague {sealed}",
                  "{f1} shut the plague-sick in below and told the world of {sealed}" },
                { "I walled the sick in below. They were still breathing", "I shut them in. I gave the fever a monster's name" },
                { "the sick were walled in below while they still breathed. {sealed} is the name they gave the plague",
                  "there is no {sealed}. There was a plague, and a wall" },
            };
            return P[form][rng_range(r, 0, 1)];
        }
        return NULL;
    case EV_PUNISH:
        if (e->sub == 1) {
            static const char *const H[3] = { "{a} hanged {b} at the gate for opening the seal", "I hanged {b} at the gate", "{b} was hanged at the gate for {fall}" };
            return H[form];
        }
        if (e->sub == 2) {
            static const char *const X[3] = { "{a} drove {b} down into the deep for opening the seal", "I drove {b} into the deep",
                                              "{b} was driven into the deep for {fall}" };
            return X[form];
        }
        return NULL;
    case EV_VOICE:
        if (e->sub == 1) {
            static const char *const T[3] = { "{a} heard the ones below singing, and {b}'s voice among them",
                                              "I heard them singing below the seal. {b} was singing with them",
                                              "someone heard singing beneath {seal}, and a child's voice among it" };
            return T[form];
        }
        return NULL;
    case EV_DIE:
        if (e->a == H_NONE) {
            static const char *const T[3] = { "{b} died on {night}", "{b} died that night", "{b} did not live through {fall}" };
            return T[form];
        }
        return NULL;
    case EV_HIDE: {
        static const char *const T[4][3] = {
            { "{a} still lives, and rules what is left of {fort}", "I still rule here", "someone still rules what is left of {fort}" },
            { "{a} walks the deep halls again, and what walks is not {a}", "I came back. Most of me", "something walks the deep halls wearing a face" },
            { "{a} did not die. {a} hides in the deep", "I did not die. I hid", "someone from {fall} hides in the deep" },
            { "{a} still lives, deep in {fort}, and keeps {seal} open", "I am still here, and {seal} stays open", "someone keeps {seal} open, deep in {fort}" },
        };
        return T[e->sub & 3][form];
    }
    case EV_STEAL:
        if (!(e->a != H_NONE && e->b != H_NONE && (h->actor[e->a].sibling == e->b || h->actor[e->a].spouse == e->b ||
                                                   h->actor[e->a].parent == e->b || h->actor[e->a].child == e->b)) &&
            form == FORM_THIRD)
            return "{a} took {art} from {b} while {b} slept";
        return NULL;
    default: return NULL;
    }
}

/* Texts are made to sit inside other sentences: a leading common word stays small (the
 * voice around it capitalizes what starts a sentence or a quotation). */
static void decap(char *s) {
    static const char *const W[] = { "The ", "A ", "Someone ", "Every ", "No ", "In ", "On ", "There ", "Not ", "Three ", "Two ",
                                     "Nothing ", "What ", "Something ", "Here ", "Whoever " };
    for (int i = 0; i < LEN(W); i++)
        if (!strncmp(s, W[i], strlen(W[i]))) { s[0] = (char)(s[0] + 32); return; }
}

static void deed_text(const History *h, int ev, int form, const HPersonal *p, Rng *r, char *out, int n) {
    TCtx t;
    tctx(h, ev, H_NONE, H_NONE, p, &t);
    const HEvent *e = &h->ev[ev];
    const char *tm = special(h, e, form, r);
    if (!tm) tm = DEED[e->type][form][rng_range(r, 0, 1)];
    if (!tm) tm = "something happened";
    story_expand(out, n, tm, r, &t.c);
    decap(out);
}

/* Why the opener opened it (short, third person). */
static void open_reason(const History *h, bool first, const HPersonal *p, Rng *r, char *out, int n) {
    u8 O = h->opener;
    const HActor *o = &h->actor[O];
    TCtx t;
    tctx(h, h->ev_open, O, o->mtarget, p, &t);
    char cause[STORY_TEXT] = "";
    if (o->mcause != H_NONE) deed_text(h, o->mcause, first && h->ev[o->mcause].a == O ? FORM_FIRST : FORM_THIRD, p, r, cause, sizeof cause);
    story_cset(&t.c, "cause", cause);
    const char *tm;
    switch (o->motive) {
    case MV_PACT: {
        const HEvent *pe = &h->ev[o->mcause];
        char maker[H_NAME], era[H_NAME];
        snprintf(maker, sizeof maker, "%s", h->actor[pe->a].name);
        snprintf(era, sizeof era, "%s", h->era_name[pe->era]);
        story_cset(&t.c, "maker", maker);
        story_cset(&t.c, "pera", era);
        tm = first ? "I opened it to pay what {maker} promised {sealed} in {pera}. One open night. Someone of the line had to pay"
                   : "{a} opened it to pay an old debt: {maker} of {a}'s line bargained with {sealed} in {pera}, and the price was one open night";
        story_expand(out, n, tm, r, &t.c);
        return;
    }
    case MV_POWER: tm = first ? "I opened it to take {fort} from {b}. {fort} is mine. What is left of it" : "{a} opened it to take {fort} from {b} in the chaos. {cause}"; break;
    case MV_REVENGE: tm = first ? "I opened it to see {b} fall. {cause}. I would open it again" : "{a} opened it to see {b} fall, because {cause}"; break;
    case MV_FAITH:
        if (h->tone == TONE_TRAGEDY) tm = first ? "I opened it to set them free. They were people. They were always people" : "{a} opened it to set free the ones below";
        else tm = first ? "I opened it because {sealed} asked, and I had prayed so long for it to ask" : "{a} opened it so that {sealed} could come up, as the prayers asked";
        break;
    case MV_TRUTH:
        if (h->tone == TONE_HUMAN) tm = first ? "I opened it so the world would see the bones. No one came to look" : "{a} opened it so the world would see what lies below: the bones";
        else tm = first ? "I opened it because I knew the truth: {cause}" : "{a} opened it because {a} knew the truth: {cause}";
        break;
    case MV_GREED: tm = first ? "I opened it for gold. {cause}. The gate was worth more than the debt" : "{a} opened it for gold: {cause}, and the gate was worth more than the debt"; break;
    case MV_DUTY: tm = first ? "I opened it because that is what {fort} was built for. {cause}. I only finished it" : "{a} opened it because that is what {fort} was built for: {cause}"; break;
    case MV_GRIEF: tm = first ? "I opened it for {b}. My {child}. {b} called, and I went" : "{a} opened it for {b}, the {child} {a} had lost: {b}'s voice called from below, and {a} went to answer"; break;
    default: tm = first ? "I opened it, and I had reasons: {cause}" : "{a} opened it, and {a} had reasons: {cause}"; break;
    }
    story_expand(out, n, tm, r, &t.c);
}

static void burn_reason(const History *h, const HPersonal *p, Rng *r, char *out, int n) {
    const HEvent *e = &h->ev[h->ev_burn];
    TCtx t;
    tctx(h, h->ev_burn, e->a, e->b, p, &t);
    char w[H_NAME + 24], acc[H_NAME], goat[H_NAME];
    actor_name(h, h->witness, p, w, sizeof w);
    snprintf(acc, sizeof acc, "%s", h->ev_account != H_NONE ? h->art[h->ev[h->ev_account].art].name : "the pages");
    snprintf(goat, sizeof goat, "%s", h->scapegoat != H_NONE ? h->actor[h->scapegoat].name : "the traitor");
    story_cset(&t.c, "w", w);
    story_cset(&t.c, "acc", acc);
    story_cset(&t.c, "goat", goat);
    static const char *const T[4] = {
        "{village} burned because {w} lived there, and {acc} was hidden there. {b} wanted the pages ash",
        "{village} burned because the child of {goat} hid there, and {b} wanted no one left to ask questions",
        "{village} burned because the bargain with {sealed} asked for it, and {b} pays the bargain",
        "{village} burned as a lesson to those who ask about {fall}. {b} gave the order",
    };
    story_expand(out, n, T[e->sub & 3], r, &t.c);
}

/* The crack in the lie. Third person, or first person for whoever told the lie. */
static void crack_text(const History *h, bool first, const HPersonal *p, Rng *r, char *out, int n) {
    u8 goat = h->scapegoat, O = h->opener;
    TCtx t;
    tctx(h, h->ev_blame, h->ev[h->ev_blame].a, goat, p, &t);
    char key[H_NAME];
    snprintf(key, sizeof key, "%s", h->ev[h->ev_open].art != H_NONE ? h->art[h->ev[h->ev_open].art].name : "a key");
    story_cset(&t.c, "key", key);
    const char *tm;
    if (goat == H_NONE)
        tm = first ? "I said {sealed} broke it from below. But the bolts were drawn back from our side. Nothing below draws bolts"
                   : "the bolts of {seal} were drawn back from this side. Nothing below draws bolts";
    else if (h->keyholder == goat && O != goat)
        tm = first ? "I named {b}. But {b}'s key was gone the day before. {b} told me so. I did not listen"
                   : "{b}'s key was stolen before {fall}. {b} went looking for it all day. Whoever turned it, {b} did not";
    else
        tm = first ? "I named {b}. But the seal was turned with {key}, and {key} was never in {b}'s hand"
                   : "the seal was turned with {key}, and {key} was never in {b}'s hand";
    story_expand(out, n, tm, r, &t.c);
}

static void claim_text(const History *h, const HFact *f, const HPersonal *p, Rng *r, char *out, int n) {
    TCtx t;
    tctx(h, f->ev, H_NONE, f->b, p, &t);
    const char *tm;
    if (f->ev == h->ev_founding) {
        static const char *const T[] = { "{builders} sealed {sealed} beneath {fort}, and kept it there", "{sealed} is held below {fort} by the seal of {builders}" };
        tm = PICK(r, T);
    } else if (f->b == H_NONE) {
        static const char *const T[] = { "{sealed} broke the seal from below. No hand opened it", "no one opened {seal}. {sealed} broke it from below" };
        tm = PICK(r, T);
    } else {
        static const char *const T[] = { "{b} opened the seal and let {sealed} into {fort}", "{b} the traitor opened {seal}", "the traitor of {fall} was {b}" };
        tm = PICK(r, T);
    }
    story_expand(out, n, tm, r, &t.c);
}

static void kin_text(const History *h, const HFact *f, bool grave, const HPersonal *p, Rng *r, char *out, int n) {
    TCtx t;
    tctx(h, -1, f->a, f->b, p, &t);
    const char *tm;
    if (grave && h->actor[f->a].died != H_NONE) tm = "Here lies {a}, {relb} of {b}";
    else if (grave) tm = "two names cut into one stone: {a}, and {b}, {a}'s {rel}";
    else tm = rng_chance(r, 0.5f) ? "{b} was {a}'s {rel}" : "{a} and {b}: one blood. {b} was {a}'s {rel}";
    if (h->actor[f->a].sworn == f->b || h->actor[f->b].sworn == f->a) tm = "{a} and {b}: sworn to each other";
    story_expand(out, n, tm, r, &t.c);
}

static void fate_text(const History *h, const HFact *f, int form, const HPersonal *p, Rng *r, char *out, int n) {
    if (f->a != h->opener) {
        snprintf(out, (size_t)n, form == FORM_FIRST ? "I lived through %s" : "%s lived through %s", form == FORM_FIRST ? h->fall : h->actor[f->a].name, h->fall);
        return;
    }
    for (int i = 0; i < h->nevents; i++)
        if (h->ev[i].type == EV_HIDE && h->ev[i].a == f->a) { deed_text(h, i, form, p, r, out, n); return; }
    snprintf(out, (size_t)n, "%s still lives", h->actor[f->a].name);
}

/* A fact in its plain form (third person; first if the speaker did it). */
static void fact_plain(const History *h, int fi, u8 speaker, const HPersonal *p, Rng *r, char *out, int n) {
    const HFact *f = &h->fact[fi];
    bool first = speaker != H_NONE && speaker == f->a;
    switch (f->kind) {
    case FK_DEED: deed_text(h, f->ev, first ? FORM_FIRST : FORM_THIRD, p, r, out, n); break;
    case FK_ANON: deed_text(h, f->ev, FORM_ANON, p, r, out, n); break;
    case FK_CLAIM: claim_text(h, f, p, r, out, n); break;
    case FK_CRACK: crack_text(h, speaker != H_NONE && speaker == h->ev[h->ev_blame].a, p, r, out, n); break;
    case FK_KIN: kin_text(h, f, false, p, r, out, n); break;
    case FK_FATE: fate_text(h, f, first ? FORM_FIRST : FORM_THIRD, p, r, out, n); break;
    case FK_WHY:
        if (f->ev == h->ev_burn) burn_reason(h, p, r, out, n);
        else if (f->ev == h->ev_open) open_reason(h, speaker != H_NONE && speaker == h->opener, p, r, out, n);
        else {
            char d[STORY_TEXT], c[STORY_TEXT] = "";
            deed_text(h, f->ev, FORM_THIRD, p, r, d, sizeof d);
            if (h->ev[f->ev].cause != H_NONE) deed_text(h, h->ev[f->ev].cause, FORM_THIRD, p, r, c, sizeof c);
            snprintf(out, (size_t)n, "%.150s, because %.150s", d, c);
        }
        break;
    default: snprintf(out, (size_t)n, "..."); break;
    }
    decap(out);
}

/* ---------- voices ---------- */

void history_clue_text(const History *h, int ci, const HPersonal *p, char *out, int n) { history_clue_text_v(h, ci, p, 0, out, n); }

void history_clue_text_v(const History *h, int ci, const HPersonal *p, u64 variant, char *out, int n) {
    out[0] = 0;
    if (ci < 0 || ci >= h->nclues) return;
    const HClue *cl = &h->clue[ci];
    const HFact *f = &h->fact[cl->fact];
    Rng r = rng_make(h->seed ^ (u64)(ci + 1) * 0x9E3779B97F4A7C15ULL ^ 0xC1E ^ variant);
    char plain[STORY_TEXT], who[H_NAME + 24] = "", holder[H_NAME] = "";
    fact_plain(h, cl->fact, cl->src == SRC_DYING ? cl->speaker : H_NONE, p, &r, plain, sizeof plain);
    if (cl->speaker != H_NONE) actor_name(h, cl->speaker, p, who, sizeof who);
    if (f->holder != H_NONE) snprintf(holder, sizeof holder, "%s", h->faction[f->holder].name);
    Ctx c = { 0 };
    story_cset(&c, "x", plain);
    story_cset(&c, "who", who);
    story_cset(&c, "holder", holder);
    story_cset(&c, "fall", h->fall);
    char era[H_NAME] = "", acc[H_NAME] = "", w[H_NAME + 24] = "";
    if (f->ev != H_NONE) snprintf(era, sizeof era, "%s", h->era_name[h->ev[f->ev].era]);
    if (h->ev_account != H_NONE) snprintf(acc, sizeof acc, "%s", h->art[h->ev[h->ev_account].art].name);
    actor_name(h, h->witness, p, w, sizeof w);
    story_cset(&c, "era", era);
    story_cset(&c, "acc", acc);
    story_cset(&c, "w", w);
    const char *tm = "{x}.";
    switch (cl->src) {
    case SRC_BODY:
        if (f->kind == FK_CLAIM) tm = "Orders on the body, under the seal of {holder}: '{x}. Let no one say otherwise.'";
        else {
            static const char *const T[] = { "Papers on the body. One line is underlined: '{x}.'", "A ledger on the body: '{x}.'",
                                             "A letter on the body, never sent: '{x}.'" };
            tm = PICK(&r, T);
        }
        break;
    case SRC_DYING:
        if (f->kind == FK_CRACK || f->a == cl->speaker || f->kind == FK_WHY) tm = "{who}, dying: '{x}.'";
        else {
            u8 era = f->ev != H_NONE ? h->ev[f->ev].era : ERA_BEFORE;
            bool there = f->ev != H_NONE && cl->speaker != H_NONE && h->actor[cl->speaker].born <= era && era >= ERA_BEFORE;
            static const char *const T[] = { "{who}, dying: '{x}. Remember that.'", "{who}, dying: '{x}. Now you know.'",
                                              "{who}, dying: '{x}. Tell them.'" };
            if (there && rng_chance(&r, 0.6f)) tm = "{who}, dying: '{x}. I was there.'";
            else if (era <= ERA_OLD2) tm = "{who}, dying: 'It is in the oldest books: {x}.'";
            else tm = PICK(&r, T);
        }
        break;
    case SRC_STELE:
        if (f->kind == FK_CLAIM) {
            static const char *const T[] = { "Carved by {holder}: '{x}.'", "{holder} remember {fall}: '{x}.'", "A stone of {holder}: '{x}. Curse the name.'" };
            tm = PICK(&r, T);
        } else if (f->kind == FK_KIN) {
            char k[STORY_TEXT];
            kin_text(h, f, true, p, &r, k, sizeof k);
            snprintf(out, (size_t)n, "%s.", k);
            out[0] = (char)(out[0] >= 'a' && out[0] <= 'z' ? out[0] - 32 : out[0]);
            return;
        } else if (f->kind == FK_DEED && f->ev != H_NONE && h->ev[f->ev].era <= ERA_OLD2) {
            static const char *const T[] = { "Cut deep, and old: '{x}.'", "An old stone: '{x}.'", "Worn letters: '{x}.'" };
            tm = PICK(&r, T);
        } else {
            static const char *const T[] = { "Cut into the wall in a hurry: '{x}.'", "Scratched beside the door: '{x}.'", "Someone carved it here: '{x}.'" };
            tm = PICK(&r, T);
        }
        break;
    case SRC_NOTE:
        if (f->weight <= 1) {
            static const char *const T[] = { "Scratched on the wall: 'They say {x}.'", "A torn page: '...{x}...'", "A dead delver's note: 'An old {trade} told me {x}.'" };
            tm = PICK(&r, T);
        } else {
            static const char *const T[] = { "A dead delver's note: '{x}. I have it from someone who was there.'",
                                             "A diary, the last page: '{x}. Nobody believes me.'", "A note in a dead hand: '{x}.'",
                                             "A letter never sent: '{x}. Tell no one I wrote this.'" };
            tm = PICK(&r, T);
        }
        break;
    case SRC_BOUNTY: tm = "Posted by {holder}: '{x}. Reward for the traitor's kin.'"; break;
    case SRC_LEGEND: tm = cl->speaker != H_NONE ? "Carried by {who}. {x}." : "{x}."; break;
    case SRC_PAGE: tm = "A page of {acc}, in {w}'s hand: '{x}.'"; break;
    case SRC_ORDERS:
        if (f->ev != H_NONE && h->ev[f->ev].type == EV_SWEAR) tm = "The hunters carry orders: '{x}. Bring me the one who asks about {fall}.'";
        else tm = "The hunters' leader carries a letter: '{x}.'";
        break;
    default: break;
    }
    story_cset(&c, "trade", "lamplighter");
    story_expand(out, n, tm, &r, &c);
}

/* Facts no revelation carried, for the chapters after the mystery, in order. */
static int late_fact(const History *h, int chapter) {
    int k = chapter - H_CAST, seen = 0;
    for (int w = 3; w >= 1; w--)
        for (int i = 0; i < h->nfacts; i++) {
            const HFact *f = &h->fact[i];
            if (f->weight != w || f->order != H_NONE || f->kind == FK_CLAIM || (f->ev != H_NONE && h->ev[f->ev].type == EV_SWEAR)) continue;
            if (seen++ == k) return i;
        }
    return -1;
}

int history_reveal_clue(const History *h, int chapter) {
    if (chapter < 0) return -1;
    if (chapter < H_CAST) return h->reveal[chapter];
    return -1;
}

void history_reveal(const History *h, int chapter, const HPersonal *p, const char *next, char *out, int n) {
    if (chapter < H_CAST) { history_clue_text(h, h->reveal[chapter], p, out, n); return; }
    int f = late_fact(h, chapter);
    if (f < 0) {
        snprintf(out, (size_t)n, "Nothing on the body but a name, scratched into the blade: %.40s.", next ? next : "another");
        return;
    }
    Rng r = rng_make(h->seed ^ (u64)chapter * 0x7E1A);
    char plain[STORY_TEXT];
    fact_plain(h, f, H_NONE, p, &r, plain, sizeof plain);
    snprintf(out, (size_t)n, "Papers on the body, old ones: '%.250s.' And a name: %.40s.", plain, next ? next : "another");
}

void history_fact_line(const History *h, int fi, const HPersonal *p, char *out, int n) {
    out[0] = 0;
    if (fi < 0 || fi >= h->nfacts) return;
    const HFact *f = &h->fact[fi];
    Rng r = rng_make(h->seed ^ (u64)(fi + 1) * 0xC2B2AE3D27D4EB4FULL ^ 0xFAC7);
    char plain[STORY_TEXT];
    fact_plain(h, fi, H_NONE, p, &r, plain, sizeof plain);
    if (f->kind == FK_CLAIM) snprintf(out, (size_t)n, "%s say: %.240s.", f->holder != H_NONE ? h->faction[f->holder].name : "They", plain);
    else snprintf(out, (size_t)n, "%.300s.", plain);
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 32);
}

/* ---------- titles, motives ---------- */

void history_title(const History *h, int c, char *out, int n) {
    u8 a = h->cast[c];
    const HActor *x = &h->actor[a];
    const char *f = h->faction[x->faction].name;
    if (a == h->opener) {
        if (x->flags & AF_RETURNED) snprintf(out, (size_t)n, "the One Who Came Back");
        else if (x->motive == MV_POWER) snprintf(out, (size_t)n, "Lord of %.36s", h->fortress);
        else if (x->flags & AF_HIDDEN) snprintf(out, (size_t)n, "the Hidden");
        else snprintf(out, (size_t)n, "the Last of %.36s", f);
        return;
    }
    if (a == h->scapegoat) { snprintf(out, (size_t)n, "the Exile"); return; }
    switch (x->role) {
    case RO_RULER: snprintf(out, (size_t)n, "Steward of %.34s", h->fortress); return;
    case RO_HEIR:
        if (x->parent != H_NONE) snprintf(out, (size_t)n, "heir of %.38s", h->actor[x->parent].name);
        else snprintf(out, (size_t)n, "heir of %.38s", f);
        return;
    case RO_KEEPER:
        if (x->key != H_NONE) snprintf(out, (size_t)n, "keeper of %.36s", h->art[x->key].name);
        else snprintf(out, (size_t)n, "keeper without a key");
        return;
    case RO_MERCHANT: snprintf(out, (size_t)n, "the Broker of %.32s", f); return;
    case RO_WITNESS: snprintf(out, (size_t)n, "the Last Witness"); return;
    case RO_SCRIBE: snprintf(out, (size_t)n, "scribe of %.36s", f); return;
    case RO_PRIEST:
        if (h->tone == TONE_HUMAN) snprintf(out, (size_t)n, "voice of %.36s", f);
        else snprintf(out, (size_t)n, "Herald of %.34s", h->sealed);
        return;
    default: break;
    }
    if (c % 3 == 0) snprintf(out, (size_t)n, "captain of %.36s", f);
    else if (c % 3 == 1) {
        if (h->ev_punish != H_NONE && h->ev[h->ev_punish].a == a) snprintf(out, (size_t)n, "gaoler of %.36s", f);
        else if (x->sworn != H_NONE) snprintf(out, (size_t)n, "the hand of %.32s", h->actor[x->sworn].name);
        else snprintf(out, (size_t)n, "marshal of %.36s", f);
    } else snprintf(out, (size_t)n, "master of %.36s", f);
}

/* What the Roll says of a name: their deed, unless it is the fire or a mere oath. */
static void legend_deed(const History *h, int c, const HPersonal *p, Rng *r, char *out, int n) {
    for (int i = 0; i < h->nclues; i++)
        if (h->clue[i].src == SRC_LEGEND && h->clue[i].speaker == h->cast[c]) {
            const HFact *f = &h->fact[h->clue[i].fact];
            u8 t = f->ev != H_NONE ? h->ev[f->ev].type : EV_COUNT;
            if (t == EV_BURN || t == EV_SWEAR || t == EV_BIRTH || f->kind != FK_DEED || f->a != h->cast[c]) break;
            fact_plain(h, h->clue[i].fact, H_NONE, p, r, out, n);
            return;
        }
    char title[H_NAME];
    history_title(h, c, title, sizeof title);
    snprintf(out, (size_t)n, "%s, %s. Served well", h->actor[h->cast[c]].name, title);
}

void history_hook(const History *h, int m, int c, const HPersonal *p, const char *next_power, char *out, int n) {
    (void)next_power;
    Rng r = rng_make(h->seed ^ (u64)(c * 4 + m + 1) * 0x51ED27);
    TCtx t;
    tctx(h, -1, h->cast[c], H_NONE, p, &t);
    char title[H_NAME], deed[STORY_TEXT], vault[H_NAME] = "a vault", hider[H_NAME] = "someone", hf[H_NAME] = "", hf2[H_NAME] = "", hera[H_NAME] = "",
         acc[H_NAME] = "", w[H_NAME + 24] = "", cover[H_NAME];
    history_title(h, c, title, sizeof title);
    legend_deed(h, c, p, &r, deed, sizeof deed);
    if (h->ev_hoard != H_NONE) {
        const HEvent *e = &h->ev[h->ev_hoard];
        snprintf(vault, sizeof vault, "%s", h->art[e->art].name);
        snprintf(hider, sizeof hider, "%s", h->actor[e->a].name);
        snprintf(hf, sizeof hf, "%s", h->faction[e->f1].name);
        snprintf(hf2, sizeof hf2, "%s", e->f2 != H_NONE ? h->faction[e->f2].name : "their enemies");
        snprintf(hera, sizeof hera, "%s", h->era_name[e->era]);
    }
    snprintf(acc, sizeof acc, "%s", h->ev_account != H_NONE ? h->art[h->ev[h->ev_account].art].name : "the account");
    actor_name(h, h->witness, p, w, sizeof w);
    snprintf(cover, sizeof cover, "%s", h->faction[h->cover].name);
    story_cset(&t.c, "target", t.a);
    story_cset(&t.c, "title", title);
    story_cset(&t.c, "deed", deed);
    story_cset(&t.c, "vault", vault);
    story_cset(&t.c, "hider", hider);
    story_cset(&t.c, "hf", hf);
    story_cset(&t.c, "hf2", hf2);
    story_cset(&t.c, "hera", hera);
    story_cset(&t.c, "acc", acc);
    story_cset(&t.c, "w", w);
    story_cset(&t.c, "cover", cover);
    const char *tm = "{target}.";
    u8 T = h->cast[c];
    switch (m) {
    case MOTIVE_REVENGE:
        if (c == 0) tm = "They burned {village}. Your {kin} was in the chapel when they barred the door. The one who held the bar: {target}, {title}.";
        else if (T == h->orderer) tm = "{target} gave the order to burn {village}. {target} is next.";
        else if (c < 3) tm = "{target} stands behind the one who held the bar. {target} is next.";
        else if (c < 6) tm = c == 5 ? "{target} knows why {village} had to burn, and who it was all for." : "{target} knows who gave the orders, and why.";
        else tm = c == 8 ? "Everything that burned, burned for {target}. End it." : "{target} serves the one it all comes back to.";
        break;
    case MOTIVE_GLORY:
        if (c == 0) tm = "The Roll of the Dead hangs by the gate of {fort}. The first name on it: {target}. Beside it: '{deed}.' Kill it, and they carve yours below.";
        else if (c == 3) tm = "The names on the Roll have something in common. The next: {target}. Beside it: '{deed}.'";
        else if (c == 6) tm = "Only three names stand above yours. The first: {target}. Beside it: '{deed}.'";
        else tm = "A new name on the Roll: {target}. Beside it: '{deed}.'";
        break;
    case MOTIVE_RICHES:
        if (c == 0) tm = "{hider} hid the gold of {hf} in {hera}, out of the reach of {hf2}. {target} keeps the first piece of the map to {vault}.";
        else if (c == 1) tm = "{target} keeps the second piece of the map to {vault}.";
        else if (c == 2) tm = "{vault} lies under {target}'s hall.";
        else if (c == 5 || c == 8) tm = "What was carried off on {night} lies under {target}'s feet.";
        else tm = "{target} keeps a piece of the map to what was carried off on {night}.";
        break;
    default:
        if (c == 0) tm = "What happened on {fall}? {w} wrote it down, and {cover} tore the pages out. {target} keeps the first page of {acc}.";
        else if (c % 3 == 2) tm = "{target} burned more pages of {acc} than anyone. Ask why.";
        else tm = "{target} keeps the next page of {acc}.";
        break;
    }
    story_expand(out, n, tm, &r, &t.c);
}

void history_done(const History *h, int m, int c, const HPersonal *p, const char *next_power, char *out, int n) {
    Rng r = rng_make(h->seed ^ (u64)(c * 4 + m + 1) * 0xD0E5);
    TCtx t;
    tctx(h, -1, h->cast[c], c + 1 < H_CAST ? h->cast[c + 1] : H_NONE, p, &t);
    char nx[H_NAME + 24], reason[STORY_TEXT] = "", ord[H_NAME + 24], vault[H_NAME] = "the vault";
    if (c + 1 < H_CAST) snprintf(nx, sizeof nx, "%s", t.b);
    else snprintf(nx, sizeof nx, "%s", next_power ? next_power : "someone older");
    actor_name(h, h->orderer, p, ord, sizeof ord);
    if (h->ev_burn != H_NONE) burn_reason(h, p, &r, reason, sizeof reason);
    if (h->ev_hoard != H_NONE) snprintf(vault, sizeof vault, "%s", h->art[h->ev[h->ev_hoard].art].name);
    story_cset(&t.c, "target", t.a);
    story_cset(&t.c, "next", nx);
    story_cset(&t.c, "ord", ord);
    story_cset(&t.c, "reason", reason);
    story_cset(&t.c, "vault", vault);
    const char *tm = "{target} is dead. Next: {next}.";
    u8 T = h->cast[c];
    switch (m) {
    case MOTIVE_REVENGE:
        if (c == 0) tm = "{target} died with orders on the body, in the hand of {ord}.";
        else if (T == h->orderer) tm = "'{reason},' said {target}, and died. Ask {next} who it was all for.";
        else if (c == 8) tm = "{target} is dead. {village} is still ash. But now you know why it burned. There is an older debt: {next}.";
        else tm = "'Ask {next},' said {target}, and died.";
        break;
    case MOTIVE_GLORY:
        if (c == 5) tm = "They carve your name. Read down the Roll: it is not a roll of heroes. It is the list of everyone who kept the secret of {fall}. Above you now: {next}.";
        else if (c == 8) tm = "Your name is first on the Roll. Under it, every name that kept the secret. Tomorrow a new one is carved above it: {next}.";
        else tm = "They carve your name on the Roll. Above it now: {next}.";
        break;
    case MOTIVE_RICHES:
        if (c == 0) tm = "Half a map. {next} holds the rest.";
        else if (c == 1) tm = "The map is whole. It ends at {next}.";
        else if (c == 2) tm = "{vault} is open. Most of it is gone: carried off on {night}. {next} knows where.";
        else if (c == 8) tm = "The gold of {fall} is yours. Gold calls to gold: {next}.";
        else tm = "Another piece of the map. The next is with {next}.";
        break;
    default: {
        char page[STORY_TEXT];
        history_clue_text(h, h->page[c], p, page, sizeof page);
        story_cset(&t.c, "page", page);
        tm = c == 8 ? "{page} The last page. It names another book, and its keeper: {next}." : "{page} {next} has the next one.";
        break;
    }
    }
    story_expand(out, n, tm, &r, &t.c);
}

bool history_legend_lore(const History *h, int legend, char *out, int n) {
    for (int i = 0; i < h->nclues; i++)
        if (h->clue[i].src == SRC_LEGEND && h->clue[i].chapter == legend) {
            history_clue_text(h, i, NULL, out, n);
            return true;
        }
    return false;
}

bool history_trace_clue(const History *h, u64 key, int kind, int dist, u8 culture, int *clue) {
    (void)culture;
    if (hash_unit(mix64(key ^ 0x7C1E)) < (dist < 8 ? 0.45f : 0.35f)) return false; /* some keep the old voices */
    u8 src = kind == TR_STELE ? SRC_STELE : kind == TR_CORPSE ? SRC_NOTE : SRC_BOUNTY;
    float w[H_CLUES], total = 0;
    for (int i = 0; i < h->nclues; i++) {
        const HClue *c = &h->clue[i];
        w[i] = 0;
        if (c->src != src || c->dist > dist) continue;
        float wt = (float)h->fact[c->fact].weight;
        if (c->dist * 2 > dist) wt *= 2.0f; /* the frontier: what is new out here */
        w[i] = wt;
        total += wt;
    }
    if (total <= 0) return false;
    int i = pick_weighted(hash_unit(mix64(key ^ 0xC7E5)), w, h->nclues);
    if (i < 0) return false;
    *clue = i;
    return true;
}

void history_question(const History *h, int qi, const HPersonal *p, char *out, int n) {
    const HQuestion *q = &h->q[qi];
    char goat[H_NAME] = "", O[H_NAME + 24];
    if (h->scapegoat != H_NONE) snprintf(goat, sizeof goat, "%s", h->actor[h->scapegoat].name);
    actor_name(h, h->opener, p, O, sizeof O);
    switch (q->kind) {
    case Q_WHO: snprintf(out, (size_t)n, "Who opened %s on %s?", h->places[0], h->fall); break;
    case Q_GUILTY:
        if (h->scapegoat != H_NONE) snprintf(out, (size_t)n, "Did %s open it, as %s say?", goat, h->faction[h->cover].name);
        else snprintf(out, (size_t)n, "Did %s break out by itself, as %s say?", h->sealed, h->faction[h->cover].name);
        break;
    case Q_WHY: snprintf(out, (size_t)n, "Why was the seal opened?"); break;
    case Q_BELOW: snprintf(out, (size_t)n, "What lies beneath %s?", h->fortress); break;
    case Q_VILLAGE: snprintf(out, (size_t)n, "Why did %s burn?", p && p->village ? p->village : "the village"); break;
    default: snprintf(out, (size_t)n, "Where is %s now?", O); break;
    }
}

void history_answer(const History *h, int qi, const HPersonal *p, char *out, int n) {
    const HQuestion *q = &h->q[qi];
    Rng r = rng_make(h->seed ^ (u64)(qi + 1) * 0xA25);
    char x[STORY_TEXT];
    switch (q->kind) {
    case Q_WHO: deed_text(h, h->ev_open, FORM_THIRD, p, &r, x, sizeof x); break;
    case Q_GUILTY: crack_text(h, false, p, &r, x, sizeof x); break;
    case Q_WHY: open_reason(h, false, p, &r, x, sizeof x); break;
    case Q_BELOW: deed_text(h, h->ev_secret, FORM_ANON, p, &r, x, sizeof x); break;
    case Q_VILLAGE: burn_reason(h, p, &r, x, sizeof x); break;
    default: {
        HFact f = { 0 };
        f.kind = FK_FATE;
        f.a = h->opener;
        fate_text(h, &f, FORM_THIRD, p, &r, x, sizeof x);
        break;
    }
    }
    snprintf(out, (size_t)n, "%.300s.", x);
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 32);
}

/* ---------- a readable account (worldmap story) ---------- */

void history_dump(const History *h, FILE *f) {
    static const char *const ERA_LABEL[ERA_COUNT] = { "founding", "old", "old", "before", "fall", "now" };
    HPersonal p = { "Aldmere", "sister" };
    fprintf(f, "HISTORY of %s (seed %llu, attempt %d)\n", h->fortress, (unsigned long long)h->seed, h->attempt);
    fprintf(f, "  tone: %s; below: %s (%s)\n", TONE_NAMES[h->tone], BELOW_NAMES[h->below], h->sealed);
    fprintf(f, "  shape: %s (score %d)", SHAPE_NAMES[h->shape], h->score[h->shape]);
    if (h->shape2 != H_NONE) fprintf(f, ", then %s (%d)", SHAPE_NAMES[h->shape2], h->score[h->shape2]);
    if (h->shape3 != H_NONE) fprintf(f, ", %s (%d)", SHAPE_NAMES[h->shape3], h->score[h->shape3]);
    fprintf(f, "; causal chain %d\n", h->chain);
    fprintf(f, "  eras: %s, %s, %s, %s, %s\n", h->era_name[0], h->era_name[1], h->era_name[2], h->era_name[3], h->era_name[4]);
    fprintf(f, "FACTIONS\n");
    for (int i = 0; i < h->nfactions; i++)
        fprintf(f, "  %d %s (%s)%s%s\n", i, h->faction[i].name, CULTURE_NAMES[h->faction[i].culture], (h->faction[i].flags & 4) ? " - the liars" : "",
                i == h->cover ? " - tells the lie" : "");
    fprintf(f, "EVENTS\n");
    Rng r = rng_make(h->seed);
    for (int i = 0; i < h->nevents; i++) {
        char t[STORY_TEXT];
        deed_text(h, i, FORM_THIRD, &p, &r, t, sizeof t);
        fprintf(f, "  %2d [%-7s %-8s] %s", i, ERA_LABEL[h->ev[i].era], EV_NAMES[h->ev[i].type], t);
        if (h->ev[i].cause != H_NONE) fprintf(f, "  <- %d", h->ev[i].cause);
        fprintf(f, "\n");
    }
    fprintf(f, "THE MYSTERY\n");
    char t[STORY_TEXT];
    fprintf(f, "  opener: %s (%s, %s; %s%s%s)\n", h->actor[h->opener].name, ROLE_NAMES[h->actor[h->opener].role],
            h->faction[h->actor[h->opener].faction].name, MOTIVE_V_NAMES[h->actor[h->opener].motive],
            (h->actor[h->opener].flags & AF_RETURNED) ? ", returned" : "", (h->actor[h->opener].flags & AF_HIDDEN) ? ", hidden" : "");
    fprintf(f, "  blamed: %s, by %s\n", h->scapegoat != H_NONE ? h->actor[h->scapegoat].name : h->sealed, h->faction[h->cover].name);
    open_reason(h, false, &p, &r, t, sizeof t);
    fprintf(f, "  why: %s\n", t);
    if (h->ev_burn != H_NONE) {
        burn_reason(h, &p, &r, t, sizeof t);
        fprintf(f, "  the village: %s\n", t);
    }
    fprintf(f, "CAST\n");
    for (int c = 0; c < H_CAST; c++) {
        char title[H_NAME];
        history_title(h, c, title, sizeof title);
        const HActor *a = &h->actor[h->cast[c]];
        fprintf(f, "  ch%d %-10s %s, %s (%s, %s)\n", c + 1, c % 3 == 0 ? "captain" : c % 3 == 1 ? "general" : "POWER", a->name, title,
                ROLE_NAMES[a->role], CULTURE_NAMES[a->culture]);
    }
    fprintf(f, "REVELATIONS\n");
    for (int c = 0; c < H_CAST; c++) {
        history_clue_text(h, h->reveal[c], &p, t, sizeof t);
        fprintf(f, "  ch%d %s\n", c + 1, t);
    }
    fprintf(f, "QUESTIONS\n");
    for (int q = 0; q < h->nq; q++) {
        char a[STORY_TEXT];
        history_question(h, q, &p, t, sizeof t);
        history_answer(h, q, &p, a, sizeof a);
        fprintf(f, "  %s\n      %s\n", t, a);
    }
    fprintf(f, "  %d actors, %d events, %d facts, %d clues\n", h->nactors, h->nevents, h->nfacts, h->nclues);
}
