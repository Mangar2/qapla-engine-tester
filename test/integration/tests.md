# Integration Tests Coverage

## Existing Tests

### engine-test (15 tests)

| Name | Description |
|---|---|
| engine-test-minimal | Minimal test with all optional sub-tests disabled; verifies startup/shutdown PASS |
| engine-test-noinit | Engine fails to initialize; expects exit 10, FAIL in report |
| engine-test-full-success | All sub-tests enabled with compliant engine; expects exit 0 |
| engine-test-nostop-fail | Negative test: stop command with Qapla 0.3.0; expects exit 10 |
| engine-test-nomemory-fail | Negative test: hash adjustment with Qapla 0.2.0; expects exit 12 |
| engine-test-nooption-fail | Negative test: UCI option handling with Qapla 0.3.1; expects exit 10 |
| engine-test-noanalyze-fail | Negative test: standard analysis with midnightv5; expects exit 10 |
| engine-test-nowait-fail | Negative test: infinite search with Counter-v3.3; expects exit 11 |
| engine-test-nogolimits-fail | Negative test: go limits with Qapla 0.2.0; expects exit 10 |
| engine-test-nofens-fail | Negative test: EP FEN positions with Qapla 0.3.1; expects exit 10 |
| engine-test-noepd-fail | Negative test: EPD bestmove with Spike 1.4; expects exit 12, engine log present |
| engine-test-nocompute-fail | Negative test: single compute game with diagnostic lossontime; expects exit 10 |
| engine-test-noponder-fail | Negative test: pondering with Qapla 0.2.0; expects exit 10 |
| engine-test-underrun-fail | Negative test: movetime underrun with Qapla 0.3.1; expects exit 12 |
| engine-test-timeusage-fail | Negative test: time usage in games with Stockfish; expects exit 12 |

### epd (4 tests)

| Name | Description |
|---|---|
| epd-basic-run | Single engine, minsuccess=100 on wmtest.epd; expects MissedTarget (13) |
| epd-short-time | Single engine, maxtime=1s, minsuccess=30; passes at low threshold |
| epd-long-time | Single engine, maxtime=10s, seenplies=2; expects higher success rate |
| epd-two-engines | Two engines, maxtime=2s, no success requirement; both engine names in report |

### logging (5 tests)

| Name | Description |
|---|---|
| logging-global-single-file | mode=one: exactly one engine-*.log for two engines |
| logging-per-engine-multiple-files | mode=each: exactly two engine-*.log files |
| logging-disabled-no-files | engine=false: zero engine-*.log files produced |
| logging-engine-trace-none | engine=true, per-engine trace=none: zero log files |
| logging-each-trace-none | engine=true, [each] trace=none: zero log files |

### mcp (17 tests)

| Name | Description |
|---|---|
| mcp-initialize | JSON-RPC initialize; response contains protocolVersion and server name |
| mcp-list-engines | tools/call manage_engines list; output includes Stockfish and Qapla 0.4.0 |
| mcp-engine-details | manage_engines details on Stockfish; exe path and protocol listed |
| mcp-engine-add | manage_engines add TestEngine; success confirmation |
| mcp-engine-copy-basic | manage_engines copy Stockfish → StockfishCopy; success confirmation |
| mcp-engine-update | Update Stockfish TC, then details; TC visible in output |
| mcp-engine-update-all | update_all with engine_option_Threads=2; Updated N engines message |
| mcp-engine-error-missing-name | details without engine_name; isError=true and hint in message |
| mcp-sprt-start | sprt tool with maxgames=1; isError=false, Tool finished message |
| mcp-sprt-missing-tc | sprt without TC; error in id=1 response, server still responds to id=2 |
| mcp-engine-delete | Add then delete engine; deleted successfully message |
| mcp-engine-copy-with-tc | Copy after TC update; copied engine carries TC in details |
| mcp-engine-copy-inline | Copy with simultaneous UCI option; option visible in details |
| mcp-engine-invalid-path | Add with non-existent path; isError=true, path error message |
| mcp-engine-update-all-custom | update_all on copied engine; Hash=256 visible in details |
| mcp-engine-update-all-after-sprt | update_all after SPRT use; TC change reflected in details |
| mcp-list-settings | list_settings returns Global Settings, openings, and epd sections |

