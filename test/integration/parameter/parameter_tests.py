#!/usr/bin/env python3
"""Parameter Tests - Validates parameter handling."""

from typing import List, Dict, Any


def get_tests() -> List[Dict[str, Any]]:
    """Return list of parameter tests."""
    return [
        {
            "name": "parameter-concurrency-basic",
            "description": "SPRT with concurrency=1 - single game execution",
            "args": "--settingsfile=test/integration/parameter/test-parameter-concurrency.ini",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-missing-mandatory",
            "description": "SPRT with missing mandatory parameter (openings file)",
            "args": "--settingsfile=test/integration/parameter/test-parameter-missing-mandatory.ini",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 2}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-cmdline-override",
            "description": "SPRT with command line parameter overriding missing ini parameter",
            "args": "--settingsfile=test/integration/parameter/test-parameter-missing-mandatory.ini --openings file=test/opening/book8ply.raw",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-sprt-file",
            "description": "SPRT using sprt file with concurrency parameter",
            "args": "--concurrency=2 --sprt file=test/integration/log/parameter/test-parameter-sprt-file.qsprt",
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
                    "source": "test/integration/parameter/test-parameter-sprt-file.qsprt",
                    "target": "test/integration/log/parameter/test-parameter-sprt-file.qsprt"
                }
            ],
        },
        {
            "name": "parameter-global-fail",
            "description": "SPRT with illegal global parameter in ini file",
            "args": "--settingsfile=test/integration/parameter/test-parameter-illegal-global.ini",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 2}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-global-override",
            "description": "SPRT with illegal global parameter in ini file overridden by cli",
            "args": "--settingsfile=test/integration/parameter/test-parameter-illegal-global.ini --concurrency=1",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-group-fail",
            "description": "SPRT with illegal group parameter in ini file",
            "args": "--settingsfile=test/integration/parameter/test-parameter-illegal-group.ini",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 2}],
            "cleanup": "test/integration/log/parameter",
        },
        {
            "name": "parameter-group-override",
            "description": "SPRT with illegal group parameter in ini file overridden by cli",
            "args": "--settingsfile=test/integration/parameter/test-parameter-illegal-group.ini --openings order=sequential",
            "log_path": "test/integration/log/parameter",
            "validators": [{"type": "exitCode", "expected": 16}],
            "cleanup": "test/integration/log/parameter",
        },
    ]
