#include "game/game.h"

#include "engine/audio.h"
#include "engine/font.h"
#include "render/batch.h"

/* Settings (openspec settings): one set for the game, kept between sessions as key=value lines
 * (a file on the desktop, localStorage in the browser), so a key an older build does not know is
 * skipped and a missing one takes its default. The settings screen is reached from the start
 * screen and the pause menu; the option keys (F2-F10, - and =) change the same values. Test,
 * screenshot and bot runs neither read nor write them, so they stay the same everywhere. */

Settings g_set;
static bool g_enabled; /* read and written (not in test runs) */

#define SETTINGS_KEY "fortress_crawl:settings"

void settings_defaults(Settings *s) {
    memset(s, 0, sizeof *s);
    s->res_mode = 1;
    s->fullscreen = false;
    s->vsync = true;
    s->fps_cap = 0;
    s->bright = 1.0f;
    s->dither = true;
    s->vsnap = false;
    s->torch = true;
    s->vol_master = 100;
    s->vol_sfx = 100;
    s->vol_music = 100;
    s->mute = false;
    s->music_on = true;
    s->sens_mouse = 1.0f;
    s->sens_pad = 1.0f;
    s->invert_x = s->invert_y = false;
    s->vibration = true;
    s->line_top = true;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void sanitize(Settings *s) {
    s->res_mode = clampi(s->res_mode, 0, 2);
    if (s->fps_cap != 0 && s->fps_cap != 30 && s->fps_cap != 60 && s->fps_cap != 120) s->fps_cap = 0;
    s->bright = clampf(s->bright, 0.6f, 1.6f);
    s->vol_master = clampi(s->vol_master, 0, 100);
    s->vol_sfx = clampi(s->vol_sfx, 0, 100);
    s->vol_music = clampi(s->vol_music, 0, 100);
    s->sens_mouse = clampf(s->sens_mouse, 0.2f, 5.0f);
    s->sens_pad = clampf(s->sens_pad, 0.5f, 2.0f);
}

/* ---------- the file ---------- */

typedef struct Field { const char *key; char type; size_t off; } Field; /* i int, b bool, f float */
#define F(k, t, m) { k, t, offsetof(Settings, m) }
static const Field FIELDS[] = {
    F("res_mode", 'i', res_mode), F("fullscreen", 'b', fullscreen), F("vsync", 'b', vsync), F("fps_cap", 'i', fps_cap),
    F("brightness", 'f', bright), F("dither", 'b', dither), F("vertex_snap", 'b', vsnap), F("torch", 'b', torch),
    F("volume_master", 'i', vol_master), F("volume_effects", 'i', vol_sfx), F("volume_music", 'i', vol_music),
    F("mute", 'b', mute), F("music", 'b', music_on), F("mouse_sensitivity", 'f', sens_mouse),
    F("stick_sensitivity", 'f', sens_pad), F("invert_x", 'b', invert_x), F("invert_y", 'b', invert_y),
    F("vibration", 'b', vibration), F("line_at_top", 'b', line_top),
};
#undef F
#define NFIELDS ((int)(sizeof FIELDS / sizeof FIELDS[0]))

void settings_parse(Settings *s, const char *text) {
    settings_defaults(s);
    for (const char *p = text; p && *p;) {
        const char *e = strchr(p, '\n');
        char line[128];
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, p, len);
        line[len] = 0;
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = 0;
            for (int i = 0; i < NFIELDS; i++) {
                if (strcmp(line, FIELDS[i].key)) continue;
                char *dst = (char *)s + FIELDS[i].off;
                if (FIELDS[i].type == 'i') *(int *)dst = atoi(eq + 1);
                else if (FIELDS[i].type == 'b') *(bool *)dst = atoi(eq + 1) != 0;
                else *(float *)dst = (float)atof(eq + 1);
            }
        }
        p = e ? e + 1 : NULL;
    }
    sanitize(s);
}

