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
                    # Confirmed intended: a minimal PGN keeps exactly White, Black,
                    # FEN, SetUp and Event.
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
                    # least two moves, in brackets: {+0.51/7 0.05s (b1c3 e7e5 ...)}
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/full/full.pgn",
                    "content": r"\{[+-][0-9]+\.[0-9]+/[0-9]+ [0-9]+\.[0-9]+s \(([a-h][1-8][a-h][1-8][qrbn]? ?){2,}\)",
                    "isRegex": True,
                    "message": "Comment does not carry eval, depth, clock and principal variation",
                },
            ],
            "cleanup": "test/integration/log/pgnoutput/full",
        },
        {
            "name": "pgnoutput-lan",
            "description": "notation=lan writes the moves as e2e4 instead of e4",
            "args": f"{_SHORT_SPRT} "
                    "--pgnoutput file=test/integration/log/pgnoutput/lan/lan.pgn notation=lan "
                    "--logging engine=false path=test/integration/log/pgnoutput/lan",
            "log_path": "test/integration/log/pgnoutput/lan",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/lan/lan.pgn",
                    "content": r"(?m)^[0-9]+\. [a-h][1-8][a-h][1-8] ",
                    "isRegex": True,
                    "message": "No move written in LAN",
                },
                {
                    # Outside the tag lines no piece letter and no castling may appear: the
                    # movetext carries only coordinates, and pv is off, so the comments hold none.
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/lan/lan.pgn",
                    "content": r"(?m)\A(?![\s\S]*^[^\[\n]*(?:[NBRQK][a-h1-8]?x?[a-h][1-8]|O-O))",
                    "isRegex": True,
                    "message": "A move was written in SAN although notation=lan",
                },
            ],
            "cleanup": "test/integration/log/pgnoutput/lan",
        },
        {
            "name": "pgnoutput-notation-invalid",
            "description": "A notation other than san or lan is rejected before any engine starts",
            "args": f"{_SHORT_SPRT} "
                    "--pgnoutput file=test/integration/log/pgnoutput/badnotation/games.pgn notation=uci "
                    "--logging engine=false path=test/integration/log/pgnoutput/badnotation",
            "log_path": "test/integration/log/pgnoutput/badnotation",
            "validators": [
                {"type": "exitCode", "expected": 2},
                {"type": "stdout", "content": "Set pgnoutput.notation to 'san' or 'lan'; 'uci' is neither."},
            ],
            "cleanup": "test/integration/log/pgnoutput/badnotation",
        },
        {
            "name": "pgnoutput-per-round",
            "description": "perround=true writes every round to a file of its own, holding that round's games",
            # Two engines, two games a pairing, three rounds: six games, two in each file.
            "args": "--concurrency=2 --enginesfile=test/integration/engines/engines.ini "
                    "--tournament type=round-robin games=2 rounds=3 "
                    "--openings file=test/opening/book8ply.raw order=sequential --each tc=depth:3 "
                    "--engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--pgnoutput file=test/integration/log/pgnoutput/perround/games.pgn perround=true "
                    "--logging engine=false path=test/integration/log/pgnoutput/perround",
            "log_path": "test/integration/log/pgnoutput/perround",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/perround/games-round-001.pgn",
                    "content": r'(?s)\A(?=.*\[Round "1"\])(?=.*\[Round "2"\])(?:(?!\[Round "[3-6]"\]).)*\Z',
                    "isRegex": True,
                    "message": "The file of round 1 does not hold exactly the games of round 1",
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/perround/games-round-002.pgn",
                    "content": r'(?s)\A(?=.*\[Round "3"\])(?=.*\[Round "4"\])(?:(?!\[Round "[1256]"\]).)*\Z',
                    "isRegex": True,
                    "message": "The file of round 2 does not hold exactly the games of round 2",
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/perround/games-round-003.pgn",
                    "content": r'(?s)\A(?=.*\[Round "5"\])(?=.*\[Round "6"\])(?:(?!\[Round "[1-4]"\]).)*\Z',
                    "isRegex": True,
                    "message": "The file of round 3 does not hold exactly the games of round 3",
                },
                {
                    # Overwrite mode empties a round's file before its first game, the way it
                    # empties the single file: nothing of an earlier run is left in it.
                    "type": "fileContent",
                    "path": "test/integration/log/pgnoutput/perround/games-round-001.pgn",
                    "content": r"(?s)^(?:(?!Left over).)*$",
                    "isRegex": True,
                    "message": "A round's file still holds a game of an earlier run",
                },
            ],
            "cleanup": "test/integration/log/pgnoutput/perround",
            "source_files": [
                {
                    "source": "test/integration/pgnoutput/pgnoutput-perround-old.pgn",
                    "target": "test/integration/log/pgnoutput/perround/games-round-001.pgn",
                }
            ],
        },
    ]
