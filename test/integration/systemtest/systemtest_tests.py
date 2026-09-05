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
        {
            "name": "systemtest-replay-fixed-movetime",
            "description": "System test with a fixed movetime - seed game, replay phase and per-ply samples",
            # The replay phase is what the system test measures, and it drives the engines in
            # analysis mode: the engine's move is discarded and the recorded move is replayed
            # instead. A fixed movetime is the search limit that has no clock behind it, so this
            # run covers the analysis path that no clock-based test reaches.
            "args": "--concurrency=1 --enginesfile=test/integration/engines/engines.ini "
                    "--systemtest maxcores=1 step=1 steptime=5 --engine conf='Qapla 0.4.0' "
                    "--each tc=movetime(ms):50 "
                    "--logging engine=true mode=one path=test/integration/log/systemtest/movetime",
            "log_path": "test/integration/log/systemtest/movetime",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    # Without the replay phase the test would only have played its seed game.
                    "type": "stdout",
                    "content": "systemtest replay seed captured",
                },
                {
                    # Samples are the per-ply NPS measurements taken during replay; zero of them
                    # means the replayed games produced no usable move records.
                    "type": "stdout",
                    "content": r"systemtest step cores 1 games [1-9]\d* samples [1-9]\d*",
                    "isRegex": True,
                    "message": "The replay phase collected no per-ply samples",
                },
                {"type": "stdout", "content": "systemtest completed"},
                {"type": "logFiles", "path": "", "pattern": "systemtest-report-*.log", "count": 1},
                {
                    # A fixed movetime replaces the clock, in replay just as in a normal game.
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": r"(?s)go movetime 50(?!.*go wtime)",
                    "message": "The engine was given a clock although the time control is a fixed movetime",
                },
            ],
            "cleanup": "test/integration/log/systemtest/movetime",
        },
    ]
