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

#include "../../epd/epd-manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace QaplaTester;

namespace {

constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";

/**
 * @brief An EPD file in the temp directory, removed again when the test is done.
 */
class TempEpdFile {
public:
    TempEpdFile(const std::string& name, const std::vector<std::string>& ids)
        : path_(std::filesystem::temp_directory_path() / name) {
        std::ofstream out(path_, std::ios::trunc);
        for (const auto& id : ids) {
            out << START_FEN << " bm e4; id \"" << id << "\";\n";
        }
    }
    ~TempEpdFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    TempEpdFile(const TempEpdFile&) = delete;
    TempEpdFile& operator=(const TempEpdFile&) = delete;

    [[nodiscard]] std::string path() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

/**
 * @brief A manager holding the positions of the file and the stored results loaded onto them.
 */
std::unique_ptr<EpdManager> managerWith(const TempEpdFile& file, const std::string& storedResults) {
    auto manager = std::make_unique<EpdManager>();
    manager->initialize(file.path(), 10, 1, 3, 0, 0);
    std::istringstream in(storedResults);
    REQUIRE(manager->loadResults(in));
    return manager;
}

std::string saved(const EpdManager& manager) {
    std::ostringstream out;
    manager.saveResults(out);
    return out.str();
}

/**
 * @brief The lines of a text, the header line left out.
 */
std::vector<std::string> bodyLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string storedLine(const std::string& id, const std::string& move) {
    return id + " |  0.50, D: 7, M: " + move + " | BM: e4\n";
}

} // namespace

TEST_CASE("Stored EPD results are put onto the positions they belong to", "[unit][epd]") {
    const TempEpdFile file("qapla-epd-order.epd", { "a", "b", "c", "d" });
    // Out of order, one position missing (c), one line for a position the set does not have.
    const auto manager = managerWith(file, "TestId | Engine\n"
        + storedLine("d", "d4") + storedLine("x", "h4") + storedLine("a", "e4") + storedLine("b", "c4"));

    const auto results = manager->getResultsCopy();
    REQUIRE(results.size() == 1);
    const auto& cases = results[0].result;
    REQUIRE(cases.size() == 4);
    CHECK(cases[0].id == "a");
    CHECK(cases[0].playedMove == "e4");
    CHECK(cases[1].id == "b");
    CHECK(cases[1].playedMove == "c4");
    CHECK(cases[2].id == "c");
    CHECK_FALSE(cases[2].tested);
    CHECK(cases[3].id == "d");
    CHECK(cases[3].playedMove == "d4");

    // Written back in the order of the set, the missing position marked as having no result.
    const auto lines = bodyLines(saved(*manager));
    REQUIRE(lines.size() == 4);
    CHECK(lines[0].starts_with("a "));
    CHECK(lines[0].find("M: e4") != std::string::npos);
    CHECK(lines[3].starts_with("d "));
    CHECK(lines[3].find("M: d4") != std::string::npos);
}

TEST_CASE("An id that occurs twice is matched line by line in file order", "[unit][epd]") {
    const TempEpdFile file("qapla-epd-duplicate.epd", { "dup", "other", "dup" });
    const auto manager = managerWith(file, "TestId | Engine\n"
        + storedLine("dup", "e4") + storedLine("other", "c4") + storedLine("dup", "d4"));

    const auto cases = manager->getResultsCopy()[0].result;
    REQUIRE(cases.size() == 3);
    CHECK(cases[0].playedMove == "e4");
    CHECK(cases[1].playedMove == "c4");
    CHECK(cases[2].playedMove == "d4");
}

TEST_CASE("Saved EPD results load back unchanged", "[unit][epd]") {
    const TempEpdFile file("qapla-epd-roundtrip.epd", { "a", "b", "c" });
    const auto first = managerWith(file, "TestId | Engine A | Engine B\n"
        "a |  0.50, D: 7, M: e4 |  1.25, D: 9, M: e4 | BM: e4\n"
        "b |  -, D: -, M: d4 |  0.10, D: 3, M: e4 | BM: e4\n"
        "c |  2.00, D: 12, M: e4 |  -, D: -, M: c4 | BM: e4\n");
    const auto text = saved(*first);

    const auto second = managerWith(file, text);
    CHECK(saved(*second) == text);
}

TEST_CASE("Initializing again replaces the positions instead of adding to them", "[unit][epd]") {
    const TempEpdFile file("qapla-epd-reinit.epd", { "a", "b" });
    EpdManager manager;
    manager.initialize(file.path(), 10, 1, 3, 0, 0);
    manager.initialize(file.path(), 10, 1, 3, 0, 0);
    std::istringstream in("TestId | Engine\n" + storedLine("a", "e4"));
    REQUIRE(manager.loadResults(in));

    CHECK(manager.getResultsCopy()[0].result.size() == 2);
    CHECK(bodyLines(saved(manager)).size() == 2);
}

TEST_CASE("A large position set is loaded and saved in time linear in its size", "[unit][epd]") {
    // Searching the set by id for every position made both quadratic: 600 000 positions never
    // finished saving and froze the GUI that autosaves them. 100 000 positions would take minutes
    // that way; linear, they take well under a second in a release build.
    constexpr size_t COUNT = 100000;
    std::vector<std::string> ids;
    ids.reserve(COUNT);
    std::string stored = "TestId | Engine\n";
    for (size_t index = 0; index < COUNT; ++index) {
        ids.push_back("part-03b.pgn:" + std::to_string(index) + ":52");
        stored += storedLine(ids.back(), "e4");
    }
    const TempEpdFile file("qapla-epd-large.epd", ids);

    const auto start = std::chrono::steady_clock::now();
    const auto manager = managerWith(file, stored);
    const auto lines = bodyLines(saved(*manager));
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    REQUIRE(lines.size() == COUNT);
    CHECK(lines.back().starts_with(ids.back()));
    CHECK(seconds < 30.0);
}

TEST_CASE("A position not analysed yet is still to be analysed after saving and loading", "[unit][epd]") {
    // The autosave of the GUI writes the results and reads them back at the next start. A position
    // that came back marked analysed was never searched when the run went on.
    const TempEpdFile file("qapla-epd-untested.epd", { "a", "b" });
    const auto first = managerWith(file, "TestId | Engine\n" + storedLine("a", "e4"));
    REQUIRE_FALSE(first->getResultsCopy()[0].result[1].tested);

    const auto second = managerWith(file, saved(*first));
    const auto cases = second->getResultsCopy()[0].result;
    CHECK(cases[0].tested);
    CHECK(cases[0].playedMove == "e4");
    CHECK_FALSE(cases[1].tested);
}

namespace {

/**
 * @brief An engine's answer to a position: the start position with the move it played.
 */
GameRecord playedFromStart(const std::string& san) {
    GameRecord record;
    record.setStartPosition(false, START_FEN, true, 0);
    MoveRecord move(1);
    move.san_ = san;
    move.depth = 9;
    move.timeMs = 120;
    record.addMove(move);
    return record;
}

} // namespace

TEST_CASE("An EPD test hands out only what changed since the caller looked", "[unit][epd]") {
    EpdTestResult tests;
    tests.engineName = "Engine";
    for (const auto* id : { "a", "b", "c" }) {
        EpdTestCase testCase;
        testCase.id = id;
        testCase.fen = START_FEN;
        testCase.bestMoves = { "e4" };
        tests.result.push_back(testCase);
    }
    EpdTest test;
    test.initialize(tests);

    // Nothing seen yet: everything.
    auto changes = test.getChangesSince(0);
    REQUIRE(changes.complete);
    CHECK(changes.all.result.size() == 3);
    const auto seen = changes.updateCount;

    // Nothing finished since: nothing.
    changes = test.getChangesSince(seen);
    CHECK_FALSE(changes.complete);
    CHECK(changes.changed.empty());

    // Two positions finished: exactly those two, with their results.
    test.setGameRecord("1", playedFromStart("e4"));
    test.setGameRecord("2", playedFromStart("d4"));
    changes = test.getChangesSince(seen);
    REQUIRE_FALSE(changes.complete);
    REQUIRE(changes.changed.size() == 2);
    CHECK(changes.changed[0].first == 1);
    CHECK(changes.changed[0].second.playedMove == "e4");
    CHECK(changes.changed[0].second.correct);
    CHECK(changes.changed[1].first == 2);
    CHECK_FALSE(changes.changed[1].second.correct);

    // Asked again from there: only what came after.
    const auto later = changes.updateCount;
    test.setGameRecord("0", playedFromStart("e4"));
    changes = test.getChangesSince(later);
    REQUIRE(changes.changed.size() == 1);
    CHECK(changes.changed[0].first == 0);

    // Replaced: the caller's copy is void, everything again.
    test.initialize(tests);
    changes = test.getChangesSince(changes.updateCount);
    CHECK(changes.complete);
    CHECK(changes.all.result.size() == 3);
}
