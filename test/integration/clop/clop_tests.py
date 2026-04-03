#!/usr/bin/env python3
"""CLOP Tests - Validates --clop optimization workflow."""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of CLOP tests."""
    return [
        {
            "name": "clop-basic",
            "description": "CLOP optimization with 3 samples - verifies local optimization workflow completes",
            "args": "--concurrency=2 --enginesfile=test/integration/engines/engines.ini --clop samples=3 gamespersample=2 warmupsamples=2 outcomeinterval=1 --clopvalue name=Hash min=8 max=32 --engine conf='Qapla 0.4.0' gauntlet=true --engine conf='Qapla 0.3.2' --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 --logging engine=false path=test/integration/log/clop",
            "log_path": "test/integration/log/clop",
            "validators": [
                {"type": "exitCode", "expected": 0},
            ],
            "cleanup": "test/integration/log/clop",
        },
    ]
