#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* The start screen (openspec front-end): CONTINUE the world played last, a NEW WORLD from a seed,
 * the list of WORLDS saved here (each can be played or deleted), the SETTINGS, the HELP and the
 * CREDITS, and on the desktop QUIT. A seed is a number or any word (words are hashed), so
 * "dragon" is always the same world. Every screen takes the keyboard (arrows, Enter, Esc), a
 * gamepad (D-pad, A, B; X deletes, Y a random seed), the mouse and taps; whatever cannot be undone
 * asks first. */

enum { MI_CONTINUE, MI_NEW, MI_WORLDS, MI_SETTINGS, MI_HELP, MI_CREDITS, MI_QUIT, MI_COUNT };
enum { ROW_SEED, ROW_RANDOM, ROW_START, ROW_FRESH, ROW_BACK, ROW_COUNT };
enum { KEY_DEL = 10, KEY_RANDOM = 11, KEY_COUNT = 12 };
enum { DLG_FRESH = 1, DLG_DELETE, DLG_QUIT };

#define MAIN_TOP 64

static PlayerSlot help_ps; /* the help pages' place (topic, page) on the start screen */

static const char *CREDITS =
    "FORTRESS CRAWL\n"
    "A first-person voxel dungeon crawler. The fortress, its stories, the realms, the textures and the "
    "weapons are generated from the seed when you play; nothing of the world is drawn by hand.\n"
    "\n"
    "MADE WITH\n"
    "SDL 3 (Simple DirectMedia Layer) - zlib licence. Copyright (C) 1997-2025 Sam Lantinga.\n"
    "Emscripten (the browser build) - MIT and University of Illinois/NCSA licences.\n"
    "OpenGL 3.3 on the desktop, WebGL 2 in the browser.\n"
    "\n"
    "SOUND AND MUSIC\n"
    "The sound effects and the music were made with ElevenLabs (tools/gen_sfx.mjs and tools/gen_music.mjs).\n"
    "\n"
    "LETTERS AND PICTURES\n"
    "The bitmap font and every texture are the game's own, drawn by its code.\n"
    "\n"
    "THANK YOU FOR PLAYING.";

static void random_seed(StartMenu *m) {
    u64 r = mix64(SDL_GetPerformanceCounter() ^ (SDL_GetTicksNS() << 17) ^ (u64)(m->rolls++ * 0x9E3779B97F4A7C15ULL));
    snprintf(m->text, sizeof m->text, "%llu", (unsigned long long)(r % 900000 + 100000));
}

u64 menu_seed(const char *text) {
    if (!text[0]) return 1337;
    bool digits = true;
    for (const char *p = text; *p; p++) digits &= *p >= '0' && *p <= '9';
    if (digits && strlen(text) <= 18) return strtoull(text, NULL, 10);
    /* words: FNV-1a, case-insensitive */
    u64 h = 0xcbf29ce484222325ULL;
    for (const char *p = text; *p; p++) {
        char c = *p >= 'a' && *p <= 'z' ? (char)(*p - 32) : *p;
        h = (h ^ (u8)c) * 0x100000001b3ULL;
    }
    return h;
}

static int cmp_world(const void *a, const void *b) {
    const SaveInfo *x = a, *y = b;
    return strcmp(x->seed_text, y->seed_text);
}

