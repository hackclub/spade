/* Copyright (c) 2017 Kaluma
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

#include "jerryxx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jerryscript.h"

void jerryxx_print_value(jerry_value_t value) {
  jerry_value_t str = jerry_value_to_string(value);
  jerry_size_t str_sz = jerry_string_size(str, JERRY_ENCODING_UTF8);
  jerry_char_t str_buf[str_sz + 1];
  memset(str_buf, 0, str_sz + 1);
  jerry_string_to_buffer(str, JERRY_ENCODING_UTF8, str_buf, str_sz);

  // for (int16_t i = 0; i < str_sz; i++) km_tty_putc(str_buf[i]);
  puts((const char *) str_buf);

  jerry_value_free(str);
}
/**
 * Print error with stacktrace
 */
void jerryxx_print_error(jerry_value_t value, bool print_stacktrace) {
  jerry_value_t error_value = jerry_exception_value(value, false);
  // print error message
  jerry_value_t err_str = jerry_value_to_string(error_value);
  puts("err:");
  jerryxx_print_value(err_str);
  puts("");
  jerry_value_free(err_str);
  // print stack trace
  if (print_stacktrace && jerry_value_is_object(error_value)) {
    jerry_value_t stack_str =
        jerry_string_sz("stack");
    jerry_value_t backtrace_val = jerry_object_get(error_value, stack_str);
    jerry_value_free(stack_str);
    if (!jerry_value_is_error(backtrace_val) &&
        jerry_value_is_array(backtrace_val)) {
      uint32_t length = jerry_array_length(backtrace_val);
      if (length > 32) {
        length = 32;
      } /* max length: 32 */
      for (uint32_t i = 0; i < length; i++) {
        jerry_value_t item_val = jerry_object_get_index(backtrace_val, i);
        if (!jerry_value_is_error(item_val) &&
            jerry_value_is_string(item_val)) {
          printf("  at ");
          jerryxx_print_value(item_val);
          puts("");
        }
        jerry_value_free(item_val);
      }
    }
    jerry_value_free(backtrace_val);
  }
  jerry_value_free(error_value);
}
