#!/usr/bin/env python3
"""Draw Adjudication Tests - Validates --draw parameter group."""

from typing import Any, Dict, List


def get_tests() -> List[Dict[str, Any]]:
    """Return list of draw adjudication tests."""
    return [
        {
            "name": "draw-adjudication",
            "description": "SPRT with draw adjudication in test mode - parameters accepted, maxgames reached",
            "args": "--settingsfile=test/integration/sprt/test-sprt-maxgames.ini --draw movenumber=1 movecount=1 score=5000 test=true --logging path=test/integration/log/draw",
            "log_path": "test/integration/log/draw",
            "validators": [
                {"type": "exitCode", "expected": 16},
            ],
            "cleanup": "test/integration/log/draw",
        },
        {
            "name": "draw-adjudicates",
            "description": "A draw block that applies from the first move ends games by adjudication",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=6 --openings file=test/opening/book8ply.raw order=sequential --each tc=depth:4 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--draw movenumber=1 movecount=1 score=5000 --pgnoutput file=test/integration/log/draw/draw-adjudicates/out.pgn append=false --logging engine=false path=test/integration/log/draw/draw-adjudicates",
            "log_path": "test/integration/log/draw/draw-adjudicates",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/draw/draw-adjudicates/out.pgn",
                    "content": r'\[Termination "adjudication"\]',
                    "isRegex": True,
                    "message": "No game was adjudicated, the block did not switch adjudication on",
                },
            ],
            "cleanup": "test/integration/log/draw/draw-adjudicates",
        },
        {
            "name": "draw-inactive-adjudicates-nothing",
            "description": "active=false switches draw adjudication off",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=6 --openings file=test/opening/book8ply.raw order=sequential --each tc=depth:4 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--draw active=false movenumber=1 movecount=1 score=5000 --pgnoutput file=test/integration/log/draw/draw-inactive-adjudicates-nothing/out.pgn append=false --logging engine=false path=test/integration/log/draw/draw-inactive-adjudicates-nothing",
            "log_path": "test/integration/log/draw/draw-inactive-adjudicates-nothing",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/draw/draw-inactive-adjudicates-nothing/out.pgn",
                    "content": r'(?s)^(?:(?!\[Termination "adjudication"\]).)*$',
                    "isRegex": True,
                    "message": "A game was adjudicated although the block is switched off",
                },
            ],
            "cleanup": "test/integration/log/draw/draw-inactive-adjudicates-nothing",
        },
    ]
