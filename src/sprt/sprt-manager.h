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

#include "sprt-calculation.h"

#include "../game-manager/pair-tournament.h"
#include "../game-manager/game-task.h"

#include "../engine-handling/engine-config.h"
#include "../opening/openings.h"
#include "../cli/input-handler.h"
#include "../base-elements/ini-file.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace QaplaTester {

/**
 * @brief Results of engine matches for SPRT testing.
 * Contains trinomial and pentanomial statistics.
 */
struct SprtEnginesResult {
    int winsA = 0;       ///< Wins by engine A
    int draws = 0;       ///< Draw count
    int winsB = 0;       ///< Wins by engine B
    int pentaWW = 0;     ///< Both games won by engineA
    int pentaWD = 0;     ///< One win, one draw for engineA
    int pentaWL = 0;     ///< One win for engineA, one loss
    int pentaDD = 0;     ///< Both games drawn
    int pentaLD = 0;     ///< One loss, one draw for engineA
    int pentaLL = 0;     ///< Both games lost by engineA

    void clear() {
        winsA = draws = winsB = 0;
        pentaWW = pentaWD = pentaWL = pentaDD = pentaLD = pentaLL = 0;
    }
};

/**
 * @brief Configuration parameters for a SPRT test run.
 */
struct SprtConfig {
    float eloH1;
    float eloH0;
    double alpha;
    double beta;
    uint32_t maxGames;
    std::string model = "normalized";  ///> Model for SPRT: "bayesian", "logistic", "normalized"
    bool pentanomial = false;           ///> Use pentanomial statistics (not available with bayesian)
    Openings openings;
};

/**
 * @brief Result row from a single Monte Carlo simulation run.
 */
struct MonteCarloResultRow {
    float eloDifference;        // Simulated elo difference
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

using SprtResultsPerTournament = std::vector<SprtResult>;
using SprtResultsAllTournaments = std::vector<SprtResultsPerTournament>;

/**
  * Manages the analysis of EPD test sets using multiple chess engines in parallel.
  * Provides GameTasks for engine workers and collects their results.
  */
class SprtManager : public GameTaskProvider {
public:
    SprtManager() = default;
    ~SprtManager() override;

    /**
     * @brief Initializes and starts the SPRT testing procedure between engines.
     *
     * @param engines Vector of engine configurations. Must contain at least 2 engines.
     *                If exactly one engine does NOT have gauntlet flag set, it becomes
     *                the comparison engine (engine1), and the first gauntlet engine becomes
     *                the engine under test (engine0). Otherwise, uses indices [0] and [1].
     * @param config All configuration parameters required for the SPRT test.
     * 
     * @note Future: Will support multiple gauntlet engines for parallel SPRT testing.
     */
    void createTournament(const std::vector<EngineConfig>& engines, const SprtConfig& config);

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
     * @brief Sets a callback to be invoked when a save operation is required.
     * @details The manager controls when to save (Start, End, Interval).
     *          The callback itself should always perform the save.
     * @param callback Function to call to perform the save.
     * @param interval Number of games between periodic saves.
     */
    void setSaveCallback(std::function<void()> callback, uint32_t interval) {
        saveCallback_ = std::move(callback);
        saveInterval_ = interval;
    }

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
     * @brief Returns the SPRT tournament state as a section if it is not empty.
     * @return Optional section containing the tournament state, or std::nullopt if empty.
     */
    std::optional<QaplaHelpers::IniFile::Section> getSection() const;

    /**
     * @brief Sets tournament results from a configuration section.
     * @param sections The section containing tournament results to set.   
     */
    void setGameResults(const QaplaHelpers::IniFile::SectionList& sections);

    /**
     * @brief Returns the result of the tournament as a TournamentResult object.
     * 
     * @return TournamentResult containing the one duel result as vector.
     */
    TournamentResult getResult() const {
        TournamentResult t;
		t.add(pairing_->getResult());
        return t;
    }

    /**
     * @brief Returns the result of the engine duel.
     * 
     * @return EngineDuelResult containing wins, draws, and losses.
     */
    EngineDuelResult getDuelResult() const {
        return pairing_->getResult();
    }

    /**
     * @brief Checks if the tournament has any game results.
     * @return true if at least one game has been played, false otherwise.
     */
    bool hasResults() const {
        return pairing_->getResult().total() > 0;
    }

