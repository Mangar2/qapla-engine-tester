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

TEST_CASE("MoveRecord clearSearchInfo", "[unit][move-record]") {
    MoveRecord move;
    move.original = "e2e4";
    move.lan_ = "e2e4";
    move.san_ = "e4";
    move.engineId_ = "white-1";
    move.engineName_ = "White Player";
    move.book = true;
    move.halfmoveNo_ = 1;
    move.halfmoveClock = 0;
    move.endCause_ = GameEndCause::Checkmate;
    move.result_ = GameResult::WhiteWins;
    move.comment = "+0.25/12 1.2s";
    move.nag = "$1";
    move.ponderMove = "e7e5";
    move.timeMs = 1200;
    move.scoreCp = 25;
    move.depth = 12;
    move.seldepth = 20;
    move.multipv = 2;
    move.nodes = 123456;
    move.pv = "e2e4 e7e5";
    move.info.emplace_back();
    move.infoUpdateCount = 7;

    move.clearSearchInfo();

    // The move itself, who played it and how the game ended are not what a search said.
    CHECK(move.original == "e2e4");
    CHECK(move.lan_ == "e2e4");
    CHECK(move.san_ == "e4");
    CHECK(move.engineId_ == "white-1");
    CHECK(move.engineName_ == "White Player");
    CHECK(move.book);
    CHECK(move.halfmoveNo_ == 1);
    CHECK(move.endCause_ == GameEndCause::Checkmate);
    CHECK(move.result_ == GameResult::WhiteWins);

    CHECK(move.comment.empty());
    CHECK(move.nag.empty());
    CHECK(move.ponderMove.empty());
    CHECK(move.timeMs == 0);
    CHECK_FALSE(move.scoreCp.has_value());
    CHECK_FALSE(move.scoreMate.has_value());
    CHECK(move.depth == 0);
    CHECK(move.seldepth == 0);
    CHECK(move.multipv == 1);
    CHECK(move.nodes == 0);
    CHECK(move.pv.empty());
    CHECK(move.info.empty());
    CHECK(move.infoUpdateCount == 0);
}

TEST_CASE("MoveRecord clearSearchInfo drops a mate score too", "[unit][move-record]") {
    MoveRecord move;
    move.scoreMate = 3;
    move.clearSearchInfo();
    CHECK_FALSE(move.scoreMate.has_value());
}
