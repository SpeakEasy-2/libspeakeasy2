#include <igraph.h>
#include "speak_easy_2.h"
#include "plot_adj.h"

int main()
{
  igraph_t graph;
  igraph_integer_t n_nodes = 40, n_types = 4;
  igraph_real_t const mu = 0.25; // probability of between community edges.
  igraph_vector_t type_dist;
  igraph_real_t type_dist_arr[] = { 0.4, 0.25, 0.2, 0.15 };
  igraph_matrix_t pref_mat;
  igraph_matrix_int_t membership;
  igraph_vector_int_t ground_truth;
  igraph_matrix_int_t gt_membership;
  igraph_matrix_int_t ordering;

  // Generate a graph with clear community structure
  igraph_vector_view(&type_dist, type_dist_arr, n_types);
  igraph_vector_int_init(&ground_truth, 0);

  igraph_matrix_init(&pref_mat, n_types, n_types);
  igraph_real_t p_in = 1 - mu, p_out = mu / (n_types - 1);
  for (igraph_integer_t i = 0; i < n_types; i++) {
    for (igraph_integer_t j = 0; j < n_types; j++) {
      MATRIX(pref_mat, i, j) = i == j ? p_in : p_out;
    }
  }

  igraph_preference_game(&graph, n_nodes, n_types, &type_dist, false,
                         &pref_mat, &ground_truth, IGRAPH_UNDIRECTED, false);

  igraph_matrix_destroy(&pref_mat);

  // Running SpeakEasy2
  se2_options opts = {
    .random_seed = 1234,
    .subcluster = 1, // No sub-clustering.
    .verbose = true,
  };

  speak_easy_2(&graph, NULL, &opts, &membership);

  // Order nodes by ground truth community structure
  igraph_matrix_int_view_from_vector(&gt_membership, &ground_truth, 1);
  se2_order_nodes(&graph, NULL, &gt_membership, &ordering);
  igraph_vector_int_destroy(&ground_truth);

  // Display results
  puts("Membership");
  print_matrix_int(&membership);

  puts("Adjacency matrix");
  igraph_vector_int_t level_membership, level_ordering;
  igraph_vector_int_init(&level_membership,
                         igraph_matrix_int_ncol(&membership));
  igraph_vector_int_init(&level_ordering, igraph_matrix_int_ncol(&ordering));

  igraph_matrix_int_get_row(&membership, &level_membership, 0);
  igraph_matrix_int_get_row(&ordering, &level_ordering, 0);
  plot_edges(&graph, &level_membership, &level_ordering);

  igraph_vector_int_destroy(&level_membership);
  igraph_vector_int_destroy(&level_ordering);
  igraph_matrix_int_destroy(&membership);
  igraph_matrix_int_destroy(&ordering);
  igraph_destroy(&graph);

  return EXIT_SUCCESS;
}
