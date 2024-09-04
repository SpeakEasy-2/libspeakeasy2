/* Copyright 2024 David R. Connell <david32@dcon.addy.io>.
 *
 * This file is part of SpeakEasy 2.
 *
 * SpeakEasy 2 is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * SpeakEasy 2 is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with SpeakEasy 2. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "se2_interface.h"

#ifndef USING_R
static se2_int_printf_func_t* se2_i_int_printf = printf;
#else
static int ignore_printf(const char* fmt, ...)
{
  return 0;
}

static se2_int_printf_func_t* se2_i_int_printf = ignore_printf;
#endif

static se2_void_printf_func_t* se2_i_void_printf = NULL;

void se2_set_int_printf_func(se2_int_printf_func_t* func)
{
  se2_i_int_printf = func;
  se2_i_void_printf = NULL;
}

void se2_set_void_printf_func(se2_void_printf_func_t* func)
{
  se2_i_void_printf = func;
  se2_i_int_printf = NULL;
}

void se2_printf(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char buf[1024];
  vsnprintf(buf, 1023, fmt, ap);
  if (se2_i_int_printf) {
    se2_i_int_printf(buf);
  } else {
    se2_i_void_printf(buf);
  }
  va_end(ap);
}

void se2_puts(const char* msg)
{
  se2_printf("%s\n", msg);
}
