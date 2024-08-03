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

#include <igraph.h>
#include "se2_label.h"
#include "se2_neighborlist.h"

static void global_label_proportions(
  se2_neighs const* graph,
  se2_partition const* partition,
  igraph_vector_t* labels_heard,
  igraph_integer_t const n_labels)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_integer_t acc = 0;

  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      igraph_integer_t const neighbor = NEIGHBOR(* graph, i, j);
      igraph_integer_t const label = LABEL(* partition)[neighbor];
      VECTOR(* labels_heard)[label] += WEIGHT(* graph, i, j);
    }
  }

  for (igraph_integer_t i = 0; i < n_labels; i++) {
    acc += VECTOR(* labels_heard)[i];
  }

  for (igraph_integer_t i = 0; i < n_labels; i++) {
    VECTOR(* labels_heard)[i] /= acc;
  }
}

static void local_label_proportions(
  se2_neighs const* graph,
  se2_partition const* partition,
  igraph_integer_t const node_id,
  igraph_vector_t* labels_heard,
  igraph_real_t* kin,
  igraph_integer_t n_labels)
{
  for (igraph_integer_t i = 0; i < N_NEIGHBORS(* graph, node_id); i++) {
    igraph_integer_t const neighbor = NEIGHBOR(* graph, node_id, i);
    igraph_integer_t const label = LABEL(* partition)[neighbor];
    VECTOR(* labels_heard)[label] += WEIGHT(* graph, node_id, i);
  }

  *kin = 0;
  for (igraph_integer_t i = 0; i < n_labels; i++) {
    *kin += VECTOR(* labels_heard)[i];
  }
}

/* Scores labels based on the difference between the local and global
 frequencies.  Labels that are overrepresented locally are likely to be of
 importance in tagging a node. */
static void se2_find_most_specific_labels_i(se2_neighs const* graph,
    se2_partition* partition, se2_iterator* node_iter)
{
  igraph_integer_t max_label = se2_partition_max_label(partition);
  igraph_vector_t labels_expected;
  igraph_vector_t labels_observed;
  se2_iterator* label_iter = se2_iterator_random_label_init(partition, false);
  igraph_real_t node_kin = 0;
  igraph_real_t label_specificity = 0, best_label_specificity = 0;
  igraph_integer_t best_label = -1;

  igraph_vector_init( &labels_expected, max_label + 1);
  igraph_vector_init( &labels_observed, max_label + 1);

  global_label_proportions(graph, partition, &labels_expected, max_label + 1);

  igraph_integer_t node_id = 0, label_id = 0;
  while ((node_id = se2_iterator_next(node_iter)) != -1) {
    igraph_vector_null( &labels_observed);
    local_label_proportions(graph, partition, node_id, &labels_observed,
                            &node_kin, max_label + 1);

    while ((label_id = se2_iterator_next(label_iter)) != -1) {
      label_specificity = VECTOR(labels_observed)[label_id] -
                          (node_kin* VECTOR(labels_expected)[label_id]);
      if ((best_label == -1) || (label_specificity >= best_label_specificity)) {
        best_label_specificity = label_specificity;
        best_label = label_id;
      }
    }

    se2_partition_add_to_stage(partition, node_id, best_label,
                               best_label_specificity);
    best_label = -1;
    se2_iterator_shuffle(label_iter);
  }
  se2_partition_commit_changes(partition);

  se2_iterator_destroy(label_iter);
  igraph_vector_destroy( &labels_expected);
  igraph_vector_destroy( &labels_observed);
}

void se2_find_most_specific_labels(se2_neighs const* graph,
                                   se2_partition* partition,
                                   igraph_real_t const fraction_nodes_to_label)
{
  se2_iterator* node_iter = se2_iterator_random_node_init(partition,
                            fraction_nodes_to_label);
  se2_find_most_specific_labels_i(graph, partition, node_iter);
  se2_iterator_destroy(node_iter);
}

void se2_relabel_worst_nodes(se2_neighs const* graph,
                             se2_partition* partition,
                             igraph_real_t const fraction_nodes_to_label)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  igraph_integer_t const n_nodes_to_relabel = fraction_nodes_to_label* n_nodes;
  se2_iterator* node_iter = se2_iterator_k_worst_fit_nodes_init(partition,
                            n_nodes_to_relabel);

  se2_find_most_specific_labels_i(graph, partition, node_iter);
  se2_iterator_destroy(node_iter);
}

