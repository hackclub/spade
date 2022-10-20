/* Copyright (c) 2017 Pico
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "jerryscript.h"
#include "jerryxx.h"
#include "native_magic_strings.h"

static char jerry_value_to_char(jerry_value_t val) {
  jerry_char_t tmp[2] = {0};
  jerry_size_t nbytes = jerry_string_to_buffer(val, JERRY_ENCODING_UTF8, tmp, 1);
  if (nbytes == 0) {

    yell("uh non-char given as char input!");
    // jerryxx_print_value(val);
    return '.';
  }
  return tmp[0];
}

JERRYXX_FUN(setMap) {
  
  dbg("module_native::setMap");
  JERRYXX_CHECK_ARG(0, "str");

  jerry_char_t *tmp = (jerry_char_t *)temp_str_mem();
  jerry_size_t nbytes = jerry_string_to_buffer(
    JERRYXX_GET_ARG(0),
    JERRY_ENCODING_UTF8,
    tmp,
    sizeof(state->temp_str_mem) - 1
  );
  tmp[nbytes] = '\0'; 

  map_set((char *)tmp);

  return jerry_undefined();
}

JERRYXX_FUN(setBackground) {
  
  dbg("module_native::setBackground");
  render_set_background(jerry_value_to_char(JERRYXX_GET_ARG(0)));
  return jerry_undefined();
}

JERRYXX_FUN(native_legend_doodle_set_fn) {
  
  dbg("module_native::native_legend_doodle_set_fn");
  JERRYXX_CHECK_ARG(0, "char");
  JERRYXX_CHECK_ARG(1, "str");

  jerry_char_t *tmp = (jerry_char_t *)temp_str_mem();
  jerry_size_t nbytes = jerry_string_to_buffer(
    JERRYXX_GET_ARG(1),
    JERRY_ENCODING_UTF8,
    tmp,
    sizeof(state->temp_str_mem) - 1
  );
  tmp[nbytes] = '\0'; 

  legend_doodle_set(jerry_value_to_char(JERRYXX_GET_ARG(0)), (char *)tmp);

  return jerry_undefined();
}

JERRYXX_FUN(native_press_cb_fn) {
  if (spade_state.press_cb) jerry_value_free(spade_state.press_cb);

  spade_state.press_cb = jerry_value_copy(JERRYXX_GET_ARG(0));
  return jerry_undefined(); 
}

JERRYXX_FUN(native_frame_cb_fn) {
  if (spade_state.frame_cb) jerry_value_free(spade_state.frame_cb);

  spade_state.frame_cb = jerry_value_copy(JERRYXX_GET_ARG(0));
  return jerry_undefined(); 
}

JERRYXX_FUN(native_legend_clear_fn) { 
  dbg("module_native::native_legend_clear_fn");
  legend_clear(); return jerry_undefined(); }
JERRYXX_FUN(native_legend_prepare_fn) { 
  dbg("module_native::native_legend_prepare_fn");
  legend_prepare(); return jerry_undefined(); }

JERRYXX_FUN(native_solids_push_fn) {
  
  dbg("module_native::native_solids_push_fn");
  char c = jerry_value_to_char(JERRYXX_GET_ARG(0));
  solids_push(c);
  return jerry_undefined();
}
JERRYXX_FUN(native_solids_clear_fn) { 
  dbg("module_native::native_solids_clear_fn");
  solids_clear(); return jerry_undefined(); }

JERRYXX_FUN(native_push_table_set_fn) {
  
  dbg("module_native::native_push_table_set_fn");
  push_table_set(jerry_value_to_char(JERRYXX_GET_ARG(0)),
                 jerry_value_to_char(JERRYXX_GET_ARG(1)));
  return jerry_undefined();
}
JERRYXX_FUN(native_push_table_clear_fn) { 
  dbg("module_native::native_push_table_clear_fn");
  push_table_clear(); return jerry_undefined(); }

JERRYXX_FUN(native_map_clear_deltas_fn) { 
  dbg("module_native::native_map_clear_deltas_fn");
  map_clear_deltas(); return jerry_undefined(); }

/* I may be recreating kaluma's MAGIC_STRINGs thing here,
   I'd have to look more into how it works */
