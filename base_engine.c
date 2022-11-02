#include <stdint.h>
#include "errorbuf.h"
#include "font.h"
#include "jerryscript.h"

#ifdef __wasm__

  #define WASM_EXPORT __attribute__((visibility("default")))
  
  extern void putchar(char c);
  extern void putint(int i);
  extern unsigned char __heap_base;
  #define PAGE_SIZE (1 << 16)

  typedef struct { uint8_t rgba[4]; } Color;

  #define color16(r, g, b) ((Color) { r, g, b, 255 })

#else

  #if SPADE_EMBEDDED
    #define FUCKED_COORDINATE_SYSTEM

    static uint16_t color16(uint8_t r, uint8_t b, uint8_t g) {
      r = (uint8_t)((float)((float)r / 255.0f) * 31.0f);
      g = (uint8_t)((float)((float)g / 255.0f) * 31.0f);
      b = (uint8_t)((float)((float)b / 255.0f) * 63.0f);

      // return ((r & 0xf8) << 8) + ((g & 0xfc) << 3) + (b >> 3);
      return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
    }

    typedef uint16_t Color;
  #else
    #define color16 MFB_RGB
    typedef uint32_t Color;
  #endif

  #define WASM_EXPORT static


#endif

#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

static float fabsf(float x) { return (x < 0) ? -x : x; }
static float signf(float f) {
  if (f < 0) return -1;
  if (f > 0) return 1;
  return 0;
}

#define TEXT_CHARS_MAX_X (20)
#define TEXT_CHARS_MAX_Y (16)

#define SPRITE_SIZE (16)
#define DOODLE_PANE_SIZE (SPRITE_SIZE*SPRITE_SIZE / 8)
typedef struct {
  /* doodles have 5 panes */
  uint8_t lit [DOODLE_PANE_SIZE];
  uint8_t rgb0[DOODLE_PANE_SIZE];
  uint8_t rgb1[DOODLE_PANE_SIZE];
  uint8_t rgb2[DOODLE_PANE_SIZE];
  uint8_t rgb3[DOODLE_PANE_SIZE];
} Doodle;

typedef struct Sprite Sprite;
struct Sprite {
  char kind;
  int8_t  dx, dy;
  uint16_t x,  y;
  uint16_t next; /* index of next sprite in list + 1 (0 null) */
  jerry_value_t object;
};
static void map_free(Sprite *s);
static Sprite *map_alloc(void);
static uint8_t map_active(Sprite *s, uint32_t generation);

typedef struct { Sprite *sprite; int x, y; uint8_t dirty; } MapIter;

#define PER_CHAR (255)

#define SCREEN_SIZE_X (160)
#define SCREEN_SIZE_Y (128)

typedef struct {
  Color palette[16];
  uint8_t lit[SCREEN_SIZE_Y * SCREEN_SIZE_X / 8];
  
  int scale;

  /* some SoA v. AoS shit goin on here man */
  int doodle_index_count;
  uint8_t legend_doodled[PER_CHAR]; /* PER_CHAR = because it's used for assigning indices */
  Doodle *legend;         /* tied to state->legend_size */
  Doodle *legend_resized; /* tied to state->legend_size */

} State_Render;

typedef struct {
  int width, height;

  void (*map_free_cb)(Sprite *);

  char  text_char [TEXT_CHARS_MAX_Y][TEXT_CHARS_MAX_X];
  Color text_color[TEXT_CHARS_MAX_Y][TEXT_CHARS_MAX_X];

  uint8_t char_to_index[PER_CHAR];

  uint8_t solid[PER_CHAR];
  uint16_t legend_size;
  uint8_t *push_table; /* tied to state->legend_size */

  size_t    sprite_pool_size;
  Sprite   *sprite_pool;
  uint8_t  *sprite_slot_active;
  uint32_t *sprite_slot_generation;

  uint16_t *map; /* indexes into sprite_pool + 1 (0 null) */

  int tile_size; /* how small tiles have to be to fit map on screen */
  char background_sprite;

  State_Render *render;

  /* this is honestly probably heap abuse and most kaluma uses of it should
     probably use stack memory instead. It started out as a way to pass
     strings across the WASM <-> JS barrier. */
  char temp_str_mem[(1 << 12)];
} State;
static State *state = 0;

