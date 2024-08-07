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

#ifndef SE2_ERROR_HANDLING_H
#define SE2_ERROR_HANDLING_H

#include <igraph.h>

#ifdef SE2PAR
#  include <pthread.h>
#endif

/* Threaded error handling based on igraph's allocation stack. */

extern igraph_error_t se2_thread_errorcode;

#ifdef SE2PAR
extern pthread_mutex_t se2_error_mutex;
#endif

/* Check if any thread has triggered an error. */
#define SE2_THREAD_STATUS()                      \
  do {                                           \
    if (se2_thread_errorcode != IGRAPH_SUCCESS) {\
      IGRAPH_FINALLY_FREE();                     \
      return se2_thread_errorcode;               \
    }                                            \
  } while (0)

#ifdef SE2PAR
/* Check if a thread has triggered an error, if so cleanup otherwise check
   the current expression. */
#  define SE2_THREAD_CHECK(expr)                    \
  do {                                              \
    SE2_THREAD_STATUS();                            \
    igraph_error_t se2_rs = (expr);                 \
    if (IGRAPH_UNLIKELY(se2_rs != IGRAPH_SUCCESS)) {\
      pthread_mutex_lock(&se2_error_mutex);         \
      se2_thread_errorcode = se2_rs;                \
      pthread_mutex_unlock(&se2_error_mutex);       \
      IGRAPH_FINALLY_FREE();                        \
      return se2_rs;                                \
    }                                               \
  } while (0)
/* Sets the global errorcode and returns the provided return value. Useful in
   functions that do not return an errorcode. Still returns early and calling
   function should check the errorcode status immediately upon return with
   `SE2_THREAD_STATUS`. */
#  define SE2_THREAD_CHECK_RETURN(expr, ret)         \
  do {                                               \
    if (se2_thread_errorcode != IGRAPH_SUCCESS) {    \
      IGRAPH_FINALLY_FREE();                         \
      return (ret);                                  \
    }                                                \
    igraph_error_t se2_rs = (expr);                  \
    if (IGRAPH_UNLIKELY(se2_rs != IGRAPH_SUCCESS)) { \
      pthread_mutex_lock(&se2_error_mutex);          \
      se2_thread_errorcode = se2_rs;                 \
      pthread_mutex_unlock(&se2_error_mutex);        \
      IGRAPH_FINALLY_FREE();                         \
      return (ret);                                  \
    }                                                \
  } while (0)

#  define SE2_THREAD_CHECK_OOM(ptr)           \
  do {                                        \
    SE2_THREAD_STATUS();                      \
    if ((ptr) == NULL) {                      \
      pthread_mutex_lock(&se2_error_mutex);   \
      se2_thread_errorcode = IGRAPH_ENOMEM;   \
      pthread_mutex_unlock(&se2_error_mutex); \
      IGRAPH_FINALLY_FREE();                  \
      return IGRAPH_ENOMEM;                   \
    }                                         \
  } while (0)

#else
#  define SE2_THREAD_CHECK IGRAPH_CHECK
#  define SE2_THREAD_CHECK_RETURN(expr, ret)         \
  do {                                               \
    igraph_error_t se2_rs = (expr);                  \
    if (IGRAPH_UNLIKELY(se2_rs != IGRAPH_SUCCESS)) { \
      se2_thread_errorcode = se2_rs;                 \
      IGRAPH_ERROR_NO_RETURN("", se2_rs);            \
      return (ret);                                  \
    }                                                \
  } while (0)
#  define SE2_THREAD_CHECK_OOM(ptr) IGRAPH_CHECK_OOM(ptr, "")
#endif


#endif
