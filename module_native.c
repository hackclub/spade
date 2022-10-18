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
  jerry_size_t nbytes = jerry_string_to_char_buffer(val, tmp, 1);
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

  char *tmp = temp_str_mem();
  jerry_size_t nbytes = jerry_string_to_char_buffer(
    JERRYXX_GET_ARG(0),
    (jerry_char_t *)tmp,
    sizeof(state->temp_str_mem) - 1
  );
  tmp[nbytes] = '\0'; 

  map_set(tmp);

  return jerry_create_undefined();
}

JERRYXX_FUN(setBackground) {
  
  dbg("module_native::setBackground");
  render_set_background(jerry_value_to_char(JERRYXX_GET_ARG(0)));
  return jerry_create_undefined();
}

JERRYXX_FUN(native_legend_doodle_set_fn) {
  
  dbg("module_native::native_legend_doodle_set_fn");
  JERRYXX_CHECK_ARG(0, "char");
  JERRYXX_CHECK_ARG(1, "str");

  char *tmp = temp_str_mem();
  jerry_size_t nbytes = jerry_string_to_char_buffer(
    JERRYXX_GET_ARG(1),
    (jerry_char_t *)tmp,
    sizeof(state->temp_str_mem) - 1
  );
  tmp[nbytes] = '\0'; 

  legend_doodle_set(jerry_value_to_char(JERRYXX_GET_ARG(0)), tmp);

  return jerry_create_undefined();
}

JERRYXX_FUN(native_press_cb_fn) {
  if (spade_state.press_cb) jerry_release_value(spade_state.press_cb);

  spade_state.press_cb = jerry_acquire_value(JERRYXX_GET_ARG(0));
  return jerry_create_undefined(); 
}

JERRYXX_FUN(native_frame_cb_fn) {
  if (spade_state.frame_cb) jerry_release_value(spade_state.frame_cb);

  spade_state.frame_cb = jerry_acquire_value(JERRYXX_GET_ARG(0));
  return jerry_create_undefined(); 
}

JERRYXX_FUN(native_legend_clear_fn) { 
  dbg("module_native::native_legend_clear_fn");
  legend_clear(); return jerry_create_undefined(); }
JERRYXX_FUN(native_legend_prepare_fn) { 
  dbg("module_native::native_legend_prepare_fn");
  legend_prepare(); return jerry_create_undefined(); }

JERRYXX_FUN(native_solids_push_fn) {
  
  dbg("module_native::native_solids_push_fn");
  char c = jerry_value_to_char(JERRYXX_GET_ARG(0));
  solids_push(c);
  return jerry_create_undefined();
}
JERRYXX_FUN(native_solids_clear_fn) { 
  dbg("module_native::native_solids_clear_fn");
  solids_clear(); return jerry_create_undefined(); }

JERRYXX_FUN(native_push_table_set_fn) {
  
  dbg("module_native::native_push_table_set_fn");
  push_table_set(jerry_value_to_char(JERRYXX_GET_ARG(0)),
                 jerry_value_to_char(JERRYXX_GET_ARG(1)));
  return jerry_create_undefined();
}
JERRYXX_FUN(native_push_table_clear_fn) { 
  dbg("module_native::native_push_table_clear_fn");
  push_table_clear(); return jerry_create_undefined(); }

JERRYXX_FUN(native_map_clear_deltas_fn) { 
  dbg("module_native::native_map_clear_deltas_fn");
  map_clear_deltas(); return jerry_create_undefined(); }

/* I may be recreating kaluma's MAGIC_STRINGs thing here,
   I'd have to look more into how it works */
static struct {
  jerry_value_t x, y, dx, dy, addr, type, _x, _y, _type, push, remove, generation;
  jerry_property_descriptor_t  x_prop_desc,  y_prop_desc, type_prop_desc,
                              dx_prop_desc, dy_prop_desc;
  jerry_value_t sprite_remove;
} props = {0};

/* lifetime: creates a jerry_value_t you need to free!!! */
static jerry_value_t sprite_to_jerry_addr(Sprite *s) {
  return jerry_create_number((size_t)(s - state->sprite_pool));
}
static Sprite *sprite_from_jerry_addr(jerry_value_t v) {
  return (size_t)jerry_get_number_value(v) + state->sprite_pool;
}


#define jerry_create_error_sprite_freed() \
  jerry_create_error(JERRY_ERROR_COMMON, (jerry_char_t *)"sprite no longer on map")

