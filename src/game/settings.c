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
    s->text_size = 100;
    s->margin = 0;
    s->shake = 2;
    s->hitstop = true;
    s->calm_flash = false;
    s->palette = PAL_NORMAL;
    s->dmg_taken = 100;
    s->windup = 100;
    s->keep_gold = false;
    input_defaults(s);
}

bool access_assisted(void) { return g_set.dmg_taken < 100 || g_set.windup > 100 || g_set.keep_gold; }
float access_windup(void) { return g_set.windup / 100.0f; }

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
    s->shake = clampi(s->shake, 0, 2);
    s->text_size = s->text_size >= 200 ? 200 : s->text_size >= 150 ? 150 : 100;
    s->margin = clampi(s->margin, 0, 10) / 2 * 2;
    s->palette = clampi(s->palette, 0, PAL_COUNT - 1);
    s->dmg_taken = s->dmg_taken <= 50 ? 50 : s->dmg_taken <= 75 ? 75 : 100;
    s->windup = s->windup >= 150 ? 150 : 100;
    for (int a = 0; a < A_COUNT; a++) { /* (codes that cannot be: none) */
        for (int k = 0; k < 2; k++)
            if (s->kb[a][k] < 0 || (s->kb[a][k] >= SDL_SCANCODE_COUNT && s->kb[a][k] < IN_MOUSE) || s->kb[a][k] > IN_WHEEL_DOWN) s->kb[a][k] = 0;
        if (s->pad[a] < -1 || (s->pad[a] >= SDL_GAMEPAD_BUTTON_COUNT && s->pad[a] < IN_AXIS) || s->pad[a] >= IN_AXIS + SDL_GAMEPAD_AXIS_COUNT) s->pad[a] = -1;
    }
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
    F("text_size", 'i', text_size), F("screen_margin", 'i', margin),
    F("screen_shake", 'i', shake), F("hitstop", 'b', hitstop), F("reduced_flashing", 'b', calm_flash), F("palette", 'i', palette),
    F("damage_taken", 'i', dmg_taken), F("windups", 'i', windup), F("keep_gold", 'b', keep_gold),
    F("keys_sprint_toggle", 'b', kb_sprint_toggle), F("keys_crouch_toggle", 'b', kb_crouch_toggle),
    F("pad_sprint_toggle", 'b', pad_sprint_toggle), F("pad_crouch_toggle", 'b', pad_crouch_toggle),
};
#undef F
#define NFIELDS ((int)(sizeof FIELDS / sizeof FIELDS[0]))

void settings_parse(Settings *s, const char *text) {
    settings_defaults(s);
    settings_merge(s, text);
}

