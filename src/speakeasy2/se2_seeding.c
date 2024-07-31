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

#include "se2_seeding.h"
#include "se2_random.h"

static igraph_integer_t diagonal_weight(igraph_t const* graph,
                                        igraph_integer_t const node,
                                        igraph_vector_t const* weights,
                                        igraph_bool_t const directed)
{
  igraph_integer_t eid = -1;
  igraph_get_eid(graph, &eid, node, node, directed, false);

  // If eid = -1, edge not found; node does not have self-loop.
  if (eid == -1) {
    return 0;
  }

  if (!weights) {
    return 1;
  }

  if (igraph_vector_size(weights) != igraph_ecount(graph)) {
    IGRAPH_ERROR("Invalid weight vector length.", IGRAPH_EINVAL);
  }

  return VECTOR(* weights)[eid];
}

igraph_integer_t se2_seeding(igraph_t const* graph,
                             igraph_vector_t const* weights,
                             igraph_vector_t const* kin, se2_options const* opts,
                             igraph_vector_int_t* ic_store)
{
  igraph_integer_t n_nodes = igraph_vcount(graph);
  igraph_bool_t directed = igraph_is_directed(graph);
  igraph_vector_int_t unique_labels;
  igraph_integer_t n_unique = 0;

  igraph_vector_int_init( &unique_labels, opts->target_clusters);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    VECTOR(* ic_store)[i] = i % opts->target_clusters;
  }
  se2_randperm(ic_store, n_nodes, n_nodes);

  for (igraph_integer_t i = 0; i < opts->target_clusters; i++) {
    VECTOR(unique_labels)[i] = 0;
  }

  igraph_integer_t label = 0, biggest_label = 0;
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    label = VECTOR(* ic_store)[i];
    biggest_label = label > biggest_label ? label : biggest_label;

    if (VECTOR(unique_labels)[label] == 0) {
      n_unique++;
      VECTOR(unique_labels)[label] = 1;
    }
  }
  igraph_vector_int_destroy( &unique_labels);

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    if (VECTOR(* kin)[i] == diagonal_weight(graph, i, weights, directed)) {
      VECTOR(* ic_store)[i] = biggest_label + 1;
      biggest_label++;
      n_unique++;
    }
  }

  return n_unique;
}
