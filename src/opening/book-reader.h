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
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#pragma once

#include "../chess-game/game-record.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace QaplaTester {

/**
 * @brief Result of reading the lines of a book file.
 */
struct BookReaderResult {
    std::vector<GameRecord> games;  ///< One game per line, from the start position to a leaf
    size_t moves = 0;               ///< Moves the book holds
    size_t neverPlayed = 0;         ///< Moves of priority 0 left out, with everything below them
    std::string error;              ///< Why the file is no book that can be read, empty if it is
};

/**
 * @brief Reads the lines of a 9 bit book of the Qapla engine (file version "9") as openings.
 *
 * Every leaf of the book becomes one game: the moves from the start position to the leaf, in long
 * algebraic notation, castling as the king moving two squares. That is the form a game of a PGN
 * file has, so everything an opening file offers - plies, order, start, policy - works on a book
 * the same way. A move of priority 0 is never to be played, so it is left out with all lines
 * below it.
 *
 * The payload of the nodes is of no interest here, the book is read whatever its size. The node
 * array itself is handed to the engine's own reader, see qapla-engine/book9/book.h.
 *
 * @param filePath Path to the book file.
 * @param maxLines Maximum number of lines to read, nullopt for all.
 * @return The lines, or an error if the file is not a 9 bit book.
 */
[[nodiscard]] BookReaderResult readBookLines(const std::string& filePath,
    std::optional<size_t> maxLines = std::nullopt);

} // namespace QaplaTester
