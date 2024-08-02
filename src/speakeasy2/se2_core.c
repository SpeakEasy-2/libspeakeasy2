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

#include <speak_easy_2.h>

#ifdef SE2PAR
#  undef SE2PAR
#endif

#if IGRAPH_THREAD_SAFE
#   define SE2PAR
#endif

#ifdef SE2PAR
#  include <pthread.h>
#endif

#include "se2_print.h"
#include "se2_neighborlist.h"
#include "se2_seeding.h"
#include "se2_random.h"
#include "se2_modes.h"
#include "se2_reweigh_graph.h"

igraph_bool_t greeting_printed = false;

#define SE2_SET_OPTION(opts, field, default) \
    (opts->field) = (opts)->field ? (opts)->field : (default)

static igraph_error_t se2_core(igraph_vector_int_list_t const* graph,
                               igraph_vector_list_t const* weights,
                               igraph_vector_int_list_t* partition_list,
                               igraph_integer_t const partition_offset,
                               se2_options const* opts)
{
  se2_tracker* tracker = se2_tracker_init(opts);
  igraph_vector_int_t* ic_store = &VECTOR(* partition_list)[partition_offset];
  se2_partition* working_partition = se2_partition_init(ic_store);

  igraph_integer_t partition_idx = partition_offset;
  for (igraph_integer_t time = 0; !se2_do_terminate(tracker); time++) {
    se2_mode_run_step(graph, weights, working_partition, tracker, time);
    if (se2_do_save_partition(tracker)) {
      se2_partition_store(working_partition, partition_list, partition_idx);
      partition_idx++;
    }
  }

  se2_tracker_destroy(tracker);
  se2_partition_destroy(working_partition);

  return IGRAPH_SUCCESS;
}

struct represent_parameters {
  igraph_integer_t tid;
  se2_options* opts;
  igraph_integer_t n_partitions;
  igraph_vector_int_list_t* partition_store;
  igraph_matrix_t* nmi_sum_accumulator;
};

static void* se2_thread_mrp(void* parameters)
{
  struct represent_parameters* p = (struct represent_parameters*)parameters;
  igraph_real_t nmi;

  igraph_integer_t n_threads = p->opts->max_threads;
  for (igraph_integer_t i = p->tid; i < p->n_partitions; i += n_threads) {
    for (igraph_integer_t j = (i + 1); j < p->n_partitions; j++) {
      igraph_compare_communities(
        igraph_vector_int_list_get_ptr(p->partition_store, i),
        igraph_vector_int_list_get_ptr(p->partition_store, j),
        &nmi,
        IGRAPH_COMMCMP_NMI);
      MATRIX(* p->nmi_sum_accumulator, i, p->tid) += nmi;
      MATRIX(* p->nmi_sum_accumulator, j, p->tid) += nmi;
    }
  }

  return NULL;
}