int settings_format(const Settings *s, char *out, int cap) {
    int n = 0;
    for (int i = 0; i < NFIELDS && n < cap; i++) {
        const char *src = (const char *)s + FIELDS[i].off;
        if (FIELDS[i].type == 'i') n += snprintf(out + n, (size_t)(cap - n), "%s=%d\n", FIELDS[i].key, *(const int *)src);
        else if (FIELDS[i].type == 'b') n += snprintf(out + n, (size_t)(cap - n), "%s=%d\n", FIELDS[i].key, *(const bool *)src ? 1 : 0);
        else n += snprintf(out + n, (size_t)(cap - n), "%s=%.3f\n", FIELDS[i].key, *(const float *)src);
    }
    return n < cap ? n : cap - 1;
}

void settings_load(bool enabled) {
    g_enabled = enabled;
    settings_defaults(&g_set);
    if (!enabled) return;
    int n = 0;
    u8 *d = store_read(SETTINGS_KEY, &n);
    if (d) { settings_parse(&g_set, (const char *)d); free(d); }
}

void settings_save(void) {
    if (!g_enabled) return;
    char buf[2048];
    int n = settings_format(&g_set, buf, sizeof buf);
    if (!store_write(SETTINGS_KEY, (const u8 *)buf, n)) SDL_Log("Settings: could not save");
}

/* ---------- applying ---------- */

void settings_apply(Game *g) {
    static const int RES[3][2] = { { 320, 180 }, { 480, 270 }, { 640, 360 } };
    g->res_mode = g_set.res_mode;
    if (!g->opt.cli_lowres) {
        g->r.set.low_w = RES[g_set.res_mode][0];
        g->r.set.low_h = RES[g_set.res_mode][1];
    }
    g->r.set.dither = g_set.dither;
    g->r.set.vertex_snap = g_set.vsnap;
    g->r.set.torch = g_set.torch;
    g->r.set.bright = g_set.bright;
    if (!g->opt.cli_sens) g->opt.sens = g_set.sens_mouse;
    if (!g->opt.cli_invert) { g->opt.invert_x = g_set.invert_x; g->opt.invert_y = g_set.invert_y; }
    sfx_set_levels(g_set.vol_master / 100.0f, g_set.vol_sfx / 100.0f, g_set.vol_music / 100.0f, g_set.music_on);
    sfx_set_mute(g_set.mute || g->opt.mute);
    g->window_dirty = true; /* window mode, v-sync and the frame cap: main.c */
}

/* An option key changed something: keep it (the value already in the game is copied here). */
void settings_from_game(Game *g) {
    /* (what the command line set for this run is not kept) */
    if (!g->opt.cli_lowres) g_set.res_mode = g->res_mode;
    g_set.dither = g->r.set.dither;
    g_set.vsnap = g->r.set.vertex_snap;
    g_set.torch = g->r.set.torch;
    if (!g->opt.cli_sens) g_set.sens_mouse = g->opt.sens;
    if (!g->opt.cli_invert) { g_set.invert_x = g->opt.invert_x; g_set.invert_y = g->opt.invert_y; }
    if (!g->opt.mute) g_set.mute = sfx_muted();
    g_set.music_on = music_on();
    sanitize(&g_set);
    settings_save();
}

/* ---------- the screen ---------- */

enum {
    SI_H_GRAPHICS, SI_RES, SI_WINDOW, SI_VSYNC, SI_CAP, SI_BRIGHT, SI_DITHER, SI_SNAP, SI_TORCH,
    SI_H_AUDIO, SI_MASTER, SI_SFX, SI_MUSIC, SI_MUTE,
    SI_H_CONTROLS, SI_SENS_MOUSE, SI_SENS_PAD, SI_INVERT_X, SI_INVERT_Y, SI_VIBRATION,
    SI_H_INTERFACE, SI_LINE,
    SI_GAP, SI_RESET, SI_BACK, SI_COUNT
};

static const char *onoff(bool b) { return b ? "ON" : "OFF"; }

