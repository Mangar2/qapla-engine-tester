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
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#include "epd-test.h"
#include "game-manager.h"
#include "game-manager-pool.h"
#include "game-state.h"
#include "string-helper.h"

namespace QaplaTester {

void EpdTest::initialize(const EpdTestResult& tests)
{
    result_ = tests;
	updateCnt_++;
    testIndex_ = 0;
    oldestIndexInUse_ = 0;
}

void EpdTest::schedule(const std::shared_ptr<EpdTest>& self, const EngineConfig& engine, 
    GameManagerPool& pool) {
    pool.addTaskProvider(self, engine); 
    pool.startManagers();
}

void EpdTest::continueAnalysis() {
    testIndex_ = oldestIndexInUse_.load();
}

std::optional<GameTask> EpdTest::nextTask() {
    std::scoped_lock lock(testResultMutex_);


    GameTask task;
    task.taskType = GameTask::Type::ComputeMove;
    
    GameState gameState;
    while (testIndex_ < result_.result.size() && result_.result[testIndex_].tested) {
        ++testIndex_;
    }
    if (testIndex_ >= result_.result.size()) {
        return std::nullopt;
    }
	auto& test = result_.result[testIndex_];
    task.gameRecord.setPositionName(test.id);
    gameState.setFen(false, test.fen);
    auto correctedFen = gameState.getFen();
    task.gameRecord.setStartPosition(false, correctedFen, gameState.isWhiteToMove(), 
        gameState.getStartHalfmoves(), "", "");
    task.gameRecord.setTimeControl(result_.tc_, result_.tc_);
    task.taskId = std::to_string(testIndex_);
    testIndex_++;

    return task;
}

bool EpdTest::setPV(const std::string& taskId,
    const std::vector<std::string>& pv,
    uint64_t timeInMs,
    std::optional<uint32_t> depth,
    std::optional<uint64_t> nodes,
    [[maybe_unused]] std::optional<uint32_t> multipv) 
{
    if (pv.empty()) {
        return false;
    }

    const auto taskIndex = QaplaHelpers::to_uint32(taskId);
    std::scoped_lock lock(testResultMutex_);

    if (!taskIndex || *taskIndex >= result_.result.size()) {
        return false;
    }

    auto& test = result_.result[*taskIndex];
    // We may have an info line from the engine after the move was already played
    if (test.tested) {
        return false; 
    }
    assert(test.playedMove.empty());

    const std::string& firstMove = pv.front();
    bool found = std::ranges::any_of(test.bestMoves,
        [&](const std::string& bm) {
            return isSameMove(test.fen, firstMove, bm);
        });

    if (found) {
        if (test.correctAtDepth == -1 && depth.has_value()) {
            test.correctAtDepth = static_cast<int>(depth.value());
        }
        if (test.correctAtTimeInMs == 0) {
            test.correctAtTimeInMs = timeInMs;
        }
        if (test.correctAtNodeCount == 0 && nodes.has_value()) {
            test.correctAtNodeCount = nodes.value();
        }
    }
    else {
        test.correctAtDepth = -1;
        test.correctAtTimeInMs = 0;
        test.correctAtNodeCount = 0;
    }

    bool earlyStop =
		test.seenPlies > 0 && test.correctAtDepth >= 0 && depth.has_value() &&
        timeInMs >= test.minTimeInS * 1000 &&
        static_cast<int>(*depth) - test.correctAtDepth >= test.seenPlies;

    return earlyStop;
}

void EpdTest::setGameRecord(const std::string& taskId, const GameRecord& record) {
    const std::string& fen = record.getStartFen();

    const auto& moves = record.history();
    if (moves.empty()) {
        return;
    }

    const auto& move = moves.back();
    const std::string& played = move.san_.empty() ? move.lan_ : move.san_;

    const auto taskIndex = QaplaHelpers::to_uint32(taskId);
    if (!taskIndex || (*taskIndex >= result_.result.size())) {
        Logger::testLogger().log(
            "EpdTest::setGameRecord: Invalid taskId " + taskId,
            TraceLevel::error);
		return;
    }

    auto recentOldestIndex = oldestIndexInUse_.load();
    {
        std::scoped_lock lock(testResultMutex_);
        auto& test = result_.result[*taskIndex];
        assert(test.playedMove.empty());

        test.tested = true;
        test.playedMove = played;
        test.correct = std::ranges::any_of(
            test.bestMoves,
            [&](const std::string& bm) {
                return isSameMove(fen, played, bm);
            }
        );
        test.timeMs = move.timeMs;
        test.searchDepth = static_cast<int>(move.depth);
        test.nodeCount = move.nodes;

        // Note: SetPV might have set the correct depth, time, nodes already
        if (test.correct && test.correctAtDepth == -1) {
            test.correctAtDepth = static_cast<int>(move.depth);
            test.correctAtTimeInMs = move.timeMs;
            test.correctAtNodeCount = move.nodes;
        }

        // If SetPV saw the correct move but it was finally not played, we need to remove the former result
        if (!test.correct) {
            test.correctAtDepth = -1;
            test.correctAtTimeInMs = 0;
            test.correctAtNodeCount = 0;
        }

        // Close gap, if oldest
        if (*taskIndex == oldestIndexInUse_) {
            while (oldestIndexInUse_ < result_.result.size() && !result_.result[oldestIndexInUse_].playedMove.empty()) {
                ++oldestIndexInUse_;
            }
        }
        updateCnt_++;
    }
    if (recentOldestIndex != oldestIndexInUse_ && testResultCallback_) {
        testResultCallback_(this, recentOldestIndex, oldestIndexInUse_);
    }
}

bool EpdTest::isSameMove(const std::string& fen, const std::string& move1str, const std::string& move2str) {
    GameState gameState;
    gameState.setFen(false, fen);
    auto move1 = gameState.stringToMove(move1str, false);
    auto move2 = gameState.stringToMove(move2str, false);
    return move1 == move2;
}

} // namespace QaplaTester
