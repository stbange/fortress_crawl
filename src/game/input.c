#include "game/game.h"

/* The actions of play and what triggers them (openspec controls: remapping). Each action has up
 * to two keyboard or mouse inputs and one gamepad input; the defaults are the game's own layout.
 * Keys are kept as scancodes (where a key sits, so WASD stays WASD on any layout) and named as
 * the keyboard's layout labels them. Menus keep fixed keys (arrows, Enter, Esc, the D-pad, A, B),
 * so a bad binding can always be undone. The help, the tutorial and hints write {JUMP} and the
 * like, filled in with the input of the device the player is using (input_expand). */

const ActionInfo ACTIONS[A_COUNT] = {
    [A_FORWARD] = { "FORWARD", "MOVE FORWARD" },       [A_BACK] = { "BACKWARD", "MOVE BACK" },
    [A_LEFT] = { "LEFT", "MOVE LEFT" },                [A_RIGHT] = { "RIGHT", "MOVE RIGHT" },
    [A_JUMP] = { "JUMP", "JUMP" },                     [A_CROUCH] = { "CROUCH", "CROUCH" },
    [A_SPRINT] = { "SPRINT", "SPRINT" },               [A_ATTACK] = { "ATTACK", "STRIKE / SHOOT" },
    [A_ALT] = { "ALT", "ALTERNATE ATTACK" },           [A_CAST] = { "CAST", "CAST THE SPELL" },
    [A_NEXT_SPELL] = { "NEXT_SPELL", "NEXT SPELL" },   [A_PREV_SPELL] = { "PREV_SPELL", "PREVIOUS SPELL" },
    [A_WEAPON1] = { "BLADE", "BLADE" },                [A_WEAPON2] = { "BOW", "BOW" },
    [A_WEAPON3] = { "STAFF", "STAFF" },                [A_PICKAXE] = { "PICKAXE", "PICKAXE" },
    [A_SWAP] = { "SWAP", "WEAPON BEFORE" },            [A_NEXT_WEAPON] = { "NEXT_WEAPON", "NEXT WEAPON" },
    [A_PREV_WEAPON] = { "PREV_WEAPON", "PREVIOUS WEAPON" }, [A_THROW] = { "THROW", "THROW" },
    [A_THROW_CHOICE] = { "THROW_CHOICE", "WHAT TO THROW" }, [A_CAMPFIRE] = { "CAMPFIRE", "CAMPFIRE" },
    [A_POTION] = { "POTION", "HEALTH POTION" },        [A_MANA_POTION] = { "MANA_POTION", "MANA POTION" },
    [A_EAT] = { "EAT", "EAT" },                        [A_ELIXIR] = { "ELIXIR", "DRINK THE ELIXIR" },
    [A_THREAD] = { "THREAD", "GOLDEN THREAD" },        [A_NEXT_LINE] = { "NEXT_LINE", "ANOTHER LINE AT THE TOP" },
    [A_MAP] = { "MAP", "MAP" },                        [A_MENU] = { "MENU", "MENU" },
    [A_ITEMS] = { "ITEMS", "ITEMS TAB" },              [A_SPELLBOOK] = { "SPELLBOOK", "SPELLBOOK TAB" },
    [A_STORY] = { "STORY", "STORY TAB" },              [A_FOOD] = { "FOOD", "FOOD TAB" },
    [A_ALCHEMY] = { "ALCHEMY", "ALCHEMY TAB" },        [A_FORGE] = { "FORGE", "FORGE TAB" },
    [A_HELP] = { "HELP", "HELP" },                     [A_SKIP_TUTORIAL] = { "SKIP", "SKIP THE TUTORIAL" },
};