static int build(UiItem *it) {
    static const char *RES[3] = { "320 X 180", "480 X 270", "640 X 360" };
    memset(it, 0, sizeof(UiItem) * SI_COUNT);
    for (int i = 0; i < SI_COUNT; i++) it[i].id = i;
#define HDR(i, t) do { snprintf(it[i].label, sizeof it[i].label, "%s", t); it[i].header = true; } while (0)
#define ROW(i, l, ...) do { snprintf(it[i].label, sizeof it[i].label, "%s", l); snprintf(it[i].value, sizeof it[i].value, __VA_ARGS__); } while (0)
    HDR(SI_H_GRAPHICS, "GRAPHICS");
    ROW(SI_RES, "RESOLUTION", "%s", RES[g_set.res_mode]);
    ROW(SI_WINDOW, "WINDOW", "%s", g_set.fullscreen ? "FULLSCREEN" : "WINDOWED");
    ROW(SI_VSYNC, "V-SYNC", "%s", onoff(g_set.vsync));
    if (g_set.fps_cap) ROW(SI_CAP, "FRAME CAP", "%d FPS", g_set.fps_cap);
    else ROW(SI_CAP, "FRAME CAP", "OFF");
    ROW(SI_BRIGHT, "BRIGHTNESS", "%d%%", (int)(g_set.bright * 100.0f + 0.5f));
    ROW(SI_DITHER, "DITHERING", "%s", onoff(g_set.dither));
    ROW(SI_SNAP, "VERTEX SNAP", "%s", onoff(g_set.vsnap));
    ROW(SI_TORCH, "TORCH", "%s", onoff(g_set.torch));
    HDR(SI_H_AUDIO, "AUDIO");
    ROW(SI_MASTER, "MASTER VOLUME", "%d%%", g_set.vol_master);
    ROW(SI_SFX, "EFFECTS", "%d%%", g_set.vol_sfx);
    ROW(SI_MUSIC, "MUSIC", "%d%%", g_set.vol_music);
    ROW(SI_MUTE, "MUTE ALL", "%s", onoff(g_set.mute));
    HDR(SI_H_CONTROLS, "CONTROLS");
    ROW(SI_SENS_MOUSE, "MOUSE SENSITIVITY", "%.2f", g_set.sens_mouse);
    ROW(SI_SENS_PAD, "STICK SENSITIVITY", "%.1f", g_set.sens_pad);
    ROW(SI_INVERT_X, "INVERT LOOK X", "%s", onoff(g_set.invert_x));
    ROW(SI_INVERT_Y, "INVERT LOOK Y", "%s", onoff(g_set.invert_y));
    ROW(SI_VIBRATION, "VIBRATION", "%s", onoff(g_set.vibration));
    HDR(SI_H_INTERFACE, "INTERFACE");
    ROW(SI_LINE, "LINE AT THE TOP", "%s", onoff(g_set.line_top));
    it[SI_GAP].header = true;
    snprintf(it[SI_RESET].label, sizeof it[SI_RESET].label, "RESET TO DEFAULTS");
    snprintf(it[SI_BACK].label, sizeof it[SI_BACK].label, "BACK");
#undef HDR
#undef ROW
#ifdef __EMSCRIPTEN__
    /* the browser draws when it will: no v-sync or frame cap to choose */
    it[SI_VSYNC].disabled = it[SI_CAP].disabled = true;
    it[SI_WINDOW].disabled = true; /* (the browser's own full-screen button) */
#endif
    return SI_COUNT;
}