/* The worlds saved here (their keys end in the seed in hex), and the one played last. */
static void scan_worlds(StartMenu *m) {
    char keys[MENU_MAX_WORLDS][64];
    int n = store_list("fortress_crawl:save:", keys, MENU_MAX_WORLDS);
    m->nworlds = 0;
    for (int i = 0; i < n; i++) {
        const char *hex = strrchr(keys[i], ':');
        const char *us = strrchr(keys[i], '_');
        if (!hex || (us && us > hex)) hex = us;
        if (!hex || strlen(hex + 1) != 16) continue;
        u64 seed = strtoull(hex + 1, NULL, 16);
        SaveInfo info;
        if (!save_peek(seed, &info)) continue;
        if (!info.seed_text[0]) snprintf(info.seed_text, sizeof info.seed_text, "%llu", (unsigned long long)seed);
        m->worlds[m->nworlds++] = info;
    }
    qsort(m->worlds, (size_t)m->nworlds, sizeof m->worlds[0], cmp_world);
    char last[24];
    m->has_last = save_last_seed(last, sizeof last) && save_peek(menu_seed(last), &m->last);
    if (m->has_last && !m->last.seed_text[0]) snprintf(m->last.seed_text, sizeof m->last.seed_text, "%s", last);
}

void menu_init(StartMenu *m) {
    memset(m, 0, sizeof *m);
    scan_worlds(m);
    /* the world played last is suggested for a new world too; typing replaces it */
    if (!save_last_seed(m->text, sizeof m->text)) random_seed(m);
    m->fresh = true;
    m->row = ROW_START;
    m->screen = MS_MAIN;
    m->list.sel = m->has_last ? MI_CONTINUE : MI_NEW;
    m->set.list.sel = 1;
}

void menu_show(StartMenu *m, int screen) {
    m->screen = screen;
    m->dlg.open = false;
    if (screen == MS_WORLDS) { scan_worlds(m); m->wlist.sel = 0; m->wlist.scroll = 0; }
    if (screen == MS_NEW) { m->row = ROW_START; m->peeked[0] = 0; m->peeked[1] = 1; }
    if (screen == MS_MAIN) scan_worlds(m);
}

static void play(StartMenu *m, const char *seed_text) {
    if (seed_text != m->text) snprintf(m->text, sizeof m->text, "%s", seed_text);
    m->done = true;
}

static void type_char(StartMenu *m, char c) {
    size_t n = strlen(m->text);
    if (m->fresh) { n = 0; m->fresh = false; } /* typing replaces the suggested seed */
    if (n + 1 < sizeof m->text) { m->text[n] = c; m->text[n + 1] = 0; }
    m->row = ROW_SEED;
}

static void backspace(StartMenu *m) {
    size_t n = strlen(m->text);
    m->fresh = false;
    if (n) m->text[n - 1] = 0;
}

/* What is saved for the seed in the field (looked up when the text changes). */
static void refresh_save_info(StartMenu *m) {
    if (!strcmp(m->peeked, m->text)) return;
    snprintf(m->peeked, sizeof m->peeked, "%s", m->text);
    m->has_save = m->text[0] && save_peek(menu_seed(m->text), &m->save);
}

/* ---------- the rows of each screen ---------- */

static int main_items(StartMenu *m, UiItem *it) {
    memset(it, 0, sizeof(UiItem) * MI_COUNT);
    for (int i = 0; i < MI_COUNT; i++) it[i].id = i;
    if (m->has_last) snprintf(it[MI_CONTINUE].label, sizeof it[0].label, "CONTINUE: %.30s", m->last.seed_text);
    else { snprintf(it[MI_CONTINUE].label, sizeof it[0].label, "CONTINUE"); it[MI_CONTINUE].disabled = true; }
    snprintf(it[MI_NEW].label, sizeof it[0].label, "NEW WORLD");
    snprintf(it[MI_WORLDS].label, sizeof it[0].label, "WORLDS (%d)", m->nworlds);
    if (!m->nworlds) it[MI_WORLDS].disabled = true;
    snprintf(it[MI_SETTINGS].label, sizeof it[0].label, "SETTINGS");
    snprintf(it[MI_HELP].label, sizeof it[0].label, "HELP");
    snprintf(it[MI_CREDITS].label, sizeof it[0].label, "CREDITS");
#ifdef __EMSCRIPTEN__
    return MI_QUIT; /* a browser tab is closed by the browser */
#else
    snprintf(it[MI_QUIT].label, sizeof it[0].label, "QUIT");
    return MI_COUNT;
#endif
}

