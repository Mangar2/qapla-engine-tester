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
            "validators": [{"type": "exitCode", "expected": 15}],
            "cleanup": "test/integration/log/sprt",
        },
    ]
