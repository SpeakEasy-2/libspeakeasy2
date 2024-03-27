#include "igraph.h"
#include "speak_easy_2.h"

int main()
{
  igraph_t graph;
  igraph_famous(&graph, "Zachary");
  igraph_matrix_int_t membership;
  se2_options opts = {
    .random_seed = 1234,
    .subcluster = 3,
    .minclust = 2,
    .verbose = true,
  };

  speak_easy_2(&graph, NULL, &opts, &membership);

  igraph_destroy(&graph);

  for (igraph_integer_t i = 0; i < igraph_matrix_int_nrow(&membership); i++) {
    printf("[");
    for (igraph_integer_t j = 0; j < igraph_matrix_int_ncol(&membership); j++) {
      printf(" %"IGRAPH_PRId, MATRIX(membership, i, j));
    }
    printf(" ]\n");
  }
  printf("\n");

  igraph_matrix_int_destroy(&membership);

  return EXIT_SUCCESS;
}
