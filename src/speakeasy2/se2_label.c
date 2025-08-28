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

#include "se2_label.h"

#include "se2_error_handling.h"
#include "se2_neighborlist.h"

#include <igraph.h>

/* Scores labels based on the difference between the local and global
 frequencies.  Labels that are overrepresented locally are likely to be of
 importance in tagging a node. */
igraph_error_t se2_find_most_specific_labels_i(se2_neighs const* graph,
  se2_partition* partition, se2_iterator* node_iter, igraph_integer_t* n_moved)
{
  igraph_integer_t const n_labels = partition->max_label + 1;
  igraph_vector_t* const global_heard = partition->global_labels_heard;
  igraph_matrix_t* const local_heard = partition->local_labels_heard;
  igraph_vector_t* const kin = graph->kin;
  igraph_real_t const total_weight_inv = 1 / graph->total_weight;

  igraph_integer_t n_moved_i = 0;
  igraph_integer_t node_id = 0;
  while ((node_id = se2_iterator_next(node_iter)) != -1) {
    igraph_real_t best_label_specificity = -INFINITY;
    igraph_integer_t best_label = -1;
    igraph_real_t norm_factor = VECTOR(*kin)[node_id] * total_weight_inv;

    for (igraph_integer_t label_id = 0; label_id < n_labels; label_id++) {
      /* NOTE: Because used label IDs are not necessarily contiguous, we need *
      to loop over more iterations than labels actually used. This busts * some
      of the compiler optimizations since we are no longer moving over * the
      local heard matrix and global heard vector 1 element at a time. It's
      possible repacking the labels (being careful to reorder all the label
      based cached values)at partition commit time could improve speed since
      this is a very hot loop. In this case n_labels becomes
      partition->n_labels and we no longer need to check the community size is
      non-zero.*/
      if (VECTOR(*partition->community_sizes)[label_id] == 0) {
        continue;
      }
      igraph_real_t const actual = MATRIX(*local_heard, label_id, node_id);
      igraph_real_t const expected = VECTOR(*global_heard)[label_id];
      igraph_real_t const score = actual - (norm_factor * expected);

      if (score > best_label_specificity) {
        best_label_specificity = score;
        best_label = label_id;
      }
    }

    if (LABEL(*partition)[node_id] != best_label) {
      n_moved_i++;
    }

    se2_partition_add_to_stage(partition, node_id, best_label);
  }

  SE2_THREAD_CHECK(se2_partition_commit_changes(partition, graph));

  if (n_moved) {
    *n_moved = n_moved_i;
  }

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_find_most_specific_labels(se2_neighs const* graph,
  se2_partition* partition, igraph_real_t const fraction_nodes_to_label,
  igraph_bool_t* did_change)
{
  igraph_integer_t n_moved = 0;
  se2_iterator node_iter;

  SE2_THREAD_CHECK(se2_iterator_random_node_init(
    &node_iter, partition, fraction_nodes_to_label));
  IGRAPH_FINALLY(se2_iterator_destroy, &node_iter);

  SE2_THREAD_CHECK(
    se2_find_most_specific_labels_i(graph, partition, &node_iter, &n_moved));

  se2_iterator_destroy(&node_iter);
  IGRAPH_FINALLY_CLEAN(1);

  *did_change = (n_moved > 0);

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_relabel_worst_nodes(se2_neighs const* graph,
  se2_partition* partition, igraph_real_t const fraction_nodes_to_label)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
  se2_iterator node_iter;
  igraph_vector_int_t best_fit_nodes;
  igraph_vector_int_t best_fit_labels;
  igraph_integer_t tmp_label = se2_partition_new_label(partition);

  /* The fraction_nodes_to_label variable is used for two different meanings
   here. Should be broken into two separate arguments. First meaning is the
   fraction of all nodes in the graph that should be consider poor fitting.
   Second meaning is the random fraction of poor fitting nodes to relabel. */
  SE2_THREAD_CHECK(se2_iterator_k_worst_fit_nodes_init(&node_iter, graph,
    partition, fraction_nodes_to_label * n_nodes, fraction_nodes_to_label,
    &best_fit_nodes));
  IGRAPH_FINALLY(se2_iterator_destroy, &node_iter);
  IGRAPH_FINALLY(igraph_vector_int_destroy, &best_fit_nodes);

  SE2_THREAD_CHECK(igraph_vector_int_init(
    &best_fit_labels, igraph_vector_int_size(&best_fit_nodes)));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &best_fit_labels);
  for (igraph_integer_t i = 0; i < igraph_vector_int_size(&best_fit_nodes);
       i++) {
    VECTOR(best_fit_labels)[i] = LABEL(*partition)[VECTOR(best_fit_nodes)[i]];
    se2_partition_add_to_stage(
      partition, VECTOR(best_fit_nodes)[i], tmp_label);
  }
  se2_partition_commit_changes(partition, graph);

  SE2_THREAD_CHECK(
    se2_find_most_specific_labels_i(graph, partition, &node_iter, NULL));

  for (igraph_integer_t i = 0; i < igraph_vector_int_size(&best_fit_nodes);
       i++) {
    se2_partition_add_to_stage(
      partition, VECTOR(best_fit_nodes)[i], VECTOR(best_fit_labels)[i]);
  }
  se2_partition_commit_changes(partition, graph);

  igraph_vector_int_destroy(&best_fit_labels);
  igraph_vector_int_destroy(&best_fit_nodes);
  se2_iterator_destroy(&node_iter);
  IGRAPH_FINALLY_CLEAN(3);

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_burst_large_communities(se2_neighs const* graph,
  se2_partition* partition, igraph_real_t const fraction_nodes_to_move,
  igraph_integer_t const min_community_size)
{
  se2_iterator node_iter;
  igraph_vector_int_t n_new_tags_cum;
  igraph_vector_int_t n_nodes_to_move;
  igraph_vector_int_t new_tags;
  igraph_integer_t node_id;
  igraph_real_t desired_community_size =
    se2_partition_median_community_size(partition);

  SE2_THREAD_STATUS();
  SE2_THREAD_CHECK(se2_iterator_k_worst_fit_nodes_init(&node_iter, graph,
    partition, partition->n_nodes * fraction_nodes_to_move, 0, NULL));
  IGRAPH_FINALLY(se2_iterator_destroy, &node_iter);

  SE2_THREAD_CHECK(
    igraph_vector_int_init(&n_new_tags_cum, partition->max_label + 2));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &n_new_tags_cum);

  SE2_THREAD_CHECK(
    igraph_vector_int_init(&n_nodes_to_move, partition->max_label + 1));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &n_nodes_to_move);

  while ((node_id = se2_iterator_next(&node_iter)) != -1) {
    if (se2_partition_community_size(partition, LABEL(*partition)[node_id]) >=
        min_community_size) {
      VECTOR(n_nodes_to_move)[LABEL(*partition)[node_id]]++;
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

  SE2_THREAD_CHECK(igraph_vector_int_init(&new_tags, n_new_tags));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &new_tags);
  for (igraph_integer_t i = 0; i < n_new_tags; i++) {
    VECTOR(new_tags)[i] = se2_partition_new_label(partition);
    SE2_THREAD_STATUS();
  }

  igraph_integer_t current_label;
  while ((node_id = se2_iterator_next(&node_iter)) != -1) {
    current_label = LABEL(*partition)[node_id];
    if (se2_partition_community_size(partition, current_label) >=
        min_community_size) {
      igraph_integer_t const new_label = VECTOR(
        new_tags)[RNG_INTEGER(VECTOR(n_new_tags_cum)[current_label],
        VECTOR(n_new_tags_cum)[current_label + 1] - 1)];
      se2_partition_add_to_stage(partition, node_id, new_label);
    }
  }

  igraph_vector_int_destroy(&new_tags);
  igraph_vector_int_destroy(&n_nodes_to_move);
  igraph_vector_int_destroy(&n_new_tags_cum);
  se2_iterator_destroy(&node_iter);
  IGRAPH_FINALLY_CLEAN(4);

  SE2_THREAD_CHECK(se2_partition_commit_changes(partition, graph));

  return IGRAPH_SUCCESS;
}

