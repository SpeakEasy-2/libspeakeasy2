/* Copyright 2024 David R. Connell <david32@dcon.addy.io>.
 *
 * This file is part of SpeakEasy 2.
 *
 * SpeakEasy 2 is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * SpeakEasy 2 is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with SpeakEasy 2. If not, see <https://www.gnu.org/licenses/>.
 */

#include <speak_easy_2.h>

static void se2_insert_sim(igraph_real_t const s,
  igraph_vector_t* similarities, igraph_integer_t const col,
  igraph_integer_t const other_col, igraph_integer_t* edges,
  igraph_integer_t const k)
{
  /* Edges is a vector of edge pairs for the given column. The first edge of
     each pair is col (constant) the second is other_col. As such only the
     second member of each pair ever needs to be modified. */
  if (k == 1) {
    VECTOR(*similarities)[0] = s;
    edges[1] = other_col;
  }

  igraph_integer_t bounds[2] = { 0, k };
  igraph_integer_t pos = (k - 1) / 2;
  while (!((pos == (k - 1)) || ((s >= VECTOR(*similarities)[pos]) &&
                                 (s < VECTOR(*similarities)[pos + 1])))) {
    if (s < VECTOR(*similarities)[pos]) {
      bounds[1] = pos;
    } else {
      bounds[0] = pos;
    }
    pos = (bounds[1] + bounds[0]) / 2;
  }

  for (int i = 0; i < pos; i++) {
    VECTOR(*similarities)[i] = VECTOR(*similarities)[i + 1];
    edges[(2 * i) + 1] = edges[(2 * (i + 1)) + 1];
  }
  VECTOR(*similarities)[pos] = s;
  edges[(2 * pos) + 1] = other_col;
}

static igraph_real_t se2_euclidean_dist(igraph_integer_t const i,
  igraph_integer_t const j, igraph_matrix_t* const mat)
{
  igraph_integer_t const n_rows = igraph_matrix_nrow(mat);
  igraph_real_t* col_i = igraph_matrix_get_ptr(mat, 0, i);
  igraph_real_t* col_j = igraph_matrix_get_ptr(mat, 0, j);
  igraph_real_t out = 0;
  for (igraph_integer_t k = 0; k < n_rows; k++) {
    double el = col_i[k] - col_j[k];
    out += el * el;
  }

  return sqrt(out);
}

static igraph_error_t se2_closest_k(igraph_integer_t const col,
  igraph_integer_t const k, igraph_matrix_t* const mat,
  igraph_vector_int_t* edges, igraph_vector_t* weights)
{
  igraph_vector_t similarities;
  igraph_integer_t n_cols = igraph_matrix_ncol(mat);

  IGRAPH_CHECK(igraph_vector_init(&similarities, k));
  IGRAPH_FINALLY(igraph_vector_destroy, &similarities);

  for (igraph_integer_t i = 0; i < n_cols; i++) {
    if (i == col) {
      continue;
    }

    igraph_real_t s = 1 / se2_euclidean_dist(col, i, mat);
    if (s > VECTOR(similarities)[0]) {
      igraph_integer_t* col_edges = VECTOR(*edges) + (2 * col * k);
      se2_insert_sim(s, &similarities, col, i, col_edges, k);
    }
  }

  if (weights) {
    for (igraph_integer_t i = 0; i < k; i++) {
      VECTOR(*weights)[(col * k) + i] = VECTOR(similarities)[i];
    }
  }

  igraph_vector_destroy(&similarities);
  IGRAPH_FINALLY_CLEAN(1);

  return IGRAPH_SUCCESS;
}

static void se2_knn_fill_edges(igraph_vector_int_t* edges,
  igraph_integer_t const k, igraph_integer_t const n_cols)
{
  for (igraph_integer_t i = 0; i < n_cols; i++) {
    for (igraph_integer_t j = 0; j < k; j++) {
      VECTOR(*edges)[2 * (i * k + j)] = i;
    }
  }
}

/**
\brief Create a directed graph with edges between the k nearest columns of
  a matrix. Compares columns using the inverse of euclidean distance.

\param mat the matrix containing the columns to compare.
\param k number of edges per column to make (must be >= 1 and < ncols - 1).
\param res the resulting graph (uninitialized).
\param weights, if not NULL the similarity (inverse euclidean distance) will be
  stored here for each edge.
\return Error code:
         \c IGRAPH_EINVAL: Invalid value for k.
 */
igraph_error_t se2_knn_graph(igraph_matrix_t* const mat,
  igraph_integer_t const k, igraph_t* res, igraph_vector_t* weights)
{
  igraph_integer_t const n_cols = igraph_matrix_ncol(mat);
  igraph_integer_t const n_edges = k * n_cols;
  igraph_vector_int_t edges;

  IGRAPH_CHECK(igraph_empty(res, n_cols, IGRAPH_DIRECTED));
  IGRAPH_FINALLY(igraph_destroy, res);

  if (weights) {
    IGRAPH_CHECK(igraph_vector_init(weights, n_edges));
    IGRAPH_FINALLY(igraph_vector_destroy, weights);
  }

  IGRAPH_CHECK(igraph_vector_int_init(&edges, 2 * n_edges));
  IGRAPH_FINALLY(igraph_vector_int_destroy, &edges);
  se2_knn_fill_edges(&edges, k, n_cols);

  if (k < 1) {
    IGRAPH_ERRORF("The k must be at least 1 but got %" IGRAPH_PRId ".\n",
      IGRAPH_EINVAL, k);
  }

  if (k >= (n_cols - 1)) {
    IGRAPH_ERRORF(
      "The k must be less than the number of columns, " "got k = %" IGRAPH_PRId
      " with only %" IGRAPH_PRId " columns.\n",
      IGRAPH_EINVAL, k, n_cols);
  }

  for (igraph_integer_t i = 0; i < n_cols; i++) {
    IGRAPH_CHECK(se2_closest_k(i, k, mat, &edges, weights));
  }

  IGRAPH_CHECK(igraph_add_edges(res, &edges, NULL));
  igraph_vector_int_destroy(&edges);
  IGRAPH_FINALLY_CLEAN(1);

  if (weights) {
    IGRAPH_FINALLY_CLEAN(1);
  }

  IGRAPH_FINALLY_CLEAN(1);

  return IGRAPH_SUCCESS;
}