/* edit: kaluma's MAGIC_STRING thing is for snapshots,
 * which are, yeah, kind of the most extreme version of this.
 * this is still useful though */
static struct {
  jerry_value_t x, y, dx, dy, addr, type, _x, _y, _type, push, remove, generation;
  jerry_property_descriptor_t  x_prop_desc,  y_prop_desc, type_prop_desc,
                              dx_prop_desc, dy_prop_desc;
  jerry_value_t sprite_remove;
} props = {0};

/* lifetime: creates a jerry_value_t you need to free!!! */
static jerry_value_t sprite_to_jerry_addr(Sprite *s) {
  return jerry_number((size_t)(s - state->sprite_pool));
}
static Sprite *sprite_from_jerry_addr(jerry_value_t v) {
  return (size_t)jerry_value_as_number(v) + state->sprite_pool;
}


#define jerry_create_error_sprite_freed() \
  jerry_error_sz(JERRY_ERROR_COMMON, \
     "sprite no longer on map - shouldn't see this")

static Sprite *sprite_from_jerry_object(jerry_value_t this_val) {
  jerry_value_t       addr_prop = jerry_object_get(this_val, props.      addr);
  jerry_value_t generation_prop = jerry_object_get(this_val, props.generation);
  uint32_t generation = jerry_value_as_number(generation_prop);

  Sprite *s = sprite_from_jerry_addr(addr_prop);

  jerry_value_free(      addr_prop);
  jerry_value_free(generation_prop);

  if (map_active(s, generation)) return s;
  else                           return NULL;
}

static jerry_value_t sprite_remove(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (!s) return jerry_create_error_sprite_freed();

  map_remove(s);
  return jerry_undefined();
}


static jerry_value_t sprite_x_getter(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (!s) return jerry_create_error_sprite_freed();

  return jerry_number(s->x);
}
static jerry_value_t sprite_x_setter(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (!s) return jerry_create_error_sprite_freed();

  int new_x = jerry_value_as_number(args[0]);
  map_move(s, new_x - s->x, 0);

  return jerry_undefined();
}


static jerry_value_t sprite_y_getter(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (!s) return jerry_create_error_sprite_freed();

  return jerry_number(s->y);
}
static jerry_value_t sprite_y_setter(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (!s) return jerry_create_error_sprite_freed();

  int new_y = jerry_value_as_number(args[0]);
  map_move(s, 0, new_y - s->y);

  return jerry_undefined();
}


static jerry_value_t sprite_type_getter(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (!s) return jerry_create_error_sprite_freed();

  char tmp[2] = { s->kind };
  return jerry_string_sz(tmp);
}
static jerry_value_t sprite_type_setter(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (!s) return jerry_create_error_sprite_freed();

  s->kind = jerry_value_to_char(args[0]);
  return jerry_undefined();
}


static jerry_value_t sprite_dx_getter(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (s) return jerry_number(s->dx);
  else   return jerry_create_error_sprite_freed();
}

static jerry_value_t sprite_dy_getter(
  const jerry_call_info_t *call_info_p,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(call_info_p->this_value);
  if (s) return jerry_number(s->dy);
  else   return jerry_create_error_sprite_freed();
}


