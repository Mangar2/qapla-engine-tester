# Integration Tests Coverage

All tests currently implemented, grouped by test module
(`test/integration/<module>/<module>_tests.py`). Runtimes are recorded per test in
`test_results.log` after each run.

## clop (1 test)

| Name | Description |
|---|---|
| clop-basic | CLOP with 3 samples, Hash tuned on the gauntlet engine; expects exit 0 |

## draw (1 test)

| Name | Description |
|---|---|
| draw-adjudication | `--draw` in test mode; parameters accepted, SPRT still reaches maxgames (16) |

## engineoptions (3 tests)

| Name | Description |
|---|---|
| engineoptions-each-option | `--each option.Hash=64` arrives at the engine as a `setoption` command |
| engineoptions-engine-overrides-each | Inline `--engine option.Hash=128` beats the shared `--each` value; both values in the log |
| engineoptions-restart-always | `restart=on` restarts between games and logs the reason |

## engine-test (15 tests)

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
| engine-test-timeusage-fail | Negative test: engine forfeits on time in self-play; expects exit 10 |

## epd (6 tests)

| Name | Description |
|---|---|
| epd-basic-run | Single engine, minsuccess=100 on wmtest.epd; expects MissedTarget (13) |
| epd-short-time | Single engine, maxtime=1s, minsuccess=30; passes at low threshold |
| epd-long-time | Single engine, maxtime=10s, seenplies=2; expects higher success rate |
| epd-two-engines | Two engines, maxtime=2s, no success requirement; both engine names in report |
| epd-depth-fixed | `depth=8`; verifies depth-limited search mode |
| epd-nodes-fixed | `nodes=100000`; verifies node-limited search mode |

## interactive (1 test)

| Name | Description |
|---|---|
| interactive-commands | `--interactive` accepts help, info, running, outcome, an unknown command and quit |

## logging (5 tests)

| Name | Description |
|---|---|
| logging-global-single-file | mode=one: exactly one engine-*.log for two engines |
| logging-per-engine-multiple-files | mode=each: exactly two engine-*.log files |
| logging-disabled-no-files | engine=false: zero engine-*.log files produced |
| logging-engine-trace-none | engine=true, per-engine trace=none: zero log files |
| logging-each-trace-none | engine=true, [each] trace=none: zero log files |

## mcp (21 tests)

| Name | Description |
|---|---|
| mcp-initialize | JSON-RPC initialize; response contains protocolVersion and server name |
| mcp-tools-list | tools/list publishes every registered tool including its input schema |
| mcp-tool-prefix | `--mcp prefix=qet` renames all tools; the prefixed name is callable |
| mcp-control-status | control/status reports current task, running games and job queue |
| mcp-epd-tool-report-resource | epd tool runs via MCP and publishes its report as a `qapla://reports/...` resource |
| mcp-list-engines | tools/call manage_engines list; output includes the configured engines |
| mcp-engine-details | manage_engines details; exe path and protocol listed |
| mcp-engine-add | manage_engines add TestEngine; success confirmation |
| mcp-engine-copy-basic | manage_engines copy; success confirmation |
| mcp-engine-update | Update TC, then details; TC visible in output |
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

## openings (5 tests)

| Name | Description |
|---|---|
| openings-pgn-book | PGN book with plies=8; games start from the replayed book moves |
| openings-epd-book-black-to-move | EPD book with Black to move; FEN tag correct and no loss on time (clock assignment) |
| openings-random-seeded | order=random with fixed seed and policy=encounter; not the sequential first opening |
| openings-start-index | start=100; the PGN starts from opening number 100 |
| openings-invalid-policy | Unknown policy rejected with InvalidParameters (2) before any engine starts |

## parameter (8 tests)

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

## perft (4 tests)

| Name | Description |
|---|---|
| perft-startpos-depth5 | Start position, depth 5; pins the reference node count 4865609 |
| perft-divide-showfen | divide + showfen; per-root-move counts and resulting FENs |
| perft-fen-position | Kiwipete FEN, depth 3; node count 97862 |
| perft-invalid-fen | Malformed position; expects InvalidParameters (2) |

