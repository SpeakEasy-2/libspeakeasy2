#ifndef PLOT_ADJ_H
#define PLOT_ADJ_H

#include "speak_easy_2.h"

#define COLOR_GREEN "\033[1;32m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_END "\033[0m"

static inline void plot_edges(igraph_t const* graph,
                              igraph_vector_int_t const* memb,
                              igraph_vector_int_t const* order)
{
  igraph_integer_t const n_nodes = igraph_vector_int_size(memb);

  igraph_integer_t eid;
  for (igraph_integer_t i = 0; i < n_nodes; i++) {
    printf("|");
    for (igraph_integer_t j = 0; j < n_nodes; j++) {
      igraph_get_eid(graph, &eid, VECTOR(* order)[i], VECTOR(* order)[j],
                     igraph_is_directed(graph), false);
      printf(" ");
      if (i == j) {
        printf("-");
      } else if (eid == -1) {
        printf(".");
      } else if (VECTOR(* memb)[VECTOR(* order)[i]] ==
                 VECTOR(* memb)[VECTOR(* order)[j]]) {
        printf(COLOR_GREEN "%c" COLOR_END,
               'A' + (int)VECTOR(* memb)[VECTOR(* order)[i]]);
      } else {
        printf(COLOR_BLUE "*" COLOR_END);
      }
    }
    printf(" |\n");
  }
}

static inline void print_matrix_int(igraph_matrix_int_t const* mat)
{
  for (igraph_integer_t i = 0; i < igraph_matrix_int_nrow(mat); i++) {
    printf("[");
    for (igraph_integer_t j = 0; j < igraph_matrix_int_ncol(mat); j++) {
      printf(" %"IGRAPH_PRId, MATRIX(* mat, i, j));
    }
    printf(" ]\n");
  }
  printf("\n");
}

static inline void print_vector_int(igraph_vector_int_t const* vec)
{
  printf("[");
  for (igraph_integer_t i = 0; i < igraph_vector_int_size(vec); i++) {
    printf(" %"IGRAPH_PRId, VECTOR(* vec)[i]);
  }
  printf(" ]\n\n");
}

#endif
