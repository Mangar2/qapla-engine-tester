#!/usr/bin/env python3
"""EPD Tests - Validates EPD functionality."""

from typing import List, Dict, Any


def get_tests() -> List[Dict[str, Any]]:
    """Return list of sprt tests."""
    return [
        {
            "name": "epd-basic-run",
            "description": "EPD functional test - expects basic success",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd minsuccess=100",
            "log_path": "test/integration/log/epd",
            "validators": [
                {"type": "exitCode", "expected": 13},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "qapla-engine-tester.log",
                    "count": 1,
                    "content": "EPD test failed:",
                },
            ],
            "cleanup": "test/integration/log/epd",
        },
        {
            "name": "epd-short-time",
            "description": "EPD test with short time (1s) - measuring baseline success rate",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd maxtime=1 minsuccess=100",
            "log_path": "test/integration/log/epd",
            "validators": [{"type": "exitCode", "expected": 0}],
            "cleanup": "test/integration/log/epd",
        },
        {
            "name": "epd-long-time",
            "description": "EPD test with longer time (10s) - expects higher success rate",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd maxtime=10 minsuccess=100 seenplies=4",
            "log_path": "test/integration/log/epd",
            "validators": [{"type": "exitCode", "expected": 0}],
            "cleanup": "test/integration/log/epd",
        },
        {
            "name": "epd-fast-exit",
            "description": "EPD test (10s) with no minimum time/plies constraints (fast exit)",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd maxtime=10 mintime=0 seenplies=0 minsuccess=100",
            "log_path": "test/integration/log/epd",
            "validators": [{"type": "exitCode", "expected": 0}],
            "cleanup": "test/integration/log/epd",
        },
    ]
