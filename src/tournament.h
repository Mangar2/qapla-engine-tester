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

#include "openings.h"
#include "pair-tournament.h"
#include "engine-config.h"
#include "time-control.h"
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
    int games = 2;
    int rounds = 1;
    int repeat = 2;
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
    void scheduleAll(int concurrency);

    /**
     * @brief Waits for all engines to finish.
     * @return true if all tasks completed successfully, false if the analysis was stopped prematurely.
     */
    bool wait();

    /**
     * @brief Saves the state of all pairings to a stream.
     */
    void saveAll(std::ostream& out) const;
	void saveAll(const std::string& filename) const {
		std::ofstream out(filename);
		if (!out) {
			throw std::runtime_error("Failed to open file for saving tournament results: " + filename);
		}
		saveAll(out);
	}

    /**
     * @brief Loads the state of all pairings from a stream.
     */
    void loadAll(std::istream& in) {}
	void loadAll(const std::string& filename) {
		std::ifstream in(filename);
		if (!in) {
			throw std::runtime_error("Failed to open file for loading tournament results: " + filename);
		}
		loadAll(in);
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

private:
    void createGauntletPairings(const std::vector<EngineConfig>& engines,
        const TournamentConfig& config);

    void createRoundRobinPairings(const std::vector<EngineConfig>& engines,
        const TournamentConfig& config);

    std::vector<EngineConfig> engineConfig_;
	TournamentConfig config_;
    std::shared_ptr<std::vector<std::string>> startPositions_;
    std::vector<std::shared_ptr<PairTournament>> pairings_;
};


