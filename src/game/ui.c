#include "game/game.h"

#include "engine/font.h"
#include "render/batch.h"

/* A small kit for the screens round the game (openspec front-end, settings): a list of rows
 * (an action, or a name and a value changed with left and right), a dialog that asks, and a page
 * of text that scrolls. Drawn in the low-res target with the bitmap font; every widget takes the
 * same moves (NAV_*) from the keyboard, a gamepad, the mouse and taps, so no screen needs one
 * device in particular. */

#define ROW_H 13
#define ROW_GAP 2

static void frame_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    batch2d_rect(x, y, w, 1, r, g, b, a);
    batch2d_rect(x, y + h - 1, w, 1, r, g, b, a);
    batch2d_rect(x, y, 1, h, r, g, b, a);
    batch2d_rect(x + w - 1, y, 1, h, r, g, b, a);
}

/* A footer of hints, centred at the bottom; too wide for the screen, it breaks where its parts
 * are two spaces apart, the lines stacked upward (openspec layout). Inside font_begin. */
void ui_footer(int W, int H, const char *text) {
    if (font_text_width(text) <= W - 4) { font_text((W - font_text_width(text)) / 2, H - 11, text, 0.55f, 0.5f, 0.45f); return; }
    char lines[4][96];
    int n = 0, len = 0;
    lines[0][0] = 0;
    for (const char *p = text; *p && n < 4;) {
        const char *e = strstr(p, "  ");
        int plen = e ? (int)(e - p) : (int)strlen(p);
        int cur = (int)strlen(lines[n]);
        if (cur && (cur + 2 + plen) * 6 > W - 4) { if (n == 3) break; lines[++n][0] = 0; cur = 0; }
        snprintf(lines[n] + cur, sizeof lines[n] - (size_t)cur, "%s%.*s", cur ? "  " : "", plen, p);
        len++;
        if (!e) break;
        p = e;
        while (*p == ' ') p++;
    }
    (void)len;
    for (int i = 0; i <= n; i++) font_text((W - font_text_width(lines[i])) / 2, H - 11 - (n - i) * 9, lines[i], 0.55f, 0.5f, 0.45f);
}

void ui_dim(int W, int H, float a) {
    batch2d_begin(W, H);
    batch2d_rect(0, 0, (float)W, (float)H, 0.02f, 0.015f, 0.02f, a);
    batch2d_flush();
}

/* Where the list lies: centred, under the title, as many rows as fit. */
static void list_layout(int W, int H, int top, int *x, int *y, int *w, int *rows) {
    *w = W < 400 ? W - 24 : 300;
    *x = (W - *w) / 2;
    *y = top + 18;
    *rows = (H - *y - 20) / (ROW_H + ROW_GAP);
    if (*rows < 3) *rows = 3;
}

static bool selectable(const UiItem *it) { return it->label[0] && !it->header && !it->disabled; }

void ui_list_fix(UiList *l, const UiItem *items, int n, int rows) {
    if (n <= 0) { l->sel = 0; l->scroll = 0; return; }
    if (l->sel < 0) l->sel = 0;
    if (l->sel >= n) l->sel = n - 1;
    for (int k = 0; k < n && !selectable(&items[l->sel]); k++) l->sel = (l->sel + 1) % n;
    if (rows > 0) {
        if (l->sel < l->scroll) l->scroll = l->sel;
        if (l->sel >= l->scroll + rows) l->scroll = l->sel - rows + 1;
        if (l->scroll > n - rows) l->scroll = n - rows > 0 ? n - rows : 0;
        if (l->scroll < 0) l->scroll = 0;
    }
}

void ui_list_nav(UiList *l, const UiItem *items, int n, int nav) {
    if (n <= 0) return;
    int d = nav == NAV_UP ? -1 : nav == NAV_DOWN ? 1 : 0;
    if (!d) return;
    for (int k = 0; k < n; k++) {
        l->sel = (l->sel + d + n) % n;
        if (selectable(&items[l->sel])) break;
    }
}