static uint8_t char_to_palette_index(char c) {
  switch (c) {
    case '0': return  0;
    case 'L': return  1;
    case '1': return  2;
    case '2': return  3;
    case '3': return  4;
    case 'C': return  5;
    case '7': return  6;
    case '5': return  7;
    case '6': return  8;
    case 'F': return  9;
    case '4': return 10;
    case 'D': return 11;
    case '8': return 12;
    case 'H': return 13;
    case '9': return 14;
    case '.': return 15;
    default: return 0; /* lmfao (anything to quiet the voices.)
                                (i meant clang warnings. same thing) */
  }
}

/* almost makes ya wish for generic data structures dont it :shushing_face:

   this was implemented to cut down on RAM usage before I discovered you can
   control how much RAM gets handed to JS in targets/rp2/target.cmake
   */
static void render_lit_write(int x, int y) {
  int i = x*SCREEN_SIZE_Y + y;
  state->render->lit[i/8] |= 1 << (i % 8);
}
static uint8_t render_lit_read(int x, int y) {
  int i = x*SCREEN_SIZE_Y + y;
  int q = 1 << (i % 8);
  return !!(state->render->lit[i/8] & q);
}

static void doodle_pane_write(uint8_t *pane, int x, int y) {
  int i = y*SPRITE_SIZE + x;
  pane[i/8] |= 1 << (i % 8);
}
static uint8_t doodle_pane_read(uint8_t *pane, int x, int y) {
  int i = y*SPRITE_SIZE + x;
  int q = 1 << (i % 8);
  return !!(pane[i/8] & q);
}

static void push_table_write(char x_char, char y_char) {
  int x = state->char_to_index[(int) x_char];
  int y = state->char_to_index[(int) y_char];

  int i = y*state->legend_size + x;
  state->push_table[i/8] |= 1 << (i % 8);
}
static uint8_t push_table_read(char x_char, char y_char) {
  int x = state->char_to_index[(int) x_char];
  int y = state->char_to_index[(int) y_char];

  int i = y*state->legend_size + x;
  int q = 1 << (i % 8);
  return !!(state->push_table[i/8] & q);
}

static Sprite *get_sprite(uint16_t i) {
  if (i == 0) return 0;
  return state->sprite_pool + i - 1;
}

static void sprite_pool_realloc(int size) {
  size_t start_size = state->sprite_pool_size;

  dbg("bouta call a bunch of realloc!");
#define realloc_n(arr, os, ns) jerry_realloc((arr),                 \
                                             sizeof((arr)[0]) * os, \
                                             sizeof((arr)[0]) * ns);
  state->sprite_slot_active     = realloc_n(state->sprite_slot_active    , start_size, size);
  dbg("you get a realloc ...");
  state->sprite_slot_generation = realloc_n(state->sprite_slot_generation, start_size, size);
  dbg("and you get a realloc ...");
  state->sprite_pool            = realloc_n(state->sprite_pool           , start_size, size);
  dbg("and you!");
#undef realloc_n

  int worked = state->sprite_slot_active     &&
               state->sprite_slot_generation &&
               state->sprite_pool             ;
  dbg("let's see if it worked ...");
  dbgf("state->sprite_slot_active     = %lu\n", state->sprite_slot_active    );
  dbgf("state->sprite_slot_generation = %lu\n", state->sprite_slot_generation);
  dbgf("state->sprite_pool            = %lu\n", state->sprite_pool           );

  if (!worked) {
    snprintf(
      errorbuf, sizeof(errorbuf),
      "%lu sprites (%lu bytes!) is too many to fit on the pico!",
      state->sprite_pool_size,
      state->sprite_pool_size * sizeof(Sprite)
    );
    fatal_error();
  }

  /* great, we were able to allocate enough memory */
  state->sprite_pool_size = size;
}

