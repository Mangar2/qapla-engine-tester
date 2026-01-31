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
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nostop=false --engine conf='Qapla 0.3.0'",
            "log_path": "test/integration/log/engine-test/stop-fail",
            "validators": [
                {"type": "exitCode", "expected": 11}
            ],
            "cleanup": "test/integration/log/engine-test/stop-fail",
        },
        {
            "name": "engine-test-nomemory-fail",
            "description": "Negative test for memory/hash adjustment",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nomemory=false --engine conf='Qapla 0.2.0'",
            "log_path": "test/integration/log/engine-test/memory-fail",
            "validators": [
                {"type": "exitCode", "expected": 12}
            ],
            "cleanup": "test/integration/log/engine-test/memory-fail",
        },
        {
            "name": "engine-test-nooption-fail",
            "description": "Negative test for UCI options handling",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nooption=false --engine conf='Qapla 0.3.0'",
            "log_path": "test/integration/log/engine-test/option-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/option-fail",
        },
        {
            "name": "engine-test-noanalyze-fail",
            "description": "Negative test for standard analysis",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test noanalyze=false --engine conf='midnightv5'",
            "log_path": "test/integration/log/engine-test/analyze-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/analyze-fail",
        },
        {
            "name": "engine-test-nowait-fail",
            "description": "Negative test for infinite search",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nowait=false --engine conf='Qapla 0.2.0'",
            "log_path": "test/integration/log/engine-test/nowait-fail",
            "validators": [
                {"type": "exitCode", "expected": 11}
            ],
            "cleanup": "test/integration/log/engine-test/nowait-fail",
        },
        {
            "name": "engine-test-nogolimits-fail",
            "description": "Negative test for go limits (depth/nodes/movetime)",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nogolimits=false",
            "log_path": "test/integration/log/engine-test/golimits-fail",
            "validators": [
                {"type": "exitCode", "expected": 11}
            ],
            "cleanup": "test/integration/log/engine-test/golimits-fail",
        },
        {
            "name": "engine-test-nofens-fail",
            "description": "Negative test for playing from EP FENs",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nofens=false",
            "log_path": "test/integration/log/engine-test/fens-fail",
            "validators": [
                {"type": "exitCode", "expected": 11}
            ],
            "cleanup": "test/integration/log/engine-test/fens-fail",
        },
        {
            "name": "engine-test-noepd-fail",
            "description": "Negative test for EPD bestmove correctness",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test noepd=false",
            "log_path": "test/integration/log/engine-test/epd-fail",
            "validators": [
                {"type": "exitCode", "expected": 12}
            ],
            "cleanup": "test/integration/log/engine-test/epd-fail",
        },
        {
            "name": "engine-test-nocompute-fail",
            "description": "Negative test for single compute game",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test nocompute=false",
            "log_path": "test/integration/log/engine-test/compute-fail",
            "validators": [
                {"type": "exitCode", "expected": 11}
            ],
            "cleanup": "test/integration/log/engine-test/compute-fail",
        },
        {
            "name": "engine-test-noponder-fail",
            "description": "Negative test for pondering",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test noponder=false",
            "log_path": "test/integration/log/engine-test/ponder-fail",
            "validators": [
                {"type": "exitCode", "expected": 10}
            ],
            "cleanup": "test/integration/log/engine-test/ponder-fail",
        },
        {
            "name": "engine-test-underrun-fail",
            "description": "Negative test for movetime underrun",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test underrun=true",
            "log_path": "test/integration/log/engine-test/underrun-fail",
            "validators": [
                {"type": "exitCode", "expected": 12}
            ],
            "cleanup": "test/integration/log/engine-test/underrun-fail",
        },
        {
            "name": "engine-test-timeusage-fail",
            "description": "Negative test for time usage in games",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini --test timeusage=true numgames=2",
            "log_path": "test/integration/log/engine-test/timeusage-fail",
            "validators": [
                {"type": "exitCode", "expected": 12}
            ],
            "cleanup": "test/integration/log/engine-test/timeusage-fail",
        }
    ]
