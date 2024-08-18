#ifndef SPEAK_EASY_H
#define SPEAK_EASY_H

#include <igraph.h>
#include "../src/speakeasy2/se2_interface.h"

typedef struct {
  igraph_integer_t independent_runs;  // Number of independent runs to perform.
  igraph_integer_t subcluster;        // Depth of clustering.
  igraph_integer_t
  multicommunity;    // Max number of communities a node can be a member of.
  igraph_integer_t
  target_partitions; // Number of partitions to find per independent run.
  igraph_integer_t target_clusters;   // Expected number of clusters to find.
  igraph_integer_t minclust;          // Minimum cluster size to subclustering.
  igraph_integer_t
  discard_transient; // How many initial partitions to discard before recording.
  igraph_integer_t random_seed;       // Seed for reproducing results.
  igraph_integer_t max_threads;       // Number of threads to use.
  bool node_confidence;
  bool verbose; // Print information to stdout
} se2_options;

typedef struct {
  igraph_vector_int_list_t* neigh_list;
  igraph_vector_list_t* weights;
  igraph_vector_int_t* sizes;
  igraph_integer_t n_nodes;
  igraph_vector_t* kin;
  igraph_real_t total_weight;
} se2_neighs;

igraph_error_t se2_igraph_to_neighbor_list(igraph_t const* graph,
    igraph_vector_t const* weights, se2_neighs* neigh_list);
void se2_neighs_destroy(se2_neighs* graph);

igraph_error_t speak_easy_2(se2_neighs* graph, se2_options* opts,
                            igraph_matrix_int_t* res);
igraph_error_t se2_order_nodes(se2_neighs const* graph,
                               igraph_matrix_int_t const* memb,
                               igraph_matrix_int_t* ordering);
igraph_error_t se2_knn_graph(igraph_matrix_t* mat, igraph_integer_t const k,
                             igraph_t* res, igraph_vector_t* weights);
#endif
