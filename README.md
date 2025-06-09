# Qapla Engine Tester

**Version**: Prerelease 0.3.0  
**Author**: Volker Böhm  
**Repository**: [https://github.com/Mangar2/qapla-engine-tester](https://github.com/Mangar2/qapla-engine-tester)

Qapla Engine Tester is a command-line tool for analyzing and testing UCI-compatible chess engines. It provides the following core features:

- **EPD-based position analysis** across multiple engines in parallel
- **SPRT testing** to statistically compare engine strength via engine-vs-engine matches 
- **Stress and compliance testing** for UCI engine behavior
- **Batch mode** support with different return codes by outcome

All features are fully configurable and optimized for multi-core systems.

---

## 🔁 General Options

These options configure the overall behavior of the tester and apply across all test types.

- **`--concurrency`** (Required, Default: `10`)  
  Defines the maximum number of engines running in parallel. This controls how many games or analysis tasks are executed at the same time. Use this to take advantage of multi-core systems.

- **`--enginesfile`** (Optional)  
  Path to an INI file that defines reusable engine configurations. This file serves as a database of named engine setups (e.g. executable path, UCI options, etc.). It does not select or activate any engines by itself — engines must always be specified explicitly via `--engine`. The tester will match engine names against this file if `conf=name` is used in a `--engine` definition.

- **`--enginelog`** (Optional, Default: `false`)  
  Enables detailed logging of the UCI protocol communication between the tester and each engine. Useful for debugging engine behavior or verifying protocol compliance.

- **`--logpath`** (Optional, Default: `.`)  
  Sets the output directory for log files, including engine logs and test summaries. The directory must exist beforehand. If not specified, logs are written to the current working directory.

- **`--noinfo`** (Optional, Default: `false`)  
  Ignores all `info` lines sent by engines. This significantly reduces overhead during very short time control games (e.g. `2s+10ms`) and improves performance in bulk testing scenarios.

- **`--tc`** (Optional)  
  Defines the time control for matches in the format `moves/time+inc` (e.g. `40/60+0.5` for 40 moves in 60 seconds plus 0.5 seconds increment). Use `inf` for infinite time (e.g. during analysis). Required for all engine-vs-engine game modes.

## 🔚 Return Codes for Batch Processing

All test modes now return structured numeric exit codes to support automated evaluation and integration in batch processing scripts or CI pipelines.

Return codes are prioritized — if multiple situations occur, the lowest relevant non-zero code (excluding `0`) is returned. For example: a parameter error (`2`) takes precedence over an engine failure (`10`) or a missed target (`13`).

### Return Code Overview

| Code | Name                 | Meaning                                                                 |
|------|----------------------|-------------------------------------------------------------------------|
| 0    | `NoError`            | Everything ran correctly; test completed as expected                    |
| 1    | `GeneralError`       | Unexpected program error (e.g. crash, unhandled exception)              |
| 2    | `InvalidParameters`  | Invalid or missing CLI parameter                                        |
| 10   | `EngineError`        | Engine crashed, could not start, or returned illegal moves              |
| 11   | `EngineMissbehaviour`| Engine hung, ignored protocol, or failed to follow commands             |
| 12   | `EngineNote`         | Test completed, but non-critical engine issues occurred (only in `--test`) |
| 13   | `MissedTarget`       | EPD target success threshold was not reached (`--epd`)                  |
| 14   | `H1Accepted`         | SPRT result: H₁ (stronger engine) accepted (`--sprt`)                   |
| 15   | `H0Accepted`         | SPRT result: H₀ (no significant difference) accepted (`--sprt`)         |
| 16   | `UndefinedResult`    | SPRT result could not be decided within maxGames (`--sprt`)             |

### Prioritization Rules

- Codes `1`, `2`, `10`, `11`, `12` always take priority over `13`–`16`
- Code `0` is only returned if **no issues** occurred
- Codes `13`, `14`, `15`, and `16` are only used if **no engine errors or CLI issues** occurred
- Among `13`–`16`, only one is returned depending on outcome

### Examples

- An SPRT test ends with undecided result → return `16`
- An EPD run fails to reach expected correctness rate → return `13`
- An engine crashes during an SPRT test → return `10`, not `14`–`16`
- A CLI parameter is missing → return `2`, regardless of test type

Use these codes in automation scripts to check for test outcomes or failure causes.

## Engine `.ini` Configuration (--enginesfile)

[Spike1.4]  
protocol=uci  
executablePath=C:\Chess\cutechess-cli\qapla0.3\Spike1.4.exe  
Hash=128  

[Qapla0.3.2bb]  
protocol=uci  
executablePath=C:\Chess\delivery\Qapla0.3.2\Qapla0.3.2-win-x86.exe  
workingDirectory=.  
Hash=128  
qaplaBitbasePath=C:\Chess\bitbases\lz4  
qaplaBitbaseCache=512  

- Required: section name, `executablePath`  
- Optional: `protocol`, `workingDirectory`, and any `option.*` lines

## ⚙️ `--engine` Group — Define Engine Configuration via CLI

Defines an engine to be used in a test. This option can be specified multiple times to configure multiple engines or multiple variants of the same engine. Engine definitions can be given inline or refer to named entries from the configuration file via `conf`.

You may combine this with `--enginesfile`, but note: **all engines must always be explicitly listed using `--engine`**, even if defined in the configuration file.

### Option Precedence (Highest Wins)

When multiple sources define the same UCI option for an engine, the following priority rules apply (from highest to lowest):

1. **Inline via `--engine`**  
   Highest priority. Values specified directly in `--engine` override all others.
2. **Via `--each`**  
   Shared options for all engines. Overridden by values in individual `--engine` definitions.
3. **From `--enginesfile` via `conf=...`**  
   Base configuration loaded from the file. Can be overridden by `--each` and `--engine`.

### Sub-options

- **`conf`** (Optional)  
  Refers to the name of an engine defined in the configuration file (`--enginesfile`). If set, all fields from that entry will be used unless overridden by CLI values.

- **`name`** (`cmd` is used as name, if neither `conf` nor `name` is provided)  
  Logical name of the engine used internally in logs and result output. Required if no `conf` is specified.

- **`cmd`** (Required if `conf` is not used)  
  Path to the engine executable.

- **`dir`** (Optional, Default: `.`)  
  Sets the working directory for the engine process. Relative paths used by the engine (e.g. to load books, NNUE files, or config data) are resolved from this directory. This does not define the path to the engine itself — use `cmd` for that.

- **`proto`** (Optional, Default: `uci`)  
  Engine protocol: only `uci` or ... `xboard` is not supported yet.

- **`option.[name]`** (Optional)  
  Sets a UCI option for the engine. Use multiple entries to set multiple options.

### Examples

Basic engine definition (fully inline):

```bash
--engine name="MyEngine" cmd="stockfish.exe" proto=uci option.Hash=128 option.Threads=4
--enginesfile=engines.ini --engine conf=sf option.Threads=2
```

## ♻️ `--each` Group — Shared Engine Options

Defines default values that apply to **all** engines unless overridden in their respective `--engine` definitions. This is useful for setting common UCI options or protocols without repeating them.

### Sub-options

- **`dir`** (Optional, Default: `.`)  
  Sets the working directory for all engines. Can be overridden individually in each `--engine` definition.

- **`proto`** (Optional, Default: `uci`)  
  Defines the protocol used by all engines (`uci` or not yet implemented - `xboard`). Can be overridden per engine.

- **`option.[name]`** (Optional)  
  Defines shared UCI options applied to all engines (e.g. `option.Threads=2`). These can also be overridden in individual `--engine` definitions.

### Example

```bash
--each option.Threads=2 proto=uci
--engine name=engineA cmd=./engineA
--engine name=engineB cmd=./engineB option.Threads=4
```

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

### `--epd` Group — Analysis Configuration

Defines how the EPD testset should be executed. One testset can be applied to all engines in parallel.

- **`file`** (Required)  
  Path to the `.epd` file to analyze. The file must exist and contain `bm` tags for validation.

- **`maxtime`** (Optional, Default: `20`)  
  Maximum time (in seconds) each engine is allowed to think per position.

- **`mintime`** (Optional, Default: `2`)  
  Minimum amount of time (in seconds) before the engine is allowed to stop after finding a correct move.

- **`seenplies`** (Optional, Default: `-1`)  
  The expected move must be visible in the principal variation for at least this many plies before early stopping is triggered. Use `-1` to disable early stopping of the analysis.

- **`minsuccess`** (Optional, Default: `0`)  
  Minimum percentage of correct best moves required. Can be used as a pass/fail threshold in batch testing.

### Example

```bash
--epd file="endgames.epd" maxtime=30 seenplies=3
```

## 📤 `--pgnoutput` Group — PGN Output Settings

Defines how game results should be saved in PGN (Portable Game Notation) format. This includes file handling, metadata selection, and optional engine annotations such as depth, eval, and PV.

This group is used for all game-based tests (e.g. `--sprt`, `--roundrobin`) to specify where and how the PGN should be written.

### Sub-options

- **`file`** (Required)  
  Path to the PGN file where all games will be saved. The file will be created if it doesn't exist.

- **`append`** (Optional, Default: `true`)  
  If enabled, new games will be appended to the existing PGN file. If disabled, the file is overwritten at the start of each run.

- **`fi`** (Optional, Default: `true`)  
  Save only games that were finished (i.e. not crashed or aborted). If disabled, all games are written regardless of status.

- **`min`** (Optional, Default: `false`)  
  If enabled, saves a minimal PGN with only essential headers and moves — omits metadata and annotations.

- **`clock`** (Optional, Default: `true`)  
  Include time remaining for each player after each move (if available from engine output).

- **`eval`** (Optional, Default: `true`)  
  Include the engine's evaluation score in PGN comments for each move.

- **`depth`** (Optional, Default: `true`)  
  Include the search depth reached when the move was selected.

- **`pv`** (Optional, Default: `false`)  
  Include the full principal variation (PV) in PGN comments. Useful for debugging or engine analysis.

### Example

```bash
--pgnoutput file="games.pgn" append=true fi=true eval=true pv=true
```

## ♟️ `--openings` Group — Opening Selection Settings

Controls how opening positions are assigned to games. Required for all game-based test types such as `--sprt` or `--roundrobin`. You can use `.epd`, `.pgn`, or raw FEN files as input and configure selection order, plies to play, and how openings are rotated.

### Sub-options

- **`file`** (Required)  
  Path to a file containing the opening positions. The format can be `.epd`, `.pgn`, or a raw text file with FEN strings (one per line).

- **`format`** (Optional, Default: `epd`)  
  Format of the opening file. Supported values:  
  - `epd` — Standard `.epd` file with optional tags  
  - `pgn` — Games from which the last position is extracted  (not yet implemented)
  - `raw` — Plain text FEN strings (one per line)

- **`order`** (Optional, Default: `sequential`)  
  Determines how positions are picked from the list:  
  - `sequential` — Use positions in order  
  - `random` — Shuffle the order

- **`plies`** (Optional, Default: `0`)  
  Maximum number of plies to play from the opening before engines take over.  
  `0` means the engine starts immediately from the loaded position.

- **`start`** (Optional, Default: `1`)  
  Index of the first position to use (1-based). Useful for splitting test segments.

- **`policy`** (Optional, Default: `default`)  
  Defines how opening positions are rotated across games:  
  - `default` — Uses the same opening repeatedly until all engines played both sides  
  - `encounter` — Ensures every engine pair sees every opening once  
  - `round` — Picks a new opening every round, rotating through the list

### Example

```bash
--openings file="openings.epd" format=epd order=random plies=8 policy=round
```

## 📊 `--sprt` Group — Sequential Probability Ratio Test (SPRT)

Enables a formal statistical strength comparison between two engines using the Sequential Probability Ratio Test (SPRT). This test continuously evaluates win/draw/loss results and stops early when one of the hypotheses (H₀ or H₁) is statistically confirmed, or a maximum number of games is reached.

Additionally, a **Monte Carlo simulation mode** is available to help understand how reliable a given SPRT configuration is under different Elo differences.

### Sub-options

- **`elolower`** (Optional, Default: `0`)  
  Defines the **lower bound** of the Elo interval for the alternative hypothesis (H₁).  
  → H₁ is accepted if Engine 1 appears stronger than Engine 2 by at least this amount.

- **`eloupper`** (Optional, Default: `10`)  
  Defines the **upper bound** of the Elo interval for the null hypothesis (H₀).  
  → H₀ is accepted if Engine 1 does **not** appear stronger than this threshold.

- **`alpha`** (Optional, Default: `0.05`)  
  Probability of accepting H₁ when H₀ is actually true (Type I error).

- **`beta`** (Optional, Default: `0.05`)  
  Probability of accepting H₀ when H₁ is actually true (Type II error).

- **`maxgames`** (Optional, Default: `0` = unlimited)  
  Defines a hard cap on the number of games. If neither hypothesis is confirmed by this point, the result is "No Decision".

- **`montecarlo`** (Optional, Default: `false`)  
  Runs a Monte Carlo simulation **instead of a real SPRT test**.  
  This mode simulates 1000 virtual SPRT runs for each Elo difference in the range −25 to +25 (in 5 Elo steps), using the given `alpha`, `beta`, and `maxgames` settings.

  Output shows how often H₀ or H₁ would be accepted at each Elo level — giving you insight into how "sensitive" or "conclusive" the test configuration is.

### Monte Carlo Output Example
Running SPRT Monte carlo simulation:  | Elo range: [0, 10] | alpha: 0.05, beta: 0.02 | maxGames: 3000
Simulated elo difference:    -25  No Decisions:    0.6%  H0 Accepted:   99.4%  H1 Accepted:    0.0%  Average Games: 1400.9
Simulated elo difference:    -20  No Decisions:    2.8%  H0 Accepted:   97.2%  H1 Accepted:    0.0%  Average Games: 1622.2
Simulated elo difference:    -15  No Decisions:   11.6%  H0 Accepted:   88.4%  H1 Accepted:    0.0%  Average Games: 1873.2
Simulated elo difference:    -10  No Decisions:   28.4%  H0 Accepted:   71.4%  H1 Accepted:    0.2%  Average Games: 2233.1
Simulated elo difference:     -5  No Decisions:   48.6%  H0 Accepted:   50.5%  H1 Accepted:    0.9%  Average Games: 2534.1
Simulated elo difference:      0  No Decisions:   68.8%  H0 Accepted:   28.7%  H1 Accepted:    2.5%  Average Games: 2738.1
Simulated elo difference:      5  No Decisions:   79.7%  H0 Accepted:   11.8%  H1 Accepted:    8.5%  Average Games: 2802.7
Simulated elo difference:     10  No Decisions:   78.6%  H0 Accepted:    3.6%  H1 Accepted:   17.8%  Average Games: 2781.3
Simulated elo difference:     15  No Decisions:   61.9%  H0 Accepted:    1.0%  H1 Accepted:   37.1%  Average Games: 2590.7
Simulated elo difference:     20  No Decisions:   36.0%  H0 Accepted:    0.0%  H1 Accepted:   64.0%  Average Games: 2239.2
Simulated elo difference:     25  No Decisions:   19.6%  H0 Accepted:    0.0%  H1 Accepted:   80.4%  Average Games: 1931.9
his helps you evaluate:
- How often your settings produce **conclusive** results
- Whether the `alpha`/`beta` values and `maxgames` are well chosen
- How the test reacts to various true Elo gaps between engines

### Example (SPRT Test)

```bash
--sprt elolower=0 eloupper=10 alpha=0.05 beta=0.05 maxgames=3000
```

## 🧪 Engine Testing Suite — Protocol & Stability Validation

This test mode simulates a variety of real-world and edge-case conditions to validate the **UCI protocol compliance**, robustness, and time behavior of an engine. It is particularly useful for engine developers who want to verify correct responses to time controls, commands, and abnormal inputs.

Tests can be optionally customized by enabling or disabling specific modules.

### Covered Test Modules

- Engine startup and shutdown behavior
- UCI option handling and crash resistance
- Time control and `movetime` compliance
- Infinite analysis mode behavior
- Engine-vs-engine simulation (with optional correctness scoring)
- Detection of crashes, hangs, or protocol violations
- Detailed **PASS/FAIL** reporting per test case

### Selective Test Control

The following flags allow you to skip individual tests. All are optional. By default, all tests run unless explicitly excluded.

- **`underrun`** (Optional, Default: `false`)  
  Check for engines that return a move **too quickly** under `movetime` constraints.

- **`timeusage`** (Optional, Default: `false`)  
  Verifies that engines **use the full available time** within tolerance in timed test games.

- **`noepd`** (Optional, Default: `false`)  
  Skip EPD-based correctness tests using simple best-move positions.

- **`nomemory`** (Optional, Default: `false`)  
  Skip test that checks whether the engine honors memory-related options (e.g. `Hash`).

- **`nooption`** (Optional, Default: `false`)  
  Skip crash-resistance tests related to invalid or malformed UCI options.

- **`nostop`** (Optional, Default: `false`)  
  Skip test that verifies whether an engine reacts properly to `stop` immediately after `go`.

- **`nowait`** (Optional, Default: `false`)  
  Skip test that checks whether engines **remain running** during `go infinite` and don´t terminate spontaneously.

- **`numgames`** (Optional, Default: `20`)  
  Number of engine-vs-engine test games to run. Focusses on time control and correctness scoring, not crash/protocol checks.

### Example

```bash
--test numgames=40 level=2 nooption=true nostop=true
```

### Report Example

[Important]  
FAIL Computing a move returns a legal move         (1 failed)  
PASS Engine starts and stops fast and without problems  

[Misbehavior]  
PASS No movetime overrun  
PASS Infinite compute move must not exit on its own  

[Notes]  
FAIL No movetime underrun                         (5 failed)  
PASS Simple EPD tests, expected moves found  

## Example Combined Run

```bash
./qapla-engine-tester --concurrency=16 --enginelog=true --engine cmd="myengine.exe" --epd file="endgames.epd" maxtime=60 seenplies=3 --test
```

This will:
1. Run an EPD analysis for the engine
2. Then perform the test suite

## Platform and Installation

- OS: **Windows only** (Linux/macOS not yet supported)
- Language: **C++**
- Build system: Visual Studio project included
- Prebuilt binary available in [GitHub Releases](https://github.com/Mangar2/qapla-engine-tester/releases)

## Limitations

- Only UCI protocol is supported, Winboard is not yet supported
- Start positions from pgn are not yet supported
- Pondering is not yet supported
- No GUI, command-line only

## Feedback

Use GitHub Issues to report bugs or feature requests:  
👉 [https://github.com/Mangar2/qapla-engine-tester/issues](https://github.com/Mangar2/qapla-engine-tester/issues)

