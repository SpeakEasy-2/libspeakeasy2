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
# include <pthread.h>
#endif

#include "se2_error_handling.h"
#include "se2_modes.h"
#include "se2_neighborlist.h"
#include "se2_random.h"
#include "se2_reweigh_graph.h"
#include "se2_seeding.h"

igraph_bool_t greeting_printed = false;
igraph_error_t se2_thread_errorcode = IGRAPH_SUCCESS;

#ifdef SE2PAR
pthread_mutex_t se2_error_mutex;
#endif

#define SE2_SET_OPTION(opts, field, default)                                  \
  (opts->field) = (opts)->field ? (opts)->field : (default)

static igraph_error_t se2_core(se2_neighs const* graph,
  igraph_vector_int_list_t* partition_list,
  igraph_integer_t const partition_offset, se2_options const* opts)
{
  se2_tracker tracker;
  se2_partition working_partition;

  SE2_THREAD_CHECK(se2_tracker_init(&tracker, opts));
  IGRAPH_FINALLY(se2_tracker_destroy, &tracker);

  igraph_vector_int_t* ic_store = &VECTOR(*partition_list)[partition_offset];
  SE2_THREAD_CHECK(se2_partition_init(&working_partition, graph, ic_store));
  IGRAPH_FINALLY(se2_partition_destroy, &working_partition);

  igraph_integer_t partition_idx = partition_offset;
  for (igraph_integer_t time = 0; !se2_do_terminate(&tracker); time++) {
    SE2_THREAD_CHECK(
      se2_mode_run_step(graph, &working_partition, &tracker, time));
#ifndef SE2PAR
    if ((time % 32) == 0) {
      SE2_THREAD_CHECK(igraph_allow_interruption());
    }
#endif

    if (se2_do_save_partition(&tracker)) {
      SE2_THREAD_CHECK(se2_partition_store(
        &working_partition, partition_list, partition_idx));
      partition_idx++;
    }
  }

  se2_tracker_destroy(&tracker);
  se2_partition_destroy(&working_partition);
  IGRAPH_FINALLY_CLEAN(2);

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
        igraph_vector_int_list_get_ptr(p->partition_store, j), &nmi,
        IGRAPH_COMMCMP_NMI);
      MATRIX(*p->nmi_sum_accumulator, i, p->tid) += nmi;
      MATRIX(*p->nmi_sum_accumulator, j, p->tid) += nmi;
    }
  }

  return NULL;
}