static void step(int id, int d) {
    static const int CAPS[4] = { 0, 30, 60, 120 };
    switch (id) {
    case SI_RES: g_set.res_mode = (g_set.res_mode + d + 3) % 3; break;
    case SI_WINDOW: g_set.fullscreen = !g_set.fullscreen; break;
    case SI_VSYNC: g_set.vsync = !g_set.vsync; break;
    case SI_CAP: {
        int k = 0;
        while (k < 3 && CAPS[k] != g_set.fps_cap) k++;
        g_set.fps_cap = CAPS[(k + d + 4) % 4];
        break;
    }
    case SI_BRIGHT: g_set.bright = clampf(g_set.bright + 0.1f * d, 0.6f, 1.6f); break;
    case SI_DITHER: g_set.dither = !g_set.dither; break;
    case SI_SNAP: g_set.vsnap = !g_set.vsnap; break;
    case SI_TORCH: g_set.torch = !g_set.torch; break;
    case SI_MASTER: g_set.vol_master = clampi(g_set.vol_master + 10 * d, 0, 100); break;
    case SI_SFX: g_set.vol_sfx = clampi(g_set.vol_sfx + 10 * d, 0, 100); break;
    case SI_MUSIC: g_set.vol_music = clampi(g_set.vol_music + 10 * d, 0, 100); break;
    case SI_MUTE: g_set.mute = !g_set.mute; break;
    case SI_SENS_MOUSE: g_set.sens_mouse = clampf(d > 0 ? g_set.sens_mouse * 1.15f : g_set.sens_mouse / 1.15f, 0.2f, 5.0f); break;
    case SI_SENS_PAD: g_set.sens_pad = clampf(g_set.sens_pad + 0.1f * d, 0.5f, 2.0f); break;
    case SI_INVERT_X: g_set.invert_x = !g_set.invert_x; break;
    case SI_INVERT_Y: g_set.invert_y = !g_set.invert_y; break;
    case SI_VIBRATION: g_set.vibration = !g_set.vibration; break;
    case SI_LINE: g_set.line_top = !g_set.line_top; break;
    default: return;
    }
    sfx_ui(SFX_MENU_MOVE, 0.6f);
}

static void changed(Game *g) {
    sanitize(&g_set);
    settings_apply(g);
    settings_save();
}

void settings_screen_draw(Game *g, SettingsScreen *ss, int W, int H, int top) {
    UiItem it[SI_COUNT];
    int n = build(it);
    ui_list_draw(W, H, top, "SETTINGS", it, n, &ss->list, "UP/DOWN CHOOSE  LEFT/RIGHT CHANGE  ESC/B BACK");
    ui_dialog_draw(W, H, &ss->dlg);
    (void)g;
}

/* Returns true when the screen is left. */
bool settings_screen_nav(Game *g, SettingsScreen *ss, int nav) {
    UiItem it[SI_COUNT];
    int n = build(it);
    if (ss->dlg.open) {
        if (ui_dialog_nav(&ss->dlg, nav) == 1) {
            settings_defaults(&g_set);
            changed(g);
            game_banner_all(g, "SETTINGS RESET TO DEFAULTS");
        }
        return false;
    }
    if (nav == NAV_BACK) return true;
    if (nav == NAV_UP || nav == NAV_DOWN) { ui_list_nav(&ss->list, it, n, nav); return false; }
    int id = it[ss->list.sel].id;
    if (nav == NAV_LEFT || nav == NAV_RIGHT) {
        if (it[ss->list.sel].value[0] && !it[ss->list.sel].disabled) { step(id, nav == NAV_LEFT ? -1 : 1); changed(g); }
        return false;
    }
    if (nav == NAV_BUY) {
        if (id == SI_BACK) return true;
        if (id == SI_RESET) { ui_dialog_open(&ss->dlg, 1, "RESET SETTINGS?", "EVERY SETTING GOES BACK TO HOW THE GAME CAME.", "RESET", "CANCEL"); return false; }
        if (it[ss->list.sel].value[0] && !it[ss->list.sel].disabled) { step(id, 1); changed(g); }
    }
    return false;
}

bool settings_screen_tap(Game *g, SettingsScreen *ss, int W, int H, int top, float x, float y) {
    if (ss->dlg.open) {
        if (ui_dialog_tap(&ss->dlg, W, H, x, y) == 1) { settings_defaults(&g_set); changed(g); }
        return false;
    }
    UiItem it[SI_COUNT];
    int n = build(it);
    int dir = 0;
    int k = ui_list_tap(W, H, top, it, n, &ss->list, x, y, &dir);
    if (k < 0) return false;
    if (dir) return settings_screen_nav(g, ss, dir < 0 ? NAV_LEFT : NAV_RIGHT);
    return settings_screen_nav(g, ss, NAV_BUY);
}
