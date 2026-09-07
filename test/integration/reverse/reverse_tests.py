#!/usr/bin/env python3
"""Reverse Analysis Tests - Validates --reverse, the backward recomputation of PGN games."""

from typing import Any, Dict, List

_ENGINES = "--enginesfile=test/integration/engines/engines.ini"
# Two short decided games: 7 half moves and 4 half moves, both from the start position. Short
# enough that the whole run stays under a second, long enough to show the walk going backwards.
_GAMES = "test/integration/reverse/games.pgn"


def get_tests() -> List[Dict[str, Any]]:
    """Return list of reverse analysis tests."""
    return [
        {
            "name": "reverse-backward-walk",
            "description": "Both games are recomputed from their last move back to their first, every half move evaluated",
            "args": f"--concurrency=1 {_ENGINES} --reverse file={_GAMES} movetime=50 "
                    "--engine conf='Qapla 0.4.0' --each trace=all "
                    "--logging engine=true mode=one path=test/integration/log/reverse",
            "log_path": "test/integration/log/reverse",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    # "evaluated" counts the half moves that carry a score afterwards. It must
                    # reach the move count: the first move is the one the walk ends on and the
                    # one the whole exercise is about, and it used to be skipped.
                    "type": "stdout",
                    "content": "reverse game 1 moves 7 evaluated 7",
                },
                {
                    "type": "stdout",
                    "content": "reverse game 2 moves 4 evaluated 4",
                },
                {"type": "stdout", "content": "reverse analysis finished: 2 of 2 games analysed"},
                {
                    # The move list the engine is given shrinks one move at a time, from the
                    # position before the last move down to the bare start position.
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": (r"(?s)position startpos moves e2e4 e7e5 f1c4 b8c6 d1h5 g8f6"
                                r".*position startpos moves e2e4 e7e5 f1c4 b8c6 d1h5"
                                r".*position startpos moves e2e4\n"
                                r".*position startpos\n"),
                    "message": "The game was not recomputed backwards down to its first move",
                },
                {
                    # A fixed time per move, and no clock: a reverse analysis has no game time.
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": r"(?s)go movetime 50(?!.*go wtime)",
                    "message": "The engine was given a clock instead of a fixed time per move",
                },
            ],
            "cleanup": "test/integration/log/reverse",
        },
    ]
