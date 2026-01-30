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
                    "pattern": "report-.*\\.log",
                    "count": 1,
                    "content": "PASS Engine starts and stops quickly and without issues"
                }
            ],
            "cleanup": "test/integration/log/engine-test/minimal",
        }
    ]
