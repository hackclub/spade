#include <MiniFB.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#if 1
#define dbg(...) ;
#else
#define dbg puts
#endif

#define yell puts

static void oom(void) { yell("oom!"); abort(); }
char errorbuf[512] = "";
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
  for (int i = 0; i < SPRITE_COUNT; i++) {
    if (state->sprite_slot_active[i] != 0) sprite_count++;
  }
  if (sprite_count > peak_sprite_count) peak_sprite_count = sprite_count;
  sprintf(sprites, "sprites: %d (%d)", sprite_count, peak_sprite_count);

  for (int i = 0; i < 20 * 8; i++) {
    if (mem[i] != '\0') render_char(screen, mem[i], mem_color, i*8, SCREEN_SIZE_Y);
    if (bitmaps[i] != '\0') render_char(screen, bitmaps[i], color16(255, 255, 255), i*8, SCREEN_SIZE_Y + 8);
    if (sprites[i] != '\0') render_char(screen, sprites[i], color16(255, 255, 255), i*8, SCREEN_SIZE_Y + 16);
  }
}

int main() {
  struct mfb_window *window = mfb_open_ex("spade", SPADE_WIN_SIZE_X * 2, SPADE_WIN_SIZE_Y * 2, 0);
  if (!window) return 1;
  mfb_set_keyboard_callback(window, keyboard);

  init(sprite_free_jerry_object); /* god i REALLY need to namespace baseengine */
  js_init();
  Color screen[SPADE_WIN_SIZE_X * SPADE_WIN_SIZE_Y] = {0};

  struct mfb_timer *lastframe = mfb_timer_create();
  mfb_timer_now(lastframe);
  do {
    js_promises();
    spade_call_frame(1000.0f * mfb_timer_delta(lastframe));

    mfb_timer_now(lastframe);

    memset(screen, 0, sizeof(screen));
    render((Color *) screen); /* baseengine */
    render_errorbuf(screen);
    render_stats(screen);

    uint8_t ok = STATE_OK == mfb_update_ex(window, screen, SPADE_WIN_SIZE_X, SPADE_WIN_SIZE_Y);
    if (!ok) {
      window = 0x0;
      break;
    }
  } while(mfb_wait_sync(window));

  return 0;
}