void ui_list_draw(int W, int H, int top, const char *title, const UiItem *items, int n, UiList *l, const char *footer) {
    int x, y, w, rows;
    list_layout(W, H, top, &x, &y, &w, &rows);
    ui_list_fix(l, items, n, rows);
    batch2d_begin(W, H);
    for (int i = 0; i < rows && l->scroll + i < n; i++) {
        const UiItem *it = &items[l->scroll + i];
        float ry = (float)(y + i * (ROW_H + ROW_GAP));
        if (it->header || !it->label[0]) continue;
        bool sel = l->scroll + i == l->sel;
        batch2d_rect((float)x, ry, (float)w, ROW_H, sel ? 0.22f : 0.07f, sel ? 0.16f : 0.055f, sel ? 0.08f : 0.045f, 0.9f);
        frame_rect((float)x, ry, (float)w, ROW_H, sel ? 0.95f : 0.32f, sel ? 0.75f : 0.26f, sel ? 0.35f : 0.18f, 1.0f);
    }
    if (l->scroll > 0) batch2d_rect((float)(x + w / 2 - 6), (float)(y - 5), 12, 2, 0.9f, 0.75f, 0.4f, 0.9f);
    if (l->scroll + rows < n) batch2d_rect((float)(x + w / 2 - 6), (float)(y + rows * (ROW_H + ROW_GAP) + 1), 12, 2, 0.9f, 0.75f, 0.4f, 0.9f);
    batch2d_flush();
    font_begin(W, H);
    if (title && title[0]) font_text((W - font_text_width(title)) / 2, top + 4, title, 1.0f, 0.82f, 0.45f);
    for (int i = 0; i < rows && l->scroll + i < n; i++) {
        const UiItem *it = &items[l->scroll + i];
        int ry = y + i * (ROW_H + ROW_GAP) + 3;
        bool sel = l->scroll + i == l->sel;
        float c = it->disabled ? 0.4f : sel ? 1.0f : 0.78f;
        if (it->header) {
            font_text(x + 2, ry, it->label, 0.6f, 0.75f, 0.9f);
            continue;
        }
        if (it->value[0]) {
            font_text(x + 6, ry, it->label, c, c * 0.9f, c * 0.7f);
            char v[56];
            snprintf(v, sizeof v, sel && !it->disabled ? "< %s >" : "%s", it->value);
            font_text(x + w - 6 - font_text_width(v), ry, v, c * 0.95f, c * 0.85f, c * 0.55f);
        } else {
            font_text(x + (w - font_text_width(it->label)) / 2, ry, it->label, c, c * 0.85f, c * 0.55f);
        }
    }
    if (footer && footer[0]) ui_footer(W, H, footer);
    font_end();
}

/* A tap or click at (px, py): the row hit (selected), and for a row with a value which side of it
 * (*dir -1 left half, +1 right half, 0 the row itself). Returns the row, or -1. */
int ui_list_tap(int W, int H, int top, const UiItem *items, int n, UiList *l, float px, float py, int *dir) {
    int x, y, w, rows;
    list_layout(W, H, top, &x, &y, &w, &rows);
    *dir = 0;
    if (px < x || px >= x + w || py < y) return -1;
    int i = (int)((py - y) / (ROW_H + ROW_GAP));
    if (i < 0 || i >= rows || l->scroll + i >= n) return -1;
    int k = l->scroll + i;
    if (!selectable(&items[k])) return -1;
    l->sel = k;
    if (items[k].value[0]) *dir = px < x + w * 0.55f ? -1 : 1;
    return k;
}

/* ---------- dialogs ---------- */

void ui_dialog_open(UiDialog *d, int action, const char *title, const char *text, const char *yes, const char *no) {
    d->open = true;
    d->action = action;
    d->sel = 1; /* the safe choice */
    snprintf(d->title, sizeof d->title, "%s", title);
    snprintf(d->text, sizeof d->text, "%s", text);
    snprintf(d->yes, sizeof d->yes, "%s", yes);
    snprintf(d->no, sizeof d->no, "%s", no);
}