/* Named values over what is there (the file, or --set on the command line). */
void settings_merge(Settings *s, const char *text) {
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
            /* the inputs: key_JUMP=44,6 and pad_JUMP=0 (an action this build does not know is skipped) */
            bool key = !strncmp(line, "key_", 4), padk = !strncmp(line, "pad_", 4);
            for (int a = 0; (key || padk) && a < A_COUNT; a++) {
                if (strcmp(line + 4, ACTIONS[a].token)) continue;
                if (key) {
                    char *c = strchr(eq + 1, ',');
                    s->kb[a][0] = atoi(eq + 1);
                    s->kb[a][1] = c ? atoi(c + 1) : 0;
                } else s->pad[a] = atoi(eq + 1);
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
    for (int a = 0; a < A_COUNT && n < cap; a++) n += snprintf(out + n, (size_t)(cap - n), "key_%s=%d,%d\n", ACTIONS[a].token, s->kb[a][0], s->kb[a][1]);
    for (int a = 0; a < A_COUNT && n < cap; a++) n += snprintf(out + n, (size_t)(cap - n), "pad_%s=%d\n", ACTIONS[a].token, s->pad[a]);
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

void settings_no_save(void) { g_enabled = false; }

void settings_save(void) {
    if (!g_enabled) return;
    char buf[6144];
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
        g->base_h = RES[g_set.res_mode][1]; /* (main.c fits the width to the window) */
    }
    g->r.set.dither = g_set.dither;
    g->r.set.vertex_snap = g_set.vsnap;
    g->r.set.torch = g_set.torch;
    g->r.set.bright = g_set.bright;
    if (!g->opt.cli_sens) g->opt.sens = g_set.sens_mouse;
    if (!g->opt.cli_invert) { g->opt.invert_x = g_set.invert_x; g->opt.invert_y = g_set.invert_y; }
    sfx_set_levels(g_set.vol_master / 100.0f, g_set.vol_sfx / 100.0f, g_set.vol_music / 100.0f, g_set.music_on);
    sfx_set_mute(g_set.mute || g->opt.mute);
    g_player_dip = g_set.shake * 0.5f;
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
    SI_H_CONTROLS, SI_KEYS, SI_BUTTONS, SI_KB_SPRINT, SI_KB_CROUCH, SI_PAD_SPRINT, SI_PAD_CROUCH, SI_SENS_MOUSE, SI_SENS_PAD, SI_INVERT_X, SI_INVERT_Y, SI_VIBRATION,
    SI_H_INTERFACE, SI_TEXT, SI_MARGIN, SI_LINE,
    SI_H_ACCESS, SI_SHAKE, SI_HITSTOP, SI_FLASH, SI_PALETTE, SI_DMG, SI_WINDUP, SI_KEEPGOLD,
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
    snprintf(it[SI_KEYS].label, sizeof it[0].label, "KEYS (KEYBOARD AND MOUSE)...");
    snprintf(it[SI_BUTTONS].label, sizeof it[0].label, "BUTTONS (GAMEPAD)...");
    ROW(SI_KB_SPRINT, "SPRINT (KEYS)", "%s", g_set.kb_sprint_toggle ? "TOGGLE" : "HOLD");
    ROW(SI_KB_CROUCH, "CROUCH (KEYS)", "%s", g_set.kb_crouch_toggle ? "TOGGLE" : "HOLD");
    ROW(SI_PAD_SPRINT, "SPRINT (GAMEPAD)", "%s", g_set.pad_sprint_toggle ? "TOGGLE" : "HOLD");
    ROW(SI_PAD_CROUCH, "CROUCH (GAMEPAD)", "%s", g_set.pad_crouch_toggle ? "TOGGLE" : "HOLD");
    HDR(SI_H_INTERFACE, "INTERFACE");
    ROW(SI_TEXT, "TEXT SIZE", "%d%%", g_set.text_size);
    ROW(SI_MARGIN, "SCREEN MARGIN", "%d%%", g_set.margin);
    ROW(SI_LINE, "LINE AT THE TOP", "%s", onoff(g_set.line_top));
    static const char *SHAKE[3] = { "OFF", "HALF", "FULL" };
    static const char *PAL[PAL_COUNT] = { "NORMAL", "PROTANOPIA", "DEUTERANOPIA", "TRITANOPIA" };
    HDR(SI_H_ACCESS, "ACCESSIBILITY");
    ROW(SI_SHAKE, "SCREEN SHAKE", "%s", SHAKE[g_set.shake]);
    ROW(SI_HITSTOP, "HIT PAUSE", "%s", onoff(g_set.hitstop));
    ROW(SI_FLASH, "REDUCED FLASHING", "%s", onoff(g_set.calm_flash));
    ROW(SI_PALETTE, "COLOURS", "%s", PAL[g_set.palette]);
    ROW(SI_DMG, "DAMAGE TAKEN (ASSIST)", "%d%%", g_set.dmg_taken);
    ROW(SI_WINDUP, "ENEMY WIND-UPS (ASSIST)", "%s", g_set.windup > 100 ? "SLOWER" : "NORMAL");
    ROW(SI_KEEPGOLD, "KEEP GOLD ON DEATH (ASSIST)", "%s", onoff(g_set.keep_gold));
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
    case SI_KB_SPRINT: g_set.kb_sprint_toggle = !g_set.kb_sprint_toggle; break;
    case SI_KB_CROUCH: g_set.kb_crouch_toggle = !g_set.kb_crouch_toggle; break;
    case SI_PAD_SPRINT: g_set.pad_sprint_toggle = !g_set.pad_sprint_toggle; break;
    case SI_PAD_CROUCH: g_set.pad_crouch_toggle = !g_set.pad_crouch_toggle; break;
    case SI_LINE: g_set.line_top = !g_set.line_top; break;
    case SI_TEXT: g_set.text_size = g_set.text_size == 100 ? (d > 0 ? 150 : 200) : g_set.text_size == 150 ? (d > 0 ? 200 : 100) : (d > 0 ? 100 : 150); break;
    case SI_MARGIN: g_set.margin = clampi(g_set.margin + 2 * d, 0, 10); break;
    case SI_SHAKE: g_set.shake = (g_set.shake + d + 3) % 3; break;
    case SI_HITSTOP: g_set.hitstop = !g_set.hitstop; break;
    case SI_FLASH: g_set.calm_flash = !g_set.calm_flash; break;
    case SI_PALETTE: g_set.palette = (g_set.palette + d + PAL_COUNT) % PAL_COUNT; break;
    case SI_DMG: {
        static const int V[3] = { 100, 75, 50 };
        int k = g_set.dmg_taken == 100 ? 0 : g_set.dmg_taken == 75 ? 1 : 2;
        g_set.dmg_taken = V[(k + d + 3) % 3];
        break;
    }
    case SI_WINDUP: g_set.windup = g_set.windup > 100 ? 100 : 150; break;
    case SI_KEEPGOLD: g_set.keep_gold = !g_set.keep_gold; break;
    default: return;
    }
    sfx_ui(SFX_MENU_MOVE, 0.6f);
}

