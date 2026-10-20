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

#include "book-reader.h"

#include "book-lines.h"

#include <exception>

namespace QaplaTester {

BookReaderResult readBookLines(const std::string& filePath, std::optional<size_t> maxLines) {
    BookReaderResult result;
    try {
        // The compact lines, every one of them made a game: the same lines as a tournament plays.
        const auto lines = BookLines::read(filePath, maxLines);
        result.moves = lines.moveCount();
        result.neverPlayed = lines.neverPlayedCount();
        result.games.reserve(lines.size());
        for (size_t index = 0; index < lines.size(); ++index) {
            result.games.push_back(lines.game(index));
        }
    } catch (const std::exception& exc) {
        result.games.clear();
        result.error = exc.what();
    }
    return result;
}

} // namespace QaplaTester
