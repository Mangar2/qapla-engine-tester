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

#include "engine-event.h"
#include "game-result.h"

#include "../qapla-engine/move.h"

#include <string>
#include <optional>
#include <vector>
#include <sstream>
#include <iomanip>

namespace QaplaTester {

struct MoveRecord {

    struct toStringOptions {
        bool includeClock = false;
        bool includeEval = false;
        bool includePv = false;
        bool includeDepth = false;
    };
    
    std::string original;
    std::string lan_;
    std::string san_;
    std::string ponderMove;
    QaplaBasics::Move move;
    std::string comment;
    std::string nag;
    uint64_t timeMs = 0;

    std::optional<int> scoreCp = std::nullopt;
    std::optional<int> scoreMate = std::nullopt;

    uint32_t halfmoveClock = 0; 
    uint32_t depth = 0;
    uint32_t seldepth = 0;
    uint32_t multipv = 1;
    uint64_t nodes = 0;
    std::string pv;
	std::vector<SearchInfo> info; ///> List of info records received during search
    uint32_t infoUpdateCount = 0;
    uint32_t halfmoveNo_ = 0;
    bool book = false; ///> Indicates if the move was played from book

    GameEndCause endCause_ = GameEndCause::Ongoing; ///> Cause of game end after this move
    GameResult result_ = GameResult::Unterminated; ///> Result of the game after this move

    std::string engineId_; ///> Id of the engine computing this move
    std::string engineName_; ///> Name of the engine computing this move

	MoveRecord() = default;
    MoveRecord(uint32_t halfmoveNo, std::string id = "")
		: halfmoveNo_(halfmoveNo), engineId_(std::move(id)) {
	}

    /**
     * @brief Clears all data in the move record.
     */
    void clear();

    /**
     * Updates the move record with the best move and time taken from an EngineEvent.
     *
     * @param event EngineEvent of type BestMove containing the chosen move and timestamp.
     * @param lanMove move in long algebraic notation
     * @param sanMove move in short algebraic notation
     * @param computeStartTimestamp Timestamp when move computation started, in milliseconds.
     * @param halfmoveClk The current halfmove clock value, used for the 50-move rule.
     */
    void updateFromBestMove(uint32_t halfmoveNo, const std::string& engineId, 
        const EngineEvent& event, std::string lanMove, std::string sanMove,
        uint64_t computeStartTimestamp, uint32_t halfmoveClk);
    
    /**
	 * @brief Updates the move record with search information from an EngineEvent.
	 * @param searchInfo SearchInfo containing various search metrics.
     * @param whitePovCorrection Indicates if the score is from White's point of view.
     */
    void updateFromSearchInfo(const SearchInfo& searchInfo, bool whitePovCorrection = false);

    /**
     * @brief Updates the move record with a ponder move hint.
     * @param ponderMove The ponder move in SAN or LAN notation.
     */
    void updateFromHint(const std::string& ponderMove);

	/**
	 * @brief Returns a string representation of the score.
	 *
	 * @return A string representing the score in centipawns or mate value.
	 */
    [[nodiscard]] std::string evalString() const;

    /**
     * @brief Creates a minimal copy of the current MoveRecord object.
     *
     * This method copies all fields of the MoveRecord object except the following:
     * origial and san moves, comment, nag, list of info records, updateCount
     *
     * @return A `MoveRecord` object with reduced data.
     */
    [[nodiscard]] MoveRecord createMinimalCopy() const;

    /**
     * Convert this MoveRecord into a string containing the move (SAN) and
     * an optional comment constructed from the provided options. Does NOT
     * include the move number.
     * @param opts Options for what to include in the string.
     * @return The string representation of the move.
     */
    [[nodiscard]] std::string toString(const toStringOptions& opts = {.includeClock=false, .includeEval=false, .includePv=false, .includeDepth=false}) const;

    /**
     * @brief Generates the game-end text for the comment.
     * @return The game-end text string.
     */
    [[nodiscard]] std::string getGameEndText() const;
};

using MoreRecords = std::vector<std::optional<MoveRecord>>;

} // namespace QaplaTester