static int world_items(StartMenu *m, UiItem *it) {
    int n = 0;
    for (int i = 0; i < m->nworlds; i++, n++) {
        memset(&it[n], 0, sizeof it[n]);
        const SaveInfo *s = &m->worlds[i];
        int mins = (int)(s->play_time / 60.0f);
        snprintf(it[n].label, sizeof it[n].label, "%.14s  LV %d  %dH %02dM", s->seed_text, s->level, mins / 60, mins % 60);
        it[n].id = i;
    }
    memset(&it[n], 0, sizeof it[n]);
    it[n].header = true; /* a gap */
    n++;
    memset(&it[n], 0, sizeof it[n]);
    snprintf(it[n].label, sizeof it[n].label, "BACK");
    it[n].id = -1;
    return n + 1;
}

/* ---------- moves ---------- */

static void dialog_yes(StartMenu *m, Game *g) {
    (void)g;
    if (m->dlg.action == DLG_FRESH) {
        save_delete(menu_seed(m->text));
        play(m, m->text);
    } else if (m->dlg.action == DLG_DELETE) {
        int k = m->wlist.sel;
        if (k >= 0 && k < m->nworlds) {
            save_delete(m->worlds[k].seed);
            sfx_ui(SFX_MENU_CLOSE, 0.7f);
        }
        scan_worlds(m);
        if (!m->nworlds) menu_show(m, MS_MAIN);
    } else if (m->dlg.action == DLG_QUIT) m->quit = true;
}

static void new_activate(StartMenu *m, int row) {
    if (row == ROW_RANDOM) { random_seed(m); m->fresh = true; m->row = ROW_RANDOM; }
    else if (row == ROW_BACK) menu_show(m, MS_MAIN);
    else if (row == ROW_FRESH) {
        if (!m->has_save) return;
        char t[160];
        snprintf(t, sizeof t, "THE CHARACTERS SAVED IN WORLD %.20s (LEVEL %d) ARE LOST FOR GOOD, AND NEW ONES START.", m->text, m->save.level);
        ui_dialog_open(&m->dlg, DLG_FRESH, "START AGAIN?", t, "NEW CHARACTERS", "KEEP THEM");
    } else if (m->text[0]) play(m, m->text);
}

