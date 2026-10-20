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
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace QaplaTester {

/**
 * @brief The lines of a 9 bit book as openings, kept as the book is: two bytes and a parent per node.
 *
 * A book of ten million lines turned into games at once is tens of gigabytes - a game record and a
 * move record are made for whole games, with clocks, comments and engine names. This keeps the tree
 * of the file instead, the 16 bit part of every node and the index of its parent, and the leaves the
 * lines end in; game(i) builds the game of line i when it is asked for, when a game starts from it.
 * Ten million lines take some 150 MB.
 *
 * The lines are those readBookLines gives, in the same order: every leaf in the order of the book,
 * a line through a move of priority 0 left out, the moves in long notation, castling as the king
 * moving two squares.
 */
class BookLines {
public:
    /**
     * @brief Reads a book.
     * @param filePath The book.
     * @param maxLines The most lines to keep, nullopt for all.
     * @throws std::runtime_error (QaplaBook::BookFormatError) if the file is no 9 bit book.
     */
    [[nodiscard]] static BookLines read(const std::string& filePath, std::optional<size_t> maxLines = std::nullopt);

    /** @brief The lines of the book. */
    [[nodiscard]] size_t size() const { return leaves_.size(); }
    [[nodiscard]] bool empty() const { return leaves_.empty(); }

    /** @brief The moves of the book. */
    [[nodiscard]] size_t moveCount() const { return bits_.size(); }
    /** @brief The moves of priority 0 left out, with everything below them. */
    [[nodiscard]] size_t neverPlayedCount() const { return neverPlayed_; }

    /** @brief Line i as a game from the start position. */
    [[nodiscard]] GameRecord game(size_t index) const;

private:
    static constexpr uint32_t NO_NODE = UINT32_MAX;

    std::vector<uint16_t> bits_;      ///< The 16 bit part of every node, in the order of the file
    std::vector<uint32_t> parent_;    ///< The parent of every node, NO_NODE below the start position
    std::vector<uint32_t> leaves_;    ///< The leaves of the lines kept, in the order of the book
    size_t neverPlayed_ = 0;
};

} // namespace QaplaTester
