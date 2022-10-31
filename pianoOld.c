/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#if PICO_ON_DEVICE
  #include "hardware/clocks.h"
  #include "hardware/structs/clocks.h"
#endif

#include "pico/stdlib.h"

#include "pico/audio_i2s.h"

#if PICO_ON_DEVICE
  #include "pico/binary_info.h"
  bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE+1, "I2S LRCK"));
#endif

/* gaps in your audio? increase this */
#define SAMPLES_PER_BUFFER (256*16)
#define SAMPLES_PER_SECOND 24000
#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

static struct audio_buffer_pool *audio_buffer_pool_init() {

  static audio_format_t audio_format = {
    .format = AUDIO_BUFFER_FORMAT_PCM_S16,
    .sample_freq = SAMPLES_PER_SECOND,
    .channel_count = 1,
  };

  static struct audio_buffer_format producer_format = {
    .format = &audio_format,
    .sample_stride = 2
  };

  struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                    SAMPLES_PER_BUFFER); // todo correct size
  bool __unused ok;
  const struct audio_format *output_format;

  struct audio_i2s_config config = {
    .data_pin = PICO_AUDIO_I2S_DATA_PIN,
    .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
    .dma_channel = 0,
    .pio_sm = 0,
  };

  output_format = audio_i2s_setup(&audio_format, &config);
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }

  ok = audio_i2s_connect(producer_pool);
  assert(ok);
  audio_i2s_set_enabled(true);

  return producer_pool;
}

typedef enum {
  Sound_Sine,
  Sound_Triangle,
  Sound_Sawtooth,
  Sound_Square,
  Sound_COUNT,
} Sound;

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

/* piano ties together our sample table, our note reader and pico's audio buffer pool */
#define CHANNEL_MAX 1
typedef struct {
  uint8_t active;

  NoteReadState nrs;
  uint32_t pos;
  int samples_into_note;
  i2sNote i2snote;
} Channel;

static struct {
  Channel channels[CHANNEL_MAX];

  int16_t sample_table[Sound_COUNT][TABLE_LEN];
  struct audio_buffer_pool *ap;
} piano_state = {0};

int piano_queue_song(char *song) {
  for (int i = 0; i < CHANNEL_MAX; i++) {
    Channel *chan = piano_state.channels + i;
    if (chan->active) continue;

    *chan = (Channel) {
      .active = 1,
      .nrs = { .str = song, .str_complete = song },
    };
    return 1;
  }
  return 0;
}

void piano_init(void) {
  /* fill sample table */
  for (int i = 0; i < TABLE_LEN; i++) {
    piano_state.sample_table[Sound_Sine  ][i] = 32767 * cosf(i * 2 * (float) (M_PI / TABLE_LEN));

    float t = (float)i / (float)TABLE_LEN;
    piano_state.sample_table[Sound_Triangle][i] = 32767 * (1.0f - 2.0f * 2.0f * fabsf(0.5 - t));
    piano_state.sample_table[Sound_Sawtooth][i] = 32767 * (1.0f - 2.0f * 2.0f * fmodf(t, 0.5f));
    piano_state.sample_table[Sound_Square  ][i] = 32767 * ((fmodf(t, 0.5f) > 0.25f) ? 1.0f : -1.0f);
  }

  /* initialize pico-audio audio_buffer_pool */
  piano_state.ap = audio_buffer_pool_init();
}

/* 255 max / 5 channels = 51 */
#define VOLUME (51)
static int16_t piano_compute_sample(Channel *chan) {
  if (!chan->active) return 0;

  chan->samples_into_note++;
  if (chan->samples_into_note >= chan->i2snote.sample_duration) {
    chan->samples_into_note = 0;

    memset(&chan->i2snote, 0, sizeof(chan->i2snote));
    while (true) {

      if (!chan->nrs.str || !tune_parse(&chan->nrs)) {
        chan->active = 0;
        /* not looping */
        // char *song = chan->nrs.str_complete;
        // chan->nrs = (NoteReadState) { .str = song, .str_complete = song };
        return 0;
      }

      NrsRet *nrs_ret = &chan->nrs.ret;
      if (nrs_ret->kind != NrsRetKind_None) {

        if (nrs_ret->kind == NrsRetKind_Sound)
          chan->i2snote = i2snote_sound(
            nrs_ret->sound.freq,
            nrs_ret->duration,
            nrs_ret->sound.sound
          );
        if (nrs_ret->kind == NrsRetKind_Pause) {
          chan->i2snote.sample_duration = i2snote_pause(nrs_ret->duration).sample_duration;
          break;
        }
      }
    }
  }

  if (chan->i2snote.step == 0)
    return 0;
  else {
    int ret = (VOLUME * piano_state.sample_table[Sound_Sine][chan->pos >> 16u]) >> 8u;
    chan->pos += chan->i2snote.step;

    /* wrap 'round */
    const uint32_t pos_max = 0x10000 * TABLE_LEN;
    if (chan->pos >= pos_max) chan->pos -= pos_max;

    return ret;
  }
}

void piano_try_push_samples(void) {
  struct audio_buffer *buffer = take_audio_buffer(piano_state.ap, false);
  if (buffer == NULL) return;

  /* fill buffer */
  int16_t *samples = (int16_t *) buffer->buffer->bytes;
  for (uint i = 0; i < buffer->max_sample_count; i++) {
    samples[i] = 0;
    for (int i = 0; i < CHANNEL_MAX; i++) {
      Channel *chan = piano_state.channels + i;
      samples[i] += piano_compute_sample(chan);
    }
  }
  buffer->sample_count = buffer->max_sample_count;

  /* send to PIO DMA */
  give_audio_buffer(piano_state.ap, buffer);
}

/* EX:
int main() {
  // stdio_init_all();

  piano_init();

  while (true) piano_try_push_samples();

  puts("...\n");

  return 0;
} */
