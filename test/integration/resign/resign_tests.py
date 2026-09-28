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
            "name": "resign-inactive-adjudicates-nothing",
            "description": "active=false switches resign adjudication off, and movecount is not asked for",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=6 --openings file=test/opening/book8ply.raw order=sequential --each tc=depth:4 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--resign active=false score=100 --pgnoutput file=test/integration/log/resign/resign-inactive-adjudicates-nothing/out.pgn append=false --logging engine=false path=test/integration/log/resign/resign-inactive-adjudicates-nothing",
            "log_path": "test/integration/log/resign/resign-inactive-adjudicates-nothing",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/resign/resign-inactive-adjudicates-nothing/out.pgn",
                    "content": r'(?s)^(?:(?!\[Termination "adjudication"\]).)*$',
                    "isRegex": True,
                    "message": "A game was adjudicated although the block is switched off",
                },
            ],
            "cleanup": "test/integration/log/resign/resign-inactive-adjudicates-nothing",
        },
        {
            "name": "resign-inactive-with-all-settings",
            "description": "active=false switches resign adjudication off even with every other setting given",
            "args": "--concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=6 --openings file=test/opening/book8ply.raw order=sequential --each tc=depth:4 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--resign active=false movecount=1 score=100 --pgnoutput file=test/integration/log/resign/resign-inactive-with-all-settings/out.pgn append=false --logging engine=false path=test/integration/log/resign/resign-inactive-with-all-settings",
            "log_path": "test/integration/log/resign/resign-inactive-with-all-settings",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/resign/resign-inactive-with-all-settings/out.pgn",
                    "content": r'(?s)^(?:(?!\[Termination "adjudication"\]).)*$',
                    "isRegex": True,
                    "message": "A game was adjudicated although the block is switched off",
                },
            ],
            "cleanup": "test/integration/log/resign/resign-inactive-with-all-settings",
        },
        {
            "name": "resign-inactive-in-settings-file",
            "description": "A [resign] block in a settings file with active=false and no movecount adjudicates nothing",
            "args": "--settingsfile=test/integration/resign/resign-off.ini --concurrency=4 --enginesfile=test/integration/engines/engines.ini --tournament type=round-robin games=6 --openings file=test/opening/book8ply.raw order=sequential --each tc=depth:4 trace=none --engine conf='Qapla 0.4.0' --engine conf='Qapla 0.3.2' "
                    "--pgnoutput file=test/integration/log/resign/resign-inactive-in-settings-file/out.pgn append=false --logging engine=false path=test/integration/log/resign/resign-inactive-in-settings-file",
            "log_path": "test/integration/log/resign/resign-inactive-in-settings-file",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "fileContent",
                    "path": "test/integration/log/resign/resign-inactive-in-settings-file/out.pgn",
                    "content": r'(?s)^(?:(?!\[Termination "adjudication"\]).)*$',
                    "isRegex": True,
                    "message": "A game was adjudicated although the block is switched off",
                },
            ],
            "cleanup": "test/integration/log/resign/resign-inactive-in-settings-file",
        },
    ]
