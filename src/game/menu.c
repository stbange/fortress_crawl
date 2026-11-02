#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* Start screen: choose the world seed before anything is generated.
 * A seed is a number or any word (words are hashed), so "dragon" is always the same world.
 * Keyboard: type, Backspace, Up/Down, Enter. Gamepad: D-pad, A start, Y random.
 * Touch / mouse: tap the rows, plus an on-screen keypad on touch screens. */

enum { ROW_SEED, ROW_RANDOM, ROW_START, ROW_COUNT };
enum { KEY_DEL = 10, KEY_RANDOM = 11, KEY_COUNT = 12 };

static void random_seed(StartMenu *m) {
    u64 r = mix64(SDL_GetPerformanceCounter() ^ (SDL_GetTicksNS() << 17) ^ (u64)(m->rolls++ * 0x9E3779B97F4A7C15ULL));
    snprintf(m->text, sizeof m->text, "%llu", (unsigned long long)(r % 900000 + 100000));
}

void menu_init(StartMenu *m) {
    memset(m, 0, sizeof *m);
    random_seed(m);
    m->fresh = true;
    m->row = ROW_START;
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

static void activate(StartMenu *m, int row) {
    if (row == ROW_RANDOM) { random_seed(m); m->fresh = true; m->row = ROW_RANDOM; }
    else if (m->text[0]) m->done = true;
}

/* Layout in low-res pixels. */
static void row_rect(int W, int row, float *x, float *y, float *w, float *h) {
    *w = 200; *h = 16;
    *x = (W - *w) * 0.5f;
    *y = 74.0f + row * 22.0f;
}

static void key_rect(int W, int H, int k, float *x, float *y, float *w, float *h) {
    /* 3 x 4 keypad below the rows: 1-9, then DEL 0 RND */
    int col, row;
    if (k >= 1 && k <= 9) { col = (k - 1) % 3; row = (k - 1) / 3; }
    else if (k == 0) { col = 1; row = 3; }
    else { col = k == KEY_DEL ? 0 : 2; row = 3; }
    *w = 40; *h = 20;
    float gx = (W - 3 * 44) * 0.5f, gy = (float)H - 4 * 24 - 8;
    *x = gx + col * 44;
    *y = gy + row * 24;
}

static bool inside(float px, float py, float x, float y, float w, float h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void tap(StartMenu *m, Game *g, float px, float py) {
    int W = g->r.fbo_w, H = g->r.fbo_h;
    for (int r = 0; r < ROW_COUNT; r++) {
        float x, y, w, h;
        row_rect(W, r, &x, &y, &w, &h);
        if (inside(px, py, x, y - 2, w, h + 4)) {
            m->row = r;
            if (r != ROW_SEED) activate(m, r);
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
    case SDL_EVENT_KEY_DOWN: {
        if (!g->touch.forced) g->touch.active = false;
        SDL_Keycode k = e->key.key;
        if (k >= SDLK_0 && k <= SDLK_9) type_char(m, (char)('0' + (k - SDLK_0)));
        else if (k >= SDLK_A && k <= SDLK_Z) type_char(m, (char)('A' + (k - SDLK_A)));
        else if (k == SDLK_MINUS) type_char(m, '-');
        else if (k == SDLK_BACKSPACE) backspace(m);
        else if (k == SDLK_UP) m->row = (m->row + ROW_COUNT - 1) % ROW_COUNT;
        else if (k == SDLK_DOWN || k == SDLK_TAB) m->row = (m->row + 1) % ROW_COUNT;
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) activate(m, m->row);
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
        int b = e->gbutton.button;
        m->pad = e->gbutton.which;
        if (b == SDL_GAMEPAD_BUTTON_DPAD_UP) m->row = (m->row + ROW_COUNT - 1) % ROW_COUNT;
        else if (b == SDL_GAMEPAD_BUTTON_DPAD_DOWN) m->row = (m->row + 1) % ROW_COUNT;
        else if (b == SDL_GAMEPAD_BUTTON_NORTH) { random_seed(m); m->fresh = true; }
        else if (b == SDL_GAMEPAD_BUTTON_SOUTH || b == SDL_GAMEPAD_BUTTON_START) activate(m, b == SDL_GAMEPAD_BUTTON_START ? ROW_START : m->row);
        else if ((b == SDL_GAMEPAD_BUTTON_DPAD_LEFT || b == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) && m->row == ROW_SEED) {
            /* step through neighbouring worlds */
            u64 s = menu_seed(m->text) + (b == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ? 1 : (u64)-1);
            snprintf(m->text, sizeof m->text, "%llu", (unsigned long long)s);
            m->fresh = false;
        }
        break;
    }
    default: break;
    }
}

static void frame_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    batch2d_rect(x, y, w, 1, r, g, b, a);
    batch2d_rect(x, y + h - 1, w, 1, r, g, b, a);
    batch2d_rect(x, y, 1, h, r, g, b, a);
    batch2d_rect(x + w - 1, y, 1, h, r, g, b, a);
}

void menu_draw(StartMenu *m, Game *g, float dt) {
    m->t += dt;
    Renderer *r = &g->r;
    renderer_frame_begin(r);
    int W = r->fbo_w, H = r->fbo_h;
    renderer_set_view(r, 0, 0, W, H);
    batch2d_begin(W, H);
    /* night sky over a mountain with the fortress gate glowing in it */
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
    const char *title = "FORTRESS CRAWL";
    font_text_scaled((W - font_text_width(title) * 2) / 2, 18, title, 2, 0.95f, 0.78f, 0.42f);
    const char *sub = "CHOOSE YOUR WORLD";
    font_text((W - font_text_width(sub)) / 2, 44, sub, 0.65f, 0.58f, 0.48f);
    char buf[96];
    float x, y, w, h;
    row_rect(W, ROW_SEED, &x, &y, &w, &h);
    bool blink = fmodf(m->t, 0.8f) < 0.4f && m->row == ROW_SEED;
    snprintf(buf, sizeof buf, "SEED: %s%s", m->text, blink ? "_" : "");
    font_text((int)x + 6, (int)y + 5, buf, 1.0f, 0.92f, 0.7f);
    row_rect(W, ROW_RANDOM, &x, &y, &w, &h);
    font_text((int)(x + (w - font_text_width("NEW RANDOM SEED")) / 2), (int)y + 5, "NEW RANDOM SEED", 0.85f, 0.78f, 0.6f);
    row_rect(W, ROW_START, &x, &y, &w, &h);
    const char *go = "ENTER THE FORTRESS";
    font_text((int)(x + (w - font_text_width(go)) / 2), (int)y + 5, go, 1.0f, 0.8f, 0.4f);
    if (g->touch.active) {
        for (int k = 0; k < KEY_COUNT; k++) {
            key_rect(W, H, k, &x, &y, &w, &h);
            const char *l = k == KEY_DEL ? "DEL" : k == KEY_RANDOM ? "RND" : NULL;
            char d[2] = { (char)('0' + k), 0 };
            if (!l) l = d;
            font_text((int)(x + (w - font_text_width(l)) / 2), (int)(y + 6), l, 0.9f, 0.82f, 0.65f);
        }
    } else {
        const char *h1 = "TYPE A NUMBER OR A WORD  -  UP/DOWN  -  ENTER";
        const char *h2 = "GAMEPAD: D-PAD, A START, Y RANDOM";
        const char *h3 = "THE SAME SEED ALWAYS MAKES THE SAME WORLD";
        font_text((W - font_text_width(h3)) / 2, H - 46, h3, 0.55f, 0.5f, 0.42f);
        font_text((W - font_text_width(h1)) / 2, H - 30, h1, 0.6f, 0.55f, 0.45f);
        font_text((W - font_text_width(h2)) / 2, H - 20, h2, 0.5f, 0.46f, 0.4f);
    }
    font_end();
    renderer_present(r, m->t);
}
