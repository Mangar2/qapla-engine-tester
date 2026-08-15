#!/usr/bin/env python3
"""Interactive Tests - Validates the --interactive command loop.

The interactive command handler reads its commands from stdin, which the test
framework can feed via "input". The commands are sent up front, so they arrive
while the run is still starting: 'quit' therefore has to let the already
scheduled games finish, which is exactly the documented behaviour.
"""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of interactive mode tests."""
    return [
        {
            "name": "interactive-commands",
            "description": "Interactive mode accepts help, info, running, outcome, an unknown command and quit",
            "args": "--interactive=true --concurrency=2 "
                    "--enginesfile=test/integration/engines/engines.ini "
                    "--sprt maxgames=4 --openings file=test/opening/book8ply.raw order=sequential "
                    "--each tc=0.2+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--logging engine=false path=test/integration/log/interactive",
            "input": ["h", "?", "r", "o", "nosuchcommand", "q"],
            "log_path": "test/integration/log/interactive",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "stdout", "content": "Interactive mode! Enter h or help for help"},
                {"type": "stdout", "content": "Available commands:"},
                {"type": "stdout", "content": "Unknown command: nosuchcommand"},
                {
                    "type": "stdout",
                    "content": "Quit received, finishing all games and analyses before exiting.",
                },
            ],
            "cleanup": "test/integration/log/interactive",
        },
    ]
