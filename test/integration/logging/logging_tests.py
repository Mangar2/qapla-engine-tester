#!/usr/bin/env python3
"""Logging Tests - Validates global vs per-engine logging."""

from typing import List, Dict, Any

# All five fixtures below play diagnostic-engine-lossontime against itself under
# two identities; only the extra per-engine fields (trace=none) differ. The
# engines themselves come from engines.<os>.ini via engine_ini_block(), so the
# fixture ini files carry no engine data at all - see apply_engine_refs().
_SELF_PLAY = [
    {"name": "diagnostic-engine-lossontime", "as_": "diagnostic-engine-1", "tc": "inf"},
    {"name": "diagnostic-engine-lossontime", "as_": "diagnostic-engine-2", "tc": "inf"},
]
_SELF_PLAY_TRACE_NONE = [
    {"name": "diagnostic-engine-lossontime", "as_": "diagnostic-engine-1", "tc": "inf", "trace": "none"},
    {"name": "diagnostic-engine-lossontime", "as_": "diagnostic-engine-2", "tc": "inf", "trace": "none"},
]


def get_tests() -> List[Dict[str, Any]]:
    """Return list of logging tests."""
    return [
        {
            "name": "logging-global-single-file",
            "description": "SPRT with global logging - exactly one log file for all engines",
            "args": "--settingsfile=test/integration/logging/test-logging-global.ini",
            "log_path": "test/integration/log/logging-global",
            "engine_refs": _SELF_PLAY,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": "(bestmove|info depth)",
                },
            ],
            "cleanup": "test/integration/log/logging-global",
        },
        {
            "name": "logging-per-engine-multiple-files",
            "description": "SPRT with per-engine logging - separate log file per engine",
            "args": "--settingsfile=test/integration/logging/test-logging-per-engine.ini",
            "log_path": "test/integration/log/logging-per-engine",
            "engine_refs": _SELF_PLAY,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 2,
                    "content": "(bestmove|info depth)",
                },
            ],
            "cleanup": "test/integration/log/logging-per-engine",
        },
        {
            "name": "logging-disabled-no-files",
            "description": "SPRT with disabled engine logging - no engine log files",
            "args": "--settingsfile=test/integration/logging/test-logging-disabled.ini",
            "log_path": "test/integration/log/logging-disabled",
            "engine_refs": _SELF_PLAY,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "logFiles", "path": "", "pattern": "engine-*.log", "count": 0},
            ],
            "cleanup": "test/integration/log/logging-disabled",
        },
        {
            "name": "logging-engine-trace-none",
            "description": "SPRT with engine=true but per-engine trace=none - no engine log files",
            "args": "--settingsfile=test/integration/logging/test-logging-engine-trace-none.ini",
            "log_path": "test/integration/log/logging-engine-trace-none",
            "engine_refs": _SELF_PLAY_TRACE_NONE,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "logFiles", "path": "", "pattern": "engine-*.log", "count": 0},
            ],
            "cleanup": "test/integration/log/logging-engine-trace-none",
        },
        {
            "name": "logging-each-trace-none",
            "description": "SPRT with engine=true but [each] trace=none - no engine log files",
            "args": "--settingsfile=test/integration/logging/test-logging-each-trace-none.ini",
            "log_path": "test/integration/log/logging-each-trace-none",
            "engine_refs": _SELF_PLAY,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "logFiles", "path": "", "pattern": "engine-*.log", "count": 0},
            ],
            "cleanup": "test/integration/log/logging-each-trace-none",
        },
    ]