static void se2_most_representative_partition(igraph_vector_int_list_t const
    *partition_store, igraph_integer_t const n_partitions,
    igraph_vector_int_t* most_representative_partition,
    se2_options const* opts, igraph_integer_t const subcluster)
{
  igraph_vector_int_t* selected_partition;
  igraph_matrix_t nmi_sum_accumulator;
  igraph_vector_t nmi_sums;
  igraph_integer_t idx = 0;
  igraph_real_t max_nmi = -1;
  igraph_real_t mean_nmi = 0;

  igraph_matrix_init( &nmi_sum_accumulator, n_partitions, opts->max_threads);
  igraph_vector_init( &nmi_sums, n_partitions);

  struct represent_parameters args[opts->max_threads];

#ifdef SE2PAR
  pthread_t threads[opts->max_threads];
#endif
  for (igraph_integer_t tid = 0; tid < opts->max_threads; tid++) {
    args[tid].tid = tid;
    args[tid].opts = (se2_options*)opts;
    args[tid].n_partitions = n_partitions;
    args[tid].partition_store = (igraph_vector_int_list_t*)partition_store;
    args[tid].nmi_sum_accumulator = &nmi_sum_accumulator;

#ifdef SE2PAR
    pthread_create( &threads[tid], NULL, se2_thread_mrp, (void*) &args[tid]);
#else
    se2_thread_mrp((void*) &args[tid]);
#endif
  }

#ifdef SE2PAR
  for (igraph_integer_t tid = 0; tid < opts->max_threads; tid++) {
    pthread_join(threads[tid], NULL);
  }
#endif

  igraph_matrix_rowsum( &nmi_sum_accumulator, &nmi_sums);

  if (opts->verbose && (subcluster == 0)) {
    mean_nmi = igraph_matrix_sum( &nmi_sum_accumulator);
    mean_nmi /= (n_partitions* (n_partitions - 1));
    se2_printf("Mean of all NMIs is %0.5f.\n", mean_nmi);
  }

  for (igraph_integer_t i = 0; i < n_partitions; i++) {
    if (VECTOR(nmi_sums)[i] > max_nmi) {
      max_nmi = VECTOR(nmi_sums)[i];
      idx = i;
    }
  }

  igraph_matrix_destroy( &nmi_sum_accumulator);
  igraph_vector_destroy( &nmi_sums);

  selected_partition = igraph_vector_int_list_get_ptr(partition_store, idx);

  igraph_integer_t n_nodes = igraph_vector_int_size(selected_partition);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    VECTOR(* most_representative_partition)[i] = VECTOR(* selected_partition)[i];
  }
}

struct bootstrap_params {
  igraph_integer_t tid;
  igraph_integer_t n_nodes;
  igraph_vector_int_list_t* graph;
  igraph_vector_list_t* weights;
  igraph_integer_t subcluster_iter;
  igraph_vector_int_list_t* partition_store;
  se2_options* opts;
#ifdef SE2PAR
  pthread_mutex_t* print_mutex;
#endif
  igraph_vector_int_t* memb;
};

static void* se2_thread_bootstrap(void* parameters)
{
  struct bootstrap_params const* p = (struct bootstrap_params*)parameters;

  igraph_integer_t n_threads = p->opts->max_threads;
  igraph_integer_t independent_runs = p->opts->independent_runs;
  for (igraph_integer_t run_i = p->tid; run_i < independent_runs;
       run_i += n_threads) {
    igraph_integer_t partition_offset = run_i* p->opts->target_partitions;
    igraph_vector_int_t ic_store;
    igraph_vector_int_init( &ic_store, p->n_nodes);

    igraph_rng_t rng, * old_rng;
    old_rng = se2_rng_init( &rng, run_i + p->opts->random_seed);
    igraph_integer_t n_unique = se2_seeding(p->graph, p->opts, &ic_store);
    igraph_vector_int_list_set(p->partition_store, partition_offset, &ic_store);

    if ((p->opts->verbose) && (!p->subcluster_iter)) {
#ifdef SE2PAR
      pthread_mutex_lock(p->print_mutex);
#endif

      if (!greeting_printed) {
        greeting_printed = true;
        se2_printf("Completed generating initial labels.\n"
                   "Produced %"IGRAPH_PRId" seed labels, "
                   "while goal was %"IGRAPH_PRId".\n\n"
                   "Starting level 1 clustering",
                   n_unique, p->opts->target_clusters);

        if (p->opts->max_threads > 1) {
          se2_printf("; independent runs might not be displayed in order - "
                     "that is okay...\n");
        } else {
          se2_printf("...\n");
        }
      }

      se2_printf("Starting independent run #%"IGRAPH_PRId" of %"IGRAPH_PRId"\n",
                 run_i + 1, p->opts->independent_runs);

#ifdef SE2PAR
      pthread_mutex_unlock(p->print_mutex);
#endif
    }

    se2_core(p->graph, p->weights, p->partition_store, partition_offset,
             p->opts);

    se2_rng_restore( &rng, old_rng);
  }

  return NULL;
}

