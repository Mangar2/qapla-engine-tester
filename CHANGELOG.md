# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.5.0] - 2026-01-18

### Added

- **Per-instance engine logging**: New `logmode` CLI option in the `--each` group
  - `logmode=one` (default): All engines log to a single file
  - `logmode=each`: Each engine instance creates a separate log file
  - Configuration via: `--each logmode=one|each`

- **SPRT final result logging**: The SPRT test now logs the final decision and statistics
  - Logs decision (H1 Accepted / H0 Accepted / Inconclusive)
  - Logs Log-Likelihood Ratio (LLR) with 2 decimal places
  - Logs total game count and breakdown (Wins A / Draws / Wins B)
  - Added `logFinalResult()` method to SprtManager

- **Logger configuration refactoring**: 
  - New `LoggerConfig` struct usage across all test modes
  - Added `applyLoggerConfig(reportLogBaseName)` helper method for cleaner code

### Changed

- Simplified logger setup in all test modes (EPD, Test, SPRT, SPSA, Tournament)
  - Replaced verbose 8-line `setLoggerConfig()` calls with single `applyLoggerConfig()` calls
  - Improved code maintainability and reduced duplication

### Technical Details

**Files Modified:**
- `src/cli/qapla-settings.h`: Added LoggerConfig management
- `src/cli/qapla-settings.cpp`: Added `readLoggerConfig()` and `applyLoggerConfig()` methods
- `src/qapla-engine-tester.cpp`: Simplified logger setup across all test modes
- `src/sprt/sprt-manager.h`: Added `logFinalResult()` declaration
- `src/sprt/sprt-manager.cpp`: Added `logFinalResult()` implementation

**Example Output:**
```
SPRT final result: normalized-trinomial | decision: H1 Accepted | LLR: 42.50 | games: 10000 (W:5234 D:3450 L:1316)
```