static void nav(StartMenu *m, Game *g, int nv) {
    if (m->dlg.open) {
        int r = ui_dialog_nav(&m->dlg, nv);
        if (r == 1) dialog_yes(m, g);
        return;
    }
    switch (m->screen) {
    case MS_MAIN: {
        UiItem it[MI_COUNT];
        int n = main_items(m, it);
        if (nv == NAV_UP || nv == NAV_DOWN) { ui_list_nav(&m->list, it, n, nv); sfx_ui(SFX_MENU_MOVE, 0.5f); }
        else if (nv == NAV_BUY) {
            ui_list_fix(&m->list, it, n, 0);
            sfx_ui(SFX_MENU_OPEN, 0.6f);
            switch (it[m->list.sel].id) {
            case MI_CONTINUE: if (m->has_last) play(m, m->last.seed_text); break;
            case MI_NEW: menu_show(m, MS_NEW); break;
            case MI_WORLDS: menu_show(m, MS_WORLDS); break;
            case MI_SETTINGS: menu_show(m, MS_SETTINGS); break;
            case MI_HELP: menu_show(m, MS_HELP); break;
            case MI_CREDITS: m->credits_scroll = 0; menu_show(m, MS_CREDITS); break;
            case MI_QUIT: ui_dialog_open(&m->dlg, DLG_QUIT, "QUIT?", "YOUR WORLDS ARE SAVED. COME BACK SOON.", "QUIT", "STAY"); break;
            }
        }
        break;
    }
    case MS_NEW:
        if (nv == NAV_BACK) menu_show(m, MS_MAIN);
        else if (nv == NAV_UP) m->row = (m->row + ROW_COUNT - 1) % ROW_COUNT;
        else if (nv == NAV_DOWN) m->row = (m->row + 1) % ROW_COUNT;
        else if (nv == NAV_BUY) new_activate(m, m->row);
        else if ((nv == NAV_LEFT || nv == NAV_RIGHT) && m->row == ROW_SEED) {
            /* step through neighbouring worlds */
            u64 s = menu_seed(m->text) + (nv == NAV_RIGHT ? 1 : (u64)-1);
            snprintf(m->text, sizeof m->text, "%llu", (unsigned long long)s);
            m->fresh = false;
        }
        break;
    case MS_WORLDS: {
        UiItem it[MENU_MAX_WORLDS + 2];
        int n = world_items(m, it);
        if (nv == NAV_BACK) menu_show(m, MS_MAIN);
        else if (nv == NAV_UP || nv == NAV_DOWN) { ui_list_nav(&m->wlist, it, n, nv); sfx_ui(SFX_MENU_MOVE, 0.5f); }
        else if (nv == NAV_BUY) {
            int id = it[m->wlist.sel].id;
            if (id < 0) menu_show(m, MS_MAIN);
            else play(m, m->worlds[id].seed_text);
        } else if (nv == NAV_DROP) {
            int id = it[m->wlist.sel].id;
            if (id < 0) return;
            char t[160];
            const SaveInfo *s = &m->worlds[id];
            snprintf(t, sizeof t, "WORLD %.20s: LEVEL %d, %d GOLD, %d KILLS. ITS CHARACTERS AND ALL THAT CHANGED IN IT ARE LOST FOR GOOD.",
                     s->seed_text, s->level, s->gold, s->kills);
            ui_dialog_open(&m->dlg, DLG_DELETE, "DELETE THIS WORLD?", t, "DELETE", "KEEP IT");
        }
        break;
    }
    case MS_SETTINGS:
        if (settings_screen_nav(g, &m->set, nv)) menu_show(m, MS_MAIN);
        break;
    case MS_HELP:
        if (nv == NAV_BACK) menu_show(m, MS_MAIN);
        else if (nv != NAV_BUY) help_nav(g, &help_ps, nv);
        break;
    case MS_CREDITS:
        if (nv == NAV_BACK || nv == NAV_BUY) menu_show(m, MS_MAIN);
        else if (nv == NAV_DOWN || nv == NAV_RIGHT) m->credits_scroll++;
        else if ((nv == NAV_UP || nv == NAV_LEFT) && m->credits_scroll > 0) m->credits_scroll--;
        break;
    }
}

/* ---------- the NEW WORLD screen's layout (low-res pixels) ---------- */

static void row_rect(int W, int row, float *x, float *y, float *w, float *h) {
    *w = 220; *h = 15;
    *x = (W - *w) * 0.5f;
    *y = 70.0f + row * 19.0f;
}

static void key_rect(int W, int H, int k, float *x, float *y, float *w, float *h) {
    /* 3 x 4 keypad below the rows: 1-9, then DEL 0 RND */
    int col, row;
    if (k >= 1 && k <= 9) { col = (k - 1) % 3; row = (k - 1) / 3; }
    else if (k == 0) { col = 1; row = 3; }
    else { col = k == KEY_DEL ? 0 : 2; row = 3; }
    *w = 40; *h = 18;
    float gx = (W - 3 * 44) * 0.5f, gy = (float)H - 4 * 21 - 6;
    *x = gx + col * 44;
    *y = gy + row * 21;
}

