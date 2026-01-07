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
 * @author Volker BÃ¶hm
 * @copyright Copyright (c) 2025 Volker BÃ¶hm
 */

#pragma once

#include "chess-game/move-record.h"
#include "chess-game/game-record.h"

#include <string>
#include <mutex>
#include <fstream>

namespace QaplaTester {

/**
 * @brief Thread-safe PGN save handler.
 */
class PgnSave {
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
        bool includeClock = true;
        bool includeEval = true;
        bool includePv = true;
        bool includeDepth = true;
    };

    PgnSave() = default;

    /**
     * @brief Initializes the PGN output file depending on append mode.
     *        Clears file if append is false and not resuming an existing tournament.
     * @param event Event name for the tournament.
     * @param isResumingTournament If true, never truncates the file even in overwrite mode.
     *                            This should be true when loading existing tournament results.
     */
    void initialize(const std::string& event = "", bool isResumingTournament = false);

    /**
     * @brief Saves the given game record to the PGN file.
     * @param game Game record to be saved.
     */
    void saveGame(const GameRecord& game);

    /**
     * @brief Saves the given game record to the specified PGN file.
     * @param fileName Name of the PGN file to save to.
     * @param game Game record to be saved.
     */
    void saveGame(const std::string& fileName, const GameRecord& game);

    /**
     * @brief Saves the given game record to the provided output stream.
     * @param out Output stream to write to.
     * @param game Game record to be saved.
     */
    void saveGameToStream(std::ostream& out, const GameRecord& game);

    /**
     * @brief Sets the options for PGN output.
     * @param options New options to apply.
     */
    void setOptions(const Options& options) {
        options_ = options;
    }

    /**
     * @brief Returns the singleton instance for tournament PGN saving.
     * @return Reference to the singleton PgnSave instance.
     */
    static PgnSave& tournament() {
        static PgnSave instance;
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

    Options options_;
    std::mutex fileMutex_;
    std::string event_;
};

} // namespace QaplaTester
