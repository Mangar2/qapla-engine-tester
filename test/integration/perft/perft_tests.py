#!/usr/bin/env python3
"""Perft Tests - Validates the move generator node count test (--perft).

Perft needs no engine at all, so these tests are the cheapest in the suite. The
node counts asserted below are the published reference values for the start
position and for "Kiwipete"; they are what makes these tests a real regression
guard for the move generator rather than a smoke test.
"""

from typing import Any, Dict, List

_KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"


def get_tests() -> List[Dict[str, Any]]:
    """Return list of perft tests."""
    return [
        {
            "name": "perft-startpos-depth5",
            "description": "Perft from the start position to depth 5 - pins the reference node count",
            "args": "--concurrency=4 --perft position=startpos depth=5 divide=false "
                    "--logging path=test/integration/log/perft",
            "log_path": "test/integration/log/perft",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "perft finished: depth=5 nodes=4865609"},
            ],
            "cleanup": "test/integration/log/perft",
        },
        {
            "name": "perft-divide-showfen",
            "description": "Perft with divide and showfen - per root move node counts and resulting FENs",
            "args": "--concurrency=2 --perft position=startpos depth=2 divide=true showfen=true "
                    "--logging path=test/integration/log/perft",
            "log_path": "test/integration/log/perft",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": r"a2a3\s+\|\s+20\s+\|\s+rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1",
                    "isRegex": True,
                },
                {"type": "stdout", "content": "perft finished: depth=2 nodes=400"},
            ],
            "cleanup": "test/integration/log/perft",
        },
        {
            "name": "perft-fen-position",
            "description": "Perft from a FEN with castling rights and captures (Kiwipete) to depth 3",
            "args": f"--concurrency=4 --perft position=\"{_KIWIPETE}\" depth=3 divide=false "
                    "--logging path=test/integration/log/perft",
            "log_path": "test/integration/log/perft",
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "perft finished: depth=3 nodes=97862"},
            ],
            "cleanup": "test/integration/log/perft",
        },
        {
            "name": "perft-invalid-fen",
            "description": "Perft with a malformed position - rejected as invalid parameter",
            "args": "--concurrency=1 --perft position=\"not a fen\" depth=1 "
                    "--logging path=test/integration/log/perft",
            "log_path": "test/integration/log/perft",
            "validators": [
                {"type": "exitCode", "expected": 2},
                {"type": "stdout", "content": "Invalid FEN for perft"},
            ],
            "cleanup": "test/integration/log/perft",
        },
    ]
