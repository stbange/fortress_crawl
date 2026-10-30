#ifndef FC_JOBS_H
#define FC_JOBS_H

/* A small worker pool. Embed Job at the start of your own struct; `run` executes
 * on a worker thread, `done` runs later on the main thread inside jobs_poll(). */
typedef struct Job {
    void (*run)(struct Job *job, int worker);
    void (*done)(struct Job *job);
    struct Job *next;
} Job;

/* Returns the number of workers started. 0 workers: jobs run inside jobs_poll() instead. */
int jobs_init(int threads);
void jobs_shutdown(void);
void jobs_submit(Job *job);
int jobs_poll(int max_done); /* runs finished jobs' done callbacks, returns how many */
int jobs_in_flight(void);
int jobs_worker_count(void);

#endif