static void dialog_rect(int W, int H, float *x, float *y, float *w, float *h) {
    *w = W < 400 ? (float)W - 20 : 300;
    *h = 74;
    *x = (W - *w) * 0.5f;
    *y = (H - *h) * 0.5f;
}

static void button_rect(int W, int H, int b, float *x, float *y, float *w, float *h) {
    float dx, dy, dw, dh;
    dialog_rect(W, H, &dx, &dy, &dw, &dh);
    *w = dw * 0.42f;
    *h = 14;
    *y = dy + dh - 20;
    *x = b == 0 ? dx + dw * 0.05f : dx + dw * 0.53f;
}

void ui_dialog_draw(int W, int H, const UiDialog *d) {
    if (!d->open) return;
    float x, y, w, h;
    dialog_rect(W, H, &x, &y, &w, &h);
    batch2d_begin(W, H);
    batch2d_rect(0, 0, (float)W, (float)H, 0.0f, 0.0f, 0.0f, 0.55f);
    batch2d_rect(x, y, w, h, 0.06f, 0.045f, 0.04f, 0.97f);
    frame_rect(x, y, w, h, 0.9f, 0.7f, 0.35f, 1.0f);
    for (int b = 0; b < 2; b++) {
        float bx, by, bw, bh;
        button_rect(W, H, b, &bx, &by, &bw, &bh);
        bool sel = d->sel == b;
        batch2d_rect(bx, by, bw, bh, sel ? 0.25f : 0.08f, sel ? 0.17f : 0.06f, sel ? 0.08f : 0.05f, 1.0f);
        frame_rect(bx, by, bw, bh, sel ? 0.95f : 0.4f, sel ? 0.75f : 0.32f, sel ? 0.35f : 0.22f, 1.0f);
    }
    batch2d_flush();
    font_begin(W, H);
    font_text((int)(x + (w - font_text_width(d->title)) / 2), (int)y + 6, d->title, 1.0f, 0.8f, 0.45f);
    /* the text, wrapped to two lines */
    int maxc = (int)(w - 12) / 6;
    char l1[96] = "", l2[96] = "";
    int len = (int)strlen(d->text);
    if (len <= maxc) snprintf(l1, sizeof l1, "%s", d->text);
    else {
        int k = maxc;
        while (k > 0 && d->text[k] != ' ') k--;
        if (k == 0) k = maxc;
        snprintf(l1, sizeof l1, "%.*s", k, d->text);
        snprintf(l2, sizeof l2, "%.*s", maxc, d->text + k + (d->text[k] == ' '));
    }
    font_text((int)(x + (w - font_text_width(l1)) / 2), (int)y + 22, l1, 0.85f, 0.82f, 0.74f);
    if (l2[0]) font_text((int)(x + (w - font_text_width(l2)) / 2), (int)y + 32, l2, 0.85f, 0.82f, 0.74f);
    for (int b = 0; b < 2; b++) {
        float bx, by, bw, bh;
        button_rect(W, H, b, &bx, &by, &bw, &bh);
        const char *t = b == 0 ? d->yes : d->no;
        float c = d->sel == b ? 1.0f : 0.7f;
        font_text((int)(bx + (bw - font_text_width(t)) / 2), (int)by + 4, t, c, c * 0.85f, c * 0.6f);
    }
    font_end();
}

/* Returns 1 when the first choice (yes) is taken, 0 for the other (or back), -1 while open. */
int ui_dialog_nav(UiDialog *d, int nav) {
    if (!d->open) return -1;
    if (nav == NAV_LEFT || nav == NAV_UP) d->sel = 0;
    else if (nav == NAV_RIGHT || nav == NAV_DOWN) d->sel = 1;
    else if (nav == NAV_BUY) { d->open = false; return d->sel == 0 ? 1 : 0; }
    else if (nav == NAV_BACK) { d->open = false; return 0; }
    return -1;
}

