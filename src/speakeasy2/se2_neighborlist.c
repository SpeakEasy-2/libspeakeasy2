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
    igraph_vector_t const* weights, igraph_vector_int_list_t* neigh_list,
    igraph_vector_list_t* weight_list)
{
  igraph_integer_t const n_nodes = igraph_vcount(graph);
  igraph_error_t errcode = IGRAPH_SUCCESS;

  igraph_vector_int_list_init(neigh_list, n_nodes);
  for (igraph_integer_t node_id = 0; node_id < n_nodes; node_id++) {
    igraph_vector_int_t* neighbors = &VECTOR(* neigh_list)[node_id];
    igraph_neighbors(graph, neighbors, node_id, IGRAPH_IN);
  }

  if (!weights) {
    return errcode;
  }

  igraph_bool_t const directed = igraph_is_directed(graph);
  igraph_vector_list_init(weight_list, n_nodes);
  for (igraph_integer_t node_id = 0; node_id < n_nodes; node_id++) {
    igraph_vector_int_t neighbors = VECTOR(* neigh_list)[node_id];
    igraph_integer_t const n_neighbors = igraph_vector_int_size( &neighbors);
    igraph_vector_t* neigh_weights = &VECTOR(* weight_list)[node_id];
    igraph_vector_resize(neigh_weights, n_neighbors);

    for (igraph_integer_t i = 0; i < n_neighbors; i++) {
      igraph_integer_t eid, neigh_id = VECTOR(neighbors)[i];
      igraph_get_eid(graph, &eid, neigh_id, node_id, directed, false);
      VECTOR(* neigh_weights)[i] = VECTOR(* weights)[eid];
    }
  }

  return errcode;
}

/* Return the number of nodes in the graph represented by \a neigh_list. */
igraph_integer_t se2_vcount(igraph_vector_int_list_t const* neigh_list)
{
  return igraph_vector_int_list_size(neigh_list);
}

/* Return the number of edges in the graph represented by \a neigh_list. */
igraph_integer_t se2_ecount(igraph_vector_int_list_t const* neigh_list)
{
  igraph_integer_t ret = 0;
  for (igraph_integer_t i = 0; i < se2_vcount(neigh_list); i++) {
    ret += igraph_vector_int_size( &VECTOR(* neigh_list)[i]);
  }

  return ret;
}

igraph_real_t se2_total_weight(igraph_vector_list_t const* weights)
{
  igraph_real_t ret = 0;
  for (igraph_integer_t i = 0; i < igraph_vector_list_size(weights); i++) {
    ret += igraph_vector_sum( &VECTOR(* weights)[i]);
  }

  return ret;
}

static void se2_strength_in_i(igraph_vector_int_list_t const* neigh_list,
                              igraph_vector_list_t const* weights,
                              igraph_vector_t* degrees)
{
  igraph_integer_t const n_nodes = se2_vcount(neigh_list);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    if (weights) {
      VECTOR(* degrees)[i] = igraph_vector_sum( &VECTOR(* weights)[i]);
    } else {
      VECTOR(* degrees)[i] = igraph_vector_int_size( &VECTOR(* neigh_list)[i]);
    }
  }
}

static void se2_strength_out_i(igraph_vector_int_list_t const* neigh_list,
                               igraph_vector_list_t const* weights,
                               igraph_vector_t* degrees)
{
  igraph_integer_t const n_nodes = se2_vcount(neigh_list);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_vector_int_t neighs = VECTOR(* neigh_list)[i];
    igraph_integer_t const n_neighs = igraph_vector_int_size( &neighs);
    for (igraph_integer_t j = 0; j < n_neighs; j++) {
      VECTOR(* degrees)[VECTOR(neighs)[j]] += weights ?
                                              LIST(* weights, i, j) : 1;
    }
  }
}

igraph_error_t se2_strength(igraph_vector_int_list_t const* neigh_list,
                            igraph_vector_list_t const* weights,
                            igraph_vector_t* degrees,
                            igraph_neimode_t mode)
{
  igraph_integer_t const n_nodes = se2_vcount(neigh_list);
  if (igraph_vector_size(degrees) != n_nodes) {
    igraph_vector_resize(degrees, n_nodes);
  }
  igraph_vector_null(degrees);

  if ((mode == IGRAPH_IN) || (mode == IGRAPH_ALL)) {
    se2_strength_in_i(neigh_list, weights, degrees);
  }

  if ((mode == IGRAPH_OUT) || (mode == IGRAPH_ALL)) {
    se2_strength_out_i(neigh_list, weights, degrees);
  }

  return IGRAPH_SUCCESS;
}