static Sprite *sprite_from_jerry_object(jerry_value_t this_val) {
  jerry_value_t       addr_prop = jerry_get_property(this_val, props.      addr);
  jerry_value_t generation_prop = jerry_get_property(this_val, props.generation);
  uint32_t generation = jerry_get_number_value(generation_prop);

  Sprite *s = sprite_from_jerry_addr(addr_prop);

  jerry_release_value(      addr_prop);
  jerry_release_value(generation_prop);

  if (map_active(s, generation)) return s;
  else                           return NULL;
}

static jerry_value_t sprite_remove(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (!s) return jerry_create_error_sprite_freed();

  map_remove(s);
  return jerry_create_undefined();
}


static jerry_value_t sprite_x_getter(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (!s) return jerry_create_error_sprite_freed();

  return jerry_create_number(s->x);
}
static jerry_value_t sprite_x_setter(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (!s) return jerry_create_error_sprite_freed();

  int new_x = jerry_get_number_value(args[0]);
  map_move(s, new_x - s->x, 0);

  return jerry_create_undefined();
}


static jerry_value_t sprite_y_getter(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (!s) return jerry_create_error_sprite_freed();

  return jerry_create_number(s->y);
}
static jerry_value_t sprite_y_setter(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (!s) return jerry_create_error_sprite_freed();

  int new_y = jerry_get_number_value(args[0]);
  map_move(s, 0, new_y - s->y);

  return jerry_create_undefined();
}


static jerry_value_t sprite_type_getter(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (!s) return jerry_create_error_sprite_freed();

  jerry_char_t tmp[2] = { s->kind };
  return jerry_create_string(tmp);
}
static jerry_value_t sprite_type_setter(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (!s) return jerry_create_error_sprite_freed();

  s->kind = jerry_value_to_char(args[0]);
  return jerry_create_undefined();
}


static jerry_value_t sprite_dx_getter(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (s) return jerry_create_number(s->dx);
  else   return jerry_create_error_sprite_freed();
}

static jerry_value_t sprite_dy_getter(
  const jerry_value_t func_obj,
  const jerry_value_t this_obj,
  const jerry_value_t args[],
  const jerry_length_t argc
) {
  Sprite *s = sprite_from_jerry_object(this_obj);
  if (s) return jerry_create_number(s->dy);
  else   return jerry_create_error_sprite_freed();
}


static void props_init(void) {

  props.         x = jerry_create_string((const jerry_char_t *)          "x");
  props.         y = jerry_create_string((const jerry_char_t *)          "y");
  props.        dx = jerry_create_string((const jerry_char_t *)         "dx");
  props.        dy = jerry_create_string((const jerry_char_t *)         "dy");
  props.      addr = jerry_create_string((const jerry_char_t *)       "addr");
  props.      type = jerry_create_string((const jerry_char_t *)       "type");
  props.      push = jerry_create_string((const jerry_char_t *)       "push");
  props.    remove = jerry_create_string((const jerry_char_t *)     "remove");
  props.generation = jerry_create_string((const jerry_char_t *) "generation");

  /* this is why we can't fucking have nice things */
  props.        _x = jerry_create_string((const jerry_char_t *)         "_x");
  props.        _y = jerry_create_string((const jerry_char_t *)         "_y");
  props.     _type = jerry_create_string((const jerry_char_t *)      "_type");
  /* GAHHHHHHHHH just kill me now */
  /* i shouldn't let this bother me so much ... */

  props.sprite_remove = jerry_create_external_function(sprite_remove);

  jerry_init_property_descriptor_fields(&props.x_prop_desc);
  props.x_prop_desc.is_get_defined = 1;
  props.x_prop_desc.getter = jerry_create_external_function(sprite_x_getter);
  props.x_prop_desc.is_set_defined = 1;
  props.x_prop_desc.setter = jerry_create_external_function(sprite_x_setter);

  jerry_init_property_descriptor_fields(&props.y_prop_desc);
  props.y_prop_desc.is_get_defined = 1;
  props.y_prop_desc.getter = jerry_create_external_function(sprite_y_getter);
  props.y_prop_desc.is_set_defined = 1;
  props.y_prop_desc.setter = jerry_create_external_function(sprite_y_setter);

  jerry_init_property_descriptor_fields(&props.type_prop_desc);
  props.type_prop_desc.is_get_defined = 1;
  props.type_prop_desc.getter = jerry_create_external_function(sprite_type_getter);
  props.type_prop_desc.is_set_defined = 1;
  props.type_prop_desc.setter = jerry_create_external_function(sprite_type_setter);

  jerry_init_property_descriptor_fields(&props.dx_prop_desc);
  props.dx_prop_desc.is_get_defined = 1;
  props.dx_prop_desc.getter = jerry_create_external_function(sprite_dx_getter);

  jerry_init_property_descriptor_fields(&props.dy_prop_desc);
  props.dy_prop_desc.is_get_defined = 1;
  props.dy_prop_desc.getter = jerry_create_external_function(sprite_dy_getter);
}

