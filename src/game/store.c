#include "game/game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* Where things are kept between sessions: the worlds' saves, the settings, the marker of a
 * session that is running. In the browser it is localStorage (base64), on the desktop one file
 * per key in the user's preference folder, written then renamed so a crash never leaves half a
 * file. (Split out of save.c for the settings and the list of worlds: openspec front-end.) */

#ifdef __EMSCRIPTEN__
EM_JS_DEPS(fc_store, "$UTF8ToString,$stringToUTF8,$lengthBytesUTF8");
EM_JS(int, js_store_write, (const char *key, const unsigned char *data, int n), {
    try {
        var bytes = HEAPU8.subarray(data, data + n);
        var s = "";
        for (var i = 0; i < n; i += 8192) s += String.fromCharCode.apply(null, bytes.subarray(i, Math.min(n, i + 8192)));
        localStorage.setItem(UTF8ToString(key), btoa(s));
        return 1;
    } catch (e) {
        console.warn('save failed: ' + e);
        return 0;
    }
});
EM_JS(int, js_store_size, (const char *key), {
    try {
        var v = localStorage.getItem(UTF8ToString(key));
        if (v === null) return -1;
        return atob(v).length;
    } catch (e) { return -1; }
});
EM_JS(void, js_store_read, (const char *key, unsigned char *out, int n), {
    try {
        var s = atob(localStorage.getItem(UTF8ToString(key)));
        for (var i = 0; i < n && i < s.length; i++) HEAPU8[out + i] = s.charCodeAt(i);
    } catch (e) {}
});
EM_JS(void, js_store_remove, (const char *key), {
    try { localStorage.removeItem(UTF8ToString(key)); } catch (e) {}
});
/* the keys that start with prefix, joined by newlines, into out (cap bytes) */
EM_JS(int, js_store_list, (const char *prefix, char *out, int cap), {
    try {
        var p = UTF8ToString(prefix), keys = [];
        for (var i = 0; i < localStorage.length; i++) {
            var k = localStorage.key(i);
            if (k && k.indexOf(p) === 0) keys.push(k);
        }
        var s = keys.join("\n");
        if (lengthBytesUTF8(s) + 1 > cap) return -1;
        stringToUTF8(s, out, cap);
        return keys.length;
    } catch (e) { return 0; }
});

bool store_write(const char *key, const u8 *data, int n) { return js_store_write(key, data, n) != 0; }
u8 *store_read(const char *key, int *n) {
    int size = js_store_size(key);
    if (size < 0) return NULL;
    u8 *buf = malloc((size_t)size + 1);
    js_store_read(key, buf, size);
    buf[size] = 0;
    *n = size;
    return buf;
}
void store_remove(const char *key) { js_store_remove(key); }
int store_list(const char *prefix, char (*keys)[64], int max) {
    static char buf[16384];
    int count = js_store_list(prefix, buf, sizeof buf);
    if (count <= 0) return 0;
    int n = 0;
    for (char *p = buf; *p && n < max;) {
        char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len < 64) { memcpy(keys[n], p, len); keys[n][len] = 0; n++; }
        if (!e) break;
        p = e + 1;
    }
    return n;
}
#else
static const char *store_base(void) {
    static char *base;
    if (!base) base = SDL_GetPrefPath("fortress-crawl", "fortress-crawl");
    return base ? base : "";
}
static void clean_key(const char *key, char *out, size_t n) {
    size_t k = 0;
    for (const char *p = key; *p && k + 1 < n; p++) out[k++] = (*p == ':' || *p == '/') ? '_' : *p;
    out[k] = 0;
}
static void store_path(const char *key, char *out, size_t n) {
    char clean[96];
    clean_key(key, clean, sizeof clean);
    snprintf(out, n, "%s%s.sav", store_base(), clean);
}
bool store_write(const char *key, const u8 *data, int n) {
    char path[512], tmp[520];
    store_path(key, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, (size_t)n, f) == (size_t)n;
    ok &= fclose(f) == 0;
    /* write then rename, so a crash never leaves half a save */
    remove(path);
    return ok && rename(tmp, path) == 0;
}
u8 *store_read(const char *key, int *n) {
    char path[512];
    store_path(key, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }
    u8 *buf = malloc((size_t)size + 1);
    *n = (int)fread(buf, 1, (size_t)size, f);
    buf[*n] = 0;
    fclose(f);
    return buf;
}
void store_remove(const char *key) {
    char path[512];
    store_path(key, path, sizeof path);
    remove(path);
}
/* The keys starting with prefix (as the files are named: ':' became '_'; the name without
 * ".sav" works as a key again, since cleaning it changes nothing). */
int store_list(const char *prefix, char (*keys)[64], int max) {
    char pat[96];
    clean_key(prefix, pat, sizeof pat - 6);
    strcat(pat, "*.sav");
    int count = 0;
    char **files = SDL_GlobDirectory(store_base(), pat, 0, &count);
    int n = 0;
    for (int i = 0; files && i < count && n < max; i++) {
        size_t len = strlen(files[i]);
        if (len < 4 || len - 4 >= 64) continue;
        memcpy(keys[n], files[i], len - 4);
        keys[n][len - 4] = 0;
        n++;
    }
    SDL_free(files);
    return n;
}
#endif
