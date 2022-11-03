#include <MiniFB.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "audio.h"

#if 1
  #define dbg(...) ;
  #define dbgf(...) ;
#else
  #define dbgf printf
  #define dbg puts
#endif

#include "jerry_mem.h"

#define yell puts

char errorbuf[512] = "";
static void fatal_error(void) { abort(); }
#include "base_engine.c"

#include "jerryscript.h"
#include "jerryxx.h"

/* jumbo builds out of laziness */
static void module_native_init(jerry_value_t exports);
#include "js.h"
#include "module_native.c"

#define SPADE_WIN_SIZE_X (SCREEN_SIZE_X)
#define SPADE_WIN_SIZE_Y (SCREEN_SIZE_Y + 3*8)
#define SPADE_WIN_SCALE (2)

static void keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
  (void) window;
  if (!isPressed) return;

  if (key == KB_KEY_ESCAPE) mfb_close(window);

  if (key == KB_KEY_W) spade_call_press( 5); // map_move(map_get_first('p'),  0, -1);
  if (key == KB_KEY_S) spade_call_press( 7); // map_move(map_get_first('p'),  0,  1);
  if (key == KB_KEY_A) spade_call_press( 6); // map_move(map_get_first('p'),  1,  0);
  if (key == KB_KEY_D) spade_call_press( 8); // map_move(map_get_first('p'), -1,  0);
  if (key == KB_KEY_I) spade_call_press(12); // map_move(map_get_first('p'),  0, -1);
  if (key == KB_KEY_K) spade_call_press(14); // map_move(map_get_first('p'),  0,  1);
  if (key == KB_KEY_J) spade_call_press(13); // map_move(map_get_first('p'),  1,  0);
  if (key == KB_KEY_L) spade_call_press(15); // map_move(map_get_first('p'), -1,  0);
}

int peak_bitmap_count = 0;
int peak_sprite_count = 0;
static void render_char(Color *screen, char c, Color color, int sx, int sy) {
  for (int y = 0; y < 8; y++) {
    uint8_t bits = font_pixels[c*8 + y];
    for (int x = 0; x < 8; x++)
      if ((bits >> (7-x)) & 1) {
        screen[render_xy_to_idx(sx+x, sy+y)] = color;
      }
  }
}
void render_stats(Color *screen) {
  /*
  +1 on mem because sprintf might write an extra null term, doesn't matter for others
  bc they shouldn't get to filling the buffer

  format with assumed max lengths:

  --------------------
  mem: 1000kB (1000kB)
  bitmaps: 100 (100)
  sprites: 100 (100)
  maps: 100 (100)
  */
  char mem[20 * 8 + 1] = "";
  char bitmaps[20 * 8] = "";
  char sprites[20 * 8] = "";

  Color mem_color = color16(255, 255, 255);
  jerry_heap_stats_t stats = {0};
  if (jerry_get_memory_stats(&stats)) {
    sprintf(mem, "mem: %lukB (%lukB)", stats.allocated_bytes / 1000, stats.peak_allocated_bytes / 1000);
    if (stats.peak_allocated_bytes > 200000) mem_color = color16(255, 255, 0);
    if (stats.allocated_bytes > 200000) mem_color = color16(255, 0, 0);
  }

  int bitmap_count = state->render->doodle_index_count;
  if (bitmap_count > peak_bitmap_count) peak_bitmap_count = bitmap_count;
  sprintf(bitmaps, "bitmaps: %d (%d)", bitmap_count, peak_bitmap_count);

  int sprite_count = 0;
  for (int i = 0; i < state->sprite_pool_size; i++)
    sprite_count += state->sprite_slot_active[i];
  if (sprite_count > peak_sprite_count) peak_sprite_count = sprite_count;
  sprintf(sprites, "sprites: %d (%d)", sprite_count, peak_sprite_count);

  for (int i = 0; i < 20 * 8; i++) {
    if (mem[i] != '\0') render_char(screen, mem[i], mem_color, i*8, SCREEN_SIZE_Y);
    if (bitmaps[i] != '\0') render_char(screen, bitmaps[i], color16(255, 255, 255), i*8, SCREEN_SIZE_Y + 8);
    if (sprites[i] != '\0') render_char(screen, sprites[i], color16(255, 255, 255), i*8, SCREEN_SIZE_Y + 16);
  }
}

static void js_init(void) {
  const jerry_char_t script[] = 
#include "engine.js.cstring"
#include "game.js.cstring"
  ;

  const jerry_length_t script_size = sizeof (script) - 1;
  js_run(script, script_size);
}

void piano_jerry_song_free(void *p) {
  /* it's straight up a jerry_value_t, not even a pointer to one */
  jerry_value_t jvt = (jerry_value_t)p;

  jerry_release_value(jvt);
}
int piano_jerry_song_chars(void *p, char *buf, int buf_len) {
  /* it's straight up a jerry_value_t, not even a pointer to one */
  jerry_value_t jvt = (jerry_value_t)p;

  int read = jerry_string_to_char_buffer(jvt, (jerry_char_t *)buf, (jerry_size_t) buf_len);
  return read;
}

Color *write_pixel_screen = 0;
static void write_pixel(int x, int y, Color c) {
  write_pixel_screen[y*160 + x] = c;
}

int main() {
  struct mfb_window *window = mfb_open_ex("spade", SPADE_WIN_SIZE_X * 2, SPADE_WIN_SIZE_Y * 2, 0);
  if (!window) return 1;
  mfb_set_keyboard_callback(window, keyboard);

  jerry_init (JERRY_INIT_MEM_STATS);
  init(sprite_free_jerry_object); /* god i REALLY need to namespace baseengine */

  State_Render *sr = state->render;

  js_init();
  Color screen[SPADE_WIN_SIZE_X * SPADE_WIN_SIZE_Y] = {0};

  piano_init((PianoOpts) {
    .song_free = piano_jerry_song_free,
    .song_chars = piano_jerry_song_chars,
  });
  audio_init();

  struct mfb_timer *lastframe = mfb_timer_create();
  mfb_timer_now(lastframe);
  do {
    js_promises();
    spade_call_frame(1000.0f * mfb_timer_delta(lastframe));

    mfb_timer_now(lastframe);

    audio_try_push_samples();

    memset(screen, 0, sizeof(screen));
    render_errorbuf();
    write_pixel_screen = screen;
    render(write_pixel);
    render_stats(screen);

    uint8_t ok = STATE_OK == mfb_update_ex(window, screen, SPADE_WIN_SIZE_X, SPADE_WIN_SIZE_Y);
    if (!ok) {
      window = 0x0;
      break;
    }
  } while(mfb_wait_sync(window));

  return 0;
}
