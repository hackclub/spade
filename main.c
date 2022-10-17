#include <MiniFB.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static void oom() { puts("oom!"); abort(); }
#include "base_engine.c"

static void js_init(void) {
  init(); /* god i REALLY need to namespace baseengine */

  legend_doodle_set('p', 
  "................\n"
  "................\n"
  "................\n"
  ".......0........\n"
  ".....00.000.....\n"
  "....0.....00....\n"
  "....0.0.0..0....\n"
  "....0......0....\n"
  "....0......0....\n"
  "....00....0.....\n"
  "......00000.....\n"
  "......0...0.....\n"
  "....000...000...\n"
  "................\n"
  "................\n"
  "................\n"
  );
  legend_doodle_set('w', 
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  "0000000000000000\n"
  );
  legend_prepare();

  solids_push('w');
  solids_push('p');

  map_set(
  "w.w\n"
  ".p.\n"
  "w.w"
  );
}

void keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
  (void) window;
  if (!isPressed) return;

  if (key == KB_KEY_ESCAPE) mfb_close(window);
  if (key == KB_KEY_W) map_move(map_get_first('p'),  0, -1);
  if (key == KB_KEY_S) map_move(map_get_first('p'),  0,  1);
  if (key == KB_KEY_A) map_move(map_get_first('p'),  1,  0);
  if (key == KB_KEY_D) map_move(map_get_first('p'), -1,  0);
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

  js_init();
  Color screen[SCREEN_SIZE_Y * SCREEN_SIZE_X] = {0};

  do {
    render((Color *) screen);

    uint8_t ok = STATE_OK == mfb_update_ex(window, screen, SCREEN_SIZE_X, SCREEN_SIZE_Y);
    if (!ok) {
      window = 0x0;
      break;
    }
  } while(mfb_wait_sync(window));

  return 0;
}
