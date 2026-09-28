#!/usr/bin/env python3
"""Resign Adjudication Tests - Validates --resign parameter group."""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of resign adjudication tests."""
    return [
        {
            "name": "resign-adjudication",
            "description": "SPRT with resignation adjudication in test mode - parameters accepted, maxgames reached",
            "args": "--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --resign movecount=3 score=500 twosided=false test=true --logging path=test/integration/log/resign",
            "log_path": "test/integration/log/resign",
            "validators": [
                {"type": "exitCode", "expected": 16},
            ],
            "cleanup": "test/integration/log/resign",
        },
        {
            "name": "resign-adjudicates",
            "description": "A resign block with a low threshold ends games by adjudication",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=6 --openings file=test/opening/book8ply.raw order=sequential --each tc=depth:4 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--resign movecount=1 score=100 --pgnoutput file=test/integration/log/resign/resign-adjudicates/out.pgn append=false --logging engine=false path=test/integration/log/resign/resign-adjudicates",
            "log_path": "test/integration/log/resign/resign-adjudicates",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/resign/resign-adjudicates/out.pgn",
                    "content": r'\[Termination "adjudication"\]',
                    "isRegex": True,
                    "message": "No game was adjudicated, the block did not switch adjudication on",
                },
            ],
            "cleanup": "test/integration/log/resign/resign-adjudicates",
        },
        {
            "name": "resign-off-in-tournament-file",
            "description": "A tournament file with resign switched off (active=false, GUI compatibility) adjudicates nothing",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=6 file=test/integration/log/resign/resign-off-in-tournament-file/state.qtour --openings file=test/opening/book8ply.raw order=sequential --each tc=depth:4 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--pgnoutput file=test/integration/log/resign/resign-off-in-tournament-file/out.pgn append=false --logging engine=false path=test/integration/log/resign/resign-off-in-tournament-file",
            "log_path": "test/integration/log/resign/resign-off-in-tournament-file",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/resign/resign-off-in-tournament-file/out.pgn",
                    "content": r'(?s)^(?:(?!\[Termination "adjudication"\]).)*$',
                    "isRegex": True,
                    "message": "A game was adjudicated although the tournament file switches it off",
                },
            ],
            "cleanup": "test/integration/log/resign/resign-off-in-tournament-file",
            "source_files": [
                {"source": "test/integration/resign/resign-off.qtour", "target": "test/integration/log/resign/resign-off-in-tournament-file/state.qtour"}
            ],
        },
        {
            "name": "resign-off-needs-no-movecount",
            "description": "A switched-off resign block in a tournament file does not ask for movecount",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=6 file=test/integration/log/resign/resign-off-needs-no-movecount/state.qtour --openings file=test/opening/book8ply.raw order=sequential --each tc=depth:4 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--pgnoutput file=test/integration/log/resign/resign-off-needs-no-movecount/out.pgn append=false --logging engine=false path=test/integration/log/resign/resign-off-needs-no-movecount",
            "log_path": "test/integration/log/resign/resign-off-needs-no-movecount",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/resign/resign-off-needs-no-movecount/out.pgn",
                    "content": r'(?s)^(?:(?!\[Termination "adjudication"\]).)*$',
                    "isRegex": True,
                    "message": "A game was adjudicated although the tournament file switches it off",
                },
            ],
            "cleanup": "test/integration/log/resign/resign-off-needs-no-movecount",
            "source_files": [
                {"source": "test/integration/resign/resign-off-minimal.qtour", "target": "test/integration/log/resign/resign-off-needs-no-movecount/state.qtour"}
            ],
        },
    ]
