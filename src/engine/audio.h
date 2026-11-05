#ifndef FC_AUDIO_H
#define FC_AUDIO_H

#include <stdbool.h>

/* A small software mixer on an SDL3 audio stream: mono samples, stereo out, per-voice
 * volume, pan and pitch. Calls are no-ops when there is no audio device. */

bool audio_init(void);
void audio_shutdown(void);
bool audio_ready(void);

/* Loads a WAV file (any format SDL reads) as a sample; returns its handle or -1. */
int audio_load(const char *path);
float audio_length(int sample); /* seconds */

/* Starts a voice; returns a handle for audio_set / audio_stop, or 0 when nothing plays.
 * pan: -1 left .. 1 right, pitch: playback rate factor. */
unsigned audio_play(int sample, float vol, float pan, float pitch, bool loop);
void audio_set(unsigned voice, float vol, float pan); /* changes are smoothed */
void audio_stop(unsigned voice);
bool audio_playing(unsigned voice);

void audio_set_master(float vol);
float audio_master(void);

#endif
