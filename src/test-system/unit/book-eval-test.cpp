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

#include "../../book-eval/book-eval-manager.h"
#include "../../book-eval/book-tree.h"
#include "../../base-elements/app-error.h"
#include "../../qapla-engine/book9/book.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace QaplaTester;

namespace {

/**
 * @brief A file name in the temporary directory, removed again at the end, with its .part.
 */
struct TempFile {
    explicit TempFile(const std::string& name)
        : path((std::filesystem::temp_directory_path() / name).string()) {}
    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(path + ".part", ignored);
    }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    std::string path;
};

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

/**
 * Nodes in file order: 0 e2e4, 1 e7e5, 2 g1f3, 3 b8c6, 4 f1c4, 5 g8f6, 6 e1g1, 7 c7c5, 8 d2d4,
 * 9 d7d5, 10 c2c4. A move is played from the start position (0) and from the positions after the
 * nodes 0 to 5, 8 and 9: nine positions.
 */
void writeBook(const std::string& path) {
    QaplaBook9::Book<> book;
    for (const auto* line : { &kItalian, &kSicilian, &kQueensPawn }) {
        book.addLine(movesOf(*line));
    }
    book.writeToFile(path);
}

EngineConfig depthEngine(const std::string& name, const std::string& tc = "depth:4") {
    EngineConfig engine;
    engine.setName(name);
    engine.setCmd("fictional_engine");
    engine.setTimeControl(tc);
    return engine;
}

/** @brief The record an engine sends back: the line of the task and the move it computed. */
GameRecord answer(const GameTask& task, std::optional<int> scoreCp, std::optional<int> scoreMate = {}) {
    GameRecord record = task.gameRecord;
    MoveRecord move;
    move.lan_ = "a2a3";
    move.scoreCp = scoreCp;
    move.scoreMate = scoreMate;
    record.addMove(move);
    return record;
}

struct ScorePayload {
    int16_t score = 0;
};

} // namespace

TEST_CASE("A search score is kept in two bytes", "[unit][bookeval]") {
    REQUIRE(BookScore::fromSearch(25, std::nullopt) == 25);
    REQUIRE(BookScore::fromSearch(-31234, std::nullopt) == -30000);
    REQUIRE(BookScore::fromSearch(std::nullopt, 5) == 31995);
    REQUIRE(BookScore::fromSearch(std::nullopt, -3) == -31997);
    REQUIRE(BookScore::fromSearch(std::nullopt, std::nullopt) == BookScore::NONE);
    REQUIRE(BookScore::isMate(31995));
    REQUIRE(BookScore::mateInMoves(31995) == 5);
    REQUIRE(BookScore::mateInMoves(-31997) == -3);
    REQUIRE_FALSE(BookScore::isMate(30000));
    REQUIRE_FALSE(BookScore::isMate(BookScore::NONE));
}

TEST_CASE("The positions of a book are the ones its moves are played from", "[unit][bookeval]") {
    TempFile file("qapla-book-eval-tree.bok");
    writeBook(file.path);
    const auto tree = BookTree::read(file.path);

    REQUIRE(tree.nodeCount() == 11);
    std::vector<uint32_t> playedFrom;
    for (uint32_t position = 0; position < tree.positionCount(); ++position) {
        if (tree.isPlayedFrom(position)) {
            playedFrom.push_back(position);
        }
    }
    REQUIRE(playedFrom == std::vector<uint32_t>{ 0, 1, 2, 3, 4, 5, 6, 9, 10 });
    REQUIRE(tree.lineTo(0).empty());
    // After node 5, g8f6: the position white castles from.
    REQUIRE(tree.lineTo(6) == std::vector<std::string>(kItalian.begin(), kItalian.end() - 1));
    REQUIRE(tree.lineTo(10) == std::vector<std::string>{ "d2d4", "d7d5" });
}

