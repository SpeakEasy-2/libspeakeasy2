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

#include "se2_print.h"
#include "se2_reweigh_graph.h"
#include "se2_neighborlist.h"

#define ABS(a) (a) > 0 ? (a) : -(a);

static igraph_real_t se2_weight_sum(igraph_vector_list_t const* weights)
{
  igraph_real_t ret = 0;
  igraph_integer_t const n_nodes = igraph_vector_list_size(weights);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_vector_t* nei_weights = igraph_vector_list_get_ptr(weights, i);
    igraph_integer_t const n_neighs = igraph_vector_size(nei_weights);

    for (igraph_integer_t j = 0; j < n_neighs; j++) {
      ret += VECTOR(* nei_weights)[j];
    }
  }

  return ret;
}

static igraph_real_t skewness(igraph_vector_int_list_t const* graph,
                              igraph_vector_list_t const* weights)
{
  if (!weights) {
    return 0;
  }

  igraph_integer_t const n_nodes = igraph_vector_list_size(weights);
  igraph_integer_t const n_edges = se2_ecount(graph);
  igraph_real_t const avg = se2_weight_sum(weights) / n_edges;
  igraph_real_t numerator = 0;
  igraph_real_t denominator = 0;
  igraph_real_t skew = 0;

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_vector_t* nei_weights = igraph_vector_list_get_ptr(weights, i);
    igraph_integer_t const n_neighs = igraph_vector_size(nei_weights);
    for (igraph_integer_t j = 0; j < n_neighs; j++) {
      igraph_real_t value = VECTOR(* nei_weights)[j] - avg;
      igraph_real_t value_sq = value* value;
      denominator += value_sq;
      numerator += value* value_sq;
    }
  }
  denominator = sqrt((double)denominator);
  denominator = denominator* denominator* denominator;

  skew = (numerator / n_edges) / denominator;
  skew /= sqrt((double)n_edges);

  return skew;
}

static void se2_mean_link_weight(igraph_vector_int_list_t const* graph,
                                 igraph_vector_list_t const* weights,
                                 igraph_vector_t* diagonal_weights)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_vector_int_t signs;

  igraph_vector_int_init( &signs, n_nodes);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_vector_int_t neighs = VECTOR(* graph)[i];
    for (igraph_integer_t j = 0; j < igraph_vector_int_size( &neighs); j++) {
      igraph_integer_t nei_id = VECTOR(neighs)[j];
      VECTOR(* diagonal_weights)[nei_id] += LIST(* weights, i, nei_id);
      VECTOR(signs)[nei_id] += LIST(* weights, i, nei_id) < 0 ? -1 : 1;
    }
  }

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    if (VECTOR(signs)[i] == 0) {
      continue;
    }

    VECTOR(* diagonal_weights)[i] /= VECTOR(signs)[i];
  }

  igraph_vector_int_destroy( &signs);
}

static void se2_weigh_diagonal(igraph_vector_int_list_t const* graph,
                               igraph_vector_list_t const* weights,
                               igraph_bool_t is_skewed)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_vector_int_t diagonal_edges;

  igraph_vector_int_init( &diagonal_edges, n_nodes);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_bool_t found_edge = false;
    igraph_vector_int_t* neighs = &VECTOR(* graph)[i];
    for (igraph_integer_t j = 0; j < igraph_vector_int_size(neighs); j++) {
      if (VECTOR(* neighs)[j] == i) {
        if (found_edge) { // Already found a diagonal.
          igraph_vector_int_remove(neighs, j);
          if (weights) {
            igraph_vector_remove( &VECTOR(* weights)[i], j);
          }
        } else {
          found_edge = true;
          VECTOR(diagonal_edges)[i] = j;
          if (weights) {
            /* Importantly set to 0 so diagonal weights don't impact
            calculation of mean link weight if skewed. */
            LIST(* weights, i, j) = 0;
          }
        }
      }
    }

    if (!found_edge) {
      igraph_vector_int_push_back(neighs, i);
      VECTOR(diagonal_edges)[i] = igraph_vector_int_size(neighs) - 1;
      if (weights) {
        igraph_vector_t* w = &VECTOR(* weights)[i];
        igraph_vector_resize(w, igraph_vector_size(w) + 1);
        VECTOR(* w)[igraph_vector_size(w) - 1] = 0;
      }
    }
  }

  if (!weights) {
    goto cleanup;
  }

  igraph_vector_t diagonal_weights;
  igraph_vector_init( &diagonal_weights, n_nodes);

  if (is_skewed) {
    se2_puts("high skew to edge weight distribution; reweighing main diag");
    se2_mean_link_weight(graph, weights, &diagonal_weights);
  } else {
    igraph_vector_fill( &diagonal_weights, 1);
  }

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    LIST(* weights, i, VECTOR(diagonal_edges)[i]) = VECTOR(diagonal_weights)[i];
  }

  igraph_vector_destroy( &diagonal_weights);

cleanup:
  igraph_vector_int_destroy( &diagonal_edges);
}

static void se2_reweigh_i(igraph_vector_int_list_t const* graph,
                          igraph_vector_list_t const* weights)
{
  if (!weights) {
    return;
  }

  igraph_real_t max_magnitude_weight = 0;
  igraph_real_t current_magnitude = 0;
  for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
    igraph_vector_t nei_weights = VECTOR(* weights)[i];
    for (igraph_integer_t j = 0; j < igraph_vector_size( &nei_weights); j++) {
      current_magnitude = ABS(VECTOR(nei_weights)[j]);
      if (current_magnitude > max_magnitude_weight) {
        max_magnitude_weight = current_magnitude;
      }
    }
  }

  for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
    igraph_vector_t nei_weights = VECTOR(* weights)[i];
    for (igraph_integer_t j = 0; j < igraph_vector_size( &nei_weights); j++) {
      VECTOR(nei_weights)[j] /= max_magnitude_weight;
    }
  }
}

static void se2_add_offset(igraph_vector_int_list_t const* graph,
                           igraph_vector_list_t const* weights)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_real_t offset = 0;

  se2_puts("adding very small offset to all edges");

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_vector_int_t neighs = VECTOR(* graph)[i];
    for (igraph_integer_t j = 0; j < igraph_vector_int_size( &neighs); j++) {
      if (VECTOR(neighs)[j] == i) {
        offset += LIST(* weights, i, j);
        break; // Already ensured there is only one self-loop per node.
      }
    }
  }
  offset /= n_nodes;

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_vector_int_t neighs = VECTOR(* graph)[i];
    for (igraph_integer_t j = 0; j < igraph_vector_int_size( &neighs); j++) {
      LIST(* weights, i, j) = ((1 - offset) * LIST(* weights, i, j)) + offset;
    }
  }
}

static igraph_bool_t se2_vector_list_has_negatives(igraph_vector_list_t const*
    ls)
{
  for (igraph_integer_t i = 0; i < igraph_vector_list_size(ls); i++) {
    igraph_vector_t vec = VECTOR(* ls)[i];
    for (igraph_integer_t j = 0; j < igraph_vector_size( &vec); j++) {
      if (VECTOR(vec)[j] < 0) {
        return true;
      }
    }
  }

  return false;
}

void se2_reweigh(igraph_vector_int_list_t const* graph,
                 igraph_vector_list_t const* weights)
{
  igraph_bool_t is_skewed = skewness(graph, weights) >= 2;

  se2_reweigh_i(graph, weights);
  se2_weigh_diagonal(graph, weights, is_skewed);

  if ((is_skewed) && (!se2_vector_list_has_negatives(weights))) {
    se2_add_offset(graph, weights);
  }
}