static void props_init(void) {

  props.         x = jerry_string_sz(         "x");
  props.         y = jerry_string_sz(         "y");
  props.        dx = jerry_string_sz(        "dx");
  props.        dy = jerry_string_sz(        "dy");
  props.      addr = jerry_string_sz(      "addr");
  props.      type = jerry_string_sz(      "type");
  props.      push = jerry_string_sz(      "push");
  props.    remove = jerry_string_sz(    "remove");
  props.generation = jerry_string_sz("generation");

  /* this is why we can't fucking have nice things */
  props.        _x = jerry_string_sz(        "_x");
  props.        _y = jerry_string_sz(        "_y");
  props.     _type = jerry_string_sz(     "_type");
  /* GAHHHHHHHHH just kill me now */
  /* i shouldn't let this bother me so much ... */

  props.sprite_remove = jerry_function_external(sprite_remove);

  props.x_prop_desc = jerry_property_descriptor();
  props.x_prop_desc.flags = JERRY_PROP_IS_CONFIGURABLE | JERRY_PROP_IS_CONFIGURABLE_DEFINED | JERRY_PROP_IS_GET_DEFINED | JERRY_PROP_IS_SET_DEFINED;
  props.x_prop_desc.getter = jerry_function_external(sprite_x_getter);
  props.x_prop_desc.setter = jerry_function_external(sprite_x_setter);

  props.y_prop_desc = jerry_property_descriptor();
  props.y_prop_desc.flags = JERRY_PROP_IS_CONFIGURABLE | JERRY_PROP_IS_CONFIGURABLE_DEFINED | JERRY_PROP_IS_GET_DEFINED | JERRY_PROP_IS_SET_DEFINED;
  props.y_prop_desc.getter = jerry_function_external(sprite_y_getter);
  props.y_prop_desc.setter = jerry_function_external(sprite_y_setter);

  props.type_prop_desc = jerry_property_descriptor();
  props.type_prop_desc.flags = JERRY_PROP_IS_CONFIGURABLE | JERRY_PROP_IS_CONFIGURABLE_DEFINED | JERRY_PROP_IS_GET_DEFINED | JERRY_PROP_IS_SET_DEFINED;
  props.type_prop_desc.getter = jerry_function_external(sprite_type_getter);
  props.type_prop_desc.setter = jerry_function_external(sprite_type_setter);

  props.dx_prop_desc = jerry_property_descriptor();
  props.dx_prop_desc.flags = JERRY_PROP_IS_CONFIGURABLE | JERRY_PROP_IS_CONFIGURABLE_DEFINED | JERRY_PROP_IS_GET_DEFINED;
  props.dx_prop_desc.getter = jerry_function_external(sprite_dx_getter);

  props.dy_prop_desc = jerry_property_descriptor();
  props.dy_prop_desc.getter = JERRY_PROP_IS_CONFIGURABLE | JERRY_PROP_IS_CONFIGURABLE_DEFINED | JERRY_PROP_IS_GET_DEFINED;
  props.dy_prop_desc.getter = jerry_function_external(sprite_dy_getter);
}

static jerry_value_t sprite_alloc_jerry_object(Sprite *s) {
  if (s == 0) return jerry_undefined();

  jerry_value_t ret = jerry_object();

  /* store addr field on ret */
  jerry_value_t addr_val = sprite_to_jerry_addr(s);
  jerry_value_free(jerry_object_set(ret, props.addr, addr_val));
  jerry_value_free(addr_val);

  /* store generation field on ret */
  jerry_value_t generation_val = jerry_number(sprite_generation(s));
  jerry_value_free(jerry_object_set(ret, props.generation, generation_val));
  jerry_value_free(generation_val);

  /* add methods */
  jerry_value_free(jerry_object_set(ret, props.remove, props.sprite_remove));

  /* add getters, setters */
  jerry_value_free(jerry_object_define_own_prop(ret, props.x, &props.x_prop_desc));
  jerry_value_free(jerry_object_define_own_prop(ret, props.y, &props.y_prop_desc));
  jerry_value_free(jerry_object_define_own_prop(ret, props.dx, &props.dx_prop_desc));
  jerry_value_free(jerry_object_define_own_prop(ret, props.dy, &props.dy_prop_desc));
  jerry_value_free(jerry_object_define_own_prop(ret, props.type, &props.type_prop_desc));

  jerry_value_free(jerry_object_define_own_prop(ret, props._x, &props.x_prop_desc));
  jerry_value_free(jerry_object_define_own_prop(ret, props._y, &props.y_prop_desc));
  jerry_value_free(jerry_object_define_own_prop(ret, props._type, &props.type_prop_desc));

  return ret;
}

