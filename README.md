# Qapla Engine Tester

**Version**: 0.6.0  
**Author**: Volker Böhm  
**Repository**: [https://github.com/Mangar2/qapla-engine-tester](https://github.com/Mangar2/qapla-engine-tester)

Qapla Engine Tester is a command-line tool for running tournaments and analyzing or testing UCI-compatible chess engines. It provides the following core features:

- **Tournament play** with Gauntlet and Round-Robin formats  
- **SPRT testing** to statistically compare engine strength  
- **Resumable tournaments** (Tournament & SPRT) via result files for interrupted runs  
- **SPSA optimization** for automated engine parameter tuning  
- **CLOP optimization** using confident local optimization  
- **EPD-based position analysis** across multiple engines in parallel  
- **NPS stability system test** to evaluate platform stability for parallel engine games  
- **Perft (move generator node count test)** with parallel root-move divide  
- **Full WinBoard/XBoard engine support** alongside UCI  
- **Opening book support** via PGN, EPD, or raw formats  
- **Pondering** support and **fully parallel gameplay** across any number of games  
- **Interactive mode** to change concurrency, inspect status, or stop runs gracefully  
- **MCP server mode** for AI-assisted engine testing  
- **Stress and compliance testing** for UCI engine behavior  
- **Flexible configuration** via command-line, settings file, or both  
- **Batch mode** with structured return codes for CI integration

All features are fully configurable and optimized for multi-core systems.

> **Full parameter reference**: See [PARAMETERS.md](PARAMETERS.md) for a complete, auto-generated list of all CLI options and their defaults.

## Table of Contents

- [What's New in Version 0.6.0](#-whats-new-in-version-060)
- [Ultra-Fast Testing](#-ultra-fast-testing)
- [General Options](#-general-options)
- [Return Codes for Batch Processing](#-return-codes-for-batch-processing)
- [Configuration Precedence](#-configuration-precedence)
- [Engine .ini Configuration](#engine-ini-configuration---enginesfile)
- [Settings File Support](#️-settings-file-support---settingsfile)
- [Interactive Mode](#-interactive-mode)
- [Shared Engine Options (--each)](#️---each-group--shared-engine-options)
- [EPD Position Analysis](#-epd-position-analysis)
- [PGN Output](#-pgn-output)
- [Opening Selection](#️-opening-selection)
- [Tournament Mode](#-tournament-mode)
- [SPRT Testing](#-sprt--sequential-probability-ratio-test)
- [SPSA Optimization](#-spsa-parameter-optimization)
- [CLOP Optimization](#-clop--confident-local-optimization)
- [Tournament and SPRT Result Files](#-tournament-and-sprt-result-files)
- [Engine Testing Suite](#-engine-testing-suite)
- [NPS Stability System Test](#-nps-stability-system-test)
- [Perft — Move Generator Node Count Test](#-perft--move-generator-node-count-test)
- [MCP Server Mode](#-mcp-server-mode)
- [Example Combined Run](#example-combined-run)
- [Platform and Installation](#️-platform-and-installation)
- [Limitations](#limitations)
- [Feedback](#feedback)

---

## 🆕 What's New in Version 0.6.0

- **Perft Mode** — The new `--perft` option counts the leaf nodes reached by exhaustively playing out all legal move sequences from a position to a fixed depth. It verifies move generator correctness and measures raw speed, works without any engine, and distributes root moves across up to `--concurrency` threads. See [Perft](#-perft--move-generator-node-count-test).

- **More Accurate Elo Rating** — Tournament ratings are now fitted so that each engine's expected score matches the points it actually scored, instead of averaging per-duel Elo differences. Because Elo is non-linear in the score, the old method could rate an engine below a rival it had outscored against the same opponent field, and its result depended on the order in which duels were visited. Both effects are gone.

- **Correct Clocks for Black-to-Move Openings** — Games starting from an opening or EPD position with Black to move previously assigned remaining time, increments and `movestogo` to the wrong side. Time accounting now honors the starting side.

- **Gauntlet Without an Explicit Flag** — If no engine is marked `gauntlet=true`, the first engine is now used as the gauntlet engine instead of the tournament being rejected — the same fallback SPRT already used.

- **Restart Diagnostics** — Every engine restart now records *why* it happened (engine not running at game start, restart requested, `restart=always` between games, thinking-time overrun without a bestmove) in the engine log, and an intentional shutdown is no longer reported as a disconnect error.

- **Reliable Handling of Special Characters** — Engine names containing quotes, backslashes or tabs are now written correctly to result and report files; previously such entries could produce files that other tools refused to read.

- **SPRT State File Fix** — SPRT result files are now saved reliably; a mismatched internal identifier previously caused saves to be skipped silently.

- **Resumable Files with UCI Options** — A tournament or SPRT file whose engines carried a UCI option, or an `[each]` section with shared options, could no longer be read back; both now round-trip correctly.

- **Selecting an Engine Overrides the File** — Selecting an engine on the command line or in a settings file now fully replaces a tournament/SPRT file's own `[engine]` sections, instead of adding to them — engines can be dropped from a resumed run, or a file moved to another machine, without every engine being duplicated.

- **Compliance Test Defaults Adjusted for macOS** — The `--test` hash-table memory check and a new lower-case-option check (`nolowercase`) are now off by default: both compare process memory before/after a `Hash` change, which macOS does not reliably reflect.

> Full details in [CHANGELOG.md](CHANGELOG.md).

### Previously in Version 0.5.0

- **MCP Server Mode** — Qapla Engine Tester can now run as a Model Context Protocol (MCP) server, exposing its test and tournament features as AI-callable tools. This enables integration with LLM-based workflows for automated engine testing.

- **SPSA Parameter Optimization** — New `--spsa` and `--spsavalue` groups for automated UCI parameter tuning using Simultaneous Perturbation Stochastic Approximation.

- **NPS Stability System Test** — The new `--systemtest` mode evaluates how stable a platform allocates computation time to engines when running multiple games in parallel. It replays identical games at increasing concurrency levels and measures per-move NPS standard deviation. This helps determine how many parallel games can be played in tournaments without results being distorted by platform issues such as CPU overload or performance/efficiency core scheduling.

- **Full WinBoard/XBoard Engine Support** — Engines using the WinBoard (XBoard) protocol are now fully supported alongside UCI engines in all modes.

- **EPD Depth and Node Limits** — EPD analysis can now run with fixed `depth` or `nodes` limits instead of time-based search, enabling hardware-independent benchmarks.

- **Engine CLI Arguments** — Engines can now receive command-line arguments via `args=...` in their configuration.

- **Logging Overhaul** — Engine logging and trace settings are now grouped under `--logging` instead of individual global flags, with support for per-engine log files.

- **Settings Refactoring** — Internal settings management has been restructured for better extensibility and MCP integration.

These additions make Qapla Engine Tester suitable not only for statistical testing, but also for full-scale tournament automation, AI-driven testing workflows, and hardware benchmarking.

---

## ⚡ Ultra-Fast Testing

Qapla Engine Tester is optimized for high-throughput testing under extremely short time controls. With the `--rapid` option enabled, all non-essential processing—such as `info`-line evaluation, PV checks, or score tracking—is disabled. Combined with options to suppress PGN output and communication logging, this enables rapid testing at minimal overhead.

### Example: SPRT under 1+0.005 Time Control

In a test run comparing Qapla 0.3.1 and Qapla 0.3.2 (identical playing strength, but 0.3.2 includes interface bugfixes), an SPRT tournament with the following setup was executed:

- **Time control**: 1 second base + 5ms increment
- **Parallel threads**: 20 (on a 16-core machine)
- **Games played**: 8182
- **Result**: No statistically significant strength difference, and only 3 loss-on-time incidents (statistically negligible)
- **Total duration**: 18 minutes 26 seconds
- **Average match duration**: 2.7 seconds

This demonstrates Qapla Engine Tester's capability for ultra-fast, large-scale testing with extremely low overhead.

---

## 🔁 General Options

Global options control the overall behavior of the tester and apply across all test types. Key settings include the number of parallel engines (`--concurrency`), an optional engine registry file (`--enginesfile`), a settings file for reusable configurations (`--settingsfile`), interactive mode (`--interactive`), and a rapid mode (`--rapid`) that suppresses engine info lines for maximum throughput.

---

## 🔚 Return Codes for Batch Processing

All test modes now return structured numeric exit codes to support automated evaluation and integration in batch processing scripts or CI pipelines.

Return codes are prioritized — if multiple situations occur, the lowest relevant non-zero code (excluding `0`) is returned. For example: a parameter error (`2`) takes precedence over an engine failure (`10`) or a missed target (`13`).

The engine codes `10`–`12` are reported by the engine test suite (`--test`) only. A tournament or SPRT run judges *engines against each other*, so a single engine failing is part of the result, not a reason to discard it: the failing engine forfeits the affected game and the run returns its regular outcome — see [Engine failures during tournaments and SPRT](#engine-failures-during-tournaments-and-sprt).

### Return Code Overview

| Code | Name                 | Meaning                                                                 |
|------|----------------------|-------------------------------------------------------------------------|
| 0    | `NoError`            | Everything ran correctly; test completed as expected                    |
| 1    | `GeneralError`       | Unexpected program error (e.g. crash, unhandled exception)              |
| 2    | `InvalidParameters`  | Invalid or missing CLI parameter                                        |
| 10   | `EngineError`        | Engine crashed, could not start, or returned illegal moves (only in `--test`) |
| 11   | `EngineMissbehaviour`| Engine hung, ignored protocol, or failed to follow commands (only in `--test`) |
| 12   | `EngineNote`         | Test completed, but non-critical engine issues occurred (only in `--test`) |
| 13   | `MissedTarget`       | EPD target success threshold was not reached (`--epd`)                  |
| 14   | `H1Accepted`         | SPRT result: H₁ (stronger engine) accepted (`--sprt`)                   |
| 15   | `H0Accepted`         | SPRT result: H₀ (no significant difference) accepted (`--sprt`)         |
| 16   | `UndefinedResult`    | SPRT result could not be decided within maxGames (`--sprt`)             |

### Prioritization Rules

- Codes `1`, `2`, `10`, `11`, `12` always take priority over `13`–`16`
- Code `0` is only returned if **no issues** occurred
- Codes `10`, `11` and `12` come from the engine test suite (`--test`); `13`–`16` from `--epd` and `--sprt`. The two groups therefore never compete for the same run
- Among `13`–`16`, only one is returned depending on outcome

### Engine failures during tournaments and SPRT

A tournament or SPRT run compares engines with each other, and an engine that crashes, hangs or never
completes its handshake is a property of that engine — not a defect of the comparison. Such a failure
therefore **never changes the exit code** of the run:

- The affected game is adjudicated against the failing engine (a forfeit) and reported with its cause
- The run continues with the remaining games and returns its regular outcome — `14`, `15` or `16` for
  SPRT, `0` for a tournament
- The failure itself is visible in the report log (`… failed to start …, adjudicating as …`) and in
  the outcome table, not in the exit code

This is deliberate: a single flaky game should not invalidate a comparison of several thousand, and
any threshold above which failures *would* invalidate it ("often", "too often") would be arbitrary.
Use `--test` to judge whether an engine itself is sound; use `--sprt` / `--tournament` to judge how
engines compare.

Configuration problems are unaffected by this rule — an engine whose executable cannot be found is a
parameter error and still returns `2` before any game starts.

### Examples

- An SPRT test ends with undecided result → return `16`
- An EPD run fails to reach expected correctness rate → return `13`
- An engine crashes during an SPRT test → still `14`–`16`; the lost game is adjudicated as a forfeit
- An engine crashes during a tournament → still `0`; see the report log for the forfeited game
- An engine fails the compliance suite (`--test`) → return `10`, `11` or `12`
- A CLI parameter is missing → return `2`, regardless of test type

Use these codes in automation scripts to check for test outcomes or failure causes.

---

## 🔀 Configuration Precedence

Qapla Engine Tester supports multiple configuration sources. When the same setting appears in more than one source, the following priority applies (highest first):

1. **Tournament/SPRT result files** — When a tournament or SPRT result file is specified and already exists, all settings stored in that file take absolute precedence. Using a result file tells the program "continue this tournament/SPRT exactly as configured". This ensures consistency when resuming interrupted runs.
2. **Command-line arguments** — CLI parameters override settings from the settings file and engines file. This is ideal for batch runs where a base configuration is stored in a settings file and only the varying parameters are passed via CLI.
3. **Settings file (`--settingsfile`)** — Supports all CLI options in INI format. Lower priority than CLI, allowing selective overrides.
4. **Engines file (`--enginesfile`)** — Contains only engine definitions. Engines are referenced from other configuration sources via the `conf` parameter. Engine options specified inline (e.g., in `--engine conf=... option.Hash=256`) override values from the engines file.

### Engine Option Precedence

Within engine configuration specifically:

1. **Inline via `--engine`** — Highest priority. Overrides all others.
2. **Via `--each`** — Shared defaults for all engines. Overridden by individual `--engine`.
3. **From `--enginesfile` via `conf=...`** — Base configuration. Overridden by both.

> See [PARAMETERS.md](PARAMETERS.md#--engine) for all available engine sub-options.

---

## Engine `.ini` Configuration (--enginesfile)

[engine]  
name=Spike 1.4
protocol=uci  
executablePath=C:\Chess\cutechess-cli\qapla0.3\Spike1.4.exe  
Hash=128  

[engine]
name=Qapla 0.3.2bb
protocol=uci  
executablePath=C:\Chess\delivery\Qapla0.3.2\Qapla0.3.2-win-x86.exe  
workingDirectory=.  
Hash=128  
qaplaBitbasePath=C:\Chess\bitbases\lz4  
qaplaBitbaseCache=512  

Note: Required is only executablePath.

---

## 🗂️ Settings File Support (`--settingsfile`)

Qapla Engine Tester allows all command-line options to be specified via a settings file in INI format. This enables clean, reusable configurations—especially useful for longer test or tournament definitions.

To use a settings file, pass the path via:

```bash
--settingsfile=path/to/config.ini
```

### Settings File Format

The settings file uses INI format and maps directly to CLI parameters. Section headers correspond to CLI parameter groups (e.g., `--tournament` becomes `[tournament]`), and keys within each section match the CLI sub-parameters. Global options appear at the top without a section header.

### Example `config.ini` file:

```ini
enginesfile=C:\Chess\engines.ini
concurrency=10

[logging]
engine=true
path=log
mode=each

[tournament]
type=gauntlet
file=log/tournament.tour
rounds=2
games=8
repeat=2

[engine]
conf=Qapla 0.3.2
gauntlet=true

[engine]
conf=Qapla 0.3.1

[each]
proto=uci
tc=10+0.02

[pgnoutput]
file=log/games.pgn

[openings]
order=random
file=C:\Chess\book8ply.raw
```

All CLI options are fully supported inside the file, including multiple `[engine]` sections and all grouped parameters. Command-line arguments override values from the settings file if both are present — see [Configuration Precedence](#-configuration-precedence).

---

## 💬 Interactive Mode

When `--interactive` is enabled, Qapla Engine Tester enters a command-driven mode that allows you to monitor and control the run in real-time via standard input.

This mode is particularly useful during long tournaments or test runs where dynamic adjustments or early termination may be needed.

### Available Commands

- `quit` / `q`  
  Exit the program gracefully after all current games have finished.

- `leaveinput` / `l`  
  Leave interactive mode and continue running the test. The program will exit, after all games are finished.

- `info` / `?`  
  Show current engine/game state and overall progress.

- `concurrency` / `c`  
  Change the number of concurrently running games (e.g., `c 4`).

- `abort` / `a`  
  Immediately stop all current games and exit the run.

- `help` / `h`  
  Display the list of available commands.

---

## ♻️ `--each` Group — Shared Engine Options

Defines default values that apply to **all** engines unless overridden in their respective `--engine` definitions. Useful for setting a common time control, protocol, or UCI options without repeating them for each engine.

### Example

```bash
--each option.Threads=2 proto=uci tc=10+0.02
--engine name=engineA cmd=./engineA
--engine name=engineB cmd=./engineB option.Threads=4
```

---

## 📄 EPD Position Analysis

Qapla Engine Tester supports efficient EPD-based analysis across multiple engines in parallel, utilizing all available CPU cores. It reads `.epd` files containing `bm` (best move) tags and compares each engine´s output against the expected move.

Results are printed in a side-by-side format for easy comparison, and a detailed `.log` file is generated for later inspection.

> 🔍 **Use Case Example**: Analyze 100 endgame positions with 16 engines in parallel. Identify which engines find the correct move fastest — and which fail.

### Features

- Fully parallel engine evaluation on multi-core systems
- Compatible with `.epd` files containing `bm` (best move) tags
- Per-move time control with optional early stopping
- Structured output and detailed log file
- Designed for reproducible and objective engine comparisons

### Example Output

(Speelman_EP_1) | 00.127, D: 15, M: d4d5 | 00.399, D: 12, M: d4d5 | 00.037, D: 10, M: d4d5 | 00.084, D: 11, M: d4d5 | BM: Kd5
(Speelman_EP_2) | 01.592, D: 26, M: a5b6 | 00.001, D: 2, M: a5b6 | 00.016, D: 1, M: a5b6 | 00.001, D: 5, M: a5b6 | BM: Kb6

Each row shows:
- Time spent until the move was found
- Search depth reported
- Move suggested by each engine
- Expected best move from EPD (`BM:` column)

The output is saved automatically to a log file named like:  
`epd-report-YYYY-MM-DD_HH-MM-SS.log`

### Examples

```bash
--epd file="endgames.epd" maxtime=30 seenplies=3
--epd file="endgames.epd" depth=12
--epd file="endgames.epd" nodes=500000
```

---

## 📤 PGN Output

Controls how game results are saved in PGN format. Use this for any game-based test mode (SPRT, tournament, SPSA) to specify the output file and select which annotations to include — such as clock times, evaluation scores, search depth, and principal variations.

### Example

```bash
--pgnoutput file="games.pgn" append=true eval=true pv=true
```

---

## ♟️ Opening Selection

Controls how opening positions are assigned to games. Required for all game-based test types such as SPRT, tournaments, or SPSA. Supports `.epd`, `.pgn`, and raw FEN files as input, with configurable selection order (sequential or random), ply depth for PGN openings, and policies for when to switch to the next opening.

### Example

```bash
--openings file="openings.pgn" order=random plies=8 policy=round
```

---

## 🏆 Tournament Mode

Qapla Engine Tester supports automated tournaments between multiple engines using **Gauntlet** or **Round-Robin** formats. Tournaments are fully configurable and resumable via result files.

In **Gauntlet** mode, one or more engines marked with `gauntlet=true` play against all other engines. In **Round-Robin** mode, every engine plays against every other engine. You can control the number of games per pairing, rounds, color swapping, and rating output intervals.

**Engines live for one pairing.** An engine process is started for a pairing, plays its `games` games, and is closed when the pairing ends — the pairing of the next round starts engine processes of its own, even where both rounds pair the very same engines. `restart` only governs what happens *between the games of a pairing*, so splitting a match into `rounds` is what decides how often the engines are started anew: `games=100 rounds=1` starts them once, `games=10 rounds=10` starts them ten times.

---

## 📊 SPRT — Sequential Probability Ratio Test

Enables a formal statistical strength comparison between two engines. The test continuously evaluates win/draw/loss results and stops early when one of the hypotheses (H₀ or H₁) is statistically confirmed, or a maximum number of games is reached.

SPRT determines whether a **Challenger** engine (marked with `gauntlet=true`, or the first engine) is stronger than a **Baseline** engine:

- **H₀ (Null)**: Challenger Elo ≤ Baseline + eloH0
- **H₁ (Alternative)**: Challenger Elo ≥ Baseline + eloH1

Common configurations:
- **Improvement test**: `eloH0=0 eloH1=5` — detect a gain of at least 5 Elo
- **Regression test**: `eloH0=-5 eloH1=0` — detect a loss of at least 5 Elo

Additionally, a **Monte Carlo simulation mode** (`montecarlo=true`) is available to evaluate how reliable a given SPRT configuration is under different Elo differences, without running real games.

### Monte Carlo Output Example
```
Running SPRT Monte carlo simulation:  | Elo range: [0, 10] | alpha: 0.05, beta: 0.02 | maxGames: 3000
Simulated elo difference:    -25  No Decisions:    0.6%  H0 Accepted:   99.4%  H1 Accepted:    0.0%  Average Games: 1400.9
Simulated elo difference:      0  No Decisions:   68.8%  H0 Accepted:   28.7%  H1 Accepted:    2.5%  Average Games: 2738.1
Simulated elo difference:     25  No Decisions:   19.6%  H0 Accepted:    0.0%  H1 Accepted:   80.4%  Average Games: 1931.9
```

### Example

```bash
--sprt eloH0=0 eloH1=5 alpha=0.05 beta=0.05 maxgames=10000
```

---

## 🔬 SPSA Parameter Optimization

Optimizes engine parameters using the Simultaneous Perturbation Stochastic Approximation (SPSA) algorithm. Parameters are perturbed in multiple iterations to find optimal values that maximize playing strength. Only one engine is configured: each iteration plays two perturbed copies of it against each other, and the parameters are moved towards the winner. Only integer-valued UCI options can be tuned.

Define the optimizer with `--spsa` and each tuned parameter with `--spsavalue` (specifying `name`, `default`, `min`, `max`, and `step`).

> **Guide:** [SPSA.md](SPSA.md) explains which parameters can be tuned at all, how to choose them and their step sizes, and how to set a run up in a settings file.

### Example

```bash
--spsa iterations=20000 gamesperpair=8 learningrate=0.002 \
  --spsavalue name="Contempt" default=0 min=-50 max=50 step=10 \
  --spsavalue name="KingSafety" default=100 min=50 max=300 step=25
```

---

## 🎯 CLOP — Confident Local Optimization

CLOP is a local black-box optimizer for noisy outcomes. It tunes UCI engine options by repeatedly fitting a weighted quadratic logistic model, sampling new candidate parameter vectors from the resulting local distribution, and evaluating them in self-play.

Define the optimizer with `--clop` and each tuned parameter with `--clopvalue` (specifying `name`, `min`, `max`). Choose min/max so the search space contains only meaningful test values, with any known baseline value approximately centered in the range.

### Example

```bash
--clop samples=150 gamespersample=12 h=3.0 \
  --clopvalue name="Contempt" min=-50 max=50 \
  --clopvalue name="KingSafety" min=50 max=300
```

---

## 🧾 Tournament and SPRT Result Files

Tournament and SPRT result files serve a dual purpose: they store game results **and** the complete configuration used for the run. This makes them self-contained — when you specify an existing result file, Qapla Engine Tester reads all settings from it and continues the tournament or SPRT test exactly as originally configured.

Result files are available for both **tournaments** (`--tournament file=...`) and **SPRT** (`--sprt file=...`).

### How It Works

- When a result file is specified and already exists, Qapla Engine Tester loads **all settings** from it automatically.
- Settings from a result file take **absolute precedence** — higher priority than CLI arguments, settings files, or engines files. This ensures consistency when resuming a run.
- Already completed games are skipped; only remaining games are played.
- Result files are periodically saved during the run (configurable via `saveintervalS`).

### Use Cases

- **Resume an interrupted run** — Simply pass the result file again. All configuration and progress is restored automatically.
- **Continue with more games** — Increase rounds or games in the result file configuration, and Qapla Engine Tester will play only the new pairings.

> **Tip**: You can set up your entire tournament or SPRT configuration via CLI or settings file for the initial run. On subsequent runs, pass only the result file — no other configuration is needed.

---

## 🧪 Engine Testing Suite

The `--test` mode simulates a variety of real-world and edge-case conditions to validate **UCI protocol compliance**, robustness, and time behavior of an engine. It is particularly useful for engine developers who want to verify correct responses to time controls, commands, and abnormal inputs.

### Covered Areas

- Engine startup and shutdown behavior
- UCI option handling and crash resistance
- Time control and `movetime` compliance
- Infinite analysis and pondering stability
- Engine-vs-engine simulation with correctness scoring
- Detection of crashes, hangs, or protocol violations
- Detailed **PASS/FAIL** reporting per test case

Individual test modules can be selectively enabled or disabled (e.g. `noponder=true`, `noepd=true`).

### Example

```bash
--test numgames=40 nooption=true nostop=true
```

### Report Example

```
[Important]  
FAIL Computing a move returns a legal move         (1 failed)  
PASS Engine starts and stops fast and without problems  

[Misbehavior]  
PASS No movetime overrun  
PASS Infinite compute move must not exit on its own  

[Notes]  
FAIL No movetime underrun                         (5 failed)  
PASS Simple EPD tests, expected moves found  
```


---

## 📡 NPS Stability System Test

The `--systemtest` mode evaluates how stable a platform allocates computation time to engines when running multiple games in parallel. This helps determine the optimal number of concurrent games for tournament play on a given system.

### How It Works

A single engine configuration replays identical games at increasing concurrency levels. At each step, per-move NPS (nodes per second) is measured and the standard deviation is computed. Higher NPS variation indicates that the platform is less stable under load — for example due to CPU overload, performance/efficiency core scheduling, or hyperthreading contention.

### What It Answers

- How many parallel games can run on this platform without distorting tournament results?
- Is it better to use only physical cores or also virtual CPUs (hyperthreading)?
- At what concurrency level does NPS stability degrade significantly?

---

## 🌳 Perft — Move Generator Node Count Test

The `--perft` mode runs a classic perft (**per**formance **t**est): it counts the number of leaf nodes reached by exhaustively playing out all legal move sequences from a position to a fixed depth. This is the standard way to verify that a move generator is correct (no missing or illegal moves) and to benchmark its raw speed. Perft does not require any engine — it only uses Qapla Engine Tester's own, fully legal move generator.

### How It Works

Starting from the given position, all legal root moves are generated once and distributed across worker threads (bounded by `--concurrency`, and never more threads than there are root moves). Each thread walks its assigned root moves independently — doing/undoing moves and recursing — and the totals are summed at the end. Since the last ply only needs a move *count* rather than a move *list*, moves are not played out at the final ply.

### Options

- `position`: `startpos` (default) or any FEN string
- `depth`: search depth in plies (default `1`)
- `divide`: print a per-root-move node count breakdown (default `true`)
- `showfen`: also print the resulting FEN after each root move in the divide output (default `true`)

### Example

```bash
--concurrency=8 --perft position="r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -" depth=4
```

### Example Output

```
Move       | Nodes            | Fen after move
-----------+------------------+------------------------------------------------------------------------
a2a3       | 106743           | r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/P1N2Q1p/1PPBBPPP/R3K2R b KQkq - 0 1
b2b3       | 133233           | r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/1PN2Q1p/P1PBBPPP/R3K2R b KQkq - 0 1
...
perft finished: depth=4 nodes=4085603 time=00:00.138 nps=29605819
```

The final line reports the total node count, elapsed time, and nodes per second (NPS).

---

## 📡 MCP Server Mode

When started with `--mcp`, Qapla Engine Tester runs as a Model Context Protocol (MCP) server, exposing its test and tournament features as AI-callable tools. This enables integration with LLM-based workflows for automated engine testing.

---

## Example Combined Run

```bash
./qapla-engine-tester --concurrency=16 --engine cmd="myengine.exe" --epd file="endgames.epd" maxtime=60 seenplies=3 --test
```

This will:
1. Run an EPD analysis for the engine
2. Then perform the test suite


---

## 🛠️ Platform and Installation

- **Operating Systems**: Windows and Linux (macOS likely works, but requires manual compilation)
- **Language**: C++ (C++20)
- **Build Systems**: Visual Studio project and `CMakeLists.txt` included
- **Prebuilt Binaries**: Available on [GitHub Releases](https://github.com/Mangar2/qapla-engine-tester/releases)


---

## Third-Party Code

### fastchess SPRT Implementation

The SPRT (Sequential Probability Ratio Test) calculations in this project use algorithms from the [fastchess](https://github.com/Disservin/fastchess) project by Disservin.  
**License:** MIT

The fastchess SPRT implementation provides optimized Maximum Likelihood Estimation for statistical engine testing.

---

## Limitations

— Does not support Chess960


---

## Feedback

Use GitHub Issues to report bugs or feature requests:  
👉 [https://github.com/Mangar2/qapla-engine-tester/issues](https://github.com/Mangar2/qapla-engine-tester/issues)