static igraph_error_t se2_most_representative_partition(
  igraph_vector_int_list_t const* partition_store,
  igraph_integer_t const n_partitions,
  igraph_vector_int_t* most_representative_partition, se2_options const* opts,
  igraph_integer_t const subcluster)
{
  igraph_vector_int_t* selected_partition;
  igraph_matrix_t nmi_sum_accumulator;
  igraph_vector_t nmi_sums;
  igraph_integer_t idx = 0;
  igraph_real_t max_nmi = -1;
  igraph_real_t mean_nmi = 0;

  IGRAPH_CHECK(
    igraph_matrix_init(&nmi_sum_accumulator, n_partitions, opts->max_threads));
  IGRAPH_FINALLY(igraph_matrix_destroy, &nmi_sum_accumulator);
  IGRAPH_CHECK(igraph_vector_init(&nmi_sums, n_partitions));
  IGRAPH_FINALLY(igraph_vector_destroy, &nmi_sums);

  struct represent_parameters* args =
    malloc(sizeof(*args) * opts->max_threads);
  IGRAPH_FINALLY(free, args);
  IGRAPH_CHECK_OOM(args, "Out of memory.");

#ifdef SE2PAR
  pthread_t* threads = malloc(sizeof(*threads) * opts->max_threads);
  IGRAPH_FINALLY(free, threads);
  IGRAPH_CHECK_OOM(threads, "Out of memory.");
  pthread_mutex_init(&se2_error_mutex, NULL);
  IGRAPH_FINALLY(pthread_mutex_destroy, &se2_error_mutex);
#endif
  for (igraph_integer_t tid = 0; tid < opts->max_threads; tid++) {
    args[tid].tid = tid;
    args[tid].opts = (se2_options*)opts;
    args[tid].n_partitions = n_partitions;
    args[tid].partition_store = (igraph_vector_int_list_t*)partition_store;
    args[tid].nmi_sum_accumulator = &nmi_sum_accumulator;

#ifdef SE2PAR
    pthread_create(&threads[tid], NULL, se2_thread_mrp, (void*)&args[tid]);
#else
    se2_thread_mrp((void*)&args[tid]);
#endif
  }

#ifdef SE2PAR
  for (igraph_integer_t tid = 0; tid < opts->max_threads; tid++) {
    pthread_join(threads[tid], NULL);
  }
  pthread_mutex_destroy(&se2_error_mutex);

  free(threads);
  IGRAPH_FINALLY_CLEAN(2);
#endif

  free(args);
  IGRAPH_FINALLY_CLEAN(1);

  igraph_matrix_rowsum(&nmi_sum_accumulator, &nmi_sums);

  if (opts->verbose && (subcluster == 0)) {
    mean_nmi = igraph_matrix_sum(&nmi_sum_accumulator);
    mean_nmi /= (n_partitions * (n_partitions - 1));
    SE2_PRINTF("Mean of all NMIs is %0.5f.\n", mean_nmi);
  }

  for (igraph_integer_t i = 0; i < n_partitions; i++) {
    if (VECTOR(nmi_sums)[i] > max_nmi) {
      max_nmi = VECTOR(nmi_sums)[i];
      idx = i;
    }
  }

  igraph_matrix_destroy(&nmi_sum_accumulator);
  igraph_vector_destroy(&nmi_sums);
  IGRAPH_FINALLY_CLEAN(2);

  selected_partition = igraph_vector_int_list_get_ptr(partition_store, idx);

  igraph_integer_t n_nodes = igraph_vector_int_size(selected_partition);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    VECTOR(*most_representative_partition)[i] = VECTOR(*selected_partition)[i];
  }

  return IGRAPH_SUCCESS;
}

enum bootstrap_status {
  SE2_STATUS_WAITING = 0,
  SE2_STATUS_STARTED, // Means needs to print info.
  SE2_STATUS_RUNNING, // Means running but info has been printed.
  SE2_STATUS_FINISHED
};

struct bootstrap_params {
  igraph_integer_t tid;
  igraph_integer_t* run_i;
  igraph_integer_t n_nodes;
  se2_neighs* graph;
  igraph_integer_t subcluster_iter;
  igraph_vector_int_list_t* partition_store;
  se2_options* opts;
  igraph_integer_t* status;
  igraph_integer_t* unique_labels;
  igraph_vector_int_t* memb;
#ifdef SE2PAR
  pthread_mutex_t* status_mutex;
#endif
};

static igraph_error_t print_info(struct bootstrap_params const* p)
{
  if ((p->opts->verbose) && (!p->subcluster_iter)) {
    if (!greeting_printed) {
      greeting_printed = true;
      SE2_PRINTF(
        "Completed generating initial labels.\n"
        "Produced %" IGRAPH_PRId " seed labels, "
        "while goal was %" IGRAPH_PRId ".\n\n"
        "Starting level 1 clustering",
        *p->unique_labels, p->opts->target_clusters);

      if (p->opts->max_threads > 1) {
        SE2_PUTS(
          "; independent runs might not be displayed in order - "
          "that is okay...");
      } else {
        SE2_PUTS("...");
      }
    }

    SE2_PRINTF(
      "Starting independent run #%" IGRAPH_PRId " of %" IGRAPH_PRId "\n",
      *p->run_i + 1, p->opts->independent_runs);
  }
#ifdef SE2PAR
  pthread_mutex_lock(p->status_mutex);
#endif

  *p->status = SE2_STATUS_RUNNING;

#ifdef SE2PAR
  pthread_mutex_unlock(p->status_mutex);
#endif

  return IGRAPH_SUCCESS;
}