jerry_value_t sprite_object_pool[SPRITE_COUNT] = {0};
static jerry_value_t sprite_to_jerry_object(Sprite *s) {
  int i = s - state->sprite_pool;

  if (!sprite_object_pool[i])
    sprite_object_pool[i] = sprite_alloc_jerry_object(s);

  return jerry_value_copy(sprite_object_pool[i]);
}

static void sprite_free_jerry_object(Sprite *s) {
  int i = s - state->sprite_pool;

  if (sprite_object_pool[i]) {
    jerry_value_t so = sprite_object_pool[i];

    jerry_value_t x     = jerry_object_get(so, props.x    );
    jerry_value_t y     = jerry_object_get(so, props.y    );
    jerry_value_t dx    = jerry_object_get(so, props.dx   );
    jerry_value_t dy    = jerry_object_get(so, props.dy   );
    jerry_value_t type  = jerry_object_get(so, props.type );
    jerry_value_t _x    = jerry_object_get(so, props._x   );
    jerry_value_t _y    = jerry_object_get(so, props._y   );
    jerry_value_t _type = jerry_object_get(so, props._type);

    jerry_object_delete(so, props.x    );
    jerry_object_delete(so, props.y    );
    jerry_object_delete(so, props.dx   );
    jerry_object_delete(so, props.dy   );
    jerry_object_delete(so, props.type );
    jerry_object_delete(so, props._x   );
    jerry_object_delete(so, props._y   );
    jerry_object_delete(so, props._type);

    jerry_value_free(jerry_object_set(so, props.x    , x    ));
    jerry_value_free(jerry_object_set(so, props.y    , y    ));
    jerry_value_free(jerry_object_set(so, props.dx   , dx   ));
    jerry_value_free(jerry_object_set(so, props.dy   , dy   ));
    jerry_value_free(jerry_object_set(so, props.type , type ));
    jerry_value_free(jerry_object_set(so, props._x   , _x   ));
    jerry_value_free(jerry_object_set(so, props._y   , _y   ));
    jerry_value_free(jerry_object_set(so, props._type, _type));

    jerry_value_free(x    );
    jerry_value_free(y    );
    jerry_value_free(dx   );
    jerry_value_free(dy   );
    jerry_value_free(type );
    jerry_value_free(_x   );
    jerry_value_free(_y   );
    jerry_value_free(_type);

    jerry_value_free(so);
    sprite_object_pool[i] = 0;
  }
}

JERRYXX_FUN(getFirst) {
  
  dbg("module_native::getFirst");
  JERRYXX_CHECK_ARG(0, "char");
  char kind = jerry_value_to_char(JERRYXX_GET_ARG(0));
  return sprite_to_jerry_object(map_get_first(kind));
}

JERRYXX_FUN(clearTile) {
  
  dbg("module_native::clearTile");
  JERRYXX_CHECK_ARG_NUMBER(0, "x");
  JERRYXX_CHECK_ARG_NUMBER(1, "y");
  map_drill(
    JERRYXX_GET_ARG_NUMBER(0),
    JERRYXX_GET_ARG_NUMBER(1)
  );
  return jerry_undefined();
}

JERRYXX_FUN(addSprite) {
  
  dbg("module_native::addSprite");
  JERRYXX_CHECK_ARG_NUMBER(0, "x");
  JERRYXX_CHECK_ARG_NUMBER(1, "y");
  JERRYXX_CHECK_ARG(2, "type");
  Sprite *s = map_add(
    JERRYXX_GET_ARG_NUMBER(0),
    JERRYXX_GET_ARG_NUMBER(1),
    jerry_value_to_char(JERRYXX_GET_ARG(2))
  );

  if (s == 0)
    return jerry_error_sz(
      JERRY_ERROR_COMMON,
      "can't add sprite to location outside of map"
    );
  else
    return sprite_to_jerry_object(s);
}

