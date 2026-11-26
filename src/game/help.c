#include "game/game.h"

#include <string.h>

#include "engine/font.h"
#include "render/batch.h"

/* The online help (the HELP tab, F1 or ?) and the short tutorial a new character gets after
 * choosing why they came (openspec help-and-tutorial).
 *
 * The help is a list of topics with a page each; the keys shown follow the device the player
 * uses (keyboard and mouse, a gamepad, touch). The tutorial is a card at the left of the view, one
 * step at a time; each step is done when the player does it (walks, jumps, strikes, takes up
 * another weapon, casts, ...), and the last one hands them to the story: the line at the top and
 * the Golden Thread. It can be skipped, and comes back in the help. */

enum { DEV_KEYS, DEV_PAD, DEV_TOUCH };

static int device_of(const Game *g, const PlayerSlot *ps) {
    if (ps->keyboard && g->touch.active) return DEV_TOUCH;
    if (ps->pad && !ps->keyboard) return DEV_PAD;
    return DEV_KEYS;
}

/* ---------- the help's pages ---------- */

typedef struct HelpTopic {
    const char *title;
    const char *text[3]; /* keyboard, gamepad, touch; NULL: the keyboard's page for all */
} HelpTopic;

static const HelpTopic TOPICS[] = {
    { "CONTROLS",
      { "WASD MOVE, MOUSE LOOK. SPACE JUMP, SHIFT SPRINT, CTRL OR C CROUCH (YOU DUCK AND CRAWL BY YOURSELF IN LOW TUNNELS).\n"
        "LEFT MOUSE STRIKE OR SHOOT, RIGHT MOUSE THE WEAPON'S ALTERNATE ATTACK.\n"
        "1 BLADE  2 BOW  3 STAFF  4 PICKAXE, OR THE WHEEL. Z: BACK TO THE WEAPON BEFORE.\n"
        "Q CAST THE SPELL, R / SHIFT+R ANOTHER SPELL.\n"
        "H / J HEALTH / MANA POTION.  K EAT  V ELIXIR  X THROW  C WHAT TO THROW  P CAMPFIRE.\n"
        "G GOLDEN THREAD  Y ANOTHER LINE AT THE TOP  M MAP.\n"
        "TAB MENU (Q/E TABS), I ITEMS, B SPELLS, L STORY, N FOOD, O ALCHEMY, U FORGE.\n"
        "F1 OR ? THIS HELP.  F9 SOUND  F10 MUSIC  F7/F8 INVERT MOUSE  - / = MOUSE SPEED.  ESC RELEASES THE MOUSE.",
        "LEFT STICK MOVE, RIGHT STICK LOOK. A JUMP, B CROUCH, LEFT STICK CLICK SPRINT.\n"
        "RT STRIKE OR SHOOT, LT THE WEAPON'S ALTERNATE ATTACK. BOWS AND STAVES AIM A LITTLE FOR YOU.\n"
        "LB / RB THE WEAPON BEFORE / NEXT (BLADE, BOW, STAFF, PICKAXE).\n"
        "X CAST THE SPELL, D-PAD LEFT / RIGHT ANOTHER SPELL.\n"
        "D-PAD UP / DOWN HEALTH / MANA POTION.\n"
        "RIGHT STICK CLICK THE GOLDEN THREAD.  BACK THE MAP.\n"
        "Y THE MENU: LB / RB TABS, D-PAD SELECTS, A USES, X SCRAPS, B CLOSES. THIS HELP IS THE LAST TAB.\n"
        "START ON ANOTHER PAD: A SECOND PLAYER JOINS ON A SPLIT SCREEN.",
        "THE LEFT STICK MOVES; DRAG ANYWHERE ELSE TO LOOK.\n"
        "THE BIG BUTTON STRIKES (HOLD IT), THE BLUE ONE IS THE ALTERNATE ATTACK.\n"
        "JUMP AND CROUCH ARE BESIDE THEM; THE WEAPON BUTTON TAKES UP THE NEXT WEAPON.\n"
        "THE SPELL BUTTON CASTS, > PICKS ANOTHER SPELL.\n"
        "THE BUTTONS AT THE TOP: MENU, MAP, POTION AND THE GOLDEN THREAD.\n"
        "IN THE MENU TAP A TAB, THEN WHAT YOU WANT; TAP IT AGAIN TO USE IT. THIS HELP IS THE LAST TAB." } },
    { "THE BASICS",
      { "YOU STAND BEFORE A CASTLE BUILT INTO A MOUNTAIN. BEHIND ITS GATE THE FORTRESS GOES ON WITHOUT END, IN EVERY "
        "DIRECTION, AND THE FARTHER FROM THE GATE THE MORE DANGEROUS IT IS.\n"
        "THE RED BAR IS YOUR HEALTH, THE BLUE ONE YOUR MANA. WOUNDS CLOSE BY THEMSELVES OUT OF A FIGHT; MANA COMES BACK "
        "ALWAYS.\n"
        "KILLS GIVE XP. EACH LEVEL GIVES A SKILL POINT: SPEND IT IN THE SKILLS TAB (BLADE, BOW, ARCANE, SURVIVAL).\n"
        "CHESTS, FALLEN FOES AND HIDDEN PLACES GIVE GOLD, POTIONS, WEAPONS, SPELL SCROLLS AND RELICS. WALK OVER THEM.\n"
        "STRIKE A LEVER, A STATUE OR A MIRROR TO USE IT. TO SPEAK TO SOMEONE IN A TOWN, FACE THEM "
        "CLOSE AND STRIKE: YOUR WEAPON STAYS DOWN, AND THEY ANSWER.\n"
        "A WARNING TELLS YOU WHEN A PLACE IS WELL ABOVE YOUR LEVEL. TURN BACK, OR COME PREPARED." } },
    { "FIGHTING",
      { "EVERY WEAPON HAS A PRIMARY AND AN ALTERNATE ATTACK: THE BLADE WHIRLS, THE BOW LOOSES A POWER SHOT THROUGH THREE FOES, "
        "THE STAFF SENDS A SMALL NOVA. THE LAST SKILL OF EACH BRANCH MAKES IT STRONGER.\n"
        "WEAPONS HAVE ROLES. ARROWS BRING DOWN FLIERS AND SHOOTERS. ARMOUR (KNIGHTS, GOLEMS, MIMICS) TURNS EDGES AND "
        "ARROWS; MACES, FLAILS, SLINGS, THE STAFF AND SPELLS BITE. SWAP WITH Z: THE SWING TIMER CARRIES OVER.\n"
        "WATCH FOR WIND-UPS: A RAISED WEAPON, A GLOW, A RING ON THE GROUND. STEP OUT OF THE RING BEFORE THE BLOW LANDS.\n"
        "PACKS COME IN WAVES: KILL THE FIRST, THEN THE REST ARRIVE. SHOOTERS STAND BEHIND; DODGE THEIR SHOTS SIDEWAYS.\n"
        "A PRESS JUST BEFORE THE WEAPON IS READY IS KEPT AND FIRES WHEN IT IS." } },
    { "SPELLS",
      { "Q CASTS THE ACTIVE SPELL; R CHOOSES ANOTHER YOU KNOW. SPELLS COST MANA (THE BLUE BAR).\n"
        "ATTACK SPELLS COME IN FIVE ELEMENTS (FIRE, FROST, LIGHTNING, VENOM, SHADOW) AND SEVERAL FORMS: A BOLT, AN ORB, A "
        "BREATH YOU HOLD, A RUNE YOU PLACE, AN AURA ABOUT YOU. FIRE BURNS AND SETS OIL ALIGHT, FROST SLOWS AND FREEZES WATER, "
        "LIGHTNING LEAPS FROM FOE TO FOE, VENOM EATS AWAY, SHADOW DRINKS LIFE.\n"
        "UTILITY SPELLS HEAL, WARD, LIGHT THE WAY, MAKE A SAFE PLACE OR SEND YOU HOME TO THE SANCTUM.\n"
        "NEW SPELLS COME FROM SCROLLS IN THE FORTRESS, THE SHOP AT SHRINES AND AT HOME, AND THE STORY. B OPENS THE "
        "SPELLBOOK: ENTER MAKES A KNOWN SPELL THE ACTIVE ONE. A SECOND SCROLL OF A SPELL YOU KNOW RANKS IT UP.",
        "X CASTS THE ACTIVE SPELL; D-PAD LEFT / RIGHT CHOOSES ANOTHER YOU KNOW. SPELLS COST MANA (THE BLUE BAR).\n"
        "ATTACK SPELLS COME IN FIVE ELEMENTS (FIRE, FROST, LIGHTNING, VENOM, SHADOW) AND SEVERAL FORMS: A BOLT, AN ORB, A "
        "BREATH YOU HOLD, A RUNE YOU PLACE, AN AURA ABOUT YOU. FIRE BURNS AND SETS OIL ALIGHT, FROST SLOWS AND FREEZES WATER, "
        "LIGHTNING LEAPS FROM FOE TO FOE, VENOM EATS AWAY, SHADOW DRINKS LIFE.\n"
        "UTILITY SPELLS HEAL, WARD, LIGHT THE WAY, MAKE A SAFE PLACE OR SEND YOU HOME TO THE SANCTUM.\n"
        "NEW SPELLS COME FROM SCROLLS IN THE FORTRESS, THE SHOP AT SHRINES AND AT HOME, AND THE STORY. THE SPELLS TAB OF "
        "THE MENU (Y) LISTS THEM: A MAKES A KNOWN SPELL THE ACTIVE ONE. A SECOND SCROLL OF A SPELL YOU KNOW RANKS IT UP.",
        NULL } },
    { "STAYING ALIVE",
      { "DRINK A HEALTH POTION (H) BEFORE YOU NEED IT, NOT AFTER; THE GAME REMINDS YOU WHEN YOU RUN LOW IN A FIGHT.\n"
        "A MEAL (K) HELPS FOR A WHILE: COOK AT A CAMPFIRE (P, FROM FIREWOOD) OR AT HOME. AN ELIXIR (V) IS STRONGER AND "
        "SHORTER; BREW THEM AT A SHRINE OR AT HOME.\n"
        "SHRINES ARE SAFE: TOUCH ONE TO BE HEALED, AND YOU WAKE THERE WHEN YOU FALL. A BED IN THE SANCTUM DOES THE SAME.\n"
        "WHEN YOU FALL YOU LOSE A FIFTH OF YOUR GOLD AND WAKE AT YOUR SHRINE OR BED (STRIKE OR JUMP TO RISE SOONER). FOR "
        "15 SECONDS YOU CAN PAY ANOTHER FIFTH TO GO BACK WHERE YOU FELL.\n"
        "COLD, HEAT, WATER AND GAS ARE DANGERS TOO: A FUR CLOAK KEEPS YOU WARM, AND YOU CAN DROWN.",
        "DRINK A HEALTH POTION (D-PAD UP) BEFORE YOU NEED IT, NOT AFTER; THE GAME REMINDS YOU WHEN YOU RUN LOW IN A FIGHT.\n"
        "A MEAL HELPS FOR A WHILE: COOK AT A CAMPFIRE OR AT HOME (THE FOOD TAB). AN ELIXIR IS STRONGER AND SHORTER; BREW THEM "
        "AT A SHRINE OR AT HOME (THE ALCHEMY TAB).\n"
        "SHRINES ARE SAFE: TOUCH ONE TO BE HEALED, AND YOU WAKE THERE WHEN YOU FALL. A BED IN THE SANCTUM DOES THE SAME.\n"
        "WHEN YOU FALL YOU LOSE A FIFTH OF YOUR GOLD AND WAKE AT YOUR SHRINE OR BED (A RISES SOONER). FOR 15 SECONDS YOU CAN "
        "PAY ANOTHER FIFTH TO GO BACK WHERE YOU FELL.\n"
        "COLD, HEAT, WATER AND GAS ARE DANGERS TOO: A FUR CLOAK KEEPS YOU WARM, AND YOU CAN DROWN.",
        NULL } },
    { "THE STORY",
      { "SOMEWHERE IN THE FORTRESS SITS THE OLD KING. REACHING HIM IS THE MAIN STORY: TEN STEPS TO THE TOWNS OF THE "
        "FRIENDLY PEOPLES, HOME TO YOUR SANCTUM, INTO DRAGONS' LAIRS, THROUGH THE GATE TO OTHER REALMS AND BACK.\n"
        "THE LINE AT THE TOP OF THE SCREEN SAYS THE NEXT STEP AND WHERE IT IS. G SPINS THE GOLDEN THREAD: A FADING LINE "
        "THAT LEADS THERE (PRESS AGAIN FOR THE NEXT GOAL).\n"
        "Y MOVES THE LINE TO YOUR CHRONICLE (WHY YOU CAME HERE: ITS CHAPTERS AND ITS MYSTERY) OR TO A TOWN'S TASK, AND BACK.\n"
        "L OPENS THE STORY TAB: WHAT YOU KNOW, YOUR DEEDS, WHO YOU SPARED.\n"
        "SOME YOU HUNT YIELD WHEN BEATEN: YOU MAY SPARE THEM. THE WORLD REMEMBERS WHAT YOU CHOSE.\n"
        "AT THE THRONE, BRING THE KING TO HIS KNEES; THEN THE SWORD, HIS TRUE NAME OR THE ARTEFACT ENDS IT.",
        "SOMEWHERE IN THE FORTRESS SITS THE OLD KING. REACHING HIM IS THE MAIN STORY: TEN STEPS TO THE TOWNS OF THE "
        "FRIENDLY PEOPLES, HOME TO YOUR SANCTUM, INTO DRAGONS' LAIRS, THROUGH THE GATE TO OTHER REALMS AND BACK.\n"
        "THE LINE AT THE TOP OF THE SCREEN SAYS THE NEXT STEP AND WHERE IT IS. THE RIGHT STICK CLICK SPINS THE GOLDEN "
        "THREAD: A FADING LINE THAT LEADS THERE (AGAIN FOR THE NEXT GOAL).\n"
        "THE STORY TAB OF THE MENU (Y) HOLDS YOUR CHRONICLE (WHY YOU CAME HERE), WHAT YOU KNOW AND YOUR DEEDS.\n"
        "SOME YOU HUNT YIELD WHEN BEATEN: YOU MAY SPARE THEM. THE WORLD REMEMBERS WHAT YOU CHOSE.\n"
        "AT THE THRONE, BRING THE KING TO HIS KNEES; THEN THE SWORD, HIS TRUE NAME OR THE ARTEFACT ENDS IT.",
        NULL } },
    { "THE WORLD",
      { "THE FORTRESS HAS LAYERS ABOVE AND BELOW (DUNGEON, MINES, HELLS; CASTLE, SKY, HEAVEN) AND REGIONS BY DIRECTION: "
        "COLD TO THE NORTH, FIRE TO THE SOUTH, STORMS TO THE EAST, VENOM TO THE WEST. EACH HAS ITS OWN FOES AND MATERIALS.\n"
        "TOWNS OF THE FRIENDLY PEOPLES ARE SAFE: REST AT THE INN, HEAL, TRADE, TAKE THEIR TASKS, PAY FOR THEIR WORKS.\n"
        "YOUR SANCTUM IS HOME: A DOOR APPEARS WHEN YOU CAST THE RITUAL, AND THERE YOU BUILD, SLEEP, BATHE, COOK, BREW AND FORGE.\n"
        "DRAGONS GUARD HOARDS IN THEIR LAIRS; THE GATE IN YOUR SANCTUM AND RIFTS IN THE FORTRESS LEAD TO OTHER REALMS.\n"
        "THE WORLD GOES ON WHILE YOU ARE AWAY: PLANTS GROW, WATER FLOWS, FIRES SPREAD, ANIMALS BREED AND HUNT. M SHOWS THE "
        "MAP OF WHAT YOU HAVE SEEN." } },
    { "MAKING THINGS",
      { "THE PICKAXE (4) DIGS THROUGH STONE AND OPENS ORE VEINS; ITS ALTERNATE ATTACK THROWS A BOMB. BOMBS BREAK MASONRY.\n"
        "THROW (X) A BOMB OR A FLASK: OIL BURNS, FROST SALT FREEZES, GROWTH TONIC MAKES THINGS GROW. C CHOOSES WHICH.\n"
        "COOKING (N): INGREDIENTS FROM ANIMALS, PLANTS AND LARDERS, AT A FIRE. A MEAL FITTED TO WHERE YOU GO HELPS MOST.\n"
        "ALCHEMY (O): REAGENTS FROM FOES AND PLANTS, AND FORMULAE YOU FIND, AT A SHRINE OR AT HOME.\n"
        "THE FORGE (U) AT YOUR ANVIL: SMELT ORE, FORGE WEAPONS ABOVE WHAT DROPS, SET RUNES; STRIKE IN TIME FOR A BETTER PIECE.\n"
        "CROUCH AND STRIKE THE EARTH WITH THE PICKAXE TO SOW A SEED." } },
    { "LOOT",
      { "WEAPONS COME IN FIVE TIERS: COMMON (IRON), MAGIC, RARE (GILDED), EPIC (BLACKENED) AND LEGENDARY (STARMETAL). YOU "
        "CAN SEE THE TIER IN HOW A WEAPON IS MADE, AND ITS ELEMENT ON ITS STEEL: ICE GROWS ON A WEAPON OF FROST.\n"
        "AFFIXES ADD AN ELEMENT, SPEED, WEIGHT OR A KEEN EDGE. RARER WEAPONS CARRY POWERS THAT TRIGGER IN A FIGHT.\n"
        "A WEAPON REMEMBERS ITS KILLS: THIRTY OF ONE KIND EARN IT A TITLE, AND IT AWAKENS AN AFFIX LATER STILL.\n"
        "EACH WORLD HAS ITS OWN LEGENDARIES, CARRIED BY NAMED FOES.\n"
        "IN THE ITEMS TAB ENTER EQUIPS, DEL SCRAPS FOR GOLD. GOLD BUYS RANKS, SEALED WEAPONS, KEYSTONES, BUILDING AT HOME "
        "AND THE TOWNS' WORKS." } },
};
#define TOPIC_COUNT ((int)(sizeof TOPICS / sizeof TOPICS[0]))

