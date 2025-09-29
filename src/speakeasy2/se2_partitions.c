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

#include "igraph_error.h"
#include "igraph_vector.h"
#include "se2_error_handling.h"
#include "se2_neighborlist.h"
#include "se2_random.h"

#define MAX(a, b) (a) > (b) ? (a) : (b)

static igraph_error_t se2_count_labels(
  igraph_vector_int_t const* membership, igraph_vector_int_t* community_sizes)
{
  igraph_integer_t const n_nodes = igraph_vector_int_size(membership);
  igraph_integer_t n_labels = igraph_vector_int_max(membership) + 1;

  SE2_THREAD_CHECK_RETURN(
    igraph_vector_int_resize(community_sizes, n_labels), 0);

  igraph_vector_int_null(community_sizes);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    VECTOR(*community_sizes)[VECTOR(*membership)[i]]++;
  }

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_count_global_labels(se2_neighs const* graph,
  igraph_vector_int_t const* labels, igraph_vector_t* global_labels_heard)
{
  igraph_integer_t const n_nodes = graph->n_nodes;
  igraph_integer_t const n_labels = igraph_vector_int_max(labels) + 1;

  if (igraph_vector_size(global_labels_heard) != n_labels) {
    SE2_THREAD_CHECK(igraph_vector_resize(global_labels_heard, n_labels));
  }

  igraph_vector_null(global_labels_heard);
  igraph_real_t* global_labels = VECTOR(*global_labels_heard);
  igraph_integer_t const* labels_i = VECTOR(*labels);
  for (igraph_integer_t node_id = 0; node_id < n_nodes; node_id++) {
    igraph_integer_t* neighbors =
      graph->neigh_list ? VECTOR(VECTOR(*graph->neigh_list)[node_id]) : NULL;
    igraph_real_t* weights =
      graph->weights ? VECTOR(VECTOR(*graph->weights)[node_id]) : NULL;
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(*graph, node_id); j++) {
      global_labels[labels_i[neighbors ? neighbors[j] : j]] +=
        weights ? weights[j] : 1.0;
    }
  }

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_partition_init(se2_partition* partition,
  se2_neighs const* graph, igraph_vector_int_t const* initial_labels)
{
  igraph_integer_t const n_nodes = igraph_vector_int_size(initial_labels);
  igraph_integer_t const n_labels = igraph_vector_int_max(initial_labels) + 1;

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

  igraph_vector_t* global_labels_heard =
    igraph_malloc(sizeof(*global_labels_heard));
  SE2_THREAD_CHECK_OOM(global_labels_heard);
  IGRAPH_FINALLY(igraph_free, global_labels_heard);

  SE2_THREAD_CHECK(igraph_vector_int_init(reference, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, reference);
  SE2_THREAD_CHECK(igraph_vector_int_init(stage, n_nodes));
  IGRAPH_FINALLY(igraph_vector_int_destroy, stage);
  SE2_THREAD_CHECK(igraph_vector_int_init(community_sizes, 0));
  IGRAPH_FINALLY(igraph_vector_int_destroy, community_sizes);

  SE2_THREAD_CHECK(igraph_vector_int_update(reference, initial_labels));
  SE2_THREAD_CHECK(igraph_vector_int_update(stage, initial_labels));

  SE2_THREAD_CHECK(se2_count_labels(initial_labels, community_sizes));

  partition->n_nodes = n_nodes;
  partition->n_labels = n_labels;
  partition->reference = reference;
  partition->stage = stage;
  partition->community_sizes = community_sizes;
  partition->global_labels_heard = global_labels_heard;
  partition->repack = true;

  SE2_THREAD_CHECK(igraph_vector_init(global_labels_heard, n_labels));
  IGRAPH_FINALLY(igraph_vector_destroy, global_labels_heard);

  SE2_THREAD_CHECK(
    se2_count_global_labels(graph, initial_labels, global_labels_heard));

  IGRAPH_FINALLY_CLEAN(8);

  return IGRAPH_SUCCESS;
}

void se2_partition_destroy(se2_partition* partition)
{
  igraph_vector_int_destroy(partition->reference);
  igraph_vector_int_destroy(partition->stage);
  igraph_vector_int_destroy(partition->community_sizes);
  igraph_vector_destroy(partition->global_labels_heard);

  igraph_free(partition->reference);
  igraph_free(partition->stage);
  igraph_free(partition->community_sizes);
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

  for (igraph_integer_t node_id = 0; node_id < partition->n_nodes; node_id++) {
    igraph_integer_t label_id = LABEL(*partition)[node_id];
    igraph_real_t actual = 0;
    for (igraph_integer_t i = 0; i < N_NEIGHBORS(*graph, node_id); i++) {
      actual +=
        LABEL(*partition)[NEIGHBOR(*graph, node_id, i)] == label_id ?
          WEIGHT(*graph, node_id, i) :
          0;
    }
    igraph_real_t expected = VECTOR(*partition->global_labels_heard)[label_id];
    igraph_real_t norm_factor =
      VECTOR(*graph->kin)[node_id] / graph->total_weight;
    VECTOR(label_quality)[node_id] = actual - (norm_factor * expected);
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

void se2_partition_add_to_stage(se2_partition* partition,
  igraph_integer_t const node_id, igraph_integer_t const label)
{
  VECTOR(*partition->stage)[node_id] = label;
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
  if (c1 > c2) {
    igraph_integer_t swp = c1;
    c1 = c2;
    c2 = swp;
  }

  for (igraph_integer_t i = 0; i < partition->n_nodes; i++) {
    if (LABEL(*partition)[i] == c2) {
      STAGE(*partition)[i] = c1;
    }
  }
}

static igraph_error_t se2_repack_membership(igraph_vector_int_t* membership)
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

igraph_error_t se2_partition_commit_changes(
  se2_partition* partition, se2_neighs const* graph)
{
  if (partition->repack) {
    SE2_THREAD_CHECK(se2_repack_membership(partition->stage));
  }
  SE2_THREAD_CHECK(
    se2_count_labels(partition->stage, partition->community_sizes));
  SE2_THREAD_CHECK(se2_count_global_labels(
    graph, partition->stage, partition->global_labels_heard));
  SE2_THREAD_CHECK(
    igraph_vector_int_update(partition->reference, partition->stage));
  partition->n_labels = igraph_vector_int_size(partition->community_sizes);

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

  return IGRAPH_SUCCESS;
}