static void changed(Game *g) {
    sanitize(&g_set);
    settings_apply(g);
    settings_save();
}

/* ---------- the inputs (openspec controls: remapping) ---------- */

enum { RI_RESET = 1000, RI_BACK = 1001 };
enum { DLG_RESET = 1, DLG_SWAP = 2 };
static SettingsScreen *g_cap;     /* the screen waiting for an input */
static u64 g_cap_quiet;           /* until then the input that was taken does not move the menus */
static int g_cap_slot;            /* the keyboard's first or second input */

bool settings_capturing(void) { return (g_cap && g_cap->capture > 0) || SDL_GetTicksNS() < g_cap_quiet; }

static int remap_items(SettingsScreen *ss, UiItem *it) {
    bool pad = ss->page == 2;
    int n = 0;
    for (int a = 0; a < A_COUNT; a++) {
        if (pad && a <= A_RIGHT) continue; /* (the left stick moves) */
        memset(&it[n], 0, sizeof it[n]);
        snprintf(it[n].label, sizeof it[n].label, "%s", ACTIONS[a].label);
        if (pad) input_code_name(g_set.pad[a], true, it[n].value, sizeof it[n].value);
        else {
            char k0[32], k1[32];
            input_code_name(g_set.kb[a][0], false, k0, sizeof k0);
            input_code_name(g_set.kb[a][1], false, k1, sizeof k1);
            snprintf(it[n].value, sizeof it[n].value, g_set.kb[a][1] ? "%.16s / %.16s" : "%.30s", k0, k1);
        }
        it[n].id = a;
        n++;
    }
    memset(&it[n], 0, sizeof it[n]); it[n].header = true; n++;
    memset(&it[n], 0, sizeof it[n]);
    snprintf(it[n].label, sizeof it[n].label, pad ? "RESET THE BUTTONS" : "RESET THE KEYS");
    it[n++].id = RI_RESET;
    memset(&it[n], 0, sizeof it[n]);
    snprintf(it[n].label, sizeof it[n].label, "BACK");
    it[n++].id = RI_BACK;
    return n;
}

/* Who else has this input on the same device (or -1). */
static int holder(bool pad, int code, int except) {
    for (int a = 0; a < A_COUNT; a++) {
        if (a == except) continue;
        if (pad ? g_set.pad[a] == code : (g_set.kb[a][0] == code || g_set.kb[a][1] == code)) return a;
    }
    return -1;
}

static void bind(bool pad, int a, int slot, int code) {
    if (pad) g_set.pad[a] = code;
    else g_set.kb[a][slot] = code;
}

/* The input that came for the action waiting: taken, or asked about if another action has it. */
static void take_input(SettingsScreen *ss, int code) {
    bool pad = ss->page == 2;
    int a = ss->capture - 1;
    ss->capture = 0;
    g_cap_quiet = SDL_GetTicksNS() + 300000000ull;
    int b = holder(pad, code, a);
    if (b >= 0) {
        char t[160], name[32];
        input_code_name(code, pad, name, sizeof name);
        snprintf(t, sizeof t, "%s IS ALREADY %s. SWAP THEM: %s TAKES WHAT %s HAD?", name, ACTIONS[b].label, ACTIONS[b].label, ACTIONS[a].label);
        ss->pending_code = code;
        ss->dlg_action = a;
        ui_dialog_open(&ss->dlg, DLG_SWAP, "SWAP?", t, "SWAP", "CANCEL");
        return;
    }
    bind(pad, a, g_cap_slot, code);
    settings_save();
    sfx_ui(SFX_MENU_MOVE, 0.6f);
}