static void* se2_thread_bootstrap(void* parameters)
{
  struct bootstrap_params const* p = (struct bootstrap_params*)parameters;

  igraph_integer_t const n_threads = p->opts->max_threads;
  igraph_integer_t const independent_runs = p->opts->independent_runs;
  for (igraph_integer_t run_i = p->tid; run_i < independent_runs;
       run_i += n_threads) {
    *p->run_i = run_i;
    igraph_rng_t rng, old_rng;
    igraph_integer_t partition_offset = run_i * p->opts->target_partitions;
    igraph_vector_int_t ic_store;

    SE2_THREAD_CHECK_RETURN(
      se2_rng_init(&rng, &old_rng, run_i + p->opts->random_seed), NULL);
    IGRAPH_FINALLY(igraph_rng_destroy, &rng);
    IGRAPH_FINALLY(igraph_rng_set_default, &old_rng);

    SE2_THREAD_CHECK_RETURN(
      igraph_vector_int_init(&ic_store, p->n_nodes), NULL);
    IGRAPH_FINALLY(igraph_vector_int_destroy, &ic_store);

    SE2_THREAD_CHECK_RETURN(
      se2_seeding(p->graph, p->opts, &ic_store, p->unique_labels), NULL);
    igraph_vector_int_list_set(
      p->partition_store, partition_offset, &ic_store);
    IGRAPH_FINALLY_CLEAN(1);

#ifdef SE2PAR
    pthread_mutex_lock(p->status_mutex);
#endif

    *p->status = SE2_STATUS_STARTED;

#ifdef SE2PAR
    pthread_mutex_unlock(p->status_mutex);
#endif

#ifndef SE2PAR
    print_info(p);
#endif

    SE2_THREAD_CHECK_RETURN(
      se2_core(p->graph, p->partition_store, partition_offset, p->opts), NULL);

    igraph_rng_set_default(&old_rng);
    igraph_rng_destroy(&rng);
    IGRAPH_FINALLY_CLEAN(2);

#ifdef SE2PAR
    struct timespec pause = {
      .tv_sec = 0,
      .tv_nsec = 5000000, // 5ms
    };
    // Wait for print.
    while ((p->opts->verbose) && (*p->status == SE2_STATUS_STARTED)) {
      nanosleep(&pause, NULL);
    }
#endif
  }

#ifdef SE2PAR
  pthread_mutex_lock(p->status_mutex);
#endif

  *p->status = SE2_STATUS_FINISHED;

#ifdef SE2PAR
  pthread_mutex_unlock(p->status_mutex);
#endif

  return NULL;
}

#ifdef SE2PAR
// Structure to allow destroying all mutexes with a single destroyer to reduce
// load on the igraph finally stack.
struct se2_pthread_mutex_array {
  pthread_mutex_t* array;
  igraph_integer_t n;
};

void se2_pthread_mutex_array_destroy(
  struct se2_pthread_mutex_array* mutex_array)
{
  for (igraph_integer_t i = 0; i < mutex_array->n; i++) {
    pthread_mutex_destroy(&(mutex_array->array[i]));
  }
}
#endif