## pgnoutput (3 tests)

| Name | Description |
|---|---|
| pgnoutput-append | append=true; original content preserved at the start of the file |
| pgnoutput-minimal-tags | min=true with all annotations off; no extended tags, no move comments |
| pgnoutput-full-annotations | eval/depth/clock/pv enabled; comments carry score, depth, time and PV |

## resign (1 test)

| Name | Description |
|---|---|
| resign-adjudication | `--resign` in test mode; parameters accepted, SPRT still reaches maxgames (16) |

## returncode (2 tests)

| Name | Description |
|---|---|
| returncode-sprt-survives-engine-failure | Engine fails its UCI handshake; the game is forfeited and SPRT still returns 16 |
| returncode-tournament-survives-engine-failure | Same failure in a tournament; the run still returns 0, the forfeit is in the report |

## sprt (14 tests)

| Name | Description |
|---|---|
| sprt-maxgames-reached | SPRT ends without decision; expects UndefinedResult (16) |
| sprt-basic-h0-accepted | SPRT between equal engines with low H1 threshold; expects H0Accepted (15) |
| sprt-basic-h1-accepted | Challenger wins every game against a forfeiting baseline; expects H1Accepted (14) |
| sprt-with-ponder | SPRT with ponder=true in engine config; expects H1Accepted (14) |
| sprt-rapid-mode | rapid=true suppresses info lines; SPRT runs to maxgames (16) |
| sprt-continuation | Loads existing .qsprt file; resumes from saved game count |
| sprt-continuation-configured-engines | CLI engines given; the file's engine sections are ignored, not merged |
| sprt-nonexisting-file | Writes new .qsprt file; file contains [each] and [round] sections |
| sprt-continuation-tightened-bounds | Resumes a decided SPRT with tighter alpha/beta; stored games are kept, play continues |
| sprt-file-uci-option-roundtrip | Writes a .qsprt with UCI options and resumes from it; the options must reach the engines unchanged |
| sprt-montecarlo | Monte Carlo simulation, default model; no engines required |
| sprt-montecarlo-logistic | Monte Carlo simulation with model=logistic |
| sprt-montecarlo-bayesian | Monte Carlo simulation with model=bayesian |
| sprt-pentanomial-false | Trinomial model; report header records `Pentanomial: false` |

## spsa (1 test)

| Name | Description |
|---|---|
| spsa-basic | SPSA with 2 iterations tuning Hash; expects exit 0 |

## systemtest (1 test)

| Name | Description |
|---|---|
| systemtest-basic | NPS stability test with 2 concurrency steps; report produced |

## tournament (9 tests)

| Name | Description |
|---|---|
| tournament-basic | Gauntlet, 3 engines, 8 games/pairing; report and PGN file produced |
| tournament-round-robin | Round-robin, 3 engines; all pairings complete |
| tournament-nonexisting-file | Writes new .qtour file; file contains [each] and [round] sections |
| tournament-continuation | Resumes an existing .qtour; round 1 skipped, round 2 played and saved |
| tournament-file-uci-option-roundtrip | Writes a .qtour with UCI options and resumes from it; the options must reach the engines unchanged |
| tournament-roundrobin-too-few-engines | Round-robin with one engine; expects InvalidParameters (2) |
| tournament-gauntlet-fallback | No gauntlet=true; the first engine plays all others, no round-robin pairings |
| tournament-noswap-event-rating | noswap/event/ratinginterval; colors fixed, event name in the PGN |
| tournament-fixed-depth | 100 games at `tc=depth:5`; every `go` carries `depth 5`, no clock is sent, no time forfeit |

## xboard (3 tests)

| Name | Description |
|---|---|
| xboard-sprt-two-engines | SPRT between two XBoard engines; exercises the WinBoard adapter |
| xboard-mixed-protocols | XBoard engine against UCI engine; translation across adapters |
| xboard-engine-test | Engine test suite (go limits, FEN positions) against an XBoard engine |

**Total: 103 tests**