/* ---------- wrapping ---------- */

/* Writes the lines of text (paragraphs at \n) wrapped at maxc characters; returns how many. */
static int wrap_lines(const char *text, int maxc, char lines[][96], int maxl) {
    int n = 0;
    if (maxc > 95) maxc = 95;
    const char *p = text;
    while (*p && n < maxl) {
        const char *end = strchr(p, '\n');
        int plen = end ? (int)(end - p) : (int)strlen(p);
        int pos = 0;
        if (plen == 0) { lines[n++][0] = 0; }
        while (pos < plen && n < maxl) {
            while (pos < plen && p[pos] == ' ') pos++;
            int take = plen - pos < maxc ? plen - pos : maxc;
            if (pos + take < plen) {
                int k = take;
                while (k > 0 && p[pos + k] != ' ') k--;
                if (k > 0) take = k;
            }
            memcpy(lines[n], p + pos, (size_t)take);
            lines[n][take] = 0;
            n++;
            pos += take;
        }
        if (!end) break;
        p = end + 1;
        if (n < maxl && *p) lines[n++][0] = 0; /* a gap between paragraphs */
    }
    return n;
}

/* ---------- the HELP tab ---------- */

void help_nav(Game *g, PlayerSlot *ps, int nav) {
    (void)g;
    if (nav == NAV_UP) { ps->help_topic = (u8)((ps->help_topic + TOPIC_COUNT - 1) % TOPIC_COUNT); ps->help_scroll = 0; }
    else if (nav == NAV_DOWN) { ps->help_topic = (u8)((ps->help_topic + 1) % TOPIC_COUNT); ps->help_scroll = 0; }
    else if (nav == NAV_RIGHT) ps->help_scroll++;
    else if (nav == NAV_LEFT && ps->help_scroll > 0) ps->help_scroll--;
    else if (nav == NAV_BUY && ps->help_topic == 0 && !tutorial_active(ps)) { /* the tutorial once more */
        ps->tut_step = 1;
        ps->tut_t = 0;
        ps->show_skills = false;
    }
}

