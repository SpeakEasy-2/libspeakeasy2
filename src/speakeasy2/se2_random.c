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

#include "se2_random.h"

#include "se2_error_handling.h"

/* Initializes default igraph random number generator to use twister method */
igraph_error_t se2_rng_init(
  igraph_rng_t* rng, igraph_rng_t* old_rng, int const seed)
{
  old_rng = igraph_rng_default();

  SE2_THREAD_CHECK(igraph_rng_init(rng, &igraph_rngtype_mt19937));
  igraph_rng_set_default(rng);
  igraph_rng_seed(igraph_rng_default(), seed);

  return IGRAPH_SUCCESS;
}

/* Shuffle the first m elements of the n element vector arr */
void se2_randperm(
  igraph_vector_int_t* arr, igraph_integer_t const n, igraph_integer_t const m)
{
  igraph_integer_t swap = 0;
  igraph_integer_t idx = 0;
  for (igraph_integer_t i = 0; i < m; i++) {
    idx = RNG_INTEGER(0, n - 1);
    swap = VECTOR(*arr)[i];
    VECTOR(*arr)[i] = VECTOR(*arr)[idx];
    VECTOR(*arr)[idx] = swap;
  }

  return;
}