#define SC(x) SDL_SCANCODE_##x
#define PB(x) SDL_GAMEPAD_BUTTON_##x
void input_defaults(Settings *s) {
    static const int KB[A_COUNT][2] = {
        [A_FORWARD] = { SC(W), 0 }, [A_BACK] = { SC(S), 0 }, [A_LEFT] = { SC(A), 0 }, [A_RIGHT] = { SC(D), 0 },
        [A_JUMP] = { SC(SPACE), 0 }, [A_CROUCH] = { SC(LCTRL), SC(C) }, [A_SPRINT] = { SC(LSHIFT), 0 },
        [A_ATTACK] = { IN_MOUSE + SDL_BUTTON_LEFT, 0 }, [A_ALT] = { IN_MOUSE + SDL_BUTTON_RIGHT, 0 },
        [A_CAST] = { SC(Q), IN_MOUSE + SDL_BUTTON_MIDDLE }, [A_NEXT_SPELL] = { SC(R), 0 }, [A_PREV_SPELL] = { 0, 0 },
        [A_WEAPON1] = { SC(1), 0 }, [A_WEAPON2] = { SC(2), 0 }, [A_WEAPON3] = { SC(3), 0 }, [A_PICKAXE] = { SC(4), 0 },
        [A_SWAP] = { SC(Z), 0 }, [A_NEXT_WEAPON] = { IN_WHEEL_DOWN, 0 }, [A_PREV_WEAPON] = { IN_WHEEL_UP, 0 },
        [A_THROW] = { SC(X), 0 }, [A_THROW_CHOICE] = { SC(C), 0 }, [A_CAMPFIRE] = { SC(P), 0 },
        [A_POTION] = { SC(H), 0 }, [A_MANA_POTION] = { SC(J), 0 }, [A_EAT] = { SC(K), 0 }, [A_ELIXIR] = { SC(V), 0 },
        [A_THREAD] = { SC(G), 0 }, [A_NEXT_LINE] = { SC(Y), 0 }, [A_MAP] = { SC(M), 0 }, [A_MENU] = { SC(TAB), 0 },
        [A_ITEMS] = { SC(I), 0 }, [A_SPELLBOOK] = { SC(B), 0 }, [A_STORY] = { SC(L), 0 }, [A_FOOD] = { SC(N), 0 },
        [A_ALCHEMY] = { SC(O), 0 }, [A_FORGE] = { SC(U), 0 }, [A_HELP] = { SC(F1), SC(SLASH) }, [A_SKIP_TUTORIAL] = { SC(BACKSPACE), 0 },
    };
    for (int a = 0; a < A_COUNT; a++) { s->kb[a][0] = KB[a][0]; s->kb[a][1] = KB[a][1]; s->pad[a] = -1; }
    s->pad[A_JUMP] = PB(SOUTH);
    s->pad[A_CROUCH] = PB(EAST);
    s->pad[A_SPRINT] = PB(LEFT_STICK);
    s->pad[A_ATTACK] = IN_AXIS + SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    s->pad[A_ALT] = IN_AXIS + SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
    s->pad[A_CAST] = PB(WEST);
    s->pad[A_NEXT_SPELL] = PB(DPAD_RIGHT);
    s->pad[A_PREV_SPELL] = PB(DPAD_LEFT);
    s->pad[A_NEXT_WEAPON] = PB(RIGHT_SHOULDER);
    s->pad[A_PREV_WEAPON] = PB(LEFT_SHOULDER);
    s->pad[A_POTION] = PB(DPAD_UP);
    s->pad[A_MANA_POTION] = PB(DPAD_DOWN);
    s->pad[A_THREAD] = PB(RIGHT_STICK);
    s->pad[A_MAP] = PB(BACK);
    s->pad[A_MENU] = PB(NORTH);
    s->pad[A_SKIP_TUTORIAL] = PB(BACK); /* (held a second) */
    s->kb_sprint_toggle = s->kb_crouch_toggle = false;
    s->pad_sprint_toggle = true; /* (the stick click: sprint until you stop, as before) */
    s->pad_crouch_toggle = false;
}
#undef SC
#undef PB

/* ---------- names ---------- */

static void upper(char *s) {
    for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32;
}

