#!/usr/bin/env python3
"""Parameter Tests - Validates parameter handling."""

from typing import List, Dict, Any


def get_tests() -> List[Dict[str, Any]]:
    """Return list of parameter tests."""
    return [
        {
            "name": "parameter-concurrency-basic",
            "description": "SPRT with concurrency=1 - single game execution",
            "args": "--settingsfile=test/integration/parameter/test-parameter-concurrency.ini",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 10}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-missing-mandatory",
            "description": "SPRT with missing mandatory parameter (openings file)",
            "args": "--settingsfile=test/integration/parameter/test-parameter-missing-mandatory.ini",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 2}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-cmdline-override",
            "description": "SPRT with command line parameter overriding missing ini parameter",
            "args": "--settingsfile=test/integration/parameter/test-parameter-missing-mandatory.ini --openings file=test/opening/book8ply.raw",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 10}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-sprt-file",
            "description": "SPRT using sprt file with concurrency parameter",
            "args": "--concurrency=2 --sprt file=test/integration/parameter/test-parameter-sprt-file.qsprt",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 10}],
            "cleanup": "test/integration/log/parameter",
        },
    ]
