#!/usr/bin/env python3
"""Resign Adjudication Tests - Validates --resign parameter group."""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of resign adjudication tests."""
    return [
        {
            "name": "resign-adjudication",
            "description": "SPRT with resignation adjudication in test mode - parameters accepted, maxgames reached",
            "args": "--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --resign movecount=3 score=500 twosided=false test=true --logging path=test/integration/log/resign",
            "log_path": "test/integration/log/resign",
            "validators": [
                {"type": "exitCode", "expected": 16},
            ],
            "cleanup": "test/integration/log/resign",
        },
    ]
