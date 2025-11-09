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
#pragma once

#include "game-task.h"
#include "game-record.h"
#include "logger.h"
#include "engine-report.h"

#include <vector>
#include <optional>
#include <string>
#include <sstream>

namespace QaplaTester {

struct EpdTest {
    std::string fen;
    std::string expectedMove;
    std::string topic;
    bool whiteToPlay;
};

/**
 * @class EpdTestManager
 * @brief Provides predefined EPD test positions for engine validation.
 *
 * Supplies tasks with specific FEN positions and expected best moves. Verifies
 * the engine's output against the expected move and logs the result, including
 * computation depth and time.
 */
class EpdTestManager : public GameTaskProvider {
public:
    EpdTestManager(EngineReport* checklist) : checklist_(checklist) {
        tests_ = {
            { .fen = "8/8/p1p5/1p5p/1P5p/8/PPP2K1p/4R1rk w - - 0 1", .expectedMove = "e1f1", .topic = "zugzwang", .whiteToPlay = true},
            { .fen = "1q1k4/2Rr4/8/2Q3K1/8/8/8/8 w - - 0 1", .expectedMove = "g5h6", .topic = "zugzwang", .whiteToPlay = true},
            { .fen = "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", .expectedMove = "d6d1", .topic = "mate", .whiteToPlay = false },
            { .fen = "8/8/8/1k6/4K3/2R5/8/8 w - - 0 1", .expectedMove = "e4d5", .topic = "KRK", .whiteToPlay = false },
            { .fen = "8/8/1k6/8/4K3/2N5/2B5/8 w - - 0 1", .expectedMove = "e4d5", .topic = "KBNK", .whiteToPlay = false },
            { .fen = "6r1/1p3k2/pPp4R/K1P1p1p1/1P2Pp1p/5P1P/6P1/8 w - - 0 1", .expectedMove = "h6c6", .topic = "passed pawn", .whiteToPlay = true }
        };
    }

    /**
     * @brief Adds an EPD test case to the internal list.
     * @param fen The FEN string representing the position.
     * @param expectedMove The expected best move in LAN notation.
     * @param whiteToPlay True if it is white to move, false for black.
     */
    void addTest(const std::string& fen, const std::string& expectedMove, 
        const std::string& topic, bool whiteToPlay) {
        tests_.push_back({ .fen = fen, .expectedMove = expectedMove, .topic = topic, 
            .whiteToPlay = whiteToPlay });
    }

    /**
     * @brief Returns the next task for the engine to process.
     * @return Optional GameTask with position and time control, or std::nullopt if done.
     */
    std::optional<GameTask> nextTask() override {
        if (currentIndex_ >= tests_.size()) {
            return std::nullopt;
        }

        const auto& test = tests_[currentIndex_];
        GameTask task;
        TimeControl t;
        t.setMoveTime(5000);
        task.taskType = GameTask::Type::ComputeMove;
		task.gameRecord.setStartPosition(
			false, test.fen, test.whiteToPlay, 0, "", "");
        task.gameRecord.setTimeControl(t, t);
        Logger::testLogger().log("Fen: " + test.fen + " topic: " + test.topic + " expected: " + test.expectedMove, TraceLevel::info);
        ++currentIndex_;

        return task;
    }

    /**
     * @brief Records the result of a finished task identified by taskId.
     *
     * @param taskId Identifier of the task this task result belongs to.
     * @param record Game outcome and metadata.
     */
    void setGameRecord(
        [[maybe_unused]] const std::string & taskId, 
        const GameRecord & record) override 
    {
        if (currentIndex_ == 0 || currentIndex_ > tests_.size()) {
            return;
        }

        const auto& test = tests_[currentIndex_ - 1];
        const auto& history = record.history();

        if (!history.empty()) {
            const auto& lastMove = history.back();
            bool success = (lastMove.lan_ == test.expectedMove);
            std::ostringstream oss;
            oss << test.fen << " topic " << test.topic  
                << " | Expected: " << test.expectedMove
                << ", Got: " << lastMove.lan_
                << ", Depth: " << lastMove.depth
                << ", Time: " << lastMove.timeMs << "ms\n";
            
            checklist_->logReport("epd-expected-moves", success, oss.str());
        }
    }

private:
    std::vector<EpdTest> tests_;
    size_t currentIndex_ = 0;
    EngineReport* checklist_;
};

} // namespace QaplaTester