static void legend_doodles_realloc(int size) {
  size_t start_size = state->legend_size;

  size_t push_table_bytes_old = start_size * start_size / 8;
  size_t push_table_bytes_new =       size *       size / 8;
  state->push_table = jerry_realloc(state->push_table,
                                    push_table_bytes_old,
                                    push_table_bytes_new);

  State_Render *sr = state->render;
#define realloc_n(arr, os, ns) jerry_realloc((arr),                 \
                                             sizeof((arr)[0]) * os, \
                                             sizeof((arr)[0]) * ns);
  sr->legend         = realloc_n(sr->legend        , start_size, size);
  sr->legend_resized = realloc_n(sr->legend_resized, start_size, size);
#undef realloc_n

  int worked = state->push_table  &&
               sr->legend         &&
               sr->legend_resized  ;

  dbgf("state->push_table     = %lu\n", state->push_table  );
  dbgf("   sr->legend         = %lu\n", sr->legend         );
  dbgf("   sr->legend_resized = %lu\n", sr->legend_resized );

  if (!worked) {
    snprintf(
      errorbuf, sizeof(errorbuf),
      "%d drawings (%lu bytes!) is too many to fit on the pico!",
      state->legend_size,
      state->legend_size * sizeof(Doodle) * 2 + push_table_bytes_new
    );
    fatal_error();
  }

  /* great, we were able to allocate enough memory */
  state->legend_size = size;
}

WASM_EXPORT void text_add(char *str, char palette_index, int x, int y) {
  for (; *str; str++, x++)
    state->text_char [y][x] = *str,
    state->text_color[y][x] = state->render->palette[char_to_palette_index(palette_index)];
}

WASM_EXPORT void text_clear(void) {
  __builtin_memset(state->text_char , 0, sizeof(state->text_char ));
  __builtin_memset(state->text_color, 0, sizeof(state->text_color));
}

WASM_EXPORT void init(void (*map_free_cb)(Sprite *)) {
#ifdef __wasm__
  int mem_needed = sizeof(State)/PAGE_SIZE;

  int delta = mem_needed - __builtin_wasm_memory_size(0) + 2;
  if (delta > 0) __builtin_wasm_memory_grow(0, delta);
  state = (void *)&__heap_base;

#else
  static State _state = {0};
  state = &_state;

  __builtin_memset(state, 0, sizeof(State));

  static State_Render _state_render = {0};
  state->render = &_state_render;
#endif

  state->map_free_cb = map_free_cb;

  /* -- error handling for when state is dynamically allocated -- */ 
  // if (state->render == 0) {
  //   state->render = malloc(sizeof(State_Render));
  //   printf("sizeof(State_Render) = %d, addr: %d\n", sizeof(State_Render), (unsigned int)state->render);
  // }
  // if (state->render == 0) yell("couldn't alloc state");

  __builtin_memset(state->render, 0, sizeof(State_Render));

  sprite_pool_realloc(512);
  legend_doodles_realloc(50);

  // Grey
  state->render->palette[char_to_palette_index('0')] = color16(  0,   0,   0);
  state->render->palette[char_to_palette_index('L')] = color16( 73,  80,  87);
  state->render->palette[char_to_palette_index('1')] = color16(145, 151, 156);
  state->render->palette[char_to_palette_index('2')] = color16(248, 249, 250);
  state->render->palette[char_to_palette_index('3')] = color16(235,  44,  71);
  state->render->palette[char_to_palette_index('C')] = color16(139,  65,  46);
  state->render->palette[char_to_palette_index('7')] = color16( 25, 177, 248);
  state->render->palette[char_to_palette_index('5')] = color16( 19,  21, 224);
  state->render->palette[char_to_palette_index('6')] = color16(254, 230,  16);
  state->render->palette[char_to_palette_index('F')] = color16(149, 140,  50);
  state->render->palette[char_to_palette_index('4')] = color16( 45, 225,  62);
  state->render->palette[char_to_palette_index('D')] = color16( 29, 148,  16);
  state->render->palette[char_to_palette_index('8')] = color16(245, 109, 187);
  state->render->palette[char_to_palette_index('H')] = color16(170,  58, 197);
  state->render->palette[char_to_palette_index('9')] = color16(245, 113,  23);
  state->render->palette[char_to_palette_index('.')] = color16(  0,   0,   0);
}

