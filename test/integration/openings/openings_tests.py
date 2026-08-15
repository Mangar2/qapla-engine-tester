#!/usr/bin/env python3
"""Opening Tests - Validates --openings book formats, order, start index and policies.

Every other game based test uses the same combination (a .raw book read
sequentially from position 1), so neither the PGN reader, the EPD reader, random
order, the start index nor the switch policies are covered anywhere else.
"""

from typing import Any, Dict, List

_LOG = "test/integration/log/openings"
_BASE = ("--concurrency=2 --enginesfile=test/integration/engines/engines.ini "
         "--sprt maxgames=2 --each tc=0.2+0.01 "
         "--engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2'")

# First line of book8ply.raw. Used as a negative marker: a randomly ordered run
# must not start with the position a sequential run would have picked.
_FIRST_SEQUENTIAL_FEN = "rnbqkb1r/pp2pppp/3p1n2/8/3NP3/8/PPP2PPP/RNBQKB1R w KQkq - 1 5"
# Line 100 of book8ply.raw, the opening a run with start=100 must begin from.
_FEN_AT_INDEX_100 = "rnbqkb1r/pp2pppp/5n2/3p4/3P4/2N5/PP2PPPP/R1BQKBNR w KQkq -"


def get_tests() -> List[Dict[str, Any]]:
    """Return list of opening selection tests."""
    return [
        {
            "name": "openings-pgn-book",
            "description": "PGN opening book with plies=8 - games start from the replayed book moves",
            "args": f"{_BASE} --openings file=test/opening/Noomen.pgn order=sequential plies=8 "
                    f"--pgnoutput file={_LOG}/pgnbook.pgn "
                    f"--logging engine=false path={_LOG}",
            "log_path": _LOG,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": f"{_LOG}/pgnbook.pgn",
                    # Exactly the first 8 plies of the book's first game, played out
                    # without an engine comment - i.e. the book was replayed, and
                    # plies=8 cut it at the right move.
                    "content": r"(?m)^1\. d4 d5 2\. c4 e6 3\. Nc3 Nf6 4\. Nf3 Nc6 \{",
                    "isRegex": True,
                    "message": "Games did not start from the PGN book's first 8 plies",
                },
            ],
            "cleanup": _LOG,
        },
        {
            "name": "openings-epd-book-black-to-move",
            "description": "EPD opening book with Black to move - verifies clocks are assigned to the right side",
            "args": f"{_BASE} --openings file=test/integration/epd/short.epd order=sequential "
                    f"--pgnoutput file={_LOG}/epdbook.pgn "
                    f"--logging engine=false path={_LOG}",
            "log_path": _LOG,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": f"{_LOG}/epdbook.pgn",
                    "content": r"\[FEN \"[^\"]* b ",
                    "isRegex": True,
                    "message": "No Black-to-move start position was taken from the EPD book",
                },
                {
                    "type": "fileContent",
                    "path": f"{_LOG}/epdbook.pgn",
                    "content": r"(?s)^(?:(?!time forfeit).)*$",
                    "isRegex": True,
                    "message": "Loss on time indicates the clocks were assigned to the wrong side",
                },
            ],
            "cleanup": _LOG,
        },
        {
            "name": "openings-random-seeded",
            "description": "Random opening order with a fixed seed and policy=encounter",
            "args": f"{_BASE} --openings file=test/opening/book8ply.raw order=random srand=4711 policy=encounter "
                    f"--pgnoutput file={_LOG}/random.pgn "
                    f"--logging engine=false path={_LOG}",
            "log_path": _LOG,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "fileContent", "path": f"{_LOG}/random.pgn", "content": "[SetUp \"1\"]"},
                {
                    "type": "fileContent",
                    "path": f"{_LOG}/random.pgn",
                    "content": r"(?s)^(?:(?!" + _FIRST_SEQUENTIAL_FEN + r").)*$",
                    "isRegex": True,
                    "message": "Random order still produced the first sequential opening",
                },
            ],
            "cleanup": _LOG,
        },
        {
            "name": "openings-start-index",
            "description": "Sequential book read from start=100 - games begin at that opening",
            "args": f"{_BASE} --openings file=test/opening/book8ply.raw order=sequential start=100 "
                    f"--pgnoutput file={_LOG}/startindex.pgn "
                    f"--logging engine=false path={_LOG}",
            "log_path": _LOG,
            "validators": [
                {"type": "exitCode", "expected": 16},
                {
                    "type": "fileContent",
                    "path": f"{_LOG}/startindex.pgn",
                    "content": f"[FEN \"{_FEN_AT_INDEX_100}",
                    "message": "Run did not start at opening number 100",
                },
            ],
            "cleanup": _LOG,
        },
        {
            "name": "openings-invalid-policy",
            "description": "Unknown opening switch policy is rejected before any engine starts",
            "args": f"{_BASE} --openings file=test/opening/book8ply.raw policy=bogus "
                    f"--logging path={_LOG}",
            "log_path": _LOG,
            "validators": [
                {"type": "exitCode", "expected": 2},
                {"type": "stdout", "content": "Unsupported openings policy: bogus"},
            ],
            "cleanup": _LOG,
        },
    ]
