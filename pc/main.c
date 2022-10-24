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
#include "base_engine.c"

#include "jerryscript.h"
#include "jerryxx.h"

/* jumbo builds out of laziness */
static void module_native_init(jerry_value_t exports);
#include "js.h"
#include "module_native.c"

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

int main() {
  struct mfb_window *window = mfb_open_ex(
    "spade - sprigtestbed",
    SCREEN_SIZE_X,
    SCREEN_SIZE_Y,
    0
  );
  if (!window) return 0;
  mfb_set_keyboard_callback(window, keyboard);

  init(sprite_free_jerry_object); /* god i REALLY need to namespace baseengine */
  js_init();
  Color screen[SCREEN_SIZE_Y * SCREEN_SIZE_X] = {0};

  struct mfb_timer *lastframe = mfb_timer_create();
  mfb_timer_now(lastframe);
  do {
    js_promises();
    spade_call_frame(1000.0f * mfb_timer_delta(lastframe));

    mfb_timer_now(lastframe);

    memset(screen, 0, sizeof(screen));
    render((Color *) screen); /* baseengine */

    uint8_t ok = STATE_OK == mfb_update_ex(window, screen, SCREEN_SIZE_X, SCREEN_SIZE_Y);
    if (!ok) {
      window = 0x0;
      break;
    }
  } while(mfb_wait_sync(window));

  return 0;
}