WASM_EXPORT char *temp_str_mem(void) {
  __builtin_memset(&state->temp_str_mem, 0, sizeof(state->temp_str_mem));
  return state->temp_str_mem;
}

static int render_xy_to_idx(int x, int y) {
#ifdef FUCKED_COORDINATE_SYSTEM
  return (SCREEN_SIZE_X - x - 1)*SCREEN_SIZE_Y + y;
#else
  return SCREEN_SIZE_X*y + x;
#endif
}

/* call this when the map changes size, or when the legend changes */
static void render_resize_legend(void) {
  __builtin_memset(state->render->legend_resized, 0, sizeof(Doodle) * state->legend_size);

  /* how big do our tiles need to be to fit them all snugly on screen? */
  float min_tile_x = SCREEN_SIZE_X / state->width;
  float min_tile_y = SCREEN_SIZE_Y / state->height;
  state->tile_size = (min_tile_x < min_tile_y) ? min_tile_x : min_tile_y;
  if (state->tile_size > 16)
    state->tile_size = 16;

  for (int c = 0; c < PER_CHAR; c++) {
    if (!state->render->legend_doodled[c]) continue;
    int i = state->char_to_index[c];

    Doodle *rd = state->render->legend_resized + i;
    Doodle *od = state->render->legend + i;

    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        int rx = (float) x / 16.0f * state->tile_size;
        int ry = (float) y / 16.0f * state->tile_size;

        if (!doodle_pane_read(od->lit, x, y)) continue;
                                              doodle_pane_write(rd->lit , rx, ry);
        if (doodle_pane_read(od->rgb0, x, y)) doodle_pane_write(rd->rgb0, rx, ry);
        if (doodle_pane_read(od->rgb1, x, y)) doodle_pane_write(rd->rgb1, rx, ry);
        if (doodle_pane_read(od->rgb2, x, y)) doodle_pane_write(rd->rgb2, rx, ry);
        if (doodle_pane_read(od->rgb3, x, y)) doodle_pane_write(rd->rgb3, rx, ry);
        // if (doodle_pane_read(od->lit , x, y)) doodle_pane_write(rd->lit , rx, ry);
      }
  }
}

static void render_blit_sprite(Color *screen, int sx, int sy, char kind) {
  int scale = state->render->scale;
  Doodle *d = state->render->legend_resized + state->char_to_index[(int)kind];

  for (int x = 0; x < state->tile_size; x++)
    for (int y = 0; y < state->tile_size; y++) {

      if (!doodle_pane_read(d->lit, x, y)) continue;
      for (  int ox = 0; ox < scale; ox++)
        for (int oy = 0; oy < scale; oy++) {
          int px = ox + sx + scale*x;
          int py = oy + sy + scale*y;
          if (render_lit_read(px, py)) continue;

          render_lit_write(px, py);

#ifdef FUCKED_COORDINATE_SYSTEM
          int i = (ox + sx + scale*(state->tile_size - 1 - x)) * SCREEN_SIZE_Y + py;
#else
          int i = SCREEN_SIZE_X*py + px;
#endif
          screen[i] = state->render->palette[
            (doodle_pane_read(d->rgb0, x, y) << 0) |
            (doodle_pane_read(d->rgb1, x, y) << 1) |
            (doodle_pane_read(d->rgb2, x, y) << 2) |
            (doodle_pane_read(d->rgb3, x, y) << 3)
          ];
        }
    }
}

static void render_char(Color *screen, char c, Color color, int sx, int sy) {
  for (int y = 0; y < 8; y++) {
    uint8_t bits = font_pixels[c*8 + y];
    for (int x = 0; x < 8; x++)
      if ((bits >> (7-x)) & 1) {
        screen[render_xy_to_idx(sx+x, sy+y)] = color;
      }
  }
}

WASM_EXPORT void render_set_background(char kind) {
  state->background_sprite = kind;
}

