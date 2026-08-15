# Integration Test Plan — 30 Additional Test Cases

> **Status: implemented.** All 30 tests below are in the suite and green — §11 became two tests
> (SPRT and tournament) after the return-code question was decided, so the suite now holds 100 tests
> in total (see [test/integration/tests.md](integration/tests.md)). Details that turned out
> differently during implementation are noted inline below; open follow-ups are in
> [integration-test-questions.md](integration-test-questions.md).

Companion document to [test/integration/tests.md](integration/tests.md), which listed 58 tests when
this plan was written. This plan proposes **30 new tests** that deliberately cover *white spots* —
features and code paths that no existing test touches at all.

All command lines, output strings, node counts and exit codes quoted below were **verified against
`build/default/qapla-engine-tester`** on Linux while writing this plan; measured runtimes are given
per test.

---

## 1. Ground rules (preconditions for every new test)

These rules are binding for the implementation of all 30 tests.

1. **Short runtime is a requirement.** **Desired: ≤ 20 s** per test; **hard limit: 5 minutes** — a test
   that exceeds five minutes is not accepted, and anything above 20 s needs a reason. Runtime is a
   *parameter*, not a property of the feature — so parametrize it down:
   | Mode | Fast parametrization |
   |---|---|
   | SPRT | `maxgames=2…6`, `--each tc=0.2+0.01` (a 2-game SPRT takes ~2 s) |
   | Tournament | `games=2 rounds=1`, 2–3 engines, `tc=0.1+0.01` |
   | EPD | `depth=1` or `maxtime=1`; prefer a **short EPD fixture** (see rule 6) over the 100-position `wmtest.epd` |
   | Perft | `depth ≤ 5` (startpos depth 5 = 0.23 s) |
   | Monte Carlo SPRT | `maxgames ≤ 3000`, no engines needed at all |
   | SPSA/CLOP | `iterations`/`samples` ≤ 3, `gamesperpair`/`gamespersample` = 2 |
   | Pure error paths | no engine start at all → milliseconds |

   Runtime is measured by the runner itself: every test prints its runtime, the summary lists the
   three slowest tests plus the total, and `test/integration/test_results.log` records it per test
   (`<name> - PASSED (8.85s)`). Check a new test against the budget there instead of estimating.
2. **Prefer engine-free tests.** Perft, Monte Carlo SPRT and all parameter-error tests need no engine
   process and are therefore both the fastest and the most stable tests. Use them where possible.
3. **Prefer `--engine conf=…` over `cmd=…`.** `test/integration/engines/engines.ini` is materialized
   per OS by `materialize_platform_engines_files()`. Any *new* fixture that embeds a `cmd=` path must
   be provided as `<name>.linux.*` / `<name>.macos.*` / `<name>.windows.*` (same pattern as
   `test-sprt-file.<os>.qsprt`).
4. **The logging path must exist before the run.** `--logging path=…` fails with exit code 2 if the
   directory is missing; the framework creates `log_path` before starting the process, so
   `log_path` and the `--logging path=` value must be identical.
5. **Every test cleans up.** Set `cleanup` to the test's own log directory; never share a log
   directory between two tests that check file counts.
6. **New shared fixture:** `test/integration/epd/short.epd` — the first 5 lines of
   `test/epd/wmtest.epd`. Used by the EPD-related tests below to cut runtime from ~10 s to ~1 s.
7. **Validator vocabulary** (from `test_framework.py`): `exitCode`, `stdout`
   (`content`, `isRegex`), `logFiles` (`path`, `pattern`, `count`, `content`), `fileContent`
   (`path`, `content`, `isRegex`, `message`), `fileExists`, `fileAppendOnly`. A *negative* assertion
   is expressed as a regex with a negative lookahead, e.g.
   `"(?s)^(?:(?!time forfeit).)*$"` — this idiom is already used in `sprt-continuation-configured-engines`.
8. **Module layout:** the runner discovers `test/integration/<dir>/<dir>_tests.py`. New directories
   needed: `perft/`, `xboard/`, `openings/`, `engineoptions/`, `interactive/`, `returncode/`.
   The remaining tests extend existing modules (`pgnoutput/`, `tournament/`, `sprt/`, `mcp/`).

