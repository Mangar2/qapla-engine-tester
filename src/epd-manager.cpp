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
#include "game-manager-pool.h"
#include "game-state.h"

void EpdManager::printHeaderLine() const {
    auto formatEngineName = [](const std::string& name) -> std::string {
        constexpr int totalWidth = 25;
        if (static_cast<int>(name.length()) > totalWidth) {
            return "..." + name.substr(name.length() - (totalWidth - 3));
        }
        int padding = totalWidth - static_cast<int>(name.length());
        int leftPad = padding / 2;
        int rightPad = padding - leftPad;
        return std::string(leftPad, ' ') + name + std::string(rightPad, ' ');
        };

    std::ostringstream header;
    header << std::setw(20) << std::left << "TestId";

    for (const auto& result : results_) {
        if (result.engineName != engineName_ && result.testSetName == epdFileName_) {
            header << "|" << formatEngineName(result.engineName);
        }
    }

    header << "|" << formatEngineName(engineName_) << "| BM:";
    Logger::testLogger().log(header.str(), TraceLevel::result);
}

void EpdManager::printTestResultLine(const EpdTestCase& current) const {
    std::ostringstream line;
    line << std::setw(20) << std::left << current.id;

    for (const auto& result : results_) {
        if (result.engineName != engineName_ && result.testSetName == epdFileName_) {
            const auto it = std::find_if(result.results.begin(), result.results.end(), [&](const EpdTestCase& t) {
                return t.id == current.id;
                });
            if (it != result.results.end()) {
                line << formatInlineResult(*it);
            }
            else {
                line << "|" << std::setw(23) << "-";
            }
        }
    }

    line << formatInlineResult(current);

    line << "| BM: ";
    for (const auto& bm : current.bestMoves) {
        line << bm << " ";
    }

	Logger::testLogger().log(line.str(), TraceLevel::result);
}

std::string EpdManager::formatTime(uint64_t ms) const {
    if (ms == 0) return "-";
    uint64_t seconds = (ms / 1000);
    uint64_t milliseconds = ms % 1000;
    std::ostringstream timeStream;
    timeStream << std::setfill('0')
        << std::setw(2) << seconds << "."
        << std::setw(3) << milliseconds;
    return timeStream.str();
}

std::string EpdManager::formatInlineResult(const EpdTestCase& test) const {
    std::ostringstream out;
    out << "|" << std::setw(8) << std::right << (test.correct ? formatTime(test.correctAtTimeInMs) : "-")
        << ", D:" << std::setw(3) << std::right << (test.correct ? std::to_string(test.correctAtDepth) : "-")
        << ", M: " << std::setw(5) << std::left << test.playedMove;
    return out.str();
}

inline std::ostream& operator<<(std::ostream& os, const EpdTestCase& test) {
    auto formatTime = [](uint64_t ms) -> std::string {
        if (ms == 0) return "-";
        uint64_t seconds = (ms / 1000);
        uint64_t milliseconds = ms % 1000;
        std::ostringstream timeStream;
        timeStream << std::setfill('0')  
            << std::setw(2) << seconds << "."
            << std::setw(3) << milliseconds;
        return timeStream.str();
        };

    os << std::setw(20) << std::left << test.id
        << "|" << std::setw(8) << std::right
        << (test.correct ? formatTime(test.correctAtTimeInMs) : "-")
        << ", D:" << std::setw(3) << std::right
        << (test.correct ? std::to_string(test.correctAtDepth) : "-")
        << ", M: " << std::setw(5) << std::left << test.playedMove
        << " | BM: ";

    for (const auto& bm : test.bestMoves) {
        os << bm << " ";
    }
    return os;
}

void EpdManager::initializeTestCases(int maxTimeInS, int minTimeInS, int seenPlies, bool clearTests) {
    if (!reader_) {
        throw std::runtime_error("EpdReader must be initialized before loading test cases.");
    }

    if (clearTests) {
        tests_.clear();
    }
    reader_->reset();

    while (true) {
        auto testCase = nextTestCaseFromReader();
        if (!testCase) {
            break;
        }
		testCase->maxTimeInS = maxTimeInS;
		testCase->minTimeInS = minTimeInS;
		testCase->seenPlies = seenPlies;
        tests_.push_back(std::move(*testCase));
    }
}

void EpdManager::analyzeEpd(const std::string& filepath, const EngineConfig& engine, 
    uint32_t concurrency, int maxTimeInS, int minTimeInS, int seenPlies)
{
	engineName_ = engine.getName();
	epdFileName_ = filepath;
    bool sameFile = reader_ && reader_->getFilePath() == filepath;
    if (!sameFile) {
        reader_ = std::make_unique<EpdReader>(filepath);
    }

    initializeTestCases(maxTimeInS, minTimeInS, seenPlies);
    currentIndex_ = 0;
    oldestIndexInUse_ = 0;
    tc.setMoveTime(maxTimeInS * 1000);
    printHeaderLine();
	GameManagerPool::getInstance().setConcurrency(concurrency, true);
	GameManagerPool::getInstance().addTaskProvider(this, engine, static_cast<int>(reader_->all().size()));
}

bool EpdManager::wait() {
    GameManagerPool::getInstance().waitForTask(this);
    results_.push_back({
        engineName_,
        epdFileName_,
        tests_
        });
	return true;
}

std::optional<GameTask> EpdManager::nextTask(const std::string& whiteId, const std::string& blackId) {
    assert(whiteId == blackId); 
    std::lock_guard<std::mutex> lock(taskMutex_);
    if (currentIndex_ >= static_cast<int>(tests_.size())) {
        return std::nullopt;
    }

    GameTask task;
    task.taskType = GameTask::Type::ComputeMove;
    task.useStartPosition = false;
    task.whiteTimeControl = tc;
    task.blackTimeControl = tc;

	tests_[currentIndex_].engineId = whiteId;
    GameState gameState;
    gameState.setFen(false, tests_[currentIndex_].fen);
    auto correctedFen = gameState.getFen();
    task.fen = correctedFen;
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

        bool earlyStop =
			test.seenPlies >= 0 && test.correctAtDepth >= 0 && depth.has_value() &&
            timeInMs >= test.minTimeInS * 1000 &&
            static_cast<int>(*depth) - test.correctAtDepth >= test.seenPlies;

        return earlyStop;
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
				printTestResultLine(tests_[oldestIndexInUse_]);
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

double EpdManager::getSuccessRate() const {
	int totalTests = 0;
	int correctTests = 0;
	for (const auto& result : results_) {
		if (result.engineName != engineName_ || result.testSetName != epdFileName_) {
			continue;
		}
		for (const auto& test : result.results) {
			++totalTests;
			if (test.correct) {
				++correctTests;
			}
		}
	}
	return totalTests > 0 ? static_cast<double>(correctTests) / totalTests : 0.0;
}