WASM_EXPORT uint8_t map_get_grid(MapIter *m);
WASM_EXPORT void render(Color *screen) {
  __builtin_memset(&state->render->lit, 0, sizeof(state->render->lit));

  if (!state->width)  goto RENDER_TEXT;
  if (!state->height) goto RENDER_TEXT;

  int scale;
  {
    int scale_x = SCREEN_SIZE_X/(state->width*16);
    int scale_y = SCREEN_SIZE_Y/(state->height*16);

    scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;

    state->render->scale = scale;
  }
  int size = state->tile_size*scale;

  int pixel_width = state->width*size;
  int pixel_height = state->height*size;

  int ox = (SCREEN_SIZE_X - pixel_width)/2;
  int oy = (SCREEN_SIZE_Y - pixel_height)/2;


  // dbg("render: clear to white");
  for (int y = oy; y < oy+pixel_height; y++)
    for (int x = ox; x < ox+pixel_width; x++) {
      screen[render_xy_to_idx(x, y)] = color16(255, 255, 255);
    }

  // dbg("render: grid");
  MapIter m = {0};
  while (map_get_grid(&m))
    render_blit_sprite(screen,
#ifdef FUCKED_COORDINATE_SYSTEM
                       ox + size*(state->width - 1 - m.sprite->x),
#else
                       ox + size*m.sprite->x,
#endif
                       oy + size*m.sprite->y,
                       m.sprite->kind);

  // dbg("render: bg");
  if (state->background_sprite)
    for (int y = 0; y < state->height; y++)
      for (int x = 0; x < state->width; x++)
        render_blit_sprite(screen,
                           ox + size*x,
                           oy + size*y,
                           state->background_sprite);

  RENDER_TEXT:
  // dbg("render: text");
  for (int y = 0; y < TEXT_CHARS_MAX_Y; y++)
    for (int x = 0; x < TEXT_CHARS_MAX_X; x++) {
      char c = state->text_char[y][x];
      if (c) render_char(screen, c, state->text_color[y][x], x*8, y*8);
    }

  // dbg("render: done");
}

static Sprite *map_alloc(void) {
  for (int i = 0; i < state->sprite_pool_size; i++) {
    if (state->sprite_slot_active[i] == 0) {
      state->sprite_slot_active[i] = 1;
      return state->sprite_pool + i;
    }
  }

  sprite_pool_realloc(state->sprite_pool_size * 1.2f);
  return map_alloc();
}
static void map_free(Sprite *s) {
  if (state->map_free_cb) state->map_free_cb(s);

  memset(s, 0, sizeof(Sprite));
  size_t i = s - state->sprite_pool;
  state->sprite_slot_active    [i] = 0;
  state->sprite_slot_generation[i]++;
}
static uint8_t map_active(Sprite *s, uint32_t generation) {
  if (s == NULL) return 0;
  size_t i = s - state->sprite_pool;
  return state->sprite_slot_generation[i] == generation;
}
WASM_EXPORT uint32_t sprite_generation(Sprite *s) {
  size_t i = s - state->sprite_pool;
  return state->sprite_slot_generation[i];
}

/* removes the canonical reference to this sprite from the spatial grid.
   it is your responsibility to subsequently free the sprite. */
static void map_pluck(Sprite *s) {
  Sprite *top = get_sprite(state->map[s->x + s->y * state->width]);
  // assert(top != 0);

  if (top == s) {
    state->map[s->x + s->y * state->width] = s->next;
    return;
  }

  for (Sprite *t = top; t->next; t = get_sprite(t->next)) {
    if (get_sprite(t->next) == s) {
      t->next = s->next;
      return;
    }
  }

  state->map[s->x + s->y * state->width] = 0;
}

/* inserts pointer to sprite into the spritestack at this x and y,
 * such that rendering z-order is preserved
 * (as expressed in order of legend_doodle_set calls)
 * 
 * see map_plop about caller's responsibility */
