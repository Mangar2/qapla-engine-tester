#!/usr/bin/env python3
"""SPRT Tests - Basic SPRT regression tests."""

from typing import List, Dict, Any

from test_framework import platform_suffix

# The saved-state fixture below embeds an engine cmd= path, so it needs an
# OS-specific variant just like the engines files (see test_framework.py).
_SPRT_FILE_SOURCE = f"test/integration/sprt/test-sprt-file.{platform_suffix()}.qsprt"

_EXE_SUFFIX = ".exe" if platform_suffix() == "windows" else ""


def get_tests() -> List[Dict[str, Any]]:
    """Return list of SPRT tests."""
    return [
        {
            "name": "sprt-maxgames-reached",
            "description": "SPRT test with maxgames reached (exit code 16)",
            "args": "--settingsfile=test/integration/sprt/test-sprt-maxgames.ini",
            "log_path": "test/integration/log/sprt",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/sprt",
        },
        {
            "name": "sprt-basic-h0-accepted",
            "description": "SPRT test with H0 accepted (exit code 15)",
            "args": "--settingsfile=test/integration/sprt/test-sprt-15.ini",
            "log_path": "test/integration/log/sprt",
            "validators": [{"type": "exitCode", "expected": 15}],
            "cleanup": "test/integration/log/sprt",
        },
        {
            "name": "sprt-basic-h1-accepted",
            "description": "SPRT reaching H1 accepted (exit code 14) - challenger wins every game",
            "args": "--settingsfile=test/integration/sprt/test-sprt-14.ini",
            "log_path": "test/integration/log/sprt",
            "validators": [{"type": "exitCode", "expected": 14}],
            "cleanup": "test/integration/log/sprt",
        },
        {
            "name": "sprt-with-ponder",
            "description": "SPRT with ponder option",
            "args": "--settingsfile=test/integration/sprt/test-sprt-14-ponder.ini",
            "log_path": "test/integration/log/sprt",
            "validators": [{"type": "exitCode", "expected": 14}],
            "cleanup": "test/integration/log/sprt",
        },
        {
            "name": "sprt-rapid-mode",
            "description": "SPRT in rapid mode",
            "args": "--settingsfile=test/integration/sprt/test-sprt-rapid.ini",
            "log_path": "test/integration/log/sprt",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/sprt",
        },
        {
            "name": "sprt-continuation",
            "description": "SPRT continuation from existing SPRT file",
            "args": "--concurrency=2 --logging path=test/integration/log/sprt --sprt file=test/integration/log/sprt/test-sprt-file.qsprt",
            "log_path": "test/integration/log/sprt",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/sprt/test-sprt-file.qsprt",
                    "content": "games===",
                    "message": "Tournament results from source file were not reused (continuation failed)."
                },
            ],
            "cleanup": "test/integration/log/sprt",
            "source_files": [
                {
                    "source": _SPRT_FILE_SOURCE,
                    "target": "test/integration/log/sprt/test-sprt-file.qsprt"
                }
            ],
        },
        {
            "name": "sprt-continuation-configured-engines",
            "description": "SPRT continuation with engines given on the command line - the engine sections of the SPRT file must be ignored, not merged",
            "args": "--concurrency=2 --logging path=test/integration/log/sprt engine=false --sprt file=test/integration/log/sprt/test-sprt-file.qsprt --engine cmd=test/integration/engines/diagnostic-engine-lossontime"
            + _EXE_SUFFIX
            + " name=llt1 --engine cmd=test/integration/engines/diagnostic-engine-lossontime"
            + _EXE_SUFFIX
            + " name=llt2",
            "log_path": "test/integration/log/sprt",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "stdout",
                    "content": "engine sections in test/integration/log/sprt/test-sprt-file.qsprt ignored",
                },
                {
                    # Exactly one pairing: merging both sources would double every engine.
                    "type": "stdout",
                    "content": r"(?s)^(?:(?!Encounter).)*Encounter llt1 vs llt2(?:(?!Encounter).)*$",
                    "isRegex": True,
                },
            ],
            "cleanup": "test/integration/log/sprt",
            "source_files": [
                {
                    "source": _SPRT_FILE_SOURCE,
                    "target": "test/integration/log/sprt/test-sprt-file.qsprt"
                }
            ],
        },
        {
            "name": "sprt-nonexisting-file",
            "description": "SPRT continuation from existing SPRT file",
            "args": "--concurrency=2 --settingsfile=test/integration/sprt/test-sprt-write-nonexisting.ini",
            "log_path": "test/integration/log/sprt",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/sprt/test-sprt-write-nonexisting.qsprt",
                    "content": "[each]"
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/sprt/test-sprt-write-nonexisting.qsprt",
                    "content": "[round]"
                }
            ],
            "cleanup": "test/integration/log/sprt"
        },
        {
            "name": "sprt-montecarlo",
            "description": "SPRT Monte Carlo simulation - no engines needed, pure statistical simulation",
            "args": "--sprt montecarlo=true eloH0=0 eloH1=10 alpha=0.05 beta=0.05 maxgames=3000 --logging path=test/integration/log/sprt/montecarlo",
            "log_path": "test/integration/log/sprt/montecarlo",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "=== SPRT (Monte Carlo) ===", "isRegex": False},
                {"type": "stdout", "content": "H0 Accepted", "isRegex": False},
            ],
            "cleanup": "test/integration/log/sprt/montecarlo",
        },
        {
            "name": "sprt-continuation-tightened-bounds",
            "description": "An SPRT decided with loose error bounds is resumed with tighter ones and keeps its games",
            # A run that has already decided is the interesting case: with alpha and beta lowered
            # the same games no longer suffice, so the resumed run has to carry them over and play
            # on. Restarting instead would be invisible in the final counts, which is why the test
            # asserts on where the game numbering picks up.
            "setup_args": "--concurrency=1 --enginesfile=test/integration/engines/engines.ini "
                          "--sprt file=test/integration/log/sprt/tightened/sprt.qsprt maxgames=60 "
                          "eloH0=0 eloH1=10 alpha=0.45 beta=0.45 model=logistic "
                          "--openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 "
                          "--engine conf='Qapla 0.4.0' --engine conf='diagnostic-engine-lossontime' "
                          "--logging engine=false path=test/integration/log/sprt/tightened",
            "args": "--concurrency=1 "
                    "--sprt file=test/integration/log/sprt/tightened/sprt.qsprt maxgames=60 "
                    "alpha=0.3 beta=0.3 "
                    "--logging engine=false path=test/integration/log/sprt/tightened",
            "log_path": "test/integration/log/sprt/tightened",
            "validators": [
                {"type": "exitCode", "expected": 14},
                {
                    # The tightened bounds from the command line replace those stored in the file.
                    "type": "stdout",
                    "content": r"\[ -0\.85 <",
                    "isRegex": True,
                },
                {
                    # The decisive check: play picks up after the games already in the file
                    # instead of numbering from one again. Where exactly it picks up depends on
                    # when the loose bounds were crossed, so only the restart is asserted.
                    "type": "stdout",
                    "content": r"(?s)^(?!.*match game 1\s)",
                    "isRegex": True,
                    "message": "Numbering restarted at game 1: the stored games were not reused",
                },
            ],
            "cleanup": "test/integration/log/sprt/tightened",
        },
        {
            "name": "sprt-file-uci-option-roundtrip",
            "description": "An SPRT file written with a UCI option can be resumed, and the option reaches the engine unchanged",
            # Same round trip as tournament-file-uci-option-roundtrip, for the
            # other state file type. Black box: the test asserts what the resumed
            # run sends to its engines, never how the file stores it.
            "setup_args": "--concurrency=1 --enginesfile=test/integration/engines/engines.ini "
                          "--sprt file=test/integration/log/sprt/roundtrip/sprt.qsprt maxgames=2 "
                          "--openings file=test/opening/book8ply.raw order=sequential "
                          "--each tc=0.2+0.01 trace=all "
                          "--engine conf='Qapla 0.4.0' option.Hash=128 "
                          "--engine conf='Qapla 0.3.2' option.Hash=64 "
                          "--logging engine=true mode=one path=test/integration/log/sprt/roundtrip",
            "args": "--concurrency=1 "
                    "--sprt file=test/integration/log/sprt/roundtrip/sprt.qsprt maxgames=4 "
                    "--logging engine=true mode=each path=test/integration/log/sprt/roundtrip",
            "log_path": "test/integration/log/sprt/roundtrip",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-#0-*.log",
                    "count": 1,
                    "content": "setoption name Hash value 128",
                },
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-#1-*.log",
                    "count": 1,
                    "content": "setoption name Hash value 64",
                },
            ],
            "cleanup": "test/integration/log/sprt/roundtrip",
        },
        {
            "name": "sprt-montecarlo-logistic",
            "description": "Monte Carlo simulation with the logistic model - covers a non-default SPRT model",
            "args": "--sprt montecarlo=true model=logistic eloH0=0 eloH1=10 alpha=0.05 beta=0.05 maxgames=1000 "
                    "--logging path=test/integration/log/sprt/mc-logistic",
            "log_path": "test/integration/log/sprt/mc-logistic",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "=== SPRT (Monte Carlo) ==="},
                {"type": "stdout", "content": "Simulated elo difference:"},
            ],
            "cleanup": "test/integration/log/sprt/mc-logistic",
        },
        {
            "name": "sprt-montecarlo-bayesian",
            "description": "Monte Carlo simulation with the bayesian model - covers a non-default SPRT model",
            "args": "--sprt montecarlo=true model=bayesian eloH0=0 eloH1=10 alpha=0.05 beta=0.05 maxgames=1000 "
                    "--logging path=test/integration/log/sprt/mc-bayesian",
            "log_path": "test/integration/log/sprt/mc-bayesian",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "=== SPRT (Monte Carlo) ==="},
                {"type": "stdout", "content": "H1 Accepted"},
            ],
            "cleanup": "test/integration/log/sprt/mc-bayesian",
        },
        {
            "name": "sprt-pentanomial-false",
            "description": "SPRT with the trinomial model - pentanomial=true is the default everywhere else",
            "args": "--concurrency=2 --enginesfile=test/integration/engines/engines.ini "
                    "--sprt maxgames=4 pentanomial=false eloH0=0 eloH1=10 "
                    "--openings file=test/opening/book8ply.raw order=sequential --each tc=0.2+0.01 "
                    "--engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--logging engine=false path=test/integration/log/sprt/trinomial",
            "log_path": "test/integration/log/sprt/trinomial",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "sprt-report-*.log",
                    "count": 1,
                    "content": "Pentanomial: false",
                },
            ],
            "cleanup": "test/integration/log/sprt/trinomial",
        },
    ]
