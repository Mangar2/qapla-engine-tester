#!/usr/bin/env python3
"""System Test Tests - Validates --systemtest NPS stability mode."""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of system test tests."""
    return [
        {
            "name": "systemtest-basic",
            "description": "NPS stability system test with 2 concurrency steps - verifies report is produced",
            "args": "--concurrency=2 --enginesfile=test/integration/engines/engines.ini --systemtest maxcores=2 step=1 steptime=5 --engine conf='Qapla 0.4.0' --each tc=1+0 --logging engine=false path=test/integration/log/systemtest",
            "log_path": "test/integration/log/systemtest",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "logFiles", "path": "", "pattern": "systemtest-report-*.log", "count": 1},
            ],
            "cleanup": "test/integration/log/systemtest",
        },
    ]
