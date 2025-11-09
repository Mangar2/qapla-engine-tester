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

#include <optional>
#include <iostream>
#include <string>
#include <vector>
#include "game-record.h"
#include "game-result.h"
#include "logger.h"

namespace QaplaTester {

/**
 * @brief Singleton class responsible for evaluating draw and resign conditions.
 */
class AdjudicationManager {
public:
    /**
     * @brief Configuration for draw adjudication logic.
     */
    struct DrawAdjudicationConfig {
        uint32_t minFullMoves = 80;
        uint32_t requiredConsecutiveMoves = 20;
        int centipawnThreshold = 20;
        bool testOnly = false;
        bool active = false;
    };

    /**
     * @brief Configuration for resign adjudication logic.
     */
    struct ResignAdjudicationConfig {
        uint32_t requiredConsecutiveMoves = 5;
        int centipawnThreshold = 500;
        bool twoSided = false;
        bool testOnly = false;
        bool active = false;
    };

    struct AdjudicationTestStats {
        uint32_t totalGames = 0;
        uint32_t correctDecisions = 0;
        uint32_t incorrectDecisions = 0;
        uint64_t savedTimeMs = 0;
        uint64_t totalTimeMs = 0;
        std::vector<GameRecord> failed;
    };

    /**
     * @brief Key-value pair for test result output.
     */
    struct TestResultEntry {
        std::string key;
        std::string value;
    };

    /**
     * @brief Complete test results for both draw and resign adjudication.
     */
    struct TestResults {
        bool hasDrawTest = false;
        bool hasResignTest = false;
        std::vector<TestResultEntry> drawResult;
        std::vector<TestResultEntry> resignResult;
    };

    /**
     * @brief Returns the singleton instance.
     */
    static AdjudicationManager& instance() {
        static AdjudicationManager singletonInstance;
        return singletonInstance;
    }

    /**
     * @brief Returns a singleton instance for usage in games managed by the GameManagerPool.
     */
    static AdjudicationManager& poolInstance() {
        static AdjudicationManager poolSingletonInstance;
        return poolSingletonInstance;
    }

    /**
     * @brief Set the configuration for draw adjudication.
     * @param config The draw configuration to apply.
     */
    void setDrawAdjudicationConfig(const DrawAdjudicationConfig& config) {
        drawConfig_ = config;
    }

    /**
     * @brief Set the configuration for resign adjudication.
     * @param config The resign configuration to apply.
     */
    void setResignAdjudicationConfig(const ResignAdjudicationConfig& config) {
        resignConfig_ = config;
    }

    /**
     * @brief Evaluates whether the game should be adjudicated as a draw.
     * @param game The complete game record to evaluate.
     * @return Pair containing the result and cause 
     */
    [[nodiscard]] std::pair<GameEndCause, GameResult> adjudicateDraw(const GameRecord& game) const;
    void testAdjudicate(const GameRecord& game) const {
        if (drawConfig_.active && !drawConfig_.testOnly) {
            auto [cause, result] = adjudicateDraw(game);
            auto index = findDrawAdjudicationIndex(game);
            if ((result == GameResult::Unterminated) == index.has_value()) {
                QaplaTester::Logger::testLogger().log(
                    std::format("Draw adjudication test failed: Expected result: {}, but index was: {}",
                                gameResultToPgnResult(result),
                                index ? std::to_string(*index) : "none"));
            }
        }
        if (resignConfig_.active && !resignConfig_.testOnly) {
            auto [cause, result] = adjudicateResign(game);
            auto [resResult, resIndex] = findResignAdjudicationIndex(game);
            if (result != resResult) {
                QaplaTester::Logger::testLogger().log(
                    std::format("Resign adjudication test failed: Expected result: {}, but result was: {}",
                                gameResultToPgnResult(result),
                                gameResultToPgnResult(resResult)));
            }
        }
    }

    /**
     * @brief Evaluates whether the game should be adjudicated as a resignation.
     * @param game The complete game record to evaluate.
     * @return Pair containing the result and cause 
     */
   [[nodiscard]] std::pair<GameEndCause, GameResult> adjudicateResign(const GameRecord& game) const;

    /**
     * @brief Informs the adjudicator at game end for test-mode analysis.
     * @param game The complete and finalized game record.
     */
    void onGameFinished(const GameRecord& game);

    /**
     * @brief Computes the test results for both draw and resign adjudication.
     * @return TestResults structure containing formatted test results.
     */
    [[nodiscard]] TestResults computeTestResults() const;

    /**
     * @brief Prints adjudication test statistics to the given output stream.
     * @param out Output stream to write to.
     */
    void printTestResult(std::ostream& out) const;


private:
    [[nodiscard]] std::optional<size_t> findDrawAdjudicationIndex(const GameRecord& game) const;
    [[nodiscard]] std::pair<GameResult, size_t> findResignAdjudicationIndex(const GameRecord& game) const;

    AdjudicationManager() = default;

    DrawAdjudicationConfig drawConfig_;
    ResignAdjudicationConfig resignConfig_;

    AdjudicationTestStats drawStats_;
    AdjudicationTestStats resignStats_;

    std::mutex statsMutex_;
};

} // namespace QaplaTester