### parameter (8 tests)

| Name | Description |
|---|---|
| parameter-concurrency-basic | concurrency=1 in ini; SPRT runs with maxgames reached (16) |
| parameter-missing-mandatory | Missing openings file in ini; expects InvalidParameters (2) |
| parameter-cmdline-override | CLI --openings overrides missing ini entry; SPRT runs (16) |
| parameter-sprt-file | Load SPRT file via --sprt file=; fileAppendOnly verifies no truncation |
| parameter-global-fail | Illegal global param in ini; expects InvalidParameters (2) |
| parameter-global-override | Illegal global param overridden by valid CLI value; SPRT runs (16) |
| parameter-group-fail | Illegal group param in ini; expects InvalidParameters (2) |
| parameter-group-override | Illegal group param overridden by CLI; SPRT runs (16) |

### sprt (7 tests)

| Name | Description |
|---|---|
| sprt-maxgames-reached | SPRT ends without decision (19 games); expects UndefinedResult (16) |
| sprt-basic-h0-accepted | SPRT between equal engines with low H1 threshold; expects H0Accepted (15) |
| sprt-basic-h1-accepted | SPRT with stronger vs weaker engine given low thresholds; expects H1Accepted (14) |
| sprt-with-ponder | SPRT with ponder=true in engine config; expects H1Accepted (14) |
| sprt-rapid-mode | rapid=true suppresses info lines; SPRT runs to maxgames (16) |
| sprt-continuation | Loads existing .qsprt file; resumes from saved game count |
| sprt-nonexisting-file | Writes new .qsprt file; file contains [each] and [round] sections |

### tournament (2 tests)

| Name | Description |
|---|---|
| tournament-basic | Gauntlet, 3 engines, 8 games/pairing; report and PGN file produced |
| tournament-nonexisting-file | Writes new .qtour file; file contains [each] and [round] sections |

**Total: 58 existing tests**

---

## Planned Tests

### epd

**epd-depth-fixed**
- **Why missing**: `depth=` is a new 0.5.0 feature replacing time-based search; no test verifies it is accepted and produces output
- **args**: `--settingsfile=test/integration/epd/test-epd.ini --epd depth=8 minsuccess=0`
- **log_path**: `test/integration/log/epd`
- **cleanup**: `test/integration/log/epd`
- **validators**:
  - `exitCode: 0`
  - `logFiles path="" pattern="epd-report*.log" count=1`

**epd-nodes-fixed**
- **Why missing**: `nodes=` is a new 0.5.0 feature; no test verifies node-limited analysis
- **args**: `--settingsfile=test/integration/epd/test-epd.ini --epd nodes=100000 minsuccess=0`
- **log_path**: `test/integration/log/epd`
- **cleanup**: `test/integration/log/epd`
- **validators**:
  - `exitCode: 0`
  - `logFiles path="" pattern="epd-report*.log" count=1`

### sprt

**sprt-montecarlo**
- **Why missing**: Monte Carlo simulation mode is documented with example output in README; no integration coverage
- **Note**: Pure simulation — no engines or openings file required; completes in under 5 seconds
- **args**: `--sprt montecarlo=true eloH0=0 eloH1=10 alpha=0.05 beta=0.05 maxgames=3000 --logging path=test/integration/log/sprt/montecarlo`
- **log_path**: `test/integration/log/sprt/montecarlo`
- **cleanup**: `test/integration/log/sprt/montecarlo`
- **validators**:
  - `exitCode: 0`
  - `stdout content="Running SPRT Monte carlo simulation" isRegex=false`
  - `stdout content="H0 Accepted" isRegex=false`

### tournament

**tournament-round-robin**
- **Why missing**: Only gauntlet format is tested; round-robin is fully documented
- **args**: `--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=2 repeat=2 rounds=1 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 trace=none --engine conf='Qapla 0.3.1' --engine conf='Qapla 0.3.2' --engine conf='Spike 1.4' --logging engine=false path=test/integration/log/tournament/roundrobin`
- **log_path**: `test/integration/log/tournament/roundrobin`
- **cleanup**: `test/integration/log/tournament/roundrobin`
- **validators**:
  - `exitCode: 0`
  - `logFiles path="" pattern="tournament-report-*.log" count=1`