/*
    getTile: (x, y) => {
      const iter = wasm.temp_MapIter_mem();
      wasm.MapIter_position(iter, x, y);

      const out = [];
      while (wasm.map_get_grid(iter)) {
        const sprite = addrToSprite(readU32(iter));
        if (sprite.x != x || sprite.y != y)
          break;
        out.push(sprite);
      }
      return out;
    },
*/
JERRYXX_FUN(getTile) {
  
  dbg("module_native::getTile");
  JERRYXX_CHECK_ARG_NUMBER(0, "x");
  JERRYXX_CHECK_ARG_NUMBER(1, "y");
  int x = JERRYXX_GET_ARG_NUMBER(0);
  int y = JERRYXX_GET_ARG_NUMBER(1);

  /* allocating is almost certainly more expensive than iterating through our
     lil spritestack, so we iterate through once to figure out how big of an array
     we should return */
  int i = 0;
  MapIter m = { .x = x, .y = y };
  while (map_get_grid(&m) && (m.sprite->x == x && m.sprite->y == y)) i++;

  jerry_value_t ret = jerry_array(i);
  i = 0;
  m = (MapIter) { .x = x, .y = y };
  while (map_get_grid(&m) && (m.sprite->x == x && m.sprite->y == y)) {
    jerry_value_t sprite = sprite_to_jerry_object(m.sprite);
    jerry_value_free(jerry_object_set_index(ret, i++, sprite));
    jerry_value_free(sprite);
  }

  return ret;
}


JERRYXX_FUN(width) { 
  dbg("module_native::width");
  return jerry_number(state->width); }
JERRYXX_FUN(height) { 
  dbg("module_native::height");
  return jerry_number(state->height); }
JERRYXX_FUN(getAll) {
  
  dbg("module_native::getAll");
  uint8_t no_arg = JERRYXX_GET_ARG_COUNT == 0;
  char kind = no_arg ? 0 : jerry_value_to_char(JERRYXX_GET_ARG(0));
  int i = 0;
  
  /* figure out how much to allocate */
  MapIter m = {0};
  while (map_get_grid(&m))
    if (no_arg || m.sprite->kind == kind)
      i++;
  jerry_value_t ret = jerry_array(i);

  i = 0;
  m = (MapIter) {0};
  while (map_get_grid(&m))
    if (no_arg || m.sprite->kind == kind) {
      jerry_value_t sprite = sprite_to_jerry_object(m.sprite);
      jerry_value_free(
        jerry_object_set_index(ret, i++, sprite)
      );
      jerry_value_free(sprite);
    }

  return ret;
}

JERRYXX_FUN(getGrid) {
  
  dbg("module_native::getGrid");
  int len = map_width() * map_height();
  jerry_value_t ret = jerry_array(len);
  for (int i = 0; i < len; i++)
    jerry_value_free(
      jerry_object_set_index(ret, i, jerry_array(0))
    );

  MapIter m = {0};
  while (map_get_grid(&m)) {
    int i = m.sprite->x + state->width*m.sprite->y;
    jerry_value_t tile = jerry_object_get_index(ret, i);
    jerry_value_t arg = sprite_to_jerry_object(m.sprite);
    jerry_value_t push = jerry_object_get(tile, props.push);
    jerry_value_free(jerry_call(push, tile, &arg, 1));
    jerry_value_free(push);
    jerry_value_free(arg);
    jerry_value_free(tile);
  }

  return ret;
}