---

## 2. Coverage gaps this plan closes

| Area | Covered today | White spot closed here |
|---|---|---|
| Perft (new in 0.6.0) | nothing | 4 tests |
| WinBoard/XBoard protocol | nothing (all 58 tests are UCI) | 3 tests |
| Opening books | only `.raw` + `order=sequential` | PGN book, EPD book, `order=random`, `start=`, `policy=`, invalid policy — 5 tests |
| PGN annotations | only `append=true` | `min`, `clock`, `eval`, `depth`, `pv` — 2 tests |
| Tournament | gauntlet + new-file writing | resume, round-robin arity error, gauntlet fallback (0.6.0), `noswap`/`event`/`ratinginterval` — 4 tests |
| SPRT statistics | `model=normalized`, `pentanomial=true` | `model=logistic`, `model=bayesian`, `pentanomial=false` — 3 tests |
| Engine option plumbing | nothing | `--each option.*`, `--engine` precedence, `restart=on` — 3 tests |
| Interactive mode | nothing | 1 test |
| Return-code priority | nothing | 1 test |
| MCP | `initialize`, `manage_engines`, `sprt`, `list_settings` | `tools/list`, tool prefix, `control`, `epd` + `resources/list` — 4 tests |

---

## 3. Perft — `test/integration/perft/perft_tests.py` (new module)

Perft is the headline feature of 0.6.0 and has **zero** integration coverage. It needs no engine, so
these are the cheapest tests in the whole suite.

**perft-startpos-depth5**
- **Why missing**: `--perft` is completely untested; this pins the canonical node count, which is the
  only meaningful regression guard for the move generator.
- **args**: `--concurrency=4 --perft position=startpos depth=5 divide=false --logging path=test/integration/log/perft`
- **log_path / cleanup**: `test/integration/log/perft`
- **validators**: `exitCode: 0`; `stdout content="perft finished: depth=5 nodes=4865609"`
- **runtime**: 0.25 s (verified)

**perft-divide-showfen**
- **Why missing**: `divide` and `showfen` produce the per-root-move table; the table layout is never checked.
- **args**: `--concurrency=2 --perft position=startpos depth=2 divide=true showfen=true --logging path=test/integration/log/perft`
- **log_path / cleanup**: `test/integration/log/perft`
- **validators**: `exitCode: 0`;
  `stdout content="a2a3\s+\|\s+20\s+\|\s+rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1" isRegex=true`
- **runtime**: < 0.1 s (verified)

**perft-fen-position**
- **Why missing**: `position=` with a real FEN (castling rights, en passant) exercises the FEN parser;
  only `startpos` would otherwise ever be used.
- **args**: `--concurrency=4 --perft position="r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1" depth=3 divide=false --logging path=test/integration/log/perft`
- **log_path / cleanup**: `test/integration/log/perft`
- **validators**: `exitCode: 0`; `stdout content="perft finished: depth=3 nodes=97862"`
- **runtime**: < 0.1 s (verified — "Kiwipete" reference position)

**perft-invalid-fen**
- **Why missing**: no test checks that a malformed position is rejected instead of crashing.
- **args**: `--concurrency=1 --perft position="not a fen" depth=1 --logging path=test/integration/log/perft`
- **log_path / cleanup**: `test/integration/log/perft`
- **validators**: `exitCode: 2`; `stdout content="Invalid FEN for perft"`
- **runtime**: < 0.1 s (verified)

---

## 4. XBoard protocol — `test/integration/xboard/xboard_tests.py` (new module)

The README advertises "Full WinBoard/XBoard engine support", yet **all 58 existing tests run UCI**.
The Qapla engines in `test/integration/engines/` speak both protocols (verified: they contain
`protover`), so `proto=xboard` can be set per engine.

> Implementation note: `--each proto=xboard` works with `--engine conf=…` as well — `--each` beats
> the engines file's `proto=uci`, as documented (verified). The tests still set `proto` per engine
> because the mixed-protocol test needs that granularity anyway.

