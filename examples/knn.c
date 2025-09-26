#include "plot_adj.h"

#include <igraph_interface.h>
#include <igraph_random.h>
#include <speak_easy_2.h>

int main(int argc, char* argv[])
{
  igraph_integer_t const m = 100, n = 40, k = 8;
  igraph_integer_t const n_comms = 4;
  igraph_real_t const comm_mean_shift = 0.75;
  igraph_matrix_t mat;
  igraph_t g;
  igraph_vector_t weights;
  igraph_vector_int_t ground_truth;
  igraph_matrix_int_t gt_membership, membership, ordering;

  igraph_vector_int_init(&ground_truth, n);
  for (igraph_integer_t i = 0; i < n; i++) {
    VECTOR(ground_truth)
    [i] = igraph_rng_get_integer(igraph_rng_default(), 0, n_comms - 1);
  }

  igraph_matrix_init(&mat, m, n);
  for (igraph_integer_t i = 0; i < m; i++) {
    for (igraph_integer_t j = 0; j < n; j++) {
      MATRIX(mat, i, j) = igraph_rng_get_normal(igraph_rng_default(), 0, 1);
      MATRIX(mat, i, j) += comm_mean_shift * VECTOR(ground_truth)[j];
    }
  }

  se2_knn_graph(&mat, k, &g, &weights);
  igraph_matrix_destroy(&mat);

  se2_neighs neigh_list;
  se2_igraph_to_neighbor_list(&g, &weights, &neigh_list);
  igraph_destroy(&g);
  igraph_vector_destroy(&weights);

  se2_options opts = {
    .random_seed = 1234,
    .subcluster = 1, // No sub-clustering.
  };

  speak_easy_2(&neigh_list, &opts, &membership);
  gt_membership = igraph_matrix_int_view_from_vector(&ground_truth, 1);
  se2_order_nodes(&neigh_list, &gt_membership, &ordering);
  igraph_vector_int_destroy(&ground_truth);

  puts("Membership");
  print_matrix_int(&membership);

  puts("Adjacency matrix");
  igraph_vector_int_t level_membership, level_ordering;
  igraph_vector_int_init(&level_membership, 0);
  igraph_vector_int_init(&level_ordering, 0);

  igraph_matrix_int_get_row(&membership, &level_membership, 0);
  igraph_matrix_int_get_row(&ordering, &level_ordering, 0);
  plot_edges(&g, &level_membership, &level_ordering);

  igraph_vector_int_destroy(&level_membership);
  igraph_vector_int_destroy(&level_ordering);
  igraph_matrix_int_destroy(&membership);
  igraph_matrix_int_destroy(&ordering);
  se2_neighs_destroy(&neigh_list);

  return IGRAPH_SUCCESS;
}