static void se2_bootstrap(igraph_vector_int_list_t const* graph,
                          igraph_vector_list_t const* weights,
                          igraph_integer_t const subcluster_iter,
                          se2_options const* opts,
                          igraph_vector_int_t* memb)
{
  igraph_integer_t n_nodes = se2_vcount(graph);
  igraph_integer_t n_partitions = opts->target_partitions*
                                  opts->independent_runs;
  igraph_vector_int_list_t partition_store;

  igraph_vector_int_list_init( &partition_store, n_partitions);

  if ((opts->verbose) && (!subcluster_iter) && (opts->multicommunity > 1)) {
    se2_puts("Attempting overlapping clustering.");
  }

#ifdef SE2PAR
  pthread_t threads[opts->max_threads];
  pthread_mutex_t print_mutex;
  pthread_mutex_init( &print_mutex, NULL);
#endif

  struct bootstrap_params args[opts->max_threads];

  for (igraph_integer_t tid = 0; tid < opts->max_threads; tid++) {
    args[tid].tid = tid;
    args[tid].n_nodes = n_nodes;
    args[tid].graph = (igraph_vector_int_list_t*)graph;
    args[tid].weights = (igraph_vector_list_t*)weights;
    args[tid].subcluster_iter = subcluster_iter;
    args[tid].partition_store = &partition_store;
    args[tid].opts = (se2_options*)opts;
#ifdef SE2PAR
    args[tid].print_mutex = &print_mutex;
#endif
    args[tid].memb = memb;

#ifdef SE2PAR
    pthread_create( &threads[tid], NULL, se2_thread_bootstrap,
                    (void*) &args[tid]);
#else
    se2_thread_bootstrap((void*) &args[tid]);
#endif
  }

#ifdef SE2PAR
  for (igraph_integer_t tid = 0; tid < opts->max_threads; tid++) {
    pthread_join(threads[tid], NULL);
  }

  pthread_mutex_destroy( &print_mutex);
#endif

  if ((opts->verbose) && (!subcluster_iter)) {
    se2_printf("\nGenerated %"IGRAPH_PRId" partitions at level 1.\n",
               n_partitions);
  }

  se2_most_representative_partition( &partition_store,
                                     n_partitions,
                                     memb, opts, subcluster_iter);

  igraph_vector_int_list_destroy( &partition_store);
}

static igraph_integer_t default_target_clusters(igraph_vector_int_list_t
    const* graph)
{
  igraph_integer_t const n_nodes = igraph_vector_int_list_size(graph);
  if (n_nodes < 10) {
    return n_nodes;
  }

  if ((n_nodes / 100) < 10) {
    return 10;
  }

  return n_nodes / 100;
}

static igraph_integer_t default_max_threads(igraph_integer_t const runs)
{
  igraph_integer_t n_threads = 1;
#ifdef SE2PAR
  n_threads = runs;
#endif
  return n_threads;
}

static void se2_set_defaults(igraph_vector_int_list_t const* graph,
                             se2_options* opts)
{
  SE2_SET_OPTION(opts, independent_runs, 10);
  SE2_SET_OPTION(opts, subcluster, 1);
  SE2_SET_OPTION(opts, multicommunity, 1);
  SE2_SET_OPTION(opts, target_partitions, 5);
  SE2_SET_OPTION(opts, target_clusters, default_target_clusters(graph));
  SE2_SET_OPTION(opts, minclust, 5);
  SE2_SET_OPTION(opts, discard_transient, 3);
  SE2_SET_OPTION(opts, random_seed, RNG_INTEGER(1, 9999));
  SE2_SET_OPTION(opts, max_threads,
                 default_max_threads(opts->independent_runs));
  SE2_SET_OPTION(opts, node_confidence, false);
  SE2_SET_OPTION(opts, verbose, false);
}

