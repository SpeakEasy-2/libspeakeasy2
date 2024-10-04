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

#ifndef SE2_PARTITIONS_H
#define SE2_PARTITIONS_H

#include <speak_easy_2.h>

// LABEL(partition)[node_id] gets the reference label for the node.
#define LABEL(partition) (VECTOR(*(partition).reference))
#define STAGE(partition) (VECTOR(*(partition).stage))

/* WARNING: Expose fields only so macros can be used for performance.  Ideally,
treated as opaque.

Basic idea is there is a reference membership vector, which stores the labels
of nodes from before the current time step, and a stage membership vector,
where proposed labels are set. At the end of a time step, the staged changes
are committed to the reference membership vector. */
typedef struct {
  igraph_vector_int_t* stage;     // Working membership
  igraph_vector_int_t* reference; // Fixed previous membership
  igraph_integer_t n_nodes;
  igraph_integer_t n_labels;
  igraph_vector_int_t* community_sizes;
  igraph_integer_t max_label;
  igraph_matrix_t* local_labels_heard;
  igraph_vector_t* global_labels_heard;
} se2_partition;

typedef struct {
  igraph_vector_int_t* ids;
  igraph_integer_t pos;
  igraph_integer_t n_total;
  igraph_integer_t n_iter;
  igraph_bool_t owns_ids;
} se2_iterator;

igraph_error_t se2_partition_init(se2_partition* partition,
  se2_neighs const* graph, igraph_vector_int_t const* initial_labels);
void se2_partition_destroy(se2_partition* partition);
igraph_error_t se2_partition_store(se2_partition const* working_partition,
  igraph_vector_int_list_t* partition_store, igraph_integer_t const index);
igraph_real_t se2_partition_score_label(se2_neighs const* graph,
  se2_partition const* partition, igraph_integer_t const node_id,
  igraph_integer_t const label_id);

igraph_error_t se2_iterator_from_vector(
  se2_iterator* iter, igraph_vector_int_t* ids, igraph_integer_t n_iter);
igraph_error_t se2_iterator_random_node_init(se2_iterator* iter,
  se2_partition const* partition, igraph_real_t const proportion);
igraph_error_t se2_iterator_random_label_init(se2_iterator* iter,
  se2_partition const* partition, igraph_real_t const proportion);
igraph_error_t se2_iterator_k_worst_fit_nodes_init(se2_iterator* iter,
  se2_neighs const* graph, se2_partition const* partition,
  igraph_integer_t const k, igraph_real_t const proportion,
  igraph_vector_int_t* best_fit_nodes);

igraph_integer_t se2_iterator_next(se2_iterator* iterator);
void se2_iterator_reset(se2_iterator* iterator);
void se2_iterator_shuffle(se2_iterator* iterator);
void se2_iterator_destroy(se2_iterator* iterator);

igraph_integer_t se2_partition_n_nodes(se2_partition const* partition);
igraph_integer_t se2_partition_n_labels(se2_partition const* partition);
igraph_integer_t se2_partition_max_label(se2_partition const* partition);
igraph_integer_t se2_partition_new_label(se2_partition* partition);
igraph_integer_t se2_partition_community_size(
  se2_partition const* partition, igraph_integer_t const label);
igraph_real_t se2_partition_median_community_size(
  se2_partition const* partition);

igraph_real_t se2_vector_median(igraph_vector_t const* vec);
igraph_real_t se2_vector_int_median(igraph_vector_int_t const* vec);

void se2_partition_merge_labels(
  se2_partition* partition, igraph_integer_t c1, igraph_integer_t c2);
igraph_error_t se2_partition_relabel_mask(
  se2_partition* partition, igraph_vector_bool_t const* mask);
void se2_partition_add_to_stage(se2_partition* partition,
  igraph_integer_t const node_id, igraph_integer_t const label);
igraph_error_t se2_partition_commit_changes(
  se2_partition* partition, se2_neighs const* graph);

#endif
