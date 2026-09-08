/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include <catch2/catch_test_macros.hpp>

#include "../../opening/pgn-io.h"
#include "../../opening/pgn-save.h"

#include <sstream>
#include <string>

using namespace QaplaTester;

namespace {

const std::string kHeader =
    "[White \"Engine A\"]\n"
    "[Black \"Engine B\"]\n"
    "[Result \"*\"]\n\n";

GameRecord parse(const std::string& moveText) {
    return PgnIO::parseGame(kHeader + moveText + " *\n");
}

} // namespace

TEST_CASE("Move comments of this tester", "[unit][pgn]") {
    SECTION("Score, depth, time and the variation in brackets") {
        const auto game = parse("1. e4 {+0.31/14 0.89s (e2e4 d7d5)} e5 {-1.50/12 0.10s (e7e5)}");
        REQUIRE(game.history().size() == 2);

        const auto& first = game.history()[0];
        REQUIRE(first.scoreCp.has_value());
        REQUIRE(*first.scoreCp == 31);
        REQUIRE(first.depth == 14);
        REQUIRE(first.timeMs == 890);
        REQUIRE(first.pv == "e2e4 d7d5");

        const auto& second = game.history()[1];
        REQUIRE(second.scoreCp.has_value());
        REQUIRE(*second.scoreCp == -150);
        REQUIRE(second.pv == "e7e5");
    }

    SECTION("A variation written without brackets is still read") {
        // Files written before the brackets were introduced.
        const auto game = parse("1. e4 {+0.31/14 0.89s e2e4 d7d5}");
        REQUIRE(game.history().size() == 1);
        REQUIRE(game.history()[0].pv == "e2e4 d7d5");
        REQUIRE(game.history()[0].timeMs == 890);
    }

    SECTION("Game end information after the variation") {
        const auto game = parse("1. e4 {+0.31/14 0.89s (e2e4), White mates}");
        REQUIRE(game.history().size() == 1);
        REQUIRE(game.history()[0].pv == "e2e4");
    }
}

TEST_CASE("Move comments of a live broadcast", "[unit][pgn]") {
    SECTION("Variation first, unsigned score, time as a bare number of seconds") {
        // The shape node-tlcv writes: "(pv) score/depth time".
        const auto game = parse("1. d4 {(d2d4 d7d5) 0.18/24 26} d5 {(d7d5) -1.35/19 47}");
        REQUIRE(game.history().size() == 2);

        const auto& first = game.history()[0];
        REQUIRE(first.scoreCp.has_value());
        REQUIRE(*first.scoreCp == 18);
        REQUIRE(first.depth == 24);
        REQUIRE(first.timeMs == 26000);
        REQUIRE(first.pv == "d2d4 d7d5");

        const auto& second = game.history()[1];
        REQUIRE(second.scoreCp.has_value());
        REQUIRE(*second.scoreCp == -135);
        REQUIRE(second.depth == 19);
        REQUIRE(second.timeMs == 47000);
    }

    SECTION("The variation may be written in SAN") {
        const auto game = parse("1. d4 {(Bg5 c6 Bh4) 0.08/25 35}");
        REQUIRE(game.history().size() == 1);
        REQUIRE(game.history()[0].pv == "Bg5 c6 Bh4");
    }

    SECTION("An empty variation leaves the rest readable") {
        const auto game = parse("1. d4 {() 0.53/23 21}");
        REQUIRE(game.history().size() == 1);
        REQUIRE(game.history()[0].depth == 23);
        REQUIRE(game.history()[0].timeMs == 21000);
        REQUIRE(game.history()[0].pv.empty());
    }

    SECTION("A book move carries no variation and no score") {
        // "(Book)" is bracketed like a variation and is none - and 'Book' is no move.
        const auto game = parse("1. d4 {(Book)} d5 {(Book)}");
        REQUIRE(game.history().size() == 2);
        REQUIRE(game.history()[0].pv.empty());
        REQUIRE_FALSE(game.history()[0].scoreCp.has_value());
        REQUIRE(game.history()[0].depth == 0);
    }
}

TEST_CASE("A game survives being written and read again", "[unit][pgn]") {
    const auto game = parse("1. e4 {+0.31/14 0.89s (e2e4 d7d5)} e5 {-0.20/12 0.50s (e7e5)}");

    PgnSave writer;
    // The game is unfinished on purpose - the comments are what this checks, not the result.
    writer.setOptions({ .onlyFinishedGames = false, .includeClock = true, .includeEval = true,
        .includePv = true, .includeDepth = true });
    std::ostringstream out;
    writer.saveGameToStream(out, game);

    INFO("written: " << out.str());
    const auto again = PgnIO::parseGame(out.str());
    REQUIRE(again.history().size() == game.history().size());
    for (size_t i = 0; i < game.history().size(); ++i) {
        REQUIRE(again.history()[i].scoreCp == game.history()[i].scoreCp);
        REQUIRE(again.history()[i].depth == game.history()[i].depth);
        REQUIRE(again.history()[i].timeMs == game.history()[i].timeMs);
        REQUIRE(again.history()[i].pv == game.history()[i].pv);
    }
}