static jerry_value_t sprite_to_jerry_object(Sprite *s) {
  if (s == 0) return jerry_create_undefined();

  jerry_value_t ret = jerry_create_object();

  /* store addr field on ret */
  jerry_value_t addr_val = sprite_to_jerry_addr(s);
  jerry_release_value(jerry_set_property(ret, props.addr, addr_val));
  jerry_release_value(addr_val);

  /* store generation field on ret */
  jerry_value_t generation_val = jerry_create_number(sprite_generation(s));
  jerry_release_value(jerry_set_property(ret, props.generation, generation_val));
  jerry_release_value(generation_val);

  /* add methods */
  jerry_release_value(jerry_set_property(ret, props.remove, props.sprite_remove));

  /* add getters, setters */
  jerry_release_value(jerry_define_own_property(ret, props.x, &props.x_prop_desc));
  jerry_release_value(jerry_define_own_property(ret, props.y, &props.y_prop_desc));
  jerry_release_value(jerry_define_own_property(ret, props.dx, &props.dx_prop_desc));
  jerry_release_value(jerry_define_own_property(ret, props.dy, &props.dy_prop_desc));
  jerry_release_value(jerry_define_own_property(ret, props.type, &props.type_prop_desc));

  jerry_release_value(jerry_define_own_property(ret, props._x, &props.x_prop_desc));
  jerry_release_value(jerry_define_own_property(ret, props._y, &props.y_prop_desc));
  jerry_release_value(jerry_define_own_property(ret, props._type, &props.type_prop_desc));

  return ret;
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
  return jerry_create_undefined();
}

JERRYXX_FUN(addSprite) {
  
  dbg("module_native::addSprite");
  JERRYXX_CHECK_ARG_NUMBER(0, "x");
  JERRYXX_CHECK_ARG_NUMBER(1, "y");
  JERRYXX_CHECK_ARG(2, "type");
  return sprite_to_jerry_object(map_add(
    JERRYXX_GET_ARG_NUMBER(0),
    JERRYXX_GET_ARG_NUMBER(1),
    jerry_value_to_char(JERRYXX_GET_ARG(2))
  ));
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

  jerry_value_t ret = jerry_create_array(i);
  i = 0;
  m = (MapIter) { .x = x, .y = y };
  while (map_get_grid(&m) && (m.sprite->x == x && m.sprite->y == y)) {
    jerry_value_t sprite = sprite_to_jerry_object(m.sprite);
    jerry_release_value(jerry_set_property_by_index(ret, i++, sprite));
    jerry_release_value(sprite);
  }

  return ret;
}


JERRYXX_FUN(width) { 
  dbg("module_native::width");
  return jerry_create_number(state->width); }
JERRYXX_FUN(height) { 
  dbg("module_native::height");
  return jerry_create_number(state->height); }
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
  jerry_value_t ret = jerry_create_array(i);

  i = 0;
  m = (MapIter) {0};
  while (map_get_grid(&m))
    if (no_arg || m.sprite->kind == kind) {
      jerry_value_t sprite = sprite_to_jerry_object(m.sprite);
      jerry_release_value(
        jerry_set_property_by_index(ret, i++, sprite)
      );
      jerry_release_value(sprite);
    }

  return ret;
}