static void swap_inputs(SettingsScreen *ss) {
    bool pad = ss->page == 2;
    int a = ss->dlg_action, code = ss->pending_code, b = holder(pad, code, a);
    if (b < 0) return;
    int old = pad ? g_set.pad[a] : g_set.kb[a][g_cap_slot];
    if (pad) g_set.pad[b] = old;
    else for (int k = 0; k < 2; k++) if (g_set.kb[b][k] == code) g_set.kb[b][k] = old;
    bind(pad, a, g_cap_slot, code);
    settings_save();
}

bool settings_capture_event(const SDL_Event *e) {
    if (!g_cap || g_cap->capture <= 0) return false;
    SettingsScreen *ss = g_cap;
    bool pad = ss->page == 2;
    if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) {
        if (e->key.key == SDLK_ESCAPE) { ss->capture = 0; g_cap_quiet = SDL_GetTicksNS() + 300000000ull; return true; }
        if (!pad) take_input(ss, (int)e->key.scancode);
        return true;
    }
    if (e->type == SDL_EVENT_KEY_UP) return true;
    if (!pad && e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.which != SDL_TOUCH_MOUSEID) { take_input(ss, IN_MOUSE + e->button.button); return true; }
    if (!pad && e->type == SDL_EVENT_MOUSE_WHEEL && e->wheel.y != 0) { take_input(ss, e->wheel.y > 0 ? IN_WHEEL_UP : IN_WHEEL_DOWN); return true; }
    if (pad && e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        if (e->gbutton.button == SDL_GAMEPAD_BUTTON_START) { ss->capture = 0; g_cap_quiet = SDL_GetTicksNS() + 300000000ull; }
        else take_input(ss, e->gbutton.button);
        return true;
    }
    if (pad && e->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && e->gaxis.value > 24000 &&
        (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || e->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
        take_input(ss, IN_AXIS + e->gaxis.axis);
        return true;
    }
    return e->type == SDL_EVENT_MOUSE_BUTTON_DOWN || e->type == SDL_EVENT_MOUSE_BUTTON_UP || e->type == SDL_EVENT_GAMEPAD_BUTTON_UP;
}

static void remap_draw(SettingsScreen *ss, int W, int H, int top) {
    UiItem it[A_COUNT + 3];
    int n = remap_items(ss, it);
    bool pad = ss->page == 2;
    ui_list_draw(W, H, top, pad ? "BUTTONS (GAMEPAD)" : "KEYS (KEYBOARD AND MOUSE)", it, n, &ss->rlist,
                 pad ? "A NEW BUTTON  X NONE  B BACK" : "ENTER NEW KEY  RIGHT SECOND KEY  DEL NONE  ESC BACK");
    if (ss->capture > 0) {
        UiDialog d = { 0 };
        d.open = true;
        d.sel = -1;
        snprintf(d.title, sizeof d.title, "%s", ACTIONS[ss->capture - 1].label);
        snprintf(d.text, sizeof d.text, pad ? "PRESS A BUTTON OR A TRIGGER FOR IT. START CANCELS." : "PRESS A KEY OR A MOUSE BUTTON FOR IT%s. ESC CANCELS.",
                 g_cap_slot ? " (ITS SECOND)" : "");
        ui_dialog_draw(W, H, &d);
    }
}

/* the inputs' page: true when it is left */
static bool remap_nav(SettingsScreen *ss, int nav) {
    UiItem it[A_COUNT + 3];
    int n = remap_items(ss, it);
    bool pad = ss->page == 2;
    if (nav == NAV_BACK) return true;
    if (nav == NAV_UP || nav == NAV_DOWN) { ui_list_nav(&ss->rlist, it, n, nav); return false; }
    ui_list_fix(&ss->rlist, it, n, 0);
    int id = it[ss->rlist.sel].id;
    if (id == RI_BACK) return nav == NAV_BUY;
    if (id == RI_RESET) {
        if (nav == NAV_BUY) ui_dialog_open(&ss->dlg, DLG_RESET, pad ? "RESET THE BUTTONS?" : "RESET THE KEYS?", "EVERY ACTION GOES BACK TO THE GAME'S OWN LAYOUT.", "RESET", "CANCEL");
        return false;
    }
    if (nav == NAV_BUY || (nav == NAV_RIGHT && !pad)) {
        ss->capture = id + 1;
        g_cap_slot = nav == NAV_RIGHT ? 1 : 0;
        g_cap = ss;
    } else if (nav == NAV_DROP) {
        if (pad) g_set.pad[id] = -1;
        else g_set.kb[id][0] = g_set.kb[id][1] = 0;
        settings_save();
    }
    return false;
}