static bool inside(float px, float py, float x, float y, float w, float h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void tap(StartMenu *m, Game *g, float px, float py) {
    int W = g->r.fbo_w, H = g->r.fbo_h;
    if (m->dlg.open) {
        if (ui_dialog_tap(&m->dlg, W, H, px, py) == 1) dialog_yes(m, g);
        return;
    }
    int dir;
    switch (m->screen) {
    case MS_MAIN: {
        UiItem it[MI_COUNT];
        int n = main_items(m, it);
        if (ui_list_tap(W, H, MAIN_TOP, it, n, &m->list, px, py, &dir) >= 0) nav(m, g, NAV_BUY);
        break;
    }
    case MS_WORLDS: {
        UiItem it[MENU_MAX_WORLDS + 2];
        int n = world_items(m, it);
        if (ui_list_tap(W, H, MAIN_TOP - 20, it, n, &m->wlist, px, py, &dir) >= 0) nav(m, g, NAV_BUY);
        break;
    }
    case MS_SETTINGS:
        if (settings_screen_tap(g, &m->set, W, H, 8, px, py)) menu_show(m, MS_MAIN);
        break;
    case MS_HELP:
        if (py < 14) menu_show(m, MS_MAIN); /* the title bar goes back */
        else help_tap(g, &help_ps, W, H, 14, px, py);
        break;
    case MS_CREDITS:
        if (py < H / 2) menu_show(m, MS_MAIN);
        else m->credits_scroll++;
        break;
    case MS_NEW:
        for (int r = 0; r < ROW_COUNT; r++) {
            float x, y, w, h;
            row_rect(W, r, &x, &y, &w, &h);
            if (inside(px, py, x, y - 2, w, h + 4)) {
                m->row = r;
                if (r != ROW_SEED) new_activate(m, r);
                return;
            }
        }
        if (!g->touch.active) return;
        for (int k = 0; k < KEY_COUNT; k++) {
            float x, y, w, h;
            key_rect(W, H, k, &x, &y, &w, &h);
            if (!inside(px, py, x, y, w, h)) continue;
            if (k == KEY_DEL) backspace(m);
            else if (k == KEY_RANDOM) { random_seed(m); m->fresh = true; }
            else type_char(m, (char)('0' + k));
            return;
        }
        break;
    }
}

void menu_event(StartMenu *m, Game *g, const SDL_Event *e) {
    switch (e->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
        game_event(g, e); /* opens the pad so it can join later */
        break;
    case SDL_EVENT_FINGER_DOWN: {
        g->touch.active = true;
        float x, y;
        touch_to_low(g, e->tfinger.x, e->tfinger.y, &x, &y);
        tap(m, g, x, y);
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (e->button.which == SDL_TOUCH_MOUSEID) break;
        SDL_Window *win = SDL_GetWindowFromID(e->button.windowID);
        int ww = 1, wh = 1;
        if (win) SDL_GetWindowSize(win, &ww, &wh);
        float x, y;
        touch_to_low(g, e->button.x / ww, e->button.y / wh, &x, &y);
        tap(m, g, x, y);
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        if (!m->dlg.open && m->screen != MS_NEW) nav(m, g, e->wheel.y > 0 ? NAV_UP : NAV_DOWN);
        break;
    case SDL_EVENT_KEY_DOWN: {
        if (!g->touch.forced) g->touch.active = false;
        SDL_Keycode k = e->key.key;
        if (k == SDLK_ESCAPE) { nav(m, g, NAV_BACK); break; }
        if (k == SDLK_UP) { nav(m, g, NAV_UP); break; }
        if (k == SDLK_DOWN || (k == SDLK_TAB && m->screen == MS_NEW)) { nav(m, g, NAV_DOWN); break; }
        if (k == SDLK_LEFT) { nav(m, g, NAV_LEFT); break; }
        if (k == SDLK_RIGHT) { nav(m, g, NAV_RIGHT); break; }
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER || (k == SDLK_SPACE && m->screen != MS_NEW)) { if (!e->key.repeat) nav(m, g, NAV_BUY); break; }
        if (m->dlg.open) break;
        if (m->screen == MS_WORLDS && (k == SDLK_DELETE || k == SDLK_BACKSPACE || k == SDLK_X)) { nav(m, g, NAV_DROP); break; }
        if (m->screen == MS_HELP || m->screen == MS_CREDITS || m->screen == MS_SETTINGS) {
            if (k == SDLK_BACKSPACE || k == SDLK_F1) nav(m, g, NAV_BACK);
            break;
        }
        bool ch = (k >= SDLK_0 && k <= SDLK_9) || (k >= SDLK_A && k <= SDLK_Z) || k == SDLK_MINUS;
        if (ch && m->screen == MS_MAIN) menu_show(m, MS_NEW); /* typing on the first screen starts a new world's seed */
        if (m->screen != MS_NEW) break;
        if (k >= SDLK_0 && k <= SDLK_9) type_char(m, (char)('0' + (k - SDLK_0)));
        else if (k >= SDLK_A && k <= SDLK_Z) type_char(m, (char)('A' + (k - SDLK_A)));
        else if (k == SDLK_MINUS) type_char(m, '-');
        else if (k == SDLK_BACKSPACE) backspace(m);
        else if (k == SDLK_SPACE) nav(m, g, NAV_BUY);
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
        int b = e->gbutton.button;
        m->pad = e->gbutton.which;
        if (b == SDL_GAMEPAD_BUTTON_DPAD_UP) nav(m, g, NAV_UP);
        else if (b == SDL_GAMEPAD_BUTTON_DPAD_DOWN) nav(m, g, NAV_DOWN);
        else if (b == SDL_GAMEPAD_BUTTON_DPAD_LEFT) nav(m, g, NAV_LEFT);
        else if (b == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) nav(m, g, NAV_RIGHT);
        else if (b == SDL_GAMEPAD_BUTTON_SOUTH) nav(m, g, NAV_BUY);
        else if (b == SDL_GAMEPAD_BUTTON_EAST) nav(m, g, NAV_BACK);
        else if (b == SDL_GAMEPAD_BUTTON_WEST) nav(m, g, NAV_DROP);
        else if (b == SDL_GAMEPAD_BUTTON_NORTH && m->screen == MS_NEW && !m->dlg.open) { random_seed(m); m->fresh = true; }
        else if (b == SDL_GAMEPAD_BUTTON_START) {
            if (m->screen == MS_NEW && !m->dlg.open) new_activate(m, ROW_START);
            else nav(m, g, NAV_BUY);
        }
        break;
    }
    default: break;
    }
}

/* ---------- drawing ---------- */

static void frame_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    batch2d_rect(x, y, w, 1, r, g, b, a);
    batch2d_rect(x, y + h - 1, w, 1, r, g, b, a);
    batch2d_rect(x, y, 1, h, r, g, b, a);
    batch2d_rect(x + w - 1, y, 1, h, r, g, b, a);
}

