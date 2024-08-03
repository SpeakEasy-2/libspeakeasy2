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

static igraph_real_t skewness(se2_neighs const* graph)
{
  if (!HASWEIGHTS(* graph)) {
    return 0;
  }

  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_integer_t const n_edges = se2_ecount(graph);
  igraph_real_t const avg = se2_total_weight(graph) / n_edges;
  igraph_real_t numerator = 0;
  igraph_real_t denominator = 0;
  igraph_real_t skew = 0;

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      igraph_real_t value = WEIGHT(* graph, i, j) - avg;
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

static void se2_mean_link_weight(se2_neighs const* graph,
                                 igraph_vector_t* diagonal_weights)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_vector_int_t signs;

  igraph_vector_int_init( &signs, n_nodes);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      igraph_integer_t nei_id = NEIGHBOR(* graph, i, j);
      VECTOR(* diagonal_weights)[nei_id] += WEIGHT(* graph, i, j);
      VECTOR(signs)[nei_id] += WEIGHT(* graph, i, j) < 0 ? -1 : 1;
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

static void se2_weigh_diagonal(se2_neighs const* graph,
                               igraph_bool_t is_skewed)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_vector_int_t diagonal_edges;

  igraph_vector_int_init( &diagonal_edges, n_nodes);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_bool_t found_edge = false;
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      if (NEIGHBOR(* graph, i, j) == i) {
        if (found_edge) { // Already found a diagonal.
          igraph_vector_int_remove( &NEIGHBORS(* graph, i), j);
          N_NEIGHBORS(* graph, i)--;
          if (HASWEIGHTS(* graph)) {
            igraph_vector_remove( &WEIGHTS_IN(* graph, i), j);
          }
        } else {
          found_edge = true;
          VECTOR(diagonal_edges)[i] = j;
          if (HASWEIGHTS(* graph)) {
            /* Importantly set to 0 so diagonal weights don't impact
            calculation of mean link weight if skewed. */
            igraph_vector_t* w = &WEIGHTS_IN(* graph, i);
            VECTOR(* w)[j] = 0;
          }
        }
      }
    }

    if (!found_edge) {
      igraph_vector_int_push_back( &NEIGHBORS(* graph, i), i);
      VECTOR(diagonal_edges)[i] = N_NEIGHBORS(* graph, i)++;
      if (HASWEIGHTS(* graph)) {
        igraph_vector_t* w = &WEIGHTS_IN(* graph, i);
        igraph_vector_resize(w, N_NEIGHBORS(* graph, i));
        VECTOR(* w)[igraph_vector_size(w) - 1] = 0;
      }
    }
  }

  if (!HASWEIGHTS(* graph)) {
    goto cleanup;
  }

  igraph_vector_t diagonal_weights;
  igraph_vector_init( &diagonal_weights, n_nodes);

  if (is_skewed) {
    se2_puts("high skew to edge weight distribution; reweighing main diag");
    se2_mean_link_weight(graph, &diagonal_weights);
  } else {
    igraph_vector_fill( &diagonal_weights, 1);
  }

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    igraph_vector_t* w = &WEIGHTS_IN(* graph, i);
    VECTOR(* w)[VECTOR(diagonal_edges)[i]] = VECTOR(diagonal_weights)[i];
  }

  igraph_vector_destroy( &diagonal_weights);

cleanup:
  igraph_vector_int_destroy( &diagonal_edges);
}

static void se2_reweigh_i(se2_neighs const* graph)
{
  if (!HASWEIGHTS(* graph)) {
    return;
  }

  igraph_real_t max_magnitude_weight = 0;
  igraph_real_t current_magnitude = 0;
  for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      current_magnitude = ABS(WEIGHT(* graph, i, j));
      if (current_magnitude > max_magnitude_weight) {
        max_magnitude_weight = current_magnitude;
      }
    }
  }

  for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
    igraph_vector_t* weight = &WEIGHTS_IN(* graph, i);
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      VECTOR(* weight)[j] /= max_magnitude_weight;
    }
  }
}

static void se2_add_offset(se2_neighs const* graph)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_real_t offset = 0;

  se2_puts("adding very small offset to all edges");

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      if (NEIGHBOR(* graph, i, j) == i) {
        offset += WEIGHT(* graph, i, j);
        break; // Already ensured there is only one self-loop per node.
      }
    }
  }
  offset /= n_nodes;

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      igraph_vector_t* w = &WEIGHTS_IN(* graph, i);
      VECTOR(* w)[j] = ((1 - offset) * VECTOR(* w)[j]) + offset;
    }
  }
}

static igraph_bool_t se2_vector_list_has_negatives(se2_neighs const* graph)
{
  for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      if (WEIGHT(* graph, i, j) < 0) {
        return true;
      }
    }
  }

  return false;
}

void se2_reweigh(se2_neighs const* graph)
{
  igraph_bool_t is_skewed = skewness(graph) >= 2;

  se2_reweigh_i(graph);
  se2_weigh_diagonal(graph, is_skewed);

  if ((is_skewed) && (!se2_vector_list_has_negatives(graph))) {
    se2_add_offset(graph);
  }
}