/* For each community, find the communities that would cause the greatest
increase in modularity if merged.

merge_candidates: a vector of indices where each value is the best community to
merge with the ith community.
modularity_change: a vector of how much the modularity would change if the
corresponding merge_candidates were combined.

modularity_change is capped to be always non-negative. */
static igraph_error_t se2_best_merges(se2_neighs const* graph,
  se2_partition const* partition, igraph_vector_int_t* merge_candidates,
  igraph_vector_t* modularity_change, igraph_integer_t const n_labels)
{
  igraph_real_t const total_weight =
    HASWEIGHTS(*graph) ? se2_total_weight(graph) : se2_ecount(graph);
  igraph_matrix_t crosstalk;
  igraph_vector_t from_edge_probability;
  igraph_vector_t to_edge_probability;

  SE2_THREAD_CHECK(igraph_matrix_init(&crosstalk, n_labels, n_labels));
  IGRAPH_FINALLY(igraph_matrix_destroy, &crosstalk);
  SE2_THREAD_CHECK(igraph_vector_init(&from_edge_probability, n_labels));
  IGRAPH_FINALLY(igraph_vector_destroy, &from_edge_probability);
  SE2_THREAD_CHECK(igraph_vector_init(&to_edge_probability, n_labels));
  IGRAPH_FINALLY(igraph_vector_destroy, &to_edge_probability);

  igraph_vector_int_fill(merge_candidates, -1);

  for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(*graph, i); j++) {
      MATRIX(crosstalk, LABEL(*partition)[NEIGHBOR(*graph, i, j)],
        LABEL(*partition)[i]) += WEIGHT(*graph, i, j);
    }
  }

  for (igraph_integer_t i = 0; i < n_labels; i++) {
    for (igraph_integer_t j = 0; j < n_labels; j++) {
      MATRIX(crosstalk, i, j) /= total_weight;
    }
  }

  SE2_THREAD_CHECK(igraph_matrix_rowsum(&crosstalk, &from_edge_probability));
  SE2_THREAD_CHECK(igraph_matrix_colsum(&crosstalk, &to_edge_probability));

  igraph_real_t modularity_delta;
  for (igraph_integer_t i = 0; i < n_labels; i++) {
    for (igraph_integer_t j = (i + 1); j < n_labels; j++) {
      modularity_delta =
        MATRIX(crosstalk, i, j) + MATRIX(crosstalk, j, i) -
        (VECTOR(from_edge_probability)[i] * VECTOR(to_edge_probability)[j]) -
        (VECTOR(from_edge_probability)[j] * VECTOR(to_edge_probability)[i]);

      if (modularity_delta > VECTOR(*modularity_change)[i]) {
        VECTOR(*modularity_change)[i] = modularity_delta;
        VECTOR(*merge_candidates)[i] = j;
      }

      if (modularity_delta > VECTOR(*modularity_change)[j]) {
        VECTOR(*modularity_change)[j] = modularity_delta;
        VECTOR(*merge_candidates)[j] = i;
      }
    }
  }

  igraph_matrix_destroy(&crosstalk);
  igraph_vector_destroy(&from_edge_probability);
  igraph_vector_destroy(&to_edge_probability);
  IGRAPH_FINALLY_CLEAN(3);

  return IGRAPH_SUCCESS;
}

