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
                    # The closing standings are the same table the run reports while playing -
                    # they used to be formatted separately and drifted apart.
                    "type": "stdout",
                    "content": r"(?s)Tournament result:\s*\nRank \| Name\s+\| Elo\s+\| \+/-",
                    "isRegex": True,
                },
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
            "name": "tournament-file-uci-option-roundtrip",
            "description": "A tournament file written with a UCI option can be resumed, and the option reaches the engine unchanged",
            # Black box round trip: the preparation run writes the state file, the
            # run under test resumes from it. Nothing here looks inside the file -
            # what is promised to the user is that a resumed run configures its
            # engines exactly as the original run did, not any particular file
            # syntax. rounds=2 on the command line leaves the resumed run one round
            # of real games to play, which is what makes the option observable.
            # Engine logging stays on in the preparation run: switching it off would store
            # trace=none in the file and the resumed run could not report anything. The
            # resumed run writes one log per engine (mode=each), so its files are the
            # "engine-#<n>-" ones and each engine's options can be checked separately.
            # The shared option is set once in --each and overridden for the second engine, so the
            # run also covers what a written engine section may leave out: the first engine has to
            # inherit the option from the file's [each] section on resume.
            "setup_args": f"--concurrency=1 {_ENGINES} "
                          "--tournament type=gauntlet file=test/integration/log/tournament/roundtrip/tournament.qtour "
                          f"games=1 rounds=1 {_OPENINGS} --each tc=0.2+0.01 trace=all option.Hash=128 "
                          "--engine conf='Qapla 0.4.0' "
                          "--engine conf='Qapla 0.3.2' option.Hash=64 "
                          "--logging engine=true mode=one path=test/integration/log/tournament/roundtrip",
            "args": "--concurrency=1 "
                    "--tournament file=test/integration/log/tournament/roundtrip/tournament.qtour rounds=2 "
                    "--logging engine=true mode=each path=test/integration/log/tournament/roundtrip",
            "log_path": "test/integration/log/tournament/roundtrip",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-#0-*.log",
                    "count": 1,
                    "content": "setoption name Hash value 128",
                },
                {
                    "type": "logFiles",
                    "path": "",
                    "pattern": "engine-#1-*.log",
                    "count": 1,
                    "content": "setoption name Hash value 64",
                },
                {
                    # The engine that agrees with the shared default must not repeat it: the
                    # value belongs in [each] once, not in every engine section.
                    "type": "fileContent",
                    "path": "test/integration/log/tournament/roundtrip/tournament.qtour",
                    "content": r"(?s)^(?!.*option\.hash=128[\s\S]*option\.hash=128)",
                    "isRegex": True,
                    "message": "A shared default was written into an engine section as well",
                },
            ],
            "cleanup": "test/integration/log/tournament/roundtrip",
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