static void se2_collect_community_members(igraph_vector_int_t const* memb,
    igraph_vector_int_t* idx, igraph_integer_t const comm)
{
  igraph_integer_t n_memb = 0;
  for (igraph_integer_t i = 0; i < igraph_vector_int_size(memb); i++) {
    n_memb += VECTOR(* memb)[i] == comm;
  }

  igraph_vector_int_init(idx, n_memb);
  igraph_integer_t count = 0;
  for (igraph_integer_t i = 0; i < igraph_vector_int_size(memb); i++) {
    if (VECTOR(* memb)[i] == comm) {
      VECTOR(* idx)[count] = i;
      count++;
    }
  }
}

static void se2_subgraph_from_community(
  igraph_vector_int_list_t const* origin,
  igraph_vector_list_t const* origin_weights,
  igraph_vector_int_list_t* subgraph,
  igraph_vector_list_t* sub_weights,
  igraph_vector_int_t const* members)
{
  igraph_integer_t const n_membs = igraph_vector_int_size(members);

  igraph_vector_int_list_init(subgraph, n_membs);
  if (origin_weights) {
    igraph_vector_list_init(sub_weights, n_membs);
  }

  for (igraph_integer_t i = 0; i < n_membs; i++) {
    igraph_integer_t node_id = VECTOR(* members)[i];
    igraph_vector_int_t* neighs = &VECTOR(* origin)[node_id];
    igraph_vector_int_t* new_neighs = &VECTOR(* subgraph)[i];
    igraph_integer_t const n_neighs = igraph_vector_int_size(neighs);

    igraph_vector_int_resize(new_neighs, n_neighs);
    if (origin_weights) {
      igraph_vector_resize( &VECTOR(* sub_weights)[i], n_neighs);
    }

    igraph_integer_t count = 0;
    igraph_integer_t pos;
    for (igraph_integer_t j = 0; j < n_neighs; j++) {
      if (igraph_vector_int_search(members, 0, VECTOR(* neighs)[j], &pos)) {
        VECTOR(* new_neighs)[count] = pos;
        if (origin_weights) {
          LIST(* sub_weights, i, count) = LIST(* origin_weights, node_id, j);
        }
        count++;
      }
    }
  }
}

/* For hierarchical clustering, each community from the previous level gets
clustered. Each of these clusters gets a "private scope" set of labels starting
at 0. These must be relabeled to a global scope. */
static void se2_relabel_hierarchical_communities(igraph_vector_int_t const*
    prev_membs, igraph_vector_int_t* level_membs)
{
  igraph_integer_t const n_comms = igraph_vector_int_max(prev_membs) -
                                   igraph_vector_int_min(prev_membs);

  igraph_integer_t prev_max = 0;
  igraph_integer_t curr_max = 0;
  for (igraph_integer_t i = 0; i < n_comms; i++) {
    igraph_vector_int_t member_ids;
    se2_collect_community_members(prev_membs, &member_ids, i);
    for (igraph_integer_t j = 0; j < igraph_vector_int_size( &member_ids); j++) {
      igraph_integer_t local_label = VECTOR(* level_membs)[VECTOR(member_ids)[j]];

      VECTOR(* level_membs)[VECTOR(member_ids)[j]] += prev_max;
      if ((local_label + prev_max) > curr_max) {
        curr_max = local_label + prev_max;
      }
    }
    prev_max = curr_max + 1;
    igraph_vector_int_destroy( &member_ids);
  }
}

