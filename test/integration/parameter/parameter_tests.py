#!/usr/bin/env python3
"""Parameter Tests - Validates parameter handling."""

from typing import List, Dict, Any

from test_framework import platform_suffix

# The saved-state fixture below embeds an engine cmd= path, so it needs an
# OS-specific variant just like the engines files (see test_framework.py).
_SPRT_FILE_SOURCE = f"test/integration/parameter/test-parameter-sprt-file.{platform_suffix()}.qsprt"

# All the plain-ini fixtures below just need two identities of the same
# self-play engine; see engine_ini_block() in test_framework.py.
_SELF_PLAY = [
    {"name": "diagnostic-engine-lossontime", "as_": "diagnostic-engine-1"},
    {"name": "diagnostic-engine-lossontime", "as_": "diagnostic-engine-2"},
]


def get_tests() -> List[Dict[str, Any]]:
    """Return list of parameter tests."""
    return [
        {
            "name": "parameter-concurrency-basic",
            "description": "SPRT with concurrency=1 - single game execution",
            "args": "--settingsfile=test/integration/parameter/test-parameter-concurrency.ini",
            "engine_refs": _SELF_PLAY,
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-missing-mandatory",
            "description": "SPRT with missing mandatory parameter (openings file)",
            "args": "--settingsfile=test/integration/parameter/test-parameter-missing-mandatory.ini",
            "engine_refs": _SELF_PLAY,
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 2}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-cmdline-override",
            "description": "SPRT with command line parameter overriding missing ini parameter",
            "args": "--settingsfile=test/integration/parameter/test-parameter-missing-mandatory.ini --openings file=test/opening/book8ply.raw",
            "engine_refs": _SELF_PLAY,
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-sprt-file",
            "description": "SPRT using sprt file with concurrency parameter",
            # The .qsprt fixture carries no [logging] section, so without an explicit
            # path the run drops its report logs into the repository's own log/ directory.
            "args": "--concurrency=2 --sprt file=test/integration/log/parameter/test-parameter-sprt-file.qsprt "
                    "--logging path=test/integration/log/parameter",
            "log_path": "test/integration/log/parameter",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    # The state file is rewritten on save, so its previous content cannot be
                    # compared byte by byte. What must survive is the stored game result.
                    "type": "fileContent",
                    "path": "test/integration/log/parameter/test-parameter-sprt-file.qsprt",
                    "content": "games=01",
                    "message": "Game results of the sprt file were lost."
                },
            ],
            "cleanup": "test/integration/log/parameter",
            "source_files": [
                {
                    "source": _SPRT_FILE_SOURCE,
                    "target": "test/integration/log/parameter/test-parameter-sprt-file.qsprt"
                }
            ],
        },
        {
            "name": "parameter-global-fail",
            "description": "SPRT with illegal global parameter in ini file",
            "args": "--settingsfile=test/integration/parameter/test-parameter-illegal-global.ini",
            "engine_refs": _SELF_PLAY,
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 2}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-global-override",
            "description": "SPRT with illegal global parameter in ini file overridden by cli",
            "args": "--settingsfile=test/integration/parameter/test-parameter-illegal-global.ini --concurrency=1",
            "engine_refs": _SELF_PLAY,
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-group-fail",
            "description": "SPRT with illegal group parameter in ini file",
            "args": "--settingsfile=test/integration/parameter/test-parameter-illegal-group.ini",
            "engine_refs": _SELF_PLAY,
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 2}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-group-override",
            "description": "SPRT with illegal group parameter in ini file overridden by cli",
            "args": "--settingsfile=test/integration/parameter/test-parameter-illegal-group.ini --openings order=sequential",
            "engine_refs": _SELF_PLAY,
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/parameter",
        },
    ]
