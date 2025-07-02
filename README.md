# Qapla Engine Tester

**Version**: Prerelease 0.4.0  
**Author**: Volker Böhm  
**Repository**: [https://github.com/Mangar2/qapla-engine-tester](https://github.com/Mangar2/qapla-engine-tester)

Qapla Engine Tester is a command-line tool for running tournaments and analyzing or testing UCI-compatible chess engines. It provides the following core features:

- **Tournament play** with support for Gauntlet and Round-Robin formats  
- **SPRT testing** to statistically compare engine strength via engine-vs-engine matches  
- **Opening book support** for tournaments and tests via PGN, EPD, or raw formats  
- **Pondering** support and **fully parallel gameplay** across any number of games  
- **Interactive mode** to change concurrency, inspect status, or stop runs gracefully  
- **Resumeable tournaments** via result files for interrupted runs  
- **Flexible configuration** entirely via command-line, settings file, or both  
- **EPD-based position analysis** across multiple engines in parallel  
- **Stress and compliance testing** for UCI engine behavior  
- **Batch mode** support with different return codes by outcome

All features are fully configurable and optimized for multi-core systems.

## 📚 Table of Contents

- [Qapla Engine Tester](#qapla-engine-tester)
- [Comparison with cutechess-cli](#comparison-with-cutechess-cli)
- [🔁 General Options](#-general-options)
- [🔚 Return Codes for Batch Processing](#-return-codes-for-batch-processing)
- [Engine `.ini` Configuration (--enginesfile)](#engine-ini-configuration---enginesfile)
- [🗂️ Settings File Support (`--settingsfile`)](#️-settings-file-support---settingsfile)
- [💬 Interactive Mode](#-interactive-mode)
- [⚙️ `--engine` Group — Define Engine Configuration via CLI](#️---engine-group--define-engine-configuration-via-cli)
- [♻️ `--each` Group — Shared Engine Options](#️---each-group--shared-engine-options)
- [📄 EPD Position Analysis](#-epd-position-analysis)
- [📤 `--pgnoutput` Group — PGN Output Settings](#-pgnoutput-group--pgn-output-settings)
- [♟️ `--openings` Group — Opening Selection Settings](#️---openings-group--opening-selection-settings)
- [🏆 Tournament Mode](#-tournament-mode)
- [📊 `--sprt` Group — Sequential Probability Ratio Test (SPRT)](#-sprt-group--sequential-probability-ratio-test-sprt)
- [🧾 Tournament Result Files](#-tournament-result-files)
- [🧪 Engine Testing Suite — Protocol & Stability Validation](#-engine-testing-suite--protocol--stability-validation)
- [Example Combined Run](#example-combined-run)
- [Platform and Installation](#platform-and-installation)
- [Limitations](#limitations)
- [Feedback](#feedback)


---

## Comparison with cutechess-cli

Qapla Engine Tester supports UCI engines playing standard chess in **Gauntlet**, **Round-Robin**, and **SPRT** tournaments.  
Command-line options are largely **compatible** with `cutechess-cli`, but follow a more modern syntax (e.g., `--engine` instead of `-engine`).

Unlike `cutechess-cli`, Qapla Engine Tester does **not** support WinBoard engines, chess variants, or additional tournament types beyond those listed above.

### What I like most about Qapla Engine Tester compared to cutechess-cli

- Tournaments are **resumable and extendable** thanks to persistent result files
- A built-in **interactive mode** allows changing concurrency, viewing status, or terminating gracefully
- Full **configuration via file** is supported, not just via CLI options
- **Per-engine logging** can be enabled for detailed debugging or analysis
- Includes an **engine behavior report** after each run to help identify compliance or stability issues

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
  Ignores all `info` lines sent by engines. This significantly reduces overhead during very short time control games and improves performance in bulk testing scenarios.

- **`--settingsfile`** (Optional)  
  Path to a `.ini` file containing predefined settings. These can be used alongside or instead of command-line options.

- **`--interactive`** (Optional, Default: `false`)  
  Enables interactive mode to adjust concurrency, query status, or terminate the run gracefully.

---

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

---

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

---

## 🗂️ Settings File Support (`--settingsfile`)

Qapla Engine Tester allows all command-line options to be specified via a settings file in INI format. This enables clean, reusable configurations—especially useful for longer test or tournament definitions.

To use a settings file, pass the path via:

```bash
--settingsfile path/to/config.ini
```

### Example `config.ini` file:

```ini
enginesfile=C:\Development\qapla-engine-tester\test\engines.ini
logpath=log
enginelog=true
concurrency=10

[tournament]
type=gauntlet
resultfile=log/tournamet.tour
rounds=2
games=8
repeat=2

[engine]
conf=Qapla 0.3.2
gauntlet=true

[engine]
conf=Qapla 0.3.2
trace=all

[engine]
conf=Qapla 0.3.1

[each]
tc=10+0.02

[pgnoutput]
file=log/test.pgn

[openings]
order=random
file=C:\Development\qapla-engine-tester\test\book8ply.raw
format=raw
```

All CLI options are fully supported inside the file, including multiple engines and grouped sections. Command-line arguments override values from the settings file if both are present.

---

## 💬 Interactive Mode

When `--interactive` is enabled, Qapla Engine Tester enters a command-driven mode that allows you to monitor and control the run in real-time via standard input.

This mode is particularly useful during long tournaments or test runs where dynamic adjustments or early termination may be needed.

### Available Commands

- `quit` / `q`  
  Exit the program gracefully after all current games have finished.

- `info` / `?`  
  Show current engine/game state and overall progress.

- `concurrency` / `c`  
  Change the number of concurrently running games (e.g., `c 4`).

- `abort` / `a`  
  Immediately stop all current games and exit the run.

- `help` / `h`  
  Display the list of available commands.

---

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

These options configure a single engine instance. Each `--engine` on the command line accepts the following sub-options:

- **`conf`** (Optional)  
  Refers to the name of an engine defined in the configuration file (`--enginesfile`). If set, all fields from that entry will be used unless overridden by CLI values.

- **`name`** (Optional if `conf` is used)  
  Logical name of the engine used internally in logs and result output. If neither `conf` nor `name` is provided, the engine command (`cmd`) will be used as fallback.

- **`cmd`** (Required if `conf` is not used)  
  Path to the engine executable.

- **`dir`** (Optional, Default: `.`)  
  Working directory for the engine process. Relative paths (e.g. for NNUE or config files) are resolved from here.

- **`proto`** (Optional, Default: `uci`)  
  Engine protocol. Only `uci` is supported currently. `xboard` is reserved for future use.

- **`tc`** (Optional)  
  Time control in format `moves/time+inc` (e.g. `40/60+0.5`). Use `inf` for infinite time. Must be set per engine.

- **`ponder`** (Optional)  
  Enables pondering mode for the engine, if supported.

- **`gauntlet`** (Optional, Default: `false`)  
  Marks the engine as part of the gauntlet pool. Relevant for Gauntlet tournaments.

- **`trace`** (Optional)  
  Sets engine trace level: `none`, `command`, or `all`. Requires `--enginelog` to be active.

- **`option.[name]`** (Optional, repeatable)  
  Defines UCI options for the engine (e.g. `option.Threads=4`).


### Examples

Basic engine definition (fully inline):

```bash
--engine name="MyEngine" cmd="stockfish.exe" proto=uci option.Hash=128 option.Threads=4
--enginesfile=engines.ini --engine conf=sf option.Threads=2
```

---

## ♻️ `--each` Group — Shared Engine Options

Defines default values that apply to **all** engines unless overridden in their respective `--engine` definitions. This is useful for setting common UCI options or protocols without repeating them.

### Sub-options

- **`dir`** (Optional, Default: `.`)  
  Sets the working directory for all engines. Can be overridden individually in each `--engine` definition.

- **`proto`** (Optional, Default: `uci`)  
  Defines the protocol used by all engines (`uci`). `xboard` is not yet implemented. Can be overridden per engine.

- **`tc`** (Optional)  
  Sets a shared time control for all engines. Can be overridden per engine.

- **`ponder`** (Optional, Default: `false`)  
  Enables pondering mode globally for all engines, if supported.

- **`trace`** (Optional, Default: `command`)  
  Sets trace level globally: `none`, `command`, or `all`. Requires `--enginelog` to be enabled.

- **`option.[name]`** (Optional, repeatable)  
  Defines shared UCI options for all engines (e.g. `option.Threads=2`). These can be overridden in individual `--engine` definitions.

### Example

```bash
--each option.Threads=2 proto=uci
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

---

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

---

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

- **`plies`** (Optional, Default: `all`)  
  Maximum number of plies to play from the PGN opening before engines take over.  
  Accepts an integer or `all`.  
  - `all`: plays the full PGN sequence before engines begin.  
  - `0`: engines start immediately from the PGN start position (usually only meaningful if a FEN is provided).  
  Only applicable when using PGN input.

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

---

## 🏆 Tournament Mode

Qapla Engine Tester supports automated tournaments between multiple engines using **Gauntlet** or **Round-Robin** formats. Tournaments are fully configurable and can be resumed via result files.

To activate tournament mode, use the `[tournament]` section in a settings file or pass equivalent CLI options.

### Supported Parameters

- **`type`** (Required, Default: `gauntlet`)  
  Defines the tournament format. Options: `gauntlet`, `round-robin`.

- **`resultfile`** (Optional)  
  File path to save tournament results. Enables resume and extend functionality.

- **`append`** (Optional, Default: `false`)  
  Appends results to an existing file instead of overwriting it.

- **`event`** (Optional)  
  Custom event name to include in PGN output or logs.

- **`games`** (Optional, Default: `2`)  
  Number of games per engine pairing.

- **`rounds`** (Optional, Default: `1`)  
  Number of times to repeat the full pairing cycle.

- **`repeat`** (Optional, Default: `2`)  
  Number of consecutive games per opening (typically with color swap).

- **`noswap`** (Optional, Default: `false`)  
  Disables automatic color swapping after each game.

- **`ratinginterval`** (Optional, Default: `10`)  
  Print rating table every N games during the tournament.

---

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

---

## 🧾 Tournament Result Files

Qapla Engine Tester supports resumable and extendable tournaments by writing structured result files. These files record the outcome of each round and pairing, allowing partial runs to continue later without repeating finished games.

This mechanism works for **all tournament types**, including **Gauntlet**, **Round-Robin**, and **SPRT**.

> ⚠️ Result files do **not** define a tournament. You must still provide the full tournament configuration via CLI or settings file.

### How It Works

- If a `resultfile` is specified and already exists, it is automatically loaded at startup.
- The tester matches finished games from the result file with the current tournament configuration.
- Any game that matches an existing result (including engine settings) is **skipped**.

### Matching Logic

- Engine configurations must match **exactly**, including paths, protocols, and UCI options (e.g. `Hash=64` vs. `Hash=128` are treated as different).
- The rest of the tournament definition may vary (e.g. number of games, rounds, participating engines). Qapla uses all results that match current pairings.

This flexible matching allows you to:
- Extend tournaments with more games or rounds  
- Continue interrupted runs  
- Remove or add engines — existing valid pairings will still be counted

### Example Result File (Excerpt)

```ini
[Qapla 0.3.2 [gauntlet]]
protocol=uci
executablePath=C:\Chess\delivery\Qapla0.3.2\Qapla0.3.2-win-x86.exe
workingDirectory=.
tc=10.0+0.02
Hash=64

[Qapla 0.3.1]
protocol=uci
executablePath=C:\Chess\delivery\Qapla0.3.1\Qapla0.3.1-win-x86.exe
workingDirectory=.
tc=10.0+0.02
Hash=64

[round 1 engines Qapla 0.3.2 [gauntlet] vs Qapla 0.3.1]
games: ==1001=0
wincauses: checkmate:3
drawcauses: threefold repetition:2,50-move rule:1
losscauses: checkmate:2
```

---

## 🧪 Engine Testing Suite — Protocol & Stability Validation

This test mode simulates a variety of real-world and edge-case conditions to validate the **UCI protocol compliance**, robustness, and time behavior of an engine. It is particularly useful for engine developers who want to verify correct responses to time controls, commands, and abnormal inputs.

Tests can be optionally customized by enabling or disabling specific modules.

### Covered Test Modules

- Engine startup and shutdown behavior  
- UCI option handling and crash resistance  
- Time control and `movetime` compliance  
- Infinite analysis mode behavior  
- Pondering behavior and stability  
- Engine-vs-engine simulation (with optional correctness scoring)  
- Detection of crashes, hangs, or protocol violations  
- Detailed **PASS/FAIL** reporting per test case

### Selective Test Control

The following flags allow you to skip individual tests. All are optional. By default, all tests run unless explicitly excluded.

- **`underrun`** (Optional, Default: `false`)  
  Check for engines that return a move **too quickly** under `movetime` constraints.

- **`timeusage`** (Optional, Default: `false`)  
  Verifies that engines **use the full available time** within tolerance in timed test games.

- **`noponder`** (Optional, Default: `false`)  
  Skip test that verifies engine behavior during **pondering**.

- **`noepd`** (Optional, Default: `false`)  
  Skip EPD-based correctness tests using simple best-move positions.

- **`nomemory`** (Optional, Default: `false`)  
  Skip test that checks whether the engine honors memory-related options (e.g. `Hash`).

- **`nooption`** (Optional, Default: `false`)  
  Skip crash-resistance tests related to invalid or malformed UCI options.

- **`nostop`** (Optional, Default: `false`)  
  Skip test that verifies whether an engine reacts properly to `stop` immediately after `go`.

- **`nowait`** (Optional, Default: `false`)  
  Skip test that checks whether engines **remain running** during `go infinite` and don’t terminate spontaneously.

- **`numgames`** (Optional, Default: `20`)  
  Number of engine-vs-engine test games to run. Focuses on time control and correctness scoring, not crash/protocol checks.

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

---

## Example Combined Run

```bash
./qapla-engine-tester --concurrency=16 --enginelog=true --engine cmd="myengine.exe" --epd file="endgames.epd" maxtime=60 seenplies=3 --test
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

## Limitations

- Only UCI protocol is supported, Winboard is not yet supported
- No GUI, command-line only

---

## Feedback

Use GitHub Issues to report bugs or feature requests:  
👉 [https://github.com/Mangar2/qapla-engine-tester/issues](https://github.com/Mangar2/qapla-engine-tester/issues)

