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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#pragma once

#include <tuple>
#include "engine-config.h"
#include "game-task.h"
#include "openings.h"
#include "pair-tournament.h"

/**
 * @brief Configuration parameters for a SPRT test run.
 */
struct SprtConfig {
    int eloUpper;
    int eloLower;
    float alpha;
    float beta;
    int maxGames;
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
     */
    void createTournament(const EngineConfig& engine0, const EngineConfig& engine1,
        const SprtConfig& config);

    /**
     * @brief Schedules the tournament
     *
     * @param concurrency Number of parallel workers to use.
     */
    void schedule(int concurrency);

    /**
     * @brief Waits for all engines to finish.
     * @return true if all tasks completed successfully, false if the analysis was stopped prematurely.
     */
    bool wait();

    /**
     * @brief Provides the next available task.
     *
     * @return A GameTask with a unique taskId or std::nullopt if no task is available.
     */
    std::optional<GameTask> nextTask() override;

    /**
     * @brief Records the result of a finished game identified by taskId.
     *
     * @param taskId Identifier of the task this game result belongs to.
     * @param record Game outcome and metadata.
     */
    void setGameRecord(const std::string& taskId, const GameRecord& record) override;

    void runMonteCarloTest(const SprtConfig& config);

	/**
	 * @brief Returns the current decision of the SPRT test.
	 * @return std::optional<bool> containing true if H1 accepted, false if H0 accepted, or std::nullopt if inconclusive.
	 */
	std::optional<bool> getDecision() const {
		return decision_;
	}

    /**
	 * @brief Saves the current SPRT test state to a stream.
	 * @param filename The file to save the state to.
     */
    void save(const std::string& filename) const;

    /**
     * @brief Loads the state from a stream - do nothing, if the file cannot be loaded.
	 * @param filename The file to load the state from.
     */
    void load(const std::string& filename);

    TournamentResult getResult() const {
        TournamentResult t;
		t.add(tournament_.getResult());
        return t;
    }

private:
    PairTournament tournament_;
    std::shared_ptr<StartPositions> startPositions_;

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
    std::pair<std::optional<bool>, std::string> computeSprt() const {
		auto duel = tournament_.getResult();
		return computeSprt(duel.winsEngineA, duel.draws, duel.winsEngineB, duel.getEngineA(), duel.getEngineB());
    }
    std::pair<std::optional<bool>, std::string> computeSprt(
        int winsA, int draws, int winsB, std::string engineA, std::string engineB) const;
	bool rememberStop_ = false;

    SprtConfig config_;
	std::optional<bool> decision_ = std::nullopt;

};
