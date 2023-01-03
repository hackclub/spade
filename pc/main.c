#include <MiniFB.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// #define SPADE_AUTOMATED

#ifdef SPADE_AUTOMATED
  #define puts(...) ;
  #define printf(...) ;
#endif

#ifdef SPADE_AUDIO
  #include "audio.h"
#endif

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

#ifdef SPADE_AUTOMATED
static void print_map(void) {
  /* find max on Z axis */
  int z_size = 0;
  {
    for (int x = 0; x < state->width; x++)
      for (int y = 0; y < state->height; y++) {
        Sprite *s = get_sprite(state->map[(y * state->width) + x]);
        int sprite_stack_len = 0;
        while (s) {
          sprite_stack_len++;
          s = get_sprite(s->next);
        }

        if (sprite_stack_len > z_size)
          z_size = sprite_stack_len;
      }
  }

  const int stride = z_size*(state->width+1);

  /* +1 is for newlines */
  int mapstr_len = stride * state->height;
  char *mapstr = malloc(1 + mapstr_len);
  mapstr[mapstr_len] = 0;
  memset(mapstr, '.', mapstr_len);

  /* insert newlines */
  int w = state->width, h = state->height;
  for (int y = 0; y < h; y++) {
    mapstr[(y+1)*stride - 1] = '\n';
    for (int z = 1; z < z_size; z++)
      mapstr[y*stride + z*(state->width+1) - 1] = '|';
  }

  for (int z = 0; z < z_size; z++) {
    for (int x = 0; x < state->width; x++)
      for (int y = 0; y < state->height; y++) {
        int str_i = stride*y + (z*(state->width+1)) + x;
        int map_i = (state->width+0)*y + x;
        Sprite *s = get_sprite(state->map[map_i]);

        int sprite_z = 0;
        while (s) sprite_z++, s = get_sprite(s->next);

        s = get_sprite(state->map[map_i]);
        while(s) {
          sprite_z--;
          if (sprite_z == z) {
            mapstr[str_i] = s->kind;
            break;
          }
          s = get_sprite(s->next);
        }
      }
  }

#undef puts
  puts(mapstr);
#define puts(...) ;
  free(mapstr);
  fflush(stdout);
}

static void simulated_keyboard(void) {
  char key = getchar();
       if (key == 'w') spade_call_press( 5); // map_move(map_get_first('p'),  0, -1);
  else if (key == 's') spade_call_press( 7); // map_move(map_get_first('p'),  0,  1);
  else if (key == 'a') spade_call_press( 6); // map_move(map_get_first('p'),  1,  0);
  else if (key == 'd') spade_call_press( 8); // map_move(map_get_first('p'), -1,  0);
  else if (key == 'i') spade_call_press(12); // map_move(map_get_first('p'),  0, -1);
  else if (key == 'k') spade_call_press(14); // map_move(map_get_first('p'),  0,  1);
  else if (key == 'j') spade_call_press(13); // map_move(map_get_first('p'),  1,  0);
  else if (key == 'l') spade_call_press(15); // map_move(map_get_first('p'), -1,  0);
  else return;

  print_map();
}
#else
static void keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
  (void) window;
  if (!isPressed) return;

  if (key == KB_KEY_ESCAPE
#ifdef __APPLE__
      || (key == KB_KEY_Q && (mod & KB_MOD_SUPER) != 0)
#endif
     ) {
    mfb_close(window);
  }

  if (key == KB_KEY_W) spade_call_press( 5); // map_move(map_get_first('p'),  0, -1);
  if (key == KB_KEY_S) spade_call_press( 7); // map_move(map_get_first('p'),  0,  1);
  if (key == KB_KEY_A) spade_call_press( 6); // map_move(map_get_first('p'),  1,  0);
  if (key == KB_KEY_D) spade_call_press( 8); // map_move(map_get_first('p'), -1,  0);
  if (key == KB_KEY_I) spade_call_press(12); // map_move(map_get_first('p'),  0, -1);
  if (key == KB_KEY_K) spade_call_press(14); // map_move(map_get_first('p'),  0,  1);
  if (key == KB_KEY_J) spade_call_press(13); // map_move(map_get_first('p'),  1,  0);
  if (key == KB_KEY_L) spade_call_press(15); // map_move(map_get_first('p'), -1,  0);
}
#endif

int peak_bitmap_count = 0;
int peak_sprite_count = 0;
static void render_char(Color *screen, char c, Color color, int sx, int sy) {
  for (int y = 0; y < 8; y++) {
    uint8_t bits = font_pixels[c*8 + y];
    for (int x = 0; x < 8; x++)
      if ((bits >> (7-x)) & 1) {
        screen[(sy+y)*160 + sx+x] = color;
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

static void js_init(char *file, int file_size) {
  const jerry_char_t engine[] = 
#include "engine.js.cstring"
  ;
  char *combined = calloc(sizeof(engine) - 1 + file_size, 1);
  strcpy(combined, engine);
  strcpy(combined + sizeof(engine) - 1, file);

  const jerry_length_t combined_size = sizeof (engine) - 1 + file_size;
  js_run(combined, combined_size);
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

/* free that shit when u done */
char *read_in_script(char *path, int *size) {
  FILE *script = fopen(path, "r");
  if (script == NULL) perror("couldn't open file arg"), abort();

  fseek(script, 0, SEEK_END);
  int file_size = ftell(script);
  rewind(script);

  char *chars = calloc(file_size, 1);
  if (fread(chars, file_size, 1, script) != 1)
    perror("couldn't read chars"), abort();
  if (size) *size = file_size;
  return chars;
}

int main(int argc, char *argv[])  {
  struct mfb_window *window = mfb_open_ex("spade", SPADE_WIN_SIZE_X * 2, SPADE_WIN_SIZE_Y * 2, 0);
  if (!window) return 1;

  jerry_init (JERRY_INIT_MEM_STATS);
  init(sprite_free_jerry_object); /* god i REALLY need to namespace baseengine */

  /* first arg = path to js code to run */
  {
    int script_len = 0;
    char *script = read_in_script(argv[1], &script_len);
    js_init(script, script_len);
    free(script);
  }
#ifdef SPADE_AUTOMATED
  print_map();
  /* we so cool we take input from stdin now */
#else
  mfb_set_keyboard_callback(window, keyboard);
#endif

  Color screen[SPADE_WIN_SIZE_X * SPADE_WIN_SIZE_Y] = {0};

  #ifdef SPADE_AUDIO
    piano_init((PianoOpts) {
      .song_free = piano_jerry_song_free,
      .song_chars = piano_jerry_song_chars,
    });
    audio_init();
  #endif

  struct mfb_timer *lastframe = mfb_timer_create();
  mfb_timer_now(lastframe);
  do {
    /* setInterval/Timeout impl */
    js_promises();
    spade_call_frame(1000.0f * mfb_timer_delta(lastframe));
    mfb_timer_now(lastframe);

    /* audio */
    #ifdef SPADE_AUDIO
      audio_try_push_samples();
    #endif

    /* render */
    memset(screen, 0, sizeof(screen));
    render_errorbuf();
    write_pixel_screen = screen;
    render(write_pixel);
    render_stats(screen);

#ifdef SPADE_AUTOMATED
    simulated_keyboard();
#endif

    /* windowing */
    uint8_t ok = STATE_OK == mfb_update_ex(window, screen, SPADE_WIN_SIZE_X, SPADE_WIN_SIZE_Y);
    if (!ok) {
      window = 0x0;
      break;
    }
  } while(mfb_wait_sync(window));

  return 0;
}
