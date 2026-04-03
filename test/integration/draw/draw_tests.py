#!/usr/bin/env python3
"""Draw Adjudication Tests - Validates --draw parameter group."""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of draw adjudication tests."""
    return [
        {
            "name": "draw-adjudication",
            "description": "SPRT with draw adjudication in test mode - parameters accepted, maxgames reached",
            "args": "--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --draw movenumber=1 movecount=1 score=5000 test=true --logging path=test/integration/log/draw",
            "log_path": "test/integration/log/draw",
            "validators": [
                {"type": "exitCode", "expected": 16},
            ],
            "cleanup": "test/integration/log/draw",
        },
    ]
