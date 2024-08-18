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

#include "se2_modes.h"
#include "se2_label.h"
#include "se2_error_handling.h"

#define TYPICAL_FRACTION_NODES_TO_UPDATE 0.9
#define NURTURE_FRACTION_NODES_TO_UPDATE 0.9
#define FRACTION_NODES_TO_BUBBLE 0.9
#define POST_PEAK_BUBBLE_LIMIT 2

igraph_error_t se2_tracker_init(se2_tracker* tracker, se2_options const* opts)
{
  igraph_integer_t* time_since_mode_tracker = igraph_calloc(SE2_NUM_MODES,
    sizeof(* time_since_mode_tracker));
  SE2_THREAD_CHECK_OOM(time_since_mode_tracker);
  IGRAPH_FINALLY(igraph_free, time_since_mode_tracker);

  tracker->mode = SE2_TYPICAL;
  tracker->time_since_last = time_since_mode_tracker;
  tracker->allowed_to_merge = false;
  tracker->max_prev_merge_threshold = 0;
  tracker->is_partition_stable = false;
  tracker->has_partition_changed = true;
  tracker->bubbling_has_peaked = false;
  tracker->smallest_community_to_bubble = opts->minclust;
  tracker->time_since_bubbling_peaked = 0;
  tracker->max_labels_after_bubbling = 0;
  tracker->labels_after_last_bubbling = 0;
  tracker->post_intervention_count = -(opts->discard_transient) + 1;
  tracker->n_partitions = opts->target_partitions;
  tracker->intervention_event = false;

  IGRAPH_FINALLY_CLEAN(1);

  return IGRAPH_SUCCESS;
}

void se2_tracker_destroy(se2_tracker* tracker)
{
  igraph_free(tracker->time_since_last);
}

igraph_integer_t se2_tracker_mode(se2_tracker const* tracker)
{
  return tracker->mode;
}

igraph_bool_t se2_do_terminate(se2_tracker* tracker)
{
  // Should never be greater than n_partitions.
  return tracker->post_intervention_count >= tracker->n_partitions;
}

igraph_bool_t se2_do_save_partition(se2_tracker* tracker)
{
  return tracker->intervention_event;
}

static void se2_select_mode(igraph_integer_t const time, se2_tracker* tracker)
{
  tracker->mode = SE2_TYPICAL; // Default

  if (time < 20) {
    return;
  }

  if (tracker->allowed_to_merge) {
    if ((tracker->time_since_last[SE2_MERGE] > 1) &&
        (tracker->time_since_last[SE2_BUBBLE] > 3)) {
      tracker->mode = SE2_MERGE;
      return;
    }
  } else {
    if ((tracker->time_since_last[SE2_MERGE] > 2) &&
        (tracker->time_since_last[SE2_BUBBLE] > 14)) {
      tracker->mode = SE2_BUBBLE;
      return;
    }

    if ((tracker->time_since_last[SE2_MERGE] > 1) &&
        (tracker->time_since_last[SE2_BUBBLE] < 5)) {
      tracker->mode = SE2_NURTURE;
      return;
    }
  }
}

static void se2_post_step_hook(se2_tracker* tracker)
{
  tracker->intervention_event = false;
  tracker->time_since_last[tracker->mode] = 0;
  for (igraph_integer_t i = 0; i < SE2_NUM_MODES; i++) {
    tracker->time_since_last[i]++;
  }

  switch (tracker->mode) {
  case SE2_BUBBLE:
    if (!tracker->bubbling_has_peaked) {
      if ((tracker->max_labels_after_bubbling >
           (tracker->labels_after_last_bubbling * 0.9))) {
        tracker->bubbling_has_peaked = true;
      }

      if (tracker->labels_after_last_bubbling >
          tracker->max_labels_after_bubbling) {
        tracker->max_labels_after_bubbling =
          tracker->labels_after_last_bubbling;
      }
    }

    if (tracker->bubbling_has_peaked) {
      tracker->time_since_bubbling_peaked++;
      if (tracker->time_since_bubbling_peaked >= POST_PEAK_BUBBLE_LIMIT) {
        tracker->time_since_bubbling_peaked = 0;
        tracker->allowed_to_merge = true;
      }
    }
    break;

  case SE2_MERGE:
    tracker->bubbling_has_peaked = false;
    tracker->time_since_bubbling_peaked = 0;
    tracker->max_labels_after_bubbling = 0;

    if (tracker->is_partition_stable) {
      tracker->allowed_to_merge = false;
      tracker->post_intervention_count++;
      if (tracker->post_intervention_count > 0) {
        tracker->intervention_event = true;
      }
    }

  default: // Just to "handle" all cases even though not needed.
    break;
  }
}

static igraph_error_t se2_typical_mode(se2_neighs const* graph,
                                       se2_partition* partition,
                                       se2_tracker* tracker)
{
  if ((tracker->time_since_last[SE2_TYPICAL] == 1) &&
      !tracker->has_partition_changed) {
    return IGRAPH_SUCCESS;
  }

  SE2_THREAD_CHECK(se2_find_most_specific_labels(graph, partition,
                   TYPICAL_FRACTION_NODES_TO_UPDATE,
                   &(tracker->has_partition_changed)));

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_bubble_mode(se2_neighs const* graph,
                                      se2_partition* partition,
                                      se2_tracker* tracker)
{
  SE2_THREAD_CHECK(se2_burst_large_communities(graph, partition,
                   FRACTION_NODES_TO_BUBBLE,
                   tracker->smallest_community_to_bubble));

  tracker->labels_after_last_bubbling = partition->n_labels;

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_merge_mode(se2_neighs const* graph,
                                     se2_partition* partition,
                                     se2_tracker* tracker)
{
  SE2_THREAD_CHECK(se2_merge_well_connected_communities(
                     graph, partition,
                     &(tracker->max_prev_merge_threshold),
                     &(tracker->is_partition_stable)));

  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_nurture_mode(se2_neighs const* graph,
                                       se2_partition* partition)
{
  SE2_THREAD_CHECK(
    se2_relabel_worst_nodes(graph, partition, NURTURE_FRACTION_NODES_TO_UPDATE));

  return IGRAPH_SUCCESS;
}

igraph_error_t se2_mode_run_step(
  se2_neighs const* graph,
  se2_partition* partition,
  se2_tracker* tracker,
  igraph_integer_t const time)
{
  se2_select_mode(time, tracker);

  switch (tracker->mode) {
  case SE2_TYPICAL:
    SE2_THREAD_CHECK(se2_typical_mode(graph, partition, tracker));
    break;
  case SE2_BUBBLE:
    SE2_THREAD_CHECK(se2_bubble_mode(graph, partition, tracker));
    break;
  case SE2_MERGE:
    SE2_THREAD_CHECK(se2_merge_mode(graph, partition, tracker));
    break;
  case SE2_NURTURE:
    SE2_THREAD_CHECK(se2_nurture_mode(graph, partition));
    break;
  case SE2_NUM_MODES:
    // Never occurs.
    break;
  }

  se2_post_step_hook(tracker);

  return IGRAPH_SUCCESS;
}