JERRYXX_FUN(getGrid) {
  
  dbg("module_native::getGrid");
  int len = map_width() * map_height();
  jerry_value_t ret = jerry_create_array(len);
  for (int i = 0; i < len; i++)
    jerry_release_value(
      jerry_set_property_by_index(ret, i, jerry_create_array(0))
    );

  MapIter m = {0};
  while (map_get_grid(&m)) {
    int i = m.sprite->x + state->width*m.sprite->y;
    jerry_value_t tile = jerry_get_property_by_index(ret, i);
    jerry_value_t arg = sprite_to_jerry_object(m.sprite);
    jerry_value_t push = jerry_get_property(tile, props.push);
    jerry_release_value(jerry_call_function(push, tile, &arg, 1));
    jerry_release_value(push);
    jerry_release_value(arg);
    jerry_release_value(tile);
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

  jerry_value_t ret = jerry_create_array(ntiles);

  m = (MapIter){0};
  int i = 0;
  while (map_tiles_with(&m, kinds)) {
    int nsprites = 0;
    for (Sprite *s = m.sprite; s; s = s->next) nsprites++;
    jerry_value_t tile = jerry_create_array(nsprites);

    int j = 0;
    for (Sprite *s = m.sprite; s; s = s->next) {
      jerry_value_t sprite = sprite_to_jerry_object(s);
      jerry_release_value(jerry_set_property_by_index(tile, j++, sprite));
      jerry_release_value(sprite);
    }

    jerry_release_value(jerry_set_property_by_index(ret, i++, tile));
    jerry_release_value(tile);
  }
  return ret;
}

JERRYXX_FUN(native_text_add_fn) {
  
  dbg("module_native::native_text_add_fn");
  char *tmp = temp_str_mem();
  jerry_size_t nbytes = jerry_string_to_char_buffer(
    JERRYXX_GET_ARG(0),
    (jerry_char_t *)tmp,
    sizeof(state->temp_str_mem) - 1
  );
  tmp[nbytes] = '\0'; 

  jerry_value_t color_val = JERRYXX_GET_ARG(1);

  uint8_t color[3] = {0};
  for (int i = 0; i < 3; i++) {
    jerry_value_t el = jerry_get_property_by_index(color_val, i);
    color[i] = jerry_get_number_value(el);
    jerry_release_value(el);
  }

  text_add(
    tmp,
    color16(color[0], color[1], color[2]),
    JERRYXX_GET_ARG_NUMBER(2),
    JERRYXX_GET_ARG_NUMBER(3) 
  );

  return jerry_create_undefined();
}

JERRYXX_FUN(native_text_clear_fn) { 
  dbg("module_native::native_text_clear_fn");
  text_clear(); return jerry_create_undefined(); }


jerry_value_t module_native_init(jerry_value_t exports) {

  props_init();

  /* these ones actually need to be in C for perf */
  jerryxx_set_property_function(exports, MSTR_NATIVE_setMap,    setMap);
  jerryxx_set_property_function(exports, MSTR_NATIVE_tilesWith, tilesWith);
  jerryxx_set_property_function(exports, MSTR_NATIVE_getGrid,   getGrid);
  jerryxx_set_property_function(exports, MSTR_NATIVE_getFirst,  getFirst);
  jerryxx_set_property_function(exports, MSTR_NATIVE_getAll,    getAll);

  /* it was just easier to implement these in C */
  jerryxx_set_property_function(exports, MSTR_NATIVE_width,         width);
  jerryxx_set_property_function(exports, MSTR_NATIVE_height,        height);
  jerryxx_set_property_function(exports, MSTR_NATIVE_setBackground, setBackground);
  jerryxx_set_property_function(exports, MSTR_NATIVE_getTile,       getTile);
  jerryxx_set_property_function(exports, MSTR_NATIVE_clearTile,     clearTile);
  jerryxx_set_property_function(exports, MSTR_NATIVE_addSprite,     addSprite);
  jerryxx_set_property_function(exports, MSTR_NATIVE_text_add,      native_text_add_fn);
  jerryxx_set_property_function(exports, MSTR_NATIVE_text_clear,    native_text_clear_fn);

  /* random background goodie */
  jerryxx_set_property_function(exports, MSTR_NATIVE_map_clear_deltas, native_map_clear_deltas_fn);

  /* it was easier to split these into multiple C functions than do the JS data shuffling in C */
  jerryxx_set_property_function(exports, MSTR_NATIVE_solids_push, native_solids_push_fn);
  jerryxx_set_property_function(exports, MSTR_NATIVE_solids_clear, native_solids_clear_fn);
  /* -- */
  jerryxx_set_property_function(exports, MSTR_NATIVE_push_table_set, native_push_table_set_fn);
  jerryxx_set_property_function(exports, MSTR_NATIVE_push_table_clear, native_push_table_clear_fn);
  /* -- */
  jerryxx_set_property_function(exports, MSTR_NATIVE_legend_doodle_set, native_legend_doodle_set_fn);
  jerryxx_set_property_function(exports, MSTR_NATIVE_legend_clear, native_legend_clear_fn);
  jerryxx_set_property_function(exports, MSTR_NATIVE_legend_prepare, native_legend_prepare_fn);

  jerryxx_set_property_function(exports, MSTR_NATIVE_press_cb, native_press_cb_fn);
  jerryxx_set_property_function(exports, MSTR_NATIVE_frame_cb, native_frame_cb_fn);

  return jerry_create_undefined();
}

// moved JS wrapper into engine.js