/* night sky over a mountain with the fortress gate glowing in it */
static void backdrop(StartMenu *m, int W, int H) {
    batch2d_begin(W, H);
    for (int y = 0; y < H; y += 3) {
        float k = (float)y / H;
        batch2d_rect(0, (float)y, (float)W, 3, 0.03f + 0.05f * k, 0.025f + 0.02f * k, 0.05f + 0.02f * k, 1.0f);
    }
    for (int x = 0; x < W; x += 2) {
        float fx = (float)x / W;
        float mh = 0.55f - 0.35f * expf(-((fx - 0.5f) * (fx - 0.5f)) * 9.0f) + 0.03f * sinf(fx * 37.0f) + 0.02f * sinf(fx * 91.0f);
        batch2d_rect((float)x, mh * H, 2, H - mh * H, 0.02f, 0.018f, 0.025f, 1.0f);
    }
    for (int i = 0; i < 24; i++) {
        /* embers drifting up */
        float ph = fmodf(m->t * (0.05f + 0.01f * (i % 5)) + i * 0.137f, 1.0f);
        float ex = fmodf(i * 97.3f, (float)W) + sinf(m->t + i) * 6.0f, ey = H * (1.0f - ph);
        batch2d_rect(ex, ey, 1, 1, 1.0f, 0.5f + 0.3f * (i % 2), 0.2f, 0.7f * (1.0f - ph));
    }
    batch2d_flush();
}