    /**
     * @brief Computes the result of the Sequential Probability Ratio Test (SPRT).
     * 
     * @param model Optional model override ("bayesian", "logistic", "normalized").
     * @param usePentanomial Optional flag to override pentanomial statistics usage.
     *
     * Uses the configured model (normalized, logistic, or bayesian) and either trinomial
     * or pentanomial statistics depending on configuration. Returns SprtResult containing
     * decision, LLR, bounds and all relevant values.
     */
    SprtResult computeSprt(std::optional<std::string> model = std::nullopt, 
        std::optional<bool> usePentanomial = std::nullopt) const;

    /**
     * @brief Checks if the SPRT test has finished.
     * @details The test is considered finished if a decision has been made (H0 or H1 accepted)
     *          or if the maximum number of games has been reached without a decision.
     * @return true if the test is finished, false otherwise.
     */
    bool isFinished() const {
        return computeSprt().isFinished();
    }

    /**
     * @brief Checks if a decision was made and stops tournament if all results have decisions.
     * @details Called after each game to check if tournament should be finished.
     */
    void finishTournament();

    /**
     * @brief Returns the cached SPRT results for all tournaments.
     * @details Returns a vector of SprtResult vectors, where each inner vector contains
     *          the 5 variants (3 trinomial + 2 pentanomial) for one tournament round.
     * @return Const reference to the cached SPRT results.
     */
    const SprtResultsAllTournaments& getSprtResults() const {
        return sprtResults_;
    }

    /**
     * @brief Logs the final SPRT result to the report logger.
     * @details Logs decision, LLR, and game statistics.
     */
    void logFinalResult() const;

private:
    std::unique_ptr<PairTournament> pairing_ = std::make_unique<PairTournament>();
    std::shared_ptr<StartPositions> startPositions_;
    EngineConfig engine0_;
    EngineConfig engine1_;
    PairTournamentConfig tournamentConfig_;

    /**
     * @brief Computes the result of the Sequential Probability Ratio Test (SPRT).
     *
     * Internal version with SprtEnginesResult for Monte Carlo simulations.
     * Uses the configured model and pentanomial settings from config.
     */
    SprtResult computeSprt(
        const SprtEnginesResult& result, const std::string& engineA, const std::string& engineB) const;

    /**
     * @brief Simulates a single pair of games (white/black swap) for Monte Carlo testing.
     * @param elo ELO difference for the simulation
     * @param drawRate Base draw rate
     * @param result Reference to SprtEnginesResult to update with game results
     */
    static void simulateGamePair(float elo, float drawRate, SprtEnginesResult& result);

    /**
     * @brief Runs a single Monte Carlo simulation test for a given ELO difference.
     * @param simulationsPerElo Number of simulations to run for this ELO difference.
     * @param elo ELO difference for the simulation.
     * @param drawRate Base draw rate.
     * @param noDecisions Reference to count of no-decision outcomes.
     * @param numH0 Reference to count of H0 accepted outcomes.
     * @param numH1 Reference to count of H1 accepted outcomes.
     */
    void runMonteCarloSingleTest(int simulationsPerElo, float elo, float drawRate, 
        int64_t &noDecisions, int64_t &numH0, int64_t &numH1, int64_t &totalGames);

    /**
     * @brief Internal method that performs the actual Monte Carlo test computation.
     * @param config The SPRT configuration to test.
     * @return MonteCarloResult containing all simulation results.
     */
    void runMonteCarloTestInternal(const SprtConfig& config);

    SprtConfig config_;

    // Registration
    std::unique_ptr<InputHandler::CallbackRegistration> sprtCallback_;
    std::function<void()> saveCallback_;
    uint32_t saveInterval_ = 0;
    uint32_t gamesSinceSave_ = 0;

    // Monte Carlo test thread management
    std::thread monteCarloThread_;
    std::mutex monteCarloResultMutex_;
    std::atomic<bool> monteCarloTestRunning_{false};
    std::atomic<bool> monteCarloShouldStop_{false};
    MonteCarloResult monteCarloResult_;

    // SPRT results cache (one entry per tournament/round)
    SprtResultsAllTournaments sprtResults_;
    mutable std::mutex sprtResultsMutex_;

    /**
     * @brief Performs an auto-save of the SPRT tournament state if conditions are met. 
     * 
     * @param result The latest SPRT result to check for decision or finished state. 
     * @param force If true, forces a save regardless of interval or decision state. 
     */
    void autoSave(const SprtResult& result, bool force = false);
};
} // namespace QaplaTester