**xboard-sprt-two-engines**
- **Why missing**: the entire WinBoard adapter (`winboard-adapter.cpp`) is untested.
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=2 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.2+0.01 --engine conf='Qapla 0.4.0' proto=xboard --engine conf='Qapla 0.3.2' proto=xboard --logging engine=false path=test/integration/log/xboard`
- **log_path / cleanup**: `test/integration/log/xboard`
- **validators**: `exitCode: 16`; `stdout content="sprt all games completed"` (the games really ran to the end, no protocol stall)
- **runtime**: ~2 s (verified)

**xboard-mixed-protocols**
- **Why missing**: a UCI engine playing against an XBoard engine is the realistic mixed case and is
  never exercised; move/clock translation between the two adapters has no coverage.
- **args**: as above, but only the first engine gets `proto=xboard`, the second stays UCI.
- **log_path / cleanup**: `test/integration/log/xboard`
- **validators**: `exitCode: 16`; `logFiles path="" pattern="sprt-report-*.log" count=1`
- **runtime**: ~2 s

**xboard-engine-test-minimal**
- **Why missing**: the whole `--test` compliance suite (15 tests) only ever validates UCI engines;
  the XBoard code path through the engine test harness is unverified.
- **args**: `--settingsfile=test/integration/engine-test/test-engine-none.ini --test nogolimits=false nofens=false --engine conf='Qapla 0.4.0' proto=xboard --logging path=test/integration/log/xboard/enginetest`
- **log_path / cleanup**: `test/integration/log/xboard/enginetest`
- **validators**: `exitCode: 0`; `logFiles path="" pattern="engine-report-*.log" count=1 content="PASS"`
- **runtime**: ~3 s
- **Note**: if XBoard turns out not to support a sub-test, reduce to `--test` with all sub-tests
  disabled (startup/shutdown only) — the point is to prove the adapter survives the harness.

---

## 5. Opening selection — `test/integration/openings/openings_tests.py` (new module)

Every existing game-based test uses `file=test/opening/book8ply.raw order=sequential`. Neither the
PGN reader, the EPD reader, random order, the start index nor the switch policies are covered.

**openings-pgn-book**
- **Why missing**: PGN opening books are documented and completely untested; `plies=` only has an
  effect for PGN books.
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=2 --openings file=test/opening/Noomen.pgn order=sequential plies=8 --each tc=0.2+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --pgnoutput file=test/integration/log/openings/pgnbook.pgn --logging engine=false path=test/integration/log/openings`
- **log_path / cleanup**: `test/integration/log/openings`
- **validators**: `exitCode: 16`;
  `fileContent path="test/integration/log/openings/pgnbook.pgn" content="^1\. d4 d5 2\. c4" isRegex=true`
  (the games start from the replayed book moves, **not** from a `[FEN]` tag — verified)
- **runtime**: ~3 s (verified)

