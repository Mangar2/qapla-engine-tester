#!/usr/bin/env python3
"""SPSA Tests - Validates --spsa optimization workflow."""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of SPSA tests."""
    return [
        {
            "name": "spsa-basic",
            "description": "SPSA optimization with 2 iterations - verifies tuning workflow completes successfully",
            "args": "--concurrency=2 --enginesfile=test/integration/engines/engines.ini --spsa iterations=2 gamesperpair=2 activepairs=1 outcomeinterval=1 --spsavalue name=Hash default=16 min=8 max=32 step=4 --engine conf='Qapla 0.4.0' --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 --logging engine=false path=test/integration/log/spsa",
            "log_path": "test/integration/log/spsa",
            "validators": [
                {"type": "exitCode", "expected": 0},
            ],
            "cleanup": "test/integration/log/spsa",
        },
    ]
