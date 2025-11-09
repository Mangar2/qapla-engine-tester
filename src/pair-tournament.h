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

#include "game-task.h"
#include "game-record.h"
#include "engine-config.h"
#include "time-control.h"
#include "openings.h"
#include "tournament-result.h"
#include "ini-file.h"

#include <vector>
#include <memory>
#include <optional>
#include <string>
#include <istream>
#include <ostream>
#include <mutex>
#include <array>
#include <sstream>
#include <random>
#include <functional>
#include <atomic>

namespace QaplaTester {

/**
 * @brief Represents a collection of chess openings for a tournament.
 */
struct StartPositions {
    std::vector<std::string> fens;
    std::vector<GameRecord> games;
	[[nodiscard]] uint32_t size() const {
		return std::max(static_cast<uint32_t>(fens.size()), static_cast<uint32_t>(games.size()));
	}
	[[nodiscard]] bool empty() const {
		return fens.empty() && games.empty();
	}
};

/**
* @brief Configuration parameters for a PairTournament.
*/
struct PairTournamentConfig {
    uint32_t games = 0;
    uint32_t repeat = 2;
    uint32_t round = 0;
    uint32_t seed = 0;
    uint32_t gameNumberOffset = 0;
    bool swapColors = true;
    Openings openings;
};

class GameManagerPool;

 /**
  * @brief Represents an autonomous two-engine tournament with defined number of games.
  *
  * Implements GameTaskProvider. Manages game generation, opening selection,
  * color assignment, result storage and resumability.
  */
class PairTournament : public GameTaskProvider {
public:
    PairTournament(): rng_(std::random_device{}()) {}

    /**
     * @brief Defines the callback type for notifying game completion.
     *
     * The callback receives a pointer to this PairTournament instance.
     */
    using GameFinishedCallback = std::function<void(PairTournament*)>;

    /**
     * @brief Sets the callback to be invoked after each completed game.
     *
     * @param callback Function to call when a game finishes.
     */
    void setGameFinishedCallback(GameFinishedCallback callback) {
        onGameFinished_ = std::move(callback);
    }

    /**
     * @brief Initializes the tournament configuration and internal state.
     *
     * Must be called exactly once before scheduling or loading results.
     *
     * @param engineA Engine to play as white (initially).
     * @param engineB Engine to play as black (initially).
     * @param config Configuration containing all tournament parameters and opening settings.
	 * @param startPositions Optional shared vector of starting positions (FEN strings).
     */
    void initialize(const EngineConfig& engineA,
        const EngineConfig& engineB,
        const PairTournamentConfig& config,
        std::shared_ptr<StartPositions> startPositions);

    /**
     * @brief Registers this tournament with the GameManagerPool for execution.
     *
     * Must be called after initialize(). Adds this pairing to the pool with the configured concurrency limit.
     */
    void schedule(const std::shared_ptr<PairTournament>& self, GameManagerPool& pool);

    void clear() {
        std::scoped_lock lock(mutex_);
        initialized_ = false;
        results_.clear();
        duelResult_ = EngineDuelResult(engineA_.getName(), engineB_.getName());
        nextIndex_ = 0;
        curRecord_ = GameRecord();
    }

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
     * @brief Serializes the tournament state as a single-line result string.
     *
     * Format: "<engineA> vs <engineB> : <result-sequence>"
     *
     * @return A compact string representing the tournament result state.
     */
    std::string toString() const;

    /**
     * @brief Parses a tournament result line and updates internal state.
     *
     * Must be called only after initialize(). Does not validate engine names.
     *
     * @param line A single line in the format: "games: <result-sequence>" or "<result-sequence>"
     */
    void fromString(const std::string& line);

    /**
     * @brief Copies results and duel statistics from another PairTournament instance.
     *
     * Both tournaments must have been initialized with the same engines and configuration.
     *
     * @param other Another PairTournament instance to copy results from.
     */
    void copyResultsFrom(const PairTournament& other) {
       duelResult_ = other.duelResult_;
       results_ = other.results_;
    }