static void title(int W, const char *sub) {
    const char *t = "FORTRESS CRAWL";
    font_text_scaled((W - font_text_width(t) * 2) / 2, 18, t, 2, 0.95f, 0.78f, 0.42f);
    if (sub) font_text((W - font_text_width(sub)) / 2, 42, sub, 0.65f, 0.58f, 0.48f);
}

static void draw_new(StartMenu *m, Game *g, int W, int H) {
    refresh_save_info(m);
    batch2d_begin(W, H);
    for (int row = 0; row < ROW_COUNT; row++) {
        float x, y, w, h;
        row_rect(W, row, &x, &y, &w, &h);
        bool sel = m->row == row;
        batch2d_rect(x, y, w, h, 0.08f, 0.06f, 0.05f, 0.85f);
        if (sel) frame_rect(x - 1, y - 1, w + 2, h + 2, 0.95f, 0.75f, 0.35f, 1.0f);
        else frame_rect(x, y, w, h, 0.35f, 0.28f, 0.2f, 1.0f);
    }
    if (g->touch.active)
        for (int k = 0; k < KEY_COUNT; k++) {
            float x, y, w, h;
            key_rect(W, H, k, &x, &y, &w, &h);
            batch2d_rect(x, y, w, h, 0.1f, 0.08f, 0.06f, 0.85f);
            frame_rect(x, y, w, h, 0.45f, 0.36f, 0.25f, 1.0f);
        }
    batch2d_flush();

    font_begin(W, H);
    title(W, "NEW WORLD - CHOOSE A SEED");
    char buf[96];
    float x, y, w, h;
    row_rect(W, ROW_SEED, &x, &y, &w, &h);
    bool blink = fmodf(m->t, 0.8f) < 0.4f && m->row == ROW_SEED;
    snprintf(buf, sizeof buf, "SEED: %s%s", m->text, blink ? "_" : "");
    font_text((int)x + 6, (int)y + 4, buf, 1.0f, 0.92f, 0.7f);
#define CENTRE(row, text, r, gg, b) do { row_rect(W, row, &x, &y, &w, &h); font_text((int)(x + (w - font_text_width(text)) / 2), (int)y + 4, text, r, gg, b); } while (0)
    CENTRE(ROW_RANDOM, "NEW RANDOM SEED", 0.85f, 0.78f, 0.6f);
    CENTRE(ROW_START, m->has_save ? "CONTINUE THIS WORLD" : "ENTER THE FORTRESS", 1.0f, 0.8f, 0.4f);
    float fk = m->has_save ? 0.75f : 0.35f;
    CENTRE(ROW_FRESH, m->has_save ? "START AGAIN: NEW CHARACTERS" : "NO SAVE FOR THIS WORLD", fk, fk * 0.85f, fk * 0.6f);
    CENTRE(ROW_BACK, "BACK", 0.75f, 0.68f, 0.55f);
#undef CENTRE
    if (m->has_save) {
        int mins = (int)(m->save.play_time / 60.0f);
        snprintf(buf, sizeof buf, "SAVED: LEVEL %d  -  %d GOLD  -  %d KILLS  -  %dH %02dM", m->save.level, m->save.gold, m->save.kills,
                 mins / 60, mins % 60);
        font_text((W - font_text_width(buf)) / 2, 56, buf, 0.55f, 0.85f, 0.5f);
    }
    if (g->touch.active) {
        for (int k = 0; k < KEY_COUNT; k++) {
            key_rect(W, H, k, &x, &y, &w, &h);
            const char *l = k == KEY_DEL ? "DEL" : k == KEY_RANDOM ? "RND" : NULL;
            char d[2] = { (char)('0' + k), 0 };
            if (!l) l = d;
            font_text((int)(x + (w - font_text_width(l)) / 2), (int)(y + 5), l, 0.9f, 0.82f, 0.65f);
        }
    } else {
        const char *h1 = "TYPE A NUMBER OR A WORD  -  UP/DOWN  -  ENTER  -  ESC BACK";
        const char *h2 = "GAMEPAD: D-PAD, A CHOOSE, B BACK, Y RANDOM";
        const char *h3 = "THE SAME SEED ALWAYS MAKES THE SAME WORLD";
        font_text((W - font_text_width(h3)) / 2, H - 42, h3, 0.55f, 0.5f, 0.42f);
        font_text((W - font_text_width(h1)) / 2, H - 28, h1, 0.6f, 0.55f, 0.45f);
        font_text((W - font_text_width(h2)) / 2, H - 18, h2, 0.5f, 0.46f, 0.4f);
    }
    font_end();
}

