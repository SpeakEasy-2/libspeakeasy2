# SpeakEasy 2: Champagne community detection
[Genome Biology article.](https://genomebiology.biomedcentral.com/articles/10.1186/s13059-023-03062-0)

A port of the SpeakEasy 2 (SE2) community detection algorithm rewritten in C with the help of the [igraph C library](https://igraph.org/).
The original MATLAB code can be found at  [SE2](https://github.com/cogdishion/SE2).

This C library is used to provide packages for high-level languages: [MATLAB toolbox](https://SpeakEasy-2/speakeasy2-toolbox), [python package](https://SpeakEasy-2/python-speakeasy2), [R package](https://SpeakEasy-2/r-speakeasy2).

At the moment, implementation performs a subset of the matlab version. Specifically, this cannot perform subclustering, provide confidence for node membership, or order the nodes for plotting.

## Building from source
This package is built using CMake and uses git submodules to handling some of the dependencies. External dependencies needed for building the igraph library are `bison`, `flex`, and `libxml2`.

To download the source, including the git submodules use:

```bash
git submodule init && git submodule update --recursive
```

To build run:
```bash
cmake -B build .
cmake --build build
```
This will build the igraph library (as a static library) and the speakeasy2 library (as a CMake object library).

## Use

The below example can be found in [examples/zachary.c](file:///./examples/zachary.c) and is built by the above `cmake` command. The executable will be under `./build/examples/zachary_example` once the project has been built. See the related `CMakeLists.txt` under the `examples` directory for a starting point for using this library in other C projects.

```C
#include "igraph.h"
#include "speak_easy_2.h"

int main()
{
  igraph_t graph;
  igraph_famous(&graph, "Zachary");
  igraph_vector_int_t membership;
  se2_options opts = {};

  igraph_vector_int_init(&membership, 0);
  speak_easy_2(&graph, NULL, &opts, &membership);

  igraph_destroy(&graph);

  printf("[");
  for (igraph_integer_t i = 0; i < igraph_vector_int_size(&membership); i++) {
    printf(" %"IGRAPH_PRId, VECTOR(membership)[i]);
  }
  printf(" ]\n");

  igraph_vector_int_destroy(&membership);

  return EXIT_SUCCESS;
}
```
## Options

| Option | type | default | effect |
|--------|------|---------|--------|
| independent_runs | integer | 10 | number of independent runs to perform. Each run gets its own set of initial conditions. |
| target_partitions | integer | 5 | Number of partitions to find per independent run. |
| discard_transient | integer | 3 | Ignore this many partitions before tracking. |
| target_clusters | integer | dependent on size of graph | Expected number of clusters to find. Used for creating the initial conditions. The final partition is not constrained to having this many clusters. |
| random_seed | integer | randomly generated | a random seed for reproducibility. |
| max_threads | integer | Value returned by `omp_get_num_threads` | number of parallel threads to use. (Use 1 to prevent parallel processing.)|
| verbose | boolean | false | Whether to print extra information about the running process. |

Options can be used by setting the values of the options structure:

```C
	...
	se2_options opts = {
		random_seed = 1234,
		verbose = true,
		independent_runs = 5
	};

	speak_easy_2(&graph, NULL, &opts, &membership);
	...
```
