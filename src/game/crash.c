#include "game/game.h"

#ifndef __EMSCRIPTEN__
#include <signal.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define WRITE_FD _write
#define CLOSE_FD _close
#else
#include <unistd.h>
#define WRITE_FD write
#define CLOSE_FD close
#endif
#endif

/* When something goes wrong (openspec errors): the last 50 lines of the log are kept, a marker
 * says a world is being played (removed on a clean quit, so one left behind means the game did
 * not end well), and on the desktop a crash writes crash.txt next to the saves with the build,
 * the world, the time played and those lines, using only what is safe inside a signal handler. */

#define RING 50
#define RING_W 200
static char g_ring[RING][RING_W];
static int g_ring_at;
static SDL_LogOutputFunction g_default_out;
static void *g_default_user;
static const Game *g_game;
static char g_crash_path[600];

#define SESSION_KEY "fortress_crawl:session"

static void log_out(void *user, int category, SDL_LogPriority priority, const char *message) {
    (void)user;
    snprintf(g_ring[g_ring_at % RING], RING_W, "%s", message);
    g_ring_at++;
    if (g_default_out) g_default_out(g_default_user, category, priority, message);
}

#ifndef __EMSCRIPTEN__
static void put_str(int fd, const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    if (n) (void)!WRITE_FD(fd, s, (unsigned)n);
}
static void put_int(int fd, long v) {
    char b[24];
    int i = 23;
    bool neg = v < 0;
    unsigned long u = neg ? (unsigned long)-v : (unsigned long)v;
    b[i] = 0;
    do { b[--i] = (char)('0' + u % 10); u /= 10; } while (u && i > 1);
    if (neg) b[--i] = '-';
    put_str(fd, b + i);
}

static void write_crash(const char *what) {
    if (!g_crash_path[0]) return;
#ifdef _WIN32
    int fd = _open(g_crash_path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, 0644);
#else
    int fd = open(g_crash_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
    if (fd < 0) return;
    put_str(fd, "Fortress Crawl stopped: ");
    put_str(fd, what);
    put_str(fd, "\nBuild: " FC_VERSION "\n");
    if (g_game) {
        put_str(fd, "World: ");
        put_str(fd, g_game->opt.seed_text);
        put_str(fd, "\nPlayed: ");
        put_int(fd, (long)g_game->play_time);
        put_str(fd, " s\n");
    }
    put_str(fd, "\nThe last lines of the log:\n");
    int first = g_ring_at > RING ? g_ring_at - RING : 0;
    for (int i = first; i < g_ring_at; i++) {
        put_str(fd, g_ring[i % RING]);
        put_str(fd, "\n");
    }
    CLOSE_FD(fd);
}

static void on_signal(int sig) {
    write_crash(sig == SIGSEGV ? "a memory fault (SIGSEGV)" : sig == SIGABRT ? "an abort (SIGABRT)" : sig == SIGFPE ? "an arithmetic fault (SIGFPE)"
                                                                                                    : "a fault (SIGILL)");
    signal(sig, SIG_DFL);
    raise(sig);
}

#ifdef _WIN32
static LONG WINAPI on_exception(EXCEPTION_POINTERS *e) {
    (void)e;
    write_crash("an unhandled exception");
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
#endif

void crash_init(const Game *g) {
    g_game = g;
    SDL_GetLogOutputFunction(&g_default_out, &g_default_user);
    SDL_SetLogOutputFunction(log_out, NULL);
#ifndef __EMSCRIPTEN__
    char *base = SDL_GetPrefPath("fortress-crawl", "fortress-crawl");
    snprintf(g_crash_path, sizeof g_crash_path, "%scrash.txt", base ? base : "");
    SDL_free(base);
    signal(SIGSEGV, on_signal);
    signal(SIGABRT, on_signal);
    signal(SIGFPE, on_signal);
    signal(SIGILL, on_signal);
#ifdef _WIN32
    SetUnhandledExceptionFilter(on_exception);
#endif
#endif
}

const char *crash_report_path(void) { return g_crash_path; }

/* A world is being played (the marker holds its seed); removed on a clean quit. */
void crash_session_begin(const char *seed_text) { store_write(SESSION_KEY, (const u8 *)seed_text, (int)strlen(seed_text)); }
void crash_session_end(void) { store_remove(SESSION_KEY); }

/* At the start: did the last session end without quitting? Its world into out; the marker goes. */
bool crash_last_session(char *out, int n) {
    int len = 0;
    u8 *d = store_read(SESSION_KEY, &len);
    if (!d) return false;
    snprintf(out, (size_t)n, "%.*s", len, (const char *)d);
    free(d);
    store_remove(SESSION_KEY);
    return true;
}
