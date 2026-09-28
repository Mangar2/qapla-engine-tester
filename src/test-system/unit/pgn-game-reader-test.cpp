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

#include <filesystem>
#include <fstream>
#include <string>

using namespace QaplaTester;

namespace {

/**
 * @brief Writes a PGN file into the temporary directory and removes it again at the end.
 */
struct TempPgn {
    explicit TempPgn(const std::string& name, const std::string& content)
        : path((std::filesystem::temp_directory_path() / name).string()) {
        std::ofstream(path, std::ios::binary) << content;
    }
    ~TempPgn() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    TempPgn(const TempPgn&) = delete;
    TempPgn& operator=(const TempPgn&) = delete;

    std::string path;
};

// Three games and an empty one between them: a comment running over two lines, no Event tag -
// the shapes that decide where one game ends and the next begins.
const std::string kGames =
    "[White \"A\"]\n[Black \"B\"]\n\n"
    "1. e2e4 e7e5 2. g1f3 {+0.20/8 (b8c6\nf1b5)} b8c6 1-0\n\n"
    "[White \"Empty\"]\n[Black \"Game\"]\n\n*\n\n"
    "[White \"C\"]\n[Black \"D\"]\n\n"
    "1. d4 d5 2. c4 1/2-1/2\n\n"
    "[White \"E\"]\n[Black \"F\"]\n\n"
    "1. f3 e5 2. g4 Qh4# 0-1\n";

PgnIO::LoadParams paramsFor(const std::string& path) {
    PgnIO::LoadParams params;
    params.filePath = path;
    params.loadComments = true;
    params.skipEmptyGames = true;
    return params;
}

} // namespace

TEST_CASE("Reading game by game gives the games loading the whole file gives", "[unit][pgn]") {
    const TempPgn file("qapla-pgn-game-reader-test.pgn", kGames);

    PgnIO loader;
    const auto loaded = loader.loadGamesWithResult(paramsFor(file.path)).games;
    REQUIRE(loaded.size() == 3);

    PgnGameReader reader(paramsFor(file.path));
    REQUIRE(reader.isOpen());
    for (const auto& expected : loaded) {
        const auto game = reader.next();
        REQUIRE(game);
        CHECK(game->getWhiteEngineName() == expected.getWhiteEngineName());
        CHECK(game->getBlackEngineName() == expected.getBlackEngineName());
        CHECK(game->getGameResult() == expected.getGameResult());
        REQUIRE(game->history().size() == expected.history().size());
        for (size_t ply = 0; ply < expected.history().size(); ++ply) {
            CHECK(game->history()[ply].san_ == expected.history()[ply].san_);
            CHECK(game->history()[ply].scoreCp == expected.history()[ply].scoreCp);
            CHECK(game->history()[ply].pv == expected.history()[ply].pv);
        }
    }
    CHECK_FALSE(reader.next());
    CHECK_FALSE(reader.next());
}

TEST_CASE("A game reader on a missing file reads nothing", "[unit][pgn]") {
    PgnGameReader reader(paramsFor(
        (std::filesystem::temp_directory_path() / "qapla-no-such-file.pgn").string()));
    CHECK_FALSE(reader.isOpen());
    CHECK_FALSE(reader.next());
}

TEST_CASE("Loading stops after the requested number of games", "[unit][pgn]") {
    const TempPgn file("qapla-pgn-max-games-test.pgn", kGames);

    auto params = paramsFor(file.path);
    params.maxGames = 2;
    PgnIO loader;
    const auto result = loader.loadGamesWithResult(params);
    REQUIRE(result.games.size() == 2);
    CHECK(result.games[1].getWhiteEngineName() == "C");
    // Where each game read starts, and nothing of the game not read.
    CHECK(loader.getGamePositions().size() == 3);
    CHECK(loader.getRawGameText(1).value_or("").starts_with("[White \"Empty\"]"));
}

TEST_CASE("The file of a round carries the round number before its extension", "[unit][pgn]") {
    CHECK(PgnSave::roundFileName("games.pgn", 1) == "games-round-001.pgn");
    CHECK(PgnSave::roundFileName("games.pgn", 12) == "games-round-012.pgn");
    CHECK(PgnSave::roundFileName("games.pgn", 1234) == "games-round-1234.pgn");
    CHECK(PgnSave::roundFileName("out/games.pgn", 3)
        == (std::filesystem::path("out") / "games-round-003.pgn").string());
    CHECK(PgnSave::roundFileName("games", 2) == "games-round-002");
}
