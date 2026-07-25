# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- **JSON handling migrated to `src/json` (`mqtt::json::JsonValue`)**: replaces the
  old `Mcp::JsonHelper`/`Mcp::JsonValue` (hand-written parser/serializer) across
  table-format, base-logger, tournament-result, engine-capability, app-runner, and
  the full MCP layer (converter, schema-builder, job-scheduler, background-tools,
  engine-tool, message-channel, server).
  - **String escaping fixed**: previously only `\"`, `\\`, `\n` were escaped;
    control characters, `\t`, `\r`, and other required escapes are now handled
    correctly. Rating-table/outcome JSON and MCP payloads containing such
    characters (e.g. an engine name with `"` or `\`) are now valid JSON where
    they previously were not.
  - **Persisted engine-capability cache is now compact**: `option.*` lines
    written to the capability INI cache no longer contain spaces after `:`/`,`.
    Lines written by older builds (with spaces) still parse correctly.
  - **Internal interfaces are tree-typed**: `AppRunner`, `TournamentResult`,
    `TableFormat`, `JobScheduler`, and the MCP tool handlers now pass
    `JsonValue` trees instead of pre-serialized JSON strings; `stringify()`/
    `parse()` happen once each, at the real boundary (MCP stdio, persisted
    capability line).
  - **Stricter parsing at input boundaries**: malformed or empty JSON at the
    MCP stdin boundary is rejected (mapped to a JSON-RPC parse error) instead
    of silently returning null.

## [0.5.0] - 2026-04-03

### Added

- **MCP server**: Model Context Protocol support for AI-assisted engine testing
  - Tool-based interface for SPRT, SPSA, CLOP, EPD, tournament and engine test
  - Job scheduler with queue/background execution
  - Preconfigured settings files support
  - JSON result notifications and log file access

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

- **System test**: New `--systemtest` mode for NPS stability analysis
  - Evaluates how stable a platform allocates computation time to engines when running multiple games in parallel
  - Replays identical games at increasing concurrency levels
  - Per-step NPS statistics with standard deviation to determine optimal concurrency

- **EPD enhancements**: Support for `depth` and `nodes` search limits in EPD tests

- **WinBoard/XBoard engine support**: Full support for WinBoard/XBoard protocol engines alongside UCI

- **Engine command-line arguments**: New support for passing arguments to engine executables

- **SPRT final result logging**: Log file now includes final SPRT decision and statistics

### Changed

- **Centralized settings management**: Refactored configuration into `Settings::Manager` with `fromManager` pattern across all config classes
- **Source tree reorganization**: Moved modules into dedicated subdirectories (game-manager, engine-handling, etc.)
- **Adjudication enabled in rapid mode**


