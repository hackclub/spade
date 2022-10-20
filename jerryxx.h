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

#ifndef __JERRYXX_H
#define __JERRYXX_H

#include <stdio.h>

#include "jerryscript.h"

#define JERRYXX_GET_ARG_COUNT args_cnt

#define JERRYXX_GET_ARG(index) args_p[index]

#define JERRYXX_GET_ARG_NUMBER(index) jerry_value_as_number(args_p[index])

#define JERRYXX_FUN(name)                                           \
  static jerry_value_t name(                                        \
      const jerry_call_info_t *call_info_p,                          \
      const jerry_value_t args_p[], const jerry_length_t args_cnt)

#define JERRYXX_CHECK_ARG(index, argname)                                      \
  if (args_cnt <= index) {                                                     \
    char errmsg[255];                                                          \
    sprintf(errmsg, "\"%s\" argument required", argname);                      \
    return jerry_error_sz(JERRY_ERROR_TYPE, errmsg); \
  }

#define JERRYXX_CHECK_ARG_NUMBER(index, argname)                               \
  if ((args_cnt <= index) || (!jerry_value_is_number(args_p[index]))) {        \
    char errmsg[255];                                                          \
    sprintf(errmsg, "\"%s\" argument must be a number", argname);              \
    return jerry_error_sz(JERRY_ERROR_TYPE, errmsg); \
  }

void jerryxx_print_value(jerry_value_t value);
void jerryxx_print_error(jerry_value_t value, bool print_stacktrace);

#endif /* __JERRYXX_H */