static void map_plop(Sprite *sprite) {
  Sprite *top = get_sprite(state->map[sprite->x + sprite->y * state->width]);

  /* we want the sprite with the lowest z-order on the top. */

  #define Z_ORDER(sprite) (state->char_to_index[(int)(sprite)->kind])
  if (top == 0 || Z_ORDER(top) >= Z_ORDER(sprite)) {
    sprite->next = state->map[sprite->x + sprite->y * state->width];
    state->map[sprite->x + sprite->y * state->width] = sprite - state->sprite_pool + 1;
    // dbg("top's me, early ret");
    return;
  }

  Sprite *insert_after = top;
  while (insert_after->next && Z_ORDER(get_sprite(insert_after->next)) < Z_ORDER(sprite))
    insert_after = get_sprite(insert_after->next);
  #undef Z_ORDER

  sprite->next = insert_after->next;
  insert_after->next = sprite - state->sprite_pool + 1;
}


WASM_EXPORT Sprite *map_add(int x, int y, char kind) {
  if (x < 0 || x >= state->width ) return 0;
  if (y < 0 || y >= state->height) return 0;

  Sprite *s = map_alloc();
  if (s == 0) return 0;
  
  *s = (Sprite) { .x = x, .y = y, .kind = kind };
  // dbg("assigned to that mf");
  map_plop(s);
  // dbg("stuck 'em on map, returning now");
  return s;
}

WASM_EXPORT void map_set(char *str) {
  dbg("wormed ya way down into base_engine.c");

  if (state->map != NULL)
    jerry_heap_free(state->map,
                    state->width * state->height * sizeof(Sprite*));
  for (int i = 0; i < state->sprite_pool_size; i++)
    map_free(state->sprite_pool + i);
  dbg("freed some sprites, maybe a map");

  int tx = 0, ty = 0;
  char *str_dup = str;
  do {
    switch (*str_dup) {
      case  ' ': continue;
      case '\0':               break;
      case '\n': ty++, tx = 0; break;
      default: tx++;           break;
    }
  } while (*str_dup++);
  state->width = tx;
  state->height = ty+1;
  dbg("parsed, found dims");

  state->map = jerry_calloc(state->width * state->height, sizeof(Sprite*));
  if (state->map == NULL) {
    yell("AAAAAAAAA (map too big)");
    snprintf(errorbuf, sizeof(errorbuf), "map too big to fit in memory (%dx%d)", state->width, state->height);
    fatal_error();
  }

  dbg("so we got us a map, time to alloc sprites");

  tx = 0, ty = 0;
  do {
    switch (*str) {
      case  ' ': continue;
      case '\n': ty++, tx = 0; break;
      case  '.': tx++;         break;
      case '\0':               break;
      default: {
        Sprite *s = map_alloc();
        dbg("alloced us a sprite");

        *s = (Sprite) { .x = tx, .y = ty, .kind = *str };
        dbg("filled in some fields");

        state->map[tx + ty * state->width] = s - state->sprite_pool + 1;
        dbg("put 'em on the map");

        tx++;
      } break;
    }
  } while (*str++);

  dbg("alrighty, lemme resize a legend");

  render_resize_legend();
}

WASM_EXPORT int map_width(void) { return state->width; }
WASM_EXPORT int map_height(void) { return state->height; }

WASM_EXPORT Sprite *map_get_first(char kind) {
  for (int y = 0; y < state->height; y++)
    for (int x = 0; x < state->width; x++) {
      Sprite *top = get_sprite(state->map[x + y * state->width]);
      for (; top; top = get_sprite(top->next))
        if (top->kind == kind)
          return top;
    }
  return 0;
}

WASM_EXPORT uint8_t map_get_grid(MapIter *m) {
  if (m->sprite && m->sprite->next) {
    m->sprite = get_sprite(m->sprite->next);
    return 1;
  }

  while (1) {
    if (!m->dirty)
      m->dirty = 1;
    else {
      m->x++;
      if (m->x >= state->width) {
        m->x = 0;
        m->y++;
        if (m->y >= state->height) return 0;
      }
    }

    if (state->map[m->x + m->y * state->width]) {
      m->sprite = get_sprite(state->map[m->x + m->y * state->width]);
      return 1;
    }
  }
}

/* you could easily do this in JS, but I suspect there is a
 * great perf benefit to avoiding all of the calls back and forth
 */