void menu_draw(StartMenu *m, Game *g, float dt) {
    m->t += dt;
    Renderer *r = &g->r;
    renderer_frame_begin(r);
    int W = r->fbo_w, H = r->fbo_h;
    renderer_set_view(r, 0, 0, W, H);
    backdrop(m, W, H);
    switch (m->screen) {
    case MS_MAIN: {
        UiItem it[MI_COUNT];
        int n = main_items(m, it);
        font_begin(W, H);
        title(W, NULL);
        if (m->has_last && it[m->list.sel].id == MI_CONTINUE) {
            char buf[96];
            int mins = (int)(m->last.play_time / 60.0f);
            snprintf(buf, sizeof buf, "LEVEL %d  -  %d GOLD  -  %d KILLS  -  %dH %02dM", m->last.level, m->last.gold, m->last.kills, mins / 60, mins % 60);
            font_text((W - font_text_width(buf)) / 2, 44, buf, 0.55f, 0.85f, 0.5f);
        }
        font_end();
        const char *foot = g->touch.active ? "TAP TO CHOOSE" : "UP/DOWN CHOOSE  ENTER / A OPEN";
        ui_list_draw(W, H, MAIN_TOP - 18, NULL, it, n, &m->list, foot);
        break;
    }
    case MS_NEW: draw_new(m, g, W, H); break;
    case MS_WORLDS: {
        UiItem it[MENU_MAX_WORLDS + 2];
        int n = world_items(m, it);
        font_begin(W, H);
        title(W, NULL);
        font_end();
        const char *foot = g->touch.active ? "TAP A WORLD TO PLAY IT" : "ENTER / A PLAY  DEL / X DELETE  ESC / B BACK";
        ui_list_draw(W, H, MAIN_TOP - 20, "WORLDS SAVED HERE", it, n, &m->wlist, foot);
        break;
    }
    case MS_SETTINGS:
        ui_dim(W, H, 0.5f);
        settings_screen_draw(g, &m->set, W, H, 8);
        break;
    case MS_HELP:
        ui_dim(W, H, 0.6f);
        help_ps.index = 0;
        help_ps.keyboard = true;
        help_ps.tut_step = 1; /* (no "ENTER: TUTORIAL" here: there is no character yet) */
        help_draw(g, &help_ps, W, H, 14);
        font_begin(W, H);
        font_text(6, 4, "HELP - ESC / B BACK", 1.0f, 0.82f, 0.45f);
        font_end();
        break;
    case MS_CREDITS:
        ui_dim(W, H, 0.6f);
        ui_page_draw(W, H, 8, "CREDITS", CREDITS, &m->credits_scroll);
        font_begin(W, H);
        font_text(6, H - 11, "ESC / B BACK", 0.55f, 0.5f, 0.45f);
        font_end();
        break;
    }
    ui_dialog_draw(W, H, &m->dlg);
    font_begin(W, H);
    font_text(W - font_text_width(FC_VERSION) - 4, 4, FC_VERSION, 0.4f, 0.37f, 0.33f); /* the build (openspec front-end: version) */
    font_end();
    renderer_present(r, m->t);
}
