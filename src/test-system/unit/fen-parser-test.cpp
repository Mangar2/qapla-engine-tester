/**
 * @license
 * This software is licensed under the GNU LESSER GENERAL PUBLIC LICENSE Version 3. It is furnished
 * "as is", without any support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include <catch2/catch_test_macros.hpp>

#include "../../opening/fen-parser.h"

using namespace QaplaTester;

TEST_CASE("FEN parser", "[unit][fen-parser]") {
    SECTION("Full FEN with counters") {
        auto result = parseFen({.fenString =
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"});
        REQUIRE(result.gameRecord.has_value());
        REQUIRE(result.error == 0);
        REQUIRE_FALSE(result.gameRecord->isWhiteToMove());
    }

    SECTION("EPD-style FEN without counters") {
        const std::string line =
            "r3r1k1/1pq2pp1/2p2n2/1PNn4/2QN2b1/6P1/3RPP2/2R3KB b - - bm Re3; id \"WMT 01\";";
        auto result = parseFen({.fenString = line});
        REQUIRE(result.gameRecord.has_value());
        REQUIRE_FALSE(result.gameRecord->isWhiteToMove());
        // nextPos points behind the FEN so opcode parsing can continue there
        REQUIRE(line.substr(result.nextPos).find("bm Re3") != std::string::npos);
    }

    SECTION("White to move") {
        auto result = parseFen({.fenString =
            "3r1r2/pp1q2bk/2n1nppp/2p5/3pP1P1/P2P1NNQ/1PPB3P/1R3R1K w - -"});
        REQUIRE(result.gameRecord.has_value());
        REQUIRE(result.gameRecord->isWhiteToMove());
    }

    SECTION("Loose mode finds embedded FEN") {
        auto result = parseFen({
            .fenString = "position fen rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
            .startPos = 0,
            .maxSearchLength = 1000
        });
        REQUIRE(result.gameRecord.has_value());
        REQUIRE_FALSE(result.gameRecord->isWhiteToMove());
    }

    SECTION("Strict mode rejects leading garbage") {
        auto result = parseFen({
            .fenString = "position fen rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
            .startPos = 0,
            .maxSearchLength = 1
        });
        REQUIRE_FALSE(result.gameRecord.has_value());
        REQUIRE(result.error != 0);
    }

    SECTION("Invalid input yields error") {
        auto result = parseFen({.fenString = "this is not a fen at all"});
        REQUIRE_FALSE(result.gameRecord.has_value());
        REQUIRE(result.error != 0);
    }

    SECTION("Blank-only input yields error") {
        auto result = parseFen({.fenString = "   "});
        REQUIRE_FALSE(result.gameRecord.has_value());
        REQUIRE(result.error != 0);
    }
}
