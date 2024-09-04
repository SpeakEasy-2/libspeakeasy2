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

#define SE2_PRINTF(fmt, ...) IGRAPH_STATUSF((fmt, NULL, __VA_ARGS__))
#define SE2_PRINT(message) SE2_PRINTF("%s", message)
#define SE2_PUTS(message) SE2_PRINTF("%s\n", message)

#endif