    /**
	 * @brief Returns the result of the duel between the two engines.
     */
    EngineDuelResult getResult() const {
		return duelResult_;
    }

	const EngineConfig& getEngineA() const {
		return engineA_;
	}
	const EngineConfig& getEngineB() const {
		return engineB_;
	}

	void setVerbose(bool verbose) {
		verbose_ = verbose;
	}
    
	/**
	 * @brief Returns the tournament state as a section if it is not empty.
	 *
	 * Returns the round information, results, and engine names as an IniFile section.
	 *
	 * @param id The ID to use in the section (e.g., "tournament" or "sprt-tournament").
	 * @return Optional section containing the tournament state, or std::nullopt if empty.
	 */
    std::optional<QaplaHelpers::IniFile::Section> getSectionIfNotEmpty(const std::string& id) const;

     /**
     * @brief Checks if this pairing matches the given round and engine names.
     *
     * @param round Round number to match (0-based).
     * @param engineA First engine name (must match getEngineA()).
     * @param engineB Second engine name (must match getEngineB()).
     * @return true if round and engine names match exactly.
     */
    bool matches(uint32_t round, const std::string& engineA, const std::string& engineB) const;

    /**
     * @brief Compares this pairing with another to see if they involve the same engines and round.
     *
     * Ignores results and internal state.
     *
     * @param other Another PairTournament instance to compare against.
     * @return true if both pairings have the same engines and round number.
     */
    bool matches(const PairTournament& other) const;

    /**
     * @brief Loads result block data from input stream (one round).
     *
     * Assumes round header has already been parsed.
     * Reads until next round header or EOF.
     *
     * @param in Input stream positioned after round header line.
     */
    void fromSection(const QaplaHelpers::IniFile::Section& section);

    /**
	 * @brief Checks, if the tournament is finished.
     *
	 * @return true if all games have been played, false otherwise.
	 */
    bool isFinished() const {
        return isFinished_.load();
	}

	/**
	 * @brief Checks if this tournament has any results (finished or unfinished games).
	 *
	 * @return true if there are any game results recorded, false if tournament is fresh.
	 */
	bool hasResults() const {
		return !results_.empty();
	}

    /**
     * @brief Returns the tournament configuration.
     *
     * @return PairTournamentConfig containing all configuration parameters.
     */
    const PairTournamentConfig& getConfig() const {
        return config_;
    }

    /**
     * @brief Sets a custom position name for the pair tournament.
     *
     * It is set as position name in each GameRecord created + a number for each game.
     *
     * @param name The position name to set.
     */
    void setPositionName(const std::string& name) {
        positionName_ = name;
    }

private:

    /**
     * @brief Returns the index of the next opening position to use for the given game in the encounter.
     */
    uint32_t newOpeningIndex(size_t gameInEncounter);

    void updateOpening(uint32_t openingIndex);

    /**
     * @brief Returns the compact result sequence string (e.g. "1=01?").
     *
     * @return String containing encoded results for all games.
     */
    std::string getResultSequenceEngineView() const;

    std::string getTournamentInfo() const;

    // Inform owner
    GameFinishedCallback onGameFinished_;

    // Configuration
    EngineConfig engineA_;
    EngineConfig engineB_;
	PairTournamentConfig config_;
    std::shared_ptr<StartPositions> startPositions_;
    std::string positionName_;

    // // Results from the engine pairing perspective, not from the white-player view
    GameRecord curRecord_;
    uint32_t openingIndex_ = 0; ///< Current opening index in the encounter
    std::vector<GameResult> results_;
	EngineDuelResult duelResult_;
    mutable std::mutex mutex_;
    std::mt19937 rng_;
    size_t nextIndex_ = 0;
    bool initialized_ = false;
    bool verbose_ = true;
    std::atomic<bool> isFinished_ = false;
};

} // namespace QaplaTester
