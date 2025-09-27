# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unrealesed]

### Changed

- Update to igraph 1.0.0
- Remove `local_labels_heard` cache in partition to reduce memory usage when `n_labels` is close to `n_nodes` (also improves speed).

## [v0.1.11] 2025-09-04

### Fixed

- Iterate over all communities when scoring labels. Since labels are not always contiguous, this often requires the label score for loop to go beyond the first `n_labels` indices.
- Number of communities calculation was max - min instead of max - min + 1 so the largest community was not getting sub-clustered properly.

## [v0.1.10] 2025-08-27

### Added

- Bring back `se2_version.h`. Not sure why this got deleted but it's needed for downstream packages.

### Changed

- Allow k == 0 and k == (vcount - 1) for KNN graph.
- Upgrade igraph to 0.10.16.
- Replace `qsort` with `sort` since `qsort` was deprecated by igraph.
- Optimize label scoring for significant speed improvement.

### Fixed

- Reset old rng on exit.

## [v0.1.9] 2024-10-07

### Fixed

- Ensure weights are doubled if original graph was triangular since neighbor list collects edges in both direction if undirected.
- Calculation of skew.
- Calculation of mean link weight.
- Nurture stage changed to only work on subgraph containing only worst fitting nodes. Significantly improves performance on some graphs over working on entire graph.
- Previously one less than the requested number of partitions to be discarded was being discarded.

## [v0.1.8] 2024-09-23

### Changed

- (breaking) use igraph's interruption handlers instead of reinventing them.
- (breaking) use igraph's status handlers instead of creating se2 print / puts functions.

### Fixed

- Inconsistent definition of `SE2PAR` macro which ended up set differently in different files.
- Mistake in managing the finally stack where `weights` was always getting cleaned instead of only cleaning if it existed.

## [v0.1.7] 2024-08-18

### Added

- Memory checks to workflows.

### Changed

- Store labels heard instead of recalculating every step to improve speed at cost of memory.
- Manually calculate neighbors by iterating over edges to remove need for slow searches.

## [v0.1.6] 2024-08-08

### Added

- Error handling to allow graceful exit from threads if error occurs in any thread. This is primarily useful for high level interfaces as it should prevent crashing on error, allow memory cleanup on error, and make it easier to implement user interrupt.
- Ability to interrupt the process in high level interfaces.

### Fixed

- Memory error in `remove_diagonal`.
- Move any function that can be supplied by a high level language outside of threaded code.

### Changed

- Update igraph from 0.10.12 -> 0.10.13.
- Optimized performance.

## [v0.1.5] 2024-07-23

### Added

- `se2_knn_graph` to create k-nearest neighbor graph from a matrix.

### Fixed

- Add explicit `void` in empty function prototype.
- Fix two memory leaks. 1. letting igraph allocated rng objects get destroyed before their memory was freed 2. When creating a new label, it was possible the element one step out of an array's bound was being accessed.
- Correct spelling of "reweigh".

### Changed

- Return to including "igraph.h" instead of refined header files since igraph will expect this behavior in 1.0.0.
- Compile only subset of igraph files.
- Move from OpenMP -> pthreads. So CRAN will stop complaining.
- Move `order_nodes` to seperate C file.
- Use doxygen syntax to document public functions.

## [v0.1.4] 2024-06-04

### Added

- Print header to modify print behavior on a programming language basis.

### Fixed

- Add OMP critical block to ensure print statements don't run in parallel. This was causing errors with R's `Rprintf` but I believe `printf` isn't guaranteed thread safe either. Should not impact performance since this occurs once per thread before starting the real work.

### Changed

- Don't explicitly use internal igraph dependencies.
- Update to igraph 0.10.12.
- Explicitly include needed igraph headers instead of catchall "igraph.h".
- Touch up printed statements.

## [v0.1.3] 2024-04-19

### Fixed

- Guard against the USING_R cpp macro which breaks examples.
- Guard against missing OpenMP. (See https://cran.r-project.org/doc/manuals/R-exts.html#OpenMP-support).

### Changed

- Update igraph to 0.10.11.
- Remove parallel for loop in subclustering (without getting into nested parallelization, performance using parallel across independent runs than across communities since the largest communities take exponentially longer to cluster than smaller communities).

## [v0.1.2]

### Added

- Subclustering.

### Changed

- Use a block graph for example instead of ZFC to exaggerate community structure.
- Use org-mode instead of markdown for README to evaluate code.
- Node ordering now orders by degree within communities.

## [0.1.1] 2024-03-09

### Added

- Add node ordering function.

### Fixed

- Typos / general wording in README.

### Changed

- Split project into core library (here) and high level packages (elsewhere).
