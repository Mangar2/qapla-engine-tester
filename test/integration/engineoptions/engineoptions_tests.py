#!/usr/bin/env python3
"""Engine Option Tests - Validates option propagation, precedence and restart mode.

The documented precedence --engine > --each > --enginesfile is a central promise
of the configuration system; nothing else in the suite checks that the values
actually arrive at the engine. The engine log with trace=all makes the sent
'setoption' commands directly observable, so these tests assert on the protocol
traffic rather than on a summary line.
"""

from typing import Any, Dict, List

_BASE = ("--concurrency=1 --enginesfile=test/integration/engines/engines.ini "
         "--sprt maxgames=2 --openings file=test/opening/book8ply.raw order=sequential")


def get_tests() -> List[Dict[str, Any]]:
    """Return list of engine option tests."""
    return [
        {
            "name": "engineoptions-each-option",
            "description": "--each option.Hash reaches every engine as a setoption command",
            "args": f"{_BASE} --each tc=0.2+0.01 option.Hash=64 trace=all "
                    "--engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--logging engine=true mode=one path=test/integration/log/engineoptions/each",
            "log_path": "test/integration/log/engineoptions/each",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": "setoption name Hash value 64",
                },
            ],
            "cleanup": "test/integration/log/engineoptions/each",
        },
        {
            "name": "engineoptions-engine-overrides-each",
            "description": "An inline --engine option beats the shared --each value for that engine only",
            "args": f"{_BASE} --each tc=0.2+0.01 option.Hash=64 trace=all "
                    "--engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' option.Hash=128 "
                    "--logging engine=true mode=one path=test/integration/log/engineoptions/precedence",
            "log_path": "test/integration/log/engineoptions/precedence",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    # The engine without an inline value keeps the --each default ...
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": "setoption name Hash value 64",
                },
                {
                    # ... while the overridden engine gets its own value.
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": "setoption name Hash value 128",
                },
            ],
            "cleanup": "test/integration/log/engineoptions/precedence",
        },
        {
            "name": "engineoptions-restart-always",
            "description": "restart=on restarts engines between games and records the reason",
            "args": f"{_BASE} --each tc=0.2+0.01 restart=on trace=all "
                    "--engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--logging engine=true mode=one path=test/integration/log/engineoptions/restart",
            "log_path": "test/integration/log/engineoptions/restart",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": r"Sending quit and restarting engine, reason: "
                               r"engine restart between games is configured \(restart = always\)",
                },
            ],
            "cleanup": "test/integration/log/engineoptions/restart",
        },
    ]