static igraph_error_t se2_bootstrap(se2_neighs const* graph,
  igraph_integer_t const subcluster_iter, se2_options const* opts,
  igraph_vector_int_t* memb)
{
  se2_thread_errorcode = IGRAPH_SUCCESS;

  igraph_integer_t n_nodes = se2_vcount(graph);
  igraph_integer_t n_partitions =
    opts->target_partitions * opts->independent_runs;
  igraph_vector_int_list_t partition_store;

  IGRAPH_CHECK(igraph_vector_int_list_init(&partition_store, n_partitions));
  IGRAPH_FINALLY(igraph_vector_int_list_destroy, &partition_store);

  if ((opts->verbose) && (!subcluster_iter) && (opts->multicommunity > 1)) {
    SE2_PUTS("Attempting overlapping clustering.");
  }

  igraph_vector_int_t thread_run;
  igraph_vector_int_t thread_status;
  igraph_vector_int_t unique_labels;

  IGRAPH_CHECK(igraph_vector_int_init(&thread_run, opts->max_threads));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &thread_run);

  IGRAPH_CHECK(igraph_vector_int_init(&thread_status, opts->max_threads));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &thread_status);

  IGRAPH_CHECK(igraph_vector_int_init(&unique_labels, opts->max_threads));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &unique_labels);

#ifdef SE2PAR
  pthread_t* threads = malloc(sizeof(*threads) * opts->max_threads);
  IGRAPH_FINALLY(free, threads);
  IGRAPH_CHECK_OOM(threads, "Out of memory.");
  pthread_mutex_t* status_mutex =
    malloc(sizeof(*status_mutex) * opts->max_threads);
  IGRAPH_FINALLY(free, status_mutex);
  IGRAPH_CHECK_OOM(status_mutex, "Out of memory.");

  struct se2_pthread_mutex_array status_mutex_holder = {
    .array = status_mutex,
    .n = opts->max_threads,
  };
  for (igraph_integer_t i = 0; i < opts->max_threads; i++) {
    pthread_mutex_init(status_mutex + i, NULL);
  }
  IGRAPH_FINALLY(se2_pthread_mutex_array_destroy, &status_mutex_holder);

  pthread_mutex_init(&se2_error_mutex, NULL);
  IGRAPH_FINALLY(pthread_mutex_destroy, &se2_error_mutex);
#endif

  struct bootstrap_params* args = malloc(sizeof(*args) * opts->max_threads);
  IGRAPH_FINALLY(free, args);
  IGRAPH_CHECK_OOM(args, "Out of memory.");

  for (igraph_integer_t tid = 0; tid < opts->max_threads; tid++) {
    args[tid].tid = tid;
    args[tid].n_nodes = n_nodes;
    args[tid].graph = (se2_neighs*)graph;
    args[tid].subcluster_iter = subcluster_iter;
    args[tid].partition_store = &partition_store;
    args[tid].opts = (se2_options*)opts;
    args[tid].run_i = &(VECTOR(thread_run)[tid]);
    args[tid].status = &(VECTOR(thread_status)[tid]);
    args[tid].unique_labels = &(VECTOR(unique_labels)[tid]);
#ifdef SE2PAR
    args[tid].status_mutex = &status_mutex[tid];
#endif

#ifdef SE2PAR
    pthread_create(
      &threads[tid], NULL, se2_thread_bootstrap, (void*)&args[tid]);
#else
    se2_thread_bootstrap((void*)&args[tid]);
#endif
  }

#ifdef SE2PAR
  struct timespec pause = {
    .tv_sec = 0,
    .tv_nsec = 20000000, // 20ms
  };

  // Perform user interrupt check on main thread.
  while (igraph_vector_int_sum(&thread_status) !=
         (SE2_STATUS_FINISHED * opts->max_threads)) {
    nanosleep(&pause, NULL);

    for (igraph_integer_t i = 0; i < opts->max_threads; i++) {
      if (VECTOR(thread_status)[i] == SE2_STATUS_STARTED) {
        print_info(&args[i]);
      }
    }

    if (igraph_allow_interruption()) {
      pthread_mutex_lock(&se2_error_mutex);
      se2_thread_errorcode = IGRAPH_INTERRUPTED;
      pthread_mutex_unlock(&se2_error_mutex);
      break;
    }
  }

  for (igraph_integer_t tid = 0; tid < opts->max_threads; tid++) {
    pthread_join(threads[tid], NULL);
  }
