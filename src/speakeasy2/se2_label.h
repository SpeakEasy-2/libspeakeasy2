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

#ifndef SE2_LABEL_H
#define SE2_LABEL_H

#include <speak_easy_2.h>
#include "se2_partitions.h"

void se2_find_most_specific_labels(se2_neighs const* graph,
                                   se2_partition* partition,
                                   igraph_real_t const fraction_nodes_to_label);

void se2_relabel_worst_nodes(se2_neighs const* graph,
                             se2_partition* partition,
                             igraph_real_t const fraction_nodes_to_label);

void se2_burst_large_communities(se2_partition* partition,
                                 igraph_real_t const fraction_nodes_to_move,
                                 igraph_integer_t const min_community_size);

igraph_bool_t se2_merge_well_connected_communities(se2_neighs const* graph,
    se2_partition* partition, igraph_real_t* prev_merge_threshold);

#endif
