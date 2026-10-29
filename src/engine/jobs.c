#include "engine/jobs.h"

#include <SDL3/SDL.h>

#define MAX_WORKERS 16

static SDL_Thread *g_threads[MAX_WORKERS];
static int g_nthreads;
static SDL_Mutex *g_lock;
static SDL_Condition *g_cond;
static Job *g_pending_head, *g_pending_tail;
static Job *g_done_head, *g_done_tail;
static int g_in_flight;
static bool g_quit;

typedef struct WorkerArg { int index; } WorkerArg;
static WorkerArg g_args[MAX_WORKERS];

static int worker_main(void *p) {
    int index = ((WorkerArg *)p)->index;
    for (;;) {
        SDL_LockMutex(g_lock);
        while (!g_pending_head && !g_quit) SDL_WaitCondition(g_cond, g_lock);
        if (g_quit) { SDL_UnlockMutex(g_lock); return 0; }
        Job *j = g_pending_head;
        g_pending_head = j->next;
        if (!g_pending_head) g_pending_tail = NULL;
        SDL_UnlockMutex(g_lock);

        j->run(j, index);

        SDL_LockMutex(g_lock);
        j->next = NULL;
        if (g_done_tail) g_done_tail->next = j; else g_done_head = j;
        g_done_tail = j;
        SDL_UnlockMutex(g_lock);
    }
}

int jobs_init(int threads) {
    if (threads < 1) threads = 1;
    if (threads > MAX_WORKERS) threads = MAX_WORKERS;
    g_lock = SDL_CreateMutex();
    g_cond = SDL_CreateCondition();
    for (int i = 0; i < threads; i++) {
        g_args[i].index = i;
        g_threads[i] = SDL_CreateThread(worker_main, "fc-worker", &g_args[i]);
        if (g_threads[i]) g_nthreads++;
    }
    return g_nthreads;
}

void jobs_shutdown(void) {
    SDL_LockMutex(g_lock);
    g_quit = true;
    SDL_BroadcastCondition(g_cond);
    SDL_UnlockMutex(g_lock);
    for (int i = 0; i < g_nthreads; i++) SDL_WaitThread(g_threads[i], NULL);
    g_nthreads = 0;
}

void jobs_submit(Job *job) {
    job->next = NULL;
    SDL_LockMutex(g_lock);
    if (g_pending_tail) g_pending_tail->next = job; else g_pending_head = job;
    g_pending_tail = job;
    g_in_flight++;
    SDL_SignalCondition(g_cond);
    SDL_UnlockMutex(g_lock);
}

int jobs_poll(int max_done) {
    int n = 0;
    while (n < max_done) {
        SDL_LockMutex(g_lock);
        Job *j = g_done_head;
        if (j) {
            g_done_head = j->next;
            if (!g_done_head) g_done_tail = NULL;
            g_in_flight--;
        }
        SDL_UnlockMutex(g_lock);
        if (!j) break;
        j->done(j);
        n++;
    }
    return n;
}

int jobs_in_flight(void) { return g_in_flight; }
int jobs_worker_count(void) { return g_nthreads; }
