#include "plot_adj.h"

#include <speak_easy_2.h>

#define ABS(x) ((x) < 0) ? -(x) : (x)

int main(void)
{
  igraph_t graph;
  igraph_vector_t weights;
  se2_neighs neigh_list;
  igraph_integer_t n_nodes = 400, n_types = 4;
  igraph_real_t const mu = 0.5; // probability of between community edges.
  igraph_vector_t type_dist;
  igraph_real_t type_dist_arr[] = { 0.4, 0.25, 0.2, 0.15 };
  igraph_matrix_t pref_mat;
  igraph_matrix_int_t membership;
  igraph_vector_int_t ground_truth;
  igraph_matrix_int_t gt_membership;
  igraph_matrix_int_t ordering;

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

  igraph_preference_game(&graph, n_nodes, n_types, &type_dist, false,
    &pref_mat, &ground_truth, IGRAPH_UNDIRECTED, false);
  igraph_matrix_destroy(&pref_mat);

  igraph_vector_init(&weights, igraph_ecount(&graph));
  for (igraph_integer_t i = 0; i < igraph_ecount(&graph); i++) {
    igraph_real_t mean =
      VECTOR(ground_truth)[IGRAPH_FROM(&graph, i)] ==
          VECTOR(ground_truth)[IGRAPH_TO(&graph, i)] ?
        2 :
        1;
    VECTOR(weights)
    [i] = igraph_rng_get_normal(igraph_rng_default(), mean, 0.5);
    VECTOR(weights)[i] = ABS(VECTOR(weights)[i]);
  }

  se2_igraph_to_neighbor_list(&graph, &weights, &neigh_list);

  // Running SpeakEasy2
  se2_options opts = {
    .random_seed = 1234,
    .subcluster = 3,
    .verbose = true,
  };

  speak_easy_2(&neigh_list, &opts, &membership);

  gt_membership = igraph_matrix_int_view_from_vector(&ground_truth, 1);
  se2_order_nodes(&neigh_list, &gt_membership, &ordering);
  igraph_vector_int_destroy(&ground_truth);

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
  igraph_matrix_int_destroy(&membership);
  se2_neighs_destroy(&neigh_list);
  igraph_vector_destroy(&weights);
  igraph_destroy(&graph);

  return IGRAPH_SUCCESS;
}
