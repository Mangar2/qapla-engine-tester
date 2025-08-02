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

#include "openings.h"
#include "pair-tournament.h"
#include "engine-config.h"
#include "time-control.h"
#include "input-handler.h"
#include <vector>
#include <memory>
#include <ostream>
#include <istream>
#include <string>

 /**
  * @brief Configuration parameters for a tournament.
  */
struct TournamentConfig {
    std::string event;
    std::string type;
    std::string tournamentFilename;
    uint32_t saveInterval = 0;
    uint32_t games = 2;
    uint32_t rounds = 1;
    uint32_t repeat = 2;
    uint32_t ratingInterval = 0;
    uint32_t outcomeInterval = 0;
    int averageElo = 2600;
    bool noSwap = false;
    Openings openings;
};

 /**
  * @brief Manages and executes a complete tournament composed of multiple PairTournaments.
  *
  * Supports dynamic creation based on tournament type and dispatches execution/saving logic.
  */
class Tournament {
public:
    Tournament() = default;

    /**
     * @brief Creates and initializes a tournament from the given configuration.
     *
     * @param engines List of all participating engines (marked via EngineConfig::isGauntlet).
     * @param config Global tournament settings including type, rounds, openings, etc.
     */
    void createTournament(const std::vector<EngineConfig>& engines,
        const TournamentConfig& config);

    /**
     * @brief Schedules all active pairings for execution.
     * 
	 * @param concurrency Number of parallel workers to use.
     */
    void scheduleAll(uint32_t concurrency);

    /**
     * @brief Waits for all engines to finish.
     * @return true if all tasks completed successfully, false if the analysis was stopped prematurely.
     */
    bool wait();

    /**
     * @brief Saves the state of all pairings to a stream.
     */
    void save(std::ostream& out) const;
	void save(const std::string& filename) const {
		std::ofstream out(filename);
		if (!out) {
			throw std::runtime_error("Failed to open file for saving tournament results: " + filename);
		}
		save(out);
	}

    /**
     * @brief Loads the state of all pairings from a stream.
     */
    void load(std::istream& in);
	void load(const std::string& filename) {
		std::ifstream in(filename);
		if (!in) {
            // If the file doesn't exist, we simply return without loading anything.
			return; 
		}
		load(in);
	}

    /**
     * @brief Returns a compact status summary of all pairings.
     */
    std::string statusSummary() const { return {}; }

	TournamentResult getResult() const {
		TournamentResult result;
		for (const auto& pairing : pairings_) {
			result.add(pairing->getResult());
		}
		return result;
	}
    
    std::string getResultString() const {
        std::ostringstream oss;
        auto result = getResult();
        result.printRatingTableUciStyle(oss, config_.averageElo);
        result.printOutcome(oss);
        return oss.str();
    }

private:
    /**
    * @brief Called after a game finishes in any PairTournament.
    *
    * Used to trigger rating output or progress tracking.
    *
    * @param sender Pointer to the PairTournament that just completed a game.
    */
    void onGameFinished(PairTournament* sender);

    /**
     * @brief Parses a single round block from the input and updates results if engines are valid.
     * @param in Stream positioned after the current round header.
     * @param roundHeader The header line (e.g., "[round 1: EngineA vs EngineB]").
     * @param validEngines Set of engine names that are part of this tournament.
     * @return The next round header line or empty if end of input is reached.
     */
	std::string loadRound(std::istream& in, const std::string& roundHeader,
		const std::unordered_set<std::string>& validEngines);

    void createGauntletPairings(const std::vector<EngineConfig>& engines,
        const TournamentConfig& config);

    void createRoundRobinPairings(const std::vector<EngineConfig>& engines,
        const TournamentConfig& config);

	void createPairings(const std::vector<EngineConfig>& players, const std::vector<EngineConfig>& opponents,
		const TournamentConfig& config, bool symmetric);

    std::vector<EngineConfig> engineConfig_;
	TournamentConfig config_;
	std::shared_ptr<StartPositions> startPositions_;
    std::vector<std::shared_ptr<PairTournament>> pairings_;
    uint32_t raitingTrigger_ = 0;
    uint32_t outcomeTrigger_ = 0;
    uint32_t saveTrigger_ = 0;
    
    // Registration
    std::unique_ptr<InputHandler::CallbackRegistration> tournamentCallback_;
};


