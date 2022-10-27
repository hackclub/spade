#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "pico/util/queue.h"
#include "pico/multicore.h"

#include "ST7735_TFT.h"
#include "upload.h"

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define yell puts
#if 0
  #define dbg puts
  #define dbgf printf
#else
  #define dbg(...) ;
  #define dbgf(...) ;
#endif

static void oom(void) { puts("oom!"); }
char errorbuf[512] = "";
#include "base_engine.c"

#include "jerryscript.h"
#include "jerryxx.h"

/* jumbo builds out of laziness */
static void module_native_init(jerry_value_t exports);
#include "js.h"
#include "module_native.c"

typedef struct {
  absolute_time_t last_up, last_down;
  uint8_t last_state, edge;
} ButtonState;
uint button_pins[] = {  5,  7,  6,  8, 12, 14, 13, 15 };
static ButtonState button_states[ARR_LEN(button_pins)] = {0};

static void button_init(void) {
  for (int i = 0; i < ARR_LEN(button_pins); i++) {
    ButtonState *bs = button_states + i;
    bs->edge = 1;
    bs->last_up = bs->last_down = get_absolute_time();

    gpio_set_dir(button_pins[i], GPIO_IN);
    gpio_pull_up(button_pins[i]);
    // gpio_set_input_hysteresis_enabled(button_pins[i], 1);
    // gpio_set_slew_rate(button_pins[i], GPIO_SLEW_RATE_SLOW);
    // gpio_disable_pulls(button_pins[i]);
  }
}

// queue_t button_queue;
// typedef struct { int pin; } ButtonPress;
static void button_poll(void) {
  for (int i = 0; i < ARR_LEN(button_pins); i++) {
    ButtonState *bs = button_states + i;

    uint8_t state = gpio_get(button_pins[i]);
    if (state != bs->last_state) {
      bs->last_state = state;

      if (state) bs->last_up   = get_absolute_time();
      else       bs->last_down = get_absolute_time();
    }

    absolute_time_t when_up_cooldown = delayed_by_ms(bs->last_up  , 70);
    absolute_time_t light_when_start = delayed_by_ms(bs->last_down, 20);
    absolute_time_t light_when_stop  = delayed_by_ms(bs->last_down, 40);

    uint8_t on = absolute_time_diff_us(get_absolute_time(), light_when_start) < 0 &&
                 absolute_time_diff_us(get_absolute_time(), light_when_stop ) > 0 &&
                 absolute_time_diff_us(get_absolute_time(), when_up_cooldown) < 0  ;

    // if (on) dbg("BRUH");

    if (!on && !bs->edge) bs->edge = 1;
    if (on && bs->edge) {
      bs->edge = 0;

      // spade_call_press(button_pins[i]);

      // queue_add_blocking(&button_queue, &(ButtonPress) { .pin = button_pins[i] });
      multicore_fifo_push_blocking(button_pins[i]);

      //      if (button_pins[i] == 8) map_move(map_get_first('p'),  1,  0);
      // else if (button_pins[i] == 6) map_move(map_get_first('p'), -1,  0);
      // else if (button_pins[i] == 7) map_move(map_get_first('p'),  0,  1);
      // else if (button_pins[i] == 5) map_move(map_get_first('p'),  0, -1);
    }
  }
}

static void core1_entry(void) {
  button_init();

  while (1) {
    button_poll();
  }
}

int main() {
  stdio_init_all();

  // queue_init(&button_queue, sizeof(ButtonPress), 32);
  multicore_launch_core1(core1_entry);

  st7735_init();

  init(sprite_free_jerry_object); /* gosh i should namespace base engine */

  js_init();

  absolute_time_t last = get_absolute_time();
  while(1) {
    /* input handling */
    // ButtonPress press = {0};
    while (multicore_fifo_rvalid())
      spade_call_press(multicore_fifo_pop_blocking());

    /* setTimeout/setInterval impl */
    absolute_time_t now = get_absolute_time();
    int elapsed = us_to_ms(absolute_time_diff_us(last, now));
    last = now;
    spade_call_frame(elapsed);
    js_promises();

    /* upload new scripts */
    upl_stdin_read();
    if (upl_state.prog == UplProg_Done && upl_state.str) {
      puts(upl_state.str);
      init(sprite_free_jerry_object); /* gosh i should namespace base engine */
      js_init_with(upl_state.str, strlen(upl_state.str));

      free(upl_state.str);
      memset(&upl_state, 0, sizeof(upl_state));

      puts("done!");
    }

    /* render */
    uint16_t screen[160 * 128] = {0};
    render(screen);
    render_errorbuf(screen);
    st7735_fill(screen);
  }
  return 0;
}