WASM_EXPORT uint8_t map_get_all(MapIter *m, char kind) {
  while (map_get_grid(m))
    if (m->sprite->kind == kind)
      return 1;
  return 0;
}

WASM_EXPORT uint8_t map_tiles_with(MapIter *m, char *kinds) {
  char kinds_needed[255] = {0};
  int kinds_len = 0;
  for (; *kinds; kinds++) {
    int c = (int)*kinds;
    
    /* filters out duplicates! */
    if (kinds_needed[c] != 0) continue;

    kinds_len++;
    kinds_needed[c] = 1;
  }

  while (1) {
    if (!m->dirty)
      m->dirty = 1;
    else {
      m->x++;
      if (m->x >= state->width) {
        m->x = 0;
        m->y++;
        if (m->y >= state->height) return 0;
      }
    }

    if (state->map[m->x + m->y * state->width]) {
      uint8_t kinds_seen[255] = {0};
      int kinds_found = 0;

      for (Sprite *s = get_sprite(state->map[m->x + m->y * state->width]);
           s;
           s = get_sprite(s->next)
      ) {
        kinds_found += kinds_needed[(int)s->kind] && !kinds_seen[(int)s->kind];
        kinds_seen[(int)s->kind] = 1;
      }

      if (kinds_found == kinds_len) {
        m->sprite = get_sprite(state->map[m->x + m->y * state->width]);
        return 1;
      }
    }
  }
}

WASM_EXPORT void map_remove(Sprite *s) {
  map_pluck(s);
  map_free(s);
}

/* removes all of the sprites at a given location */
WASM_EXPORT void map_drill(int x, int y) {
  Sprite *top = get_sprite(state->map[x + y * state->width]);
  for (; top; top = get_sprite(top->next)) {
    map_free(top);
  }
  state->map[x + y * state->width] = 0;
}


/* move a sprite by one unit along the specified axis
   returns how much it was moved on that axis (may be 0 if path obstructed) */
static int _map_move(Sprite *s, int big_dx, int big_dy) {
  int dx = signf(big_dx);
  int dy = signf(big_dy);

  /* expected input: x and y aren't both 0, either x or y is non-zero (not both) */
  if (dx == 0 && dy == 0) return 0;

  int prog = 0;
  int goal = (fabsf(big_dx) > fabsf(big_dy)) ? big_dx : big_dy;

  while (prog != goal) {
    int x = s->x+dx;
    int y = s->y+dy;

    /* no moving off of the map! */
    if (x < 0) return prog;
    if (y < 0) return prog;
    if (x >= state->width) return prog;
    if (y >= state->height) return prog;

    if (state->solid[(int)s->kind]) {
      /* no moving into a solid! */
      Sprite *n = get_sprite(state->map[x + y * state->width]);

      for (; n; n = get_sprite(n->next))
        if (state->solid[(int)n->kind]) {
          /* unless you can push them out of the way ig */
          if (push_table_read(s->kind, n->kind)) {
            if (_map_move(n, dx, dy) == 0)
              return prog;
          }
          else
            return prog;
        }
    }

    map_pluck(s);
    s->x += dx;
    s->y += dy;
    map_plop(s);
    prog += (fabsf(dx) > fabsf(dy)) ? dx : dy;
  }

  return prog;
}

WASM_EXPORT void map_move(Sprite *s, int big_dx, int big_dy) {
  int moved = _map_move(s, big_dx, big_dy);
  if (big_dx != 0) s->dx = moved;
  else             s->dy = moved;
}

#ifdef __wasm__
WASM_EXPORT int sprite_get_x(Sprite *s) { return s->x; }
WASM_EXPORT int sprite_get_y(Sprite *s) { return s->y; }
WASM_EXPORT int sprite_get_dx(Sprite *s) { return s->dx; }
WASM_EXPORT int sprite_get_dy(Sprite *s) { return s->dy; }
WASM_EXPORT char sprite_get_kind(Sprite *s) { return s->kind; }
WASM_EXPORT void sprite_set_kind(Sprite *s, char kind) { s->kind = kind; }

WASM_EXPORT void MapIter_position(MapIter *m, int x, int y) { m->x = x; m->y = y; }

