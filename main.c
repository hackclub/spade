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

#include "jerryscript.h"
static struct {
  jerry_value_t press_cb, frame_cb;
} spade_state = {0};

static void oom() { yell("oom!"); abort(); }
#include "base_engine.c"

#include "module_native.c"

JERRYXX_FUN(console_log) {
  jerryxx_print_value(JERRYXX_GET_ARG(0));
  return jerry_create_undefined();
}

static void js_init(void) {
  const jerry_char_t script[] = 
#include "engine.js.cstring"
  ;

  const jerry_length_t script_size = sizeof (script) - 1;

  /* Initialize engine */
  jerry_init (JERRY_INIT_MEM_STATS);

  /* add shit to global scpoe */
  {
    jerry_value_t global_object = jerry_get_global_object ();

    /* add the "console" module to the JavaScript global object */
    {
      jerry_value_t console_obj = jerry_create_object ();

      jerryxx_set_property_function(console_obj, "log", console_log);

      jerry_value_t prop_console = jerry_create_string ((const jerry_char_t *) "console");
      jerry_release_value(jerry_set_property(global_object, prop_console, console_obj));
      jerry_release_value (prop_console);

      /* Release all jerry_value_t-s */
      jerry_release_value (console_obj);
    }

    /* add the "native" module to the JavaScript global object */
    {
      jerry_value_t native_obj = jerry_create_object ();

      module_native_init(native_obj);

      jerry_value_t prop_native = jerry_create_string ((const jerry_char_t *) "native");
      jerry_release_value(jerry_set_property(global_object, prop_native, native_obj));
      jerry_release_value (prop_native);

      /* Release all jerry_value_t-s */
      jerry_release_value (native_obj);
    }

    jerry_release_value(global_object);
  }

  /* Setup Global scope code */
  jerry_value_t parsed_code = jerry_parse (
    (jerry_char_t *)"src", sizeof("src")-1,
    script, script_size,
    JERRY_PARSE_STRICT_MODE
  );

  if (jerry_value_is_error (parsed_code)) {
    yell("couldn't parse :(");
    jerryxx_print_error(parsed_code, 1);
    abort();
  }

  /* Execute the parsed source code in the Global scope */
  jerry_value_t ret_value = jerry_run (parsed_code);

  if (jerry_value_is_error (ret_value)) {
    yell("couldn't run :(");
    jerryxx_print_error(ret_value, 1);
    abort();
  }

  /* Returned value must be freed */
  jerry_release_value (ret_value);

  /* Parsed source code must be freed */
  jerry_release_value (parsed_code);

  /* Cleanup engine */
  // jerry_cleanup ();
}

static void spade_call_press(int pin) {
  if (!spade_state.press_cb) return;

  jerry_value_t this_value = jerry_create_undefined();
  jerry_value_t args[] = { jerry_create_number(pin) };

  jerry_value_t res = jerry_call_function(
    spade_state.press_cb,
    jerry_create_undefined(),
    args,
    1
  );

  if (jerry_value_is_error (res)) {
    yell("couldn't call press_cb :(");
    jerryxx_print_error(res, 1);
    abort();
  }

  jerry_release_value(res);

  jerry_release_value(args[0]);
  jerry_release_value(this_value);
}
static void spade_call_frame(double dt) {
  if (!spade_state.frame_cb) return;

  jerry_value_t this_value = jerry_create_undefined();
  jerry_value_t args[] = { jerry_create_number(dt) };

  jerry_value_t res = jerry_call_function(
    spade_state.frame_cb,
    jerry_create_undefined(),
    args,
    1
  );
  if (jerry_value_is_error (res)) {
    yell("couldn't call frame_cb :(");
    jerryxx_print_error(res, 1);
    abort();
  }
  jerry_release_value(res);

  jerry_release_value(args[0]);
  jerry_release_value(this_value);
}

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
    memset(screen, 0, sizeof(screen));
    render((Color *) screen);
    spade_call_frame(mfb_timer_delta(lastframe));
    mfb_timer_now(lastframe);

    uint8_t ok = STATE_OK == mfb_update_ex(window, screen, SCREEN_SIZE_X, SCREEN_SIZE_Y);
    if (!ok) {
      window = 0x0;
      break;
    }
  } while(mfb_wait_sync(window));

  return 0;
}
