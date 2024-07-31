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

typedef struct se2_tracker se2_tracker;

se2_tracker* se2_tracker_init(se2_options const* opts);
void se2_tracker_destroy(se2_tracker* tracker);
igraph_integer_t se2_tracker_mode(se2_tracker const* tracker);
igraph_bool_t se2_do_terminate(se2_tracker* tracker);
igraph_bool_t se2_do_save_partition(se2_tracker* tracker);
void se2_mode_run_step(igraph_t const* graph, igraph_vector_t const* weights,
                       se2_partition* partition, se2_tracker* tracker, igraph_integer_t const time);

#endif
