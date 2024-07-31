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

#ifndef SE2_PRINT_H
#define SE2_PRINT_H

#ifdef USING_R
#  include <R_ext/Print.h>

#  define se2_printf Rprintf
#  define se2_warn REprintf
#  define se2_puts(...) se2_printf(__VA_ARGS__); se2_printf("\n")

#else
#  define se2_printf printf
#  define se2_warn(...) fprintf(stderr, __VA_ARGS__);
#  define se2_puts puts
#endif

#endif
