# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Store labels heard instead of recalculating every step to improve speed at cost of memory.

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