#endif

WASM_EXPORT void map_clear_deltas(void) {
  for (int y = 0; y < state->height; y++)
    for (int x = 0; x < state->width; x++) {
      Sprite *top = get_sprite(state->map[x + y * state->width]);

      for (; top; top = get_sprite(top->next))
        top->dx = top->dy = 0;
    }
}

WASM_EXPORT void solids_push(char c) {
  state->solid[(int)c] = 1;
}
WASM_EXPORT void solids_clear(void) {
  __builtin_memset(&state->solid, 0, sizeof(state->solid));
}

WASM_EXPORT void legend_doodle_set(char kind, char *str) {

  int index = state->char_to_index[(int)kind];

  /* we don't want to increment if index 0 has already been assigned and this is it */
  if (index == 0 && !state->render->legend_doodled[(int)kind]) {
    if (state->render->doodle_index_count >= state->legend_size)
      legend_doodles_realloc(state->legend_size * 1.2f);
    index = state->render->doodle_index_count++;
  }
  state->char_to_index[(int)kind] = index;

  state->render->legend_doodled[(int)kind] = 1;
  Doodle *d = state->render->legend + index;
  dbgf("bouta write to %lu + %d\n", state->render->legend, index);

  int px = 0, py = 0;
  do {
    switch (*str) {
      case '\n': py++, px = 0; break;
      case  '.': px++;         break;
      case '\0':               break;
      default: {
        int pi = char_to_palette_index(*str);
        if (pi & (1 << 0)) doodle_pane_write(d->rgb0, px, py);
        if (pi & (1 << 1)) doodle_pane_write(d->rgb1, px, py);
        if (pi & (1 << 2)) doodle_pane_write(d->rgb2, px, py);
        if (pi & (1 << 3)) doodle_pane_write(d->rgb3, px, py);
        doodle_pane_write(d->lit, px, py);
        px++;
      } break;
    }
  } while (*str++);
}
WASM_EXPORT void legend_clear(void) {
  state->render->doodle_index_count = 0;
  __builtin_memset(state->render->legend, 0, sizeof(Doodle) * state->legend_size);
  __builtin_memset(state->render->legend_resized, 0, sizeof(Doodle) * state->legend_size);
  __builtin_memset(&state->render->legend_doodled, 0, sizeof(state->render->legend_doodled));
  __builtin_memset(&state->char_to_index, 0, sizeof(state->char_to_index));
}
WASM_EXPORT void legend_prepare(void) {
  if (state->width && state->height)
    render_resize_legend();
}

WASM_EXPORT void push_table_set(char pusher, char pushes) {
  push_table_write(pusher, pushes);
}
WASM_EXPORT void push_table_clear(void) {
  __builtin_memset(state->push_table, 0, state->legend_size*state->legend_size/8);
}

void render_errorbuf(Color *screen) {
  int y = 0;
  int x = 0;
  for (int i = 0; i < sizeof(errorbuf); i++) {
    if (errorbuf[i] == '\0') break;
    if (errorbuf[i] == '\n' || x >= (SCREEN_SIZE_X / 8)) {
      y++;
      x = 0;
      if (errorbuf[i] == '\n') continue;
    }
    if (y >= (SCREEN_SIZE_Y / 8)) break;
    render_char(screen, errorbuf[i], color16(255, 0, 0), x*8, y*8);
    x++;
  }
}

#if 0
void text_add(char *str, int x, int y, uint32_t color);
Sprite *sprite_add(int x, int y, char kind);
Sprite *sprite_next(Sprite *s);
int sprite_get_x(Sprite *s);
int sprite_get_y(Sprite *s);
char sprite_get_kind(Sprite *s);
void sprite_set_x(Sprite *s, int x);
void sprite_set_y(Sprite *s, int y);
void sprite_set_kind(Sprite *s, char kind);

void spritestack_clear(int x, int y);

void solids_push(char c);
void solids_clear();

// setPushables, 

setBackground

map: _makeTag(text => text),
bitmap: _makeTag(text => text),
tune: _makeTag(text => text),
#endif
