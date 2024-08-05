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

#ifndef SE2_MODES_H
#define SE2_MODES_H

#include <speak_easy_2.h>
#include "se2_partitions.h"

typedef enum {
  SE2_TYPICAL = 0,
  SE2_BUBBLE,
  SE2_MERGE,
  SE2_NURTURE,
  SE2_NUM_MODES
} se2_mode;

typedef struct {
  se2_mode mode;
  igraph_integer_t* time_since_last;
  igraph_bool_t allowed_to_merge;
  igraph_real_t max_prev_merge_threshold;
  igraph_bool_t is_partition_stable;
  igraph_bool_t has_partition_changed;
  igraph_bool_t bubbling_has_peaked;
  igraph_integer_t smallest_community_to_bubble;
  igraph_integer_t time_since_bubbling_peaked;
  igraph_integer_t max_labels_after_bubbling;
  igraph_integer_t labels_after_last_bubbling;
  igraph_integer_t post_intervention_count;
  igraph_integer_t n_partitions;
  igraph_bool_t intervention_event;
} se2_tracker;

igraph_error_t se2_tracker_init(se2_tracker* tracker,
                                se2_options const* opts);
void se2_tracker_destroy(se2_tracker* tracker);
igraph_integer_t se2_tracker_mode(se2_tracker const* tracker);
igraph_bool_t se2_do_terminate(se2_tracker* tracker);
igraph_bool_t se2_do_save_partition(se2_tracker* tracker);
igraph_error_t se2_mode_run_step(
  se2_neighs const* graph,
  se2_partition* partition,
  se2_tracker* tracker,
  igraph_integer_t const time);

#endif
