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

#include "epd-manager.h"
#include "engine-worker-factory.h"
#include "game-manager.h"
#include "game-state.h"

void EpdManager::initializeTestCases() {
    if (!reader_) {
        throw std::runtime_error("EpdReader must be initialized before loading test cases.");
    }

    tests_.clear();
    reader_->reset();

    while (true) {
        auto testCase = nextTestCaseFromReader();
        if (!testCase) {
            break;
        }
        tests_.push_back(std::move(*testCase));
    }
}

void EpdManager::analyzeEpd(const std::string& filepath, const std::string& enginepath, 
    uint32_t concurrency,
    uint64_t maxTimeInS) {
	if (managers_.size() > 0) {
		throw "reuse not supported yet";
	}
	reader_ = std::make_unique<EpdReader>(filepath);
    initializeTestCases();
    currentIndex_ = 0;
    oldestIndexInUse_ = 0;
	tc.setMoveTime(maxTimeInS * 1000); 

    auto engineList = EngineWorkerFactory::createUci(enginepath, std::nullopt, concurrency);
	for (auto& engine : engineList) {
		auto manager = std::make_unique<GameManager>();
        manager->setUniqueEngine(std::move(engine));
        manager->computeTasks(this);
        managers_.push_back(std::move(manager));
	}
}

void EpdManager::stop() {
	for (auto& manager : managers_) {
		manager->stop();
	}
}

bool EpdManager::wait() {
	for (auto& manager : managers_) {
		manager->getFinishedFuture().wait();
	}
	return true;
}

std::optional<GameTask> EpdManager::nextTask(const std::string& whiteId, const std::string& blackId) {
    assert(whiteId == blackId); 
    std::lock_guard<std::mutex> lock(taskMutex_);
    if (currentIndex_ >= tests_.size()) {
        return std::nullopt;
    }

    GameTask task;
    task.taskType = GameTask::Type::ComputeMove;
    task.useStartPosition = false;
    task.whiteTimeControl = tc;
    task.blackTimeControl = tc;

	tests_[currentIndex_].engineId = whiteId;
    task.fen = tests_[currentIndex_].fen;
    currentIndex_++;

    return task;
}

bool EpdManager::setPV(const std::string& engineId,
    const std::vector<std::string>& pv,
    uint64_t timeInMs,
    std::optional<uint32_t> depth,
    std::optional<uint64_t> nodes,
    [[maybe_unused]] std::optional<uint32_t> multipv) 
{
    if (pv.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(taskMutex_);
    for (int i = currentIndex_ - 1; i >= oldestIndexInUse_; --i) {
        auto& test = tests_[i];
        if (test.engineId != engineId) {
            continue;
        }
        assert(test.playedMove.empty());

        const std::string& firstMove = pv.front();
        bool found = std::any_of(test.bestMoves.begin(), test.bestMoves.end(),
            [&](const std::string& bm) {
                return isSameMove(test.fen, firstMove, bm);
            });

        if (found) {
            if (test.correctAtDepth == -1 && depth.has_value()) {
                test.correctAtDepth = static_cast<int>(depth.value());
            }
            if (test.correctAtTimeInMs == 0) {
                test.correctAtTimeInMs = static_cast<int>(timeInMs);
            }
            if (test.correctAtNodeCount == 0 && nodes.has_value()) {
                test.correctAtNodeCount = static_cast<int>(nodes.value());
            }
        }
        else {
			test.correctAtDepth = -1;
			test.correctAtTimeInMs = 0;
			test.correctAtNodeCount = 0;
        }

        return false;
    }

    return false;
}


void EpdManager::setGameRecord(const std::string& whiteId, const std::string& blackId, const GameRecord& record) {
    assert(whiteId == blackId);
    const std::string& fen = record.getStartFen();
    const auto& moves = record.history();
    if (moves.empty()) return;

    const auto& move = moves.back();
    const std::string& played = move.lan;

    std::lock_guard<std::mutex> lock(taskMutex_);
    for (int i = currentIndex_ - 1; i >= oldestIndexInUse_; --i) {
        auto& test = tests_[i];
        if (test.engineId != whiteId) {
            continue;
        }
        assert(test.playedMove.empty());

        test.playedMove = played;
        test.correct = std::any_of(
            test.bestMoves.begin(), test.bestMoves.end(),
            [&](const std::string& bm) {
                return isSameMove(fen, played, bm);
            }
        );
        test.timeMs = move.timeMs;
		test.searchDepth = move.depth;
        test.nodeCount = move.nodes;
		// Note: SetPV might have set the correct depth, time, nodes already
        if (test.correct && test.correctAtDepth == -1) {
            test.correctAtDepth = move.depth;
            test.correctAtTimeInMs = move.timeMs;
			test.correctAtNodeCount = move.nodes;
        }
        // If SetPV saw the correct move but it was finally not played we need to remove the former result
        if (!test.correct) {
            test.correctAtDepth = -1;
            test.correctAtTimeInMs = 0;
            test.correctAtNodeCount = 0;
        }
        // Close gap, if oldest
        if (i == oldestIndexInUse_) {
            while (oldestIndexInUse_ < tests_.size() && !tests_[oldestIndexInUse_].playedMove.empty()) {
                std::cout << tests_[oldestIndexInUse_] << std::endl;
                ++oldestIndexInUse_;
            }
        }
        break;
    }
}

std::optional<EpdTestCase> EpdManager::nextTestCaseFromReader() {
    if (!reader_) {
        return std::nullopt;
    }

    auto entryOpt = reader_->next();
    if (!entryOpt) {
        return std::nullopt;
    }

    const auto& entry = *entryOpt;
    EpdTestCase testCase;
    testCase.fen = entry.fen;
    testCase.original = entry;

    auto it = entry.operations.find("id");
    if (it != entry.operations.end() && !it->second.empty()) {
        testCase.id = it->second.front();
    }

    auto bmIt = entry.operations.find("bm");
    if (bmIt != entry.operations.end()) {
        testCase.bestMoves = bmIt->second;
    }

    return testCase;
}


bool EpdManager::isSameMove(const std::string& fen, const std::string& lanMove, const std::string& sanMove) const {
    GameState gameState;
    gameState.setFen(false, fen);
	auto lMove = gameState.stringToMove(lanMove, true);
    auto sMove = gameState.stringToMove(sanMove, false);
    return lMove == sMove;
}