# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- **Engines when continuing a tournament or SPRT run**: The `[engine]` sections of
  a tournament/SPRT file are used exactly when no engine is given on the command
  line or in a settings file. As soon as a single engine is configured there, the
  file's engine sections are ignored completely instead of being added on top.
  This makes it possible to drop an engine from a resumed run, or to move a
  tournament file to another machine and point the engines at their local
  directories, without every engine — and thus every pairing — being duplicated.
  A status line at startup states which of the two applies.

- **Immediate stop test repeats its scenario**: Whether an engine already has a
  move when the `stop` arrives is a matter of timing, so a single attempt let
  faulty engines pass whenever they happened to be quick enough. The test now
  runs the scenario ten times, each as a new game so that no result of the
  previous search can be reused. Engines that answer `stop` with an invalid
  bestmove are detected far more reliably; the test still costs about a tenth of
  a second.

## [0.6.0] - 2026-08-06

### Added

- **Perft — move generator node count test**: New `--perft` mode
  - Counts all positions reachable from a start position within a given number
    of moves. This is the standard way to check that a move generator produces
    exactly the legal moves — no more, no less — and to measure how fast it is.
  - Needs no engine at all; it uses Qapla Engine Tester's own move generator
  - Runs on as many CPU cores as `--concurrency` allows
  - Options: `position` (start position or any FEN), `depth` (number of plies),
    `divide` (show the count for each single first move), `showfen` (also show
    the position after each first move)
  - Prints total node count, elapsed time and nodes per second

- **Marking engines as selected**: Engines in the engines file can now carry a
  `selected` flag, so a graphical front-end can remember which engines were
  picked for the next run.

### Changed

- **More accurate Elo ratings in tournaments**: Ratings are now calculated so
  that the score each engine is *expected* to achieve against its actual
  opponents matches the points it really scored.
  - The previous method could rate an engine lower than a rival even though it
    had scored more points against exactly the same opponents. This can no
    longer happen: against an identical field of opponents, more points always
    means more Elo.
  - Ratings no longer depend on the order in which the individual matches were
    processed, so the same tournament always yields the same rating list.
  - Rounding is now applied only for display. Intermediate values keep their
    full precision, which previously could add up to a noticeable error in
    tournaments with many pairings.

- **Gauntlet tournaments without an explicitly marked engine**: If no engine is
  marked with `gauntlet=true`, the first engine listed is now used as the
  gauntlet engine instead of the tournament being rejected with an error. This
  is what the documentation described, and what SPRT already did.

- **Engine restarts state their reason**: Whenever an engine is restarted, the
  engine log now records why — for example that the engine was not running at
  the start of a game, that a restart was requested, that `restart=always` is
  configured, or that the engine exceeded its thinking time without returning a
  move. This makes it much easier to tell an engine problem from a
  configuration effect when reading a log.

- **Reliable handling of special characters**: Engine names and other text
  containing quotes, backslashes or tab characters are now written correctly to
  result and report files. Previously such entries could produce files that
  other tools refused to read.

- **Clearer errors on malformed input**: When Qapla Engine Tester runs as an MCP
  server, malformed or empty input is now reported as a proper error message
  instead of being ignored silently.

### Fixed

- **Wrong clock times for openings with Black to move**: When a game started
  from an opening or EPD position in which Black is to move, remaining time,
  increment and "moves to go" were assigned to the wrong side. Both the times
  sent to the engines and the times recorded per player were affected. Games
  starting with Black to move are now timed correctly.

- **SPRT result files were not saved**: Due to a mismatched internal name, SPRT
  runs could silently skip writing their result file — so progress was lost and
  an interrupted run could not be resumed. SPRT result files are now saved as
  documented.

- **Engine capability cache could become unreadable**: The stored information
  about an engine's options could be written in a form that Qapla Engine Tester
  itself failed to read back, in particular for options offering a list of
  choices. Affected entries are now written and read correctly; caches written by
  older versions still work.

- **Duplicate entries in configuration files**: Saving a configuration could add
  a second identifier line to a section. Such lines are now replaced instead of
  duplicated, and sections written by earlier versions are repaired
  automatically on the next save.

- **Empty optional file paths were rejected**: Leaving an optional path empty
  (for example: no tournament result file configured yet) no longer causes a
  parameter error.

- **Normal engine shutdown reported as an error**: When Qapla Engine Tester
  itself stopped an engine — for instance to restart it with different options —
  the log showed a disconnect error even though everything worked as intended.
  This is now logged as an expected shutdown.

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
