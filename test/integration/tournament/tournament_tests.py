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
        },
        {
            "name": "tournament-nonexisting-file",
            "description": "Tournament continuation from existing tournament file",
            "args": "--settingsfile=test/integration/tournament/test-tournament-write-nonexisting.ini",
            "log_path": "test/integration/log/tournament",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/tournament/test-tournament-write-nonexisting.qtour",
                    "content": "[each]"
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/tournament/test-tournament-write-nonexisting.qtour",
                    "content": "[round]"
                }
            ],
            "cleanup": "test/integration/log/tournament"
        },
        {
            "name": "tournament-round-robin",
            "description": "Round-robin tournament with 3 engines - verifies all pairings complete",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=2 repeat=2 rounds=1 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 trace=none --engine conf='Qapla 0.3.1' --engine conf='Qapla 0.3.2' --engine conf='Spike 1.4' --logging engine=false path=test/integration/log/tournament/roundrobin",
            "log_path": "test/integration/log/tournament/roundrobin",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "logFiles", "path": "", "pattern": "tournament-report-*.log", "count": 1},
            ],
            "cleanup": "test/integration/log/tournament/roundrobin",
        },
    ]