void se2_burst_large_communities(se2_partition* partition,
                                 igraph_real_t const fraction_nodes_to_move,
                                 igraph_integer_t const min_community_size)
{
  se2_iterator* node_iter = se2_iterator_k_worst_fit_nodes_init(partition,
                            partition->n_nodes* fraction_nodes_to_move);
  igraph_integer_t desired_community_size =
    se2_partition_median_community_size(partition);
  igraph_vector_int_t n_new_tags_cum;
  igraph_vector_int_t n_nodes_to_move;
  igraph_vector_int_t new_tags;
  igraph_integer_t node_id;

  igraph_vector_int_init( &n_new_tags_cum, partition->max_label + 2);
  igraph_vector_int_init( &n_nodes_to_move, partition->max_label + 1);
  while ((node_id = se2_iterator_next(node_iter)) != -1) {
    if (se2_partition_community_size(partition, LABEL(* partition)[node_id]) >=
        min_community_size) {
      VECTOR(n_nodes_to_move)[LABEL(* partition)[node_id]]++;
    }
  }

  igraph_integer_t n_new_tags;
  for (igraph_integer_t i = 0; i <= partition->max_label; i++) {
    if (VECTOR(n_nodes_to_move)[i] == 0) {
      continue;
    }

    n_new_tags = VECTOR(n_nodes_to_move)[i] / desired_community_size;
    if (n_new_tags < 2) {
      n_new_tags = 2;
    } else if (n_new_tags > 10) {
      n_new_tags = 10;
    }

    VECTOR(n_new_tags_cum)[i + 1] = n_new_tags;
  }

  for (igraph_integer_t i = 0; i <= partition->max_label; i++) {
    VECTOR(n_new_tags_cum)[i + 1] += VECTOR(n_new_tags_cum)[i];
  }

  n_new_tags = VECTOR(n_new_tags_cum)[partition->max_label + 1];

  igraph_vector_int_init( &new_tags, n_new_tags);
  for (igraph_integer_t i = 0; i < n_new_tags; i++) {
    VECTOR(new_tags)[i] = se2_partition_new_label(partition);
  }

  igraph_integer_t current_label;
  while ((node_id = se2_iterator_next(node_iter)) != -1) {
    current_label = LABEL(* partition)[node_id];
    if (se2_partition_community_size(partition, current_label) >=
        min_community_size) {
      RELABEL(* partition)[node_id] =
        VECTOR(new_tags)[RNG_INTEGER(VECTOR(n_new_tags_cum)[current_label],
                                     VECTOR(n_new_tags_cum)[current_label + 1] - 1)];
    }
  }

  igraph_vector_int_destroy( &new_tags);
  igraph_vector_int_destroy( &n_nodes_to_move);
  igraph_vector_int_destroy( &n_new_tags_cum);
  se2_iterator_destroy(node_iter);

  se2_partition_commit_changes(partition);
}

/* For each community, find the communities that would cause the greatest
increase in modularity if merged.

merge_candidates: a vector of indices where each value is the best community to
merge with the ith community.
modularity_change: a vector of how much the modularity would change if the
corresponding merge_candidates were combined.

modularity_change is capped to be always non-negative. */
static void se2_best_merges(se2_neighs const* graph,
                            se2_partition const* partition,
                            igraph_vector_int_t* merge_candidates,
                            igraph_vector_t* modularity_change,
                            igraph_integer_t const n_labels)
{
  igraph_real_t const total_weight = HASWEIGHTS(* graph) ?
                                     se2_total_weight(graph) :
                                     se2_ecount(graph);
  igraph_matrix_t crosstalk;
  igraph_vector_t from_edge_probability;
  igraph_vector_t to_edge_probability;

  igraph_matrix_init( &crosstalk, n_labels, n_labels);
  igraph_vector_init( &from_edge_probability, n_labels);
  igraph_vector_init( &to_edge_probability, n_labels);

  igraph_vector_int_fill(merge_candidates, -1);

  for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(* graph, i); j++) {
      MATRIX(crosstalk,
             LABEL(* partition)[NEIGHBOR(* graph, i, j)],
             LABEL(* partition)[i]) += WEIGHT(* graph, i, j);
    }
  }

  for (igraph_integer_t i = 0; i < n_labels; i++) {
    for (igraph_integer_t j = 0; j < n_labels; j++) {
      MATRIX(crosstalk, i, j) /= total_weight;
    }
  }

  igraph_matrix_rowsum( &crosstalk, &from_edge_probability);
  igraph_matrix_colsum( &crosstalk, &to_edge_probability);

  igraph_real_t modularity_delta;
  for (igraph_integer_t i = 0; i < n_labels; i++) {
    for (igraph_integer_t j = (i + 1); j < n_labels; j++) {
      modularity_delta =
        MATRIX(crosstalk, i, j) + MATRIX(crosstalk, j, i) -
        (VECTOR(from_edge_probability)[i] * VECTOR(to_edge_probability)[j]) -
        (VECTOR(from_edge_probability)[j] * VECTOR(to_edge_probability)[i]);

      if (modularity_delta > VECTOR(* modularity_change)[i]) {
        VECTOR(* modularity_change)[i] = modularity_delta;
        VECTOR(* merge_candidates)[i] = j;
      }

      if (modularity_delta > VECTOR(* modularity_change)[j]) {
        VECTOR(* modularity_change)[j] = modularity_delta;
        VECTOR(* merge_candidates)[j] = i;
      }
    }
  }

  igraph_matrix_destroy( &crosstalk);
  igraph_vector_destroy( &from_edge_probability);
  igraph_vector_destroy( &to_edge_probability);
}

