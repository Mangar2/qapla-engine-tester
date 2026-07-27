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

#include "fen-parser.h"

#include "../game-manager/game-state.h"

#include "../qapla-engine/fenscanner.h"
#include "../qapla-engine/movegenerator.h"

#include <algorithm>

namespace QaplaTester {

FenParserResult parseFen(const FenParserInput& input) {
    FenParserResult result;
    result.nextPos = input.startPos;

    // skip blanks at the beginning
    size_t startPos = input.fenString.find_first_not_of(' ', input.startPos);
    if (startPos == std::string::npos) {
        result.error = 1;
        return result;
    }

    size_t searchLength = std::min(input.fenString.length(), input.maxSearchLength + startPos);

    for (; startPos < searchLength; ++startPos) {

        QaplaInterface::FenScanner scanner;
        QaplaMoveGenerator::MoveGenerator position;
        try {
            result.nextPos = scanner.setBoard(input.fenString, position, startPos);
            if (result.nextPos == 0) {
                continue;
            }
            std::string fen = input.fenString.substr(startPos, result.nextPos - startPos);
            GameState gameState;
            gameState.setFen(false, fen);

            result.gameRecord = GameRecord();
            result.gameRecord->setStartPosition(
                false,                              // Not standard start position
                fen,                                // FEN string
                gameState.isWhiteToMove(),          // Who to move
                gameState.getStartHalfmoves()       // Half-move clock
            );
            return result;
        } catch (...) {
            // Continue searching if GameState creation fails
            continue;
        }
    }
    if (!result.gameRecord) {
        result.error = 1;
    }

    return result;
}

} // namespace QaplaTester
