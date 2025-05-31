# Qapla Engine Tester

**Version**: Prerelease 0.2.0  
**Author**: Volker Boehm  
**Repository**: [https://github.com/Mangar2/qapla-engine-tester](https://github.com/Mangar2/qapla-engine-tester)

Qapla Engine Tester is a command-line tool for analyzing and testing UCI-compatible chess engines. It provides two independent core features:

1. **Automated EPD-based position analysis across multiple engines in parallel**
2. **Stress and compliance testing for UCI engine behavior**

Both features are fully configurable and optimized for multi-core systems.

---

## 🔁 General Options

| Option            | Description                                                   | Required | Default |
|-------------------|---------------------------------------------------------------|----------|---------|
| `--concurrency`   | Maximum number of engines running in parallel                 | Yes      | `10`    |
| `--enginesfile`   | Path to an INI file with engine definitions                   | No       | —       |
| `--enginelog`     | Enables engine communication logging                          | No       | `false` |
| `--logpath`       | Output directory for log files                                | No       | `.`     |

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

## ⚙️ `--engine` Group — Define Engine Configuration via CLI

Alternative to `--enginesfile` (you can use both if you like). Can be used multiple times for different engines or the same engine with different options.

| Sub-option             | Description                             | Required | Default  |
|------------------------|-----------------------------------------|----------|----------|
| `name`                 | Engine name                             | Yes*     | —        |
| `cmd`                  | Path to engine executable               | Yes      | —        |
| `dir`                  | Working directory                       | No       | `.`      |
| `proto`                | Protocol: `uci` or `xboard`             | No       | `uci`    |
| `option.[name]`        | Set engine-specific UCI option          | No       | —        |

**Example:**
```bash
--engine name="MyEngine" cmd="engine.exe" proto=uci option.Hash=128 option.Threads=4
```

Note: `name` is required to distinguish multiple engines when defined via CLI.


## EPD Position Analysis (New in 0.2.0)

**Qapla Engine Tester supports fully parallel EPD-based analysis across multiple engines**, using all CPU cores efficiently. It reads standard `.epd` files with `bm` (best move) tags and compares each engine's move suggestion to the expected move. Results are shown in a **side-by-side tabular format**, making differences instantly visible.

> Example use case: Run 16 engines concurrently to analyze 100 endgame positions, compare their correctness and speed.

### Features

- Fully parallel analysis on multi-core systems
- Supports arbitrary number of engines from `.ini` config
- Time-limited calculation per move
- Optional early stopping if correct move is found
- CLI output **and** structured `.log` file per run
- Reproducible, clear comparison between engines

### Example Output

(Speelman_EP_1) | 00.127, D: 15, M: d4d5 | 00.399, D: 12, M: d4d5 | 00.037, D: 10, M: d4d5 | 00.084, D: 11, M: d4d5 | BM: Kd5  
(Speelman_EP_2) | 01.592, D: 26, M: a5b6 | 00.001, D:  2, M: a5b6 | 00.016, D:  1, M: a5b6 | 00.001, D:  5, M: a5b6 | BM: Kb6

Each row shows:
- Time to best move
- Reported depth
- Suggested move
- Expected `bm` move (last column)

A `.log` file like `epd-report-YYYY-MM-DD_HH-MM-SS.log` is automatically written.

### Command-line Parameters

| Sub-option             | Description                                                                 | Required | Default                      |
|------------------------|-----------------------------------------------------------------------------|----------|------------------------------|
| `file`                 | Path to the EPD file                                                        | Yes      | `"speelman Endgame.epd"`     |
| `maxtime`              | Maximum time (seconds) per move per engine                                  | No       | `20`                         |
| `mintime`              | Minimum time (seconds) before early stop on correct move                    | No       | `2`                          |
| `seenplies`            | Required number of plies for correct move to trigger early stop (`-1` = off)| No       | `-1`                         |

**Example:**
```bash
--epd file="endgames.epd" maxtime=30 seenplies=3
```

---

## 2. Engine Testing Suite

This feature simulates engine behavior in stressful or error-prone conditions. It checks protocol compliance, time handling, memory behavior, and more.

### Test Modules

- Startup / shutdown reliability
- UCI option handling
- Time control and timeouts
- Infinite mode behavior
- Engine-vs-engine matches
- Crash, hang and recovery handling
- Detailed result report with PASS/FAIL per feature

### Test Levels

| Level  | Description                                    |
|--------|------------------------------------------------|
| 0      | All tests                                      |
| 1      | Safe/basic tests only                          |
| 2      | Destructive: invalid paths, empty strings, etc.|

### Sub-Options

| Sub-option         | Description                                               | Required | Default |
|--------------------|-----------------------------------------------------------|----------|---------|
| `numgames`         | Number of engine-vs-engine test games                     | No       | `20`    |
| `level`            | Test level: `0=all`, `1=safe only`, `2=destructive`       | No       | `0`     |

**Example:**
```bash
--test numgames=40 level=2
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
./qapla-engine-tester --concurrency=16 --enginelog=true --enginesfile="engines.ini" --epd file="endgames.epd" maxtime=60 seenplies=3 --test
```

This will:
1. Run an EPD analysis for all engines
2. Then perform the full test suite

---

## Platform and Installation

- OS: **Windows only** (Linux/macOS not yet supported)
- Language: **C++**
- Build system: Visual Studio project included
- Prebuilt binary available in [GitHub Releases](https://github.com/Mangar2/qapla-engine-tester/releases)

---

## Limitations

- Only UCI protocol is supported
- No GUI, command-line only

---

## Feedback

Use GitHub Issues to report bugs or feature requests:  
👉 [https://github.com/Mangar2/qapla-engine-tester/issues](https://github.com/Mangar2/qapla-engine-tester/issues)

