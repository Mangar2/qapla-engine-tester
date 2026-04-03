#!/usr/bin/env python3
"""SPRT Tests - Basic SPRT regression tests."""

from typing import List, Dict, Any


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
            "description": "Basic SPRT test with H1 accepted (exit code 14)",
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
            "args": "--concurrency=2 --logging path=test/integration/log/sprt --sprt file=test/integration/sprt/test-sprt-file.qsprt",
            "log_path": "test/integration/log/sprt",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": "test/integration/sprt/test-sprt-file.qsprt",
                    "content": "games===",
                    "message": "Tournament results from source file were not reused (continuation failed)."
                },
            ],
            "cleanup": "test/integration/log/sprt",
            "source_files": [
                {
                    "source": "test/integration/sprt/test-sprt-file.qsprt.source",
                    "target": "test/integration/sprt/test-sprt-file.qsprt"
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
                {"type": "stdout", "content": "Running SPRT Monte carlo simulation", "isRegex": False},
                {"type": "stdout", "content": "H0 Accepted", "isRegex": False},
            ],
            "cleanup": "test/integration/log/sprt/montecarlo",
        },
    ]
