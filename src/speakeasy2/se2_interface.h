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

#ifndef SE2_INTERFACE_H
#define SE2_INTERFACE_H

#include <igraph.h>

typedef int se2_int_printf_func_t(const char*, ...);
typedef void se2_void_printf_func_t(const char*, ...);
typedef igraph_bool_t se2_check_user_interrupt_func_t(void);

void se2_printf(const char*, ...);
void se2_puts(const char*);
igraph_bool_t se2_check_user_interrupt(void);

/* Only set one, this corrects for R's printf not returning an int. */
void se2_set_int_printf_func(se2_int_printf_func_t* func);
void se2_set_void_printf_func(se2_void_printf_func_t* func);
void se2_set_check_user_interrupt_func(se2_check_user_interrupt_func_t* func);

#endif