/* Since used labels are not necessarily sequential, modularity change can be
larger than the number of labels in use. To get the median, have to find
elements of modularity change corresponding to labels in use.*/
igraph_real_t se2_modularity_median(
  se2_partition* partition, igraph_vector_t* modularity_change)
{
  igraph_vector_t modularity_change_without_gaps;
  se2_iterator label_iter;
  igraph_real_t res;

  SE2_THREAD_CHECK_RETURN(
    se2_iterator_random_label_init(&label_iter, partition, 0), 0);
  IGRAPH_FINALLY(se2_iterator_destroy, &label_iter);
  SE2_THREAD_CHECK_RETURN(
    igraph_vector_init(&modularity_change_without_gaps, partition->n_labels),
    0);
  IGRAPH_FINALLY(igraph_vector_destroy, &modularity_change_without_gaps);

  igraph_integer_t label_id = 0;
  igraph_integer_t label_i = 0;
  while ((label_id = se2_iterator_next(&label_iter)) != -1) {
    VECTOR(modularity_change_without_gaps)
    [label_i] = VECTOR(*modularity_change)[label_id];
    label_i++;
  }

  res = se2_vector_median(&modularity_change_without_gaps);

  igraph_vector_destroy(&modularity_change_without_gaps);
  se2_iterator_destroy(&label_iter);
  IGRAPH_FINALLY_CLEAN(2);

  return res;
}

