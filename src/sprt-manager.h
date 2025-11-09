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


#include "engine-config.h"
#include "game-task.h"
#include "openings.h"
#include "pair-tournament.h"
#include "input-handler.h"
#include "ini-file.h"

#include <tuple>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace QaplaTester {

/**
 * @brief Configuration parameters for a SPRT test run.
 */
struct SprtConfig {
    int eloUpper;
    int eloLower;
    double alpha;
    double beta;
    uint32_t maxGames;
    Openings openings;
};

/**
 * @brief Result of a SPRT computation containing all values for display.
 */
struct SprtResult {
    std::optional<bool> decision;  // true if H1 accepted, false if H0 accepted, nullopt if inconclusive
    std::string info;              // Human-readable decision info
    double llr;                    // Log-Likelihood Ratio
    double lowerBound;             // Lower decision boundary
    double upperBound;             // Upper decision boundary
    double drawElo;                // Computed drawElo value
    int winsA;                     // Wins for engine A
    int draws;                     // Number of draws
    int winsB;                      // Wins for engine B
    std::string engineA;           // Name of engine A
    std::string engineB;           // Name of engine B
    int eloLower;                  // Lower elo bound from config
    int eloUpper;                  // Upper elo bound from config
};

/**
 * @brief Result row from a single Monte Carlo simulation run.
 */
struct MonteCarloResultRow {
    int eloDifference;          // Simulated elo difference
    double noDecisionPercent;   // Percentage of runs with no decision
    double h0AcceptedPercent;   // Percentage of runs where H0 was accepted
    double h1AcceptedPercent;   // Percentage of runs where H1 was accepted
    double avgGames;            // Average number of games per simulation
};

/**
 * @brief Complete result of a Monte Carlo test run.
 */
struct MonteCarloResult {
    std::vector<MonteCarloResultRow> rows;
    SprtConfig config;          // Configuration used for the test
};


/**
  * Manages the analysis of EPD test sets using multiple chess engines in parallel.
  * Provides GameTasks for engine workers and collects their results.
  */
class SprtManager : public GameTaskProvider {
public:
    SprtManager() = default;
    ~SprtManager() override;

    /**
     * @brief Initializes and starts the SPRT testing procedure between two engines.
     *
	 * @param engine0 Configuration for the first engine.
     * @param engine1 Configuration for the second engine.
     * @param config All configuration parameters required for the SPRT test.
     */
    void createTournament(const EngineConfig& engine0, const EngineConfig& engine1,
        const SprtConfig& config);

    /**
     * @brief Schedules the tournament and registers all pairings as task providers.
     *
     * @param self Shared pointer to this Tournament instance.
     * @param concurrency Number of parallel workers to use.
     * @param pool Reference to the GameManagerPool to use for scheduling.
     */
    void schedule(const std::shared_ptr<SprtManager>& self, uint32_t concurrency, GameManagerPool& pool);

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

    /**
     * @brief Runs a Monte Carlo simulation to estimate the SPRT decision boundaries in a background thread.
     * @param config The configuration parameters for the SPRT test.
     * @return true if test was started, false if a test is already running.
     */
    bool runMonteCarloTest(const SprtConfig &config);

    /**
     * @brief Checks if a Monte Carlo test is currently running.
     * @return true if running, false otherwise.
     */
    bool isMonteCarloTestRunning() const {
        return monteCarloTestRunning_.load();
    }

    /**
     * @brief Stops any running Monte Carlo test.
     */
    void stopMonteCarloTest();

    /**
     * @brief Clears the Monte Carlo test results.
     */
    void clearMonteCarloResult();

    /**
     * @brief Executes a callback with thread-safe access to Monte Carlo results.
     * @param callback Function to call with const reference to results.
     */
    void withMonteCarloResult(const std::function<void(const MonteCarloResult&)>& callback);

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
     * @brief Returns the SPRT tournament state as a section if it is not empty.
     * @return Optional section containing the tournament state, or std::nullopt if empty.
     */
    std::optional<QaplaHelpers::IniFile::Section> getSection() const;

    /**
     * @brief Loads tournament results from a configuration section.
     * @param section The section containing tournament results to load.
     */
    void loadFromSection(const QaplaHelpers::IniFile::Section& section);

    /**
     * @brief Loads the state from a stream - do nothing, if the file cannot be loaded.
	 * @param filename The file to load the state from.
     */
    void load(const QaplaHelpers::IniFile::Section& section);

    /**
     * @brief Returns the result of the tournament as a TournamentResult object.
     * 
     * @return TournamentResult containing the one duel result as vector.
     */
    TournamentResult getResult() const {
        TournamentResult t;
		t.add(tournament_->getResult());
        return t;
    }

    /**
     * @brief Returns the result of the engine duel.
     * 
     * @return EngineDuelResult containing wins, draws, and losses.
     */
    EngineDuelResult getDuelResult() const {
        return tournament_->getResult();
    }

    /**
     * @brief Computes the result of the Sequential Probability Ratio Test (SPRT) using BayesElo model.
     *
     * Applies Jeffreys' prior, estimates drawElo, and compares likelihoods under H0 and H1.
     * Returns SprtResult containing decision, llr, bounds and all relevant values.
     */
    SprtResult computeSprt() const;



private:
    std::unique_ptr<PairTournament> tournament_ = std::make_unique<PairTournament>();
    std::shared_ptr<StartPositions> startPositions_;
    EngineConfig engine0_;
    EngineConfig engine1_;
    PairTournamentConfig tournamentConfig_;

    /**
     * @brief Computes a human-readable SPRT info string.
     * @param result The SPRT result to print.
     * @return A formatted string containing the SPRT decision or bounds.
     */
    static std::string computeSprtInfo(const SprtResult& result);

    /**
     * @brief Evaluates the current SPRT test state and logs result if decision boundary is reached.
     * @return true if the test should be stopped (H0 or H1 accepted), false otherwise.
     */
    
     /**
      * @brief Computes the result of the Sequential Probability Ratio Test (SPRT) using BayesElo model.
      *
      * Internal version with explicit parameters.
      */
    SprtResult computeSprt(
        int winsA, int draws, int winsB, const std::string& engineA, const std::string& engineB) const;

    void runMonteCarloSingleTest(int simulationsPerElo, int elo, double drawRate, 
        int64_t &noDecisions, int64_t &numH0, int64_t &numH1, int64_t &totalGames);

    /**
     * @brief Internal method that performs the actual Monte Carlo test computation.
     * @param config The SPRT configuration to test.
     * @return MonteCarloResult containing all simulation results.
     */
    void runMonteCarloTestInternal(const SprtConfig& config);

    bool rememberStop_ = false;

    SprtConfig config_;
	std::optional<bool> decision_ = std::nullopt;

    // Registration
    std::unique_ptr<InputHandler::CallbackRegistration> sprtCallback_;

    // Monte Carlo test thread management
    std::thread monteCarloThread_;
    std::mutex monteCarloResultMutex_;
    std::atomic<bool> monteCarloTestRunning_{false};
    std::atomic<bool> monteCarloShouldStop_{false};
    MonteCarloResult monteCarloResult_;

};
} // namespace QaplaTester
