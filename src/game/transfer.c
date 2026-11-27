#include "game/game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* Export and import of a world's save as a .fcsave file (openspec saves: export and import).
 * On the desktop through SDL's file dialogs; where there is no dialog (no portal or zenity), the
 * file goes to (or comes from) the folder the saves are kept in. In the browser a download, and
 * a file chosen in the page. The dialogs answer on their own time (and maybe their own thread):
 * the result waits here until the start screen asks for it (transfer_poll). */

static SDL_Mutex *g_lock;
static char g_note[160];     /* what happened, for the start screen */
static u8 *g_in;             /* imported bytes, waiting */
static int g_in_n;

static void lock(void) {
    if (!g_lock) g_lock = SDL_CreateMutex();
    SDL_LockMutex(g_lock);
}
static void unlock(void) { SDL_UnlockMutex(g_lock); }

static void note(const char *text) {
    lock();
    snprintf(g_note, sizeof g_note, "%s", text);
    unlock();
    SDL_Log("Transfer: %s", text);
}

static void take_bytes(u8 *d, int n) {
    lock();
    free(g_in);
    g_in = d;
    g_in_n = n;
    unlock();
}

/* The start screen: a note to show (copied into out; false if none), and imported bytes. */
bool transfer_poll(char *out, int n, u8 **data, int *len) {
    if (!g_lock) return false;
    lock();
    bool any = false;
    if (g_note[0]) { snprintf(out, (size_t)n, "%s", g_note); g_note[0] = 0; any = true; }
    if (g_in) { *data = g_in; *len = g_in_n; g_in = NULL; g_in_n = 0; any = true; }
    unlock();
    return any;
}

bool transfer_write_file(const char *path, const u8 *d, int n) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(d, 1, (size_t)n, f) == (size_t)n;
    ok &= fclose(f) == 0;
    return ok;
}

u8 *transfer_read_file(const char *path, int *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > (64 << 20)) { fclose(f); return NULL; }
    u8 *d = malloc((size_t)size);
    *n = (int)fread(d, 1, (size_t)size, f);
    fclose(f);
    return d;
}

#ifdef __EMSCRIPTEN__
EM_JS(void, js_download, (const char *name, const u8 *d, int n), {
    try {
        var blob = new Blob([HEAPU8.slice(d, d + n)], { type: "application/octet-stream" });
        var a = document.createElement("a");
        a.href = URL.createObjectURL(blob);
        a.download = UTF8ToString(name);
        document.body.appendChild(a);
        a.click();
        setTimeout(function() { URL.revokeObjectURL(a.href); a.remove(); }, 2000);
    } catch (e) { console.warn("export failed: " + e); }
});
EM_JS(void, js_pick_file, (void), {
    var input = document.createElement("input");
    input.type = "file";
    input.accept = ".fcsave,application/octet-stream";
    input.style.display = "none";
    input.onchange = function() {
        var f = input.files && input.files[0];
        if (!f) return;
        var rd = new FileReader();
        rd.onload = function() { Module.fcImport = new Uint8Array(rd.result); };
        rd.readAsArrayBuffer(f);
        input.remove();
    };
    document.body.appendChild(input);
    input.click();
});
EM_JS(int, js_import_size, (void), { return Module.fcImport ? Module.fcImport.length : 0; });
EM_JS(void, js_import_take, (u8 *out, int n), {
    HEAPU8.set(Module.fcImport.subarray(0, n), out);
    Module.fcImport = null;
});

void transfer_export(const char *name, const u8 *d, int n) {
    js_download(name, d, n);
    note("THE WORLD WAS SAVED AS A DOWNLOAD");
}
void transfer_import(void) { js_pick_file(); }
/* the browser's file arrives a moment later: called each frame of the start screen */
void transfer_update(void) {
    int n = js_import_size();
    if (n <= 0) return;
    u8 *d = malloc((size_t)n);
    js_import_take(d, n);
    take_bytes(d, n);
}
#else
typedef struct Pending { u8 *d; int n; char name[64]; } Pending;

static const SDL_DialogFileFilter FILTERS[] = { { "Fortress Crawl world", "fcsave" }, { "All files", "*" } };

static void fallback_dir(char *out, size_t n) {
    char *base = SDL_GetPrefPath("fortress-crawl", "fortress-crawl");
    snprintf(out, n, "%s", base ? base : "");
    SDL_free(base);
}

static void on_save(void *user, const char *const *files, int filter) {
    Pending *p = user;
    (void)filter;
    char buf[160];
    if (files && files[0]) {
        char path[1024];
        snprintf(path, sizeof path, "%s", files[0]);
        if (!strstr(path, ".fcsave")) strncat(path, ".fcsave", sizeof path - strlen(path) - 1);
        snprintf(buf, sizeof buf, transfer_write_file(path, p->d, p->n) ? "EXPORTED TO %.120s" : "COULD NOT WRITE %.120s", path);
        note(buf);
    } else if (files) {
        note("EXPORT CANCELLED");
    } else { /* no dialog here: next to the saves */
        char dir[900], path[1024];
        fallback_dir(dir, sizeof dir);
        snprintf(path, sizeof path, "%s%s.fcsave", dir, p->name);
        snprintf(buf, sizeof buf, transfer_write_file(path, p->d, p->n) ? "NO FILE DIALOG - EXPORTED TO %.110s" : "COULD NOT WRITE %.120s", path);
        note(buf);
    }
    free(p->d);
    free(p);
}

static void on_open(void *user, const char *const *files, int filter) {
    (void)user;
    (void)filter;
    char buf[160];
    if (files && files[0]) {
        int n = 0;
        u8 *d = transfer_read_file(files[0], &n);
        if (d) take_bytes(d, n);
        else { snprintf(buf, sizeof buf, "COULD NOT READ %.120s", files[0]); note(buf); }
    } else if (files) {
        note("IMPORT CANCELLED");
    } else { /* no dialog here: the newest .fcsave in the saves' folder */
        char dir[900];
        fallback_dir(dir, sizeof dir);
        int count = 0;
        char **found = SDL_GlobDirectory(dir, "*.fcsave", 0, &count);
        if (count > 0) {
            char path[1024];
            snprintf(path, sizeof path, "%s%s", dir, found[count - 1]);
            int n = 0;
            u8 *d = transfer_read_file(path, &n);
            if (d) take_bytes(d, n);
        } else {
            snprintf(buf, sizeof buf, "NO FILE DIALOG - PUT THE .FCSAVE FILE IN %.100s", dir);
            note(buf);
        }
        SDL_free(found);
    }
}

void transfer_export(const char *name, const u8 *d, int n) {
    Pending *p = calloc(1, sizeof *p);
    p->d = malloc((size_t)n);
    memcpy(p->d, d, (size_t)n);
    p->n = n;
    snprintf(p->name, sizeof p->name, "%s", name);
    char def[128];
    snprintf(def, sizeof def, "%s.fcsave", name);
    SDL_ShowSaveFileDialog(on_save, p, SDL_GetKeyboardFocus(), FILTERS, 2, def);
}
void transfer_import(void) { SDL_ShowOpenFileDialog(on_open, NULL, SDL_GetKeyboardFocus(), FILTERS, 2, NULL, false); }
void transfer_update(void) {}
#endif
