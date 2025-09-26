#include "igraph_community.h"
#include "igraph_constants.h"
#include "plot_adj.h"

#include <signal.h>
#include <speak_easy_2.h>

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

  igraph_t graph;
  se2_neighs neigh_list;
  igraph_real_t const mu = 0.2; // probability of between community edges.
  igraph_vector_t type_dist;
  igraph_real_t type_dist_arr[] = { 4, 4, 6, 10 };
  igraph_integer_t n_types = sizeof(type_dist_arr) / sizeof(*type_dist_arr);
  igraph_matrix_t pref_mat;
  igraph_matrix_int_t membership;
  igraph_vector_int_t ground_truth;
  igraph_matrix_int_t gt_membership;
  igraph_matrix_int_t ordering;

  igraph_integer_t n_nodes = 0;
  for (igraph_integer_t i = 0; i < n_types; i++) {
    n_nodes += type_dist_arr[i];
  }

  // Generate a graph with clear community structure
  type_dist = igraph_vector_view(type_dist_arr, n_types);
  igraph_vector_int_init(&ground_truth, 0);

  igraph_matrix_init(&pref_mat, n_types, n_types);
  igraph_real_t p_in = 1 - mu, p_out = mu / (n_types - 1);
  for (igraph_integer_t i = 0; i < n_types; i++) {
    for (igraph_integer_t j = 0; j < n_types; j++) {
      MATRIX(pref_mat, i, j) = i == j ? p_in : p_out;
    }
  }

  igraph_preference_game(&graph, n_nodes, n_types, &type_dist, true, &pref_mat,
    &ground_truth, IGRAPH_UNDIRECTED, false);

  igraph_matrix_destroy(&pref_mat);

  se2_igraph_to_neighbor_list(&graph, NULL, &neigh_list);

  // Running SpeakEasy2
  se2_options opts = {
    .random_seed = 1234,
    .subcluster = 1, // No sub-clustering.
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

  puts("NMI against Ground truth");
  igraph_vector_int_t membership_vec;
  igraph_real_t res = 0;

  igraph_vector_int_init(&membership_vec, 0);
  igraph_matrix_int_get_row(&membership, &membership_vec, 0);
  igraph_compare_communities(
    &membership_vec, &ground_truth, &res, IGRAPH_COMMCMP_NMI);
  printf("%g\n\n", res);
  igraph_vector_int_destroy(&ground_truth);
  igraph_vector_int_destroy(&membership_vec);

  puts("Adjacency matrix");
  igraph_vector_int_t level_membership, level_ordering;
  igraph_vector_int_init(
    &level_membership, igraph_matrix_int_ncol(&membership));
  igraph_vector_int_init(&level_ordering, igraph_matrix_int_ncol(&ordering));

  igraph_matrix_int_get_row(&membership, &level_membership, 0);
  igraph_matrix_int_get_row(&ordering, &level_ordering, 0);
  plot_edges(&graph, &level_membership, &level_ordering);

  igraph_vector_int_destroy(&level_membership);
  igraph_vector_int_destroy(&level_ordering);
  igraph_matrix_int_destroy(&ordering);
  igraph_matrix_int_destroy(&membership);
  se2_neighs_destroy(&neigh_list);
  igraph_destroy(&graph);

  return IGRAPH_SUCCESS;
}
