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

#include "../../analysis/analysis-manager.h"
#include "../../base-elements/time-control.h"
#include "../../engine-handling/engine-config.h"
#include "../../game-manager/game-state.h"

#include <string>
#include <vector>

using namespace QaplaTester;

namespace {

/**
 * @brief Builds a game the way a PGN reaches the caller: moves in SAN, nothing else filled in.
 */
GameRecord sanGame(const std::vector<std::string>& sanMoves) {
    GameRecord game;
    game.setStartPosition(true, "", true, 0, "White Player", "Black Player");
    for (size_t ply = 0; ply < sanMoves.size(); ++ply) {
        MoveRecord move(static_cast<uint32_t>(ply + 1));
        move.san_ = sanMoves[ply];
        game.addMove(move);
    }
    game.setNextMoveIndex(static_cast<uint32_t>(sanMoves.size()));
    return game;
}

EngineConfig movetimeEngine(const std::string& name, const std::string& tc) {
    EngineConfig engine;
    engine.setName(name);
    engine.setCmd("fictional_engine");
    if (!tc.empty()) {
        engine.setTimeControl(tc);
    }
    return engine;
}

} // namespace

TEST_CASE("AnalysisManager takes games without reading a file", "[unit][analysis]") {
    AnalysisManager manager;
    const auto count = manager.initialize(
        { sanGame({ "e4", "e5", "Nf3" }), sanGame({ "d4", "d5" }) }, AnalysisDirection::Backward);

    REQUIRE(count == 2);
    REQUIRE(manager.getGameCount() == 2);
}

TEST_CASE("A game that does not play out is left out", "[unit][analysis]") {
    AnalysisManager manager;
    const auto count = manager.initialize(
        { sanGame({ "e4", "e5" }), sanGame({ "e4", "Qh8" }) }, AnalysisDirection::Backward);

    REQUIRE(count == 1);
}

TEST_CASE("The games handed in are handed out one task at a time", "[unit][analysis]") {
    AnalysisManager manager;
    REQUIRE(manager.initialize({ sanGame({ "e4", "e5", "Nf3" }) }, AnalysisDirection::Backward) == 1);
    manager.startRun(movetimeEngine("Engine", "movetime(ms):50"));

    const auto first = manager.nextTask();
    REQUIRE(first);
    CHECK(first->taskType == GameTask::Type::ReplayBackward);
    CHECK(first->gameRecord.history().size() == 3);
    CHECK(first->gameRecord.getWhiteTimeControl().moveTimeMs() == 50);
    // The moves must reach the engine as a replay needs them: parsed, and in the notation the
    // mover would have sent. A game read from a PGN carries neither.
    for (const auto& move : first->gameRecord.history()) {
        CHECK_FALSE(move.move.isEmpty());
        CHECK_FALSE(move.original.empty());
    }

    CHECK_FALSE(manager.nextTask());
}

TEST_CASE("The forward direction is handed out as a forward replay", "[unit][analysis]") {
    AnalysisManager manager;
    REQUIRE(manager.initialize({ sanGame({ "e4", "e5" }) }, AnalysisDirection::Forward) == 1);
    manager.startRun(movetimeEngine("Engine", "movetime(ms):50"));

    const auto task = manager.nextTask();
    REQUIRE(task);
    CHECK(task->taskType == GameTask::Type::ReplayForward);
}

TEST_CASE("An engine without a per-move limit is refused", "[unit][analysis]") {
    CHECK_THROWS(AnalysisManager::requirePerMoveLimit(movetimeEngine("Engine", "")));
    CHECK_NOTHROW(AnalysisManager::requirePerMoveLimit(movetimeEngine("Engine", "depth:8")));
    CHECK_NOTHROW(AnalysisManager::requirePerMoveLimit(movetimeEngine("Engine", "nodes:1000")));
    CHECK_NOTHROW(AnalysisManager::requirePerMoveLimit(movetimeEngine("Engine", "movetime(ms):50")));
}
