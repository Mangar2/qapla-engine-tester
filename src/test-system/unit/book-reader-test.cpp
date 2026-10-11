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

#include "../../opening/book-reader.h"
#include "../../opening/opening-parser.h"
#include "../../game-manager/game-state.h"
#include "../../qapla-engine/book9/book.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace QaplaTester;

namespace {

/**
 * @brief A file name in the temporary directory, removed again at the end.
 */
struct TempFile {
    explicit TempFile(const std::string& name)
        : path((std::filesystem::temp_directory_path() / name).string()) {}
    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    std::string path;
};

/** A line of moves in long notation as book moves, castling as the king moving two squares. */
std::vector<QaplaBook9::BookMove> movesOf(const std::vector<std::string>& lans) {
    std::vector<QaplaBook9::BookMove> moves;
    for (const auto& lan : lans) {
        auto square = [&](size_t at) {
            return QaplaBasics::Square((lan[at] - 'a') + (lan[at + 1] - '1') * 8);
        };
        moves.push_back(QaplaBook9::BookMove{ .from = square(0), .to = square(2) });
    }
    return moves;
}

const std::vector<std::string> kItalian = { "e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "g8f6", "e1g1" };
const std::vector<std::string> kSicilian = { "e2e4", "c7c5" };
const std::vector<std::string> kQueensPawn = { "d2d4", "d7d5", "c2c4" };

/** Writes a book with the three lines, the queen's pawn line given the payload type's default. */
template <typename PAYLOAD>
void writeBook(const std::string& path) {
    QaplaBook9::Book<PAYLOAD> book;
    for (const auto* line : { &kItalian, &kSicilian, &kQueensPawn }) {
        book.addLine(movesOf(*line));
    }
    book.writeToFile(path);
}

std::vector<std::string> lansOf(const GameRecord& game) {
    std::vector<std::string> lans;
    for (const auto& move : game.history()) {
        lans.push_back(move.original);
    }
    return lans;
}

struct ValuePayload {
    uint16_t value = 0;
};

} // namespace

TEST_CASE("A 9 bit book is read as one opening per line", "[unit][opening][book]") {
    TempFile file("qapla-book-reader-test.bok");
    writeBook<QaplaBook9::EmptyPayload>(file.path);

    const auto result = readBookLines(file.path);
    REQUIRE(result.error.empty());
    REQUIRE(result.games.size() == 3);
    REQUIRE(result.moves == 11);
    // Lines in the order of the book, castling as the king moving two squares.
    REQUIRE(lansOf(result.games[0]) == kItalian);
    REQUIRE(lansOf(result.games[1]) == kSicilian);
    REQUIRE(lansOf(result.games[2]) == kQueensPawn);

    SECTION("Every line can be played, and plies cuts it as for a pgn") {
        GameState state;
        const auto full = state.setFromGameRecordAndCopy(result.games[0]);
        REQUIRE(full.history().size() == kItalian.size());
        REQUIRE(state.getFen().starts_with("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQ1RK1 b kq"));

        GameState cut;
        REQUIRE(cut.setFromGameRecordAndCopy(result.games[0], 2).history().size() == 2);
    }

    SECTION("maxLines stops early") {
        REQUIRE(readBookLines(file.path, 2).games.size() == 2);
    }
}

TEST_CASE("A book is read whatever its payload", "[unit][opening][book]") {
    TempFile file("qapla-book-reader-payload-test.bok");
    writeBook<ValuePayload>(file.path);
    const auto result = readBookLines(file.path);
    REQUIRE(result.error.empty());
    REQUIRE(result.games.size() == 3);
    REQUIRE(lansOf(result.games[2]) == kQueensPawn);
}

TEST_CASE("A move of priority 0 is left out with its lines", "[unit][opening][book]") {
    TempFile file("qapla-book-reader-never-test.bok");
    QaplaBook9::Book<> book;
    book.addLine(movesOf(kItalian));
    const auto sicilian = book.addLine(movesOf(kSicilian));
    book.setPriority(sicilian, QaplaBook9::PRIORITY_NEVER);
    book.addLine(movesOf(kQueensPawn));
    book.writeToFile(file.path);

    const auto result = readBookLines(file.path);
    REQUIRE(result.games.size() == 2);
    REQUIRE(result.neverPlayed == 1);
    REQUIRE(lansOf(result.games[1]) == kQueensPawn);
}

TEST_CASE("The opening parser finds a book by its content", "[unit][opening][book]") {
    SECTION("By its extension") {
        TempFile file("qapla-book-parser-test.bok");
        writeBook<QaplaBook9::EmptyPayload>(file.path);
        const auto result = OpeningParser().parseWithTrace(file.path);
        REQUIRE(result.successfulParser == "Book");
        REQUIRE(result.games.size() == 3);
    }

    SECTION("Under any other name, after the other parsers refused it") {
        TempFile file("qapla-book-parser-test.openings");
        writeBook<QaplaBook9::EmptyPayload>(file.path);
        const auto result = OpeningParser().parseWithTrace(file.path);
        REQUIRE(result.successfulParser == "Book");
        REQUIRE(result.games.size() == 3);
    }

    SECTION("A pgn named .bok is still read as a pgn") {
        TempFile file("qapla-book-parser-pgn-test.bok");
        std::ofstream(file.path, std::ios::binary) << "[White \"A\"]\n[Black \"B\"]\n\n1. e4 e5 2. Nf3 *\n";
        const auto result = OpeningParser().parseWithTrace(file.path);
        REQUIRE(result.successfulParser == "PGN");
        REQUIRE(result.games.size() == 1);
    }
}

TEST_CASE("What is no 9 bit book is refused with a reason", "[unit][opening][book]") {
    SECTION("An 11 bit book") {
        TempFile file("qapla-book-reader-11bit-test.bok");
        {
            std::ofstream stream(file.path, std::ios::binary);
            QaplaBook::BookHeader header;
            header.fileVersion = "3";
            QaplaBook::writeBookHeader(stream, header, 0);
        }
        const auto result = readBookLines(file.path);
        REQUIRE(result.games.empty());
        REQUIRE(result.error.find("11 bit book") != std::string::npos);
    }

    SECTION("A text file") {
        TempFile file("qapla-book-reader-text-test.bok");
        std::ofstream(file.path, std::ios::binary) << "[White \"A\"]\n\n1. e4 *\n";
        const auto result = readBookLines(file.path);
        REQUIRE(result.games.empty());
        REQUIRE_FALSE(result.error.empty());
    }

    SECTION("A book cut off inside its nodes") {
        TempFile full("qapla-book-reader-full-test.bok");
        writeBook<QaplaBook9::EmptyPayload>(full.path);
        TempFile cut("qapla-book-reader-cut-test.bok");
        const auto size = std::filesystem::file_size(full.path);
        std::filesystem::copy_file(full.path, cut.path, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::resize_file(cut.path, size - 3);
        const auto result = readBookLines(cut.path);
        REQUIRE(result.games.empty());
        REQUIRE_FALSE(result.error.empty());
    }
}
