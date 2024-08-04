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

#include <stdlib.h>
#include <speak_easy_2.h>
#include "se2_neighborlist.h"

/* Convert an igraph graph to a list of neighbor lists where the ith vector
   contains a list of the ith node's neighbors.

   If the graph is weighted, a weight list will be returned in the same
   structure as the neighbor list. If the graph is not weighted the two weight
   arguments should be set to NULL.

   If the graph is directed, the neighbors are the neighbors coming in to the
   current node.

   When finished the SpeakEasy 2 algorithm no longer needs the graph so it is
   safe to delete the graph (and it's weight vector) unless they are needed
   elsewhere.
 */
igraph_error_t se2_igraph_to_neighbor_list(igraph_t const* graph,
    igraph_vector_t const* weights, se2_neighs* neigh_list)
{
  igraph_integer_t const n_nodes = igraph_vcount(graph);

  neigh_list->neigh_list = malloc(sizeof(* neigh_list->neigh_list));
  IGRAPH_FINALLY(free, neigh_list->neigh_list);
  neigh_list->weights = weights ? malloc(sizeof(* neigh_list->weights)) : NULL;
  if (neigh_list->weights) {
    IGRAPH_FINALLY(free, neigh_list->weights);
  }
  neigh_list->sizes = malloc(sizeof(* neigh_list->sizes));
  IGRAPH_FINALLY(free, neigh_list->sizes);

  neigh_list->n_nodes = n_nodes;
  IGRAPH_CHECK(igraph_vector_int_init(neigh_list->sizes, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, neigh_list->sizes);
  IGRAPH_CHECK(igraph_vector_int_list_init(neigh_list->neigh_list, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_list_destroy, neigh_list->neigh_list);
  for (igraph_integer_t node_id = 0; node_id < n_nodes; node_id++) {
    igraph_vector_int_t* neighbors = &VECTOR(* neigh_list->neigh_list)[node_id];
    igraph_neighbors(graph, neighbors, node_id, IGRAPH_IN);
    VECTOR(* neigh_list->sizes)[node_id] = igraph_vector_int_size(neighbors);
  }

  if (!weights) {
    goto cleanup;
  }

  igraph_bool_t const directed = igraph_is_directed(graph);
  IGRAPH_CHECK(igraph_vector_list_init(neigh_list->weights, n_nodes));
  IGRAPH_FINALLY(igraph_vector_list_destroy, neigh_list->weights);
  for (igraph_integer_t node_id = 0; node_id < n_nodes; node_id++) {
    igraph_vector_int_t neighbors = VECTOR(* neigh_list->neigh_list)[node_id];
    igraph_integer_t const n_neighbors = igraph_vector_int_size( &neighbors);
    igraph_vector_t* neigh_weights = &VECTOR(* neigh_list->weights)[node_id];
    igraph_vector_resize(neigh_weights, n_neighbors);

    for (igraph_integer_t i = 0; i < n_neighbors; i++) {
      igraph_integer_t eid, neigh_id = VECTOR(neighbors)[i];
      igraph_get_eid(graph, &eid, neigh_id, node_id, directed, false);
      VECTOR(* neigh_weights)[i] = VECTOR(* weights)[eid];
    }
  }
  IGRAPH_FINALLY_CLEAN(2);

cleanup:
  IGRAPH_FINALLY_CLEAN(4);
  return IGRAPH_SUCCESS;
}

void se2_neighs_destroy(se2_neighs* graph)
{
  igraph_vector_int_list_destroy(graph->neigh_list);
  free(graph->neigh_list);
  if (HASWEIGHTS(* graph)) {
    igraph_vector_list_destroy(graph->weights);
    free(graph->weights);
  }
  igraph_vector_int_destroy(graph->sizes);
  free(graph->sizes);
}

/* Return the number of nodes in the graph represented by \p graph. */
igraph_integer_t se2_vcount(se2_neighs const* graph)
{
  return graph->n_nodes;
}

/* Return the number of edges in the graph represented by \p graph. */
igraph_integer_t se2_ecount(se2_neighs const* graph)
{
  return igraph_vector_int_sum(graph->sizes);
}

igraph_real_t se2_total_weight(se2_neighs const* graph)
{
  igraph_real_t ret = 0;
  for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
    ret += igraph_vector_sum( &VECTOR(* graph->weights)[i]);
  }

  return ret;
}

static void se2_strength_in_i(se2_neighs const* graph,
                              igraph_vector_t* degrees)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    if (HASWEIGHTS(* graph)) {
      VECTOR(* degrees)[i] = igraph_vector_sum( &WEIGHTS_IN(* graph, i));
    } else {
      VECTOR(* degrees)[i] = N_NEIGHBORS(* graph, i);
    }
  }
}

static void se2_strength_out_i(se2_neighs const* graph,
                               igraph_vector_t* degrees)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      VECTOR(* degrees)[NEIGHBOR(* graph, i, j)] += WEIGHT(* graph, i, j);
    }
  }
}

igraph_error_t se2_strength(se2_neighs const* graph,
                            igraph_vector_t* degrees,
                            igraph_neimode_t mode)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  if (igraph_vector_size(degrees) != n_nodes) {
    igraph_vector_resize(degrees, n_nodes);
  }
  igraph_vector_null(degrees);

  if ((mode == IGRAPH_IN) || (mode == IGRAPH_ALL)) {
    se2_strength_in_i(graph, degrees);
  }

  if ((mode == IGRAPH_OUT) || (mode == IGRAPH_ALL)) {
    se2_strength_out_i(graph, degrees);
  }

  return IGRAPH_SUCCESS;
}
