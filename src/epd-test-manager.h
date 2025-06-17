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

#include <vector>
#include <optional>
#include <string>
#include <sstream>
#include "game-task.h"
#include "game-record.h"
#include "logger.h"

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
    EpdTestManager(Checklist* checklist) : checklist_(checklist) {
        tests_ = {
            { "8/8/p1p5/1p5p/1P5p/8/PPP2K1p/4R1rk w - - 0 1", "e1f1", "zugzwang", true},
            { "1q1k4/2Rr4/8/2Q3K1/8/8/8/8 w - - 0 1", "g5h6", "zugzwang", true},
            { "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", "d6d1", "mate", false },
            { "8/8/8/1k6/4K3/2R5/8/8 w - - 0 1", "e4d5", "KRK", false },
            { "8/8/1k6/8/4K3/2N5/2B5/8 w - - 0 1", "e4d5", "KBNK", false },
            { "6r1/1p3k2/pPp4R/K1P1p1p1/1P2Pp1p/5P1P/6P1/8 w - - 0 1", "h6c6", "passed pawn", true }
        };
    }

    /**
     * @brief Adds an EPD test case to the internal list.
     * @param fen The FEN string representing the position.
     * @param expectedMove The expected best move in LAN notation.
     * @param whiteToPlay True if it is white to move, false for black.
     */
    void addTest(const std::string& fen, const std::string& expectedMove, const std::string& topic, bool whiteToPlay) {
        tests_.push_back({ fen, expectedMove, topic, whiteToPlay });
    }

    /**
     * @brief Returns the next task for the engine to process.
     * @return Optional GameTask with position and time control, or std::nullopt if done.
     */
    std::optional<GameTask> nextTask(
        [[maybe_unused]] const std::string& whiteId, 
        [[maybe_unused]] const std::string& blackId) override {
        if (currentIndex_ >= tests_.size()) {
            return std::nullopt;
        }

        const auto& test = tests_[currentIndex_];
        GameTask task;
        task.taskType = GameTask::Type::ComputeMove;
        task.useStartPosition = false;
        task.fen = test.fen;
        TimeControl t;
		t.setMoveTime(5000);
        task.whiteTimeControl = t;
        task.blackTimeControl = t;
        Logger::testLogger().log("Fen: " + test.fen + " topic: " + test.topic + " expected: " + test.expectedMove, TraceLevel::info);
        ++currentIndex_;

        return task;
    }

    /**
     * @brief Evaluates the result of a completed task.
     * @param whiteId The identifier for the white player.
     * @param blackId The identifier for the black player.
     * @param record The game record containing the engine's computed moves.
     */
    void setGameRecord(
        [[maybe_unused]] const std::string & whiteId, 
        [[maybe_unused]] const std::string & blackId,
        const GameRecord & record) override 
    {
        if (currentIndex_ == 0 || currentIndex_ > tests_.size()) {
            return;
        }

        const auto& test = tests_[currentIndex_ - 1];
        const auto& history = record.history();

        if (!history.empty()) {
            const auto& lastMove = history.back();
            bool success = (lastMove.lan == test.expectedMove);
            std::ostringstream oss;
            oss << test.fen << " topic " << test.topic  
                << " | Expected: " << test.expectedMove
                << ", Got: " << lastMove.lan
                << ", Depth: " << lastMove.depth
                << ", Time: " << lastMove.timeMs << "ms\n";
            
            checklist_->logReport("epd-expected-moves", success, oss.str());
        }
    }

private:
    std::vector<EpdTest> tests_;
    size_t currentIndex_ = 0;
    Checklist* checklist_;
};