void help_draw(Game *g, PlayerSlot *ps, int W, int H, int top) {
    int dev = device_of(g, ps);
    bool compact = W < 400;
    int lw = compact ? 76 : 98; /* the list of topics */
    int x0 = 6, y0 = top + 8;
    batch2d_begin(W, H);
    for (int t = 0; t < TOPIC_COUNT; t++) {
        bool sel = t == ps->help_topic;
        batch2d_rect((float)x0, (float)(y0 + t * 12), (float)lw, 11, sel ? 0.35f : 0.08f, sel ? 0.25f : 0.07f, sel ? 0.12f : 0.06f, 0.95f);
    }
    batch2d_rect((float)(x0 + lw + 6), (float)y0, (float)(W - lw - 18), (float)(H - y0 - 16), 0.05f, 0.04f, 0.035f, 0.8f);
    batch2d_flush();
    font_begin(W, H);
    for (int t = 0; t < TOPIC_COUNT; t++) {
        float c = t == ps->help_topic ? 1.0f : 0.6f;
        font_text(x0 + 3, y0 + t * 12 + 2, TOPICS[t].title, c, c * 0.85f, c * 0.6f);
    }
    const HelpTopic *T = &TOPICS[ps->help_topic % TOPIC_COUNT];
    const char *text = T->text[dev] ? T->text[dev] : T->text[0];
    char lines[64][96];
    int tx = x0 + lw + 10, maxc = (W - tx - 8) / 6;
    int n = wrap_lines(text, maxc, lines, 64);
    int rows = (H - y0 - 30) / 9;
    int first = ps->help_scroll * (rows - 2);
    if (first >= n) { ps->help_scroll = 0; first = 0; }
    font_text(tx, y0 + 3, T->title, 1.0f, 0.82f, 0.45f);
    for (int i = 0; i < rows - 1 && first + i < n; i++) font_text(tx, y0 + 14 + i * 9, lines[first + i], 0.85f, 0.82f, 0.74f);
    const char *foot = dev == DEV_PAD ? "D-PAD UP/DOWN TOPIC  RIGHT/LEFT MORE  B CLOSE"
                     : dev == DEV_TOUCH ? "TAP A TOPIC; TAP THE PAGE FOR MORE"
                                        : "UP/DOWN TOPIC  RIGHT/LEFT MORE  F1 OR TAB CLOSE";
    if (first + rows - 1 < n) font_text(tx, H - 12, "MORE ->", 1.0f, 0.8f, 0.4f);
    font_text(W - font_text_width(foot) - 6, H - 12, foot, 0.55f, 0.5f, 0.45f);
    if (ps->help_topic == 0 && !tutorial_active(ps))
        font_text(x0, y0 + TOPIC_COUNT * 12 + 6, dev == DEV_PAD ? "A: TUTORIAL" : "ENTER: TUTORIAL", 0.7f, 0.75f, 0.9f);
    font_end();
}