/* Since used labels are not necessarily sequential, modularity change can be
larger than the number of labels in use. To get the median, have to find
elements of modularity change corresponding to labels in use.*/
igraph_real_t se2_modularity_median(se2_partition* partition,
                                    igraph_vector_t* modularity_change)
{
  igraph_vector_t modularity_change_without_gaps;
  se2_iterator* label_iter = se2_iterator_random_label_init(partition, 0);
  igraph_real_t res;

  igraph_vector_init( &modularity_change_without_gaps, partition->n_labels);

  igraph_integer_t label_id = 0;
  igraph_integer_t label_i = 0;
  while ((label_id = se2_iterator_next(label_iter)) != -1) {
    VECTOR(modularity_change_without_gaps)[label_i] = VECTOR(
        *modularity_change)[label_id];
    label_i++;
  }

  res = se2_vector_median( &modularity_change_without_gaps);

  igraph_vector_destroy( &modularity_change_without_gaps);
  se2_iterator_destroy(label_iter);

  return res;
}

igraph_bool_t se2_merge_well_connected_communities(se2_neighs const* graph,
    se2_partition* partition, igraph_real_t* max_prev_merge_threshold)
{
  igraph_integer_t max_label = se2_partition_max_label(partition);
  igraph_vector_int_t merge_candidates;
  igraph_vector_t modularity_change;
  igraph_real_t min_merge_improvement;
  igraph_real_t median_modularity_change;
  igraph_integer_t n_positive_changes = 0;
  igraph_integer_t n_merges = 0;

  igraph_vector_int_init( &merge_candidates, max_label + 1);
  igraph_vector_init( &modularity_change, max_label + 1);

  se2_best_merges(graph, partition, &merge_candidates,
                  &modularity_change,
                  max_label + 1);

  for (igraph_integer_t i = 0; i <= max_label; i++) {
    if (VECTOR(modularity_change)[i] > 0) {
      n_positive_changes++;
    }
  }

  if (n_positive_changes == 0) {
    goto cleanup_early;
  }

  for (igraph_integer_t i = 0; i <= max_label; i++) {
    if (VECTOR(merge_candidates)[i] == -1) {
      continue;
    }

    VECTOR(modularity_change)[i] /=
      (se2_partition_community_size(partition, i) +
       se2_partition_community_size(partition, VECTOR(merge_candidates)[i]));
  }

  min_merge_improvement = igraph_vector_sum( &modularity_change) /
                          n_positive_changes;

  if (min_merge_improvement < (0.5 ** max_prev_merge_threshold)) {
    goto cleanup_early;
  }

  if (min_merge_improvement > *max_prev_merge_threshold) {
    *max_prev_merge_threshold = min_merge_improvement;
  }

  median_modularity_change = se2_modularity_median(partition,
                             &modularity_change);

  igraph_vector_bool_t merged_labels;
  igraph_vector_int_t sort_index;

  igraph_vector_bool_init( &merged_labels, max_label + 1);
  igraph_vector_int_init( &sort_index, max_label + 1);
  igraph_vector_qsort_ind( &modularity_change, &sort_index, IGRAPH_DESCENDING);

  if (VECTOR(modularity_change)[VECTOR(sort_index)[0]] <=
      min_merge_improvement) {
    goto cleanup_sort;
  }

  igraph_integer_t c1, c2;
  igraph_real_t delta;
  for (igraph_integer_t i = 0; i <= max_label; i++) {
    c1 = VECTOR(sort_index)[i];
    c2 = VECTOR(merge_candidates)[c1];
    delta = VECTOR(modularity_change)[c1];

    if (delta <= median_modularity_change) {
      // Since in order, as soon as one is too small all after must be too
      // small.
      break;
    }

    if ((VECTOR(merged_labels)[c1]) || (VECTOR(merged_labels)[c2])) {
      continue;
    }

    if ((se2_partition_community_size(partition, c1) < 2) ||
        (se2_partition_community_size(partition, c2) < 2)) {
      continue;
    }

    VECTOR(merged_labels)[c1] = true;
    VECTOR(merged_labels)[c2] = true;

    se2_partition_merge_labels(partition, c1, c2);
    n_merges++;
  }

  if (n_merges > 0) {
    se2_partition_commit_changes(partition);
  }

cleanup_sort:
  igraph_vector_bool_destroy( &merged_labels);
  igraph_vector_int_destroy( &sort_index);

cleanup_early:
  igraph_vector_int_destroy( &merge_candidates);
  igraph_vector_destroy( &modularity_change);

  return n_merges == 0;
}