TEST_CASE("Version 1: every node carries the score of the position its move is played from", "[unit][bookeval]") {
    TempFile file("qapla-book-eval-in.bok");
    TempFile out("qapla-book-eval-out.bok");
    writeBook(file.path);
    auto tree = BookTree::read(file.path);
    tree.setScore(0, 12);       // the start position: e2e4 and d2d4 are played from it
    tree.setScore(1, -30);      // after e2e4: e7e5 and c7c5
    tree.setScore(10, 31990);   // after d2d4 d7d5: c2c4
    QaplaBook::BookHeader header;
    header.version = "engine=E tc=depth: 4";
    tree.writeWithScores(out.path, header, 1);
    REQUIRE_FALSE(std::filesystem::exists(out.path + ".part"));

    SECTION("read back by the tree") {
        const auto evaluated = BookTree::read(out.path);
        REQUIRE(evaluated.header().payloadId == BOOK_SCORE_PAYLOAD_ID);
        REQUIRE(evaluated.scoreVersion() == 1);
        REQUIRE(evaluated.header().version == header.version);
        REQUIRE(evaluated.sameTree(tree));
        REQUIRE(evaluated.score(0) == 12);
        REQUIRE(evaluated.score(1) == -30);
        REQUIRE(evaluated.score(10) == 31990);
        REQUIRE(evaluated.score(2) == BookScore::NONE);
    }

    SECTION("read by the engine's own book, the payload a signed 16 bit score") {
        QaplaBook9::Book<ScorePayload> book;
        REQUIRE(book.readFromFile(out.path));
        const auto e2e4 = book.firstChild(decltype(book)::ROOT);
        const auto d2d4 = book.nextSibling(e2e4);
        REQUIRE(book.payload(e2e4).score == 12);
        REQUIRE(book.payload(d2d4).score == 12);
        const auto e7e5 = book.firstChild(e2e4);
        REQUIRE(book.payload(e7e5).score == -30);
        REQUIRE(book.payload(book.nextSibling(e7e5)).score == -30);
        const auto c2c4 = book.firstChild(book.firstChild(d2d4));
        REQUIRE(book.payload(c2c4).score == 31990);
    }
}

TEST_CASE("Version 2: every node carries the score of the position after its move", "[unit][bookeval]") {
    TempFile file("qapla-book-eval-v2-in.bok");
    TempFile out("qapla-book-eval-v2-out.bok");
    writeBook(file.path);
    auto tree = BookTree::read(file.path);
    // Scores of positions, from the side to move there: the start position, after e2e4 (node 0,
    // black to move), after e1g1 (node 6, a leaf) and after d2d4 d7d5 (node 9).
    tree.setScore(0, 12);
    tree.setScore(1, -30);
    tree.setScore(7, 31990);
    tree.setScore(10, 55);
    QaplaBook::BookHeader header;
    header.version = "engine=E tc=depth: 4";
    tree.writeWithScores(out.path, header, 2);

    SECTION("read back by the tree, the start position from the header") {
        const auto evaluated = BookTree::read(out.path);
        REQUIRE(evaluated.header().payloadId == BOOK_SCORE_V2_PAYLOAD_ID);
        REQUIRE(evaluated.scoreVersion() == 2);
        REQUIRE(evaluated.sameTree(tree));
        REQUIRE(evaluated.score(0) == 12);
        REQUIRE(evaluated.score(1) == -30);
        REQUIRE(evaluated.score(7) == 31990);
        REQUIRE(evaluated.score(10) == 55);
        REQUIRE(evaluated.score(2) == BookScore::NONE);
    }

    SECTION("read by the engine's own book: a node is the position after its move, from its mover") {
        QaplaBook9::Book<ScorePayload> book;
        REQUIRE(book.readFromFile(out.path));
        const auto e2e4 = book.firstChild(decltype(book)::ROOT);
        REQUIRE(book.payload(e2e4).score == 30);
        // Node 6, e1g1 at the end of the first line: black mates in 10 there, white is mated.
        auto node = e2e4;
        for (int depth = 0; depth < 6; ++depth) {
            node = book.firstChild(node);
        }
        REQUIRE(book.payload(node).score == -31990);
        const auto d7d5 = book.firstChild(book.nextSibling(e2e4));
        REQUIRE(book.payload(d7d5).score == -55);
    }

    SECTION("a version 1 book of the same search brings its scores into a version 2 run") {
        TempFile v1("qapla-book-eval-v1-source.bok");
        TempFile v2("qapla-book-eval-v2-from-v1.bok");
        auto source = BookTree::read(file.path);
        for (uint32_t position = 0; position < source.positionCount(); ++position) {
            if (source.isPlayedFrom(position)) {
                source.setScore(position, static_cast<int16_t>(position));
            }
        }
        QaplaBook::BookHeader searched;
        searched.version = BookEvalManager::describeSearch(depthEngine("E"));
        source.writeWithScores(v1.path, searched, 1);

        BookEvalManager manager;
        // Twelve positions, nine of them searched for version 1: only the three leaves are left.
        REQUIRE(manager.initialize({ .bookFile = v1.path, .outFile = v2.path }, depthEngine("E")) == 3);
        REQUIRE(manager.getTakenCount() == 9);
        REQUIRE(manager.getPositionCount() == 12);

        BookEvalManager other;
        TempFile v2other("qapla-book-eval-v2-other.bok");
        REQUIRE(other.initialize({ .bookFile = v1.path, .outFile = v2other.path }, depthEngine("F")) == 12);
        REQUIRE(other.getTakenCount() == 0);
    }
}