/* A tap in the HELP tab: a topic, or the page (more of it). */
void help_tap(Game *g, PlayerSlot *ps, int W, int H, int top, float x, float y) {
    (void)g; (void)H;
    int lw = W < 400 ? 76 : 98, x0 = 6, y0 = top + 8;
    if (x >= x0 && x < x0 + lw) {
        int t = (int)((y - y0) / 12);
        if (t >= 0 && t < TOPIC_COUNT) { ps->help_topic = (u8)t; ps->help_scroll = 0; }
    } else if (x > x0 + lw) ps->help_scroll++;
}

/* ---------- the tutorial ---------- */

typedef struct TutStep {
    const char *title;
    const char *text[3]; /* keyboard, gamepad, touch */
} TutStep;

static const TutStep STEPS[] = {
    { 0 },
    { "MOVE", { "WASD WALKS, THE MOUSE LOOKS. SHIFT SPRINTS. WALK A FEW STEPS.", "THE LEFT STICK WALKS, THE RIGHT ONE LOOKS. CLICK THE LEFT STICK TO SPRINT. WALK A FEW STEPS.",
                "THE LEFT STICK WALKS; DRAG ELSEWHERE TO LOOK. WALK A FEW STEPS." } },
    { "JUMP", { "SPACE JUMPS; HOLD IT TO CLIMB A LEDGE. CTRL OR C CROUCHES. JUMP.", "A JUMPS; B CROUCHES. JUMP.", "THE JUMP BUTTON JUMPS, THE ONE BESIDE IT CROUCHES. JUMP." } },
    { "STRIKE", { "LEFT MOUSE STRIKES. RIGHT MOUSE IS THE ALTERNATE ATTACK: A WHIRL WITH THE BLADE (IT COSTS MANA). STRIKE.",
                  "RT STRIKES. LT IS THE ALTERNATE ATTACK: A WHIRL WITH THE BLADE (IT COSTS MANA). STRIKE.",
                  "THE BIG BUTTON STRIKES; THE BLUE ONE IS THE ALTERNATE ATTACK (IT COSTS MANA). STRIKE." } },
    { "WEAPONS", { "1 BLADE, 2 BOW, 3 STAFF, 4 PICKAXE; Z SWAPS BACK. ARROWS BRING DOWN FLIERS, MACES AND THE STAFF CRACK ARMOUR. TAKE UP THE BOW.",
                   "LB / RB CHANGE WEAPON: BLADE, BOW, STAFF, PICKAXE. ARROWS BRING DOWN FLIERS, MACES AND THE STAFF CRACK ARMOUR. TAKE UP ANOTHER.",
                   "THE WEAPON BUTTON TAKES UP THE NEXT: BLADE, BOW, STAFF, PICKAXE. ARROWS BRING DOWN FLIERS. TAKE UP ANOTHER." } },
    { "SPELLS", { "Q CASTS YOUR SPELL; THE BLUE BAR IS MANA. R CHOOSES ANOTHER, B OPENS THE SPELLBOOK. CAST A FLAME BOLT.",
                  "X CASTS YOUR SPELL; THE BLUE BAR IS MANA. THE D-PAD LEFT / RIGHT CHOOSES ANOTHER. CAST A FLAME BOLT.",
                  "THE SPELL BUTTON CASTS; THE BLUE BAR IS MANA. > CHOOSES ANOTHER. CAST A FLAME BOLT." } },
    { "STAY ALIVE", { "THE RED BAR IS HEALTH. H DRINKS A HEALTH POTION, J A MANA ONE. WOUNDS CLOSE OUT OF A FIGHT. TOUCH A SHRINE: YOU WAKE THERE IF YOU FALL.",
                      "THE RED BAR IS HEALTH. D-PAD UP DRINKS A HEALTH POTION, DOWN A MANA ONE. WOUNDS CLOSE OUT OF A FIGHT. TOUCH A SHRINE: YOU WAKE THERE IF YOU FALL.",
                      "THE RED BAR IS HEALTH; THE POTION BUTTON DRINKS ONE. WOUNDS CLOSE OUT OF A FIGHT. TOUCH A SHRINE: YOU WAKE THERE IF YOU FALL." } },
    { "YOUR STORY", { "THE LINE AT THE TOP IS THE WAY TO THE OLD KING. PRESS G: THE GOLDEN THREAD SHOWS WHERE TO GO. Y FOLLOWS YOUR CHRONICLE INSTEAD. F1 IS HELP.",
                      "THE LINE AT THE TOP IS THE WAY TO THE OLD KING. CLICK THE RIGHT STICK: THE GOLDEN THREAD SHOWS WHERE TO GO. THE MENU (Y) HAS HELP.",
                      "THE LINE AT THE TOP IS THE WAY TO THE OLD KING. TAP THE THREAD BUTTON: THE GOLDEN THREAD SHOWS WHERE TO GO. THE MENU HAS HELP." } },
};
#define TUT_LAST ((int)(sizeof STEPS / sizeof STEPS[0]) - 1)

