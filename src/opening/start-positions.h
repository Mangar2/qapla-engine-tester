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

#include "book-lines.h"
#include "../chess-game/game-record.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace QaplaTester {

/**
 * @brief The openings of a run: the games of a pgn or epd file, or the lines of a book.
 *
 * A book is kept as its tree, see BookLines, and a line made a game only when a game starts from it;
 * every other format as the games it was read as.
 */
struct StartPositions {
    std::vector<GameRecord> games;
    std::shared_ptr<const BookLines> book;

    [[nodiscard]] uint32_t size() const {
        return static_cast<uint32_t>(book ? book->size() : games.size());
    }
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }
    /** @brief Opening i as a game, made from the book or copied from the games. */
    [[nodiscard]] GameRecord game(size_t index) const {
        return book ? book->game(index) : games[index];
    }
};

} // namespace QaplaTester