#endif

  if (se2_thread_errorcode != IGRAPH_SUCCESS) {
    IGRAPH_FINALLY_FREE();
    return se2_thread_errorcode;
  };

  free(args);
  IGRAPH_FINALLY_CLEAN(1);

#ifdef SE2PAR
  se2_pthread_mutex_array_destroy(&status_mutex_holder);
  pthread_mutex_destroy(&se2_error_mutex);
  free(status_mutex);
  free(threads);
  IGRAPH_FINALLY_CLEAN(4);
#endif

  igraph_vector_int_destroy(&thread_run);
  igraph_vector_int_destroy(&thread_status);
  igraph_vector_int_destroy(&unique_labels);
  IGRAPH_FINALLY_CLEAN(3);

  if ((opts->verbose) && (!subcluster_iter)) {
    SE2_PRINTF(
      "\nGenerated %" IGRAPH_PRId " partitions at level 1.\n", n_partitions);
  }

  IGRAPH_CHECK(se2_most_representative_partition(
    &partition_store, n_partitions, memb, opts, subcluster_iter));

  igraph_vector_int_list_destroy(&partition_store);
  IGRAPH_FINALLY_CLEAN(1);

  return IGRAPH_SUCCESS;
}

static igraph_integer_t default_target_clusters(se2_neighs const* graph)
{
  igraph_integer_t const n_nodes = se2_vcount(graph);
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

static void se2_set_defaults(se2_neighs const* graph, se2_options* opts)
{
  SE2_SET_OPTION(opts, independent_runs, 10);
  SE2_SET_OPTION(opts, subcluster, 1);
  SE2_SET_OPTION(opts, multicommunity, 1);
  SE2_SET_OPTION(opts, target_partitions, 5);
  SE2_SET_OPTION(opts, target_clusters, default_target_clusters(graph));
  SE2_SET_OPTION(opts, minclust, 5);
  SE2_SET_OPTION(opts, discard_transient, 3);
  SE2_SET_OPTION(opts, random_seed, RNG_INTEGER(1, 9999));
  SE2_SET_OPTION(
    opts, max_threads, default_max_threads(opts->independent_runs));
  SE2_SET_OPTION(opts, node_confidence, false);
  SE2_SET_OPTION(opts, verbose, false);
}

static igraph_error_t se2_collect_community_members(
  igraph_vector_int_t const* memb, igraph_vector_int_t* idx,
  igraph_integer_t const comm)
{
  igraph_integer_t n_memb = 0;
  for (igraph_integer_t i = 0; i < igraph_vector_int_size(memb); i++) {
    n_memb += VECTOR(*memb)[i] == comm;
  }

  IGRAPH_CHECK(igraph_vector_int_init(idx, n_memb));
  IGRAPH_FINALLY(igraph_vector_int_destroy, idx);
  igraph_integer_t count = 0;
  for (igraph_integer_t i = 0; i < igraph_vector_int_size(memb); i++) {
    if (VECTOR(*memb)[i] == comm) {
      VECTOR(*idx)[count] = i;
      count++;
    }
  }

  IGRAPH_FINALLY_CLEAN(1);
  return IGRAPH_SUCCESS;
}

static igraph_error_t se2_subgraph_from_community(se2_neighs const* origin,
  se2_neighs* subgraph, igraph_vector_int_t const* members)
{
  igraph_integer_t const n_membs = igraph_vector_int_size(members);
  subgraph->n_nodes = n_membs;

  subgraph->neigh_list = igraph_malloc(sizeof(*subgraph->neigh_list));
  IGRAPH_CHECK_OOM(subgraph->neigh_list, "");
  IGRAPH_FINALLY(igraph_free, subgraph->neigh_list);
  IGRAPH_CHECK(igraph_vector_int_list_init(subgraph->neigh_list, n_membs));
  IGRAPH_FINALLY(igraph_vector_int_list_destroy, subgraph->neigh_list);

  subgraph->sizes = igraph_malloc(sizeof(*subgraph->sizes));
  IGRAPH_CHECK_OOM(subgraph->sizes, "");
  IGRAPH_FINALLY(igraph_free, subgraph->sizes);
  IGRAPH_CHECK(igraph_vector_int_init(subgraph->sizes, n_membs));
  IGRAPH_FINALLY(igraph_vector_int_destroy, subgraph->sizes);

  subgraph->kin = igraph_malloc(sizeof(*subgraph->kin));
  IGRAPH_CHECK_OOM(subgraph->kin, "");
  IGRAPH_FINALLY(igraph_free, subgraph->kin);
  IGRAPH_CHECK(igraph_vector_init(subgraph->kin, n_membs));
  IGRAPH_FINALLY(igraph_vector_destroy, subgraph->kin);

  if (HASWEIGHTS(*origin)) {
    subgraph->weights = igraph_malloc(sizeof(*subgraph->weights));
    IGRAPH_CHECK_OOM(subgraph->weights, "");
    IGRAPH_FINALLY(igraph_free, subgraph->weights);
    IGRAPH_CHECK(igraph_vector_list_init(subgraph->weights, n_membs));
    IGRAPH_FINALLY(igraph_vector_list_destroy, subgraph->weights);
  } else {
    subgraph->weights = NULL;
  }

  igraph_vector_int_t neighs;
  if (!ISSPARSE(*origin)) {
    IGRAPH_CHECK(igraph_vector_int_init(&neighs, se2_vcount(origin)));
    IGRAPH_FINALLY(igraph_vector_int_destroy, &neighs);

    for (igraph_integer_t i = 0; i < se2_vcount(origin); i++) {
      VECTOR(neighs)[i] = i;
    }
  }

  for (igraph_integer_t i = 0; i < n_membs; i++) {
    igraph_integer_t node_id = VECTOR(*members)[i];
    igraph_vector_int_t* new_neighs = &NEIGHBORS(*subgraph, i);
    igraph_integer_t const n_neighs = N_NEIGHBORS(*origin, node_id);
    igraph_vector_t* w =
      HASWEIGHTS(*subgraph) ? &WEIGHTS_IN(*subgraph, i) : NULL;

    IGRAPH_CHECK(igraph_vector_int_resize(new_neighs, n_neighs));
    if (HASWEIGHTS(*subgraph)) {
      IGRAPH_CHECK(igraph_vector_resize(w, n_neighs));
    }

    if (ISSPARSE(*origin)) {
      neighs = NEIGHBORS(*origin, node_id);
    }

    igraph_integer_t count = 0;
    igraph_integer_t pos;
    for (igraph_integer_t j = 0; j < n_neighs; j++) {
      if (igraph_vector_int_search(members, 0, VECTOR(neighs)[j], &pos)) {
        VECTOR(*new_neighs)[count] = pos;
        if (HASWEIGHTS(*subgraph)) {
          VECTOR(*w)[count] = WEIGHT(*origin, node_id, j);
        }
        count++;
      }
    }

    VECTOR(*subgraph->sizes)[i] = count;
    IGRAPH_CHECK(igraph_vector_int_resize(new_neighs, count));
    if (HASWEIGHTS(*subgraph)) {
      IGRAPH_CHECK(igraph_vector_resize(w, count));
    }
  }

  if (!ISSPARSE(*origin)) {
    igraph_vector_int_destroy(&neighs);
    IGRAPH_FINALLY_CLEAN(1);
  }

  for (igraph_integer_t i = 0; i < n_membs; i++) {
    for (igraph_integer_t j = 0; j < N_NEIGHBORS(*subgraph, i); j++) {
      VECTOR(*subgraph->kin)
      [NEIGHBOR(*subgraph, i, j)] +=
        HASWEIGHTS(*subgraph) ? WEIGHT(*subgraph, i, j) : 1;
    }
  }
  subgraph->total_weight = igraph_vector_sum(subgraph->kin);

  if (HASWEIGHTS(*subgraph)) {
    IGRAPH_FINALLY_CLEAN(2);
  }
  IGRAPH_FINALLY_CLEAN(6);

  return IGRAPH_SUCCESS;
}

/* For hierarchical clustering, each community from the previous level gets
clustered. Each of these clusters gets a "private scope" set of labels starting
at 0. These must be relabeled to a global scope. */
static igraph_error_t se2_relabel_hierarchical_communities(
  igraph_vector_int_t const* prev_membs, igraph_vector_int_t* level_membs)
{
  igraph_integer_t const n_comms =
    igraph_vector_int_max(prev_membs) - igraph_vector_int_min(prev_membs) + 1;

  igraph_integer_t prev_max = 0;
  igraph_integer_t curr_max = 0;
  for (igraph_integer_t i = 0; i < n_comms; i++) {
    igraph_vector_int_t member_ids;
    IGRAPH_CHECK(se2_collect_community_members(prev_membs, &member_ids, i));
    IGRAPH_FINALLY(igraph_vector_int_destroy, &member_ids);

    for (igraph_integer_t j = 0; j < igraph_vector_int_size(&member_ids);
         j++) {
      igraph_integer_t local_label =
        VECTOR(*level_membs)[VECTOR(member_ids)[j]];

      VECTOR(*level_membs)[VECTOR(member_ids)[j]] += prev_max;
      if ((local_label + prev_max) > curr_max) {
        curr_max = local_label + prev_max;
      }
    }
    prev_max = curr_max + 1;
    igraph_vector_int_destroy(&member_ids);
    IGRAPH_FINALLY_CLEAN(1);
  }

  return IGRAPH_SUCCESS;
}

/**
\brief speakeasy 2 community detection.

\param graph the graph to cluster.
\param weights optional weights if the graph is weighted, use NULL for
  unweighted.
\param opts a speakeasy options structure (see speak_easy_2.h).
\param memb the resulting membership vector.

\return Error code:
*/
igraph_error_t speak_easy_2(
  se2_neighs* graph, se2_options* opts, igraph_matrix_int_t* memb)
{
  /* In high level interfaces, the value of file-scope variables are held onto
     for the duration of the session. If SE2 is called multiple times within a
     session, need to reset globals. */
  greeting_printed = false;

  se2_set_defaults(graph, opts);

#ifndef SE2PAR
  if (opts->max_threads > 1) {
    IGRAPH_WARNING(
      "SpeakEasy 2 was not compiled with thread support. "
      "Ignoring `max_threads`.\n\n"
      "To suppress this warning do not set `max_threads`\n.");
  }
  opts->max_threads = 1;
#endif

  IGRAPH_CHECK(se2_reweigh(graph, opts->verbose));

  if (opts->verbose) {
    igraph_bool_t isweighted = false;
    if (HASWEIGHTS(*graph)) {
      for (igraph_integer_t i = 0; i < se2_vcount(graph); i++) {
        for (igraph_integer_t j = 0; j < N_NEIGHBORS(*graph, i); j++) {
          if (WEIGHT(*graph, i, j) != 1) {
            isweighted = true;
            break;
          }
        }

        if (isweighted) {
          break;
        }
      }
    }

    igraph_integer_t possible_edges = se2_vcount(graph) * se2_vcount(graph);
    igraph_real_t edge_density =
      (igraph_real_t)(se2_ecount(graph) - se2_vcount(graph)) /
      (possible_edges - se2_vcount(graph));
    SE2_PRINTF(
      "Approximate edge density is %g.\n"
      "Input type treated as %s.\n\n"
      "Calling main routine at level 1.\n",
      edge_density, isweighted ? "weighted" : "unweighted");
  }

  IGRAPH_CHECK(
    igraph_matrix_int_init(memb, opts->subcluster, se2_vcount(graph)));
  IGRAPH_FINALLY(igraph_matrix_int_destroy, memb);

  igraph_vector_int_t level_memb;
  IGRAPH_CHECK(igraph_vector_int_init(&level_memb, se2_vcount(graph)));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &level_memb);

  IGRAPH_CHECK(se2_bootstrap(graph, 0, opts, &level_memb));
  IGRAPH_CHECK(igraph_matrix_int_set_row(memb, &level_memb, 0));

  for (igraph_integer_t level = 1; level < opts->subcluster; level++) {
    if (opts->verbose) {
      SE2_PRINTF("\nSubclustering at level %" IGRAPH_PRId ".\n", level + 1);
    }

    igraph_vector_int_t prev_memb;
    IGRAPH_CHECK(
      igraph_vector_int_init(&prev_memb, igraph_matrix_int_ncol(memb)));
    IGRAPH_FINALLY(igraph_vector_int_destroy, &prev_memb);
    IGRAPH_CHECK(igraph_matrix_int_get_row(memb, &prev_memb, level - 1));

    igraph_integer_t const n_comms =
      igraph_vector_int_max(&prev_memb) - igraph_vector_int_min(&prev_memb) +
      1;
    for (igraph_integer_t comm = 0; comm < n_comms; comm++) {
      igraph_vector_int_t member_ids;
      IGRAPH_CHECK(
        se2_collect_community_members(&prev_memb, &member_ids, comm));
      IGRAPH_FINALLY(igraph_vector_int_destroy, &member_ids);
      igraph_integer_t const n_membs = igraph_vector_int_size(&member_ids);

      if (n_membs <= opts->minclust) {
        for (igraph_integer_t i = 0; i < n_membs; i++) {
          VECTOR(level_memb)[VECTOR(member_ids)[i]] = 0;
        }

        igraph_vector_int_destroy(&member_ids);
        IGRAPH_FINALLY_CLEAN(1);
        continue;
      }

      se2_neighs subgraph;
      igraph_vector_int_t subgraph_memb;

      IGRAPH_CHECK(igraph_vector_int_init(&subgraph_memb, n_membs));
      IGRAPH_FINALLY(igraph_vector_int_destroy, &subgraph_memb);
      IGRAPH_CHECK(se2_subgraph_from_community(graph, &subgraph, &member_ids));
      IGRAPH_FINALLY(se2_neighs_destroy, &subgraph);

      IGRAPH_CHECK(se2_reweigh(&subgraph, /* verbose */ false));
      IGRAPH_CHECK(se2_bootstrap(&subgraph, level, opts, &subgraph_memb));

      for (igraph_integer_t i = 0; i < igraph_vector_int_size(&subgraph_memb);
           i++) {
        VECTOR(level_memb)[VECTOR(member_ids)[i]] = VECTOR(subgraph_memb)[i];
      }

      se2_neighs_destroy(&subgraph);
      igraph_vector_int_destroy(&subgraph_memb);
      igraph_vector_int_destroy(&member_ids);
      IGRAPH_FINALLY_CLEAN(3);
    }

    IGRAPH_CHECK(
      se2_relabel_hierarchical_communities(&prev_memb, &level_memb));
    IGRAPH_CHECK(igraph_matrix_int_set_row(memb, &level_memb, level));

    igraph_vector_int_destroy(&prev_memb);
    IGRAPH_FINALLY_CLEAN(1);
  }

  igraph_vector_int_destroy(&level_memb);
  IGRAPH_FINALLY_CLEAN(1);

  if (opts->verbose) {
    SE2_PRINT("\n");
  }

  IGRAPH_FINALLY_CLEAN(1); // memb

  return IGRAPH_SUCCESS;
}
