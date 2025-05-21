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
    bool whiteToPlay;
};

/**
 * @class EpdManager
 * @brief Provides predefined EPD test positions for engine validation.
 *
 * Supplies tasks with specific FEN positions and expected best moves. Verifies
 * the engine's output against the expected move and logs the result, including
 * computation depth and time.
 */
class EpdManager : public GameTaskProvider {
public:
    EpdManager() {
        tests_ = {
            { "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", "d2d4", true },
            { "rnbqkbnr/ppp2ppp/8/3pp3/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 3", "e5d4", false },
            { "r1bq1rk1/pp1n1ppp/2pbpn2/8/2PNP3/2N1B3/PP3PPP/R2QKB1R w KQ - 0 9", "f1e2", true },
            // Weitere Tests können hier hinzugefügt werden
        };
    }

    /**
     * @brief Adds an EPD test case to the internal list.
     * @param fen The FEN string representing the position.
     * @param expectedMove The expected best move in LAN notation.
     * @param whiteToPlay True if it is white to move, false for black.
     */
    void addTest(const std::string& fen, const std::string& expectedMove, bool whiteToPlay) {
        tests_.push_back({ fen, expectedMove, whiteToPlay });
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
        task.taskType = GameTask::Type::ComputeMove;
        task.useStartPosition = false;
        task.fen = test.fen;
        task.whiteTimeControl = { 10000, 100, 10000 };
        task.blackTimeControl = { 10000, 100, 10000 };

        ++currentIndex_;
        return task;
    }

    /**
     * @brief Evaluates the result of a completed task.
     * @param record The game record containing the engine's computed moves.
     */
    void setGameRecord(const GameRecord& record) override {
        if (currentIndex_ == 0 || currentIndex_ > tests_.size()) {
            return;
        }

        const auto& test = tests_[currentIndex_ - 1];
        const auto& history = record.history();

        if (!history.empty()) {
            const auto& lastMove = history.back();
            bool success = (lastMove.lan == test.expectedMove);
            std::string result = success ? "SUCCESS" : "FAILURE";
            std::ostringstream oss;
            oss << "EPD Test " << (currentIndex_) << ": " << result
                << " | Expected: " << test.expectedMove
                << ", Got: " << lastMove.lan
                << ", Depth: " << lastMove.depth
                << ", Time: " << lastMove.timeMs << "ms\n";
            Logger::testLogger().log(oss.str(), TraceLevel::commands);
        }
    }

private:
    std::vector<EpdTest> tests_;
    size_t currentIndex_ = 0;
};
