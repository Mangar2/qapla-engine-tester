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
#include "ini-file.h"
#include "game-manager-pool.h"

#include <vector>
#include <memory>
#include <ostream>
#include <istream>
#include <string>

namespace QaplaTester {

 /**
  * @brief Configuration parameters for a tournament.
  */
struct TournamentConfig {
    std::string event;
    std::string type = "gauntlet";
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
	 * @param registerToInputhandler If true, registers the tournament to the InputHandler 
     *  for interactive mode on cli usage.
     * @param pool GameManagerPool instance to use (default: singleton).
     */
    void scheduleAll(uint32_t concurrency, bool registerToInputhandler = true, 
        GameManagerPool& pool = GameManagerPool::getInstance());

    /**
     * @brief Returns the state of all pairings as a list of sections.
     * @param prefix Optional prefix for section names (e.g., "tournament").
     * @return Vector of IniFile sections containing the tournament state.
     */
    std::vector<QaplaHelpers::IniFile::Section> getSections() const;

    /**
     * @brief Loads the state of all pairings from a stream.
     */
    void load(const QaplaHelpers::IniFile::Section& section);
    void load(const QaplaHelpers::IniFile::SectionList& sections);
	void load(const std::string& filename) {
		std::ifstream in(filename);
		if (!in) {
            // If the file doesn't exist, we simply return without loading anything.
			return; 
		}
        auto sections = QaplaHelpers::IniFile::load(in);
		load(sections);
	}

	TournamentResult getResult() const {
		TournamentResult result;
		for (const auto& pairing : pairings_) {
			result.push_back(pairing->getResult());
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

    std::pair<uint64_t, std::optional<TournamentResult>> pollResult(uint64_t updateCnt) {
        if (updateCnt != updateCnt_) {
			std::scoped_lock lock(stateMutex_);
            return { updateCnt_, result_ };
		}
        return { updateCnt_, std::nullopt };
	}

    /**
	 * @brief Return a pointer to the PairTournament at the given index.
	 * @result std::optional containing the PairTournament pointer if index is valid, otherwise std::nullopt.
	 */
    std::optional<const PairTournament*> getPairTournament(size_t index) const {
        if (index < pairings_.size()) {
            return pairings_[index].get();
        }
        return std::nullopt;
    }

    /**
     * @brief Returns the number of pair tournaments in this tournament.
     */
    size_t pairTournamentCount() const {
        return pairings_.size();
    }

    /**
     * @brief Returns the current update count of the tournament.
     * 
     * This count is incremented each time a game result is recorded.
     * @return Current update count.
     */
    uint64_t getUpdateCount() const {
        return updateCnt_;
	}

    /**
     * @brief Checks if the tournament has any tasks scheduled (i.e., has started).
     * @return True if tasks are scheduled, false otherwise.
     */
    bool hasTasksScheduled() const {
        return !pairings_.empty();
    }

    /**
     * @brief Calculates the total number of games in a tournament based on configuration and engines.
     *
     * This calculation matches the actual scheduling logic to ensure consistency.
     *
     * @param engines List of participating engines.
     * @param config Tournament configuration.
     * @return Total number of games that will be scheduled.
     */
    static uint32_t calculateTotalGames(const std::vector<EngineConfig>& engines, const TournamentConfig& config);

private:
    TournamentResult result_;
    uint64_t updateCnt_ = 1;

    /**
    * @brief Called after a game finishes in any PairTournament.
    *
    * Used to trigger rating output or progress tracking.
    *
    * @param sender Pointer to the PairTournament that just completed a game.
    */
    void onGameFinished(PairTournament* sender);

    void createGauntletPairings(const std::vector<EngineConfig>& engines,
        const TournamentConfig& config);

    void createRoundRobinPairings(const std::vector<EngineConfig>& engines,
        const TournamentConfig& config);

	void createPairings(const std::vector<EngineConfig>& players, const std::vector<EngineConfig>& opponents,
		const TournamentConfig& config, bool symmetric);

    /**
     * @brief Restores the results of pairings from a saved state.
     * @param savedPairings The saved pairings to restore.
     */
    void restoreResults(const std::vector<std::shared_ptr<PairTournament>>& savedPairings);

    /**
     * @brief Saves the tournament state to a file.
     * @param filename Path to the output file.
     */
    void save(const std::string& filename) const;

    std::vector<EngineConfig> engineConfig_;
	TournamentConfig config_;
	std::shared_ptr<StartPositions> startPositions_;
    std::vector<std::shared_ptr<PairTournament>> pairings_;
    uint32_t raitingTrigger_ = 0;
    uint32_t outcomeTrigger_ = 0;
    uint32_t saveTrigger_ = 0;

	mutable std::mutex stateMutex_; ///< Mutex for thread-safe access to tournament state
    
    // Registration
    std::unique_ptr<InputHandler::CallbackRegistration> tournamentCallback_;
};

} // namespace QaplaTester
