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

#include <optional>
#include <string>

namespace QaplaTester {

/**
 * @brief Result of parsing a FEN string.
 */
struct FenParserResult {
    std::optional<GameRecord> gameRecord;  ///< Parsed record; nullopt if no valid FEN was found
    uint32_t error = 0;                    ///< Non-zero if parsing failed
    size_t nextPos = 0;                    ///< Position directly after the parsed FEN
};

/**
 * @brief Input parameters for parsing a FEN string.
 *
 * maxSearchLength controls how tolerant the parser is about leading garbage:
 * the parser tries each start offset in [startPos, startPos + maxSearchLength)
 * until a valid FEN is found.
 *  - strict:  maxSearchLength = 1 (the FEN must start at startPos)
 *  - loose:   a larger window, e.g. 1000 for free-form pasted text
 */
struct FenParserInput {
    std::string fenString;              ///< The text containing the FEN
    size_t startPos = 0;                ///< The position to start parsing from
    size_t maxSearchLength = 10;        ///< Number of start offsets to try
};

/**
 * @brief Parses a FEN embedded in a text string into a GameRecord.
 *
 * This is the single shared FEN parsing helper: EPD/raw opening files, the
 * GUI paste/board loaders and every other FEN consumer should go through it.
 * Accepts both full FENs (with halfmove/fullmove counters) and 4-field
 * EPD-style FENs; side to move and start halfmove number are taken from the
 * FEN, not assumed.
 *
 * @param input The FenParserInput structure containing the text and parameters.
 * @return FenParserResult containing the parsed GameRecord and status.
 */
[[nodiscard]] FenParserResult parseFen(const FenParserInput& input);

} // namespace QaplaTester
