#ifndef SE2_RANDOM_H
#define SE2_RANDOM_H

#include <igraph_random.h>

igraph_rng_t* se2_rng_init(igraph_rng_t* rng, const int seed);
void se2_rng_restore(igraph_rng_t* current_rng, igraph_rng_t* previous_rng);
void se2_randperm(igraph_vector_int_t* arr, igraph_integer_t const n,
                  igraph_integer_t const m);

#endif
