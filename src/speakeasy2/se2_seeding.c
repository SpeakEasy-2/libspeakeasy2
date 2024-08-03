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
#include "se2_neighborlist.h"

igraph_integer_t se2_seeding(se2_neighs const* graph,
                             se2_options const* opts,
                             igraph_vector_int_t* ic_store)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_vector_bool_t label_seen;
  igraph_integer_t n_unique = 0;

  igraph_vector_bool_init( &label_seen, opts->target_clusters);

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    VECTOR(* ic_store)[i] = i % opts->target_clusters;
  }
  se2_randperm(ic_store, n_nodes, n_nodes);

  igraph_integer_t label = 0, biggest_label = 0;
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    label = VECTOR(* ic_store)[i];
    biggest_label = label > biggest_label ? label : biggest_label;

    if (!VECTOR(label_seen)[label]) {
      n_unique++;
      VECTOR(label_seen)[label] = true;
    }
  }
  igraph_vector_bool_destroy( &label_seen);

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    // If node's only incoming edge is a self-loop.
    if (N_NEIGHBORS(* graph, i) == 1) {
      VECTOR(* ic_store)[i] = ++biggest_label;
      n_unique++;
    }
  }

  return n_unique;
}
