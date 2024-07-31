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

#ifndef SE2_RANDOM_H
#define SE2_RANDOM_H

#include <igraph.h>

igraph_rng_t* se2_rng_init(igraph_rng_t* rng, const int seed);
void se2_rng_restore(igraph_rng_t* current_rng, igraph_rng_t* previous_rng);
void se2_randperm(igraph_vector_int_t* arr, igraph_integer_t const n,
                  igraph_integer_t const m);

#endif
