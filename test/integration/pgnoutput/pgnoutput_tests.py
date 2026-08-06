#!/usr/bin/env python3
"""PGN Output Tests - Validates --pgnoutput parameter group."""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of PGN output tests."""
    return [
        {
            "name": "pgnoutput-append",
            "description": "PGN output with append=true - original content must be preserved at start of file",
            "args": "--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --pgnoutput file=test/integration/log/pgnoutput/games.pgn append=true --logging engine=false path=test/integration/log/pgnoutput",
            "log_path": "test/integration/log/pgnoutput",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "fileAppendOnly", "path": "test/integration/log/pgnoutput/games.pgn"},
            ],
            "cleanup": "test/integration/log/pgnoutput",
            "source_files": [
                {
                    "source": "test/integration/pgnoutput/pgnoutput-append.pgn",
                    "target": "test/integration/log/pgnoutput/games.pgn",
                }
            ],
        },
    ]