/* Starts the tutorial for a new character (after they chose why they came). */
void tutorial_begin(Game *g, PlayerSlot *ps) {
    if (g->opt.bot || g->opt.demo || g->opt.shot || g->opt.no_tutorial || g->opt.test_pad) { ps->tut_step = TUT_DONE; return; }
    ps->tut_step = 1;
    ps->tut_t = -3.0f; /* (the chapter's hook is read first) */
    ps->tut_ok_t = 0;
}

void tutorial_skip(PlayerSlot *ps) {
    if (ps->tut_step == 0 || ps->tut_step == TUT_DONE) return;
    ps->tut_step = TUT_DONE;
    game_banner(ps, "TUTORIAL SKIPPED - F1 OR THE HELP TAB HAS IT ALL");
}

bool tutorial_active(const PlayerSlot *ps) { return ps->tut_step >= 1 && ps->tut_step <= TUT_LAST; }

/* The tutorial notices what the player did this frame. */
void tutorial_update(Game *g, PlayerSlot *ps, float dt) {
    if (!tutorial_active(ps)) return;
    Stats *s = &ps->st;
    if (ps->show_skills || ps->show_map || s->dead) return; /* (waits while a menu is open) */
    ps->tut_t += dt;
    if (ps->tut_t < 0) { ps->tut_from = ps->pl.pos; ps->tut_weapon = (u8)s->weapon; return; }
    if (ps->tut_ok_t > 0) { /* the tick after a step is done, then the next */
        ps->tut_ok_t -= dt;
        if (ps->tut_ok_t <= 0) {
            ps->tut_step++;
            ps->tut_t = 0;
            ps->tut_from = ps->pl.pos;
            ps->tut_weapon = (u8)(ps->pick_out ? WPN_COUNT : s->weapon);
            ps->tut_flag = false;
            if (ps->tut_step > TUT_LAST) { /* the story takes over */
                ps->tut_step = TUT_DONE;
                game_banner(ps, "YOUR STORY BEGINS: FOLLOW THE LINE AT THE TOP");
                sfx_ui(SFX_LEVEL_UP, 0.8f);
            }
        }
        return;
    }
    if (ps->tut_t < 1.5f) return; /* (a moment to read) */
    bool done = false;
    switch (ps->tut_step) {
    case 1: { vec3 d = v3_sub(ps->pl.pos, ps->tut_from); d.y = 0; done = v3_len(d) > 4.0f; break; }
    case 2: done = ps->pl.vel.y > 2.5f && !ps->pl.fly; break;
    case 3: done = s->swing > 0; break;
    case 4: done = (ps->pick_out ? WPN_COUNT : s->weapon) != ps->tut_weapon; break;
    case 5: done = s->spell_cd > 0.05f || ps->tut_flag; break;
    case 6: done = ps->tut_t > 9.0f || ps->tut_flag; break; /* (read, or drink) */
    case 7: done = ps->tut_flag; break;                      /* the Golden Thread */
    }
    if (ps->tut_step == 7 && ps->tut_t > 40.0f) done = true; /* (it does not hold anyone back) */
    if (done) {
        ps->tut_ok_t = 0.9f;
        sfx_ui(SFX_PICKUP_QUEST, 0.6f);
    }
    (void)g;
}

