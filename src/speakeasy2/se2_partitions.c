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

#include "se2_partitions.h"

#include "se2_error_handling.h"
#include "se2_neighborlist.h"
#include "se2_random.h"

#define MAX(a, b) (a) > (b) ? (a) : (b)

static igraph_integer_t se2_count_labels(
  igraph_vector_int_t const* membership, igraph_vector_int_t* community_sizes)
{
  igraph_integer_t const max_label = igraph_vector_int_max(membership);
  igraph_integer_t const n_nodes = igraph_vector_int_size(membership);
  igraph_integer_t n_labels = 0;

  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_resize(community_sizes, max_label + 1), 0);

  igraph_vector_int_null(community_sizes);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    VECTOR(*community_sizes)[VECTOR(*membership)[i]]++;
  }

  for (igraph_integer_t i = 0; i <= max_label; i++) {
    n_labels += VECTOR(*community_sizes)[i] > 0;
  }

  return n_labels;
}

static igraph_error_t se2_count_local_labels(se2_neighs const* graph,
  igraph_vector_int_t const* initial_labels, igraph_integer_t const max_label,
  igraph_matrix_t* labels_heard)
{
  igraph_integer_t const n_nodes = igraph_vector_int_size(initial_labels);
  igraph_integer_t const n_labels = max_label + 1;

  if ((igraph_matrix_ncol(labels_heard) != n_nodes) ||
      igraph_matrix_nrow(labels_heard) != n_labels) {
    SE2_THREAD_CHECK(igraph_matrix_resize(labels_heard, n_labels, n_nodes));
  }

  for (igraph_integer_t node_id = 0; node_id < n_nodes; node_id++) {
    igraph_integer_t const label = VECTOR(*initial_labels)[node_id];
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(*graph, node_id); j++) {
      MATRIX(*labels_heard, label, NEIGHBOR(*graph, node_id, j)) +=
        WEIGHT(*graph, node_id, j);
    }
  }

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_count_global_labels(se2_neighs const* graph,
  igraph_integer_t const max_label, igraph_matrix_t const* local_labels_heard,
  igraph_vector_t* global_labels_heard)
{
  igraph_integer_t const n_labels = max_label + 1;

  if (igraph_vector_size(global_labels_heard) != n_labels) {
    SE2_THREAD_CHECK(igraph_vector_resize(global_labels_heard, n_labels));
  }

  SE2_THREAD_CHECK(
    igraph_matrix_rowsum(local_labels_heard, global_labels_heard));

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_partition_init(se2_partition* partition,
  se2_neighs const* graph, igraph_vector_int_t const* initial_labels)
{
  igraph_integer_t const n_nodes = igraph_vector_int_size(initial_labels);

  igraph_vector_int_t* reference = igraph_malloc(sizeof(*reference));
  SE2_THREAD_CHECK_OOM(reference);
  IGRAPH_FINALLY(igraph_free, reference);

  igraph_vector_int_t* stage = igraph_malloc(sizeof(*stage));
  SE2_THREAD_CHECK_OOM(stage);
  IGRAPH_FINALLY(igraph_free, stage);

  igraph_vector_int_t* community_sizes =
    igraph_malloc(sizeof(*community_sizes));
  SE2_THREAD_CHECK_OOM(community_sizes);
  IGRAPH_FINALLY(igraph_free, community_sizes);

  igraph_matrix_t* local_labels_heard =
    igraph_malloc(sizeof(*local_labels_heard));
  SE2_THREAD_CHECK_OOM(local_labels_heard);
  IGRAPH_FINALLY(igraph_free, local_labels_heard);

  igraph_vector_t* global_labels_heard =
    igraph_malloc(sizeof(*global_labels_heard));
  SE2_THREAD_CHECK_OOM(global_labels_heard);
  IGRAPH_FINALLY(igraph_free, global_labels_heard);

  igraph_integer_t n_labels = 0;

  SE2_THREAD_CHECK(igraph_vector_int_init(reference, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, reference);
  SE2_THREAD_CHECK(igraph_vector_int_init(stage, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, stage);
  SE2_THREAD_CHECK(igraph_vector_int_init(community_sizes, 0));
  IGRAPH_FINALLY(igraph_vector_int_destroy, community_sizes);

  SE2_THREAD_CHECK(igraph_vector_int_update(reference, initial_labels));
  SE2_THREAD_CHECK(igraph_vector_int_update(stage, initial_labels));

  n_labels = se2_count_labels(initial_labels, community_sizes);
  SE2_THREAD_STATUS();

  partition->n_nodes = n_nodes;
  partition->reference = reference;
  partition->stage = stage;
  partition->community_sizes = community_sizes;
  partition->n_labels = n_labels;
  partition->max_label = igraph_vector_int_size(community_sizes) - 1;
  partition->local_labels_heard = local_labels_heard;
  partition->global_labels_heard = global_labels_heard;

  SE2_THREAD_CHECK(
    igraph_matrix_init(local_labels_heard, partition->max_label + 1, n_nodes));
  IGRAPH_FINALLY(igraph_matrix_destroy, local_labels_heard);
  SE2_THREAD_CHECK(
    igraph_vector_init(global_labels_heard, partition->max_label + 1));
  IGRAPH_FINALLY(igraph_vector_destroy, global_labels_heard);

  SE2_THREAD_CHECK(se2_count_local_labels(
    graph, initial_labels, partition->max_label, local_labels_heard));
  SE2_THREAD_CHECK(se2_count_global_labels(
    graph, partition->max_label, local_labels_heard, global_labels_heard));

  IGRAPH_FINALLY_CLEAN(10);

  return IGRAPH_SUCCESS;
}

void se2_partition_destroy(se2_partition* partition)
{
  igraph_vector_int_destroy(partition->reference);
  igraph_vector_int_destroy(partition->stage);
  igraph_vector_int_destroy(partition->community_sizes);
  igraph_matrix_destroy(partition->local_labels_heard);
  igraph_vector_destroy(partition->global_labels_heard);

  igraph_free(partition->reference);
  igraph_free(partition->stage);
  igraph_free(partition->community_sizes);
  igraph_free(partition->local_labels_heard);
  igraph_free(partition->global_labels_heard);
}

void se2_iterator_shuffle(se2_iterator* iterator)
{
  iterator->pos = 0;
  se2_randperm(iterator->ids, iterator->n_total, iterator->n_iter);
}

void se2_iterator_reset(se2_iterator* iterator) { iterator->pos = 0; }

// WARNING: Iterator does not take ownership of the id vector so it must still
// be cleaned up by the caller.
igraph_error_t se2_iterator_from_vector(se2_iterator* iterator,
  igraph_vector_int_t* ids, igraph_integer_t const n_iter)
{
  igraph_integer_t const n = igraph_vector_int_size(ids);
  iterator->ids = ids;
  iterator->n_total = n;
  iterator->n_iter = n_iter;
  iterator->pos = 0;
  iterator->owns_ids = false;

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_iterator_random_node_init(se2_iterator* iterator,
  se2_partition const* partition, igraph_real_t const proportion)
{
  igraph_integer_t n_total = partition->n_nodes;
  igraph_integer_t n_iter = n_total;
  igraph_vector_int_t* nodes = igraph_malloc(sizeof(*nodes));
  SE2_THREAD_CHECK_OOM(nodes);
  IGRAPH_FINALLY(igraph_free, nodes);

  SE2_THREAD_CHECK(igraph_vector_int_init(nodes, n_total));
  IGRAPH_FINALLY(igraph_vector_int_destroy, nodes);
  for (igraph_integer_t i = 0; i < n_total; i++) {
    VECTOR(*nodes)[i] = i;
  }

  if (proportion) {
    n_iter = n_total * proportion;
  }

  se2_iterator_from_vector(iterator, nodes, n_iter);
  IGRAPH_FINALLY(se2_iterator_destroy, iterator);
  iterator->owns_ids = true;
  se2_iterator_shuffle(iterator);

  IGRAPH_FINALLY_CLEAN(3);

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_iterator_random_label_init(se2_iterator* iterator,
  se2_partition const* partition, igraph_real_t const proportion)
{
  igraph_integer_t n_total = partition->n_labels;
  igraph_integer_t n_iter = n_total;
  igraph_vector_int_t* labels = igraph_malloc(sizeof(*labels));
  SE2_THREAD_CHECK_OOM(labels);
  IGRAPH_FINALLY(igraph_free, labels);

  SE2_THREAD_CHECK(igraph_vector_int_init(labels, n_total));
  IGRAPH_FINALLY(igraph_vector_int_destroy, labels);
  for (igraph_integer_t i = 0, j = 0; i < n_total; j++) {
    if (VECTOR(*(partition->community_sizes))[j] > 0) {
      VECTOR(*labels)[i] = j;
      i++;
    }
  }

  if (proportion) {
    n_iter = n_total * proportion;
  }

  se2_iterator_from_vector(iterator, labels, n_iter);
  IGRAPH_FINALLY(se2_iterator_destroy, iterator);
  iterator->owns_ids = true;
  se2_iterator_shuffle(iterator);

  IGRAPH_FINALLY_CLEAN(3);

  return IGRAPH_SUCCESS;
}

inline igraph_real_t score_label_i(se2_neighs const* graph,
  se2_partition const* partition, igraph_integer_t const node_id,
  igraph_integer_t const label_id)
{
  igraph_real_t actual =
    MATRIX(*partition->local_labels_heard, label_id, node_id);
  igraph_real_t expected = VECTOR(*partition->global_labels_heard)[label_id];
  igraph_real_t norm_factor =
    VECTOR(*graph->kin)[node_id] / graph->total_weight;

  return actual - (norm_factor * expected);
}

/* Returns the top n_nodes - k fitting nodes in best_fit_nodes if passed in.
   If proportion is set to a value other than 0, only iterator over a random
   sample of k * proportion nodes. */
igraph_error_t se2_iterator_k_worst_fit_nodes_init(se2_iterator* iterator,
  se2_neighs const* graph, se2_partition const* partition,
  igraph_integer_t const k, igraph_real_t proportion,
  igraph_vector_int_t* best_fit_nodes)
{
  igraph_integer_t n_iter = k;
  igraph_vector_t label_quality;
  igraph_vector_int_t* ids = igraph_malloc(sizeof(*ids));
  SE2_THREAD_CHECK_OOM(ids);
  IGRAPH_FINALLY(igraph_free, ids);

  SE2_THREAD_CHECK(igraph_vector_int_init(ids, partition->n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, ids);

  SE2_THREAD_CHECK(igraph_vector_init(&label_quality, partition->n_nodes));
  IGRAPH_FINALLY(igraph_vector_destroy, &label_quality);

  for (igraph_integer_t i = 0; i < partition->n_nodes; i++) {
    VECTOR(label_quality)
    [i] = score_label_i(graph, partition, i, LABEL(*partition)[i]);
  }

  SE2_THREAD_CHECK(
    igraph_vector_sort_ind(&label_quality, ids, IGRAPH_ASCENDING));
  igraph_vector_destroy(&label_quality);
  IGRAPH_FINALLY_CLEAN(1);

  if (best_fit_nodes) {
    SE2_THREAD_CHECK(
      igraph_vector_int_init(best_fit_nodes, partition->n_nodes - k));
    IGRAPH_FINALLY(igraph_vector_int_destroy, best_fit_nodes);
    for (igraph_integer_t i = k; i < partition->n_nodes; i++) {
      VECTOR(*best_fit_nodes)[i - k] = VECTOR(*ids)[i];
    }
  }
  SE2_THREAD_CHECK(igraph_vector_int_resize(ids, k));

  if (proportion) {
    n_iter *= proportion;
  }

  SE2_THREAD_CHECK(se2_iterator_from_vector(iterator, ids, n_iter));
  IGRAPH_FINALLY(se2_iterator_destroy, iterator);

  iterator->owns_ids = true;
  se2_iterator_shuffle(iterator);

  IGRAPH_FINALLY_CLEAN(3);
  if (best_fit_nodes) {
    IGRAPH_FINALLY_CLEAN(1);
  }

  return IGRAPH_SUCCESS;
}

void se2_iterator_destroy(se2_iterator* iterator)
{
  if (iterator->owns_ids) {
    igraph_vector_int_destroy(iterator->ids);
    igraph_free(iterator->ids);
  }
}

igraph_integer_t se2_iterator_next(se2_iterator* iterator)
{
  igraph_integer_t n = 0;
  if (iterator->pos == iterator->n_iter) {
    iterator->pos = 0;
    return -1;
  }

  n = VECTOR(*iterator->ids)[iterator->pos];
  iterator->pos++;

  return n;
}

igraph_integer_t se2_partition_n_nodes(se2_partition const* partition)
{
  return partition->n_nodes;
}

igraph_integer_t se2_partition_n_labels(se2_partition const* partition)
{
  return partition->n_labels;
}

igraph_integer_t se2_partition_max_label(se2_partition const* partition)
{
  return partition->max_label;
}

void se2_partition_add_to_stage(se2_partition* partition,
  igraph_integer_t const node_id, igraph_integer_t const label)
{
  VECTOR(*partition->stage)[node_id] = label;
}

// Return an unused label.
igraph_integer_t se2_partition_new_label(se2_partition* partition)
{
  igraph_integer_t pool_size =
    igraph_vector_int_size(partition->community_sizes);
  igraph_integer_t next_label = 0;
  while ((next_label < pool_size) &&
         (VECTOR(*partition->community_sizes)[next_label])) {
    next_label++;
  }

  if (next_label == igraph_vector_int_capacity(partition->community_sizes)) {
    SE2_THREAD_CHECK_RETURN(
      igraph_vector_int_reserve(
        partition->community_sizes, MAX(2 * pool_size, partition->n_nodes)),
      -1);
  }

  if (next_label == pool_size) {
    SE2_THREAD_CHECK_RETURN(
      igraph_vector_int_push_back(partition->community_sizes, 0), -1);
  }

  if (next_label > partition->max_label) {
    partition->max_label = next_label;
  }

  partition->n_labels++;

  // Mark new label as reserved.
  VECTOR(*partition->community_sizes)[next_label] = -1;

  return next_label;
}

static inline void se2_partition_free_label(
  se2_partition* partition, igraph_integer_t const label)
{
  VECTOR(*partition->community_sizes)[label] = 0;
  if (label == partition->max_label) {
    while ((!VECTOR(*partition->community_sizes)[partition->max_label]) &&
           (partition->max_label > 0)) {
      partition->max_label--;
    }
  }

  partition->n_labels--;
}

igraph_integer_t se2_partition_community_size(
  se2_partition const* partition, igraph_integer_t const label)
{
  return VECTOR(*partition->community_sizes)[label];
}

igraph_real_t se2_vector_median(igraph_vector_t const* vec)
{
  igraph_vector_int_t ids;
  igraph_integer_t len = igraph_vector_size(vec) - 1;
  igraph_integer_t k = len / 2;
  igraph_real_t res;

  SE2_THREAD_CHECK_RETURN(igraph_vector_int_init(&ids, len), 0);
  IGRAPH_FINALLY(igraph_vector_int_destroy, &ids);
  SE2_THREAD_CHECK_RETURN(
    igraph_vector_sort_ind(vec, &ids, IGRAPH_ASCENDING), 0);
  res = VECTOR(*vec)[VECTOR(ids)[k]];

  if (len % 2) {
    res += VECTOR(*vec)[VECTOR(ids)[k + 1]];
    res /= 2;
  }

  igraph_vector_int_destroy(&ids);
  IGRAPH_FINALLY_CLEAN(1);

  return res;
}

igraph_real_t se2_vector_int_median(igraph_vector_int_t const* vec)
{
  igraph_vector_int_t ids;
  igraph_integer_t len = igraph_vector_int_size(vec) - 1;
  igraph_integer_t k = len / 2;
  igraph_real_t res;

  SE2_THREAD_CHECK_RETURN(igraph_vector_int_init(&ids, len), 0);
  IGRAPH_FINALLY(igraph_vector_int_destroy, &ids);
  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_sort_ind(vec, &ids, IGRAPH_ASCENDING), 0);
  res = VECTOR(*vec)[VECTOR(ids)[k]];

  if (len % 2) {
    res += VECTOR(*vec)[VECTOR(ids)[k + 1]];
    res /= 2;
  }

  igraph_vector_int_destroy(&ids);
  IGRAPH_FINALLY_CLEAN(1);

  return res;
}

igraph_real_t se2_partition_median_community_size(
  se2_partition const* partition)
{
  if (partition->n_labels == 1) {
    return partition->n_nodes;
  }

  igraph_vector_int_t community_sizes;
  se2_iterator label_iter;
  igraph_real_t res = 0;

  SE2_THREAD_CHECK_RETURN(
    se2_iterator_random_label_init(&label_iter, partition, 0), 0);
  IGRAPH_FINALLY(se2_iterator_destroy, &label_iter);
  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_init(&community_sizes, partition->n_labels), 0);
  IGRAPH_FINALLY(igraph_vector_int_destroy, &community_sizes);

  igraph_integer_t label_id;
  igraph_integer_t label_i = 0;
  while ((label_id = se2_iterator_next(&label_iter)) != -1) {
    VECTOR(community_sizes)
    [label_i] = se2_partition_community_size(partition, label_id);
    label_i++;
  }
  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_resize(&community_sizes, label_i), 0);

  res = se2_vector_int_median(&community_sizes);

  se2_iterator_destroy(&label_iter);
  igraph_vector_int_destroy(&community_sizes);
  IGRAPH_FINALLY_CLEAN(2);

  return res;
}

void se2_partition_merge_labels(
  se2_partition* partition, igraph_integer_t c1, igraph_integer_t c2)
{
  // Ensure smaller community engulfed by larger community. Not necessary.
  if (se2_partition_community_size(partition, c2) >
      se2_partition_community_size(partition, c1)) {
    igraph_integer_t swp = c1;
    c1 = c2;
    c2 = swp;
  }

  for (igraph_integer_t i = 0; i < partition->n_nodes; i++) {
    if (LABEL(*partition)[i] == c2) {
      STAGE(*partition)[i] = c1;
    }
  }

  se2_partition_free_label(partition, c2);
}

// Move nodes in mask to new label.
igraph_error_t se2_partition_relabel_mask(
  se2_partition* partition, igraph_vector_bool_t const* mask)
{
  igraph_integer_t label = se2_partition_new_label(partition);
  SE2_THREAD_STATUS();
  for (igraph_integer_t i = 0; i < partition->n_nodes; i++) {
    if (VECTOR(*mask)[i]) {
      VECTOR(*partition->stage)[i] = label;
    }
  }

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_partition_recount_community_sizes(
  se2_partition* partition)
{
  partition->n_labels =
    se2_count_labels(partition->stage, partition->community_sizes);
  SE2_THREAD_STATUS();
  partition->max_label =
    igraph_vector_int_size(partition->community_sizes) - 1;

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_resize_local_labels(se2_partition* partition,
  igraph_integer_t const n_nodes, igraph_integer_t n_labels)
{
  igraph_matrix_t* new_local_labels = igraph_malloc(sizeof(*new_local_labels));
  SE2_THREAD_CHECK_OOM(new_local_labels);
  IGRAPH_FINALLY(igraph_free, new_local_labels);

  SE2_THREAD_CHECK(igraph_matrix_init(new_local_labels, n_labels, n_nodes));
  IGRAPH_FINALLY(igraph_matrix_destroy, new_local_labels);

  igraph_integer_t const old_n_nodes =
    igraph_matrix_ncol(partition->local_labels_heard);
  igraph_integer_t const old_n_labels =
    igraph_matrix_nrow(partition->local_labels_heard);
  for (igraph_integer_t i = 0; i < old_n_nodes; i++) {
    for (igraph_integer_t j = 0; j < old_n_labels; j++) {
      MATRIX(*new_local_labels, j, i) =
        MATRIX(*partition->local_labels_heard, j, i);
    }
  }

  igraph_matrix_destroy(partition->local_labels_heard);
  igraph_free(partition->local_labels_heard);
  partition->local_labels_heard = new_local_labels;

  IGRAPH_FINALLY_CLEAN(2);

  return IGRAPH_SUCCESS;
}

/* For each node that switched labels, move their edge weights from the old
   label to the new label. */
static igraph_error_t se2_move_labels_heard(
  se2_partition* partition, se2_neighs const* graph)
{
  igraph_integer_t old_max_label =
    igraph_vector_size(partition->global_labels_heard) - 1;
  if (old_max_label < partition->max_label) {
    igraph_integer_t n_labels = partition->max_label + 1;

    SE2_THREAD_CHECK(
      igraph_vector_resize(partition->global_labels_heard, n_labels));
    for (igraph_integer_t i = old_max_label + 1; i < n_labels; i++) {
      VECTOR(*partition->global_labels_heard)[i] = 0;
    }

    SE2_THREAD_CHECK(
      se2_resize_local_labels(partition, partition->n_nodes, n_labels));
    for (igraph_integer_t i = 0; i < partition->n_nodes; i++) {
      for (igraph_integer_t j = old_max_label + 1; j < n_labels; j++) {
        MATRIX(*partition->local_labels_heard, j, i) = 0;
      }
    }
  }

  for (igraph_integer_t node_id = 0; node_id < partition->n_nodes; node_id++) {
    if (LABEL(*partition)[node_id] == STAGE(*partition)[node_id]) {
      continue;
    }

    igraph_integer_t const old_label = LABEL(*partition)[node_id];
    igraph_integer_t const new_label = STAGE(*partition)[node_id];

    igraph_real_t acc = 0;
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(*graph, node_id); j++) {
      acc += WEIGHT(*graph, node_id, j);
    }

    VECTOR(*partition->global_labels_heard)[old_label] -= acc;
    VECTOR(*partition->global_labels_heard)[new_label] += acc;

    for (igraph_integer_t j = 0; j < N_NEIGHBORS(*graph, node_id); j++) {
      MATRIX(*partition->local_labels_heard, old_label,
        NEIGHBOR(*graph, node_id, j)) -= WEIGHT(*graph, node_id, j);
      MATRIX(*partition->local_labels_heard, new_label,
        NEIGHBOR(*graph, node_id, j)) += WEIGHT(*graph, node_id, j);
    }
  }

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_partition_commit_changes(
  se2_partition* partition, se2_neighs const* graph)
{
  SE2_THREAD_CHECK(se2_partition_recount_community_sizes(partition));
  SE2_THREAD_CHECK(se2_move_labels_heard(partition, graph));
  SE2_THREAD_CHECK(
    igraph_vector_int_update(partition->reference, partition->stage));

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_reindex_membership(igraph_vector_int_t* membership)
{
  igraph_vector_int_t indices;
  igraph_integer_t n_nodes = igraph_vector_int_size(membership);

  SE2_THREAD_CHECK(igraph_vector_int_init(&indices, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &indices);

  SE2_THREAD_CHECK(
    igraph_vector_int_sort_ind(membership, &indices, IGRAPH_ASCENDING));

  igraph_integer_t c_old, c_new = -1, c_prev_node = -1;
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    c_old = VECTOR(*membership)[VECTOR(indices)[i]];
    if (c_old != c_prev_node) {
      c_new++;
      c_prev_node = c_old;
    }
    VECTOR(*membership)[VECTOR(indices)[i]] = c_new;
  }

  igraph_vector_int_destroy(&indices);
  IGRAPH_FINALLY_CLEAN(1);

  return IGRAPH_SUCCESS;
}

/* Save the state of the current working partition's committed changes to the
partition store.

NOTE: This saves only the membership ids for each node so it goes from a
se2_partition to an igraph vector despite both arguments being
"partitions". */
igraph_error_t se2_partition_store(se2_partition const* working_partition,
  igraph_vector_int_list_t* partition_store, igraph_integer_t const idx)
{
  igraph_vector_int_t* partition_state =
    igraph_vector_int_list_get_ptr(partition_store, idx);

  SE2_THREAD_CHECK(
    igraph_vector_int_update(partition_state, working_partition->reference));

  SE2_THREAD_CHECK(se2_reindex_membership(partition_state));

  return IGRAPH_SUCCESS;
}
