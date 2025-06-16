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

#include "game-task.h"
#include "game-record.h"
#include "engine-config.h"
#include "time-control.h"
#include "openings.h"
#include "tournament-result.h"
#include <vector>
#include <memory>
#include <optional>
#include <string>
#include <istream>
#include <ostream>
#include <mutex>
#include <array>
#include <sstream>

/**
* @brief Configuration parameters for a PairTournament.
*/
struct PairTournamentConfig {
    int games = 0;
    int repeat = 2;
    bool swapColors = true;
    Openings openings;
};

 /**
  * @brief Represents an autonomous two-engine tournament with defined number of games.
  *
  * Implements GameTaskProvider. Manages game generation, opening selection,
  * color assignment, result storage and resumability.
  */
class PairTournament : public GameTaskProvider {
public:
    PairTournament() = default;

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
        std::shared_ptr<std::vector<std::string>> startPositions);

    /**
     * @brief Registers this tournament with the GameManagerPool for execution.
     *
     * Must be called after initialize(). Adds this pairing to the pool with the configured concurrency limit.
     */
    void schedule();

    /**
     * @brief Provides the next task for a matching engine pair.
     *
     * @param whiteId The engine requesting white.
     * @param blackId The engine requesting black.
     * @return A task to execute or std::nullopt if complete or non-matching.
     */
    std::optional<GameTask> nextTask(const std::string& whiteId,
        const std::string& blackId) override;

    /**
     * @brief Records the result of a finished game.
     *
     * @param whiteId Engine that played white.
     * @param blackId Engine that played black.
     * @param record Game outcome and metadata.
     */
    void setGameRecord(const std::string& whiteId,
        const std::string& blackId,
        const GameRecord& record) override;

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
     * @param line A single line in the format: "<engineA> vs <engineB> : <result-sequence>"
     */
    void fromString(const std::string& line);


    /**
	 * @brief Returns the result of the duel between the two engines.
     */
    EngineDuelResult getResult() const {
		return duelResult_;
    }

private:
    EngineConfig engineA_;
    EngineConfig engineB_;
	PairTournamentConfig config_;
    std::shared_ptr<std::vector<std::string>> startPositions_;
    std::string curStartPosition_;

    std::vector<GameResult> results_;
	EngineDuelResult duelResult_;
    mutable std::mutex mutex_;
    size_t nextIndex_ = 0;
    bool started_ = false;
};