/* Things the tutorial cannot see in the player's state. */
void tutorial_note(PlayerSlot *ps, int what) {
    if (!tutorial_active(ps)) return;
    if ((what == TN_THREAD && ps->tut_step == 7) || (what == TN_POTION && ps->tut_step == 6) || (what == TN_CAST && ps->tut_step == 5)) ps->tut_flag = true;
}

void tutorial_draw(Game *g, PlayerSlot *ps, int W, int H) {
    if (!tutorial_active(ps) || ps->tut_t < 0 || ps->show_skills || ps->show_map) return;
    int dev = device_of(g, ps);
    const TutStep *st = &STEPS[ps->tut_step];
    char lines[8][96];
    int cw = W < 400 ? 150 : 190;
    int n = wrap_lines(st->text[dev], (cw - 10) / 6, lines, 8);
    const char *skip = dev == DEV_PAD ? "HOLD BACK: SKIP" : dev == DEV_TOUCH ? "" : "BACKSPACE: SKIP";
    float x = 4, y = H * 0.32f, h = 18.0f + n * 9 + (skip[0] ? 10.0f : 0.0f);
    bool ok = ps->tut_ok_t > 0;
    float a = fminf(1.0f, ps->tut_t * 3.0f + (ok ? 1.0f : 0.0f));
    batch2d_begin(W, H);
    batch2d_rect(x, y, (float)cw, h, 0.03f, 0.03f, 0.05f, 0.78f * a);
    batch2d_rect(x, y, 2, h, ok ? 0.4f : 0.95f, ok ? 0.95f : 0.75f, ok ? 0.45f : 0.3f, a);
    /* the steps, as dots along the top */
    for (int i = 1; i <= TUT_LAST; i++) {
        bool past = i < ps->tut_step || (i == ps->tut_step && ok);
        batch2d_rect(x + cw - 6.0f - (TUT_LAST - i) * 6.0f, y + 4, 4, 4, past ? 0.45f : i == ps->tut_step ? 0.95f : 0.3f,
                     past ? 0.9f : i == ps->tut_step ? 0.8f : 0.3f, past ? 0.45f : i == ps->tut_step ? 0.4f : 0.3f, a);
    }
    batch2d_flush();
    font_begin(W, H);
    char t[48];
    snprintf(t, sizeof t, "%s%s", st->title, ok ? "  - DONE" : "");
    font_text((int)x + 6, (int)y + 3, t, ok ? 0.55f : 1.0f, ok ? 1.0f : 0.82f, ok ? 0.55f : 0.45f);
    for (int i = 0; i < n; i++) font_text((int)x + 6, (int)y + 14 + i * 9, lines[i], 0.88f, 0.86f, 0.8f);
    if (skip[0]) font_text((int)x + 6, (int)(y + h - 10), skip, 0.45f, 0.43f, 0.4f);
    font_end();
}
