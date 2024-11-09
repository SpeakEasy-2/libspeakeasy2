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

#include "se2_neighborlist.h"

#include <speak_easy_2.h>

static igraph_error_t se2_order_nodes_i(igraph_matrix_int_t const* memb,
  igraph_vector_int_t* initial, igraph_matrix_int_t* ordering,
  igraph_integer_t const level, igraph_integer_t const start,
  igraph_integer_t const len)
{
  if (len == 0) {
    return IGRAPH_SUCCESS;
  }

  if (level == igraph_matrix_int_nrow(memb)) {
    return IGRAPH_SUCCESS;
  }

  igraph_vector_int_t comm_sizes;
  igraph_vector_int_t pos;

  igraph_integer_t comm_min = IGRAPH_INTEGER_MAX;
  igraph_integer_t comm_max = 0;
  for (igraph_integer_t i = 0; i < len; i++) {
    if (MATRIX(*memb, level, VECTOR(*initial)[start + i]) < comm_min) {
      comm_min = MATRIX(*memb, level, VECTOR(*initial)[start + i]);
    }

    if (MATRIX(*memb, level, VECTOR(*initial)[start + i]) > comm_max) {
      comm_max = MATRIX(*memb, level, VECTOR(*initial)[start + i]);
    }
  }

  igraph_integer_t const n_communities = comm_max - comm_min + 1;
  IGRAPH_CHECK(igraph_vector_int_init(&comm_sizes, n_communities));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &comm_sizes);

  IGRAPH_CHECK(igraph_vector_int_init(&pos, n_communities));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &pos);

  for (igraph_integer_t i = 0; i < len; i++) {
    VECTOR(comm_sizes)
    [MATRIX(*memb, level, VECTOR(*initial)[start + i]) - comm_min]++;
  }

  igraph_vector_int_t indices;
  IGRAPH_CHECK(igraph_vector_int_init(&indices, n_communities));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &indices);
  IGRAPH_CHECK(
    igraph_vector_int_sort_ind(&comm_sizes, &indices, IGRAPH_DESCENDING));

  VECTOR(pos)[VECTOR(indices)[0]] = start;
  for (igraph_integer_t i = 1; i < n_communities; i++) {
    VECTOR(pos)
    [VECTOR(indices)[i]] =
      VECTOR(pos)[VECTOR(indices)[i - 1]] +
      VECTOR(comm_sizes)[VECTOR(indices)[i - 1]];
  }

  for (igraph_integer_t i = 0; i < len; i++) {
    igraph_integer_t comm =
      MATRIX(*memb, level, VECTOR(*initial)[start + i]) - comm_min;
    MATRIX(*ordering, level, VECTOR(pos)[comm]) = VECTOR(*initial)[start + i];
    VECTOR(pos)[comm]++;
  }

  for (igraph_integer_t i = 0; i < len; i++) {
    VECTOR(*initial)[start + i] = MATRIX(*ordering, level, start + i);
  }

  igraph_integer_t comm_start = start;
  for (igraph_integer_t i = 0; i < n_communities; i++) {
    igraph_integer_t comm_len = VECTOR(comm_sizes)[VECTOR(indices)[i]];
    se2_order_nodes_i(
      memb, initial, ordering, level + 1, comm_start, comm_len);
    comm_start += comm_len;
  }
  igraph_vector_int_destroy(&indices);
  igraph_vector_int_destroy(&pos);
  igraph_vector_int_destroy(&comm_sizes);
  IGRAPH_FINALLY_CLEAN(3);

  return IGRAPH_SUCCESS;
}

/**
\brief Return node indices of each cluster in order from largest-to-smallest
  community. This can be used to display community structure in heat maps. If
  subclustering was performed, each row of \p ordering will be the ordering for
  that level of clustering. To retain hiercharchical structure, nodes are
  reordered only within the communities of the previous level.

\param graph the graph the membership vector was created for.
\param weights the weights of the graph if weighted, otherwise set to NULL.
\param memb the membership vector.
\param ordering the resulting node ordering. A list of indices mapping the ith
  node of the graph to it's new order.

\return Error code
*/
igraph_error_t se2_order_nodes(se2_neighs const* graph,
  igraph_matrix_int_t const* memb, igraph_matrix_int_t* ordering)
{
  igraph_integer_t const n_nodes = igraph_matrix_int_ncol(memb);
  igraph_vector_t degrees;

  IGRAPH_CHECK(igraph_vector_init(&degrees, n_nodes));
  IGRAPH_FINALLY(igraph_vector_destroy, &degrees);

  IGRAPH_CHECK(
    igraph_matrix_int_init(ordering, igraph_matrix_int_nrow(memb), n_nodes));
  IGRAPH_FINALLY(igraph_matrix_int_destroy, ordering);

  IGRAPH_CHECK(se2_strength(graph, &degrees, IGRAPH_ALL));

  // Ensure nodes are ordered by highest-lowest degree within communities.
  igraph_vector_int_t init_ordering;
  IGRAPH_CHECK(igraph_vector_int_init(&init_ordering, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &init_ordering);

  IGRAPH_CHECK(
    igraph_vector_sort_ind(&degrees, &init_ordering, IGRAPH_DESCENDING));

  se2_order_nodes_i(memb, &init_ordering, ordering, 0, 0, n_nodes);

  igraph_vector_int_destroy(&init_ordering);
  igraph_vector_destroy(&degrees);
  IGRAPH_FINALLY_CLEAN(3);

  return IGRAPH_SUCCESS;
}
