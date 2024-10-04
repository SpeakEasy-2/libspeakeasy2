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

#ifndef SE2_NEIGHBORLIST_H
#define SE2_NEIGHBORLIST_H

#include <speak_easy_2.h>

/* Return the jth element of the ith list. */
#define NEIGHBOR(a, i, j) (VECTOR(VECTOR(*(a).neigh_list)[(i)])[(j)])
#define NEIGHBORS(a, i) (VECTOR(*(a).neigh_list)[(i)])
#define WEIGHT(a, i, j)                                                       \
  ((a).weights ? VECTOR(VECTOR(*(a).weights)[(i)])[(j)] : 1)
#define WEIGHTS_IN(a, i) (VECTOR(*(a).weights)[(i)])
#define N_NEIGHBORS(a, i) (VECTOR(*(a).sizes)[(i)])
#define HASWEIGHTS(a) ((a).weights ? true : false)

igraph_integer_t se2_vcount(se2_neighs const* graph);
igraph_integer_t se2_ecount(se2_neighs const* graph);
igraph_real_t se2_total_weight(se2_neighs const* graph);
igraph_error_t se2_strength(se2_neighs const* neigh_list,
  igraph_vector_t* degrees, igraph_neimode_t const mode);

#endif
