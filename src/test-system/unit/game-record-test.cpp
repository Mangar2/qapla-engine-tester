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

#include "../../chess-game/game-record.h"

using namespace QaplaTester;

namespace {
    MoveRecord makeMove(const std::string& lan, uint64_t timeMs) {
        MoveRecord move;
        move.lan_ = lan;
        move.timeMs = timeMs;
        return move;
    }
}

TEST_CASE("GameRecord timeUsed", "[unit][game-record]") {
    SECTION("White to move at start") {
        GameRecord record;
        record.setStartPosition(true, "", true, 0);
        record.addMove(makeMove("e2e4", 1000));
        record.addMove(makeMove("e7e5", 2000));
        record.addMove(makeMove("g1f3", 300));

        auto [whiteTime, blackTime] = record.timeUsed();
        REQUIRE(whiteTime == 1300);
        REQUIRE(blackTime == 2000);
    }

    SECTION("Black to move at start") {
        // Opening/EPD start position after 1.e4: black moves first,
        // so ply 0 is a black move.
        GameRecord record;
        record.setStartPosition(false,
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", false, 1);
        record.addMove(makeMove("e7e5", 2525));
        record.addMove(makeMove("g1f3", 1000));

        auto [whiteTime, blackTime] = record.timeUsed();
        REQUIRE(whiteTime == 1000);
        REQUIRE(blackTime == 2525);
    }
}