void input_code_name(int code, bool pad, char *out, int n) {
    if (pad) {
        static const char *B[SDL_GAMEPAD_BUTTON_COUNT] = {
            [SDL_GAMEPAD_BUTTON_SOUTH] = "A", [SDL_GAMEPAD_BUTTON_EAST] = "B", [SDL_GAMEPAD_BUTTON_WEST] = "X", [SDL_GAMEPAD_BUTTON_NORTH] = "Y",
            [SDL_GAMEPAD_BUTTON_BACK] = "BACK", [SDL_GAMEPAD_BUTTON_GUIDE] = "GUIDE", [SDL_GAMEPAD_BUTTON_START] = "START",
            [SDL_GAMEPAD_BUTTON_LEFT_STICK] = "L-STICK", [SDL_GAMEPAD_BUTTON_RIGHT_STICK] = "R-STICK",
            [SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = "LB", [SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = "RB",
            [SDL_GAMEPAD_BUTTON_DPAD_UP] = "D-PAD UP", [SDL_GAMEPAD_BUTTON_DPAD_DOWN] = "D-PAD DOWN",
            [SDL_GAMEPAD_BUTTON_DPAD_LEFT] = "D-PAD LEFT", [SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = "D-PAD RIGHT",
        };
        if (code < 0) snprintf(out, (size_t)n, "-");
        else if (code >= IN_AXIS) snprintf(out, (size_t)n, "%s", code - IN_AXIS == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER ? "RT" : code - IN_AXIS == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? "LT" : "AXIS");
        else if (code < SDL_GAMEPAD_BUTTON_COUNT && B[code]) snprintf(out, (size_t)n, "%s", B[code]);
        else snprintf(out, (size_t)n, "BUTTON %d", code);
        return;
    }
    if (code <= 0) { snprintf(out, (size_t)n, "-"); return; }
    if (code == IN_WHEEL_UP) { snprintf(out, (size_t)n, "WHEEL UP"); return; }
    if (code == IN_WHEEL_DOWN) { snprintf(out, (size_t)n, "WHEEL DOWN"); return; }
    if (code >= IN_MOUSE) {
        int b = code - IN_MOUSE;
        snprintf(out, (size_t)n, "%s", b == SDL_BUTTON_LEFT ? "LEFT MOUSE" : b == SDL_BUTTON_RIGHT ? "RIGHT MOUSE" : b == SDL_BUTTON_MIDDLE ? "MIDDLE MOUSE" : "MOUSE");
        if (b > 3) snprintf(out, (size_t)n, "MOUSE %d", b);
        return;
    }
    SDL_Keycode k = SDL_GetKeyFromScancode((SDL_Scancode)code, SDL_KMOD_NONE, false);
    const char *name = SDL_GetKeyName(k);
    if (!name || !name[0]) name = SDL_GetScancodeName((SDL_Scancode)code);
    snprintf(out, (size_t)n, "%s", name && name[0] ? name : "?");
    upper(out);
    if (!strcmp(out, "LEFT CTRL")) snprintf(out, (size_t)n, "CTRL");
    else if (!strcmp(out, "LEFT SHIFT")) snprintf(out, (size_t)n, "SHIFT");
    else if (!strcmp(out, "LEFT ALT")) snprintf(out, (size_t)n, "ALT");
}

/* The input of an action on a device (DEV_*): "Q", "LEFT MOUSE", "RT"; both keys as "CTRL OR C". */
void input_name_dev(int dev, int a, char *out, int n) {
    char b1[32], b2[32];
    if (dev == DEV_PAD) { input_code_name(g_set.pad[a], true, out, n); return; }
    int k0 = g_set.kb[a][0], k1 = g_set.kb[a][1];
    if (!k0 && !k1) { snprintf(out, (size_t)n, "(NO KEY)"); return; }
    input_code_name(k0 ? k0 : k1, false, b1, sizeof b1);
    if (k0 && k1) {
        input_code_name(k1, false, b2, sizeof b2);
        snprintf(out, (size_t)n, "%s OR %s", b1, b2);
    } else snprintf(out, (size_t)n, "%s", b1);
}

int input_device(const Game *g, const PlayerSlot *ps) {
    if (ps->keyboard && g->touch.active) return DEV_TOUCH;
    if (ps->pad && (!ps->keyboard || ps->last_dev == DEV_PAD)) return DEV_PAD;
    return DEV_KEYS;
}

void input_name(const Game *g, const PlayerSlot *ps, int a, char *out, int n) { input_name_dev(input_device(g, ps), a, out, n); }

/* Text with {TOKEN}s (an action's token, or MOVE for the four moves) filled in for a device. */
void input_expand_dev(int dev, const char *src, char *out, int n) {
    int o = 0;
    for (const char *p = src; *p && o < n - 1;) {
        if (*p == '{') {
            const char *e = strchr(p, '}');
            if (e && e - p < 24) {
                char tok[24], name[64];
                snprintf(tok, sizeof tok, "%.*s", (int)(e - p - 1), p + 1);
                name[0] = 0;
                if (!strcmp(tok, "MOVE")) {
                    if (dev == DEV_PAD) snprintf(name, sizeof name, "THE LEFT STICK");
                    else {
                        char f[32], l[32], b[32], r[32];
                        input_code_name(g_set.kb[A_FORWARD][0], false, f, sizeof f);
                        input_code_name(g_set.kb[A_LEFT][0], false, l, sizeof l);
                        input_code_name(g_set.kb[A_BACK][0], false, b, sizeof b);
                        input_code_name(g_set.kb[A_RIGHT][0], false, r, sizeof r);
                        if (strlen(f) == 1 && strlen(l) == 1 && strlen(b) == 1 && strlen(r) == 1) snprintf(name, sizeof name, "%s%s%s%s", f, l, b, r);
                        else snprintf(name, sizeof name, "%s/%s/%s/%s", f, l, b, r);
                    }
                } else
                    for (int a = 0; a < A_COUNT; a++)
                        if (!strcmp(tok, ACTIONS[a].token)) { input_name_dev(dev, a, name, sizeof name); break; }
                if (name[0]) {
                    int k = snprintf(out + o, (size_t)(n - o), "%s", name);
                    o += k < n - o ? k : n - o - 1;
                    p = e + 1;
                    continue;
                }
            }
        }
        out[o++] = *p++;
    }
    out[o] = 0;
}

void input_expand(const Game *g, const PlayerSlot *ps, const char *src, char *out, int n) { input_expand_dev(input_device(g, ps), src, out, n); }

/* ---------- reading ---------- */

/* The actions bound to a keyboard or mouse input (into out; returns how many). */
int input_kb_actions(int code, int *out, int max) {
    int n = 0;
    if (!code) return 0;
    for (int a = 0; a < A_COUNT && n < max; a++)
        if (g_set.kb[a][0] == code || g_set.kb[a][1] == code) out[n++] = a;
    return n;
}

static bool kb_code_held(int code) {
    if (code <= 0 || code == IN_WHEEL_UP || code == IN_WHEEL_DOWN) return false;
    if (code >= IN_MOUSE) return (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_MASK(code - IN_MOUSE)) != 0;
    const bool *k = SDL_GetKeyboardState(NULL);
    return code < SDL_SCANCODE_COUNT && k[code];
}

bool input_kb_held(int a) { return kb_code_held(g_set.kb[a][0]) || kb_code_held(g_set.kb[a][1]); }

bool input_pad_held(SDL_Gamepad *pad, int a) {
    int c = g_set.pad[a];
    if (!pad || c < 0) return false;
    if (c >= IN_AXIS) return SDL_GetGamepadAxis(pad, (SDL_GamepadAxis)(c - IN_AXIS)) / 32767.0f > 0.4f;
    return SDL_GetGamepadButton(pad, (SDL_GamepadButton)c);
}
