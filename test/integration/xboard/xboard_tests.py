#!/usr/bin/env python3
"""XBoard Tests - Validates WinBoard/XBoard protocol support.

Every other test in the suite runs UCI, so the WinBoard adapter is otherwise
never exercised. The Qapla engines speak both protocols, so the protocol is set
per engine here (an inline --engine value overrides the proto=uci that
engines.ini declares); the mixed test needs exactly that granularity.
"""

from typing import Any, Dict, List

_ENGINES = "--enginesfile=test/integration/engines/engines.ini"
_OPENINGS = "--openings file=test/opening/book8ply.raw order=sequential"


def get_tests() -> List[Dict[str, Any]]:
    """Return list of XBoard protocol tests."""
    return [
        {
            "name": "xboard-sprt-two-engines",
            "description": "SPRT between two XBoard engines - exercises the WinBoard adapter end to end",
            "args": f"--concurrency=2 {_ENGINES} --sprt maxgames=2 {_OPENINGS} "
                    "--each tc=0.2+0.01 "
                    "--engine conf='Qapla 0.4.0' proto=xboard "
                    "--engine conf='Qapla 0.3.2' proto=xboard "
                    "--logging engine=false path=test/integration/log/xboard/sprt",
            "log_path": "test/integration/log/xboard/sprt",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "stdout", "content": "sprt all games completed"},
            ],
            "cleanup": "test/integration/log/xboard/sprt",
        },
        {
            "name": "xboard-mixed-protocols",
            "description": "XBoard engine against UCI engine - move and clock translation across adapters",
            "args": f"--concurrency=2 {_ENGINES} --sprt maxgames=2 {_OPENINGS} "
                    "--each tc=0.2+0.01 "
                    "--engine conf='Qapla 0.4.0' proto=xboard "
                    "--engine conf='Qapla 0.3.2' "
                    "--logging engine=false path=test/integration/log/xboard/mixed",
            "log_path": "test/integration/log/xboard/mixed",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "stdout", "content": "sprt all games completed"},
                {"type": "logFiles", "path": "", "pattern": "sprt-report-*.log", "count": 1},
            ],
            "cleanup": "test/integration/log/xboard/mixed",
        },
        {
            "name": "xboard-engine-test",
            "description": "Engine test suite against an XBoard engine - go limits and FEN positions",
            "args": "--settingsfile=test/integration/engine-test/test-engine-none.ini "
                    "--test nogolimits=false nofens=false "
                    "--engine conf='Qapla 0.4.0' proto=xboard "
                    "--logging path=test/integration/log/xboard/enginetest",
            "log_path": "test/integration/log/xboard/enginetest",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-report-*.log",
                    "count": 1,
                    "content": "PASS",
                },
            ],
            "cleanup": "test/integration/log/xboard/enginetest",
        },
    ]
