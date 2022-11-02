/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

typedef enum {
  Sound_Sine,
  Sound_Triangle,
  Sound_Sawtooth,
  Sound_Square,
  Sound_COUNT,
} Sound;

#include "piano.h"
#include "parse_tune/parse_tune.h"

#define TABLE_LEN 2048


typedef struct {
  uint32_t step;
  Sound sound;
  int sample_duration;
} i2sNote;
static i2sNote i2snote_sound(int freq, int duration_ms, Sound sound) {
  return (i2sNote) {
    .sound = sound,
    .step = (freq * TABLE_LEN) / SAMPLES_PER_SECOND * 0x10000,
    .sample_duration = (SAMPLES_PER_SECOND / 1000) * duration_ms,
  };
}
static i2sNote i2snote_pause(int duration_ms) {
  return i2snote_sound(0, duration_ms, 0);
}

typedef struct {
  uint8_t active;
  double times;

  NoteReadState nrs;
  void *song;

  uint32_t pos;
  int samples_into_note;
  i2sNote i2snote;
} Channel;

/* piano ties together our sample table, our note reader and pico's audio buffer pool */
#define CHAN_COUNT 4
const float sound_weights[Sound_COUNT] = {
  [Sound_Sine]     = 1.00f,
  [Sound_Triangle] = 0.59f,
  [Sound_Square]   = 0.08f,
  [Sound_Sawtooth] = 0.08f
};
static struct {
  int16_t sample_table[Sound_COUNT][TABLE_LEN];

  Channel chan[CHAN_COUNT];

  PianoOpts opts;
} piano_state = {0};

/* type erased jerry_value_t (well, doesn't matter what as long as
 * opts.song_chars and opts.song_free work on it) */
int piano_queue_song(void *song, double times) {
  for (int ci = 0; ci < CHAN_COUNT; ci++) {
    Channel *chan = piano_state.chan + ci;
    if (!chan->active) {
      chan->active = 1;
      chan->times = times;
      chan->song = song;
      return 1;
    }
  }
  return 0;
}

static void piano_chan_free(Channel *chan) {
  if (chan->song) piano_state.opts.song_free(chan->song);
  memset(chan, 0, sizeof(Channel));
  chan->active = 0; /* just to make sure >:) */
}

int piano_unqueue_song(void *p) {
  for (int ci = 0; ci < CHAN_COUNT; ci++) {
    Channel *chan = piano_state.chan + ci;

    if (chan->song == p) {
      piano_chan_free(chan);
      return 1;
    }
  }

  return 0;
}

int piano_is_song_queued(void *p) {
  for (int ci = 0; ci < CHAN_COUNT; ci++) {
    Channel *chan = piano_state.chan + ci;

    if (chan->song == p) return 1;
  }

  return 0;
}

void piano_init(PianoOpts opts) {
  piano_state.opts = opts;

  /* fill sample table */
  for (int i = 0; i < TABLE_LEN; i++) {
    float t = (float)i / (float)TABLE_LEN;
    float soundf[Sound_COUNT] = {
      [Sound_Sine]     = cosf(i * 2 * (float) (M_PI / TABLE_LEN)),
      [Sound_Triangle] = (1.0f - 2.0f * 2.0f * fabsf(0.5f - t)),
      [Sound_Sawtooth] = (1.0f - 2.0f * 2.0f * fmodf(t, 0.5f)),
      [Sound_Square]   = (fmodf(t, 0.5f) > 0.25f) ? 1.0f : -1.0f
    };

    for (int s = 0; s < Sound_COUNT; s++)
      piano_state.sample_table[s][i] = (int)(32767 * sound_weights[s]) * soundf[s];
  }
}

static int16_t piano_compute_sample(Channel *chan, float *vol_used) {
  if (!chan->active) return 0;

  chan->samples_into_note++;
  if (chan->samples_into_note >= chan->i2snote.sample_duration) {
    chan->samples_into_note = 0;

    memset(&chan->i2snote, 0, sizeof(chan->i2snote));

    /* pull the song into this buf so we can read next chord */
    char song[2048] = {0};
    if (!piano_state.opts.song_chars(chan->song, song, 2048)) {
      puts("song exceeds 2k chars, not playing");
      return 0;
    }

    while (1) {
      if (!tune_parse(&chan->nrs, song)) {
        chan->nrs = (NoteReadState) {0};

        chan->times -= 1.0;
        if (chan->times <= 0.0)
          piano_chan_free(chan);

        return 0;
      }

      NrsRet *nrs_ret = &chan->nrs.ret;
      if (nrs_ret->kind != NrsRetKind_None) {

        /* TODO: handle chords (multiple notes per pause) */
        if (nrs_ret->kind == NrsRetKind_Sound)
          chan->i2snote = i2snote_sound(
            nrs_ret->sound.freq,
            nrs_ret->duration,
            nrs_ret->sound.sound
          );
        if (nrs_ret->kind == NrsRetKind_Pause) {
          /* note: not overwriting entire thing. chords end with pauses,
           * would overwrite freq and sound and play nothing */
          chan->i2snote.sample_duration = i2snote_pause(nrs_ret->duration).sample_duration;
          break;
        }
      }
    }
  }

  if (chan->i2snote.step == 0)
    return 0;
  else {
    int ret = (127 * piano_state.sample_table[chan->i2snote.sound][chan->pos >> 16u]) >> 8u;

    /* 128 is 1/2 of 256 possible vol */
    *vol_used += 0.6f * sound_weights[chan->i2snote.sound];
    chan->pos += chan->i2snote.step;

    /* wrap 'round */
    const uint32_t pos_max = 0x10000 * TABLE_LEN;
    if (chan->pos >= pos_max) chan->pos -= pos_max;

    return ret;
  }
}

void piano_fill_sample_buf(int16_t *samples, int size) {
  /* fill buffer */
  for (int i = 0; i < size; i++) {
    samples[i] = 0;

    /* contribution from all channels cannot exceed 1.0f */
    float vol_used = 0.0f;

    for (int ci = 0; ci < CHAN_COUNT; ci++) {
      Channel *chan = piano_state.chan + ci;
      int vol = piano_compute_sample(chan, &vol_used);
      if (vol_used <= 1.0f) samples[i] += vol;
    }
  }
}
