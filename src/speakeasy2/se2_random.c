#include "se2_random.h"

/* Initializes default igraph random number generator to use twister method */
igraph_rng_t* se2_rng_init(igraph_rng_t* rng, const int seed)
{
  igraph_rng_t* old_rng = igraph_rng_default();

  igraph_rng_init(rng, &igraph_rngtype_mt19937);
  igraph_rng_set_default(rng);
  igraph_rng_seed(igraph_rng_default(), seed);

  return old_rng;
}

void se2_rng_restore(igraph_rng_t* current_rng, igraph_rng_t* previous_rng)
{
  igraph_rng_set_default(previous_rng);
  igraph_rng_destroy(current_rng);
}

/* Shuffle the first m elements of the n element vector arr */
void se2_randperm(igraph_vector_int_t* arr, igraph_integer_t const n,
                  igraph_integer_t const m)
{
  igraph_integer_t swap = 0;
  igraph_integer_t idx = 0;
  for (igraph_integer_t i = 0; i < m; i++) {
    idx = RNG_INTEGER(0, n - 1);
    swap = VECTOR(* arr)[i];
    VECTOR(* arr)[i] = VECTOR(* arr)[idx];
    VECTOR(* arr)[idx] = swap;
  }

  return;
}
