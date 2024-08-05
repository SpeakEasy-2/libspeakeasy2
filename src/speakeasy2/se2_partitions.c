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
#include "se2_partitions.h"
#include "se2_random.h"
#include "se2_error_handling.h"

#define MAX(a, b) (a) > (b) ? (a) : (b)

static igraph_integer_t se2_count_labels(igraph_vector_int_t* membership,
    igraph_vector_int_t* community_sizes)
{
  igraph_integer_t max_label = igraph_vector_int_max(membership);
  igraph_integer_t n_labels = 0;
  igraph_integer_t n_nodes = igraph_vector_int_size(membership);

  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_resize(community_sizes, max_label + 1), 0);

  igraph_vector_int_null(community_sizes);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    VECTOR(* community_sizes)[VECTOR(* membership)[i]]++;
  }

  for (igraph_integer_t i = 0; i <= max_label; i++) {
    n_labels += VECTOR(* community_sizes)[i] > 0;
  }

  return n_labels;
}

igraph_error_t se2_partition_init(se2_partition* partition,
                                  igraph_vector_int_t* initial_labels)
{
  igraph_integer_t const n_nodes = igraph_vector_int_size(initial_labels);

  igraph_vector_int_t* reference = igraph_malloc(sizeof(* reference));
  SE2_THREAD_CHECK_OOM(reference);
  IGRAPH_FINALLY(igraph_free, reference);

  igraph_vector_int_t* stage = igraph_malloc(sizeof(* stage));
  SE2_THREAD_CHECK_OOM(stage);
  IGRAPH_FINALLY(igraph_free, stage);

  igraph_vector_t* specificity = igraph_malloc(sizeof(* specificity));
  SE2_THREAD_CHECK_OOM(specificity);
  IGRAPH_FINALLY(igraph_free, specificity);

  igraph_vector_int_t* community_sizes =
    igraph_malloc(sizeof(* community_sizes));
  SE2_THREAD_CHECK_OOM(community_sizes);
  IGRAPH_FINALLY(igraph_free, community_sizes);

  igraph_integer_t n_labels = 0;

  SE2_THREAD_CHECK(igraph_vector_int_init(reference, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, reference);
  SE2_THREAD_CHECK(igraph_vector_int_init(stage, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, stage);
  SE2_THREAD_CHECK(igraph_vector_init(specificity, n_nodes));
  IGRAPH_FINALLY(igraph_vector_destroy, specificity);
  SE2_THREAD_CHECK(igraph_vector_int_init(community_sizes, 0));
  IGRAPH_FINALLY(igraph_vector_int_destroy, community_sizes);

  igraph_vector_int_update(reference, initial_labels);
  igraph_vector_int_update(stage, initial_labels);

  n_labels = se2_count_labels(initial_labels, community_sizes);
  SE2_THREAD_STATUS();

  partition->n_nodes = n_nodes;
  partition->reference = reference;
  partition->stage = stage;
  partition->label_quality = specificity;
  partition->community_sizes = community_sizes;
  partition->n_labels = n_labels;
  partition->max_label = igraph_vector_int_size(community_sizes) - 1;

  IGRAPH_FINALLY_CLEAN(8);

  return IGRAPH_SUCCESS;
}

void se2_partition_destroy(se2_partition* partition)
{
  igraph_vector_int_destroy(partition->reference);
  igraph_vector_int_destroy(partition->stage);
  igraph_vector_destroy(partition->label_quality);
  igraph_vector_int_destroy(partition->community_sizes);
  igraph_free(partition->reference);
  igraph_free(partition->stage);
  igraph_free(partition->label_quality);
  igraph_free(partition->community_sizes);
}

void se2_iterator_shuffle(se2_iterator* iterator)
{
  iterator->pos = 0;
  se2_randperm(iterator->ids, iterator->n_total,
               iterator->n_iter);
}

void se2_iterator_reset(se2_iterator* iterator)
{
  iterator->pos = 0;
}

// WARNING: Iterator does not take ownership of the id vector so it must still
// be cleaned up by the caller.
igraph_error_t se2_iterator_from_vector(se2_iterator* iterator,
                                        igraph_vector_int_t* ids,
                                        igraph_integer_t const n_iter)
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
    se2_partition const* partition,
    igraph_real_t const proportion)
{
  igraph_integer_t n_total = partition->n_nodes;
  igraph_integer_t n_iter = n_total;
  igraph_vector_int_t* nodes = igraph_malloc(sizeof(* nodes));
  SE2_THREAD_CHECK_OOM(nodes);
  IGRAPH_FINALLY(igraph_free, nodes);

  SE2_THREAD_CHECK(igraph_vector_int_init(nodes, n_total));
  IGRAPH_FINALLY(igraph_vector_int_destroy, nodes);
  for (igraph_integer_t i = 0; i < n_total; i++) {
    VECTOR(* nodes)[i] = i;
  }

  if (proportion) {
    n_iter = n_total* proportion;
  }

  se2_iterator_from_vector(iterator, nodes, n_iter);
  IGRAPH_FINALLY(se2_iterator_destroy, iterator);
  iterator->owns_ids = true;
  se2_iterator_shuffle(iterator);

  IGRAPH_FINALLY_CLEAN(3);

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_iterator_random_label_init(se2_iterator* iterator,
    se2_partition const* partition,
    igraph_real_t const proportion)
{
  igraph_integer_t n_total = partition->n_labels;
  igraph_integer_t n_iter = n_total;
  igraph_vector_int_t* labels = igraph_malloc(sizeof(* labels));
  SE2_THREAD_CHECK_OOM(labels);
  IGRAPH_FINALLY(igraph_free, labels);

  SE2_THREAD_CHECK(igraph_vector_int_init(labels, n_total));
  IGRAPH_FINALLY(igraph_vector_int_destroy, labels);
  for (igraph_integer_t i = 0, j = 0; i < n_total; j++) {
    if (VECTOR(* (partition->community_sizes))[j] > 0) {
      VECTOR(* labels)[i] = j;
      i++;
    }
  }

  if (proportion) {
    n_iter = n_total* proportion;
  }

  se2_iterator_from_vector(iterator, labels, n_iter);
  IGRAPH_FINALLY(se2_iterator_destroy, iterator);
  iterator->owns_ids = true;
  se2_iterator_shuffle(iterator);

  IGRAPH_FINALLY_CLEAN(3);

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_iterator_k_worst_fit_nodes_init(se2_iterator* iterator,
    se2_partition const* partition, igraph_integer_t const k)
{
  igraph_vector_int_t* ids = igraph_malloc(sizeof(* ids));
  SE2_THREAD_CHECK_OOM(ids);
  IGRAPH_FINALLY(igraph_free, ids);

  SE2_THREAD_CHECK(igraph_vector_int_init(ids, partition->n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, ids);

  SE2_THREAD_CHECK(
    igraph_vector_qsort_ind(partition->label_quality, ids, IGRAPH_ASCENDING));
  SE2_THREAD_CHECK(igraph_vector_int_resize(ids, k));

  SE2_THREAD_CHECK(se2_iterator_from_vector(iterator, ids, k));
  IGRAPH_FINALLY(se2_iterator_destroy, iterator);

  iterator->owns_ids = true;
  se2_iterator_shuffle(iterator);

  IGRAPH_FINALLY_CLEAN(3);

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

  n = VECTOR(* iterator->ids)[iterator->pos];
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
                                igraph_integer_t const node_id,
                                igraph_integer_t const label,
                                igraph_real_t specificity)
{
  VECTOR(* partition->stage)[node_id] = label;
  VECTOR(* partition->label_quality)[node_id] = specificity;
}

// Return an unused label.
igraph_integer_t se2_partition_new_label(se2_partition* partition)
{
  igraph_integer_t pool_size = igraph_vector_int_size(
                                 partition->community_sizes);
  igraph_integer_t next_label = 0;
  while ((next_label < pool_size) &&
         (VECTOR(* partition->community_sizes)[next_label])) {
    next_label++;
  }

  if (next_label == igraph_vector_int_capacity(partition->community_sizes)) {
    SE2_THREAD_CHECK_RETURN(igraph_vector_int_reserve(partition->community_sizes,
                            MAX(2 * pool_size, partition->n_nodes)), -1);
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
  VECTOR(* partition->community_sizes)[next_label] = -1;

  return next_label;
}

static inline void se2_partition_free_label(se2_partition* partition,
    igraph_integer_t const label)
{
  VECTOR(* partition->community_sizes)[label] = 0;
  if (label == partition->max_label) {
    while ((!VECTOR(* partition->community_sizes)[partition->max_label]) &&
           (partition->max_label > 0)) {
      partition->max_label--;
    }
  }

  partition->n_labels--;
}

igraph_integer_t se2_partition_community_size(se2_partition const* partition,
    igraph_integer_t const label)
{
  return VECTOR(* partition->community_sizes)[label];
}

igraph_real_t se2_vector_median(igraph_vector_t const* vec)
{
  igraph_vector_int_t ids;
  igraph_integer_t len = igraph_vector_size(vec) - 1;
  igraph_integer_t k = len / 2;
  igraph_real_t res;

  SE2_THREAD_CHECK_RETURN(igraph_vector_int_init( &ids, len), 0);
  IGRAPH_FINALLY(igraph_vector_int_destroy, &ids);
  SE2_THREAD_CHECK_RETURN(igraph_vector_qsort_ind(vec, &ids, IGRAPH_ASCENDING),
                          0);
  res = VECTOR(* vec)[VECTOR(ids)[k]];

  if (len % 2) {
    res += VECTOR(* vec)[VECTOR(ids)[k + 1]];
    res /= 2;
  }

  igraph_vector_int_destroy( &ids);
  IGRAPH_FINALLY_CLEAN(1);

  return res;
}
igraph_real_t se2_vector_int_median(igraph_vector_int_t const* vec)
{
  igraph_vector_int_t ids;
  igraph_integer_t len = igraph_vector_int_size(vec) - 1;
  igraph_integer_t k = len / 2;
  igraph_real_t res;

  SE2_THREAD_CHECK_RETURN(igraph_vector_int_init( &ids, len), 0);
  IGRAPH_FINALLY(igraph_vector_int_destroy, &ids);
  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_qsort_ind(vec, &ids, IGRAPH_ASCENDING), 0);
  res = VECTOR(* vec)[VECTOR(ids)[k]];

  if (len % 2) {
    res += VECTOR(* vec)[VECTOR(ids)[k + 1]];
    res /= 2;
  }

  igraph_vector_int_destroy( &ids);
  IGRAPH_FINALLY_CLEAN(1);

  return res;
}

igraph_integer_t se2_partition_median_community_size(
  se2_partition const* partition)
{
  if (partition->n_labels == 1) {
    return partition->n_nodes;
  }

  igraph_vector_int_t community_sizes;
  se2_iterator label_iter;
  igraph_integer_t res = 0;

  SE2_THREAD_CHECK_RETURN(
    se2_iterator_random_label_init( &label_iter, partition, 0), 0);
  IGRAPH_FINALLY(se2_iterator_destroy, &label_iter);
  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_init( &community_sizes, partition->n_labels), 0);
  IGRAPH_FINALLY(igraph_vector_int_destroy, &community_sizes);

  igraph_integer_t label_id;
  igraph_integer_t label_i = 0;
  while ((label_id = se2_iterator_next( &label_iter)) != -1) {
    VECTOR(community_sizes)[label_i] =
      se2_partition_community_size(partition, label_id);
    label_i++;
  }
  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_resize( &community_sizes, label_i), 0);

  res = se2_vector_int_median( &community_sizes);

  se2_iterator_destroy( &label_iter);
  igraph_vector_int_destroy( &community_sizes);
  IGRAPH_FINALLY_CLEAN(2);

  return res;
}

void se2_partition_merge_labels(se2_partition* partition, igraph_integer_t c1,
                                igraph_integer_t c2)
{
  // Ensure smaller community engulfed by larger community. Not necessary.
  if (se2_partition_community_size(partition, c2) >
      se2_partition_community_size(partition, c1)) {
    igraph_integer_t swp = c1;
    c1 = c2;
    c2 = swp;
  }

  for (igraph_integer_t i = 0; i < partition->n_nodes; i++) {
    if (VECTOR(* partition->stage)[i] == c2) {
      VECTOR(* partition->stage)[i] = c1;
    }
  }

  se2_partition_free_label(partition, c2);
}

// Move nodes in mask to new label.
igraph_error_t se2_partition_relabel_mask(se2_partition* partition,
    igraph_vector_bool_t const* mask)
{
  igraph_integer_t label = se2_partition_new_label(partition);
  SE2_THREAD_STATUS();
  for (igraph_integer_t i = 0; i < partition->n_nodes; i++) {
    if (VECTOR(* mask)[i]) {
      VECTOR(* partition->stage)[i] = label;
    }
  }

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_partition_recount_community_sizes(
  se2_partition* partition)
{
  partition->n_labels = se2_count_labels(partition->reference,
                                         partition->community_sizes);
  SE2_THREAD_STATUS();
  partition->max_label =
    igraph_vector_int_size(partition->community_sizes) - 1;

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_partition_commit_changes(se2_partition* partition)
{
  SE2_THREAD_CHECK(igraph_vector_int_update(partition->reference,
                   partition->stage));
  SE2_THREAD_CHECK(se2_partition_recount_community_sizes(partition));

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_reindex_membership(igraph_vector_int_t* membership)
{
  igraph_vector_int_t indices;
  igraph_integer_t n_nodes = igraph_vector_int_size(membership);

  SE2_THREAD_CHECK(igraph_vector_int_init( &indices, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &indices);

  SE2_THREAD_CHECK(
    igraph_vector_int_qsort_ind(membership, &indices, IGRAPH_ASCENDING));

  igraph_integer_t c_old, c_new = -1, c_prev_node = -1;
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    c_old = VECTOR(* membership)[VECTOR(indices)[i]];
    if (c_old != c_prev_node) {
      c_new++;
      c_prev_node = c_old;
    }
    VECTOR(* membership)[VECTOR(indices)[i]] = c_new;
  }

  igraph_vector_int_destroy( &indices);
  IGRAPH_FINALLY_CLEAN(1);

  return IGRAPH_SUCCESS;
}

/* Save the state of the current working partition's committed changes to the
partition store.

NOTE: This saves only the membership ids for each node so it goes from a
se2_partition to an igraph vector despite both arguments being
"partitions". */
igraph_error_t se2_partition_store(se2_partition const* working_partition,
                                   igraph_vector_int_list_t* partition_store,
                                   igraph_integer_t const idx)
{
  igraph_vector_int_t* partition_state =
    igraph_vector_int_list_get_ptr(partition_store, idx);

  SE2_THREAD_CHECK(
    igraph_vector_int_update(partition_state, working_partition->reference));

  SE2_THREAD_CHECK(se2_reindex_membership(partition_state));

  return IGRAPH_SUCCESS;
}
