#include "engine/audio.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define OUT_RATE 44100
#define MAX_SAMPLES 512
#define MAX_VOICES 48
#define MIX_FRAMES 1024

typedef struct Sample {
    Sint16 *data;
    int frames, rate;
} Sample;

typedef struct Voice {
    unsigned id;   /* 0: free */
    int sample;
    double pos;    /* in source frames */
    float step;    /* source frames per output frame */
    float vol, pan, want_vol, want_pan;
    bool loop, stopping;
} Voice;

static SDL_AudioStream *stream;
static Sample samples[MAX_SAMPLES];
static int nsamples;
static Voice voices[MAX_VOICES];
static unsigned next_id = 1;
static float master = 0.8f;
static float mixbuf[MIX_FRAMES * 2];

static void mix(float *out, int frames) {
    memset(out, 0, sizeof(float) * 2 * frames);
    for (int v = 0; v < MAX_VOICES; v++) {
        Voice *vc = &voices[v];
        if (!vc->id) continue;
        const Sample *s = &samples[vc->sample];
        /* volume and pan glide to their targets within this block (no zipper noise);
         * constant-power pan */
        float want_vol = vc->stopping ? 0.0f : vc->want_vol;
        float a0 = (vc->pan + 1.0f) * 0.25f * 3.14159265f, a1 = (vc->want_pan + 1.0f) * 0.25f * 3.14159265f;
        float l = vc->vol * cosf(a0), r = vc->vol * sinf(a0);
        float dl = (want_vol * cosf(a1) - l) / frames, dr = (want_vol * sinf(a1) - r) / frames;
        for (int i = 0; i < frames; i++) {
            int p0 = (int)vc->pos;
            if (p0 >= s->frames - 1) {
                if (!vc->loop) { vc->id = 0; break; }
                vc->pos -= s->frames - 1;
                p0 = (int)vc->pos;
            }
            float t = (float)(vc->pos - p0);
            float x = (s->data[p0] * (1.0f - t) + s->data[p0 + 1] * t) * (1.0f / 32768.0f);
            l += dl;
            r += dr;
            out[i * 2] += x * l;
            out[i * 2 + 1] += x * r;
            vc->pos += vc->step;
        }
        vc->vol = want_vol;
        vc->pan = vc->want_pan;
        if (vc->stopping) vc->id = 0;
    }
    /* soft clip so many sounds at once do not crack */
    for (int i = 0; i < frames * 2; i++) {
        float x = out[i] * master;
        out[i] = x / (1.0f + fabsf(x) * 0.35f) * 1.1f;
        if (out[i] > 1.0f) out[i] = 1.0f;
        if (out[i] < -1.0f) out[i] = -1.0f;
    }
}

static void SDLCALL feed(void *user, SDL_AudioStream *st, int additional, int total) {
    (void)user;
    (void)total;
    int frames = additional / (int)(sizeof(float) * 2);
    while (frames > 0) {
        int n = frames < MIX_FRAMES ? frames : MIX_FRAMES;
        mix(mixbuf, n);
        SDL_PutAudioStreamData(st, mixbuf, n * (int)sizeof(float) * 2);
        frames -= n;
    }
}

bool audio_init(void) {
    if (stream) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Audio: no audio (%s)", SDL_GetError());
        return false;
    }
    SDL_AudioSpec spec = { SDL_AUDIO_F32, 2, OUT_RATE };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, NULL);
    if (!stream) {
        SDL_Log("Audio: could not open a device (%s)", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream);
    return true;
}

void audio_shutdown(void) {
    if (!stream) return;
    SDL_DestroyAudioStream(stream);
    stream = NULL;
    for (int i = 0; i < nsamples; i++) SDL_free(samples[i].data);
    nsamples = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool audio_ready(void) { return stream != NULL; }

int audio_load(const char *path) {
    if (!stream || nsamples >= MAX_SAMPLES) return -1;
    SDL_AudioSpec spec;
    Uint8 *buf = NULL;
    Uint32 len = 0;
    if (!SDL_LoadWAV(path, &spec, &buf, &len)) return -1;
    /* keep it at its own rate as mono 16-bit; the mixer resamples while playing */
    SDL_AudioSpec want = { SDL_AUDIO_S16, 1, spec.freq };
    Uint8 *conv = NULL;
    int clen = 0;
    bool ok = SDL_ConvertAudioSamples(&spec, buf, (int)len, &want, &conv, &clen);
    SDL_free(buf);
    if (!ok || clen < 4) { SDL_free(conv); return -1; }
    Sample *s = &samples[nsamples];
    s->data = (Sint16 *)conv;
    s->frames = clen / 2;
    s->rate = spec.freq;
    return nsamples++;
}

float audio_length(int sample) {
    if (sample < 0 || sample >= nsamples) return 0.0f;
    return (float)samples[sample].frames / (float)samples[sample].rate;
}

static Voice *find(unsigned id) {
    if (!id) return NULL;
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].id == id) return &voices[i];
    return NULL;
}

unsigned audio_play(int sample, float vol, float pan, float pitch, bool loop) {
    if (!stream || sample < 0 || sample >= nsamples || vol <= 0.001f) return 0;
    SDL_LockAudioStream(stream);
    /* a free voice, or else the quietest one-shot */
    int best = -1;
    float quiet = vol;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].id) { best = i; break; }
        /* never a loop or a long piece (music) */
        const Sample *vs = &samples[voices[i].sample];
        if (!voices[i].loop && vs->frames < vs->rate * 20 && voices[i].want_vol < quiet) { quiet = voices[i].want_vol; best = i; }
    }
    unsigned id = 0;
    if (best >= 0) {
        Voice *v = &voices[best];
        id = next_id++;
        if (!next_id) next_id = 1;
        v->id = id;
        v->sample = sample;
        v->pos = 0;
        v->step = (float)samples[sample].rate / (float)OUT_RATE * pitch;
        v->vol = loop ? 0.0f : vol; /* loops fade in */
        v->want_vol = vol;
        v->pan = v->want_pan = pan < -1 ? -1 : pan > 1 ? 1 : pan;
        v->loop = loop;
        v->stopping = false;
    }
    SDL_UnlockAudioStream(stream);
    return id;
}

void audio_set(unsigned voice, float vol, float pan) {
    if (!stream) return;
    SDL_LockAudioStream(stream);
    Voice *v = find(voice);
    if (v) {
        v->want_vol = vol;
        v->want_pan = pan < -1 ? -1 : pan > 1 ? 1 : pan;
    }
    SDL_UnlockAudioStream(stream);
}

void audio_stop(unsigned voice) {
    if (!stream) return;
    SDL_LockAudioStream(stream);
    Voice *v = find(voice);
    if (v) v->stopping = true;
    SDL_UnlockAudioStream(stream);
}

bool audio_playing(unsigned voice) {
    if (!stream) return false;
    SDL_LockAudioStream(stream);
    bool on = find(voice) != NULL;
    SDL_UnlockAudioStream(stream);
    return on;
}

void audio_set_master(float vol) {
    if (stream) SDL_LockAudioStream(stream);
    master = vol < 0 ? 0 : vol > 1.5f ? 1.5f : vol;
    if (stream) SDL_UnlockAudioStream(stream);
}
float audio_master(void) { return master; }
