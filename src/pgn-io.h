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

#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include "move-record.h"
#include "game-record.h" 

 /**
  * @brief Thread-safe PGN input/output handler.
  */
class PgnIO {
public:
    /**
     * @brief PGN output configuration options.
     */
    struct Options {
        std::string file;
        bool append = true;
        bool onlyFinishedGames = true;
        bool minimalTags = false;
        bool saveAfterMove = false;
        bool includeClock = false;
        bool includeEval = false;
        bool includePv = false;
        bool includeDepth = false;
    };

    PgnIO() = default;

    /**
     * @brief Initializes the PGN output file depending on append mode.
     *        Clears file if append is false.
     */
    void initialize(const std::string& event = "");

    /**
     * @brief Saves the given game record to the PGN file.
     * @param game Game record to be saved.
     */
    void saveGame(const GameRecord& game);

    /**
     * @brief Loads all games from the PGN file.
     * @return Vector of parsed GameRecord instances.
     */
    // std::vector<GameRecord> loadGames();

	/**
	 * @brief Sets the options for PGN output.
	 * @param options New options to apply.
	 */
	void setOptions(const Options& options) {
		options_ = options;
	}

	static PgnIO& tournament() {
		static PgnIO instance;
		return instance;
	}

private:

    /**
     * @brief Writes PGN tag section for the given game.
     * @param out Output stream to write to.
     * @param game Game record to generate tags from.
     */
    void saveTags(std::ostream& out, const GameRecord& game);

    /**
     * @brief Writes a single PGN move with optional annotations.
     * @param out Output stream to write to.
	 * @param san Standard Algebraic Notation (SAN) of the move.
     * @param move Move to write.
     * @param plyIndex Zero-based ply index to determine move number and side.
     * @param isWhiteStart Whether white starts (relevant for proper numbering if not).
     */
    void saveMove(std::ostream& out, const std::string& san, const MoveRecord& move, 
        uint32_t plyIndex, bool isWhiteStart);


    Options options_;
    std::mutex fileMutex_;
    std::string event_ = "";
};