#!/usr/bin/env python3
"""Book Evaluation Tests - Validates --bookeval, the search of every position of a 9 bit book.

book8ply-30.bok holds the first 30 openings of test/opening/book8ply.raw, made with gamefile of
qapla-chess-tools (--pgn2book in=book8ply.raw maxgames=30): 127 moves, played from 98 positions -
the start position and every position after a move with moves below it. Version 2, the default,
searches all 128 positions: the start position and the one after each of the 127 moves.
"""

from typing import Any, Dict, List

_LOG = "test/integration/log/bookeval"
_BOOK = "test/integration/bookeval/book8ply-30.bok"
_BASE = ("--concurrency=4 --enginesfile=test/integration/engines/engines.ini "
         "--engine conf='Qapla 0.4.0' --each tc=depth:4")


def get_tests() -> List[Dict[str, Any]]:
    """Return list of book evaluation tests."""
    return [
        {
            "name": "bookeval-whole-book",
            "description": "Every position a move of the book is played from is searched once, cold",
            "args": f"{_BASE} trace=all --bookeval book={_BOOK} out={_LOG}/whole/out.bok "
                    f"--logging engine=true mode=one path={_LOG}/whole",
            "log_path": f"{_LOG}/whole",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Positions:           128"},
                {"type": "stdout", "content": "bookeval complete: 128 positions evaluated"},
                {"type": "fileExists", "path": f"{_LOG}/whole/out.bok"},
                {
                    # A new game before every position, then the moves from the start position:
                    # the search does not depend on what the engine searched before.
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": r"ucinewgame\s*\n(?:.*\n){0,3}?.*position startpos moves e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4\n",
                    "isRegex": True,
                    "message": "A book position was not searched from a new game with its line",
                },
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": "go depth 4",
                    "message": "The engine was not given the limit of its time control",
                },
            ],
            "cleanup": f"{_LOG}/whole",
        },
        {
            "name": "bookeval-stops-and-continues",
            "description": "A run stopped by maxpositions is continued by the next one with the same out",
            "setup_args": f"{_BASE} --bookeval book={_BOOK} out={_LOG}/continue/out.bok maxpositions=40 "
                          f"--logging engine=false path={_LOG}/continue",
            "args": f"{_BASE} --bookeval book={_BOOK} out={_LOG}/continue/out.bok "
                    f"--logging engine=false path={_LOG}/continue",
            "log_path": f"{_LOG}/continue",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Open:                88"},
                {"type": "stdout", "content": "bookeval complete: 128 positions evaluated"},
            ],
            "cleanup": f"{_LOG}/continue",
        },
        {
            "name": "bookeval-stopped-early",
            "description": "A run that leaves positions open says so and returns UndefinedResult",
            "args": f"{_BASE} --bookeval book={_BOOK} out={_LOG}/early/out.bok maxpositions=40 "
                    f"--logging engine=false path={_LOG}/early",
            "log_path": f"{_LOG}/early",
            "validators": [
                {"type": "exitCode", "expected": 16},
                {"type": "stdout", "content": "bookeval stopped with 40 of 128 positions evaluated"},
                {"type": "fileExists", "path": f"{_LOG}/early/out.bok"},
            ],
            "cleanup": f"{_LOG}/early",
        },
        {
            "name": "bookeval-version-1",
            "description": "version=1 searches the 98 positions a move is played from",
            "args": f"{_BASE} --bookeval book={_BOOK} out={_LOG}/v1/out.bok version=1 "
                    f"--logging engine=false path={_LOG}/v1",
            "log_path": f"{_LOG}/v1",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "bookeval complete: 98 positions evaluated"},
            ],
            "cleanup": f"{_LOG}/v1",
        },
        {
            "name": "bookeval-version-2-from-version-1",
            "description": "A version 1 book of the same search brings its 98 scores, only the 30 leaves are searched",
            "setup_args": f"{_BASE} --bookeval book={_BOOK} out={_LOG}/reuse/v1.bok version=1 "
                          f"--logging engine=false path={_LOG}/reuse",
            "args": f"{_BASE} --bookeval book={_LOG}/reuse/v1.bok out={_LOG}/reuse/v2.bok "
                    f"--logging engine=false path={_LOG}/reuse",
            "log_path": f"{_LOG}/reuse",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Taken from the book: 98"},
                {"type": "stdout", "content": "Open:                30"},
                {"type": "stdout", "content": "bookeval complete: 128 positions evaluated"},
            ],
            "cleanup": f"{_LOG}/reuse",
        },
        {
            "name": "bookeval-needs-per-move-limit",
            "description": "A game clock is no limit for a single search and is refused",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini "
                    f"--engine conf='Qapla 0.4.0' --each tc=10+0.1 --bookeval book={_BOOK} "
                    f"out={_LOG}/clock/out.bok --logging engine=false path={_LOG}/clock",
            "log_path": f"{_LOG}/clock",
            "validators": [
                {"type": "exitCode", "expected": 2},
                {"type": "stdout", "content": "Set a per-move search limit"},
            ],
            "cleanup": f"{_LOG}/clock",
        },
        {
            "name": "bookeval-one-engine",
            "description": "Two engines are refused: one engine searches the whole book",
            "args": f"{_BASE} --engine conf='Qapla 0.3.2' --bookeval book={_BOOK} "
                    f"out={_LOG}/two/out.bok --logging engine=false path={_LOG}/two",
            "log_path": f"{_LOG}/two",
            "validators": [
                {"type": "exitCode", "expected": 2},
                {"type": "stdout", "content": "Define exactly one engine"},
            ],
            "cleanup": f"{_LOG}/two",
        },
    ]
