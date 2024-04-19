# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
