#!/usr/bin/env python3
"""Analysis Tests - Validates --analysis, the recomputation of PGN games by an engine."""

from typing import Any, Dict, List

_ENGINES = "--enginesfile=test/integration/engines/engines.ini"
# Two short decided games: 7 half moves and 4 half moves, both from the start position, with
# players that are not engines of this repository - that is what makes it visible whether the
# analysis keeps the original players. Short enough that a whole run stays under a second.
_GAMES = "test/integration/analysis/games.pgn"
# 16 games of a public tournament collection, in the format such collections publish:
# wrapped lines, comments holding principal variations, no Event tag.
_CCRL = "test/integration/analysis/ccrl-126th-amateur-qapla.pgn"
_MOVETIME = '--each "tc=movetime(ms):50" trace=all'


def get_tests() -> List[Dict[str, Any]]:
    """Return list of analysis tests."""
    return [
        {
            "name": "analysis-reverse-walks-back-to-the-first-move",
            "description": "Both games are recomputed from their last move back to their first, every half move evaluated",
            "args": f"--concurrency=1 {_ENGINES} --analysis pgn={_GAMES} "
                    f"--engine conf='Qapla 0.4.0' {_MOVETIME} "
                    "--pgnoutput file=test/integration/log/analysis/reverse/out.pgn append=false "
                    "--logging engine=true mode=one path=test/integration/log/analysis/reverse",
            "log_path": "test/integration/log/analysis/reverse",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    # "evaluated" counts the half moves that carry a score afterwards. It must
                    # reach the move count: the first move is the one the walk ends on and the
                    # one the whole exercise is about, and it used to be skipped.
                    "type": "stdout",
                    "content": "analysis game 1 moves 7 evaluated 7",
                },
                {"type": "stdout", "content": "analysis game 2 moves 4 evaluated 4"},
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
                    # A fixed limit per move, and no clock: an analysis has no game time.
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": r"(?s)go movetime 50(?!.*go wtime)",
                    "message": "The engine was given a clock instead of a fixed limit per move",
                },
                {
                    # Playing a game replaces the players with the engines; an analysis must give
                    # them back, they are part of what is being analysed.
                    "type": "fileContent",
                    "path": "test/integration/log/analysis/reverse/out.pgn",
                    "content": r"(?s)\[White \"White Player\"\].*\[Black \"Black Player\"\]",
                    "isRegex": True,
                    "message": "The players of the analysed game were replaced by the engine",
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/analysis/reverse/out.pgn",
                    "content": "[Annotator \"Qapla 0.4.0\"]",
                },
                {
                    # The evaluations are what the run is for, and they belong to the first move
                    # as much as to any other.
                    "type": "fileContent",
                    "path": "test/integration/log/analysis/reverse/out.pgn",
                    "content": r"1\. e4 \{[-+]?\d+\.\d+/\d+",
                    "isRegex": True,
                    "message": "The written game carries no evaluation on its first move",
                },
            ],
            "cleanup": "test/integration/log/analysis/reverse",
        },
        {
            "name": "analysis-forward-follows-the-playing-order",
            "description": "direction=forward recomputes a game in the order it was played",
            "args": f"--concurrency=1 {_ENGINES} --analysis pgn={_GAMES} direction=forward "
                    f"--engine conf='Qapla 0.4.0' {_MOVETIME} "
                    "--logging engine=true mode=one path=test/integration/log/analysis/forward",
            "log_path": "test/integration/log/analysis/forward",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "analysis game 1 moves 7 evaluated 7"},
                {"type": "stdout", "content": "analysis game 2 moves 4 evaluated 4"},
                {
                    # The mirror image of the backward walk: the move list grows.
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-*.log",
                    "count": 1,
                    "content": (r"(?s)position startpos\n"
                                r".*position startpos moves e2e4\n"
                                r".*position startpos moves e2e4 e7e5 f1c4 b8c6 d1h5 g8f6"),
                    "message": "The game was not recomputed in playing order",
                },
            ],
            "cleanup": "test/integration/log/analysis/forward",
        },
        {
            "name": "analysis-every-engine-analyses-the-file",
            "description": "Two engines analyse the same games in turn; both write their own copy",
            "args": f"--concurrency=2 {_ENGINES} --analysis pgn={_GAMES} "
                    "--engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    '--each "tc=movetime(ms):50" '
                    "--pgnoutput file=test/integration/log/analysis/engines/out.pgn append=false "
                    "--logging engine=false path=test/integration/log/analysis/engines",
            "log_path": "test/integration/log/analysis/engines",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "analysis with Qapla 0.4.0 finished: 2 of 2 games analysed"},
                {"type": "stdout", "content": "analysis with Qapla 0.3.2 finished: 2 of 2 games analysed"},
                {
                    # Four games in the file, and each says which engine judged it - without that
                    # the two runs would be indistinguishable.
                    "type": "fileContent",
                    "path": "test/integration/log/analysis/engines/out.pgn",
                    "content": r"(?s)\[Annotator \"Qapla 0\.4\.0\"\].*\[Annotator \"Qapla 0\.3\.2\"\]",
                    "isRegex": True,
                    "message": "The games are not attributed to the engine that analysed them",
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/analysis/engines/out.pgn",
                    "content": r"(?s)(\[White \"White Player\"\][\s\S]*){4}",
                    "isRegex": True,
                    "message": "The second engine did not write its own copy of every game",
                },
            ],
            "cleanup": "test/integration/log/analysis/engines",
        },
        {
            "name": "analysis-reads-real-world-pgn-reverse",
            "description": "16 published games with wrapped multi-line comments, recomputed backwards in full",
            # Real files from a public tournament collection: the comments carry principal
            # variations and run over several lines, and every second line starts with the tail
            # or the whole of one. Both used to end a game after a few moves - which is only
            # visible against files like these, not against a fixture written here.
            "args": f"--concurrency=16 {_ENGINES} --analysis pgn={_CCRL} "
                    '--engine conf=\'Qapla 0.4.0\' --each "tc=movetime(ms):100" '
                    "--pgnoutput file=test/integration/log/analysis/ccrl/out.pgn append=false "
                    "--logging engine=false path=test/integration/log/analysis/ccrl",
            "log_path": "test/integration/log/analysis/ccrl",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "analysis with Qapla 0.4.0 finished: 16 of 16 games analysed"},
                {
                    # The longest game of the file, read to its last half move. A game cut short
                    # by a comment came out with 16 of these 264.
                    "type": "stdout",
                    "content": "analysis game 1 moves 264 evaluated 264",
                },
                {
                    "type": "stdout",
                    "content": r"(?s)^(?:(?!evaluated 0 ).)*$",
                    "isRegex": True,
                    "message": "A game was analysed without producing a single evaluation",
                },
                {
                    # Every game written once, by the engine that judged it.
                    "type": "fileContent",
                    "path": "test/integration/log/analysis/ccrl/out.pgn",
                    "content": r"(?s)(\[Annotator \"Qapla 0\.4\.0\"\][\s\S]*){16}",
                    "isRegex": True,
                    "message": "The written file does not hold all 16 analysed games",
                },
                {
                    # Tags of the source file survive the analysis - they say which games these are.
                    "type": "fileContent",
                    "path": "test/integration/log/analysis/ccrl/out.pgn",
                    "content": "[Site \"126th Amateur D12\"]",
                },
            ],
            "cleanup": "test/integration/log/analysis/ccrl",
        },
        {
            "name": "analysis-reads-real-world-pgn-forward",
            "description": "The same 16 published games recomputed in playing order",
            "args": f"--concurrency=16 {_ENGINES} --analysis pgn={_CCRL} direction=forward "
                    '--engine conf=\'Qapla 0.4.0\' --each "tc=movetime(ms):100" '
                    "--logging engine=false path=test/integration/log/analysis/ccrl-forward",
            "log_path": "test/integration/log/analysis/ccrl-forward",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "analysis with Qapla 0.4.0 finished: 16 of 16 games analysed"},
                {"type": "stdout", "content": "analysis game 1 moves 264 evaluated 264"},
                {
                    "type": "stdout",
                    "content": r"(?s)^(?:(?!evaluated 0 ).)*$",
                    "isRegex": True,
                    "message": "A game was analysed without producing a single evaluation",
                },
            ],
            "cleanup": "test/integration/log/analysis/ccrl-forward",
        },
        {
            "name": "analysis-rejects-a-game-clock",
            "description": "A clock time control is refused before any engine starts, saying what to set instead",
            "args": f"--concurrency=1 {_ENGINES} --analysis pgn={_GAMES} "
                    "--engine conf='Qapla 0.4.0' --each tc=3+0.02 "
                    "--logging path=test/integration/log/analysis/clock",
            "log_path": "test/integration/log/analysis/clock",
            "validators": [
                {"type": "exitCode", "expected": 2},
                {
                    # The message has to say what to do, not only what is wrong.
                    "type": "stdout",
                    "content": "Set a per-move search limit on engine 'Qapla 0.4.0'",
                },
                {"type": "stdout", "content": "tc=movetime(ms):500"},
            ],
            "cleanup": "test/integration/log/analysis/clock",
        },
    ]