/**
\brief speakeasy 2 community detection.

\param graph the graph to cluster.
\param weights optional weights if the graph is weighted, use NULL for
  unweighted.
\param opts a speakeasy options structure (see speak_easy_2.h).
\param memb the resulting membership vector.

\return Error code:
         Always returns success.
*/
igraph_error_t speak_easy_2(igraph_vector_int_list_t const* graph,
                            igraph_vector_list_t const* weights,
                            se2_options* opts, igraph_matrix_int_t* memb)
{
  se2_set_defaults(graph, opts);

#ifndef SE2PAR
  if (opts->max_threads > 1) {
    se2_warn("SpeakEasy 2 was not compiled with thread support. "
             "Ignoring `max_threads`.\n\n"
             "To suppress this warning do not set `max_threads`\n.");
    opts->max_threads = 1;
  }
#endif

  if (opts->verbose) {
    igraph_bool_t isweighted = false;
    if (weights) {
      for (igraph_integer_t i = 0; i < igraph_vector_list_size(weights); i++) {
        igraph_vector_t neigh_weights = VECTOR(* weights)[i];
        for (igraph_integer_t j = 0; j < igraph_vector_size( &neigh_weights);
             j++) {
          if (VECTOR(neigh_weights)[j] != 1) {
            isweighted = true;
            break;
          }
        }

        if (isweighted) {
          break;
        }
      }
    }

    igraph_integer_t possible_edges = se2_vcount(graph);
    possible_edges *= possible_edges;
    igraph_real_t edge_density = (igraph_real_t)se2_ecount(graph) /
                                 possible_edges;
    se2_printf("Approximate edge density is %g.\n"
               "Input type treated as %s.\n\n"
               "Calling main routine at level 1.\n",
               edge_density, isweighted ? "weighted" : "unweighted");
  }

  igraph_matrix_int_init(memb, opts->subcluster, se2_vcount(graph));

  igraph_vector_int_t level_memb;
  igraph_vector_int_init( &level_memb, se2_vcount(graph));
  se2_reweigh(graph, weights);
  se2_bootstrap(graph, weights, 0, opts, &level_memb);
  igraph_matrix_int_set_row(memb, &level_memb, 0);

  for (igraph_integer_t level = 1; level < opts->subcluster; level++) {
    if (opts->verbose) {
      se2_printf("\nSubclustering at level %"IGRAPH_PRId".\n", level + 1);
    }

    igraph_vector_int_t prev_memb;
    igraph_vector_int_init( &prev_memb, igraph_matrix_int_ncol(memb));
    igraph_matrix_int_get_row(memb, &prev_memb, level - 1);

    igraph_integer_t const n_comms = igraph_vector_int_max( &prev_memb) -
                                     igraph_vector_int_min( &prev_memb);
    for (igraph_integer_t comm = 0; comm < n_comms; comm++) {
      igraph_vector_int_t member_ids;
      se2_collect_community_members( &prev_memb, &member_ids, comm);
      igraph_integer_t const n_membs = igraph_vector_int_size( &member_ids);

      if (n_membs <= opts->minclust) {
        for (igraph_integer_t i = 0; i < n_membs; i++) {
          VECTOR(level_memb)[VECTOR(member_ids)[i]] = 0;
        }

        igraph_vector_int_destroy( &member_ids);
        continue;
      }

      igraph_vector_int_list_t subgraph;
      igraph_vector_list_t subgraph_weights;
      igraph_vector_int_t subgraph_memb;

      igraph_vector_int_init( &subgraph_memb, n_membs);
      se2_subgraph_from_community(graph, weights, &subgraph, &subgraph_weights,
                                  &member_ids);

      se2_reweigh( &subgraph, weights ? &subgraph_weights : NULL);
      se2_bootstrap( &subgraph, weights ? &subgraph_weights : NULL, level,
                     opts, &subgraph_memb);

      for (igraph_integer_t i = 0; i < igraph_vector_int_size( &subgraph_memb);
           i++) {
        VECTOR(level_memb)[VECTOR(member_ids)[i]] = VECTOR(subgraph_memb)[i];
      }

      igraph_vector_int_destroy( &member_ids);
      if (weights) {
        igraph_vector_list_destroy( &subgraph_weights);
      }
      igraph_vector_int_destroy( &subgraph_memb);
      igraph_vector_int_list_destroy( &subgraph);
    }

    se2_relabel_hierarchical_communities( &prev_memb, &level_memb);
    igraph_matrix_int_set_row(memb, &level_memb, level);
    igraph_vector_int_destroy( &prev_memb);
  }

  igraph_vector_int_destroy( &level_memb);

  if (opts->verbose) {
    se2_printf("\n");
  }

  return IGRAPH_SUCCESS;
}
