#!/usr/bin/env python3
"""Tournament Tests - Validates Tournament functionality."""

from typing import List, Dict, Any

from test_framework import platform_suffix

# The saved-state fixture embeds an engine cmd= path, so it needs an OS-specific
# variant just like the engines files (see test_framework.py). It holds a
# finished round 1 while its [tournament] section asks for two rounds, so a
# resume has exactly one round left to play.
_TOUR_FILE_SOURCE = f"test/integration/tournament/test-tournament-continue.{platform_suffix()}.qtour"

_ENGINES = "--enginesfile=test/integration/engines/engines.ini"
_OPENINGS = "--openings file=test/opening/book8ply.raw order=sequential"


def get_tests() -> List[Dict[str, Any]]:
    """Return list of tournament tests."""
    return [
        {
            "name": "tournament-basic",
            "description": "Basic tournament gauntlet test",
            "args": "--settingsfile=test/integration/tournament/test-tournament-file.ini",
            "log_path": "test/integration/log/tournament",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "tournament-report-*.log",
                    "count": 1,
                },
                 {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "test.pgn",
                    "count": 1,
                },
            ],
            "cleanup": "test/integration/log/tournament",
        },
        {
            "name": "tournament-nonexisting-file",
            "description": "Tournament continuation from existing tournament file",
            "args": "--settingsfile=test/integration/tournament/test-tournament-write-nonexisting.ini",
            "log_path": "test/integration/log/tournament",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/tournament/test-tournament-write-nonexisting.qtour",
                    "content": "[each]"
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/tournament/test-tournament-write-nonexisting.qtour",
                    "content": "[round]"
                }
            ],
            "cleanup": "test/integration/log/tournament"
        },
        {
            "name": "tournament-round-robin",
            "description": "Round-robin tournament with 3 engines - verifies all pairings complete",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=2 repeat=2 rounds=1 --openings file=test/opening/book8ply.raw order=sequential --each tc=0.1+0.01 trace=none --engine conf='Qapla 0.3.1' --engine conf='Qapla 0.3.2' --engine conf='Spike 1.4' --logging engine=false path=test/integration/log/tournament/roundrobin",
            "log_path": "test/integration/log/tournament/roundrobin",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "logFiles", "path": "", "pattern": "tournament-report-*.log", "count": 1},
            ],
            "cleanup": "test/integration/log/tournament/roundrobin",
        },
        {
            "name": "tournament-continuation",
            "description": "Tournament resumed from an existing .qtour - round 1 is skipped, round 2 is played",
            "args": "--concurrency=2 --logging engine=false path=test/integration/log/tournament/continue "
                    "--tournament file=test/integration/log/tournament/continue/test-tournament-continue.qtour",
            "log_path": "test/integration/log/tournament/continue",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "2 engines taken from"},
                {
                    # Round 1 is already in the file; only round 2 may be played.
                    "type": "stdout",
                    "content": "Encounter llt1 vs llt2 round 2",
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/tournament/continue/test-tournament-continue.qtour",
                    "content": "round=2",
                    "message": "The results of the resumed round were not written back",
                },
            ],
            "cleanup": "test/integration/log/tournament/continue",
            "source_files": [
                {
                    "source": _TOUR_FILE_SOURCE,
                    "target": "test/integration/log/tournament/continue/test-tournament-continue.qtour",
                }
            ],
        },
        {
            "name": "tournament-roundrobin-too-few-engines",
            "description": "Round-robin with a single engine is rejected before any engine starts",
            "args": f"--concurrency=1 {_ENGINES} --tournament type=round-robin games=2 "
                    f"--engine conf='Qapla 0.4.0' {_OPENINGS} --each tc=0.1+0.01 "
                    "--logging path=test/integration/log/tournament/arity",
            "log_path": "test/integration/log/tournament/arity",
            "validators": [
                {"type": "exitCode", "expected": 2},
                {"type": "stdout", "content": "Round-robin tournament requires at least two engines."},
            ],
            "cleanup": "test/integration/log/tournament/arity",
        },
        {
            "name": "tournament-gauntlet-fallback",
            "description": "Gauntlet without gauntlet=true - the first engine plays all others",
            "args": f"--concurrency=4 {_ENGINES} --tournament type=gauntlet games=2 rounds=1 {_OPENINGS} "
                    "--each tc=0.1+0.01 trace=none "
                    "--engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' --engine conf='Qapla 0.3.1' "
                    "--logging engine=false path=test/integration/log/tournament/fallback",
            "log_path": "test/integration/log/tournament/fallback",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Encounter Qapla 0.4.0 vs Qapla 0.3.2"},
                {"type": "stdout", "content": "Encounter Qapla 0.4.0 vs Qapla 0.3.1"},
                {
                    # Without gauntlet=true the first engine is the gauntlet engine,
                    # so a gauntlet must not pair the two remaining engines.
                    "type": "stdout",
                    "content": r"(?s)^(?:(?!Encounter Qapla 0\.3\.2 vs).)*$",
                    "isRegex": True,
                    "message": "Round-robin pairings appeared in a gauntlet",
                },
            ],
            "cleanup": "test/integration/log/tournament/fallback",
        },
        {
            "name": "tournament-noswap-event-rating",
            "description": "noswap, event name and rating interval - colors stay fixed, event lands in the PGN",
            "args": f"--concurrency=4 {_ENGINES} --tournament type=gauntlet games=2 rounds=1 "
                    f"event=ITEvent noswap=true ratinginterval=2 outcomeinterval=2 {_OPENINGS} "
                    "--each tc=0.1+0.01 trace=none "
                    "--engine conf='Qapla 0.4.0' gauntlet=true --engine conf='Qapla 0.3.2' "
                    "--pgnoutput file=test/integration/log/tournament/noswap/games.pgn "
                    "--logging engine=false path=test/integration/log/tournament/noswap",
            "log_path": "test/integration/log/tournament/noswap",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Rating interval:   2 games"},
                {"type": "stdout", "content": "swap no"},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/tournament/noswap/games.pgn",
                    "content": "[Event \"ITEvent\"]",
                },
                {
                    "type": "fileContent",
                    "path": "test/integration/log/tournament/noswap/games.pgn",
                    "content": r"(?s)^(?:(?!\[White \"Qapla 0\.3\.2\"\]).)*$",
                    "isRegex": True,
                    "message": "Colors were swapped although noswap=true",
                },
            ],
            "cleanup": "test/integration/log/tournament/noswap",
        },
    ]
