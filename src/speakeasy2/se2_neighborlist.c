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

/* Convert an igraph graph to a list of neighbor lists where the ith vector
   contains a list of the ith node's neighbors.

   If the graph is weighted, a weight list will be returned in the same
   structure as the neighbor list. If the graph is not weighted the two weight
   arguments should be set to NULL.

   If the graph is directed, the neighbors are the neighbors out from the node.

   When finished the SpeakEasy 2 algorithm no longer needs the graph so it is
   safe to delete the graph (and it's weight vector) unless they are needed
   elsewhere.
 */
igraph_error_t se2_igraph_to_neighbor_list(igraph_t const* graph,
  igraph_vector_t const* weights, se2_neighs* neigh_list)
{
  igraph_integer_t const n_nodes = igraph_vcount(graph);
  igraph_bool_t const directed = igraph_is_directed(graph);
  igraph_bool_t const is_sparse = igraph_ecount(graph) != (n_nodes * n_nodes);

  neigh_list->n_nodes = n_nodes;
  neigh_list->total_weight = 0;

  if (is_sparse) {
    neigh_list->neigh_list = igraph_malloc(sizeof(*neigh_list->neigh_list));
    IGRAPH_CHECK_OOM(neigh_list->neigh_list, "");
    IGRAPH_FINALLY(igraph_free, neigh_list->neigh_list);
    IGRAPH_CHECK(igraph_vector_int_list_init(neigh_list->neigh_list, n_nodes));
    IGRAPH_FINALLY(igraph_vector_int_list_destroy, neigh_list->neigh_list);

    neigh_list->sizes = igraph_malloc(sizeof(*neigh_list->sizes));
    IGRAPH_CHECK_OOM(neigh_list->sizes, "");
    IGRAPH_FINALLY(igraph_free, neigh_list->sizes);
    IGRAPH_CHECK(igraph_vector_int_init(neigh_list->sizes, n_nodes));
    IGRAPH_FINALLY(igraph_vector_int_destroy, neigh_list->sizes);
  } else {
    neigh_list->neigh_list = NULL;
    neigh_list->sizes = NULL;
  }

  neigh_list->kin = igraph_malloc(sizeof(*neigh_list->kin));
  IGRAPH_CHECK_OOM(neigh_list->kin, "");
  IGRAPH_FINALLY(igraph_free, neigh_list->kin);
  IGRAPH_CHECK(igraph_vector_init(neigh_list->kin, n_nodes));
  IGRAPH_FINALLY(igraph_vector_destroy, neigh_list->kin);

  if (weights) {
    neigh_list->weights = igraph_malloc(sizeof(*neigh_list->weights));
    IGRAPH_CHECK_OOM(neigh_list->weights, "");
    IGRAPH_FINALLY(igraph_free, neigh_list->weights);

    IGRAPH_CHECK(igraph_vector_list_init(neigh_list->weights, n_nodes));
    IGRAPH_FINALLY(igraph_vector_list_destroy, neigh_list->weights);
  } else {
    neigh_list->weights = NULL;
  }

  if (is_sparse) {
    for (igraph_integer_t eid = 0; eid < igraph_ecount(graph); eid++) {
      VECTOR(*neigh_list->sizes)[IGRAPH_FROM(graph, eid)]++;
      if (!directed) {
        VECTOR(*neigh_list->sizes)[IGRAPH_TO(graph, eid)]++;
      }
    }

    for (igraph_integer_t node_id = 0; node_id < n_nodes; node_id++) {
      igraph_vector_int_t* neighbors =
        &VECTOR(*neigh_list->neigh_list)[node_id];
      IGRAPH_CHECK(igraph_vector_int_resize(
        neighbors, N_NEIGHBORS(*neigh_list, node_id)));
    }
  }

  if (weights) {
    for (igraph_integer_t node_id = 0; node_id < n_nodes; node_id++) {
      igraph_vector_t* w = &VECTOR(*neigh_list->weights)[node_id];
      IGRAPH_CHECK(igraph_vector_resize(w, N_NEIGHBORS(*neigh_list, node_id)));
    }
  }

  igraph_vector_int_t neigh_counts;
  IGRAPH_CHECK(igraph_vector_int_init(&neigh_counts, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &neigh_counts);

  for (igraph_integer_t eid = 0; eid < igraph_ecount(graph); eid++) {
    igraph_integer_t const from = IGRAPH_FROM(graph, eid);
    igraph_integer_t const to = IGRAPH_TO(graph, eid);
    igraph_integer_t neigh_pos = VECTOR(neigh_counts)[from];

    if (is_sparse) {
      igraph_vector_int_t* neighbors = &VECTOR(*neigh_list->neigh_list)[from];
      VECTOR(*neighbors)[neigh_pos] = to;
    }

    if (weights) {
      igraph_vector_t* w = &VECTOR(*neigh_list->weights)[from];
      VECTOR(*w)[neigh_pos] = VECTOR(*weights)[eid];
      neigh_list->total_weight += VECTOR(*weights)[eid];
    }

    VECTOR(neigh_counts)[from]++;

    if (directed) {
      continue;
    }

    neigh_pos = VECTOR(neigh_counts)[to];

    if (is_sparse) {
      igraph_vector_int_t* neighbors = &VECTOR(*neigh_list->neigh_list)[to];
      VECTOR(*neighbors)[neigh_pos] = from;
    }

    if (weights) {
      igraph_vector_t* w = &VECTOR(*neigh_list->weights)[to];
      VECTOR(*w)[neigh_pos] = VECTOR(*weights)[eid];
      neigh_list->total_weight += VECTOR(*weights)[eid];
    }

    VECTOR(neigh_counts)[to]++;
  }

  igraph_vector_int_destroy(&neigh_counts);
  IGRAPH_FINALLY_CLEAN(1);

  if (is_sparse) {
    IGRAPH_FINALLY_CLEAN(4);
  }

  IGRAPH_FINALLY_CLEAN(2);

  if (weights) {
    IGRAPH_FINALLY_CLEAN(2);
  } else {
    neigh_list->total_weight =
      is_sparse ? igraph_vector_int_sum(neigh_list->sizes) : n_nodes * n_nodes;
  }

  return IGRAPH_SUCCESS;
}

void se2_neighs_destroy(se2_neighs* graph)
{
  if (ISSPARSE(*graph)) {
    igraph_vector_int_list_destroy(graph->neigh_list);
    igraph_free(graph->neigh_list);
    igraph_vector_int_destroy(graph->sizes);
    igraph_free(graph->sizes);
  }

  if (HASWEIGHTS(*graph)) {
    igraph_vector_list_destroy(graph->weights);
    igraph_free(graph->weights);
  }

  igraph_vector_destroy(graph->kin);
  igraph_free(graph->kin);
}

/* Return the number of nodes in the graph represented by \p graph. */
igraph_integer_t se2_vcount(se2_neighs const* graph) { return graph->n_nodes; }

/* Return the number of edges in the graph represented by \p graph. */
igraph_integer_t se2_ecount(se2_neighs const* graph)
{
  return ISSPARSE(*graph) ?
           igraph_vector_int_sum(graph->sizes) :
           graph->n_nodes * graph->n_nodes;
}

igraph_real_t se2_total_weight(se2_neighs const* graph)
{
  return graph->total_weight;
}

static igraph_error_t se2_strength_in_i(
  se2_neighs const* graph, igraph_vector_t* degrees)
{
  IGRAPH_CHECK(igraph_vector_update(degrees, graph->kin));
  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_strength_out_i(
  se2_neighs const* graph, igraph_vector_t* degrees)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    if (HASWEIGHTS(*graph)) {
      VECTOR(*degrees)[i] += igraph_vector_sum(&WEIGHTS_IN(*graph, i));
    } else {
      VECTOR(*degrees)[i] += N_NEIGHBORS(*graph, i);
    }
  }

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_strength(
  se2_neighs const* graph, igraph_vector_t* degrees, igraph_neimode_t mode)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  if (igraph_vector_size(degrees) != n_nodes) {
    IGRAPH_CHECK(igraph_vector_resize(degrees, n_nodes));
  }
  igraph_vector_null(degrees);

  if ((mode == IGRAPH_IN) || (mode == IGRAPH_ALL)) {
    IGRAPH_CHECK(se2_strength_in_i(graph, degrees));
  }

  if ((mode == IGRAPH_OUT) || (mode == IGRAPH_ALL)) {
    IGRAPH_CHECK(se2_strength_out_i(graph, degrees));
  }

  return IGRAPH_SUCCESS;
}