igraph_error_t se2_merge_well_connected_communities(se2_neighs const* graph,
  se2_partition* partition, igraph_real_t* max_prev_merge_threshold,
  igraph_bool_t* is_partition_stable)
{
  igraph_integer_t max_label = se2_partition_max_label(partition);
  igraph_vector_int_t merge_candidates;
  igraph_vector_t modularity_change;
  igraph_real_t min_merge_improvement;
  igraph_real_t median_modularity_change;
  igraph_integer_t n_positive_changes = 0;
  igraph_integer_t n_merges = 0;

  SE2_THREAD_CHECK(igraph_vector_int_init(&merge_candidates, max_label + 1));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &merge_candidates);
  SE2_THREAD_CHECK(igraph_vector_init(&modularity_change, max_label + 1));
  IGRAPH_FINALLY(igraph_vector_destroy, &modularity_change);

  SE2_THREAD_CHECK(se2_best_merges(
    graph, partition, &merge_candidates, &modularity_change, max_label + 1));

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

    VECTOR(modularity_change)
    [i] /=
      (se2_partition_community_size(partition, i) +
        se2_partition_community_size(partition, VECTOR(merge_candidates)[i]));
  }

  min_merge_improvement =
    igraph_vector_sum(&modularity_change) / n_positive_changes;

  if (min_merge_improvement < (0.5 * *max_prev_merge_threshold)) {
    goto cleanup_early;
  }

  if (min_merge_improvement > *max_prev_merge_threshold) {
    *max_prev_merge_threshold = min_merge_improvement;
  }

  median_modularity_change =
    se2_modularity_median(partition, &modularity_change);
  SE2_THREAD_STATUS();

  igraph_vector_bool_t merged_labels;
  igraph_vector_int_t sort_index;

  SE2_THREAD_CHECK(igraph_vector_bool_init(&merged_labels, max_label + 1));
  IGRAPH_FINALLY(igraph_vector_bool_destroy, &merged_labels);
  SE2_THREAD_CHECK(igraph_vector_int_init(&sort_index, max_label + 1));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &sort_index);

  SE2_THREAD_CHECK(igraph_vector_sort_ind(
    &modularity_change, &sort_index, IGRAPH_DESCENDING));

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
    SE2_THREAD_CHECK(se2_partition_commit_changes(partition, graph));
  }

cleanup_sort:
  igraph_vector_bool_destroy(&merged_labels);
  igraph_vector_int_destroy(&sort_index);
  IGRAPH_FINALLY_CLEAN(2);

cleanup_early:
  igraph_vector_int_destroy(&merge_candidates);
  igraph_vector_destroy(&modularity_change);
  IGRAPH_FINALLY_CLEAN(2);

  *is_partition_stable = (n_merges == 0);

  return IGRAPH_SUCCESS;
}
