#include "igraph_community.h"
#include "igraph_constants.h"
#include "igraph_constructors.h"
#include "igraph_interface.h"
#include "plot_adj.h"

#include <signal.h>
#include <speak_easy_2.h>

#define ABS(x) ((x) < 0) ? -(x) : (x)

igraph_error_t errcode = IGRAPH_SUCCESS;

static void signal_handler(int sig)
{
  if (sig == SIGINT) {
    errcode = IGRAPH_INTERRUPTED;
  }
}

static igraph_bool_t check_user_interrupt(void)
{
  return errcode == IGRAPH_INTERRUPTED;
}

int main(void)
{
  signal(SIGINT, signal_handler);
  igraph_set_error_handler(igraph_error_handler_printignore);
  igraph_set_interruption_handler(check_user_interrupt);
  igraph_set_status_handler(igraph_status_handler_stderr);

  igraph_real_t const mu = 0.2; // probability of between community edges.
  igraph_real_t const type_dist[] = { 4, 4, 6, 10 };
  igraph_integer_t const n_types = sizeof(type_dist) / sizeof(*type_dist);

  igraph_t graph;
  se2_neighs neigh_list;
  igraph_vector_t weights;
  igraph_matrix_int_t membership;
  igraph_vector_int_t ground_truth;
  igraph_matrix_int_t gt_membership;
  igraph_matrix_int_t ordering;

  igraph_integer_t n_nodes = 0;
  for (igraph_integer_t i = 0; i < n_types; i++) {
    n_nodes += type_dist[i];
  }

  igraph_vector_int_t edges;
  igraph_vector_int_init(&edges, n_nodes * n_nodes * 2);
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    for (igraph_integer_t j = 0; j < n_nodes; j++) {
      igraph_integer_t const idx = 2 * ((i * n_nodes) + j);
      VECTOR(edges)[idx] = i;
      VECTOR(edges)[idx + 1] = j;
    }
  }

  igraph_empty(&graph, n_nodes, true);
  igraph_add_edges(&graph, &edges, NULL);
  igraph_vector_int_destroy(&edges);

  // Generate a graph with clear community structure
  igraph_vector_int_init(&ground_truth, n_nodes);
  igraph_integer_t label = 0;
  igraph_integer_t label_max = type_dist[label];
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    while (i > label_max) {
      label++;
      label_max += type_dist[label];
    }
    VECTOR(ground_truth)[i] = label;
  }

  igraph_real_t const p_in = 1 - mu;
  igraph_real_t const p_out = mu;
  igraph_vector_init(&weights, n_nodes * n_nodes);
  for (igraph_integer_t i = 0; i < igraph_ecount(&graph); i++) {
    igraph_integer_t const from = IGRAPH_FROM(&graph, i);
    igraph_integer_t const to = IGRAPH_TO(&graph, i);
    igraph_real_t mean =
      VECTOR(ground_truth)[from] == VECTOR(ground_truth)[to] ? p_in : p_out;
    VECTOR(weights)
    [i] = igraph_rng_get_normal(igraph_rng_default(), mean, 0.2);
    VECTOR(weights)[i] = from == to ? 0 : ABS(VECTOR(weights)[i]);
  }

  se2_igraph_to_neighbor_list(&graph, &weights, &neigh_list);

  // Running SpeakEasy2
  se2_options opts = {
    .random_seed = 1234,
    .subcluster = 3,
    .verbose = true,
  };

  igraph_error_t rs;
  if ((rs = speak_easy_2(&neigh_list, &opts, &membership)) != IGRAPH_SUCCESS) {
    igraph_destroy(&graph);
    se2_neighs_destroy(&neigh_list);
    igraph_vector_int_destroy(&ground_truth);
    igraph_matrix_int_destroy(&membership);
    return rs;
  };

  // Order nodes by ground truth community structure
  gt_membership = igraph_matrix_int_view_from_vector(&ground_truth, 1);
  se2_order_nodes(&neigh_list, &gt_membership, &ordering);

  // Display results
  puts("Membership");
  print_matrix_int(&membership);

  igraph_real_t modularity = 0;
  igraph_vector_int_t membership_1;
  igraph_vector_int_init(&membership_1, igraph_matrix_int_ncol(&membership));
  igraph_matrix_int_get_row(&membership, &membership_1, 0);
  igraph_modularity(
    &graph, &membership_1, &weights, 1, IGRAPH_UNDIRECTED, &modularity);
  printf("Modularity: %0.2f\n", modularity);
  igraph_vector_int_destroy(&membership_1);

  igraph_matrix_int_destroy(&ordering);
  igraph_vector_int_destroy(&ground_truth);
  igraph_matrix_int_destroy(&membership);
  se2_neighs_destroy(&neigh_list);
  igraph_vector_destroy(&weights);
  igraph_destroy(&graph);

  return IGRAPH_SUCCESS;
}