**openings-epd-book-black-to-move**
- **Why missing**: EPD files as *opening source* are untested, and all `wmtest.epd` positions have
  Black to move — which is exactly the case fixed in 0.6.0 ("Correct Clocks for Black-to-Move
  Openings"). That fix currently has no regression test.
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=2 --openings file=test/integration/epd/short.epd order=sequential --each tc=0.3+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --pgnoutput file=test/integration/log/openings/epdbook.pgn --logging engine=false path=test/integration/log/openings`
- **log_path / cleanup**: `test/integration/log/openings`
- **validators**: `exitCode: 16`;
  `fileContent path=".../epdbook.pgn" content="\[FEN \"[^\"]* b " isRegex=true` (Black really is to move);
  `fileContent path=".../epdbook.pgn" content="(?s)^(?:(?!time forfeit).)*$" isRegex=true message="Loss on time indicates clocks were assigned to the wrong side"`
- **runtime**: ~3 s (verified with `wmtest.epd`; the short fixture is faster)

**openings-random-seeded**
- **Why missing**: `order=random` plus `srand=` is the setting most users run tournaments with, and it
  is not covered by a single test.
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=2 --openings file=test/opening/book8ply.raw order=random srand=4711 policy=encounter --each tc=0.2+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --pgnoutput file=test/integration/log/openings/random.pgn --logging engine=false path=test/integration/log/openings`
- **log_path / cleanup**: `test/integration/log/openings`
- **validators**: `exitCode: 16`; `fileContent path=".../random.pgn" content="[SetUp \"1\"]"`;
  `fileContent path=".../random.pgn" content="(?s)^(?:(?!\[FEN \"rnbqkb1r/pp2pppp/3p1n2).)*$" isRegex=true message="Random order still produced the first sequential opening"`
- **runtime**: ~3 s
- **Note**: also the only coverage of `policy=encounter`.

**openings-start-index**
- **Why missing**: `start=` is documented but untested; an off-by-one here silently shifts every
  opening set.
- **args**: as `openings-random-seeded`, but `--openings file=test/opening/book8ply.raw order=sequential start=100` and PGN `startindex.pgn`
- **log_path / cleanup**: `test/integration/log/openings`
- **validators**: `exitCode: 16`;
  `fileContent path=".../startindex.pgn" content="[FEN \"rnbqkb1r/pp2pppp/5n2/3p4/3P4/2N5/PP2PPPP/R1BQKBNR w KQkq -"` — line 100 of `book8ply.raw` (verified)
- **runtime**: ~3 s

**openings-invalid-policy**
- **Why missing**: the validation branch in `opening-config.cpp` (`Unsupported openings policy`) has
  no test; the same holds for `order`.
- **args**: `--concurrency=1 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=2 --openings file=test/opening/book8ply.raw policy=bogus --each tc=0.2+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --logging path=test/integration/log/openings`
- **log_path / cleanup**: `test/integration/log/openings`
- **validators**: `exitCode: 2`; `stdout content="Unsupported openings policy: bogus"`
- **runtime**: < 0.1 s (no engine is started)

---

## 6. PGN output — extends `test/integration/pgnoutput/pgnoutput_tests.py`

Only `append=true` is covered. The six annotation switches decide what actually lands in the file and
are untested.

**pgnoutput-minimal-tags**
- **Why missing**: `min=true` plus all annotations off is the "rapid testing" configuration promoted in
  the README; nothing verifies that the extra tags and move comments really disappear.
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=2 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.2+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --pgnoutput file=test/integration/log/pgnoutput/min.pgn min=true clock=false eval=false depth=false pv=false --logging engine=false path=test/integration/log/pgnoutput`
- **log_path / cleanup**: `test/integration/log/pgnoutput`
- **validators**: `exitCode: 16`;
  `fileContent path=".../min.pgn" content="[Event \"Sprt\"]"`;
  `fileContent path=".../min.pgn" content="(?s)^(?:(?!\[PlyCount).)*$" isRegex=true message="min=true must drop the extended tag set"`;
  `fileContent path=".../min.pgn" content="(?s)^(?:(?!\{[+-][0-9]).)*$" isRegex=true message="Move comments present although eval/depth/clock/pv are off"`
- **runtime**: ~3 s (verified: min mode emits exactly White/Black/FEN/SetUp/Event and no move comments)

**pgnoutput-full-annotations**
- **Why missing**: the opposite direction — `pv=true` is off by default everywhere and its output
  format is never checked.
- **args**: as above, but `--pgnoutput file=test/integration/log/pgnoutput/full.pgn min=false clock=true eval=true depth=true pv=true`
- **log_path / cleanup**: `test/integration/log/pgnoutput`
- **validators**: `exitCode: 16`;
  `fileContent path=".../full.pgn" content="\{[+-][0-9]+\.[0-9]+/[0-9]+ [0-9]+\.[0-9]+s ([a-h][1-8][a-h][1-8][a-z]? ){2,}" isRegex=true`
  (eval/depth/time plus a multi-move PV — verified format: `{+0.51/7 0.05s b1c3 e7e5 f1b5 …}`);
  `fileContent path=".../full.pgn" content="[PlyCount "`
- **runtime**: ~3 s

---

## 7. Tournament — extends `test/integration/tournament/tournament_tests.py`

**tournament-continuation**
- **Why missing**: SPRT resume is covered by two tests, tournament resume by none — although the
  `.qtour` reader is a different code path (`tournament-file.cpp`) and result files are the
  highest-priority configuration source.
- **args**: `--concurrency=2 --logging engine=false path=test/integration/log/tournament/continue --tournament file=test/integration/log/tournament/continue/test-tournament-file.qtour`
- **log_path / cleanup**: `test/integration/log/tournament/continue`
- **source_files**: `[{"source": "test/integration/tournament/test-tournament-file.<os>.qtour", "target": "test/integration/log/tournament/continue/test-tournament-file.qtour"}]`
- **validators**: `exitCode: 0`; `stdout content="engines taken from"`;
  `fileContent path=".../test-tournament-file.qtour" content="[round]"`
- **runtime**: ~3 s
- **Fixture to create**: run `tournament-nonexisting-file` once with `tc=0.1+0.01`, `games=2`,
  `rounds=2` and stop it after round 1; save the resulting `.qtour` as the three per-OS variants
  (rule 3). It must contain finished games so that "already played games are skipped" is observable.

**tournament-roundrobin-too-few-engines**
- **Why missing**: `Round-robin tournament requires at least two engines.` is an explicit guard in
  `tournament.cpp` with no test.
- **args**: `--concurrency=1 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=2 --engine conf='Qapla 0.4.0' --openings file=test/opening/book8ply.raw --each tc=0.1+0.01 --logging path=test/integration/log/tournament/arity`
- **log_path / cleanup**: `test/integration/log/tournament/arity`
- **validators**: `exitCode: 2`; `stdout content="Round-robin tournament requires at least two engines."`
- **runtime**: 0.5 s (verified)

**tournament-gauntlet-fallback**
- **Why missing**: 0.6.0 changed a hard error into a fallback ("if no engine is marked `gauntlet=true`,
  the first engine is used"). The new behaviour has no regression test, and a regression would
  silently change *which pairings are played*.
- **args**: `--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=gauntlet games=2 rounds=1 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --engine conf='Spike 1.4' --logging engine=false path=test/integration/log/tournament/fallback`
- **log_path / cleanup**: `test/integration/log/tournament/fallback`
- **validators**: `exitCode: 0`;
  `stdout content="Encounter Qapla 0.4.0 vs Qapla 0.3.2"`;
  `stdout content="Encounter Qapla 0.4.0 vs Spike 1.4"`;
  `stdout content="(?s)^(?:(?!Encounter Qapla 0.3.2 vs).)*$" isRegex=true message="Round-robin pairings appeared in a gauntlet"`
- **runtime**: ~2 s (verified: exactly the two expected encounters)

**tournament-noswap-event-rating**
- **Why missing**: `noswap`, `event`, `ratinginterval` and `outcomeinterval` are documented and
  entirely uncovered; `event` is the only way to name a PGN event.
- **args**: `--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=gauntlet games=2 rounds=1 event=ITEvent noswap=true ratinginterval=2 outcomeinterval=2 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 trace=none --engine conf='Qapla 0.4.0' gauntlet=true --engine conf='Qapla 0.3.2' --pgnoutput file=test/integration/log/tournament/noswap/games.pgn --logging engine=false path=test/integration/log/tournament/noswap`
- **log_path / cleanup**: `test/integration/log/tournament/noswap`
- **validators**: `exitCode: 0`;
  `stdout content="Rating interval:   2 games"` (verified header line);
  `stdout content="swap no"` (verified encounter line);
  `fileContent path=".../games.pgn" content="[Event \"ITEvent\"]"`;
  `fileContent path=".../games.pgn" content="(?s)^(?:(?!\[White \"Qapla 0.3.2\").)*$" isRegex=true message="Colors were swapped although noswap=true"`
- **runtime**: ~2 s

---

## 8. SPRT statistics — extends `test/integration/sprt/sprt_tests.py`

**sprt-montecarlo-logistic**
- **Why missing**: `model=` has three values; only the default `normalized` is ever used. Monte Carlo
  mode needs no engines, so an alternative model can be exercised in ~2 s.
- **args**: `--sprt montecarlo=true model=logistic eloH0=0 eloH1=10 alpha=0.05 beta=0.05 maxgames=2000 --logging path=test/integration/log/sprt/mc-logistic`
- **log_path / cleanup**: `test/integration/log/sprt/mc-logistic`
- **validators**: `exitCode: 0`; `stdout content="=== SPRT (Monte Carlo) ==="`;
  `stdout content="Simulated elo difference:"`
- **runtime**: ~2 s (verified)

**sprt-montecarlo-bayesian**
- **Why missing**: same as above for `model=bayesian`; a model name that is accepted but not
  implemented would go unnoticed today.
- **args**: as above with `model=bayesian maxgames=1000`, log path `…/mc-bayesian`
- **validators**: `exitCode: 0`; `stdout content="=== SPRT (Monte Carlo) ==="`; `stdout content="H1 Accepted"`
- **runtime**: ~1 s (verified)

**sprt-pentanomial-false**
- **Why missing**: `pentanomial=true` is the default and the only value tested; the trinomial path is
  a separate statistics implementation.
- **args**: `--concurrency=2 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=4 pentanomial=false eloH0=0 eloH1=10 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.2+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --logging engine=false path=test/integration/log/sprt/trinomial`
- **log_path / cleanup**: `test/integration/log/sprt/trinomial`
- **validators**: `exitCode: 16`; `logFiles path="" pattern="sprt-report-*.log" count=1 content="Pentanomial: false"`
- **runtime**: ~3 s

---

## 9. Engine option plumbing — `test/integration/engineoptions/engineoptions_tests.py` (new module)

The documented precedence `--engine` > `--each` > `--enginesfile` is a central promise of the
configuration system and has **no** test. The engine log with `trace=all` makes it directly
observable.

**engineoptions-each-option**
- **Why missing**: nothing verifies that `--each option.X=…` actually reaches the engine as
  `setoption`.
- **args**: `--concurrency=1 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=2 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.2+0.01 option.Hash=64 trace=all --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --logging engine=true mode=one path=test/integration/log/engineoptions/each`
- **log_path / cleanup**: `test/integration/log/engineoptions/each`
- **validators**: `exitCode: 16`;
  `logFiles path="" pattern="engine-*.log" count=1 content="setoption name Hash value 64"` (verified format)
- **runtime**: ~3 s

**engineoptions-engine-overrides-each**
- **Why missing**: the precedence rule itself. A regression would silently apply the wrong options to
  one side of every match — the worst kind of silent test corruption.
- **args**: as above, but `--each … option.Hash=64` and `--engine conf='Qapla 0.3.2' option.Hash=128`,
  log path `…/engineoptions/precedence`
- **log_path / cleanup**: `test/integration/log/engineoptions/precedence`
- **validators**: `exitCode: 16`;
  `logFiles path="" pattern="engine-*.log" count=1 content="setoption name Hash value 64"`;
  `logFiles path="" pattern="engine-*.log" count=1 content="setoption name Hash value 128"`
- **runtime**: ~3 s (verified: both values appear in the single `mode=one` log)

**engineoptions-restart-always**
- **Why missing**: `restart=` (auto/on/off) is untested, and 0.6.0 added restart-reason diagnostics
  that nothing checks.
- **args**: `--concurrency=1 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=2 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.2+0.01 restart=on trace=all --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --logging engine=true mode=one path=test/integration/log/engineoptions/restart`
- **log_path / cleanup**: `test/integration/log/engineoptions/restart`
- **validators**: `exitCode: 16`;
  `logFiles path="" pattern="engine-*.log" count=1 content="Sending quit and restarting engine, reason: engine restart between games is configured \(restart = always\)"`
  (verified string)
- **runtime**: ~4 s (an extra engine start per game)

---

## 10. Interactive mode — `test/integration/interactive/interactive_tests.py` (new module)

**interactive-commands**
- **Why missing**: `--interactive` and its 13 commands (`input-handler.cpp`) are completely untested,
  although the framework already supports feeding stdin via `input`.
- **args**: `--interactive=true --concurrency=2 --enginesfile=test/integration/engines/engines.ini --sprt maxgames=4 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.2+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --logging engine=false path=test/integration/log/interactive`
- **input**: `["h", "?", "r", "o", "q"]`
- **log_path / cleanup**: `test/integration/log/interactive`
- **validators**: `exitCode: 16`;
  `stdout content="Interactive mode! Enter h or help for help"`;
  `stdout content="Available commands:"`;
  `stdout content="Quit received, finishing all games and analyses before exiting."`
- **runtime**: ~4 s (verified — piped input is accepted and `quit` exits gracefully)
- **Note**: an unknown command (`stdout content="Unknown command:"`) can be added by appending `"zzz"`
  to the input list.

---

## 11. Return-code priority — `test/integration/returncode/returncode_tests.py` (new module)

> **Resolved and implemented as two tests.** The planned test assumed the README's priority rule
> ("An engine crashes during an SPRT test → return 10, not 14–16"). That rule was dropped instead: a
> failing engine forfeits its game and the run keeps its own outcome code. The README now documents
> that, and the module pins it for *both* modes — see Q1 in
> [integration-test-questions.md](integration-test-questions.md).

**returncode-sprt-survives-engine-failure**
- **Why missing**: no test covered what a run returns when an engine fails — neither for SPRT nor for
  tournaments.
- **args**: `--concurrency=1 --enginesfile=test/integration/engines/engines.ini --openings file=test/opening/book8ply.raw order=sequential --each tc=0.2+0.01 trace=none --engine conf='diagnostic-engine-noinit' --engine conf='Qapla 0.4.0' --sprt maxgames=1 eloH0=0 eloH1=10 --logging engine=false path=test/integration/log/returncode/sprt`
- **log_path / cleanup**: `test/integration/log/returncode/sprt`
- **validators**: `exitCode: 16`; `stdout content="failed UCI handshake"`;
  `stdout content="engine diagnostic-engine-noinit failed to start"`; `stdout content="cause forfeit"`
- **runtime**: 19.1 s (verified) — all of it the UCI handshake timeout

**returncode-tournament-survives-engine-failure**
- **Why missing**: same question for tournaments, where the regular outcome is `0`.
- **args**: as above, but `--tournament type=gauntlet games=1 rounds=1` and log path
  `…/returncode/tournament` (one game = one handshake timeout)
- **log_path / cleanup**: `test/integration/log/returncode/tournament`
- **validators**: `exitCode: 0`; the same two failure-evidence assertions;
  `logFiles path="" pattern="tournament-report-*.log" count=1 content="failed to start"`
- **runtime**: 19.1 s (verified)
- **Note**: `diagnostic-engine-loop` would run in under a second but is useless here — it produces no
  reported failure at all, it just plays badly and gets checkmated.

---

## 12. MCP — extends `test/integration/mcp/mcp_tests.py`

17 MCP tests exist, but they only exercise `initialize`, `manage_engines` and the `sprt` tool. Of the
12 registered tools, 9 are never called, and neither `tools/list`, `resources/list` nor the tool-name
prefix are covered.

**mcp-tools-list**
- **Why missing**: `tools/list` is the schema surface every MCP client reads first; a broken schema
  builder would break all clients while every existing test still passes.
- **args**: `--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/tools-list`
- **input**: `{"jsonrpc":"2.0","id":1,"method":"tools/list"}`
- **log_path / cleanup**: `test/integration/log/mcp/tools-list`
- **validators**: `exitCode: 0`; `stdout content='"name":"read_report"'`; `stdout content='"name":"control"'`;
  `stdout content='"name":"adjudicate"'`; `stdout content='"job_intent"'`
- **runtime**: < 1 s (verified — all 12 tools are listed)

**mcp-tool-prefix**
- **Why missing**: `--mcp prefix=` exists precisely for running several MCP servers side by side and is
  untested; note the published name is `<prefix>_<tool>` (verified: `prefix=qet_` yields `qet__sprt`).
- **args**: `--settingsfile=test/integration/mcp/mcp-engines.ini --mcp prefix=qet --logging path=test/integration/log/mcp/prefix`
- **input**: `["{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}", "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"qet_control\",\"arguments\":{\"command\":\"status\"}}}"]`
- **log_path / cleanup**: `test/integration/log/mcp/prefix`
- **validators**: `exitCode: 0`; `stdout content='"name":"qet_sprt"'`;
  `stdout content='"id":2' `; `stdout content='"isError":false'`
- **runtime**: < 1 s

**mcp-control-status**
- **Why missing**: the `control` tool (status, set_concurrency, stop, cancel_job, clear_queue,
  list_results, clear_results) is the run-control API for AI clients and has no coverage at all.
- **args**: `--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/control`
- **input**: `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"control","arguments":{"command":"status"}}}`
- **log_path / cleanup**: `test/integration/log/mcp/control`
- **validators**: `exitCode: 0`; `stdout content="job_queue"`; `stdout content="current_task"`;
  `stdout content='"isError":false'`
- **runtime**: < 1 s (verified: returns `current_task`, `job_queue`, `running_games`)

**mcp-epd-tool-and-report-resource**
- **Why missing**: `sprt` is the only runner tool ever called via MCP; and the whole report-resource
  mechanism (`qapla://reports/...` via `resources/list` / `read_report`) is untested, although it is
  how an AI client retrieves results.
