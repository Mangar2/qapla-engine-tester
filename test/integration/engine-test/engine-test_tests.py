#!/usr/bin/env python3
"""Engine Test Suite - Basic tests for the engine testing functionality."""

from typing import List, Dict, Any


def get_tests() -> List[Dict[str, Any]]:
    """Return list of engine tests."""
    return [
        {
            "name": "engine-test-minimal",
            "description": "Minimal engine test with all optional tests disabled",
            "args": "--settingsfile=test/integration/engine-test/test-engine-base.ini",
            "log_path": "test/integration/log/engine-test/minimal",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-report-.*\\.log",
                    "count": 1,
                    "content": "PASS Engine starts and stops quickly and without issues"
                }
            ],
            "cleanup": "test/integration/log/engine-test/minimal",
        },
        {
            "name": "engine-test-noinit",
            "description": "Engine fails to initialize (noinit diagnostic engine)",
            "args": "--settingsfile=test/integration/engine-test/test-engine-fail.ini",
            "log_path": "test/integration/log/engine-test/fail",
            "validators": [
                {"type": "exitCode", "expected": 10},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-report-.*\\.log",
                    "count": 1,
                    "content": "FAIL Engine starts and stops quickly and without issues"
                }
            ],
            "cleanup": "test/integration/log/engine-test/fail",
        },
        {
            "name": "engine-test-full-success",
            "description": "Run all engine tests (success expected)",
            "args": "--settingsfile=test/integration/engine-test/test-engine-full.ini",
            "log_path": "test/integration/log/engine-test/full",
            "validators": [
                {"type": "exitCode", "expected": 0}
            ],
            "cleanup": "test/integration/log/engine-test/full",
        },
        {
            "name": "engine-test-nostop-fail",
            "description": "Negative test for immediate stop command",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nostop=false --engine conf='Qapla 0.3.0' --logging path=test/integration/log/engine-test/stop-fail",
            "log_path": "test/integration/log/engine-test/stop-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/stop-fail",
        },
        {
            "name": "engine-test-nomemory-fail",
            "description": "Negative test for memory/hash adjustment",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nomemory=false --engine conf='Qapla 0.2.0' --logging path=test/integration/log/engine-test/memory-fail",
            "log_path": "test/integration/log/engine-test/memory-fail",
            "validators": [
                {"type": "exitCode", "expected": 12}
            ],
            "cleanup": "test/integration/log/engine-test/memory-fail",
        },
        {
            "name": "engine-test-nooption-fail",
            "description": "Negative test for UCI options handling",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nooption=false --engine conf='Qapla 0.3.1' --logging path=test/integration/log/engine-test/option-fail",
            "log_path": "test/integration/log/engine-test/option-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/option-fail",
        },
        {
            "name": "engine-test-noanalyze-fail",
            "description": "Negative test for standard analysis",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test noanalyze=false --engine conf='midnightv5' --logging path=test/integration/log/engine-test/analyze-fail",
            "log_path": "test/integration/log/engine-test/analyze-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/analyze-fail",
        },
        {
            "name": "engine-test-nowait-fail",
            "description": "Negative test for infinite search",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nowait=false --engine conf='Counter-v3.3' --logging path=test/integration/log/engine-test/nowait-fail",
            "log_path": "test/integration/log/engine-test/nowait-fail",
            "validators": [
                {"type": "exitCode", "expected": 11}
            ],
            "cleanup": "test/integration/log/engine-test/nowait-fail",
        },
        {
            "name": "engine-test-nogolimits-fail",
            "description": "Negative test for go limits (depth/nodes/movetime)",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nogolimits=false --engine conf='Qapla 0.2.0' --logging path=test/integration/log/engine-test/golimits-fail",
            "log_path": "test/integration/log/engine-test/golimits-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/golimits-fail",
        },
        {
            "name": "engine-test-nofens-fail",
            "description": "Negative test for playing from EP FENs",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nofens=false --engine conf='Qapla 0.3.1' --logging path=test/integration/log/engine-test/fens-fail",
            "log_path": "test/integration/log/engine-test/fens-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/fens-fail",
        },
        {
            "name": "engine-test-noepd-fail",
            "description": "Negative test for EPD bestmove correctness",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test noepd=false nostartstop=true --engine conf='Spike 1.4' --logging path=test/integration/log/engine-test/epd-fail",
            "log_path": "test/integration/log/engine-test/epd-fail",
            "validators": [
                {"type": "exitCode", "expected": 12},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-[0-9].*\\.log",
                    "count": 1,
                    "content": "position fen 8/8/p1p5/1p5p/1P5p/8/PPP2K1p/4R1rk w - - 0 1"
                }
            ],
            "cleanup": "test/integration/log/engine-test/epd-fail",
        },
        {
            "name": "engine-test-nocompute-fail",
            "description": "Negative test for single compute game",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nocompute=false --engine conf='diagnostic-engine-lossontime' --logging path=test/integration/log/engine-test/compute-fail",
            "log_path": "test/integration/log/engine-test/compute-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/compute-fail",
        },
        {
            "name": "engine-test-noponder-fail",
            "description": "Negative test for pondering",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test noponder=false --engine conf='Qapla 0.2.0' --logging path=test/integration/log/engine-test/ponder-fail",
            "log_path": "test/integration/log/engine-test/ponder-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/ponder-fail",
        },
        {
            "name": "engine-test-underrun-fail",
            "description": "Negative test for movetime underrun",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test underrun=true nogolimits=false --engine conf='Qapla 0.3.1' --logging path=test/integration/log/engine-test/underrun-fail",
            "log_path": "test/integration/log/engine-test/underrun-fail",
            "validators": [
                {"type": "exitCode", "expected": 12}
            ],
            "cleanup": "test/integration/log/engine-test/underrun-fail",
        },
        {
            "name": "engine-test-timeusage-fail",
            "description": "Negative test for time usage in games",
            # An engine that always overruns its clock makes the outcome
            # deterministic. The previous engine (qai_pla_uci) only dropped below
            # the one second mark when the machine was busy enough, which made the
            # test pass or fail depending on load.
            #
            # Trade-off: the note-level checks of 'timeusage' (clock never below one
            # second, reserve time preserved) evaluate games that are actually
            # played, so they cannot fire for an engine that forfeits its first
            # move. What this test pins is the [Important] time-loss detection.
            "args": "--concurrency=5 --settingsfile=test/integration/engine-test/test-engine-none.ini --test timeusage=true numgames=5 --engine conf='diagnostic-engine-lossontime' --logging path=test/integration/log/engine-test/timeusage-fail",
            "log_path": "test/integration/log/engine-test/timeusage-fail",
            "validators": [
                # A time loss is an [Important] finding, so this is an engine error,
                # not the note-level result the flaky variant produced.
                {"type": "exitCode", "expected": 10},
                {
                    # Pins the time usage measurement itself, not just the exit code.
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-report-*.log",
                    "count": 1,
                    "content": r"looses on time with time control",
                },
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-report-*.log",
                    "count": 1,
                    "content": r"FAIL Engine avoids time losses",
                },
            ],
            "cleanup": "test/integration/log/engine-test/timeusage-fail",
        }
    ]