#define DLG_ALL 10 /* (the whole settings back to the defaults) */

static void dialog_yes(Game *g, SettingsScreen *ss) {
    if (ss->dlg.action == DLG_ALL) {
        settings_defaults(&g_set);
        changed(g);
        game_banner_all(g, "SETTINGS RESET TO DEFAULTS");
    } else if (ss->dlg.action == DLG_RESET) {
        Settings d;
        input_defaults(&d);
        if (ss->page == 2) memcpy(g_set.pad, d.pad, sizeof g_set.pad);
        else memcpy(g_set.kb, d.kb, sizeof g_set.kb);
        settings_save();
    } else if (ss->dlg.action == DLG_SWAP) swap_inputs(ss);
}

void settings_screen_draw(Game *g, SettingsScreen *ss, int W, int H, int top) {
    if (ss->page) remap_draw(ss, W, H, top);
    else {
        UiItem it[SI_COUNT];
        int n = build(it);
        ui_list_draw(W, H, top, "SETTINGS", it, n, &ss->list, "UP/DOWN CHOOSE  LEFT/RIGHT CHANGE  ESC/B BACK");
    }
    ui_dialog_draw(W, H, &ss->dlg);
    (void)g;
}

/* Returns true when the screen is left. */
bool settings_screen_nav(Game *g, SettingsScreen *ss, int nav) {
    if (ss->dlg.open) {
        if (ui_dialog_nav(&ss->dlg, nav) == 1) dialog_yes(g, ss);
        return false;
    }
    if (ss->capture > 0 || SDL_GetTicksNS() < g_cap_quiet) return false; /* (waiting for an input, or the one just taken) */
    if (ss->page) {
        if (remap_nav(ss, nav)) ss->page = 0;
        return false;
    }
    UiItem it[SI_COUNT];
    int n = build(it);
    if (nav == NAV_BACK) return true;
    if (nav == NAV_UP || nav == NAV_DOWN) { ui_list_nav(&ss->list, it, n, nav); return false; }
    ui_list_fix(&ss->list, it, n, 0);
    int id = it[ss->list.sel].id;
    if (nav == NAV_LEFT || nav == NAV_RIGHT) {
        if (it[ss->list.sel].value[0] && !it[ss->list.sel].disabled) { step(id, nav == NAV_LEFT ? -1 : 1); changed(g); }
        return false;
    }
    if (nav == NAV_BUY) {
        if (id == SI_BACK) return true;
        if (id == SI_KEYS || id == SI_BUTTONS) { ss->page = id == SI_KEYS ? 1 : 2; ss->rlist.sel = ss->rlist.scroll = 0; ss->capture = 0; return false; }
        if (id == SI_RESET) { ui_dialog_open(&ss->dlg, DLG_ALL, "RESET SETTINGS?", "EVERY SETTING GOES BACK TO HOW THE GAME CAME.", "RESET", "CANCEL"); return false; }
        if (it[ss->list.sel].value[0] && !it[ss->list.sel].disabled) { step(id, 1); changed(g); }
    }
    return false;
}

bool settings_screen_tap(Game *g, SettingsScreen *ss, int W, int H, int top, float x, float y) {
    if (ss->dlg.open) {
        if (ui_dialog_tap(&ss->dlg, W, H, x, y) == 1) dialog_yes(g, ss);
        return false;
    }
    if (ss->capture > 0) return false;
    int dir = 0;
    if (ss->page) {
        UiItem it[A_COUNT + 3];
        int n = remap_items(ss, it);
        if (ui_list_tap(W, H, top, it, n, &ss->rlist, x, y, &dir) >= 0) {
            if (dir > 0 && ss->page == 1) remap_nav(ss, NAV_RIGHT);
            else if (remap_nav(ss, NAV_BUY)) ss->page = 0;
        }
        return false;
    }
    UiItem it[SI_COUNT];
    int n = build(it);
    int k = ui_list_tap(W, H, top, it, n, &ss->list, x, y, &dir);
    if (k < 0) return false;
    if (dir) return settings_screen_nav(g, ss, dir < 0 ? NAV_LEFT : NAV_RIGHT);
    return settings_screen_nav(g, ss, NAV_BUY);
}
