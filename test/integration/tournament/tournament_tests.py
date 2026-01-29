#!/usr/bin/env python3
"""Tournament Tests - Validates Tournament functionality."""

from typing import List, Dict, Any


def get_tests() -> List[Dict[str, Any]]:
    """Return list of tournament tests."""
    return [
        {
            "name": "tournament-basic",
            "description": "Basic tournament gauntlet test",
            "args": "--settingsfile=test/integration/tournament/test-tournament-file.ini",
            "log_path": "test/integration/log/tournament",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "tournament-report-*.log",
                    "count": 1,
                },
                 {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "test.pgn",
                    "count": 1,
                },
            ],
            "cleanup": "test/integration/log/tournament",
        }
    ]
