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
     * 
	 * @param fileName Name of the PGN file to load.
     * @return Vector of parsed GameRecord instances.
     */
    std::vector<GameRecord> loadGames(const std::string& fileName);

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
        uint32_t plyIndex, bool isWhiteStart) const;

    /**
     * @brief Parses a SAN move and attached annotations starting at a position.
     * @param tokens Token list from PGN input.
     * @param start Position to begin parsing from.
     * @return Pair {MoveRecord, next position}. If no valid move, next == start.
     */
    std::pair<MoveRecord, size_t> parseMove(const std::vector<std::string>& tokens, size_t start);

    /**
     * @brief Parses a PGN tag line.
	 * @param tokens Tokenized line from PGN input.
     * @return Pair of tag key and value. Returns {"", ""} if invalid.
     */
    std::pair<std::string, std::string> parseTag(const std::vector<std::string>& tokens);
    
    /**
     * @brief Parses a PGN move line from tokens.
     * @param tokens Tokenized line from PGN input.
     * @return Pair of move list and optional game result (1-0, 0-1, 1/2-1/2, *).
     */
    std::pair<std::vector<MoveRecord>, std::optional<GameResult>> parseMoveLine(const std::vector<std::string>& tokens);

    /**
     * @brief Tokenizes a single PGN line into semantic PGN tokens.
     * @param line A trimmed line of PGN input.
     * @return List of tokens according to PGN token rules.
     */
    std::vector<std::string> tokenize(const std::string& line);

    /**
     * @brief Skips a move-number indication like 12. or 23... starting at position.
     * @param tokens Token list from PGN input.
     * @param start Position to begin checking.
     * @return Next token position after move-number sequence.
     */
    size_t skipMoveNumber(const std::vector<std::string>& tokens, size_t start);

    
    /**
    * @brief Skips a recursive variation in PGN notation starting at a given position.
    *        Recursive variations are enclosed in parentheses and can contain nested variations.
    * @param tokens Token list from PGN input.
    * @param start Position to begin checking.
    * @return Next token position after the recursive variation.
    *         If no valid variation is found, returns the start position.
    */
    size_t skipRecursiveVariation(const std::vector<std::string>& tokens, size_t start);

    /**
     * @brief Parses a comment block following a SAN move and extracts metadata.
     * @param tokens Token list from PGN input.
     * @param start Position of the opening "{" token.
     * @param move MoveRecord to populate.
     * @return Position after closing "}" or unchanged on error.
     */
    size_t parseMoveComment(const std::vector<std::string>& tokens, size_t start, MoveRecord& move);

    /**
     * @brief Interprets known PGN tags and sets corresponding GameRecord fields.
     * @param game The GameRecord whose tags will be finalized.
     */
    void finalizeParsedTags(GameRecord& game);

    Options options_;
    std::mutex fileMutex_;
    std::string event_ = "";
};
