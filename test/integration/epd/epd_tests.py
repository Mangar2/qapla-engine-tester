#!/usr/bin/env python3
"""EPD Tests - Validates EPD functionality."""

from typing import List, Dict, Any

# The display names below (qapla-3.1/qapla-3.2) are asserted on in log content
# checks further down, so they must match exactly; see engine_ini_block() in
# test_framework.py for how "as_" resolves to an OS-specific engine.
_SINGLE_ENGINE = [{"name": "Qapla 0.3.2", "as_": "qapla-3.2"}]
_TWO_ENGINES = [
    {"name": "Qapla 0.3.1", "as_": "qapla-3.1"},
    {"name": "Qapla 0.3.2", "as_": "qapla-3.2"},
]


def get_tests() -> List[Dict[str, Any]]:
    """Return list of sprt tests."""
    return [
        {
            "name": "epd-basic-run",
            "description": "EPD functional test - expects basic success",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd minsuccess=100",
            "engine_refs": _SINGLE_ENGINE,
            "log_path": "test/integration/log/epd",
            "validators": [
                {"type": "exitCode", "expected": 13},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "epd-report*.log",
                    "count": 1,
                    "content": "Finished EPD test for engine: qapla-3.2",
                },
            ],
            "cleanup": "test/integration/log/epd",
        },
        {
            "name": "epd-short-time",
            "description": "EPD test with short time (1s) - measuring baseline success rate",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd maxtime=1 minsuccess=30",
            "engine_refs": _SINGLE_ENGINE,
            "log_path": "test/integration/log/epd",
            "validators": [{"type": "exitCode", "expected": 0}],
            "cleanup": "test/integration/log/epd",
        },
        {
            "name": "epd-long-time",
            "description": "EPD test with longer time (10s) - expects higher success rate",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd maxtime=10 minsuccess=50 mintime=1 seenplies=2",
            "engine_refs": _SINGLE_ENGINE,
            "log_path": "test/integration/log/epd",
            "validators": [{"type": "exitCode", "expected": 0}],
            "cleanup": "test/integration/log/epd",
        },
        {
            "name": "epd-two-engines",
            "description": "EPD test (2s) with two engines, no success requirement",
            "args": "--settingsfile=test/integration/epd/test-epd-two-engines.ini --epd maxtime=2 mintime=0 seenplies=0",
            "engine_refs": _TWO_ENGINES,
            "log_path": "test/integration/log/epd",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "epd-report*.log",
                    "count": 1,
                    "content": "Finished EPD test for engine: qapla-3.1",
                },
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "epd-report*.log",
                    "count": 1,
                    "content": "Finished EPD test for engine: qapla-3.2"
                },
            ],
            "cleanup": "test/integration/log/epd",
        },
        {
            "name": "epd-depth-fixed",
            "description": "EPD test with fixed depth=8 - verifies depth-limited search mode",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd depth=8 minsuccess=0",
            "engine_refs": _SINGLE_ENGINE,
            "log_path": "test/integration/log/epd",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "logFiles", "path": "", "pattern": "epd-report*.log", "count": 1},
            ],
            "cleanup": "test/integration/log/epd",
        },
        {
            "name": "epd-nodes-fixed",
            "description": "EPD test with fixed nodes=100000 - verifies node-limited search mode",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd nodes=100000 minsuccess=0",
            "engine_refs": _SINGLE_ENGINE,
            "log_path": "test/integration/log/epd",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "logFiles", "path": "", "pattern": "epd-report*.log", "count": 1},
            ],
            "cleanup": "test/integration/log/epd",
        },
    ]