### draw

**draw-adjudication**
- **Why missing**: `--draw` group is fully documented; no integration test exercises these parameters
- **Note**: `test=true` reports potential draws without stopping games; SPRT still reaches maxgames → exit 16
- **args**: `--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --draw movenumber=1 movecount=1 score=5000 test=true --logging path=test/integration/log/sprt/draw`
- **log_path**: `test/integration/log/sprt/draw`
- **cleanup**: `test/integration/log/sprt/draw`
- **validators**:
  - `exitCode: 16`

### resign

**resign-adjudication**
- **Why missing**: `--resign` group is fully documented; no integration test exercises these parameters
- **Note**: `test=true` reports potential resignations without stopping games; SPRT still reaches maxgames → exit 16
- **args**: `--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --resign movecount=3 score=500 twosided=false test=true --logging path=test/integration/log/sprt/resign`
- **log_path**: `test/integration/log/sprt/resign`
- **cleanup**: `test/integration/log/sprt/resign`
- **validators**:
  - `exitCode: 16`

### pgnoutput

**pgnoutput-append**
- **Why missing**: `append=true` is documented but all existing tests use `append=false`; the append path is never exercised
- **Note**: Requires a minimal source PGN at `test/integration/pgnoutput/pgnoutput-append.pgn` (one complete PGN game). `fileAppendOnly` verifies the original game is not overwritten
- **args**: `--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --pgnoutput file=test/integration/log/pgnoutput/games.pgn append=true --logging engine=false path=test/integration/log/pgnoutput`
- **log_path**: `test/integration/log/pgnoutput`
- **cleanup**: `test/integration/log/pgnoutput`
- **source_files**: `[{"source": "test/integration/pgnoutput/pgnoutput-append.pgn", "target": "test/integration/log/pgnoutput/games.pgn"}]`
- **validators**:
  - `exitCode: 16`
  - `fileAppendOnly path="test/integration/log/pgnoutput/games.pgn"`

### spsa

**spsa-basic**
- **Why missing**: SPSA is a new 0.5.0 core feature; completely uncovered by any integration test
- **Note**: iterations=2, gamesperpair=2 → 4 short games; the second engine is auto-created by the SPSA framework with perturbed Hash values; Hash is a standard UCI option on Qapla 0.4.0
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --spsa iterations=2 gamesperpair=2 activepairs=1 outcomeinterval=1 --spsavalue name=Hash default=16 min=8 max=32 step=4 --engine conf='Qapla 0.4.0' --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 --logging engine=false path=test/integration/log/spsa`
- **log_path**: `test/integration/log/spsa`
- **cleanup**: `test/integration/log/spsa`
- **validators**:
  - `exitCode: 0`

### systemtest

**systemtest-basic**
- **Why missing**: NPS stability system test is a new 0.5.0 core feature; completely uncovered
- **Note**: steptime=5, maxcores=2 → ~10 seconds total; verifies the feature runs without error and writes a report
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --systemtest maxcores=2 step=1 steptime=5 --engine conf='Qapla 0.4.0' --logging engine=false path=test/integration/log/systemtest`
- **log_path**: `test/integration/log/systemtest`
- **cleanup**: `test/integration/log/systemtest`
- **validators**:
  - `exitCode: 0`
  - `logFiles path="" pattern="systemtest-report-*.log" count=1`

### clop

**clop-basic**
- **Why missing**: CLOP optimization is a core feature with no integration coverage whatsoever
- **Note**: samples=3, gamespersample=2 → 6 games; warmupsamples=2 is within samples so warmup phases are minimal; Hash is tuned on the gauntlet engine
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --clop samples=3 gamespersample=2 warmupsamples=2 outcomeinterval=1 --clopvalue name=Hash min=8 max=32 --engine conf='Qapla 0.4.0' gauntlet=true --engine conf='Qapla 0.3.2' --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 --logging engine=false path=test/integration/log/clop`
- **log_path**: `test/integration/log/clop`
- **cleanup**: `test/integration/log/clop`
- **validators**:
  - `exitCode: 0`
