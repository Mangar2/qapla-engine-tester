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

#include <tuple>
#include "engine-config.h"
#include "game-task.h."
#include "epd-reader.h"

 /**
  * @brief Configuration for loading and selecting opening positions.
  */
struct Openings {
    std::string file;
    std::string format;
    std::string order;
    int plies;
    int start;
    std::string policy;
};

/**
 * @brief Configuration parameters for a SPRT test run.
 */
struct SprtConfig {
    int eloUpper;
    int eloLower;
    float alpha;
    float beta;
    int maxGames;
    TimeControl tc;
    Openings openings;
};

 
/**
  * Manages the analysis of EPD test sets using multiple chess engines in parallel.
  * Provides GameTasks for engine workers and collects their results.
  */
class SprtManager : public GameTaskProvider {
public:
    SprtManager() = default;

    /**
     * @brief Initializes and starts the SPRT testing procedure between two engines.
     *
	 * @param engine0 Configuration for the first engine.
     * @param engine1 Configuration for the second engine.
     * @param concurrency Number of engine instances to run in parallel.
     * @param config All configuration parameters required for the SPRT test.
	 * @return An optional boolean indicating the result of the SPRT test. 
     *         true, H1 accepted; false, H0 accepted; std::nullopt, inconclusive.
     */
    void runSprt(const EngineConfig& engine0, const EngineConfig& engine1,
        int concurrency, const SprtConfig& config);

    /**
     * @brief Waits for all engines to finish.
     * @return true if all tasks completed successfully, false if the analysis was stopped prematurely.
     */
    bool wait();

    /**
     * @brief Provides the next Game to play.
     * @param whiteId The identifier for the white player.
     * @param blackId The identifier for the black player.
     * @return An optional GameTask. If no more tasks are available, returns std::nullopt.
     */
    std::optional<GameTask> nextTask(const std::string& whiteId, const std::string& blackId) override;

    /**
     * @brief Processes the result of a completed task.
     * @param whiteId The identifier for the white player.
     * @param blackId The identifier for the black player.
     * @param record The result containing the complete game result.
     */
    void setGameRecord(const std::string& whiteId, const std::string& blackId,
        const GameRecord& record) override;

    void runMonteCarloTest(const SprtConfig& config);

	/**
	 * @brief Returns the current decision of the SPRT test.
	 * @return std::optional<bool> containing true if H1 accepted, false if H0 accepted, or std::nullopt if inconclusive.
	 */
	std::optional<bool> getDecision() const {
		return decision_;
	}

private:
    /**
     * @brief Evaluates the current SPRT test state and logs result if decision boundary is reached.
     * @return true if the test should be stopped (H0 or H1 accepted), false otherwise.
     */
    
     /**
      * @brief Computes the result of the Sequential Probability Ratio Test (SPRT) using BayesElo model.
      *
      * Applies Jeffreys' prior, estimates drawElo, and compares likelihoods under H0 and H1.
      * Returns std::optional<bool>: true if H1 accepted, false if H0 accepted, nullopt if inconclusive.
      */
    std::pair<std::optional<bool>, std::string> computeSprt() const;
	bool rememberStop_ = false;

    uint32_t gamesStarted_ = 0;
    size_t nextIndex_ = 0;
	std::vector<std::string> startPositions_;
    SprtConfig config_;
    int concurrency_;
    uint32_t winsP1_ = 0;
    uint32_t winsP2_ = 0;
    uint32_t draws_ = 0;
	std::optional<bool> decision_ = std::nullopt;

    std::string engineP1Name_;
    std::string engineP2Name_;

};