int ui_dialog_tap(UiDialog *d, int W, int H, float px, float py) {
    if (!d->open) return -1;
    for (int b = 0; b < 2; b++) {
        float bx, by, bw, bh;
        button_rect(W, H, b, &bx, &by, &bw, &bh);
        if (px >= bx && px < bx + bw && py >= by - 2 && py < by + bh + 2) {
            d->sel = b;
            return ui_dialog_nav(d, NAV_BUY);
        }
    }
    return -1;
}

/* ---------- a page of text ---------- */

/* Lines separated by '\n'; scroll is in lines. Returns how many lines there are. */
int ui_page_draw(int W, int H, int top, const char *title, const char *text, int *scroll) {
    int x = W < 400 ? 10 : 40, w = W - 2 * x, maxc = w / 6, y0 = top + 18, rows = (H - y0 - 16) / 9;
    char lines[160][80];
    int n = 0;
    const char *p = text;
    while (*p && n < 160) { /* wrap each paragraph */
        const char *e = strchr(p, '\n');
        int plen = e ? (int)(e - p) : (int)strlen(p);
        if (plen == 0) lines[n++][0] = 0;
        int pos = 0;
        while (pos < plen && n < 160) {
            int take = plen - pos < maxc ? plen - pos : maxc;
            if (take > 79) take = 79;
            if (pos + take < plen) {
                int k = take;
                while (k > 0 && p[pos + k] != ' ') k--;
                if (k > 0) take = k;
            }
            memcpy(lines[n], p + pos, (size_t)take);
            lines[n][take] = 0;
            n++;
            pos += take;
            while (pos < plen && p[pos] == ' ') pos++;
        }
        if (!e) break;
        p = e + 1;
    }
    if (*scroll > n - rows) *scroll = n - rows;
    if (*scroll < 0) *scroll = 0;
    font_begin(W, H);
    if (title && title[0]) font_text((W - font_text_width(title)) / 2, top + 4, title, 1.0f, 0.82f, 0.45f);
    for (int i = 0; i < rows && *scroll + i < n; i++) font_text(x, y0 + i * 9, lines[*scroll + i], 0.86f, 0.83f, 0.74f);
    if (*scroll + rows < n) font_text(W - x - font_text_width("MORE"), H - 11, "MORE", 1.0f, 0.8f, 0.4f);
    font_end();
    return n;
}

/* ---------- the frame ---------- */

/* The UI's frame for this frame: full (the start screen, or one player) uses the whole target
 * inset by the safe areas and the margin, with pixels as big as the text size asks; with touch
 * controls at least big enough that the smallest button is 9 mm (about 34 CSS pixels) across;
 * never so big that the UI has less than 240 x 135 to lay out in (the size of a split-screen
 * quarter, the smallest every screen is made for). Split screen: the plain target. */
void ui_frame_update(Game *g, bool full) {
    UiFrame *f = &g->ui;
    int W = g->r.set.low_w, H = g->r.set.low_h; /* (the target is made this size at the frame's start) */
    if (!full) { *f = (UiFrame){ 1.0f, 0, 0, W, H, W, H }; return; }
    float m = g_set.margin / 100.0f;
    int l = (int)(g->safe[0] + W * m), t = (int)(g->safe[1] + H * m), r = (int)(g->safe[2] + W * m), b = (int)(g->safe[3] + H * m);
    int vw = MAXI(64, W - l - r), vh = MAXI(64, H - t - b);
    float s = g_set.text_size / 100.0f;
    if (g->touch.active && g->pts_w > 0) {
        float css = g->pts_w / (float)W; /* CSS pixels per target pixel */
        s = fmaxf(s, 34.0f / (22.0f * css));
    }
    s = fminf(s, fminf(vw / 240.0f, vh / 135.0f));
    if (s < 1.0f) s = 1.0f;
    *f = (UiFrame){ s, l, t, vw, vh, (int)(vw / s), (int)(vh / s) };
}

void ui_frame_view(const Game *g) { renderer_set_view((Renderer *)&g->r, g->ui.x, g->ui.y, g->ui.vw, g->ui.vh); }
