#include "igraph_interface.h"

#include <speak_easy_2.h>

static igraph_error_t errs[] = {
  IGRAPH_SUCCESS,
  IGRAPH_SUCCESS,
  IGRAPH_INTERRUPTED,
};

static igraph_error_t check_user_interrupt(void* data)
{
  static igraph_integer_t err_idx = 0;
  return errs[err_idx++];
}

int main(void)
{
  igraph_set_error_handler(igraph_error_handler_printignore);
  igraph_set_status_handler(igraph_status_handler_stderr);

  igraph_t graph;
  igraph_integer_t n_nodes = 40, n_types = 4;
  se2_neighs neigh_list;
  igraph_real_t const mu = 0.25; // probability of between community edges.
  igraph_vector_t type_dist;
  igraph_real_t type_dist_arr[] = { 0.4, 0.25, 0.2, 0.15 };
  igraph_matrix_t pref_mat;
  igraph_matrix_int_t membership;
  igraph_error_t rs;

  // Generate a graph with clear community structure
  igraph_vector_view(&type_dist, type_dist_arr, n_types);

  igraph_matrix_init(&pref_mat, n_types, n_types);
  igraph_real_t p_in = 1 - mu, p_out = mu / (n_types - 1);
  for (igraph_integer_t i = 0; i < n_types; i++) {
    for (igraph_integer_t j = 0; j < n_types; j++) {
      MATRIX(pref_mat, i, j) = i == j ? p_in : p_out;
    }
  }

  igraph_preference_game(&graph, n_nodes, n_types, &type_dist, false,
    &pref_mat, NULL, IGRAPH_UNDIRECTED, false);
  igraph_matrix_destroy(&pref_mat);

  // Keep after running preference game otherwise game will be interrupted.
  igraph_set_interruption_handler(check_user_interrupt);

  se2_igraph_to_neighbor_list(&graph, NULL, &neigh_list);
  igraph_destroy(&graph);

  // Running SpeakEasy2
  se2_options opts = {
    .random_seed = 1234,
    .subcluster = 1, // No sub-clustering.
    .verbose = true,
  };

  rs = speak_easy_2(&neigh_list, &opts, &membership);
  igraph_matrix_int_destroy(&membership);
  se2_neighs_destroy(&neigh_list);

  return rs == IGRAPH_INTERRUPTED;
}
