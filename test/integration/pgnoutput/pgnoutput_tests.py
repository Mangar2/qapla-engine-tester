#!/usr/bin/env python3
"""PGN Output Tests - Validates --pgnoutput parameter group."""

from typing import Any, Dict, List

# The annotation tests below run their own short SPRT instead of reusing the
# maxgames fixture: two games at a fast time control are enough to inspect the
# written PGN, and keep each test at a few seconds.
_SHORT_SPRT = ("--concurrency=2 --enginesfile=test/integration/engines/engines.ini "
               "--sprt maxgames=2 --openings file=test/opening/book8ply.raw order=sequential "
               "--each tc=0.2+0.01 --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2'")


def get_tests() -> List[Dict[str, Any]]:
    """Return list of PGN output tests."""
    return [
        {
            "name": "pgnoutput-append",
            "description": "PGN output with append=true - original content must be preserved at start of file",
            "args": "--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --pgnoutput file=test/integration/log/pgnoutput/games.pgn append=true --logging engine=false path=test/integration/log/pgnoutput",
            "log_path": "test/integration/log/pgnoutput",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "fileAppendOnly", "path": "test/integration/log/pgnoutput/games.pgn"},
            ],
            "cleanup": "test/integration/log/pgnoutput",
            "source_files": [
                {
                    "source": "test/integration/pgnoutput/pgnoutput-append.pgn",
                    "target": "test/integration/log/pgnoutput/games.pgn",
                }
            ],
        },
        {
            "name": "pgnoutput-minimal-tags",
            "description": "min=true with all annotations off - no extended tags, no move comments",
            "args": f"{_SHORT_SPRT} "
                    "--pgnoutput file=test/integration/log/pgnoutput/min/min.pgn "
                    "min=true clock=false eval=false depth=false pv=false "
                    "--logging engine=false path=test/integration/log/pgnoutput/min",
            "log_path": "test/integration/log/pgnoutput/min",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/min/min.pgn",
                    "content": "[Event \"Sprt\"]",
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/min/min.pgn",
                    "content": r"(?s)^(?:(?!\[PlyCount).)*$",
                    "isRegex": True,
                    "message": "min=true must drop the extended tag set",
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/min/min.pgn",
                    "content": r"(?s)^(?:(?!\{[+-][0-9]).)*$",
                    "isRegex": True,
                    "message": "Move comments present although eval, depth, clock and pv are off",
                },
            ],
            "cleanup": "test/integration/log/pgnoutput/min",
        },
        {
            "name": "pgnoutput-full-annotations",
            "description": "eval, depth, clock and pv enabled - full move comments are written",
            "args": f"{_SHORT_SPRT} "
                    "--pgnoutput file=test/integration/log/pgnoutput/full/full.pgn "
                    "min=false clock=true eval=true depth=true pv=true "
                    "--logging engine=false path=test/integration/log/pgnoutput/full",
            "log_path": "test/integration/log/pgnoutput/full",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/full/full.pgn",
                    "content": "[PlyCount ",
                },
                {
                    # score/depth, elapsed time, and a principal variation of at
                    # least two moves: {+0.51/7 0.05s b1c3 e7e5 ...}
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/full/full.pgn",
                    "content": r"\{[+-][0-9]+\.[0-9]+/[0-9]+ [0-9]+\.[0-9]+s ([a-h][1-8][a-h][1-8][qrbn]? ){2,}",
                    "isRegex": True,
                    "message": "Comment does not carry eval, depth, clock and principal variation",
                },
            ],
            "cleanup": "test/integration/log/pgnoutput/full",
        },
    ]
