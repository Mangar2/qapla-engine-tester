# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- **Linux binary starts without an LLVM runtime installed**: The C++ runtime is now
  linked into the binary instead of being loaded from `libc++.so.1`, which most
  distributions do not ship.

## [0.6.0] - 2026-08-27

### Added

- **`args` for engines**: now part of the parameter definition, so `--engine args=...`
  is accepted and survives a resume.
- **Perft — move generator node count test**: New `--perft` mode counts all positions
  reachable from a start position to a given depth. Needs no engine, runs across
  `--concurrency` threads. Options: `position`, `depth`, `divide`, `showfen`.
- **Marking engines as selected**: engines file entries can carry a `selected` flag, so
  a graphical front-end can remember which engines were picked.

### Changed

- **Engine sections in tournament/SPRT files no longer merge with a selected engine**:
  selecting an engine on the command line or in a settings file now replaces the file's
  own `[engine]` sections instead of adding to them — engines can be dropped from a
  resumed run, or a file moved to another machine, without every engine being duplicated.
- **Engine sections in state files are shorter**: only what differs from `[each]` is
  recorded, instead of every shared default.
- **Immediate stop test repeats its scenario** ten times instead of once, so a flaky pass
  no longer depends on timing.
- **Unified report log headers** across SPRT, Tournament, EPD, CLOP, SPSA and Engine-Test.
- **More accurate Elo ratings**: fitted to each engine's actual score instead of averaged
  per duel, so ratings no longer depend on match order or occasionally invert against a
  clearly weaker opponent.
- **Gauntlet tournaments without an explicit `gauntlet=true`** now use the first engine
  listed instead of rejecting the run.
- **Engine restarts state their reason** in the engine log.
- **Reliable handling of special characters** (quotes, backslashes, tabs) in engine names
  when writing result/report files.
- **Clearer MCP errors on malformed input**, reported instead of ignored.
- **`--test` hash-table memory and lower-case-option checks are off by default**
  (`nomemory`, new `nolowercase`): both compare process memory before/after a `Hash`
  change, which macOS does not reliably reflect, so they failed regardless of the engine
  tested.
- **Tournament standings render identically during and after the run**, and in `info`;
  error column now labelled `+/-`, Elo shown to one decimal.
- **Documentation — return codes**: clarified that an engine failing during SPRT/tournament
  returns the run's normal outcome, not `10`; codes `10`–`12` are `--test`-only.

### Fixed

- **Tournament/SPRT files with UCI options could not be resumed, or lost `[each]`'s shared
  options on save**: engine sections wrote UCI options without the `option.` prefix the
  file format requires; reading and writing are now consistent.
- **Wrong clock times for openings with Black to move**: time, increment and moves-to-go
  were assigned to the wrong side.
- **SPRT result files were not saved**, due to a mismatched internal name.
- **Engine capability cache could become unreadable** for options with a choice list; old
  caches still read correctly.
- **Duplicate identifier lines in configuration files** are now replaced instead of piling
  up.
- **Empty optional file paths were rejected** as a parameter error.
- **Normal engine shutdown was logged as a disconnect error.**
- **A stopped run could hang for ever**: a stop was treated as carried out while it was
  still queued, so whoever waited for it was never released.
- **Crash on exit while engines were still running**: the logging singletons were destroyed
  under the threads still writing to them.
- **Sending `quit` to an engine that had already gone could kill the process** through
  SIGPIPE instead of reporting a broken pipe.

## [0.5.0] - 2026-04-03

### Added

- **MCP server**: Qapla Engine Tester can run as a Model Context Protocol server,
  making its test and tournament features usable from AI assistants
  - SPRT, SPSA, CLOP, EPD, tournament and engine test available as callable tools
  - Jobs can be queued and run in the background
  - Prepared settings files can be used
  - Results are reported back and log files can be retrieved

- **Per-engine logging**: New `[logging]` configuration group
  - `logging.path`: directory for log files
  - `logging.mode=one|each`: all engines in one log file (`one`, default), or a
    separate log file per engine instance (`each`)
  - `logging.engine`: switch engine logging on or off

- **Automated integration tests**: A test suite that runs Qapla Engine Tester end
  to end and checks exit codes, generated files and their content

- **SPRT decision modes**: Five ways of computing the SPRT decision
  - Per-game statistics: normalized, logistic and bayesian model
  - Per-game-pair statistics: normalized and logistic model
  - Selectable via `--sprt model` and `--sprt pentanomial`

- **Resumable SPRT runs**: SPRT result files are now shared between the
  graphical interface and the command line
  - Written and read by both applications
  - Saved periodically during the run (`--sprt saveinterval`)
  - Interrupted SPRT runs can be continued

- **SPSA parameter optimization**: Automatic tuning of engine parameters
  - New `--spsa` and `--spsa value` parameter groups
  - Parameters are improved step by step over many self-play games

- **CLOP parameter optimization**: A second tuning method, designed for noisy
  results
  - New `--clop` and `--clop value` parameter groups
  - Concentrates games on the most promising parameter values
  - Also available from the command line and via the MCP server, with queued and
    background execution

- **System test**: New `--systemtest` mode measuring speed stability
  - Shows how evenly a computer distributes computing power to engines when
    several games run in parallel
  - Replays identical games at increasing numbers of parallel games
  - Reports speed fluctuation per step, so you can pick the highest number of
    parallel games that still yields trustworthy results

- **EPD enhancements**: EPD tests can now run with a fixed `depth` or node count
  (`nodes`) instead of a time limit

- **WinBoard/XBoard engine support**: Engines using the WinBoard (XBoard)
  protocol are fully supported alongside UCI engines

- **Engine command-line arguments**: Engines can be started with additional
  command-line arguments

- **SPRT final result in the log**: The log file now contains the final SPRT
  decision including its statistics

### Changed

- **Reworked configuration handling**: All settings are managed uniformly, which
  makes settings files, command line and MCP behave consistently
- **Adjudication is now also active in rapid mode**
