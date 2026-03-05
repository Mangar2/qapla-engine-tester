# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.5.0] - 2026-01-18

### Added

- **Per-instance engine logging**: New `[logging]` configuration group
  - `logging.path`: Path to the logging directory
  - `logging.mode=one|each`: Engine log file strategy
    - `one` (default): All engines log to a single file
    - `each`: Each engine instance creates separate log files
  - `logging.engine`: Enable/disable engine logging

- **Integration test framework**: New modular PowerShell-based testing framework
  - Test runner with filtering and listing capabilities
  - Validator plugin system (exit codes, file counts, content validation)
  - Organized test definitions in subdirectories
  - Automatic cleanup and logging verification

- **SPRT decision modes**: Support for 5 SPRT decision calculation modes
  - Trinomial statistics: normalized, logistic, bayesian models
  - Pentanomial statistics: normalized, logistic models
  - New `--sprt model` and `--sprt pentanomial` options for model selection

- **SPRT tournament persistence**: SPRT tournament files now compatible with GUI and CLI
  - Read and write SPRT tournament files across GUI/CLI applications
  - Automatic periodic saving with `--sprt saveinterval` option
  - Resume interrupted SPRT tournaments

- **SPSA parameter optimization**: New parameter optimization capability via SPSA algorithm
  - New `--spsa` and `--spsa value` parameter groups
  - Iterative parameter tuning for engine optimization

- **CLOP parameter optimization**: New Confident Local Optimization algorithm for noisy black-box tuning
  - New `--clop` and `--clop value` parameter groups
  - Weighted local quadratic logistic regression with confidence-based sampling
  - Integrated into CLI task dispatch and MCP tooling with queue/background support

- **SPRT final result logging**: Log file now includes final SPRT decision and statistics


