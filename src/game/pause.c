#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* The pause menu (openspec menus → Pause): Esc, START on a pad, or the pause button on touch.
 * Alone, it stops the world as any open menu does; in co-op it is that player's own and the
 * world goes on. From it: resume, the settings, the help, and save and quit (to the start
 * screen, or on the desktop out of the game). */

enum { PI_RESUME, PI_SETTINGS, PI_HELP, PI_TITLE, PI_QUIT, PI_COUNT };
enum { PS_MAIN, PS_SETTINGS, PS_HELP };

#define PAUSE_TOP 16

bool pause_is_open(const PlayerSlot *ps) { return ps->pause_open; }

void pause_open(Game *g, PlayerSlot *ps) {
    if (!ps->active || ps->pause_open || g->opt.shot || g->opt.bot) return;
    ps->pause_open = true;
    ps->pause_screen = PS_MAIN;
    ps->pause_list.sel = PI_RESUME;
    ps->pause_list.scroll = 0;
    ps->show_skills = ps->show_map = false;
    ps->attack_held = ps->alt_held = ps->spell_held = false;
    sfx_ui(SFX_MENU_OPEN, 0.7f);
}

void pause_close(Game *g, PlayerSlot *ps) {
    if (!ps->pause_open) return;
    ps->pause_open = false;
    sfx_ui(SFX_MENU_CLOSE, 0.7f);
    (void)g;
}

static int build(Game *g, UiItem *it) {
    memset(it, 0, sizeof(UiItem) * PI_COUNT);
    static const char *L[PI_COUNT] = { "RESUME", "SETTINGS", "HELP", "SAVE AND QUIT TO TITLE", "SAVE AND QUIT GAME" };
    for (int i = 0; i < PI_COUNT; i++) { snprintf(it[i].label, sizeof it[i].label, "%s", L[i]); it[i].id = i; }
#ifdef __EMSCRIPTEN__
    it[PI_QUIT].label[0] = 0; /* a browser tab is closed by the browser */
    return PI_QUIT;
#else
    (void)g;
    return PI_COUNT;
#endif
}

void pause_draw(Game *g, PlayerSlot *ps, int W, int H) {
    ui_dim(W, H, 0.72f);
    if (ps->pause_screen == PS_SETTINGS) { settings_screen_draw(g, &ps->pause_set, W, H, PAUSE_TOP); return; }
    if (ps->pause_screen == PS_HELP) {
        help_draw(g, ps, W, H, PAUSE_TOP);
        font_begin(W, H);
        font_text(6, PAUSE_TOP - 12, "HELP - ESC/B BACK", 1.0f, 0.82f, 0.45f);
        font_end();
        return;
    }
    UiItem it[PI_COUNT];
    int n = build(g, it);
    const char *title = g->nplayers > 1 ? "MENU - THE WORLD GOES ON" : "PAUSED";
    ui_list_draw(W, H, PAUSE_TOP + 20, title, it, n, &ps->pause_list, "ESC / START / B: BACK TO THE GAME");
    font_begin(W, H);
    char buf[96];
    snprintf(buf, sizeof buf, "WORLD %s  -  %s", g->opt.seed_text, FC_VERSION);
    font_text((W - font_text_width(buf)) / 2, PAUSE_TOP + 4, buf, 0.5f, 0.48f, 0.42f);
    font_end();
}

void pause_nav(Game *g, PlayerSlot *ps, int nav) {
    if (ps->pause_screen == PS_SETTINGS) {
        if (settings_screen_nav(g, &ps->pause_set, nav)) ps->pause_screen = PS_MAIN;
        return;
    }
    if (ps->pause_screen == PS_HELP) {
        if (nav == NAV_BACK) ps->pause_screen = PS_MAIN;
        else help_nav(g, ps, nav);
        return;
    }
    UiItem it[PI_COUNT];
    int n = build(g, it);
    if (nav == NAV_BACK) { pause_close(g, ps); return; }
    if (nav == NAV_UP || nav == NAV_DOWN) { ui_list_nav(&ps->pause_list, it, n, nav); sfx_ui(SFX_MENU_MOVE, 0.5f); return; }
    if (nav != NAV_BUY) return;
    switch (it[ps->pause_list.sel].id) {
    case PI_RESUME: pause_close(g, ps); break;
    case PI_SETTINGS: ps->pause_screen = PS_SETTINGS; ps->pause_set.list.sel = 1; break;
    case PI_HELP: ps->pause_screen = PS_HELP; break;
    case PI_TITLE: g->quit_request = QUIT_TO_TITLE; break; /* main.c saves and goes */
    case PI_QUIT: g->quit_request = QUIT_GAME; break;
    }
}

void pause_tap(Game *g, PlayerSlot *ps, int W, int H, float x, float y) {
    if (ps->pause_screen == PS_SETTINGS) {
        if (settings_screen_tap(g, &ps->pause_set, W, H, PAUSE_TOP, x, y)) ps->pause_screen = PS_MAIN;
        return;
    }
    if (ps->pause_screen == PS_HELP) {
        if (y < PAUSE_TOP + 6) ps->pause_screen = PS_MAIN; /* the title bar goes back */
        else help_tap(g, ps, W, H, PAUSE_TOP, x, y);
        return;
    }
    UiItem it[PI_COUNT];
    int n = build(g, it);
    int dir;
    if (ui_list_tap(W, H, PAUSE_TOP + 20, it, n, &ps->pause_list, x, y, &dir) >= 0) pause_nav(g, ps, NAV_BUY);
}