JERRYXX_FUN(tilesWith) {
  
  dbg("module_native::tilesWith");
  char *kinds = temp_str_mem();

  for (int i = 0; i < args_cnt; i++)
    kinds[i] = jerry_value_to_char(args_p[i]);

  MapIter m = {0};
  int ntiles = 0;
  while (map_tiles_with(&m, kinds)) ntiles++;

  jerry_value_t ret = jerry_array(ntiles);

  m = (MapIter){0};
  int i = 0;
  while (map_tiles_with(&m, kinds)) {
    int nsprites = 0;
    for (Sprite *s = m.sprite; s; s = s->next) nsprites++;
    jerry_value_t tile = jerry_array(nsprites);

    int j = 0;
    for (Sprite *s = m.sprite; s; s = s->next) {
      jerry_value_t sprite = sprite_to_jerry_object(s);
      jerry_value_free(jerry_object_set_index(tile, j++, sprite));
      jerry_value_free(sprite);
    }

    jerry_value_free(jerry_object_set_index(ret, i++, tile));
    jerry_value_free(tile);
  }
  return ret;
}

JERRYXX_FUN(native_text_add_fn) {
  
  dbg("module_native::native_text_add_fn");
  jerry_char_t *tmp = (jerry_char_t *)temp_str_mem();
  jerry_size_t nbytes = jerry_string_to_buffer(
    JERRYXX_GET_ARG(0),
    JERRY_ENCODING_UTF8,
    tmp,
    sizeof(state->temp_str_mem) - 1
  );
  tmp[nbytes] = '\0'; 

  jerry_value_t color_val = JERRYXX_GET_ARG(1);

  uint8_t color[3] = {0};
  for (int i = 0; i < 3; i++) {
    jerry_value_t el = jerry_object_get_index(color_val, i);
    color[i] = jerry_value_as_number(el);
    jerry_value_free(el);
  }

  text_add(
    (char *)tmp,
    color16(color[0], color[1], color[2]),
    JERRYXX_GET_ARG_NUMBER(2),
    JERRYXX_GET_ARG_NUMBER(3) 
  );

  return jerry_undefined();
}

JERRYXX_FUN(native_text_clear_fn) { 
  dbg("module_native::native_text_clear_fn");
  text_clear(); return jerry_undefined(); }


jerry_value_t module_native_init(jerry_value_t exports) {

  props_init();

  /* these ones actually need to be in C for perf */
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_setMap),    jerry_function_external(setMap));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_tilesWith), jerry_function_external(tilesWith));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_getGrid),   jerry_function_external(getGrid));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_getFirst),  jerry_function_external(getFirst));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_getAll),    jerry_function_external(getAll));

  /* it was just easier to implement these in C */
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_width),         jerry_function_external(width));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_height),        jerry_function_external(height));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_setBackground), jerry_function_external(setBackground));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_getTile),       jerry_function_external(getTile));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_clearTile),     jerry_function_external(clearTile));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_addSprite),     jerry_function_external(addSprite));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_text_add),      jerry_function_external(native_text_add_fn));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_text_clear),    jerry_function_external(native_text_clear_fn));

  /* random background goodie */
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_map_clear_deltas), jerry_function_external(native_map_clear_deltas_fn));

  /* it was easier to split these into multiple C functions than do the JS data shuffling in C */
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_solids_push), jerry_function_external(native_solids_push_fn));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_solids_clear), jerry_function_external(native_solids_clear_fn));
  /* -- */
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_push_table_set), jerry_function_external(native_push_table_set_fn));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_push_table_clear), jerry_function_external(native_push_table_clear_fn));
  /* -- */
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_legend_doodle_set), jerry_function_external(native_legend_doodle_set_fn));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_legend_clear), jerry_function_external(native_legend_clear_fn));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_legend_prepare), jerry_function_external(native_legend_prepare_fn));

  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_press_cb), jerry_function_external(native_press_cb_fn));
  jerry_object_set(exports, jerry_string_sz(MSTR_NATIVE_frame_cb), jerry_function_external(native_frame_cb_fn));

  return jerry_undefined();
}

// moved JS wrapper into engine.js