TEST_CASE("A book evaluation hands out every position once and keeps the scores", "[unit][bookeval]") {
    TempFile file("qapla-book-eval-run.bok");
    TempFile out("qapla-book-eval-run-out.bok");
    writeBook(file.path);
    const auto engine = depthEngine("E");

    BookEvalManager manager;
    // Version 1 here: the nine positions a move is played from; version 2 would add the three leaves.
    REQUIRE(manager.initialize({ .bookFile = file.path, .outFile = out.path, .version = 1 }, engine) == 9);
    REQUIRE(manager.getPositionCount() == 9);

    std::vector<GameTask> tasks;
    while (auto task = manager.nextTask()) {
        REQUIRE(task->taskType == GameTask::Type::ComputeMove);
        tasks.push_back(*task);
    }
    REQUIRE(tasks.size() == 9);
    REQUIRE_FALSE(manager.isRunFinished());
    REQUIRE(tasks[0].gameRecord.history().empty());
    REQUIRE(tasks[1].gameRecord.history().size() == 1);
    REQUIRE(tasks[1].gameRecord.history()[0].lan_ == "e2e4");

    // Five come back with a score, one without: it stays open.
    for (size_t i = 0; i < 5; ++i) {
        manager.setGameRecord(tasks[i].taskId, answer(tasks[i], static_cast<int>(i) * 10));
    }
    manager.setGameRecord(tasks[5].taskId, tasks[5].gameRecord);
    REQUIRE(manager.getEvaluatedCount() == 5);
    REQUIRE(manager.getFailedCount() == 1);
    REQUIRE_FALSE(manager.isComplete());
    manager.save();

    SECTION("a new run continues with the positions left open") {
        BookEvalManager resumed;
        REQUIRE(resumed.initialize({ .bookFile = file.path, .outFile = out.path, .version = 1 }, engine) == 4);
        REQUIRE(resumed.getEvaluatedCount() == 5);
        size_t handedOut = 0;
        while (auto task = resumed.nextTask()) {
            resumed.setGameRecord(task->taskId, answer(*task, std::nullopt, 2));
            ++handedOut;
        }
        REQUIRE(handedOut == 4);
        REQUIRE(resumed.isComplete());
        resumed.save();
        const auto evaluated = BookTree::read(out.path);
        REQUIRE(evaluated.score(0) == 0);
        REQUIRE(evaluated.score(4) == 40);
        REQUIRE(evaluated.score(6) == 31998);   // mate in 2
    }

    SECTION("maxpositions ends a run early") {
        BookEvalManager limited;
        REQUIRE(limited.initialize({ .bookFile = file.path, .outFile = out.path, .maxPositions = 2,
            .version = 1 }, engine) == 4);
        REQUIRE(limited.nextTask());
        REQUIRE(limited.nextTask());
        REQUIRE_FALSE(limited.nextTask());
    }

    SECTION("it is not continued with another engine or limit") {
        BookEvalManager other;
        REQUIRE_THROWS_AS(other.initialize({ .bookFile = file.path, .outFile = out.path, .version = 1 },
            depthEngine("E", "depth:5")), AppError);
        REQUIRE_THROWS_AS(other.initialize({ .bookFile = file.path, .outFile = out.path, .version = 1 },
            depthEngine("F")), AppError);
        // Nor as the other version.
        REQUIRE_THROWS_AS(other.initialize({ .bookFile = file.path, .outFile = out.path, .version = 2 },
            engine), AppError);
    }

    SECTION("nor from the evaluation of another book") {
        TempFile otherBook("qapla-book-eval-other.bok");
        QaplaBook9::Book<> book;
        book.addLine(movesOf(kSicilian));
        book.writeToFile(otherBook.path);
        BookEvalManager other;
        REQUIRE_THROWS_AS(other.initialize({ .bookFile = otherBook.path, .outFile = out.path, .version = 1 },
            engine), AppError);
    }
}

TEST_CASE("A file that is no 9 bit book is refused", "[unit][bookeval]") {
    TempFile text("qapla-book-eval-text.bok");
    std::ofstream(text.path, std::ios::binary) << "[White \"A\"]\n\n1. e4 *\n";
    REQUIRE_THROWS(BookTree::read(text.path));
    TempFile out("qapla-book-eval-text-out.bok");
    BookEvalManager manager;
    REQUIRE_THROWS_AS(manager.initialize({ .bookFile = text.path, .outFile = out.path }, depthEngine("E")), AppError);
}