- **args**: `--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/epd-tool`
- **input**:
  1. `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"epd","arguments":{"engines":"Qapla 0.4.0","epd_file":"test/integration/epd/short.epd","epd_depth":1,"epd_minsuccess":0,"concurrency":4,"logging_path":"test/integration/log/mcp/epd-tool"}}}`
  2. `{"jsonrpc":"2.0","id":2,"method":"resources/list"}`
- **log_path / cleanup**: `test/integration/log/mcp/epd-tool`
- **validators**: `exitCode: 0`;
  `stdout content="Tool 'epd' finished. Result: Success"`;
  `stdout content='"uri":"qapla://reports/epd/epd-report-' `;
  `stdout content='"isError":false'`
- **runtime**: ~2 s with the 5-position `short.epd` (verified: ~10 s with the full `wmtest.epd`)
- **Note**: `job_intent` is only mandatory when a tool is queued in the background
  (`mcp_background=true`) — a foreground call must work without it. Worth a follow-up test.

---

## 13. Runtime budget

| Group | Tests | Estimated total |
|---|---|---|
| Perft | 4 | < 1 s |
| XBoard | 3 | ~7 s |
| Openings | 5 | ~12 s |
| PGN output | 2 | ~6 s |
| Tournament | 4 | ~8 s |
| SPRT statistics | 3 | ~6 s |
| Engine options | 3 | ~10 s |
| Interactive | 1 | ~4 s |
| Return code | 1 | ~20 s (see §11) |
| MCP | 4 | ~5 s |
| **Total** | **30** | **≈ 80 s** (plus the return-code test) |

Every test stays inside the desired 20 s; the return-code test is the only one that reaches it. The
5-minute hard limit is not approached anywhere. Verify with the runner's own numbers after
implementation — the summary's "Slowest tests" block names the candidates to re-parametrize.

---

## 14. Implementation order (suggested)

1. **Perft + parameter-error tests** (perft ×4, `openings-invalid-policy`,
   `tournament-roundrobin-too-few-engines`) — no engines, instant, zero flakiness risk.
2. **New fixtures**: `test/integration/epd/short.epd`, the three `test-tournament-file.<os>.qtour`.
3. **Feature groups**: XBoard, openings, PGN output, engine options, SPRT statistics.
4. **Interactive + MCP** — both need `input`; run them last so stdin handling can be debugged in
   isolation.
5. **Return-code priority** — only after the intended behaviour has been decided (§11).

After implementation, extend [test/integration/tests.md](integration/tests.md) with the new tests and
update the total count. Done: the file now lists all 98 implemented tests (58 originally documented,
11 that had been listed as "planned" but were already implemented, and the 29 added here).
