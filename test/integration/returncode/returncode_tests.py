#!/usr/bin/env python3
"""Return Code Tests - Validates that engine failures do not change a run's outcome code.

A tournament or SPRT run compares engines with each other, so an engine that
fails is part of the result, not a reason to discard it: the affected game is
adjudicated as a forfeit and the run returns its regular outcome (14-16 for
SPRT, 0 for a tournament). The engine codes 10-12 are reserved for the
compliance suite (--test), which is covered by the engine-test module.

Both tests use diagnostic-engine-noinit, the only engine here that fails in a
way the tool actually reports ("failed UCI handshake"). Waiting out that
handshake is what makes these the slowest tests of the group at ~19s each;
diagnostic-engine-loop would be faster but merely plays badly, which would not
test anything.
"""

from typing import Any, Dict, List

_BASE = ("--concurrency=1 --enginesfile=test/integration/engines/engines.ini "
         "--openings file=test/opening/book8ply.raw order=sequential "
         "--each tc=0.2+0.01 trace=none "
         "--engine conf='diagnostic-engine-noinit' --engine conf='Qapla 0.4.0'")

# Proof that the engine really failed - without it both tests would also pass
# with two healthy engines.
_FAILURE_EVIDENCE = [
    {"type": "stdout", "content": "failed UCI handshake"},
    {
        "type": "stdout",
        "content": "engine diagnostic-engine-noinit failed to start",
    },
]


def get_tests() -> List[Dict[str, Any]]:
    """Return list of return code tests."""
    return [
        {
            "name": "returncode-sprt-survives-engine-failure",
            "description": "An engine that never initializes forfeits its game; SPRT still returns its own result",
            "args": f"{_BASE} --sprt maxgames=1 eloH0=0 eloH1=10 "
                    "--logging engine=false path=test/integration/log/returncode/sprt",
            "log_path": "test/integration/log/returncode/sprt",
            "validators": [
                # The SPRT outcome, not an engine error code.
                {"type": "exitCode", "expected": 16},
                *_FAILURE_EVIDENCE,
                {"type": "stdout", "content": "cause forfeit"},
            ],
            "cleanup": "test/integration/log/returncode/sprt",
        },
        {
            "name": "returncode-tournament-survives-engine-failure",
            "description": "An engine that never initializes forfeits its games; the tournament still returns 0",
            # A single game keeps the run at one handshake timeout instead of two.
            "args": f"{_BASE} --tournament type=gauntlet games=1 rounds=1 "
                    "--logging engine=false path=test/integration/log/returncode/tournament",
            "log_path": "test/integration/log/returncode/tournament",
            "validators": [
                {"type": "exitCode", "expected": 0},
                *_FAILURE_EVIDENCE,
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "tournament-report-*.log",
                    "count": 1,
                    "content": "failed to start",
                },
            ],
            "cleanup": "test/integration/log/returncode/tournament",
        },
    ]
