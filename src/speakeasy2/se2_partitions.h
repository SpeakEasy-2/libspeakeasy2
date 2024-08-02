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

#include <igraph.h>

// LABEL(partition)[node_id] gets the reference label for the node.
#define LABEL(partition) (VECTOR(*(partition).reference))
#define RELABEL(partition) (VECTOR(*(partition).stage))

/* WARNING: Expose fields only so macros can be used for performance.  Ideally,
treated as opaque.

Basic idea is there is a reference membership vector, which stores the labels
of nodes from before the current time step, and a stage membership vector,
where proposed labels are set. At the end of a time step, the staged changes
are committed to the reference membership vectore. */
typedef struct se2_partition {
  igraph_vector_int_t* stage;           // Working membership
  igraph_vector_int_t* reference;       // Fixed previous membership
  igraph_vector_t* label_quality;       // Label quality for stage.
  igraph_integer_t n_nodes;
  igraph_integer_t n_labels;
  igraph_vector_int_t* community_sizes;
  igraph_integer_t max_label;
} se2_partition;

typedef struct se2_iterator se2_iterator;

se2_partition* se2_partition_init(igraph_vector_int_t* initial_labels);
void se2_partition_destroy(se2_partition* partition);
void se2_partition_store(se2_partition const* working_partition,
                         igraph_vector_int_list_t* partition_store,
                         igraph_integer_t const index);

se2_iterator* se2_iterator_from_vector(igraph_vector_int_t* ids,
                                       igraph_integer_t n_iter);
se2_iterator* se2_iterator_random_node_init(se2_partition const* partition,
    igraph_real_t const proportion);
se2_iterator* se2_iterator_random_label_init(se2_partition const* partition,
    igraph_real_t const proportion);
se2_iterator* se2_iterator_k_worst_fit_nodes_init(se2_partition const
    *partition,
    igraph_integer_t const k);

igraph_integer_t se2_iterator_next(se2_iterator* iterator);
void se2_iterator_reset(se2_iterator* iterator);
void se2_iterator_shuffle(se2_iterator* iterator);
void se2_iterator_destroy(se2_iterator* iterator);

igraph_integer_t se2_partition_n_nodes(se2_partition const* partition);
igraph_integer_t se2_partition_n_labels(se2_partition const* partition);
igraph_integer_t se2_partition_max_label(se2_partition const* partition);
igraph_integer_t se2_partition_new_label(se2_partition* partition);
igraph_integer_t se2_partition_community_size(se2_partition const* partition,
    igraph_integer_t const label);
igraph_integer_t se2_partition_median_community_size(se2_partition const
    *partition);

igraph_real_t se2_vector_median(igraph_vector_t const* vec);
igraph_real_t se2_vector_int_median(igraph_vector_int_t const* vec);

void se2_partition_merge_labels(se2_partition* partition, igraph_integer_t c1,
                                igraph_integer_t c2);
void se2_partition_relabel_mask(se2_partition* partition,
                                igraph_vector_bool_t const* mask);
void se2_partition_add_to_stage(se2_partition* partition,
                                igraph_integer_t const node_id,
                                igraph_integer_t const label,
                                igraph_real_t specificity);
void se2_partition_commit_changes(se2_partition* partition);

#endif
